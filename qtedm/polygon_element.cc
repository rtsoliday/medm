#include "polygon_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

PolygonElement::PolygonElement(QWidget *parent)
  : GraphicShapeElement(parent)
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

  if (isSelected()) {
    drawSelectionOutline(painter, rect().adjusted(0, 0, -1, -1));
  }
}

void PolygonElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  recalcLocalPolygon();
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
