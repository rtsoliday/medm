#include "polyline_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

PolylineElement::PolylineElement(QWidget *parent)
  : QWidget(parent)
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

void PolylineElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool PolylineElement::isSelected() const
{
  return selected_;
}

QColor PolylineElement::color() const
{
  return color_;
}

void PolylineElement::setForegroundColor(const QColor &color)
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

TextColorMode PolylineElement::colorMode() const
{
  return colorMode_;
}

void PolylineElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode PolylineElement::visibilityMode() const
{
  return visibilityMode_;
}

void PolylineElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString PolylineElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void PolylineElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString PolylineElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void PolylineElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void PolylineElement::setAbsolutePoints(const QVector<QPoint> &points)
{
  if (points.size() < 2) {
    return;
  }

  QPolygon polygon(points);
  QRect bounding = polygon.boundingRect();
  if (bounding.width() <= 0) {
    bounding.setWidth(1);
  }
  if (bounding.height() <= 0) {
    bounding.setHeight(1);
  }

  const int widthSpan = std::max(1, bounding.width() - 1);
  const int heightSpan = std::max(1, bounding.height() - 1);
  const double width = static_cast<double>(widthSpan);
  const double height = static_cast<double>(heightSpan);

  normalizedPoints_.clear();
  normalizedPoints_.reserve(points.size());
  for (const QPoint &point : points) {
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

  const QColor effectiveColor = color_.isValid() ? color_ : defaultForegroundColor();

  QPen pen(effectiveColor);
  pen.setWidth(lineWidth_);
  pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                       : Qt::SolidLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPolyline(localPolyline_);

  if (selected_) {
    QPen selectionPen(Qt::black);
    selectionPen.setStyle(Qt::DashLine);
    selectionPen.setWidth(1);
    painter.setPen(selectionPen);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

void PolylineElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  recalcLocalPolyline();
}

QColor PolylineElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
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

