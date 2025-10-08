#include "composite_element.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>

CompositeElement::CompositeElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_MouseNoMask, true);
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

void CompositeElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  if (!selected_) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(defaultForegroundColor());
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
