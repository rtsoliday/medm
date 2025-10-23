#include "oval_element.h"

#include <algorithm>
#include <limits>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"

OvalElement::OvalElement(QWidget *parent)
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

void OvalElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool OvalElement::isSelected() const
{
  return selected_;
}

QColor OvalElement::color() const
{
  return color_;
}

void OvalElement::setForegroundColor(const QColor &color)
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

RectangleFill OvalElement::fill() const
{
  return fill_;
}

void OvalElement::setFill(RectangleFill fill)
{
  if (fill_ == fill) {
    return;
  }
  fill_ = fill;
  update();
}

RectangleLineStyle OvalElement::lineStyle() const
{
  return lineStyle_;
}

void OvalElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int OvalElement::lineWidth() const
{
  return lineWidth_;
}

void OvalElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

TextColorMode OvalElement::colorMode() const
{
  return colorMode_;
}

void OvalElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode OvalElement::visibilityMode() const
{
  return visibilityMode_;
}

void OvalElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString OvalElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void OvalElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString OvalElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void OvalElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void OvalElement::setExecuteMode(bool execute)
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

bool OvalElement::isExecuteMode() const
{
  return executeMode_;
}

void OvalElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    updateExecuteState();
  }
}

void OvalElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyRuntimeVisibility();
  }
}

void OvalElement::setRuntimeSeverity(short severity)
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

void OvalElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(visible);
}

void OvalElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();
  QRect drawRect = rect().adjusted(0, 0, -1, -1);

  if (fill_ == RectangleFill::kSolid) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveColor);
    painter.drawEllipse(drawRect);
  } else {
    painter.setBrush(Qt::NoBrush);
    QPen pen(effectiveColor);
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
      painter.drawEllipse(outlineRect);
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

QColor OvalElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return Qt::black;
}

QColor OvalElement::effectiveForegroundColor() const
{
  const QColor baseColor = color_.isValid() ? color_ : defaultForegroundColor();
  if (!executeMode_) {
    return baseColor;
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    if (!runtimeConnected_) {
      return QColor(255, 255, 255);
    }
    return MedmColors::alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void OvalElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_;
    QWidget::setVisible(visible);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

void OvalElement::updateExecuteState()
{
  applyRuntimeVisibility();
  update();
}

