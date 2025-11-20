#include "rectangle_element.h"

#include <algorithm>
#include <limits>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"

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
  if (!suppressLineWidthTracking_ && lineWidth_ != clamped) {
    lineWidthEdited_ = true;
  }
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

void RectangleElement::setLineWidthFromAdl(int width)
{
  const bool previous = suppressLineWidthTracking_;
  suppressLineWidthTracking_ = true;
  setLineWidth(width);
  suppressLineWidthTracking_ = previous;
  lineWidthEdited_ = false;
}

int RectangleElement::adlLineWidth() const
{
  return adlLineWidth_;
}

void RectangleElement::setAdlLineWidth(int width, bool hasProperty)
{
  adlLineWidth_ = width;
  hasAdlLineWidthProperty_ = hasProperty;
  lineWidthEdited_ = false;
}

bool RectangleElement::shouldSerializeLineWidth() const
{
  if (hasAdlLineWidthProperty_) {
    return true;
  }
  return lineWidthEdited_;
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

void RectangleElement::setGeometry(const QRect &rect)
{
  const QSize previousSize = QWidget::geometry().size();
  if (!suppressGeometryTracking_ && hasOriginalAdlSize_ &&
      rect.size() != previousSize) {
    sizeEdited_ = true;
  }
  QWidget::setGeometry(rect);
}

void RectangleElement::initializeFromAdlGeometry(const QRect &geometry,
    const QSize &adlSize)
{
  originalAdlSize_ = adlSize;
  hasOriginalAdlSize_ = true;
  sizeEdited_ = false;
  setGeometryWithoutTracking(geometry);
}

void RectangleElement::setGeometryWithoutTracking(const QRect &geometry)
{
  const bool previous = suppressGeometryTracking_;
  suppressGeometryTracking_ = true;
  QWidget::setGeometry(geometry);
  suppressGeometryTracking_ = previous;
}

QRect RectangleElement::geometryForSerialization() const
{
  QRect serialized = geometry();
  if (hasOriginalAdlSize_ && !sizeEdited_) {
    serialized.setSize(originalAdlSize_);
  }
  return serialized;
}

void RectangleElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor currentColor = effectiveForegroundColor();
  QRect deviceRect;
  /* Mimic medm bug: outline rectangles are drawn at (+1,+1) with
     dimensions reduced by 1 pixel */
  if (fill_ == RectangleFill::kSolid) {
    deviceRect = rect().adjusted(0, 0, -1, -1);
  } else {
    deviceRect = (adlLineWidth_ == 0) ? rect().adjusted(0, 0, -1, -1) : rect().adjusted(1, 1, -1, -1);
  }

  if (fill_ == RectangleFill::kSolid) {
    // Draw solid fill with one less pixel on right and bottom
    painter.fillRect(deviceRect, currentColor);
  } else {
    // MEDM's X11 renderer extends outline mode one pixel past the widget
    // bounds, so expand the Qt outline by a pixel on each side to match.
    painter.setBrush(Qt::NoBrush);
    QPen pen(currentColor);
    pen.setWidth(lineWidth_);
    pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                         : Qt::SolidLine);
    painter.setPen(pen);
    
    const int halfWidth = (lineWidth_ + 1) / 2;
    const int outlineInset = std::max(0, halfWidth - 1);
    QRect outlineRect = deviceRect.adjusted(-1, -1, 1, 1);
    outlineRect = outlineRect.adjusted(outlineInset, outlineInset,
                                       -outlineInset, -outlineInset);
    outlineRect = outlineRect.intersected(deviceRect);
    if (outlineRect.width() <= 0 || outlineRect.height() <= 0) {
      outlineRect = deviceRect;
    }
    painter.drawRect(outlineRect);
  }

  if (selected_) {
    QPen pen(Qt::black);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(deviceRect);
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

  if (!runtimeConnected_) {
    return QColor(255, 255, 255);
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    return MedmColors::alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void RectangleElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_;
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

