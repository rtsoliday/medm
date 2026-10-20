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
  const bool wasCosmetic = fill_ == RectangleFill::kOutline && adlLineWidth() == 0;
  fill_ = fill;
  updateCosmeticOutlinePadding(wasCosmetic,
      fill_ == RectangleFill::kOutline && adlLineWidth() == 0);
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
  const bool wasCosmetic = fill_ == RectangleFill::kOutline && adlLineWidth() == 0;
  const int clamped = std::max(1, width);
  if (!suppressLineWidthTracking_
      && (lineWidth_ != clamped || adlLineWidth_ != clamped)) {
    lineWidthEdited_ = true;
  }
  lineWidth_ = clamped;
  updateCosmeticOutlinePadding(wasCosmetic,
      fill_ == RectangleFill::kOutline && adlLineWidth() == 0);
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
  return lineWidthEdited_ ? lineWidth_ : std::max(0, adlLineWidth_);
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
  const QRect deviceRect = fill_ == RectangleFill::kSolid
      ? rect() : medmOutlineRect(rect(), adlLineWidth());

  if (fill_ == RectangleFill::kSolid) {
    // XFillRectangle covers the full width and height, including the last
    // row and column; leaving them transparent exposes graphics underneath.
    painter.fillRect(deviceRect, currentColor);
  } else {
    painter.setBrush(Qt::NoBrush);
    QPen pen(currentColor);
    pen.setWidth(lineWidth_);
    pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                         : Qt::SolidLine);
    painter.setPen(pen);
    if (deviceRect.width() > 0 && deviceRect.height() > 0) {
      painter.drawRect(deviceRect);
    }
  }

  if (isSelected()) {
    drawSelectionOutline(painter, deviceRect);
  }
}
