#include <QPainterPath>
#include "heatmap_element.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include "window_utils.h"
#include "heatmap_runtime.h"
#include "update_coordinator.h"

#include <algorithm>
#include <cmath>
#include <tuple>

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

struct HeatmapLayout
{
  QRect titleRect;
  QRect heatmapRect;
  QRect legendRect;
  QRect topProfileRect;
  QRect rightProfileRect;
};

QVector<QRgb> getHeatmapPalette(HeatmapColorMap map, bool invert) {
  QVector<QRgb> palette(256);
  for (int i = 0; i < 256; ++i) {
    int idx;
    if (map == HeatmapColorMap::kGrayscale) {
      idx = invert ? i : (255 - i);
    } else {
      idx = invert ? (255 - i) : i;
    }
    double t = idx / 255.0;
    int r = 0, g = 0, b = 0;
    switch (map) {
      case HeatmapColorMap::kGrayscale:
        r = g = b = idx;
        break;
      case HeatmapColorMap::kJet:
        r = std::clamp(static_cast<int>(255 * std::min(4 * t - 1.5, -4 * t + 4.5)), 0, 255);
        g = std::clamp(static_cast<int>(255 * std::min(4 * t - 0.5, -4 * t + 3.5)), 0, 255);
        b = std::clamp(static_cast<int>(255 * std::min(4 * t + 0.5, -4 * t + 2.5)), 0, 255);
        break;
      case HeatmapColorMap::kHot:
        r = std::clamp(static_cast<int>(255 * (3 * t)), 0, 255);
        g = std::clamp(static_cast<int>(255 * (3 * t - 1)), 0, 255);
        b = std::clamp(static_cast<int>(255 * (3 * t - 2)), 0, 255);
        break;
      case HeatmapColorMap::kCool:
        r = std::clamp(static_cast<int>(255 * t), 0, 255);
        g = std::clamp(static_cast<int>(255 * (1 - t)), 0, 255);
        b = 255;
        break;
      case HeatmapColorMap::kRainbow:
        t = t * 0.8;
        r = std::clamp(static_cast<int>(255 * std::min({4 * t - 1.5, -4 * t + 4.5, 1.0})), 0, 255);
        g = std::clamp(static_cast<int>(255 * std::min({4 * t - 0.5, -4 * t + 3.5, 1.0})), 0, 255);
        b = std::clamp(static_cast<int>(255 * std::min({4 * t + 0.5, -4 * t + 2.5, 1.0})), 0, 255);
        break;
      case HeatmapColorMap::kTurbo:
        r = std::clamp(static_cast<int>(255 * std::sin(M_PI * t)), 0, 255);
        g = std::clamp(static_cast<int>(255 * std::sin(M_PI * (t + 0.3))), 0, 255);
        b = std::clamp(static_cast<int>(255 * std::sin(M_PI * (t + 0.6))), 0, 255);
        break;
    }
    palette[i] = qRgb(r, g, b);
  }
  return palette;
}
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
  QObject::connect(&interactionTimer_, &QTimer::timeout, []() {
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

HeatmapColorMap HeatmapElement::colorMap() const
{
  return colorMap_;
}

void HeatmapElement::setColorMap(HeatmapColorMap colorMap)
{
  if (colorMap_ == colorMap) {
    return;
  }
  colorMap_ = colorMap;
  invalidatePaletteCache();
  invalidateCache();
}


bool HeatmapElement::preserveAspectRatio() const
{
  return preserveAspectRatio_;
}

void HeatmapElement::setPreserveAspectRatio(bool preserve)
{
  if (preserveAspectRatio_ != preserve) {
    preserveAspectRatio_ = preserve;
    requestVisualUpdate();
  }
}

bool HeatmapElement::flipHorizontal() const
{
  return flipHorizontal_;
}

void HeatmapElement::setFlipHorizontal(bool flip)
{
  if (flipHorizontal_ != flip) {
    flipHorizontal_ = flip;
    invalidateCache();
  }
}

bool HeatmapElement::flipVertical() const
{
  return flipVertical_;
}

void HeatmapElement::setFlipVertical(bool flip)
{
  if (flipVertical_ != flip) {
    flipVertical_ = flip;
    invalidateCache();
  }
}

HeatmapRotation HeatmapElement::rotation() const
{
  return rotation_;
}

void HeatmapElement::setRotation(HeatmapRotation rotation)
{
  if (rotation_ != rotation) {
    rotation_ = rotation;
    invalidateCache();
  }
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
  invalidatePaletteCache();
  invalidateCache();
}


void HeatmapElement::setRuntimeSharedData(std::shared_ptr<const double> sharedData, size_t size)
{
  runtimeSharedValues_ = sharedData;
  runtimeSharedSize_ = size;
  runtimeValues_.clear();
  runtimeDataValid_ = (runtimeSharedValues_ && runtimeSharedSize_ > 0);
  invalidateCache();
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
  const QFont titleFont = medmTextFieldFont(kTitleFontHeight);
  const QFont titleFontToUse = titleFont.family().isEmpty()
      ? font()
      : titleFont;
  const QFont legendFont = medmTextFieldFont(kLegendFontHeight);
  const QFont legendFontToUse = legendFont.family().isEmpty()
      ? font()
      : legendFont;
  const QFontMetrics legendMetrics(legendFontToUse);
  const QString titleText = title_.trimmed();
  auto computeLayout = [&](int legendWidth, bool canShowLegend) {
    HeatmapLayout layout;
    layout.heatmapRect = drawRect;

    if (!titleText.isEmpty()) {
      const QFontMetrics titleMetrics(titleFontToUse);
      const int titleHeight = titleMetrics.height();
      layout.titleRect = QRect(drawRect.left(), drawRect.top(),
          drawRect.width(), titleHeight);
      layout.heatmapRect.setTop(layout.titleRect.bottom() + 2);
    }

    if (canShowLegend && layout.heatmapRect.width() > (legendWidth + 10)) {
      layout.heatmapRect.setRight(drawRect.right() - legendWidth);
      layout.legendRect = QRect(layout.heatmapRect.right() + 1,
          layout.heatmapRect.top(),
          drawRect.right() - layout.heatmapRect.right(),
          layout.heatmapRect.height());
    }

    const int profileSize = std::min(40, std::min(layout.heatmapRect.width() / 4,
        layout.heatmapRect.height() / 4));
    if (showTopProfile_ && profileSize > 0) {
      layout.topProfileRect = QRect(layout.heatmapRect.left(),
          layout.heatmapRect.top(), layout.heatmapRect.width(), profileSize);
      layout.heatmapRect.setTop(layout.heatmapRect.top() + profileSize + 2);
    }
    if (showRightProfile_ && profileSize > 0) {
      layout.rightProfileRect = QRect(layout.heatmapRect.right() - profileSize + 1,
          layout.heatmapRect.top(), profileSize, layout.heatmapRect.height());
      layout.heatmapRect.setRight(layout.heatmapRect.right() - profileSize - 2);
      if (showTopProfile_) {
        layout.topProfileRect.setWidth(layout.heatmapRect.width());
      }
    }

    return layout;
  };

  auto computeLegendMetrics = [&]() {
    const bool canShowLegend = isExecuteMode() && runtimeDataValid_
        && runtimeRangeValid_;
    const QString minLabel = QString::number(runtimeMinValue_, 'g', 6);
    const QString maxLabel = QString::number(runtimeMaxValue_, 'g', 6);
    const int labelWidth = std::max(legendMetrics.horizontalAdvance(minLabel),
        legendMetrics.horizontalAdvance(maxLabel));
    return std::tuple<bool, QString, QString, int>(canShowLegend, minLabel,
        maxLabel, labelWidth);
  };

  auto [canShowLegend, minLabel, maxLabel, labelWidth] = computeLegendMetrics();
  HeatmapLayout layout = computeLayout(
      kLegendBarWidth + kLegendPadding + labelWidth + kLegendPadding
          + kLegendNumberPadding,
      canShowLegend);

  QSize desiredRenderSize = renderTargetSize(layout.heatmapRect.size());
  if (!cacheValid_ || cachedRenderSize_ != desiredRenderSize) {
    rebuildImage(desiredRenderSize);
    std::tie(canShowLegend, minLabel, maxLabel, labelWidth) =
        computeLegendMetrics();
    layout = computeLayout(
        kLegendBarWidth + kLegendPadding + labelWidth + kLegendPadding
            + kLegendNumberPadding,
        canShowLegend);
    desiredRenderSize = renderTargetSize(layout.heatmapRect.size());
    if (cachedRenderSize_ != desiredRenderSize) {
      rebuildImage(desiredRenderSize);
      std::tie(canShowLegend, minLabel, maxLabel, labelWidth) =
          computeLegendMetrics();
      layout = computeLayout(
          kLegendBarWidth + kLegendPadding + labelWidth + kLegendPadding
              + kLegendNumberPadding,
          canShowLegend);
    }
  }

  if (!layout.titleRect.isEmpty()) {
    painter.setFont(titleFontToUse);
    painter.setPen(QPen(borderColor(), 1, Qt::SolidLine));
    painter.drawText(layout.titleRect, Qt::AlignHCenter | Qt::AlignVCenter,
        titleText);
    painter.setFont(font());
  }

  QRect heatmapRect = layout.heatmapRect;
  const QRect legendRect = layout.legendRect;
  const QRect topProfileRect = layout.topProfileRect;
  const QRect rightProfileRect = layout.rightProfileRect;

  if (preserveAspectRatio_ && !cachedImage_.isNull()) {
    int iw = cachedImage_.width();
    int ih = cachedImage_.height();
    if (iw > 0 && ih > 0) {
      double imgAspect = static_cast<double>(iw) / ih;
      double rectAspect = static_cast<double>(heatmapRect.width()) / heatmapRect.height();
      int newW = heatmapRect.width();
      int newH = heatmapRect.height();

      if (imgAspect > rectAspect) {
        newH = qRound(heatmapRect.width() / imgAspect);
      } else {
        newW = qRound(heatmapRect.height() * imgAspect);
      }
      
      heatmapRect = QRect(
          heatmapRect.left() + (heatmapRect.width() - newW) / 2,
          heatmapRect.top() + (heatmapRect.height() - newH) / 2,
          newW,
          newH
      );
    }
  }


  lastHeatmapRect_ = heatmapRect;

  if (!cachedImage_.isNull()) {
    if (cachedImage_.size() == heatmapRect.size()) {
      painter.drawImage(heatmapRect.topLeft(), cachedImage_);
    } else {
      painter.drawImage(heatmapRect, cachedImage_);
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
      const QImage &barImage = legendBarImage(barRect.size());
      if (!barImage.isNull()) {
        painter.drawImage(barRect.topLeft(), barImage);
      }
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

void HeatmapElement::requestVisualUpdate(bool immediate)
{
  if (immediate || !isExecuteMode()) {
    update();
    return;
  }
  UpdateCoordinator::instance().requestUpdate(this);
}

void HeatmapElement::invalidateCache(bool immediate)
{
  cacheValid_ = false;
  cachedRenderSize_ = QSize();
  downsampledCachedImage_ = QImage();
  downsampledTargetSize_ = QSize();
  requestVisualUpdate(immediate);
}

void HeatmapElement::invalidatePaletteCache()
{
  paletteCacheValid_ = false;
  cachedLegendBarImage_ = QImage();
  cachedLegendBarSize_ = QSize();
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

QSize HeatmapElement::visibleDataSize() const
{
  const QSize dims = effectiveDimensions();
  if (dims.isEmpty()) {
    return QSize();
  }

  int width = dims.width();
  int height = dims.height();

  if (!zoomed_) {
    return dims;
  }

  int xStart = std::floor(zoomXMin_ * width);
  int xEnd = std::ceil(zoomXMax_ * width);
  int yStart = std::floor(zoomYMin_ * height);
  int yEnd = std::ceil(zoomYMax_ * height);

  if (xStart < 0) xStart = 0;
  if (xEnd > width) xEnd = width;
  if (yStart < 0) yStart = 0;
  if (yEnd > height) yEnd = height;
  if (xEnd <= xStart) xEnd = xStart + 1;
  if (yEnd <= yStart) yEnd = yStart + 1;

  return QSize(xEnd - xStart, yEnd - yStart);
}

QSize HeatmapElement::renderTargetSize(const QSize &availableSize) const
{
  if (availableSize.isEmpty()) {
    return QSize();
  }

  QSize desired = availableSize;
  if (rotation_ == HeatmapRotation::k90
      || rotation_ == HeatmapRotation::k270) {
    desired.transpose();
  }

  const QSize visibleSize = visibleDataSize();
  if (!visibleSize.isEmpty()) {
    desired.setWidth(std::min(desired.width(), visibleSize.width()));
    desired.setHeight(std::min(desired.height(), visibleSize.height()));
  }

  desired.setWidth(std::max(1, desired.width()));
  desired.setHeight(std::max(1, desired.height()));
  return desired;
}

void HeatmapElement::rebuildImage(const QSize &targetSize)
{
  cacheValid_ = true;
  downsampledCachedImage_ = QImage();
  downsampledTargetSize_ = QSize();

  const QSize dims = effectiveDimensions();
  if (dims.isEmpty()) {
    cachedRenderSize_ = QSize();
    cachedImage_ = QImage();
    return;
  }

  const int width = dims.width();
  const int height = dims.height();
  const int totalCells = width * height;
  if (totalCells <= 0) {
    cachedRenderSize_ = QSize();
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
    cachedRenderSize_ = QSize();
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

  topProfileData_.clear();
  rightProfileData_.clear();
  QVector<int> topCounts;
  QVector<int> rightCounts;
  const bool computeProfiles = showTopProfile_ || showRightProfile_;
  if (computeProfiles) {
    topProfileData_.resize(zoomedWidth);
    topProfileData_.fill(0.0);
    rightProfileData_.resize(zoomedHeight);
    rightProfileData_.fill(0.0);
    topCounts.resize(zoomedWidth);
    topCounts.fill(0);
    rightCounts.resize(zoomedHeight);
    rightCounts.fill(0);
  }

  // Calculate min/max and accumulate profile data over the visible zoomed
  // range in a single source-data traversal.
  for (int y = yStart; y < yEnd; ++y) {
    const int zy = y - yStart;
    if (order_ == HeatmapOrder::kRowMajor) {
      int index = y * width + xStart;
      for (int x = xStart; x < xEnd; ++x, ++index) {
        if (index >= available) {
          continue;
        }
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
        if (computeProfiles) {
          const int zx = x - xStart;
          topProfileData_[zx] += v;
          topCounts[zx]++;
          rightProfileData_[zy] += v;
          rightCounts[zy]++;
        }
      }
    } else {
      for (int x = xStart; x < xEnd; ++x) {
        const int index = x * height + y;
        if (index >= available) {
          continue;
        }
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
        if (computeProfiles) {
          const int zx = x - xStart;
          topProfileData_[zx] += v;
          topCounts[zx]++;
          rightProfileData_[zy] += v;
          rightCounts[zy]++;
        }
      }
    }
  }
  
  if (!haveValue) {
    cachedRenderSize_ = QSize();
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

  if (computeProfiles) {
    for (int x = 0; x < zoomedWidth; ++x) {
      if (topCounts[x] > 0) {
        topProfileData_[x] /= topCounts[x];
      } else {
        topProfileData_[x] = std::numeric_limits<double>::quiet_NaN();
      }
    }
    for (int y = 0; y < zoomedHeight; ++y) {
      if (rightCounts[y] > 0) {
        rightProfileData_[y] /= rightCounts[y];
      } else {
        rightProfileData_[y] = std::numeric_limits<double>::quiet_NaN();
      }
    }

    // Apply flips
    if (flipHorizontal_) {
      std::reverse(topProfileData_.begin(), topProfileData_.end());
    }
    if (flipVertical_) {
      std::reverse(rightProfileData_.begin(), rightProfileData_.end());
    }

    // Apply rotations
    if (rotation_ == HeatmapRotation::k90) {
      QVector<double> newTop = rightProfileData_;
      std::reverse(newTop.begin(), newTop.end());
      QVector<double> newRight = topProfileData_;
      topProfileData_ = newTop;
      rightProfileData_ = newRight;
    } else if (rotation_ == HeatmapRotation::k180) {
      std::reverse(topProfileData_.begin(), topProfileData_.end());
      std::reverse(rightProfileData_.begin(), rightProfileData_.end());
    } else if (rotation_ == HeatmapRotation::k270) {
      QVector<double> newTop = rightProfileData_;
      QVector<double> newRight = topProfileData_;
      std::reverse(newRight.begin(), newRight.end());
      topProfileData_ = newTop;
      rightProfileData_ = newRight;
    }

    if (showTopProfile_) {
      bool haveTop = false;
      for (int x = 0; x < topProfileData_.size(); ++x) {
        double v = topProfileData_[x];
        if (!std::isnan(v)) {
          if (!haveTop) {
            topProfileMin_ = v;
            topProfileMax_ = v;
            haveTop = true;
          } else {
            topProfileMin_ = std::min(topProfileMin_, v);
            topProfileMax_ = std::max(topProfileMax_, v);
          }
        }
      }
      if (!haveTop) { topProfileMin_ = 0.0; topProfileMax_ = 0.0; }
    } else {
      topProfileData_.clear();
    }

    if (showRightProfile_) {
      bool haveRight = false;
      for (int y = 0; y < rightProfileData_.size(); ++y) {
        double v = rightProfileData_[y];
        if (!std::isnan(v)) {
          if (!haveRight) {
            rightProfileMin_ = v;
            rightProfileMax_ = v;
            haveRight = true;
          } else {
            rightProfileMin_ = std::min(rightProfileMin_, v);
            rightProfileMax_ = std::max(rightProfileMax_, v);
          }
        }
      }
      if (!haveRight) { rightProfileMin_ = 0.0; rightProfileMax_ = 0.0; }
    } else {
      rightProfileData_.clear();
    }
  }
  
  QSize renderSize = targetSize;
  if (renderSize.isEmpty()) {
    renderSize = QSize(zoomedWidth, zoomedHeight);
  }
  renderSize.setWidth(std::min(renderSize.width(), zoomedWidth));
  renderSize.setHeight(std::min(renderSize.height(), zoomedHeight));
  renderSize.setWidth(std::max(1, renderSize.width()));
  renderSize.setHeight(std::max(1, renderSize.height()));
  cachedRenderSize_ = renderSize;

  cachedImage_ = QImage(renderSize, QImage::Format_RGB32);
  cachedImage_.fill(backgroundColor());

  const double scale = (range > 0.0) ? (255.0 / range) : 0.0;
  const QVector<QRgb> &palette = cachedPalette();

  for (int dy = 0; dy < renderSize.height(); ++dy) {
    const int srcYStart = yStart + (dy * zoomedHeight) / renderSize.height();
    int srcYEnd = yStart + ((dy + 1) * zoomedHeight) / renderSize.height();
    srcYEnd = std::max(srcYStart + 1, srcYEnd);
    srcYEnd = std::min(srcYEnd, yEnd);

    QRgb *scanLine = reinterpret_cast<QRgb*>(cachedImage_.scanLine(dy));
    for (int dx = 0; dx < renderSize.width(); ++dx) {
      const int srcXStart = xStart + (dx * zoomedWidth) / renderSize.width();
      int srcXEnd = xStart + ((dx + 1) * zoomedWidth) / renderSize.width();
      srcXEnd = std::max(srcXStart + 1, srcXEnd);
      srcXEnd = std::min(srcXEnd, xEnd);

      double sum = 0.0;
      int count = 0;

      for (int srcY = srcYStart; srcY < srcYEnd; ++srcY) {
        if (order_ == HeatmapOrder::kRowMajor) {
          int index = srcY * width + srcXStart;
          for (int srcX = srcXStart; srcX < srcXEnd; ++srcX, ++index) {
            if (index >= available) {
              continue;
            }
            const double value = dataValues[index];
            if (std::isnan(value) || std::isinf(value)) {
              continue;
            }
            sum += value;
            ++count;
          }
        } else {
          for (int srcX = srcXStart; srcX < srcXEnd; ++srcX) {
            const int index = srcX * height + srcY;
            if (index >= available) {
              continue;
            }
            const double value = dataValues[index];
            if (std::isnan(value) || std::isinf(value)) {
              continue;
            }
            sum += value;
            ++count;
          }
        }
      }

      if (count <= 0) {
        scanLine[dx] = backgroundColor().rgb();
        continue;
      }

      double offset = (sum / count) - minValue;
      if (offset < 0.0) offset = 0.0;
      else if (offset > range) offset = range;

      int colorIdx = static_cast<int>(offset * scale);
      if (colorIdx < 0) colorIdx = 0;
      if (colorIdx > 255) colorIdx = 255;
      scanLine[dx] = palette[colorIdx];
    }
  }
  
  if (flipHorizontal_ || flipVertical_) {
    cachedImage_ = cachedImage_.mirrored(flipHorizontal_, flipVertical_);
  }
  
  if (rotation_ != HeatmapRotation::kNone) {
    QTransform transform;
    if (rotation_ == HeatmapRotation::k90) {
      transform.rotate(90);
    } else if (rotation_ == HeatmapRotation::k180) {
      transform.rotate(180);
    } else if (rotation_ == HeatmapRotation::k270) {
      transform.rotate(270);
    }
    cachedImage_ = cachedImage_.transformed(transform);
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

  if (colorMap_ != HeatmapColorMap::kGrayscale) {
    return source.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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

const QVector<QRgb> &HeatmapElement::cachedPalette()
{
  if (!paletteCacheValid_) {
    cachedPalette_ = getHeatmapPalette(colorMap_, invertGreyscale_);
    paletteCacheValid_ = true;
    cachedLegendBarImage_ = QImage();
    cachedLegendBarSize_ = QSize();
  }
  return cachedPalette_;
}

const QImage &HeatmapElement::legendBarImage(const QSize &size)
{
  if (size.isEmpty()) {
    cachedLegendBarImage_ = QImage();
    cachedLegendBarSize_ = QSize();
    return cachedLegendBarImage_;
  }

  if (cachedLegendBarImage_.isNull() || cachedLegendBarSize_ != size) {
    cachedLegendBarImage_ = QImage(size, QImage::Format_RGB32);
    cachedLegendBarSize_ = size;

    QPainter painter(&cachedLegendBarImage_);
    QLinearGradient gradient(QPointF(0.0, 0.0),
        QPointF(0.0, static_cast<double>(size.height())));
    const QVector<QRgb> &palette = cachedPalette();
    for (int i = 0; i <= 255; ++i) {
      gradient.setColorAt(1.0 - (i / 255.0), QColor(palette[i]));
    }
    painter.fillRect(QRect(QPoint(0, 0), size), gradient);
  }

  return cachedLegendBarImage_;
}

bool HeatmapElement::showTopProfile() const
{
  return showTopProfile_;
}

void HeatmapElement::setShowTopProfile(bool show)
{
  if (showTopProfile_ != show) {
    showTopProfile_ = show;
    requestVisualUpdate();
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
    requestVisualUpdate();
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
  invalidateCache(true);
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
    double dx = pos.x() - panStartPos_.x();
    double dy = pos.y() - panStartPos_.y();
    
    double normDx = dx / lastHeatmapRect_.width();
    double normDy = dy / lastHeatmapRect_.height();
    
    mapVisualToDataDelta(normDx, normDy);
    
    // Convert dx, dy to normalized coordinates [0, 1] relative to zoomed area
    const double currentRangeX = panStartXMax_ - panStartXMin_;
    const double currentRangeY = panStartYMax_ - panStartYMin_;
    
    normDx *= currentRangeX;
    normDy *= currentRangeY;
    
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
    
    invalidateCache(true);
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
  
  bool zoomX = !event->modifiers().testFlag(Qt::ControlModifier);
  bool zoomY = !event->modifiers().testFlag(Qt::ShiftModifier);
  
  double chartX = (pos.x() - lastHeatmapRect_.left()) / lastHeatmapRect_.width();
  double chartY = (pos.y() - lastHeatmapRect_.top()) / lastHeatmapRect_.height();
  
  mapVisualToDataZoomFlags(zoomX, zoomY);
  mapVisualToDataFraction(chartX, chartY);
  
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
  
  invalidateCache(true);
}


void HeatmapElement::mapVisualToDataFraction(double& x, double& y) const
{
  double nx = x;
  double ny = y;
  
  // Undo rotation
  if (rotation_ == HeatmapRotation::k90) {
    double tx = ny;
    double ty = 1.0 - nx;
    nx = tx; ny = ty;
  } else if (rotation_ == HeatmapRotation::k180) {
    nx = 1.0 - nx;
    ny = 1.0 - ny;
  } else if (rotation_ == HeatmapRotation::k270) {
    double tx = 1.0 - ny;
    double ty = nx;
    nx = tx; ny = ty;
  }
  
  // Undo flip
  if (flipHorizontal_) {
    nx = 1.0 - nx;
  }
  if (flipVertical_) {
    ny = 1.0 - ny;
  }
  
  x = nx;
  y = ny;
}

void HeatmapElement::mapVisualToDataDelta(double& dx, double& dy) const
{
  double nx = dx;
  double ny = dy;
  
  if (rotation_ == HeatmapRotation::k90) {
    double tx = ny;
    double ty = -nx;
    nx = tx; ny = ty;
  } else if (rotation_ == HeatmapRotation::k180) {
    nx = -nx;
    ny = -ny;
  } else if (rotation_ == HeatmapRotation::k270) {
    double tx = -ny;
    double ty = nx;
    nx = tx; ny = ty;
  }
  
  if (flipHorizontal_) nx = -nx;
  if (flipVertical_) ny = -ny;
  
  dx = nx;
  dy = ny;
}

void HeatmapElement::mapVisualToDataZoomFlags(bool& zoomX, bool& zoomY) const
{
  if (rotation_ == HeatmapRotation::k90 || rotation_ == HeatmapRotation::k270) {
    bool temp = zoomX;
    zoomX = zoomY;
    zoomY = temp;
  }
}
