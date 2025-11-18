#include "composite_element.h"

#include <climits>

#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QDebug>
#include <QTimer>

#include "text_element.h"
#include "choice_button_element.h"
#include "slider_element.h"
#include "rectangle_element.h"
#include "oval_element.h"
#include "arc_element.h"
#include "line_element.h"
#include "polyline_element.h"
#include "polygon_element.h"
#include "image_element.h"

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
  designModeVisible_ = QWidget::isVisible();
  runtimeVisible_ = true;
  setExecuteMode(false);
  updateMouseTransparency();
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
    child->installEventFilter(this);
  }
  refreshChildStackingOrder();
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
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QWidget::isVisible();
  } else {
    QWidget::setVisible(designModeVisible_);
  }

  executeMode_ = execute;
  updateMouseTransparency();

  /* Propagate the execute mode to ALL children FIRST
   * so they update their internal state before we modify visibility. */
  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child || child == this) {
      continue;
    }

    if (auto *elem = dynamic_cast<TextElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<CompositeElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<ChoiceButtonElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<SliderElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<RectangleElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<OvalElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<ArcElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<LineElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<PolylineElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<PolygonElement *>(child)) {
      elem->setExecuteMode(execute);
    }
  }

  applyRuntimeVisibility();
  update();

  refreshChildStackingOrder();
}

void CompositeElement::setChannelConnected(bool connected)
{
  if (channelConnected_ == connected) {
    return;
  }
  const bool wasVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  channelConnected_ = connected;
  applyRuntimeVisibility();
  update();
  const bool nowVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  if (!wasVisible && nowVisible) {
    raiseCompositeHierarchy();
  }
}

bool CompositeElement::isChannelConnected() const
{
  return channelConnected_;
}

void CompositeElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  const bool wasVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  runtimeVisible_ = visible;
  applyRuntimeVisibility();
  update();
  const bool nowVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  if (!wasVisible && nowVisible) {
    raiseCompositeHierarchy();
  }
}

void CompositeElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  /* In execute mode with a channel defined but not connected, fill with white */
  if (executeMode_ && hasActiveChannel() && !channelConnected_) {
    painter.fillRect(rect(), Qt::white);
    if (selected_) {
      QPen pen(Qt::black);
      pen.setStyle(Qt::DashLine);
      pen.setWidth(1);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    return;
  }

  /* Draw selection border if selected */
  if (selected_) {
    QPen pen(foregroundColor());
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
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

void CompositeElement::applyRuntimeVisibility()
{
  if (!executeMode_) {
    QWidget::setVisible(designModeVisible_);
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(true);
        child->show();
      }
    }
    return;
  }

  const bool hasChannel = hasActiveChannel();
  if (!hasChannel) {
    QWidget::setVisible(designModeVisible_);
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(designModeVisible_);
        if (!designModeVisible_) {
          child->hide();
        }
      }
    }
    return;
  }

  if (!channelConnected_) {
    QWidget::setVisible(designModeVisible_);
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(false);
        child->hide();
      }
    }
    QTimer::singleShot(0, this, [this]() {
      if (!executeMode_ || channelConnected_ || !hasActiveChannel()) {
        return;
      }
      for (const auto &pointer : childWidgets_) {
        if (QWidget *child = pointer.data()) {
          child->hide();
        }
      }
    });
    return;
  }

  const bool show = designModeVisible_ && runtimeVisible_;
  QWidget::setVisible(show);
  for (const auto &pointer : childWidgets_) {
    if (QWidget *child = pointer.data()) {
      child->setVisible(show);
      if (!show) {
        child->hide();
      }
    }
  }
}

void CompositeElement::raiseCompositeHierarchy()
{
  raise();
  refreshChildStackingOrder();
}

bool CompositeElement::isStaticChildWidget(const QWidget *child) const
{
  if (!child) {
    return false;
  }

  if (const auto *composite = dynamic_cast<const CompositeElement *>(child)) {
    const QList<QWidget *> children = composite->childWidgets();
    if (children.isEmpty()) {
      return true;
    }
    for (QWidget *grandChild : children) {
      if (!composite->isStaticChildWidget(grandChild)) {
        return false;
      }
    }
    return true;
  }

  return dynamic_cast<const RectangleElement *>(child)
      || dynamic_cast<const ImageElement *>(child)
      || dynamic_cast<const OvalElement *>(child)
      || dynamic_cast<const ArcElement *>(child)
      || dynamic_cast<const LineElement *>(child)
      || dynamic_cast<const PolylineElement *>(child)
      || dynamic_cast<const PolygonElement *>(child)
      || dynamic_cast<const TextElement *>(child);
}

void CompositeElement::refreshChildStackingOrder()
{
  QList<QWidget *> staticWidgets;
  QList<QWidget *> interactiveWidgets;

  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child) {
      continue;
    }
    if (isStaticChildWidget(child)) {
      staticWidgets.append(child);
    } else {
      interactiveWidgets.append(child);
    }
  }

  for (int i = staticWidgets.size() - 1; i >= 0; --i) {
    if (QWidget *widget = staticWidgets.at(i)) {
      widget->raise();
    }
  }
  for (QWidget *widget : interactiveWidgets) {
    widget->raise();
  }
}

void CompositeElement::scheduleChildStackingRefresh()
{
  if (childStackingRefreshPending_) {
    return;
  }
  childStackingRefreshPending_ = true;
  QTimer::singleShot(0, this, [this]() {
    childStackingRefreshPending_ = false;
    refreshChildStackingOrder();
  });
}

bool CompositeElement::eventFilter(QObject *watched, QEvent *event)
{
  if (!watched || !event) {
    return QWidget::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::ShowToParent:
  case QEvent::HideToParent:
  case QEvent::ParentChange:
  case QEvent::ZOrderChange:
    scheduleChildStackingRefresh();
    break;
  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
}
