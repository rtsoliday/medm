#include "oval_element.h"

#include <algorithm>

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
