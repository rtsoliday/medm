#include "rectangle_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

RectangleElement::RectangleElement(QWidget *parent)
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

RectangleFill RectangleElement::fill() const
{
  return fill_;
}

void RectangleElement::setFill(RectangleFill fill)
{
  if (fill_ == fill) {
    return;
  }
  fill_ = fill;
  update();
}

RectangleLineStyle RectangleElement::lineStyle() const
{
  return lineStyle_;
}

void RectangleElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int RectangleElement::lineWidth() const
{
  return lineWidth_;
}

void RectangleElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (!suppressLineWidthTracking_ && lineWidth_ != clamped) {
    lineWidthEdited_ = true;
  }
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

void RectangleElement::setLineWidthFromAdl(int width)
{
  const bool previous = suppressLineWidthTracking_;
  suppressLineWidthTracking_ = true;
  setLineWidth(width);
  suppressLineWidthTracking_ = previous;
  lineWidthEdited_ = false;
}

int RectangleElement::adlLineWidth() const
{
  return adlLineWidth_;
}

void RectangleElement::setAdlLineWidth(int width, bool hasProperty)
{
  adlLineWidth_ = width;
  hasAdlLineWidthProperty_ = hasProperty;
  lineWidthEdited_ = false;
}

bool RectangleElement::shouldSerializeLineWidth() const
{
  if (hasAdlLineWidthProperty_) {
    return true;
  }
  return lineWidthEdited_;
}

void RectangleElement::setGeometry(const QRect &rect)
{
  const QSize previousSize = QWidget::geometry().size();
  if (!suppressGeometryTracking_ && hasOriginalAdlSize_ &&
      rect.size() != previousSize) {
    sizeEdited_ = true;
  }
  QWidget::setGeometry(rect);
}

void RectangleElement::initializeFromAdlGeometry(const QRect &geometry,
    const QSize &adlSize)
{
  originalAdlSize_ = adlSize;
  hasOriginalAdlSize_ = true;
  sizeEdited_ = false;
  setGeometryWithoutTracking(geometry);
}

void RectangleElement::setGeometryWithoutTracking(const QRect &geometry)
{
  const bool previous = suppressGeometryTracking_;
  suppressGeometryTracking_ = true;
  QWidget::setGeometry(geometry);
  suppressGeometryTracking_ = previous;
}

QRect RectangleElement::geometryForSerialization() const
{
  QRect serialized = geometry();
  if (hasOriginalAdlSize_ && !sizeEdited_) {
    serialized.setSize(originalAdlSize_);
  }
  return serialized;
}

void RectangleElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor currentColor = effectiveForegroundColor();
  QRect deviceRect;
  /* Mimic medm bug: outline rectangles are drawn at (+1,+1) with
     dimensions reduced by 1 pixel */
  if (fill_ == RectangleFill::kSolid) {
    deviceRect = rect().adjusted(0, 0, -1, -1);
  } else {
    deviceRect = (adlLineWidth_ == 0) ? rect().adjusted(0, 0, -1, -1) : rect().adjusted(1, 1, -1, -1);
  }

  if (fill_ == RectangleFill::kSolid) {
    // Draw solid fill with one less pixel on right and bottom
    painter.fillRect(deviceRect, currentColor);
  } else {
    // MEDM's X11 renderer extends outline mode one pixel past the widget
    // bounds, so expand the Qt outline by a pixel on each side to match.
    painter.setBrush(Qt::NoBrush);
    QPen pen(currentColor);
    pen.setWidth(lineWidth_);
    pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                         : Qt::SolidLine);
    painter.setPen(pen);
    
    const int halfWidth = (lineWidth_ + 1) / 2;
    const int outlineInset = std::max(0, halfWidth - 1);
    QRect outlineRect = deviceRect.adjusted(-1, -1, 1, 1);
    outlineRect = outlineRect.adjusted(outlineInset, outlineInset,
                                       -outlineInset, -outlineInset);
    outlineRect = outlineRect.intersected(deviceRect);
    if (outlineRect.width() <= 0 || outlineRect.height() <= 0) {
      outlineRect = deviceRect;
    }
    painter.drawRect(outlineRect);
  }

  if (isSelected()) {
    drawSelectionOutline(painter, deviceRect);
  }
}
