#include <QPainterPath>
#include "heatmap_element.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include "window_utils.h"
#include "heatmap_runtime.h"

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

  interactionTimer_.setSingleShot(true);
  interactionTimer_.setInterval(2000);
  QObject::connect(&interactionTimer_, &QTimer::timeout, [this]() {
    HeatmapRuntime::setGlobalUpdatesPaused(false);
  });
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


void HeatmapElement::setRuntimeSharedData(std::shared_ptr<const double> sharedData, size_t size)
{
  runtimeSharedValues_ = sharedData;
  runtimeSharedSize_ = size;
  runtimeValues_.clear();
  invalidateCache();
  update();
}

void HeatmapElement::setRuntimeData(const QVector<double> &values)
{
  runtimeValues_ = values;
  runtimeSharedValues_.reset();
  runtimeSharedSize_ = 0;
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
  lastHeatmapRect_ = heatmapRect;

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

  const int profileSize = std::min(40, std::min(heatmapRect.width() / 4, heatmapRect.height() / 4));
  QRect topProfileRect;
  QRect rightProfileRect;

  if (showTopProfile_ && profileSize > 0) {
    topProfileRect = QRect(heatmapRect.left(), heatmapRect.top(), heatmapRect.width(), profileSize);
    heatmapRect.setTop(heatmapRect.top() + profileSize + 2);
  }
  if (showRightProfile_ && profileSize > 0) {
    rightProfileRect = QRect(heatmapRect.right() - profileSize + 1, heatmapRect.top(), profileSize, heatmapRect.height());
    heatmapRect.setRight(heatmapRect.right() - profileSize - 2);
    if (showTopProfile_) {
      topProfileRect.setWidth(heatmapRect.width());
    }
  }
  if (!cachedImage_.isNull()) {
    QImage imageToDraw = cachedImage_;
    const QSize targetSize = heatmapRect.size();
    if (targetSize.width() > 0 && targetSize.height() > 0
        && (cachedImage_.width() > targetSize.width()
            || cachedImage_.height() > targetSize.height())) {
      if (downsampledCachedImage_.isNull() || downsampledTargetSize_ != targetSize) {
        downsampledCachedImage_ = maxPoolDownsample(cachedImage_, targetSize);
        downsampledTargetSize_ = targetSize;
      }
      imageToDraw = downsampledCachedImage_;
    }

    if (!imageToDraw.isNull()
        && imageToDraw.size() == targetSize) {
      painter.drawImage(heatmapRect.topLeft(), imageToDraw);
    } else {
      painter.drawImage(heatmapRect, imageToDraw);
    }
  } else {
    painter.fillRect(heatmapRect, backgroundColor());
    painter.setPen(QPen(borderColor(), 1, Qt::DashLine));
    painter.drawRect(heatmapRect);
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawLine(heatmapRect.topLeft(), heatmapRect.bottomRight());
    painter.drawLine(heatmapRect.topRight(), heatmapRect.bottomLeft());
  }

  if (!topProfileRect.isEmpty() && !topProfileData_.isEmpty()) {
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawRect(topProfileRect);
    
    QPainterPath path;
    bool hasStart = false;
    double range = topProfileMax_ - topProfileMin_;
    double y_scale = range > 0 ? (topProfileRect.height() - 4.0) / range : 0;
    for (int i = 0; i < topProfileData_.size(); ++i) {
      double v = topProfileData_[i];
      if (std::isnan(v)) continue;
      double x = topProfileRect.left() + 1 + (i + 0.5) * (topProfileRect.width() - 2) / topProfileData_.size();
      double y = topProfileRect.bottom() - 1 - (v - topProfileMin_) * y_scale;
      if (!hasStart) {
        path.moveTo(x, y);
        hasStart = true;
      } else {
        path.lineTo(x, y);
      }
    }
    if (hasStart) {
      painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
      painter.drawPath(path);
    }
  }

  if (!rightProfileRect.isEmpty() && !rightProfileData_.isEmpty()) {
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawRect(rightProfileRect);
    
    QPainterPath path;
    bool hasStart = false;
    double range = rightProfileMax_ - rightProfileMin_;
    double x_scale = range > 0 ? (rightProfileRect.width() - 4.0) / range : 0;
    for (int i = 0; i < rightProfileData_.size(); ++i) {
      double v = rightProfileData_[i];
      if (std::isnan(v)) continue;
      double y = rightProfileRect.top() + 1 + (i + 0.5) * (rightProfileRect.height() - 2) / rightProfileData_.size();
      double x = rightProfileRect.left() + 1 + (v - rightProfileMin_) * x_scale;
      if (!hasStart) {
        path.moveTo(x, y);
        hasStart = true;
      } else {
        path.lineTo(x, y);
      }
    }
    if (hasStart) {
      painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
      painter.drawPath(path);
    }
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
  downsampledCachedImage_ = QImage();
  downsampledTargetSize_ = QSize();
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

  const double* dataValues = nullptr;
  int dataCount = 0;

  if (isExecuteMode()) {
    if (runtimeSharedValues_) {
      dataValues = runtimeSharedValues_.get();
      dataCount = runtimeSharedSize_;
    } else {
      dataValues = runtimeValues_.constData();
      dataCount = runtimeValues_.size();
    }
  }

  const int available = std::min(dataCount, totalCells);
  if (available <= 0) {
    cachedImage_ = QImage();
    return;
  }

  int xStart = 0;
  int xEnd = width;
  int yStart = 0;
  int yEnd = height;
  
  if (zoomed_) {
    xStart = std::floor(zoomXMin_ * width);
    xEnd = std::ceil(zoomXMax_ * width);
    yStart = std::floor(zoomYMin_ * height);
    yEnd = std::ceil(zoomYMax_ * height);
    
    if (xStart < 0) xStart = 0;
    if (xEnd > width) xEnd = width;
    if (yStart < 0) yStart = 0;
    if (yEnd > height) yEnd = height;
    
    if (xEnd <= xStart) xEnd = xStart + 1;
    if (yEnd <= yStart) yEnd = yStart + 1;
  }
  
  const int zoomedWidth = xEnd - xStart;
  const int zoomedHeight = yEnd - yStart;

  bool haveValue = false;
  double minValue = 0.0;
  double maxValue = 0.0;
  
  // Calculate min/max over the visible zoomed range
  for (int y = yStart; y < yEnd; ++y) {
    for (int x = xStart; x < xEnd; ++x) {
      int index = (order_ == HeatmapOrder::kRowMajor) ? (y * width + x) : (x * height + y);
      if (index < available) {
        const double v = dataValues[index];
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
    }
  }
  
  if (!haveValue) {
    cachedImage_ = QImage();
    return;
  }
  
  // Apply standard runtime range logic
  if (runtimeRangeValid_) {
    minValue = std::min(minValue, runtimeMinValue_);
    maxValue = std::max(maxValue, runtimeMaxValue_);
  }
  runtimeRangeValid_ = true;
  runtimeMinValue_ = minValue;
  runtimeMaxValue_ = maxValue;

  const double range = maxValue - minValue;

  if (showTopProfile_ || showRightProfile_) {
    topProfileData_.clear();
    rightProfileData_.clear();
    
    if (showTopProfile_) {
      topProfileData_.resize(zoomedWidth);
      topProfileData_.fill(0.0);
    }
    if (showRightProfile_) {
      rightProfileData_.resize(zoomedHeight);
      rightProfileData_.fill(0.0);
    }

    QVector<int> topCounts(zoomedWidth, 0);
    QVector<int> rightCounts(zoomedHeight, 0);

    for (int y = yStart; y < yEnd; ++y) {
      for (int x = xStart; x < xEnd; ++x) {
        int index = (order_ == HeatmapOrder::kRowMajor) ? (y * width + x) : (x * height + y);
        if (index < available) {
          const double v = dataValues[index];
          if (!std::isnan(v) && !std::isinf(v)) {
            const int zx = x - xStart;
            const int zy = y - yStart;
            if (showTopProfile_ && zx >= 0 && zx < zoomedWidth) {
              topProfileData_[zx] += v;
              topCounts[zx]++;
            }
            if (showRightProfile_ && zy >= 0 && zy < zoomedHeight) {
              rightProfileData_[zy] += v;
              rightCounts[zy]++;
            }
          }
        }
      }
    }
    
    if (showTopProfile_) {
      bool haveTop = false;
      for (int x = 0; x < zoomedWidth; ++x) {
        if (topCounts[x] > 0) {
          topProfileData_[x] /= topCounts[x];
          if (!haveTop) {
            topProfileMin_ = topProfileData_[x];
            topProfileMax_ = topProfileData_[x];
            haveTop = true;
          } else {
            topProfileMin_ = std::min(topProfileMin_, topProfileData_[x]);
            topProfileMax_ = std::max(topProfileMax_, topProfileData_[x]);
          }
        } else {
          topProfileData_[x] = std::numeric_limits<double>::quiet_NaN();
        }
      }
      if (!haveTop) { topProfileMin_ = 0.0; topProfileMax_ = 0.0; }
    }

    if (showRightProfile_) {
      bool haveRight = false;
      for (int y = 0; y < zoomedHeight; ++y) {
        if (rightCounts[y] > 0) {
          rightProfileData_[y] /= rightCounts[y];
          if (!haveRight) {
            rightProfileMin_ = rightProfileData_[y];
            rightProfileMax_ = rightProfileData_[y];
            haveRight = true;
          } else {
            rightProfileMin_ = std::min(rightProfileMin_, rightProfileData_[y]);
            rightProfileMax_ = std::max(rightProfileMax_, rightProfileData_[y]);
          }
        } else {
          rightProfileData_[y] = std::numeric_limits<double>::quiet_NaN();
        }
      }
      if (!haveRight) { rightProfileMin_ = 0.0; rightProfileMax_ = 0.0; }
    }
  }
  
  cachedImage_ = QImage(zoomedWidth, zoomedHeight, QImage::Format_RGB32);
  cachedImage_.fill(backgroundColor());

  const double scale = (range > 0.0) ? (255.0 / range) : 0.0;

  if (order_ == HeatmapOrder::kRowMajor) {
    for (int y = yStart; y < yEnd; ++y) {
      const int zy = y - yStart;
      QRgb *scanLine = reinterpret_cast<QRgb*>(cachedImage_.scanLine(zy));
      int index = y * width + xStart;
      for (int x = xStart; x < xEnd; ++x) {
        const int zx = x - xStart;
        if (index < available) {
          const double value = dataValues[index];
          double offset = value - minValue;
          if (offset < 0.0) offset = 0.0;
          else if (offset > range) offset = range;

          int gray = static_cast<int>(offset * scale);
          if (!invertGreyscale_) {
            gray = 255 - gray;
          }
          scanLine[zx] = qRgb(gray, gray, gray);
        }
        ++index;
      }
    }
  } else {
    for (int y = yStart; y < yEnd; ++y) {
      const int zy = y - yStart;
      QRgb *scanLine = reinterpret_cast<QRgb*>(cachedImage_.scanLine(zy));
      // index for col-major: x * height + y
      for (int x = xStart; x < xEnd; ++x) {
        const int zx = x - xStart;
        int index = x * height + y;
        if (index < available) {
          const double value = dataValues[index];
          double offset = value - minValue;
          if (offset < 0.0) offset = 0.0;
          else if (offset > range) offset = range;

          int gray = static_cast<int>(offset * scale);
          if (!invertGreyscale_) {
            gray = 255 - gray;
          }
          scanLine[zx] = qRgb(gray, gray, gray);
        }
      }
    }
  }
}
QImage HeatmapElement::maxPoolDownsample(const QImage &source,
    const QSize &targetSize) const
{
  if (source.isNull() || targetSize.isEmpty()) {
    return QImage();
  }

  const int srcWidth = source.width();
  const int srcHeight = source.height();
  const int dstWidth = targetSize.width();
  const int dstHeight = targetSize.height();

  if (dstWidth <= 0 || dstHeight <= 0) {
    return QImage();
  }

  if (dstWidth >= srcWidth && dstHeight >= srcHeight) {
    return source;
  }

  QImage pooled(dstWidth, dstHeight, QImage::Format_RGB32);
  const int srcStride = source.bytesPerLine() / sizeof(QRgb);
  const QRgb* srcBits = reinterpret_cast<const QRgb*>(source.constBits());

  if (invertGreyscale_) {
    for (int y = 0; y < dstHeight; ++y) {
      const int yStart = (y * srcHeight) / dstHeight;
      int yEnd = ((y + 1) * srcHeight) / dstHeight;
      yEnd = std::max(yStart + 1, yEnd);
      yEnd = std::min(yEnd, srcHeight);

      QRgb *dstScanLine = reinterpret_cast<QRgb*>(pooled.scanLine(y));

      for (int x = 0; x < dstWidth; ++x) {
        const int xStart = (x * srcWidth) / dstWidth;
        int xEnd = ((x + 1) * srcWidth) / dstWidth;
        xEnd = std::max(xStart + 1, xEnd);
        xEnd = std::min(xEnd, srcWidth);

        int extremeGray = 0;
        for (int srcY = yStart; srcY < yEnd; ++srcY) {
          const QRgb *srcRow = srcBits + srcY * srcStride;
          for (int srcX = xStart; srcX < xEnd; ++srcX) {
            const int gray = qGray(srcRow[srcX]);
            if (gray > extremeGray) extremeGray = gray;
          }
        }
        dstScanLine[x] = qRgb(extremeGray, extremeGray, extremeGray);
      }
    }
  } else {
    for (int y = 0; y < dstHeight; ++y) {
      const int yStart = (y * srcHeight) / dstHeight;
      int yEnd = ((y + 1) * srcHeight) / dstHeight;
      yEnd = std::max(yStart + 1, yEnd);
      yEnd = std::min(yEnd, srcHeight);

      QRgb *dstScanLine = reinterpret_cast<QRgb*>(pooled.scanLine(y));

      for (int x = 0; x < dstWidth; ++x) {
        const int xStart = (x * srcWidth) / dstWidth;
        int xEnd = ((x + 1) * srcWidth) / dstWidth;
        xEnd = std::max(xStart + 1, xEnd);
        xEnd = std::min(xEnd, srcWidth);

        int extremeGray = 255;
        for (int srcY = yStart; srcY < yEnd; ++srcY) {
          const QRgb *srcRow = srcBits + srcY * srcStride;
          for (int srcX = xStart; srcX < xEnd; ++srcX) {
            const int gray = qGray(srcRow[srcX]);
            if (gray < extremeGray) extremeGray = gray;
          }
        }
        dstScanLine[x] = qRgb(extremeGray, extremeGray, extremeGray);
      }
    }
  }

  return pooled;
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
bool HeatmapElement::showTopProfile() const
{
  return showTopProfile_;
}

void HeatmapElement::setShowTopProfile(bool show)
{
  if (showTopProfile_ != show) {
    showTopProfile_ = show;
    update();
  }
}

bool HeatmapElement::showRightProfile() const
{
  return showRightProfile_;
}

void HeatmapElement::setShowRightProfile(bool show)
{
  if (showRightProfile_ != show) {
    showRightProfile_ = show;
    update();
  }
}

bool HeatmapElement::isZoomed() const
{
  return zoomed_;
}

void HeatmapElement::resetZoom()
{
  zoomed_ = false;
  zoomXMin_ = 0.0;
  zoomXMax_ = 1.0;
  zoomYMin_ = 0.0;
  zoomYMax_ = 1.0;
  panning_ = false;
  invalidateCache();
  update();
}

void HeatmapElement::onExecuteStateApplied()
{
  GraphicShapeElement::onExecuteStateApplied();
  
  if (isExecuteMode()) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
  } else {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    resetZoom();
  }
}

bool HeatmapElement::event(QEvent *event)
{
  return QWidget::event(event);
}

bool HeatmapElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  if (!event) return false;
  QWidget *target = window();
  if (!target) return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF globalPosF = event->globalPosition();
  const QPoint globalPoint = globalPosF.toPoint();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos, globalPosF,
      event->button(), event->buttons(), event->modifiers());
#else
  const QPoint globalPoint = event->globalPos();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos,
      QPointF(globalPoint), event->button(), event->buttons(),
      event->modifiers());
