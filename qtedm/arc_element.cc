#include "arc_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

ArcElement::ArcElement(QWidget *parent)
  : GraphicShapeElement(parent)
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

  if (isSelected()) {
    drawSelectionOutline(painter, drawRect);
  }
}

int ArcElement::toQtAngle(int angle64) const
{
  if (angle64 >= 0) {
    return (angle64 + 2) / 4;
  }
  return (angle64 - 2) / 4;
}
