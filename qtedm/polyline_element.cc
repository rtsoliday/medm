#include "polyline_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QVariant>

PolylineElement::PolylineElement(QWidget *parent)
  : GraphicShapeElement(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setForegroundColor(defaultForegroundColor());
  setLineStyle(RectangleLineStyle::kSolid);
  setLineWidth(1);
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  update();
}

RectangleLineStyle PolylineElement::lineStyle() const
{
  return lineStyle_;
}

void PolylineElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int PolylineElement::lineWidth() const
{
  return lineWidth_;
}

void PolylineElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

void PolylineElement::setAbsolutePoints(const QVector<QPoint> &points)
{
  if (points.size() < 2) {
    return;
  }

  QPolygon polygon(points);
  QRect bounding = polygon.boundingRect();
  
  /* Expand geometry to accommodate line width, matching MEDM behavior */
  const int halfWidth = lineWidth_ / 2;
  bounding.adjust(-halfWidth, -halfWidth, halfWidth, halfWidth);
  
  if (bounding.width() <= 0) {
    bounding.setWidth(std::max(1, lineWidth_));
  }
  if (bounding.height() <= 0) {
    bounding.setHeight(std::max(1, lineWidth_));
  }

  QRect targetRect = bounding;
  const QVariant original = property("_adlOriginalGeometry");
  const bool geometryEdited = property("_adlGeometryEdited").toBool();
  if (original.isValid() && original.canConvert<QRect>()
      && !geometryEdited) {
    targetRect = original.toRect();
  }

  const int widthSpan = std::max(1, targetRect.width());
  const int heightSpan = std::max(1, targetRect.height());
  const double width = static_cast<double>(widthSpan);
  const double height = static_cast<double>(heightSpan);

  normalizedPoints_.clear();
  normalizedPoints_.reserve(points.size());
  for (const QPoint &point : points) {
    const double nx = width > 0.0
        ? static_cast<double>(point.x() - targetRect.left()) / width
        : 0.0;
    const double ny = height > 0.0
        ? static_cast<double>(point.y() - targetRect.top()) / height
        : 0.0;
    normalizedPoints_.append(QPointF(std::clamp(nx, 0.0, 1.0),
        std::clamp(ny, 0.0, 1.0)));
  }

  QWidget::setGeometry(targetRect);
  recalcLocalPolyline();
  update();
}

QVector<QPoint> PolylineElement::absolutePoints() const
{
  QVector<QPoint> points;
  if (normalizedPoints_.isEmpty()) {
    return points;
  }

  points.reserve(normalizedPoints_.size());
  const QRect globalRect = geometry();
  const int w = std::max(1, globalRect.width());
  const int h = std::max(1, globalRect.height());
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

bool PolylineElement::containsGlobalPoint(const QPoint &point) const
{
  if (!geometry().contains(point)) {
    return false;
  }

  const QVector<QPoint> points = absolutePoints();
  if (points.size() < 2) {
    return false;
  }

  const double tolerance = std::max(3, lineWidth_);
  const double toleranceSquared = tolerance * tolerance;
  const QPointF p(point);
  for (int i = 0; i < points.size() - 1; ++i) {
    const QPointF a = points[i];
    const QPointF b = points[i + 1];
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double lengthSquared = dx * dx + dy * dy;
    double t = 0.0;
    if (lengthSquared > 0.0) {
      t = ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / lengthSquared;
      t = std::clamp(t, 0.0, 1.0);
    }
    const double projX = a.x() + t * dx;
    const double projY = a.y() + t * dy;
    const double distX = p.x() - projX;
    const double distY = p.y() - projY;
    const double distSquared = distX * distX + distY * distY;
    if (distSquared <= toleranceSquared) {
      return true;
    }
  }
  return false;
}

void PolylineElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  if (localPolyline_.size() < 2) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();

  QPen pen(effectiveColor);
  pen.setWidth(lineWidth_);
  pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                       : Qt::SolidLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPolyline(localPolyline_);

  if (isSelected()) {
    drawSelectionOutline(painter, rect().adjusted(0, 0, -1, -1));
  }
}

void PolylineElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  recalcLocalPolyline();
}

void PolylineElement::recalcLocalPolyline()
{
  localPolyline_.clear();
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
    localPolyline_.append(QPoint(x, y));
  }
}