#endif
  QCoreApplication::sendEvent(target, &forwarded);
  return true;
}

void HeatmapElement::mousePressEvent(QMouseEvent *event)
{
  if (isExecuteMode()) {
    if (event->button() == Qt::MiddleButton) {
      if (forwardMouseEventToParent(event)) return;
    }
    if (event->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
      if (forwardMouseEventToParent(event)) return;
    }
    if (event->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
      const QPointF pos = event->localPos();
#else
      const QPointF pos = event->localPos();
#endif
      if (lastHeatmapRect_.contains(pos) && runtimeDataValid_) {
        panning_ = true;
        panStartPos_ = pos;
        panStartXMin_ = zoomXMin_;
        panStartXMax_ = zoomXMax_;
        panStartYMin_ = zoomYMin_;
        panStartYMax_ = zoomYMax_;
      }
    }
  }
  QWidget::mousePressEvent(event);
}

void HeatmapElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && panning_) {
    panning_ = false;
  }
  QWidget::mouseReleaseEvent(event);
}

void HeatmapElement::mouseMoveEvent(QMouseEvent *event)
{
  if (panning_) {
    HeatmapRuntime::setGlobalUpdatesPaused(true);
    interactionTimer_.start();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF pos = event->localPos();
#else
    const QPointF pos = event->localPos();
#endif
    const double dx = pos.x() - panStartPos_.x();
    const double dy = pos.y() - panStartPos_.y();
    
    // Convert dx, dy to normalized coordinates [0, 1] relative to zoomed area
    const double currentRangeX = panStartXMax_ - panStartXMin_;
    const double currentRangeY = panStartYMax_ - panStartYMin_;
    
    const double normDx = (dx / lastHeatmapRect_.width()) * currentRangeX;
    const double normDy = (dy / lastHeatmapRect_.height()) * currentRangeY;
    
    double newXMin = panStartXMin_ - normDx;
    double newXMax = panStartXMax_ - normDx;
    double newYMin = panStartYMin_ - normDy;
    double newYMax = panStartYMax_ - normDy;
    
    // Clamp to [0, 1]
    if (newXMin < 0.0) {
      newXMin = 0.0;
      newXMax = currentRangeX;
    } else if (newXMax > 1.0) {
      newXMax = 1.0;
      newXMin = 1.0 - currentRangeX;
    }
    
    if (newYMin < 0.0) {
      newYMin = 0.0;
      newYMax = currentRangeY;
    } else if (newYMax > 1.0) {
      newYMax = 1.0;
      newYMin = 1.0 - currentRangeY;
    }
    
    zoomXMin_ = newXMin;
    zoomXMax_ = newXMax;
    zoomYMin_ = newYMin;
    zoomYMax_ = newYMax;
    
    invalidateCache();
    update();
  }
  QWidget::mouseMoveEvent(event);
}

