#include "rectangle_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

namespace {

QColor alarmColorForSeverity(short severity)
{
  switch (severity) {
  case 0:
    return QColor(0, 205, 0);
  case 1:
    return QColor(255, 255, 0);
  case 2:
    return QColor(255, 0, 0);
  case 3:
    return QColor(255, 255, 255);
  default:
    return QColor(204, 204, 204);
  }
}

} // namespace

RectangleElement::RectangleElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setForegroundColor(palette().color(QPalette::WindowText));
  setFill(RectangleFill::kOutline);
  setLineStyle(RectangleLineStyle::kSolid);
  setLineWidth(1);
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  designModeVisible_ = QWidget::isVisible();
  update();
}

void RectangleElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool RectangleElement::isSelected() const
{
  return selected_;
}

QColor RectangleElement::color() const
{
  return color_;
}

void RectangleElement::setForegroundColor(const QColor &color)
{
  QColor effective = color;
  if (!effective.isValid()) {
    effective = defaultForegroundColor();
  }
  if (color_ == effective) {
    return;
  }
  color_ = effective;
  update();
}

RectangleFill RectangleElement::fill() const
{
  return fill_;
}

void RectangleElement::setFill(RectangleFill fill)
{
  if (fill_ == fill) {
    return;
  }
  fill_ = fill;
  update();
}

RectangleLineStyle RectangleElement::lineStyle() const
{
  return lineStyle_;
}

void RectangleElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int RectangleElement::lineWidth() const
{
  return lineWidth_;
}

void RectangleElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

TextColorMode RectangleElement::colorMode() const
{
  return colorMode_;
}

void RectangleElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode RectangleElement::visibilityMode() const
{
  return visibilityMode_;
}

void RectangleElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString RectangleElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void RectangleElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString RectangleElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void RectangleElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void RectangleElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QWidget::isVisible();
  }

  executeMode_ = execute;
  runtimeConnected_ = false;
  runtimeVisible_ = true;
  runtimeSeverity_ = 0;
  updateExecuteState();
}

bool RectangleElement::isExecuteMode() const
{
  return executeMode_;
}

void RectangleElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    updateExecuteState();
  }
}

void RectangleElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyRuntimeVisibility();
  }
}

void RectangleElement::setRuntimeSeverity(short severity)
{
  if (severity < 0) {
    severity = 0;
  }
  severity = std::min<short>(severity, 3);
  if (runtimeSeverity_ == severity) {
    return;
  }
  runtimeSeverity_ = severity;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    update();
  }
}

void RectangleElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(visible);
}

void RectangleElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor currentColor = effectiveForegroundColor();
  QRect drawRect = rect().adjusted(0, 0, -1, -1);

  if (fill_ == RectangleFill::kSolid) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(currentColor);
    painter.drawRect(drawRect);
  } else {
    painter.setBrush(Qt::NoBrush);
    QPen pen(currentColor);
    pen.setWidth(lineWidth_);
    pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                         : Qt::SolidLine);
    painter.setPen(pen);
    QRect outlineRect = drawRect;
    if (lineWidth_ > 1) {
      const int offset = lineWidth_ / 2;
      outlineRect.adjust(offset, offset, -offset, -offset);
    }
    if (outlineRect.width() > 0 && outlineRect.height() > 0) {
      painter.drawRect(outlineRect);
    }
  }

  if (selected_) {
    QPen pen(Qt::black);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
      painter.drawRect(drawRect);
  }
}

QColor RectangleElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor RectangleElement::effectiveForegroundColor() const
{
  QColor baseColor = color_.isValid() ? color_ : defaultForegroundColor();
  if (!executeMode_) {
    return baseColor;
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    return alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void RectangleElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_ && runtimeConnected_;
    QWidget::setVisible(visible);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

void RectangleElement::updateExecuteState()
{
  applyRuntimeVisibility();
  update();
}

