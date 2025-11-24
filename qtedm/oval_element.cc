#include "oval_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

OvalElement::OvalElement(QWidget *parent)
  : GraphicShapeElement(parent)
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
  fill_ = fill;
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
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

void OvalElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();
  QRect drawRect = rect().adjusted(0, 0, -1, -1);

  if (fill_ == RectangleFill::kSolid) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveColor);
    painter.drawEllipse(drawRect);
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
      painter.drawEllipse(outlineRect);
    }
  }

  if (isSelected()) {
    drawSelectionOutline(painter, drawRect);
  }
}
