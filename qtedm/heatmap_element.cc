#include "heatmap_element.h"

#include <algorithm>
#include <cmath>

#include <QFontMetrics>
#include <QPainter>
#include "text_font_utils.h"
#include <QPalette>
#include <QPen>


namespace {
constexpr int kDefaultDimension = 10;
constexpr int kLegendBarWidth = 12;
constexpr int kLegendPadding = 6;
constexpr int kLegendNumberPadding = 12;
constexpr int kTitleFontHeight = 24;
constexpr int kLegendFontHeight = 12;
}

HeatmapElement::HeatmapElement(QWidget *parent)
  : GraphicShapeElement(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  xDimension_ = kDefaultDimension;
  yDimension_ = kDefaultDimension;
}

QString HeatmapElement::dataChannel() const
{
  return dataChannel_;
}

void HeatmapElement::setDataChannel(const QString &channel)
{
  if (dataChannel_ == channel) {
    return;
  }
  dataChannel_ = channel.trimmed();
}

QString HeatmapElement::title() const
{
  return title_;
}

void HeatmapElement::setTitle(const QString &title)
{
  const QString trimmed = title.trimmed();
  if (title_ == trimmed) {
    return;
  }
  title_ = trimmed;
  invalidateCache();
}

HeatmapDimensionSource HeatmapElement::xDimensionSource() const
{
  return xDimensionSource_;
}

void HeatmapElement::setXDimensionSource(HeatmapDimensionSource source)
{
  if (xDimensionSource_ == source) {
    return;
  }
  xDimensionSource_ = source;
  invalidateCache();
}

HeatmapDimensionSource HeatmapElement::yDimensionSource() const
{
  return yDimensionSource_;
}

void HeatmapElement::setYDimensionSource(HeatmapDimensionSource source)
{
  if (yDimensionSource_ == source) {
    return;
  }
  yDimensionSource_ = source;
  invalidateCache();
}

int HeatmapElement::xDimension() const
{
  return xDimension_;
}

void HeatmapElement::setXDimension(int value)
{
  const int clamped = std::max(1, value);
  if (xDimension_ == clamped) {
    return;
  }
  xDimension_ = clamped;
  invalidateCache();
}

int HeatmapElement::yDimension() const
{
  return yDimension_;
}

void HeatmapElement::setYDimension(int value)
{
  const int clamped = std::max(1, value);
  if (yDimension_ == clamped) {
    return;
  }
  yDimension_ = clamped;
  invalidateCache();
}

QString HeatmapElement::xDimensionChannel() const
{
  return xDimensionChannel_;
}

void HeatmapElement::setXDimensionChannel(const QString &channel)
{
  if (xDimensionChannel_ == channel) {
    return;
  }
  xDimensionChannel_ = channel.trimmed();
}

QString HeatmapElement::yDimensionChannel() const
{
  return yDimensionChannel_;
}

void HeatmapElement::setYDimensionChannel(const QString &channel)
{
  if (yDimensionChannel_ == channel) {
    return;
  }
  yDimensionChannel_ = channel.trimmed();
}

HeatmapOrder HeatmapElement::order() const
{
  return order_;
}

void HeatmapElement::setOrder(HeatmapOrder order)
{
  if (order_ == order) {
    return;
  }
  order_ = order;
  invalidateCache();
}

bool HeatmapElement::invertGreyscale() const
{
  return invertGreyscale_;
}

void HeatmapElement::setInvertGreyscale(bool invert)
{
  if (invertGreyscale_ == invert) {
    return;
  }
  invertGreyscale_ = invert;
  invalidateCache();
}

void HeatmapElement::setRuntimeData(const QVector<double> &values)
{
  runtimeValues_ = values;
  runtimeDataValid_ = !runtimeValues_.isEmpty();
  invalidateCache();
}

void HeatmapElement::setRuntimeDimensions(int xDim, int yDim)
{
  runtimeXDimension_ = xDim;
  runtimeYDimension_ = yDim;
  runtimeDimensionsValid_ = (runtimeXDimension_ > 0 && runtimeYDimension_ > 0);
  invalidateCache();
}

