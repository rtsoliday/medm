#include "arc_element.h"

#include <algorithm>
#include <limits>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"

ArcElement::ArcElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setForegroundColor(palette().color(QPalette::WindowText));
  setFill(RectangleFill::kOutline);
  setLineStyle(RectangleLineStyle::kSolid);
  setLineWidth(1);
  setBeginAngle(0);
  setPathAngle(90 * 64);
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  designModeVisible_ = QWidget::isVisible();
  update();
}

void ArcElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ArcElement::isSelected() const
{
  return selected_;
}

QColor ArcElement::color() const
{
  return color_;
}

void ArcElement::setForegroundColor(const QColor &color)
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

RectangleFill ArcElement::fill() const
{
  return fill_;
}

void ArcElement::setFill(RectangleFill fill)
{
  if (fill_ == fill) {
    return;
  }
  fill_ = fill;
  update();
}

RectangleLineStyle ArcElement::lineStyle() const
{
  return lineStyle_;
}

void ArcElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int ArcElement::lineWidth() const
{
  return lineWidth_;
}

void ArcElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

int ArcElement::beginAngle() const
{
  return beginAngle_;
}

void ArcElement::setBeginAngle(int angle64)
{
  if (beginAngle_ == angle64) {
    return;
  }
  beginAngle_ = angle64;
  update();
}

int ArcElement::pathAngle() const
{
  return pathAngle_;
}

void ArcElement::setPathAngle(int angle64)
{
  if (pathAngle_ == angle64) {
    return;
  }
  pathAngle_ = angle64;
  update();
}

TextColorMode ArcElement::colorMode() const
{
  return colorMode_;
}

void ArcElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode ArcElement::visibilityMode() const
{
  return visibilityMode_;
}

void ArcElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString ArcElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void ArcElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString ArcElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void ArcElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void ArcElement::setExecuteMode(bool execute)
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

bool ArcElement::isExecuteMode() const
{
  return executeMode_;
}

void ArcElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    updateExecuteState();
  }
}

void ArcElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyRuntimeVisibility();
  }
}

void ArcElement::setRuntimeSeverity(short severity)
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

void ArcElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(visible);
}

void ArcElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();
  QRect drawRect = rect().adjusted(0, 0, -1, -1);
  const int startAngle = toQtAngle(beginAngle_);
  const int spanAngle = toQtAngle(pathAngle_);

  if (fill_ == RectangleFill::kSolid) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveColor);
    painter.drawPie(drawRect, startAngle, spanAngle);
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
      painter.drawArc(outlineRect, startAngle, spanAngle);
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

QColor ArcElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return Qt::black;
}

QColor ArcElement::effectiveForegroundColor() const
{
  const QColor baseColor = color_.isValid() ? color_ : defaultForegroundColor();
  if (!executeMode_) {
    return baseColor;
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm: {
    const short severity = runtimeConnected_
        ? runtimeSeverity_
        : std::numeric_limits<short>::max();
    return MedmColors::alarmColorForSeverity(severity);
  }
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void ArcElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_;
    QWidget::setVisible(visible);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

void ArcElement::updateExecuteState()
{
  applyRuntimeVisibility();
  update();
}

int ArcElement::toQtAngle(int angle64) const
{
  if (angle64 >= 0) {
    return (angle64 + 2) / 4;
  }
  return (angle64 - 2) / 4;
}

