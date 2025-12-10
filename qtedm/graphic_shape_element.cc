#include "graphic_shape_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"
#include "update_coordinator.h"

GraphicShapeElement::GraphicShapeElement(QWidget *parent)
  : QWidget(parent)
{
  designModeVisible_ = QWidget::isVisible();
}

void GraphicShapeElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool GraphicShapeElement::isSelected() const
{
  return selected_;
}

QColor GraphicShapeElement::color() const
{
  return color_;
}

void GraphicShapeElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (color_ == effective) {
    return;
  }
  color_ = effective;
  update();
}

TextColorMode GraphicShapeElement::colorMode() const
{
  return colorMode_;
}

void GraphicShapeElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode GraphicShapeElement::visibilityMode() const
{
  return visibilityMode_;
}

void GraphicShapeElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString GraphicShapeElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void GraphicShapeElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString GraphicShapeElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void GraphicShapeElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void GraphicShapeElement::setExecuteMode(bool execute)
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
  onRuntimeStateReset();
  updateExecuteState();
}

bool GraphicShapeElement::isExecuteMode() const
{
  return executeMode_;
}

void GraphicShapeElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  onRuntimeConnectedChanged();
  if (executeMode_) {
    updateExecuteState();
  }
}

void GraphicShapeElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  onRuntimeVisibilityChanged();
  applyRuntimeVisibility();
}

void GraphicShapeElement::setRuntimeSeverity(short severity)
{
  const short normalized = normalizeRuntimeSeverity(severity);
  if (runtimeSeverity_ == normalized) {
    return;
  }
  runtimeSeverity_ = normalized;
  onRuntimeSeverityChanged();
}

void GraphicShapeElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(visible);
}

QColor GraphicShapeElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor GraphicShapeElement::effectiveForegroundColor() const
{
  const QColor baseColor = color_.isValid()
      ? color_
      : defaultForegroundColor();
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

void GraphicShapeElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_;
    QWidget::setVisible(visible);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

void GraphicShapeElement::updateExecuteState()
{
  applyRuntimeVisibility();
  onExecuteStateApplied();
}

void GraphicShapeElement::drawSelectionOutline(QPainter &painter,
    const QRect &rect) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect);
}

void GraphicShapeElement::onExecuteStateApplied()
{
  /* Use UpdateCoordinator for throttled updates with adaptive rate control.
   * This reduces CPU load when PVs update faster than we can paint. */
  if (executeMode_) {
    UpdateCoordinator::instance().requestUpdate(this);
  } else {
    update();
  }
}

void GraphicShapeElement::onRuntimeSeverityChanged()
{
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    onExecuteStateApplied();
  }
}

short GraphicShapeElement::normalizeRuntimeSeverity(short severity) const
{
  if (severity < 0) {
    return 0;
  }
  return std::min<short>(severity, 3);
}