void HeatmapElement::clearRuntimeState()
{
  runtimeValues_.clear();
  runtimeDataValid_ = false;
  runtimeXDimension_ = 0;
  runtimeYDimension_ = 0;
  runtimeDimensionsValid_ = false;
  runtimeRangeValid_ = false;
  runtimeMinValue_ = 0.0;
  runtimeMaxValue_ = 0.0;
  invalidateCache();
}

void HeatmapElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

  const QRect drawRect = rect().adjusted(0, 0, -1, -1);
  if (!cacheValid_) {
    rebuildImage();
  }

    const QFont titleFont = medmTextFieldFont(kTitleFontHeight);
    const QFont legendFont = medmTextFieldFont(kLegendFontHeight);
    const QFontMetrics legendMetrics(legendFont.family().isEmpty()
      ? font()
      : legendFont);
    const bool canShowLegend = isExecuteMode() && runtimeDataValid_ && runtimeRangeValid_;
    const QString minLabel = QString::number(runtimeMinValue_, 'g', 6);
    const QString maxLabel = QString::number(runtimeMaxValue_, 'g', 6);
    const int labelWidth = std::max(legendMetrics.horizontalAdvance(minLabel),
      legendMetrics.horizontalAdvance(maxLabel));
    const int legendWidth = kLegendBarWidth + kLegendPadding + labelWidth
      + kLegendPadding + kLegendNumberPadding;

  QRect heatmapRect = drawRect;
  QRect legendRect;

  const QString titleText = title_.trimmed();
  if (!titleText.isEmpty()) {
    const QFont titleFontToUse = titleFont.family().isEmpty()
        ? font()
        : titleFont;
    painter.setFont(titleFontToUse);
    const QFontMetrics titleMetrics(titleFontToUse);
    const int titleHeight = titleMetrics.height();
    const QRect titleRect(drawRect.left(), drawRect.top(),
        drawRect.width(), titleHeight);
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter, titleText);
    heatmapRect.setTop(titleRect.bottom() + 2);
    painter.setFont(font());
  }

  if (canShowLegend && heatmapRect.width() > (legendWidth + 10)) {
    heatmapRect.setRight(drawRect.right() - legendWidth);
    legendRect = QRect(heatmapRect.right() + 1, heatmapRect.top(),
        drawRect.right() - heatmapRect.right(), heatmapRect.height());
  }

  if (!cachedImage_.isNull()) {
    painter.drawImage(heatmapRect, cachedImage_);
  } else {
    painter.fillRect(heatmapRect, backgroundColor());
    painter.setPen(QPen(borderColor(), 1, Qt::DashLine));
    painter.drawRect(heatmapRect);
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawLine(heatmapRect.topLeft(), heatmapRect.bottomRight());
    painter.drawLine(heatmapRect.topRight(), heatmapRect.bottomLeft());
  }

  if (!legendRect.isEmpty()) {
    painter.fillRect(legendRect, backgroundColor());
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawLine(legendRect.topLeft(), legendRect.bottomLeft());

    QRect barRect = legendRect.adjusted(kLegendPadding,
      kLegendPadding, -kLegendPadding - labelWidth - kLegendNumberPadding,
      -kLegendPadding);
    if (barRect.height() > 4 && barRect.width() > 0) {
      QLinearGradient gradient(barRect.topLeft(), barRect.bottomLeft());
      const QColor topColor = invertGreyscale_
          ? QColor(255, 255, 255)
          : QColor(0, 0, 0);
      const QColor bottomColor = invertGreyscale_
          ? QColor(0, 0, 0)
          : QColor(255, 255, 255);
      gradient.setColorAt(0.0, topColor);
      gradient.setColorAt(1.0, bottomColor);
      painter.fillRect(barRect, gradient);
      painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
      painter.drawRect(barRect.adjusted(0, 0, -1, -1));
    }

    QRect labelRect = legendRect.adjusted(
      kLegendPadding + kLegendBarWidth + kLegendPadding, kLegendPadding,
      -kLegendPadding - kLegendNumberPadding, -kLegendPadding);
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    if (!legendFont.family().isEmpty()) {
      painter.setFont(legendFont);
    }
    painter.drawText(labelRect, Qt::AlignTop | Qt::AlignLeft, maxLabel);
    painter.drawText(labelRect, Qt::AlignBottom | Qt::AlignLeft, minLabel);
    if (!legendFont.family().isEmpty()) {
      painter.setFont(font());
    }
  }

  painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
  painter.drawRect(drawRect);

  if (isSelected()) {
    drawSelectionOutline(painter, drawRect);
  }
}

