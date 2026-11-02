#include "oval_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPen>

void fillX11Ellipse(QPainter &painter, const QRect &box, const QColor &color)
{
  const qint64 width = box.width();
  const qint64 height = box.height();
  if (width <= 0 || height <= 0) {
    return;
  }

  /* miFillArcSetup */
  qint64 y = height >> 1;
  const qint64 dy = height & 1;
  const qint64 yorg = box.y() + y;
  const qint64 xorg = box.x() + (width >> 1) + (width & 1);
  const qint64 dx = 1 - (width & 1);
  qint64 xk = 0;
  qint64 xm = 0;
  qint64 yk = 0;
  qint64 ym = 0;
  qint64 e = 0;
  if (width == height) {
    ym = 8;
    xm = 8;
    yk = y << 3;
    if (!dx) {
      xk = 0;
      e = -1;
    } else {
      ++y;
      yk += 4;
      xk = -4;
      e = -(y << 3);
    }
  } else {
    ym = (width * width) << 3;
    xm = (height * height) << 3;
    yk = y * ym;
    if (!dy) {
      yk -= ym >> 1;
    }
    if (!dx) {
      xk = 0;
      e = -(xm >> 3);
    } else {
      ++y;
      yk += ym;
      xk = -(xm >> 1);
      e = xk - yk;
    }
  }

  /* MIFILLARCSTEP and ADDSPANS */
  qint64 x = 0;
  while (y > 0) {
    e += yk;
    while (e >= 0) {
      ++x;
      xk -= xm;
      e += xk;
    }
    --y;
    yk -= ym;
    qint64 spanWidth = (x << 1) + dx;
    if (e == xk && spanWidth > 1) {
      --spanWidth;
    }
    const int left = static_cast<int>(xorg - x);
    painter.fillRect(QRect(left, static_cast<int>(yorg - y),
        static_cast<int>(spanWidth), 1), color);
    if ((y + dy) != 0 && (spanWidth > 1 || e != xk)) {
      painter.fillRect(QRect(left, static_cast<int>(yorg + y + dy),
          static_cast<int>(spanWidth), 1), color);
    }
  }
}

namespace {

/* Root of the distance equation by bisection, from D. Eberly, "Distance
 * from a Point to an Ellipse, an Ellipsoid, or a Hyperellipsoid". */
double ellipseDistanceRoot(double r0, double z0, double z1, double g)
{
  const double n0 = r0 * z0;
  double s0 = z1 - 1.0;
  double s1 = g < 0.0 ? 0.0 : std::hypot(n0, z1) - 1.0;
  double s = 0.0;
  for (int i = 0; i < 200; ++i) {
    s = (s0 + s1) / 2.0;
    if (s == s0 || s == s1) {
      break;
    }
    const double ratio0 = n0 / (s + r0);
    const double ratio1 = z1 / (s + 1.0);
    g = ratio0 * ratio0 + ratio1 * ratio1 - 1.0;
    if (g > 0.0) {
      s0 = s;
    } else if (g < 0.0) {
      s1 = s;
    } else {
      break;
    }
  }
  return s;
}

/* Distance from (x, y) to the ellipse with semi-axes a and b centered at
 * the origin. */
double ellipseDistance(double a, double b, double x, double y)
{
  x = std::abs(x);
  y = std::abs(y);
  if (a == b) {
    return std::abs(std::hypot(x, y) - a);
  }
  if (a < b) {
    std::swap(a, b);
    std::swap(x, y);
  }
  if (y > 0.0) {
    if (x > 0.0) {
      const double z0 = x / a;
      const double z1 = y / b;
      const double g = z0 * z0 + z1 * z1 - 1.0;
      if (g == 0.0) {
        return 0.0;
      }
      const double r0 = (a / b) * (a / b);
      const double s = ellipseDistanceRoot(r0, z0, z1, g);
      return std::hypot(r0 * x / (s + r0) - x, y / (s + 1.0) - y);
    }
    return std::abs(y - b);
  }
  const double numerator = a * x;
  const double denominator = a * a - b * b;
  if (numerator < denominator) {
    const double ratio = numerator / denominator;
    return std::hypot(a * ratio - x, b * std::sqrt(1.0 - ratio * ratio));
  }
  return std::abs(x - a);
}

} // namespace

