#include "composite_element.h"

#include <climits>

#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>

#include "text_element.h"

CompositeElement::CompositeElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_MouseNoMask, true);
  /* Clear any clipping mask to allow children to extend beyond bounds */
  clearMask();
  /* Ensure no content margins that might offset children */
  setContentsMargins(0, 0, 0, 0);
  setExecuteMode(false);
  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();
}

void CompositeElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool CompositeElement::isSelected() const
{
  return selected_;
}

QString CompositeElement::compositeName() const
{
  return compositeName_;
}

void CompositeElement::setCompositeName(const QString &name)
{
  compositeName_ = name;
}

QString CompositeElement::compositeFile() const
{
  return compositeFile_;
}

void CompositeElement::setCompositeFile(const QString &filePath)
{
  compositeFile_ = filePath;
}

QColor CompositeElement::foregroundColor() const
{
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return defaultForegroundColor();
}

void CompositeElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  update();
}

QColor CompositeElement::backgroundColor() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackgroundColor();
}

void CompositeElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  update();
}

TextColorMode CompositeElement::colorMode() const
{
  return colorMode_;
}

void CompositeElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode CompositeElement::visibilityMode() const
{
  return visibilityMode_;
}

void CompositeElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString CompositeElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void CompositeElement::setVisibilityCalc(const QString &calc)
{
  visibilityCalc_ = calc;
}

QString CompositeElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[static_cast<std::size_t>(index)];
}

void CompositeElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  channels_[static_cast<std::size_t>(index)] = value;
  updateMouseTransparency();
}

std::array<QString, 5> CompositeElement::channels() const
{
  return channels_;
}

void CompositeElement::adoptChild(QWidget *child)
{
  if (!child) {
    return;
  }
  if (child->parentWidget() != this) {
    child->setParent(this);
  }
  if (!childWidgets_.contains(child)) {
    childWidgets_.append(QPointer<QWidget>(child));
  }
}

void CompositeElement::expandToFitChildren()
{
  if (childWidgets_.isEmpty()) {
    return;
  }

  /*
   * Calculate the bounding box that encompasses all child widgets,
   * similar to how MEDM handles composite bounds (medmComposite.c).
   */
  int minX = INT_MAX;
  int minY = INT_MAX;
  int maxX = INT_MIN;
  int maxY = INT_MIN;

  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child) {
      continue;
    }

    /* Get child geometry relative to this composite */
    QRect childGeom = child->geometry();
    if (auto *textChild = dynamic_cast<TextElement *>(child)) {
      const QRect textBounds = textChild->visualBoundsRelativeToParent();
      if (textBounds.isValid()) {
        childGeom = textBounds;
      }
    }
    int childX = childGeom.x();
    int childY = childGeom.y();
    int childRight = childX + childGeom.width();
    int childBottom = childY + childGeom.height();

    if (childX < minX) minX = childX;
    if (childY < minY) minY = childY;
    if (childRight > maxX) maxX = childRight;
    if (childBottom > maxY) maxY = childBottom;
  }

  /* Nothing to do if no valid children found */
  if (minX == INT_MAX || minY == INT_MAX) {
    return;
  }

  /*
   * Adjust composite geometry to encompass all children.
   * Store current position to calculate offset.
   */
  QRect currentGeom = geometry();
  int oldX = currentGeom.x();
  int oldY = currentGeom.y();

  /* New composite bounds based on children */
  int newX = oldX + minX;
  int newY = oldY + minY;
  int newWidth = maxX - minX;
  int newHeight = maxY - minY;

  /* Update composite geometry */
  setGeometry(newX, newY, newWidth, newHeight);

  /*
   * Adjust all child positions relative to new composite origin.
   * Since composite moved by (minX, minY), shift children by (-minX, -minY).
   */
  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child) {
      continue;
    }
    QRect childGeom = child->geometry();
    child->setGeometry(childGeom.x() - minX, childGeom.y() - minY,
                      childGeom.width(), childGeom.height());
  }
}

QList<QWidget *> CompositeElement::childWidgets() const
{
  QList<QWidget *> result;
  for (const auto &pointer : childWidgets_) {
    if (QWidget *widget = pointer.data()) {
      result.append(widget);
    }
  }
  return result;
}

void CompositeElement::setExecuteMode(bool execute)
{
  executeMode_ = execute;
  updateMouseTransparency();

  /* Propagate the execute mode to nested composites so their mouse behaviour
   * matches the current state even if they were loaded indirectly. */
  for (const auto &pointer : childWidgets_) {
    if (auto *childComposite = dynamic_cast<CompositeElement *>(pointer.data())) {
      if (childComposite == this) {
        continue;
      }
      childComposite->setExecuteMode(execute);
    }
  }
  
  /* When leaving execute mode, ensure all children are visible */
  if (!execute) {
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(true);
      }
    }
  }
  /* When entering execute mode with non-static visibility and disconnected channel,
   * hide children */
  else if (visibilityMode_ != TextVisibilityMode::kStatic && !channelConnected_) {
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(false);
      }
    }
  }
}

void CompositeElement::setChannelConnected(bool connected)
{
  if (channelConnected_ == connected) {
    return;
  }
  channelConnected_ = connected;
  
  /* Hide/show child widgets based on connection state in execute mode */
  if (executeMode_ && visibilityMode_ != TextVisibilityMode::kStatic) {
    bool shouldHideChildren = !connected;
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(!shouldHideChildren);
      }
    }
  }
  
  update();
}

bool CompositeElement::isChannelConnected() const
{
  return channelConnected_;
}

void CompositeElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  /* In execute mode with dynamic visibility, don't paint anything if invisible.
   * The children are already hidden via setChannelConnected(). */
  if (executeMode_ && visibilityMode_ != TextVisibilityMode::kStatic && 
      !channelConnected_) {
    /* Do nothing - composite is invisible, don't paint white box */
    return;
  }

  if (!selected_) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(foregroundColor());
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

QColor CompositeElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor CompositeElement::defaultBackgroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

bool CompositeElement::hasActiveChannel() const
{
  for (const auto &channel : channels_) {
    if (!channel.trimmed().isEmpty()) {
      return true;
    }
  }
  return false;
}

void CompositeElement::updateMouseTransparency()
{
  /* In execute mode, always allow mouse events so child widgets can receive
   * them (for cursors, tooltips, interaction). In edit mode, be transparent
   * so clicks select/manipulate the composite itself rather than children. */
  setAttribute(Qt::WA_TransparentForMouseEvents, !executeMode_);
}

void CompositeElement::mousePressEvent(QMouseEvent *event)
{
  // Forward middle button and right-click events to parent window for PV info functionality
  if (executeMode_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

bool CompositeElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  if (!event) {
    return false;
  }
  QWidget *target = window();
  if (!target) {
    return false;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF globalPosF = event->globalPosition();
  const QPoint globalPoint = globalPosF.toPoint();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos, globalPosF,
      event->button(), event->buttons(), event->modifiers());
#else
  const QPoint globalPoint = event->globalPos();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos,
      QPointF(globalPoint), event->button(), event->buttons(),
      event->modifiers());
#endif
  QCoreApplication::sendEvent(target, &forwarded);
  return true;
}