void HeatmapElement::onRuntimeStateReset()
{
  clearRuntimeState();
}

void HeatmapElement::onRuntimeConnectedChanged()
{
  invalidateCache();
}

void HeatmapElement::onRuntimeSeverityChanged()
{
  if (isExecuteMode()) {
    onExecuteStateApplied();
  }
}

void HeatmapElement::invalidateCache()
{
  cacheValid_ = false;
  update();
}

QSize HeatmapElement::effectiveDimensions() const
{
  const bool useRuntimeX = isExecuteMode()
    && xDimensionSource_ == HeatmapDimensionSource::kChannel
    && runtimeXDimension_ > 0;
  const bool useRuntimeY = isExecuteMode()
    && yDimensionSource_ == HeatmapDimensionSource::kChannel
    && runtimeYDimension_ > 0;
  const int effectiveX = useRuntimeX ? runtimeXDimension_ : xDimension_;
  const int effectiveY = useRuntimeY ? runtimeYDimension_ : yDimension_;
  if (effectiveX <= 0 || effectiveY <= 0) {
    return QSize();
  }
  return QSize(effectiveX, effectiveY);
}

void HeatmapElement::rebuildImage()
{
  cacheValid_ = true;
  runtimeRangeValid_ = false;

  const QSize dims = effectiveDimensions();
  if (dims.isEmpty()) {
    cachedImage_ = QImage();
    return;
  }

  const int width = dims.width();
  const int height = dims.height();
  const int totalCells = width * height;
  if (totalCells <= 0) {
    cachedImage_ = QImage();
    return;
  }

  QVector<double> values = runtimeValues_;
  if (!isExecuteMode()) {
    values.clear();
  }

  const int available = std::min(values.size(), totalCells);
  if (available <= 0) {
    cachedImage_ = QImage();
    return;
  }

  bool haveValue = false;
  double minValue = 0.0;
  double maxValue = 0.0;
  for (int i = 0; i < available; ++i) {
    const double v = values[i];
    if (std::isnan(v) || std::isinf(v)) {
      continue;
    }
    if (!haveValue) {
      minValue = v;
      maxValue = v;
      haveValue = true;
    } else {
      minValue = std::min(minValue, v);
      maxValue = std::max(maxValue, v);
    }
  }
  if (!haveValue) {
    cachedImage_ = QImage();
    return;
  }
  if (runtimeRangeValid_) {
    minValue = std::min(minValue, runtimeMinValue_);
    maxValue = std::max(maxValue, runtimeMaxValue_);
  }
  runtimeRangeValid_ = true;
  runtimeMinValue_ = minValue;
  runtimeMaxValue_ = maxValue;

  const double range = maxValue - minValue;

  cachedImage_ = QImage(width, height, QImage::Format_RGB32);
  cachedImage_.fill(backgroundColor());

  auto indexFor = [&](int x, int y) {
    if (order_ == HeatmapOrder::kRowMajor) {
      return y * width + x;
    }
    return x * height + y;
  };

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int index = indexFor(x, y);
      if (index < 0 || index >= available) {
        continue;
      }
      const double value = values[index];
      double ratio = 0.0;
      if (range > 0.0) {
        ratio = (value - minValue) / range;
      }
      ratio = std::clamp(ratio, 0.0, 1.0);
      const double intensity = invertGreyscale_ ? ratio : (1.0 - ratio);
      const int gray = static_cast<int>(std::round(intensity * 255.0));
      const QColor color(gray, gray, gray);
      cachedImage_.setPixelColor(x, y, color);
    }
  }
}

QColor HeatmapElement::backgroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  return palette().color(QPalette::Window);
}

QColor HeatmapElement::borderColor() const
{
  return effectiveForegroundColor();
}