void HeatmapElement::wheelEvent(QWheelEvent *event)
{
  if (!isExecuteMode() || !runtimeDataValid_) {
    QWidget::wheelEvent(event);
    return;
  }
  
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  const QPointF pos = event->position();
#elif QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif

  if (!lastHeatmapRect_.contains(pos) || lastHeatmapRect_.width() <= 0 || lastHeatmapRect_.height() <= 0) {
    QWidget::wheelEvent(event);
    return;
  }

  HeatmapRuntime::setGlobalUpdatesPaused(true);
  interactionTimer_.start();
  
  if (!zoomed_) {
    zoomed_ = true;
    zoomXMin_ = 0.0;
    zoomXMax_ = 1.0;
    zoomYMin_ = 0.0;
    zoomYMax_ = 1.0;
  }
  
  const double degrees = event->angleDelta().y() / 8.0;
  const double steps = degrees / 15.0;
  const double factor = std::pow(0.9, steps);
  
  const bool zoomX = !event->modifiers().testFlag(Qt::ControlModifier);
  const bool zoomY = !event->modifiers().testFlag(Qt::ShiftModifier);
  
  const double chartX = (pos.x() - lastHeatmapRect_.left()) / lastHeatmapRect_.width();
  const double chartY = (pos.y() - lastHeatmapRect_.top()) / lastHeatmapRect_.height();
  
  if (zoomX) {
    const double xCenter = zoomXMin_ + chartX * (zoomXMax_ - zoomXMin_);
    double range = (zoomXMax_ - zoomXMin_) * factor;
    // Limit min zoom to a small fraction, e.g. 0.01 (1%)
    if (range < 0.01) range = 0.01;
    if (range > 1.0) range = 1.0;
    
    zoomXMin_ = xCenter - chartX * range;
    zoomXMax_ = zoomXMin_ + range;
    
    if (zoomXMin_ < 0.0) { zoomXMin_ = 0.0; zoomXMax_ = range; }
    if (zoomXMax_ > 1.0) { zoomXMax_ = 1.0; zoomXMin_ = 1.0 - range; }
  }
  
  if (zoomY) {
    const double yCenter = zoomYMin_ + chartY * (zoomYMax_ - zoomYMin_);
    double range = (zoomYMax_ - zoomYMin_) * factor;
    if (range < 0.01) range = 0.01;
    if (range > 1.0) range = 1.0;
    
    zoomYMin_ = yCenter - chartY * range;
    zoomYMax_ = zoomYMin_ + range;
    
    if (zoomYMin_ < 0.0) { zoomYMin_ = 0.0; zoomYMax_ = range; }
    if (zoomYMax_ > 1.0) { zoomYMax_ = 1.0; zoomYMin_ = 1.0 - range; }
  }
  
  if (zoomXMin_ <= 0.0 && zoomXMax_ >= 1.0 && zoomYMin_ <= 0.0 && zoomYMax_ >= 1.0) {
    zoomed_ = false;
  }
  
  invalidateCache();
  update();
}