QVector<QRect> x11EllipseRingSpans(const QRect &box, int lineWidth)
{
  QVector<QRect> spans;
  if (lineWidth < 1 || box.width() <= 0 || box.height() <= 0) {
    return spans;
  }
  const double a = box.width() / 2.0;
  const double b = box.height() / 2.0;
  const double half = lineWidth / 2.0;
  const double cx = box.x() + a;
  const double cy = box.y() + b;
  /* X spans are half open: a pixel exactly half a line width from the
   * ellipse belongs to the ring at its top and left outer edge and its
   * bottom and right inner edge, but not at the opposite ones. */
  const auto inRing = [&](int x, int y) {
    const double dx = x - cx;
    const double dy = y - cy;
    const double distance = ellipseDistance(a, b, dx, dy);
    if (distance != half) {
      return distance < half;
    }
    const bool outside = (dx * dx) / (a * a) + (dy * dy) / (b * b) > 1.0;
    const bool before = dx != 0.0 ? dx < 0.0 : dy < 0.0;
    return outside == before;
  };
  const int top = static_cast<int>(std::floor(cy - b - half));
  const int bottom = static_cast<int>(std::ceil(cy + b + half));
  const int left = static_cast<int>(std::floor(cx - a - half));
  const int right = static_cast<int>(std::ceil(cx + a + half));

  /* In each row the ring is a run of pixels from each side until the hole,
   * or one run through the middle. */
  for (int y = top; y <= bottom; ++y) {
    /* A ring pixel lies within half a line width of a point of the ellipse
     * at least nearest above or below its center, so no farther across than
     * reach. Scan from there, not from the box edges: the corners outside
     * the ring held most of the pixels tested. */
    const double nearest = std::max(0.0, std::abs(y - cy) - half);
    if (nearest > b) {
      continue;
    }
    const double across = nearest / b;
    const double reach =
        a * std::sqrt(std::max(0.0, 1.0 - across * across)) + half;
    const int rowLeft = std::max(left,
        static_cast<int>(std::floor(cx - reach)) - 1);
    const int rowRight = std::min(right,
        static_cast<int>(std::ceil(cx + reach)) + 1);
    int leftEnd = rowLeft;
    while (leftEnd <= rowRight && !inRing(leftEnd, y)) {
      ++leftEnd;
    }
    if (leftEnd > rowRight) {
      continue;
    }
    const int leftStart = leftEnd;
    while (leftEnd <= rowRight && inRing(leftEnd, y)) {
      ++leftEnd;
    }
    int rightStart = rowRight;
    while (rightStart >= leftEnd && !inRing(rightStart, y)) {
      --rightStart;
    }
    const int rightEnd = rightStart + 1;
    while (rightStart >= leftEnd && inRing(rightStart, y)) {
      --rightStart;
    }
    ++rightStart;
    if (rightEnd <= leftEnd) {
      spans.append(QRect(leftStart, y, leftEnd - leftStart, 1));
    } else {
      spans.append(QRect(leftStart, y, leftEnd - leftStart, 1));
      spans.append(QRect(rightStart, y, rightEnd - rightStart, 1));
    }
  }
  return spans;
}

OvalElement::OvalElement(QWidget *parent)
  : GraphicShapeElement(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setForegroundColor(palette().color(QPalette::WindowText));
  setFill(RectangleFill::kOutline);
  setLineStyle(RectangleLineStyle::kSolid);
  setLineWidth(0);
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
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
  const bool wasCosmetic = fill_ == RectangleFill::kOutline && lineWidth_ == 0;
  fill_ = fill;
  updateCosmeticOutlinePadding(wasCosmetic,
      fill_ == RectangleFill::kOutline && lineWidth_ == 0);
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
  const int clamped = std::max(0, width);
  if (lineWidth_ == clamped) {
    return;
  }
  const bool wasCosmetic = fill_ == RectangleFill::kOutline && lineWidth_ == 0;
  lineWidth_ = clamped;
  updateCosmeticOutlinePadding(wasCosmetic,
      fill_ == RectangleFill::kOutline && lineWidth_ == 0);
  update();
}

bool OvalElement::containsGlobalPoint(const QPoint &point) const
{
  if (!geometry().contains(point)) {
    return false;
  }

  const QRect drawRect = fill_ == RectangleFill::kSolid
      ? rect() : medmOutlineRect(rect(), lineWidth_);
  if (drawRect.width() <= 0 || drawRect.height() <= 0) {
    return false;
  }

  const QPointF localPoint = QPointF(point - geometry().topLeft());
  auto ellipsePathForRect = [](const QRect &ellipseRect) {
    QPainterPath path;
    if (ellipseRect.width() > 0 && ellipseRect.height() > 0) {
      path.addEllipse(QRectF(ellipseRect));
    }
    return path;
  };

  if (fill_ == RectangleFill::kSolid) {
    return ellipsePathForRect(drawRect).contains(localPoint);
  }

  const QRect outlineRect = drawRect;
  if (outlineRect.width() <= 0 || outlineRect.height() <= 0) {
    return false;
  }

  QPainterPathStroker stroker;
  const qreal pickRadius = std::max<qreal>(3.0, static_cast<qreal>(lineWidth_));
  stroker.setWidth(pickRadius * 2.0);
  stroker.setCapStyle(Qt::RoundCap);
  stroker.setJoinStyle(Qt::RoundJoin);
  return stroker.createStroke(ellipsePathForRect(outlineRect)).contains(localPoint);
}

void OvalElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();
  const QRect drawRect = fill_ == RectangleFill::kSolid
      ? rect() : medmOutlineRect(rect(), lineWidth_);

  if (fill_ == RectangleFill::kSolid) {
    fillX11Ellipse(painter, drawRect, effectiveColor);
  } else if (lineWidth_ >= 1 && lineStyle_ == RectangleLineStyle::kSolid) {
    if (ringBox_ != drawRect || ringLineWidth_ != lineWidth_) {
      ringBox_ = drawRect;
      ringLineWidth_ = lineWidth_;
      ringSpans_ = x11EllipseRingSpans(drawRect, lineWidth_);
    }
    for (const QRect &span : std::as_const(ringSpans_)) {
      painter.fillRect(span, effectiveColor);
    }
  } else {
    painter.setBrush(Qt::NoBrush);
    QPen pen = medmLinePen(effectiveColor, lineWidth_, lineStyle_);
    painter.setPen(pen);
    if (drawRect.width() > 0 && drawRect.height() > 0) {
      painter.drawEllipse(drawRect);
    }
  }

  if (isSelected()) {
    drawSelectionOutline(painter, drawRect);
  }
}
