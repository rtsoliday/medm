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
  setLineWidth(0);
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
  const bool wasCosmetic = fill_ == RectangleFill::kOutline && lineWidth_ == 0;
  fill_ = fill;
  updateCosmeticOutlinePadding(wasCosmetic,
      fill_ == RectangleFill::kOutline && lineWidth_ == 0);
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
  const QRect drawRect = fill_ == RectangleFill::kSolid
      ? rect() : medmOutlineRect(rect(), lineWidth_);
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
    if (drawRect.width() > 0 && drawRect.height() > 0) {
      painter.drawArc(drawRect, startAngle, spanAngle);
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
