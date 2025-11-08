#include "polygon_element.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"

PolygonElement::PolygonElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setForegroundColor(defaultForegroundColor());
  setFill(RectangleFill::kSolid);
  setLineStyle(RectangleLineStyle::kSolid);
  setLineWidth(1);
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  designModeVisible_ = QWidget::isVisible();
  update();
}

void PolygonElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool PolygonElement::isSelected() const
{
  return selected_;
}

QColor PolygonElement::color() const
{
  return color_;
}

void PolygonElement::setForegroundColor(const QColor &color)
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

RectangleFill PolygonElement::fill() const
{
  return fill_;
}

void PolygonElement::setFill(RectangleFill fill)
{
  if (fill_ == fill) {
    return;
  }
  const RectangleFill oldFill = fill_;
  fill_ = fill;
  
  /* Recalculate geometry if fill mode changes between solid and outline,
   * as outline polygons need extra space for line width */
  const bool oldNeedsPadding = (oldFill != RectangleFill::kSolid);
  const bool newNeedsPadding = (fill_ != RectangleFill::kSolid);
  if (oldNeedsPadding != newNeedsPadding && !normalizedPoints_.isEmpty()) {
    const QVector<QPoint> points = absolutePoints();
    setAbsolutePoints(points);
  } else {
    update();
  }
}

RectangleLineStyle PolygonElement::lineStyle() const
{
  return lineStyle_;
}

void PolygonElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int PolygonElement::lineWidth() const
{
  return lineWidth_;
}

void PolygonElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  
  /* Recalculate geometry for outline polygons when line width changes,
   * as the bounding box needs to expand to accommodate the new width */
  if (fill_ != RectangleFill::kSolid && !normalizedPoints_.isEmpty()) {
    const QVector<QPoint> points = absolutePoints();
    setAbsolutePoints(points);
  } else {
    update();
  }
}

TextColorMode PolygonElement::colorMode() const
{
  return colorMode_;
}

void PolygonElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode PolygonElement::visibilityMode() const
{
  return visibilityMode_;
}

void PolygonElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString PolygonElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void PolygonElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString PolygonElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void PolygonElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void PolygonElement::setExecuteMode(bool execute)
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

bool PolygonElement::isExecuteMode() const
{
  return executeMode_;
}

void PolygonElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    updateExecuteState();
  }
}

void PolygonElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyRuntimeVisibility();
  }
}

void PolygonElement::setRuntimeSeverity(short severity)
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

void PolygonElement::setAbsolutePoints(const QVector<QPoint> &points)
{
  if (points.size() < 2) {
    return;
  }

  QVector<QPoint> effectivePoints = points;
  if (effectivePoints.first() != effectivePoints.last()) {
    effectivePoints.append(effectivePoints.first());
  }

  QPolygon polygon(effectivePoints);
  QRect bounding = polygon.boundingRect();
  
  /* Expand geometry to accommodate line width when outline fill, matching MEDM behavior.
   * For solid fill, line width doesn't affect bounding box since interior is filled. */
  if (fill_ != RectangleFill::kSolid) {
    const int halfWidth = lineWidth_ / 2;
    bounding.adjust(-halfWidth, -halfWidth, halfWidth, halfWidth);
  }
  
  if (bounding.width() <= 0) {
    bounding.setWidth(std::max(1, lineWidth_));
  }
  if (bounding.height() <= 0) {
    bounding.setHeight(std::max(1, lineWidth_));
  }

  const int widthSpan = std::max(1, bounding.width() - 1);
  const int heightSpan = std::max(1, bounding.height() - 1);
  const double width = static_cast<double>(widthSpan);
  const double height = static_cast<double>(heightSpan);

  normalizedPoints_.clear();
  normalizedPoints_.reserve(effectivePoints.size());
  for (const QPoint &point : effectivePoints) {
    const double nx = width > 0.0
        ? static_cast<double>(point.x() - bounding.left()) / width
        : 0.0;
    const double ny = height > 0.0
        ? static_cast<double>(point.y() - bounding.top()) / height
        : 0.0;
    normalizedPoints_.append(QPointF(std::clamp(nx, 0.0, 1.0),
        std::clamp(ny, 0.0, 1.0)));
  }

  QWidget::setGeometry(bounding);
  recalcLocalPolygon();
  update();
}

QVector<QPoint> PolygonElement::absolutePoints() const
{
  QVector<QPoint> points;
  if (normalizedPoints_.isEmpty()) {
    return points;
  }

  points.reserve(normalizedPoints_.size());
  const QRect globalRect = geometry();
  const int w = std::max(1, globalRect.width() - 1);
  const int h = std::max(1, globalRect.height() - 1);
  for (const QPointF &norm : normalizedPoints_) {
    const double clampedX = std::clamp(norm.x(), 0.0, 1.0);
    const double clampedY = std::clamp(norm.y(), 0.0, 1.0);
    const int x = globalRect.left()
        + static_cast<int>(std::round(clampedX * w));
    const int y = globalRect.top()
        + static_cast<int>(std::round(clampedY * h));
    points.append(QPoint(x, y));
  }
  return points;
}

bool PolygonElement::containsGlobalPoint(const QPoint &point) const
{
  if (localPolygon_.isEmpty()) {
    return false;
  }
  const QPoint localPoint = point - geometry().topLeft();
  return localPolygon_.containsPoint(localPoint, Qt::OddEvenFill);
}

void PolygonElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(visible);
}

void PolygonElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  if (localPolygon_.size() < 2) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();

  if (fill_ == RectangleFill::kSolid) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveColor);
    painter.drawPolygon(localPolygon_);
  } else {
    painter.setBrush(Qt::NoBrush);
    QPen pen(effectiveColor);
    pen.setWidth(lineWidth_);
    pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                         : Qt::SolidLine);
    painter.setPen(pen);
    painter.drawPolygon(localPolygon_);
  }

  if (selected_) {
    QPen selectionPen(Qt::black);
    selectionPen.setStyle(Qt::DashLine);
    selectionPen.setWidth(1);
    painter.setPen(selectionPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

void PolygonElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  recalcLocalPolygon();
}

QColor PolygonElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor PolygonElement::effectiveForegroundColor() const
{
  const QColor baseColor = color_.isValid() ? color_ : defaultForegroundColor();
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

void PolygonElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_;
    QWidget::setVisible(visible);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

void PolygonElement::updateExecuteState()
{
  applyRuntimeVisibility();
  update();
}

void PolygonElement::recalcLocalPolygon()
{
  localPolygon_.clear();
  if (normalizedPoints_.isEmpty()) {
    return;
  }

  const int w = std::max(1, width() - 1);
  const int h = std::max(1, height() - 1);
  for (const QPointF &norm : normalizedPoints_) {
    const double clampedX = std::clamp(norm.x(), 0.0, 1.0);
    const double clampedY = std::clamp(norm.y(), 0.0, 1.0);
    const int x = static_cast<int>(std::round(clampedX * w));
    const int y = static_cast<int>(std::round(clampedY * h));
    localPolygon_.append(QPoint(x, y));
  }
}

