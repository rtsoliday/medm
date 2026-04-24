#include "waterfall_plot_element.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QTransform>
#include <QToolTip>
#include <QWheelEvent>

#include "text_font_utils.h"
#include "window_utils.h"

namespace {

constexpr int kOuterMargin = 4;
constexpr int kTitleFontHeight = 24;
constexpr int kAxisLabelFontHeight = 18;
constexpr int kAxisTickFontHeight = 10;
constexpr int kLegendFontHeight = 12;
constexpr int kTickGap = 4;
constexpr int kLegendWidth = 14;
constexpr int kLegendPadding = 6;
constexpr int kMinimumTimeViewPixels = 8;
constexpr int kRaisedFrameThickness = 2;

QColor effectiveColor(const QWidget *widget, const QColor &candidate,
    QPalette::ColorRole role, const QColor &fallback)
{
  if (candidate.isValid()) {
    return candidate;
  }
  const QWidget *current = widget;
  while (current) {
    const QColor color = current->palette().color(role);
    if (color.isValid()) {
      return color;
    }
    current = current->parentWidget();
  }
  return fallback;
}

QVector<QRgb> buildHeatmapPalette(HeatmapColorMap map, bool invert)
{
  QVector<QRgb> palette(256);
  for (int i = 0; i < 256; ++i) {
    int idx;
    if (map == HeatmapColorMap::kGrayscale) {
      idx = invert ? i : (255 - i);
    } else {
      idx = invert ? (255 - i) : i;
    }
    const double t = idx / 255.0;
    int r = 0;
    int g = 0;
    int b = 0;
    switch (map) {
    case HeatmapColorMap::kGrayscale:
      r = g = b = idx;
      break;
    case HeatmapColorMap::kJet:
      r = std::clamp(static_cast<int>(
          255 * std::min(4 * t - 1.5, -4 * t + 4.5)), 0, 255);
      g = std::clamp(static_cast<int>(
          255 * std::min(4 * t - 0.5, -4 * t + 3.5)), 0, 255);
      b = std::clamp(static_cast<int>(
          255 * std::min(4 * t + 0.5, -4 * t + 2.5)), 0, 255);
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
    case HeatmapColorMap::kRainbow: {
      const double scaled = t * 0.8;
      r = std::clamp(static_cast<int>(
          255 * std::min({4 * scaled - 1.5, -4 * scaled + 4.5, 1.0})),
          0, 255);
      g = std::clamp(static_cast<int>(
          255 * std::min({4 * scaled - 0.5, -4 * scaled + 3.5, 1.0})),
          0, 255);
      b = std::clamp(static_cast<int>(
          255 * std::min({4 * scaled + 0.5, -4 * scaled + 2.5, 1.0})),
          0, 255);
      break;
    }
    case HeatmapColorMap::kTurbo:
      r = std::clamp(static_cast<int>(255 * std::sin(M_PI * t)), 0, 255);
      g = std::clamp(static_cast<int>(
          255 * std::sin(M_PI * (t + 0.3))), 0, 255);
      b = std::clamp(static_cast<int>(
          255 * std::sin(M_PI * (t + 0.6))), 0, 255);
      break;
    }
    palette[i] = qRgb(r, g, b);
  }
  return palette;
}

bool isHorizontalScroll(WaterfallScrollDirection direction)
{
  return direction == WaterfallScrollDirection::kLeftToRight
      || direction == WaterfallScrollDirection::kRightToLeft;
}

bool newestAtLeadingEdge(WaterfallScrollDirection direction)
{
  return direction == WaterfallScrollDirection::kTopToBottom
      || direction == WaterfallScrollDirection::kLeftToRight;
}

QFont waterfallPlotFont(const QWidget *widget, int height)
{
  const QFont medmFont = medmTextFieldFont(height);
  return medmFont.family().isEmpty()
      ? widget->font()
      : medmFont;
}

void drawRaisedFrame(QPainter &painter, const QRect &outerRect,
    const QColor &background)
{
  if (!outerRect.isValid() || outerRect.isEmpty()) {
    return;
  }

  const QColor lightShadow = background.lighter(150);
  const QColor darkShadow = background.darker(150);

  for (int i = 0; i < kRaisedFrameThickness; ++i) {
    painter.setPen(QPen(lightShadow, 1));
    painter.drawLine(outerRect.left() + i, outerRect.top() + i,
        outerRect.right() - i, outerRect.top() + i);
    painter.drawLine(outerRect.left() + i, outerRect.top() + i,
        outerRect.left() + i, outerRect.bottom() - i);

    painter.setPen(QPen(darkShadow, 1));
    painter.drawLine(outerRect.left() + i, outerRect.bottom() - i,
        outerRect.right() - i, outerRect.bottom() - i);
    painter.drawLine(outerRect.right() - i, outerRect.top() + i,
        outerRect.right() - i, outerRect.bottom() - i);
  }
}

} // namespace

WaterfallPlotElement::WaterfallPlotElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  setContextMenuPolicy(Qt::NoContextMenu);
  setMouseTracking(true);
}

QSize WaterfallPlotElement::sizeHint() const
{
  return QSize(300, 200);
}

void WaterfallPlotElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool WaterfallPlotElement::isSelected() const
{
  return selected_;
}

QColor WaterfallPlotElement::foregroundColor() const
{
  return foregroundColor_;
}

void WaterfallPlotElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor WaterfallPlotElement::backgroundColor() const
{
  return backgroundColor_;
}

void WaterfallPlotElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  invalidateImageCache();
}

QString WaterfallPlotElement::title() const
{
  return title_;
}

void WaterfallPlotElement::setTitle(const QString &title)
{
  const QString trimmed = title.trimmed();
  if (title_ == trimmed) {
    return;
  }
  title_ = trimmed;
  update();
}

QString WaterfallPlotElement::xLabel() const
{
  return xLabel_;
}

void WaterfallPlotElement::setXLabel(const QString &label)
{
  const QString trimmed = label.trimmed();
  if (xLabel_ == trimmed) {
    return;
  }
  xLabel_ = trimmed;
  update();
}

QString WaterfallPlotElement::yLabel() const
{
  return yLabel_;
}

void WaterfallPlotElement::setYLabel(const QString &label)
{
  const QString trimmed = label.trimmed();
  if (yLabel_ == trimmed) {
    return;
  }
  yLabel_ = trimmed;
  update();
}

QString WaterfallPlotElement::dataChannel() const
{
  return dataChannel_;
}

void WaterfallPlotElement::setDataChannel(const QString &channel)
{
  const QString trimmed = channel.trimmed();
  if (dataChannel_ == trimmed) {
    return;
  }
  dataChannel_ = trimmed;
}

QString WaterfallPlotElement::countChannel() const
{
  return countChannel_;
}

void WaterfallPlotElement::setCountChannel(const QString &channel)
{
  const QString trimmed = channel.trimmed();
  if (countChannel_ == trimmed) {
    return;
  }
  countChannel_ = trimmed;
  update();
}

QString WaterfallPlotElement::triggerChannel() const
{
  return triggerChannel_;
}

void WaterfallPlotElement::setTriggerChannel(const QString &channel)
{
  const QString trimmed = channel.trimmed();
  if (triggerChannel_ == trimmed) {
    return;
  }
  triggerChannel_ = trimmed;
  update();
}

QString WaterfallPlotElement::eraseChannel() const
{
  return eraseChannel_;
}

void WaterfallPlotElement::setEraseChannel(const QString &channel)
{
  const QString trimmed = channel.trimmed();
  if (eraseChannel_ == trimmed) {
    return;
  }
  eraseChannel_ = trimmed;
  update();
}

WaterfallEraseMode WaterfallPlotElement::eraseMode() const
{
  return eraseMode_;
}

void WaterfallPlotElement::setEraseMode(WaterfallEraseMode mode)
{
  if (eraseMode_ == mode) {
    return;
  }
  eraseMode_ = mode;
}

int WaterfallPlotElement::historyCount() const
{
  return historyCount_;
}

void WaterfallPlotElement::setHistoryCount(int count)
{
  const int clamped = std::clamp(count, 1, kWaterfallMaxHistory);
  if (historyCount_ == clamped) {
    return;
  }
  historyCount_ = clamped;
  sampleLengths_.fill(0, historyCount_);
  sampleTimestampsMs_.fill(0, historyCount_);
  writeCursor_ = 0;
  bufferedSamples_ = 0;
  timeViewStart_ = 0.0;
  timeViewEnd_ = 1.0;
  ensureBufferCapacity(bufferColumnCount_);
  invalidateImageCache();
}

WaterfallScrollDirection WaterfallPlotElement::scrollDirection() const
{
  return scrollDirection_;
}

void WaterfallPlotElement::setScrollDirection(WaterfallScrollDirection direction)
{
  if (scrollDirection_ == direction) {
    return;
  }
  scrollDirection_ = direction;
  invalidateImageCache();
}

HeatmapColorMap WaterfallPlotElement::colorMap() const
{
  return colorMap_;
}

void WaterfallPlotElement::setColorMap(HeatmapColorMap map)
{
  if (colorMap_ == map) {
    return;
  }
  colorMap_ = map;
  invalidatePaletteCache();
  invalidateImageCache();
}

bool WaterfallPlotElement::invertGreyscale() const
{
  return invertGreyscale_;
}

void WaterfallPlotElement::setInvertGreyscale(bool invert)
{
  if (invertGreyscale_ == invert) {
    return;
  }
  invertGreyscale_ = invert;
  invalidatePaletteCache();
  invalidateImageCache();
}

WaterfallIntensityScale WaterfallPlotElement::intensityScale() const
{
  return intensityScale_;
}

void WaterfallPlotElement::setIntensityScale(WaterfallIntensityScale scale)
{
  if (intensityScale_ == scale) {
    return;
  }
  intensityScale_ = scale;
  invalidateImageCache();
}

double WaterfallPlotElement::intensityMin() const
{
  return intensityMin_;
}

void WaterfallPlotElement::setIntensityMin(double value)
{
  if (qFuzzyCompare(intensityMin_, value)) {
    return;
  }
  intensityMin_ = value;
  invalidateImageCache();
}

double WaterfallPlotElement::intensityMax() const
{
  return intensityMax_;
}

void WaterfallPlotElement::setIntensityMax(double value)
{
  if (qFuzzyCompare(intensityMax_, value)) {
    return;
  }
  intensityMax_ = value;
  invalidateImageCache();
}

bool WaterfallPlotElement::showLegend() const
{
  return showLegend_;
}

void WaterfallPlotElement::setShowLegend(bool show)
{
  if (showLegend_ == show) {
    return;
  }
  showLegend_ = show;
  update();
}

bool WaterfallPlotElement::showGrid() const
{
  return showGrid_;
}

void WaterfallPlotElement::setShowGrid(bool show)
{
  if (showGrid_ == show) {
    return;
  }
  showGrid_ = show;
  update();
}

double WaterfallPlotElement::samplePeriod() const
{
  return samplePeriod_;
}

void WaterfallPlotElement::setSamplePeriod(double period)
{
  const double clamped = std::max(0.0, period);
  if (qFuzzyCompare(samplePeriod_, clamped)) {
    return;
  }
  samplePeriod_ = clamped;
  update();
}

TimeUnits WaterfallPlotElement::units() const
{
  return units_;
}

void WaterfallPlotElement::setUnits(TimeUnits units)
{
  if (units_ == units) {
    return;
  }
  units_ = units;
  update();
}

void WaterfallPlotElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (!executeMode_) {
    runtimeConnected_ = false;
    runtimeWaveformLength_ = 0;
    resetZoom();
  }
  update();
}

bool WaterfallPlotElement::isExecuteMode() const
{
  return executeMode_;
}

void WaterfallPlotElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  update();
}

bool WaterfallPlotElement::isRuntimeConnected() const
{
  return runtimeConnected_;
}

void WaterfallPlotElement::setRuntimeWaveformLength(int length)
{
  const int clamped = std::max(0, length);
  if (runtimeWaveformLength_ == clamped) {
    return;
  }
  runtimeWaveformLength_ = clamped;
  if (runtimeWaveformLength_ > 0) {
    ensureBufferCapacity(runtimeWaveformLength_);
  }
  invalidateImageCache();
}

int WaterfallPlotElement::runtimeWaveformLength() const
{
  return runtimeWaveformLength_;
}

void WaterfallPlotElement::pushWaveform(const double *values, int count,
    qint64 timestampMs, bool requestUpdate)
{
  if (!values || count <= 0) {
    return;
  }
  ensureBufferCapacity(count);
  if (bufferColumnCount_ <= 0 || sampleLengths_.isEmpty()) {
    return;
  }

  const int clampedCount = std::min(count, bufferColumnCount_);
  const int slot = writeCursor_;
  const int base = slot * bufferColumnCount_;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (int i = 0; i < bufferColumnCount_; ++i) {
    bufferValues_[base + i] = nan;
  }
  for (int i = 0; i < clampedCount; ++i) {
    bufferValues_[base + i] = values[i];
  }
  sampleLengths_[slot] = clampedCount;
  sampleTimestampsMs_[slot] = timestampMs > 0
      ? timestampMs
      : QDateTime::currentMSecsSinceEpoch();

  writeCursor_ = (writeCursor_ + 1) % historyCount_;
  if (bufferedSamples_ < historyCount_) {
    ++bufferedSamples_;
  }

  invalidateImageCache(requestUpdate);
}

void WaterfallPlotElement::clearBuffer(bool requestUpdate)
{
  bufferedSamples_ = 0;
  writeCursor_ = 0;
  if (!sampleLengths_.isEmpty()) {
    sampleLengths_.fill(0);
  }
  if (!sampleTimestampsMs_.isEmpty()) {
    sampleTimestampsMs_.fill(0);
  }
  if (!bufferValues_.isEmpty()) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::fill(bufferValues_.begin(), bufferValues_.end(), nan);
  }
  invalidateImageCache(requestUpdate);
}

bool WaterfallPlotElement::isZoomed() const
{
  return (timeViewEnd_ - timeViewStart_) < 0.999;
}

void WaterfallPlotElement::resetZoom()
{
  timeViewStart_ = 0.0;
  timeViewEnd_ = 1.0;
  panning_ = false;
  update();
}

int WaterfallPlotElement::bufferedSampleCount() const
{
  return bufferedSamples_;
}

int WaterfallPlotElement::waveformLength() const
{
  return bufferColumnCount_ > 0 ? bufferColumnCount_ : runtimeWaveformLength_;
}

int WaterfallPlotElement::sampleLength(int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= bufferedSamples_) {
    return 0;
  }
  return sampleLengths_.value(bufferSlotForSample(sampleIndex), 0);
}

qint64 WaterfallPlotElement::sampleTimestampMs(int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= bufferedSamples_) {
    return 0;
  }
  return sampleTimestampsMs_.value(bufferSlotForSample(sampleIndex), 0);
}

double WaterfallPlotElement::sampleValue(int sampleIndex, int pointIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= bufferedSamples_
      || pointIndex < 0 || pointIndex >= bufferColumnCount_) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const int slot = bufferSlotForSample(sampleIndex);
  return bufferValues_.value(slot * bufferColumnCount_ + pointIndex,
      std::numeric_limits<double>::quiet_NaN());
}

void WaterfallPlotElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, !executeMode_);

  const QColor plotBackground = effectiveColor(this, backgroundColor_,
      QPalette::Window, QColor(Qt::white));
  const QColor plotForeground = effectiveColor(this, foregroundColor_,
      QPalette::WindowText, QColor(Qt::black));
  const QRect frameRect = rect();
  const QRect selectionRect = rect().adjusted(0, 0, -1, -1);
  const QRect bounds = rect().adjusted(kRaisedFrameThickness,
      kRaisedFrameThickness, -kRaisedFrameThickness, -kRaisedFrameThickness);

  painter.fillRect(rect(), plotBackground);

  const QFont titleFont = waterfallPlotFont(this, kTitleFontHeight);
  const QFont labelFont = waterfallPlotFont(this, kAxisLabelFontHeight);
  const QFont tickFont = waterfallPlotFont(this, kAxisTickFontHeight);
  const QFont legendFont = waterfallPlotFont(this, kLegendFontHeight);
  const QFontMetrics titleMetrics(titleFont);
  const QFontMetrics labelMetrics(labelFont);
  const QFontMetrics tickMetrics(tickFont);
  const QFontMetrics legendMetrics(legendFont);

  QImage fullImage = buildPlotImage();
  const int visibleStart = visibleTimeStartIndex();
  const int visibleEnd = visibleTimeEndIndex();
  lastVisibleTimeStart_ = visibleStart;
  lastVisibleTimeEnd_ = visibleEnd;

  const int waveformMax = std::max(0, waveformLength() - 1);
  const QString waveformMinText = QStringLiteral("0");
  const QString waveformMaxText = QString::number(waveformMax);
  const QString timeMinText = timeAxisValueText(visibleStart);
  const QString timeMaxText = timeAxisValueText(std::max(visibleStart,
      visibleEnd - 1));
  const bool horizontal = isHorizontalScroll(scrollDirection_);
  const int leftTickWidth = horizontal
      ? std::max(tickMetrics.horizontalAdvance(waveformMinText),
          tickMetrics.horizontalAdvance(waveformMaxText))
      : std::max(tickMetrics.horizontalAdvance(timeMinText),
          tickMetrics.horizontalAdvance(timeMaxText));
  const IntensityRange legendRange = executeMode_
      ? cachedIntensityRange_
      : IntensityRange{};
  const QString legendMaxText = legendRange.valid
      ? intensityValueText(legendRange.maximum)
      : QStringLiteral("1");
  const QString legendMinText = legendRange.valid
      ? intensityValueText(legendRange.minimum)
      : QStringLiteral("0");
  const int legendTextWidth = std::max(legendMetrics.horizontalAdvance(
      legendMaxText), legendMetrics.horizontalAdvance(legendMinText));
  const PlotLayout layout = computeLayout(bounds, titleMetrics.height(),
      labelMetrics.height(), tickMetrics.height(), leftTickWidth,
      legendTextWidth);
  lastPlotRect_ = layout.plotRect;
  lastImageRect_ = layout.plotRect;

  if (!layout.titleRect.isEmpty() && !title_.isEmpty()) {
    painter.setFont(titleFont);
    painter.setPen(plotForeground);
    painter.drawText(layout.titleRect, Qt::AlignCenter, title_);
  }

  painter.setPen(QPen(plotForeground, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(layout.plotRect);

  QImage visibleImage = fullImage;
  if (!fullImage.isNull()) {
    if (isHorizontalScroll(scrollDirection_)) {
      visibleImage = fullImage.copy(visibleStart, 0,
          std::max(1, visibleEnd - visibleStart), fullImage.height());
    } else {
      visibleImage = fullImage.copy(0, visibleStart, fullImage.width(),
          std::max(1, visibleEnd - visibleStart));
    }
  }

  if (executeMode_ && !visibleImage.isNull()) {
    painter.drawImage(layout.plotRect, visibleImage);
  } else {
    painter.fillRect(layout.plotRect, plotBackground);
    painter.setPen(QPen(plotForeground, 1, Qt::DashLine));
    painter.drawLine(layout.plotRect.topLeft(),
        layout.plotRect.bottomRight());
    painter.drawLine(layout.plotRect.topRight(),
        layout.plotRect.bottomLeft());
  }

  if (showGrid_) {
    painter.setPen(QPen(plotForeground.lighter(150), 1, Qt::DashLine));
    for (int i = 1; i < 5; ++i) {
      const int x = layout.plotRect.left()
          + (layout.plotRect.width() * i) / 5;
      const int y = layout.plotRect.top()
          + (layout.plotRect.height() * i) / 5;
      painter.drawLine(x, layout.plotRect.top(), x, layout.plotRect.bottom());
      painter.drawLine(layout.plotRect.left(), y, layout.plotRect.right(), y);
    }
  }

  painter.setFont(tickFont);
  painter.setPen(plotForeground);
  const int bottomTickWidth = horizontal
      ? std::max(tickMetrics.horizontalAdvance(timeMinText),
          tickMetrics.horizontalAdvance(timeMaxText))
      : std::max(tickMetrics.horizontalAdvance(waveformMinText),
          tickMetrics.horizontalAdvance(waveformMaxText));
  const int bottomTickHeight = tickMetrics.height();
  const int leftAxisTextLeft = layout.yLabelRect.right() + kTickGap + 2;
  const int leftAxisTextWidth = std::max(1, layout.plotRect.left()
      - leftAxisTextLeft - 2);
  if (horizontal) {
    painter.drawText(QRect(layout.plotRect.left(), layout.plotRect.bottom() + 2,
        bottomTickWidth + 4, bottomTickHeight), Qt::AlignLeft | Qt::AlignTop,
        timeMinText);
    painter.drawText(QRect(layout.plotRect.right() - bottomTickWidth - 3,
        layout.plotRect.bottom() + 2, bottomTickWidth + 4, bottomTickHeight),
        Qt::AlignRight | Qt::AlignTop, timeMaxText);
    painter.drawText(QRect(leftAxisTextLeft, layout.plotRect.top()
        - bottomTickHeight / 2, leftAxisTextWidth, bottomTickHeight),
        Qt::AlignRight | Qt::AlignVCenter, waveformMaxText);
    painter.drawText(QRect(leftAxisTextLeft, layout.plotRect.bottom()
        - bottomTickHeight / 2, leftAxisTextWidth, bottomTickHeight),
        Qt::AlignRight | Qt::AlignVCenter, waveformMinText);
  } else {
    painter.drawText(QRect(layout.plotRect.left(), layout.plotRect.bottom() + 2,
        bottomTickWidth + 4, bottomTickHeight), Qt::AlignLeft | Qt::AlignTop,
        waveformMinText);
    painter.drawText(QRect(layout.plotRect.right() - bottomTickWidth - 3,
        layout.plotRect.bottom() + 2, bottomTickWidth + 4, bottomTickHeight),
        Qt::AlignRight | Qt::AlignTop, waveformMaxText);
    painter.drawText(QRect(leftAxisTextLeft, layout.plotRect.top()
        - bottomTickHeight / 2, leftAxisTextWidth, bottomTickHeight),
        Qt::AlignRight | Qt::AlignVCenter, timeMinText);
    painter.drawText(QRect(leftAxisTextLeft, layout.plotRect.bottom()
        - bottomTickHeight / 2, leftAxisTextWidth, bottomTickHeight),
        Qt::AlignRight | Qt::AlignVCenter, timeMaxText);
  }

  painter.setFont(labelFont);
  if (!layout.xLabelRect.isEmpty()) {
    painter.drawText(layout.xLabelRect, Qt::AlignCenter, physicalXAxisLabel());
  }
  if (!layout.yLabelRect.isEmpty()) {
    const QString yLabelText = physicalYAxisLabel().trimmed();
    if (!yLabelText.isEmpty()) {
      const int textWidth = std::max(1,
          labelMetrics.horizontalAdvance(yLabelText));
      const int textHeight = std::max(1, labelMetrics.height());
      QImage textImage(textWidth, textHeight,
          QImage::Format_ARGB32_Premultiplied);
      textImage.fill(Qt::transparent);

      QPainter textPainter(&textImage);
      textPainter.setFont(labelFont);
      textPainter.setPen(plotForeground);
      textPainter.drawText(QPointF(0.0, labelMetrics.ascent()), yLabelText);
      textPainter.end();

      QTransform transform;
      transform.rotate(-90.0);
      const QImage rotatedText = textImage.transformed(transform);
      const QPoint drawPoint(layout.yLabelRect.center().x()
          - rotatedText.width() / 2, layout.yLabelRect.center().y()
          - rotatedText.height() / 2);
      painter.drawImage(drawPoint, rotatedText);
    }
  }

  if (!layout.legendRect.isEmpty()) {
    painter.fillRect(layout.legendRect, plotBackground);
    painter.setPen(QPen(plotForeground, 1));
    painter.drawLine(layout.legendRect.topLeft(),
        layout.legendRect.bottomLeft());
    QRect gradientRect = layout.legendRect.adjusted(kLegendPadding,
        kLegendPadding, -kLegendPadding - legendTextWidth,
        -kLegendPadding);
    if (gradientRect.width() > 0 && gradientRect.height() > 0) {
      QLinearGradient gradient(QPointF(0.0, gradientRect.top()),
          QPointF(0.0, gradientRect.bottom()));
      const QVector<QRgb> &palette = cachedPalette();
      for (int i = 0; i < 256; ++i) {
        gradient.setColorAt(1.0 - (i / 255.0), QColor(palette[i]));
      }
      painter.fillRect(gradientRect, gradient);
      painter.drawRect(gradientRect.adjusted(0, 0, -1, -1));
    }
    painter.setFont(legendFont);
    QRect textRect = layout.legendRect.adjusted(
        kLegendPadding + gradientRect.width() + kLegendPadding, kLegendPadding,
        -kLegendPadding, -kLegendPadding);
    painter.drawText(textRect, Qt::AlignTop | Qt::AlignLeft, legendMaxText);
    painter.drawText(textRect, Qt::AlignBottom | Qt::AlignLeft, legendMinText);
  }

  if (executeMode_ && !dataChannel_.isEmpty() && !runtimeConnected_) {
    painter.fillRect(layout.plotRect, QColor(0, 0, 0, 96));
    painter.setPen(Qt::white);
    painter.drawText(layout.plotRect, Qt::AlignCenter,
        QStringLiteral("Disconnected"));
  }

  drawRaisedFrame(painter, frameRect, plotBackground);
  if (selected_) {
    drawSelectionOutline(painter, selectionRect);
  }
}

void WaterfallPlotElement::mousePressEvent(QMouseEvent *event)
{
  if (executeMode_) {
    if (event->button() == Qt::MiddleButton) {
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
    if (event->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
    if (event->button() == Qt::RightButton) {
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF pos = event->position();
#else
    const QPointF pos = event->localPos();
#endif
    if (event->button() == Qt::LeftButton && isZoomed()
        && lastImageRect_.contains(pos.toPoint())) {
      panning_ = true;
      panStartPos_ = pos.toPoint();
      panStartTimeViewStart_ = timeViewStart_;
      panStartTimeViewEnd_ = timeViewEnd_;
    }
  }
  QWidget::mousePressEvent(event);
}

void WaterfallPlotElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    panning_ = false;
  }
  QWidget::mouseReleaseEvent(event);
}

void WaterfallPlotElement::mouseMoveEvent(QMouseEvent *event)
{
  if (!panning_ || !executeMode_ || !isZoomed()) {
    QWidget::mouseMoveEvent(event);
    return;
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPoint pos = event->position().toPoint();
#else
  const QPoint pos = event->localPos().toPoint();
#endif
  const int axisPixels = isHorizontalScroll(scrollDirection_)
      ? std::max(lastImageRect_.width(), kMinimumTimeViewPixels)
      : std::max(lastImageRect_.height(), kMinimumTimeViewPixels);
  const int deltaPixels = isHorizontalScroll(scrollDirection_)
      ? (pos.x() - panStartPos_.x())
      : (pos.y() - panStartPos_.y());
  const double span = panStartTimeViewEnd_ - panStartTimeViewStart_;
  const double delta = static_cast<double>(deltaPixels) / axisPixels * span;
  double newStart = panStartTimeViewStart_ - delta;
  if (newStart < 0.0) {
    newStart = 0.0;
  }
  if (newStart + span > 1.0) {
    newStart = 1.0 - span;
  }
  timeViewStart_ = std::clamp(newStart, 0.0, 1.0);
  timeViewEnd_ = std::clamp(timeViewStart_ + span, 0.0, 1.0);
  update();
  QWidget::mouseMoveEvent(event);
}

void WaterfallPlotElement::wheelEvent(QWheelEvent *event)
{
  if (!executeMode_ || historyCount_ <= 1) {
    QWidget::wheelEvent(event);
    return;
  }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->posF();
#endif
  if (!lastImageRect_.contains(pos.toPoint())) {
    QWidget::wheelEvent(event);
    return;
  }

  const int delta = event->angleDelta().y();
  if (delta == 0) {
    event->accept();
    return;
  }

  const double localFraction = isHorizontalScroll(scrollDirection_)
      ? std::clamp((pos.x() - lastImageRect_.left())
            / std::max(1.0, static_cast<double>(lastImageRect_.width())),
            0.0, 1.0)
      : std::clamp((pos.y() - lastImageRect_.top())
            / std::max(1.0, static_cast<double>(lastImageRect_.height())),
            0.0, 1.0);
  const double oldSpan = timeViewEnd_ - timeViewStart_;
  const double factor = delta > 0 ? 0.8 : 1.25;
  const double minSpan = 1.0 / std::max(1, historyCount_);
  double newSpan = std::clamp(oldSpan * factor, minSpan, 1.0);
  if (newSpan >= 0.999) {
    resetZoom();
    event->accept();
    return;
  }

  const double center = timeViewStart_ + localFraction * oldSpan;
  double newStart = center - localFraction * newSpan;
  if (newStart < 0.0) {
    newStart = 0.0;
  }
  if (newStart + newSpan > 1.0) {
    newStart = 1.0 - newSpan;
  }
  timeViewStart_ = std::clamp(newStart, 0.0, 1.0);
  timeViewEnd_ = std::clamp(timeViewStart_ + newSpan, 0.0, 1.0);
  update();
  event->accept();
}

bool WaterfallPlotElement::event(QEvent *event)
{
  if (event && event->type() == QEvent::ToolTip) {
    auto *helpEvent = static_cast<QHelpEvent *>(event);
    const QString text = tooltipTextForPosition(helpEvent->pos());
    if (!text.isEmpty()) {
      QToolTip::showText(helpEvent->globalPos(), text, this, lastImageRect_);
    } else {
      QToolTip::hideText();
      event->ignore();
    }
    return true;
  }
  return QWidget::event(event);
}

void WaterfallPlotElement::invalidateImageCache(bool requestUpdate)
{
  imageCacheValid_ = false;
  cachedImage_ = QImage();
  cachedIntensityRange_ = {};
  if (requestUpdate) {
    update();
  }
}

void WaterfallPlotElement::invalidatePaletteCache()
{
  paletteCacheValid_ = false;
  cachedPalette_.clear();
}

void WaterfallPlotElement::ensureBufferCapacity(int columnCount)
{
  const int clampedColumns = std::max(columnCount, runtimeWaveformLength_);
  if (clampedColumns <= 0) {
    return;
  }
  if (bufferColumnCount_ == clampedColumns && sampleLengths_.size() == historyCount_) {
    return;
  }

  bufferColumnCount_ = clampedColumns;
  bufferValues_.resize(historyCount_ * bufferColumnCount_);
  sampleLengths_.fill(0, historyCount_);
  sampleTimestampsMs_.fill(0, historyCount_);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::fill(bufferValues_.begin(), bufferValues_.end(), nan);
  writeCursor_ = 0;
  bufferedSamples_ = 0;
}

int WaterfallPlotElement::bufferSlotForSample(int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= bufferedSamples_) {
    return -1;
  }
  if (bufferedSamples_ < historyCount_) {
    return sampleIndex;
  }
  return (writeCursor_ + sampleIndex) % historyCount_;
}

int WaterfallPlotElement::leadingBlankCount() const
{
  if (newestAtLeadingEdge(scrollDirection_)) {
    return 0;
  }
  return std::max(0, historyCount_ - bufferedSamples_);
}

int WaterfallPlotElement::timeIndexForChronologicalSample(int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= bufferedSamples_) {
    return -1;
  }
  if (newestAtLeadingEdge(scrollDirection_)) {
    return bufferedSamples_ - 1 - sampleIndex;
  }
  return leadingBlankCount() + sampleIndex;
}

int WaterfallPlotElement::chronologicalSampleForTimeIndex(int timeIndex) const
{
  if (timeIndex < 0 || timeIndex >= historyCount_) {
    return -1;
  }
  if (newestAtLeadingEdge(scrollDirection_)) {
    if (timeIndex >= bufferedSamples_) {
      return -1;
    }
    return bufferedSamples_ - 1 - timeIndex;
  }
  const int blank = leadingBlankCount();
  if (timeIndex < blank) {
    return -1;
  }
  const int sampleIndex = timeIndex - blank;
  return sampleIndex < bufferedSamples_ ? sampleIndex : -1;
}

int WaterfallPlotElement::visibleTimeStartIndex() const
{
  const int index = static_cast<int>(std::floor(timeViewStart_ * historyCount_));
  return std::clamp(index, 0, std::max(0, historyCount_ - 1));
}

int WaterfallPlotElement::visibleTimeEndIndex() const
{
  const int start = visibleTimeStartIndex();
  const int index = static_cast<int>(std::ceil(timeViewEnd_ * historyCount_));
  return std::clamp(std::max(start + 1, index), 1, historyCount_);
}

WaterfallPlotElement::IntensityRange
WaterfallPlotElement::computeIntensityRange() const
{
  if (intensityScale_ == WaterfallIntensityScale::kManual) {
    IntensityRange range;
    range.valid = true;
    range.minimum = intensityMin_;
    range.maximum = intensityMax_;
    if (!(range.maximum > range.minimum)) {
      range.maximum = range.minimum + 1.0;
    }
    return range;
  }

  bool haveValue = false;
  double minimum = 0.0;
  double maximum = 0.0;
  for (int sample = 0; sample < bufferedSamples_; ++sample) {
    const int slot = bufferSlotForSample(sample);
    if (slot < 0) {
      continue;
    }
    const int length = sampleLengths_.value(slot, 0);
    const int base = slot * bufferColumnCount_;
    for (int i = 0; i < length; ++i) {
      const double value = bufferValues_.value(base + i);
      if (!std::isfinite(value)) {
        continue;
      }
      if (intensityScale_ == WaterfallIntensityScale::kLog && value <= 0.0) {
        continue;
      }
      if (!haveValue) {
        minimum = value;
        maximum = value;
        haveValue = true;
      } else {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
      }
    }
  }

  IntensityRange range;
  range.valid = haveValue;
  range.minimum = minimum;
  range.maximum = maximum;
  if (!range.valid) {
    range.minimum = 0.0;
    range.maximum = 1.0;
    return range;
  }
  if (!(range.maximum > range.minimum)) {
    if (intensityScale_ == WaterfallIntensityScale::kLog) {
      range.maximum = range.minimum * 10.0;
    } else {
      range.maximum = range.minimum + 1.0;
    }
  }
  return range;
}

double WaterfallPlotElement::normalizedIntensity(double value,
    const IntensityRange &range) const
{
  if (!std::isfinite(value) || !range.valid) {
    return -1.0;
  }

  if (intensityScale_ == WaterfallIntensityScale::kLog) {
    if (value <= 0.0 || range.minimum <= 0.0 || range.maximum <= 0.0) {
      return -1.0;
    }
    const double logMin = std::log10(range.minimum);
    const double logMax = std::log10(range.maximum);
    if (!(logMax > logMin)) {
      return 0.0;
    }
    return std::clamp((std::log10(value) - logMin) / (logMax - logMin),
        0.0, 1.0);
  }

  if (!(range.maximum > range.minimum)) {
    return 0.0;
  }
  return std::clamp((value - range.minimum) / (range.maximum - range.minimum),
      0.0, 1.0);
}

QImage WaterfallPlotElement::buildPlotImage() const
{
  if (imageCacheValid_) {
    return cachedImage_;
  }

  imageCacheValid_ = true;
  cachedImage_ = QImage();
  cachedIntensityRange_ = computeIntensityRange();
  const int columns = waveformLength();
  if (columns <= 0 || historyCount_ <= 0) {
    return cachedImage_;
  }

  const bool horizontal = isHorizontalScroll(scrollDirection_);
  const QSize imageSize(horizontal ? historyCount_ : columns,
      horizontal ? columns : historyCount_);
  cachedImage_ = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
  cachedImage_.fill(effectiveColor(this, backgroundColor_, QPalette::Window,
      QColor(Qt::white)));
  const QVector<QRgb> &palette = cachedPalette();

  for (int sample = 0; sample < bufferedSamples_; ++sample) {
    const int slot = bufferSlotForSample(sample);
    const int timeIndex = timeIndexForChronologicalSample(sample);
    if (slot < 0 || timeIndex < 0) {
      continue;
    }
    const int length = std::min(sampleLengths_.value(slot, 0), columns);
    const int base = slot * bufferColumnCount_;
    for (int i = 0; i < length; ++i) {
      const double value = bufferValues_.value(base + i);
      const double normalized = normalizedIntensity(value, cachedIntensityRange_);
      if (normalized < 0.0) {
        continue;
      }
      const int paletteIndex = std::clamp(
          static_cast<int>(normalized * 255.0 + 0.5), 0, 255);
      if (horizontal) {
        cachedImage_.setPixel(timeIndex, columns - 1 - i, palette[paletteIndex]);
      } else {
        cachedImage_.setPixel(i, timeIndex, palette[paletteIndex]);
      }
    }
  }

  return cachedImage_;
}

const QVector<QRgb> &WaterfallPlotElement::cachedPalette() const
{
  if (!paletteCacheValid_) {
    cachedPalette_ = buildHeatmapPalette(colorMap_, invertGreyscale_);
    paletteCacheValid_ = true;
  }
  return cachedPalette_;
}

WaterfallPlotElement::PlotLayout WaterfallPlotElement::computeLayout(
    const QRect &bounds, int titleHeight, int axisLabelHeight,
    int tickLabelHeight, int leftTickWidth, int legendTextWidth) const
{
  PlotLayout layout;
  QRect working = bounds.adjusted(kOuterMargin, kOuterMargin,
      -kOuterMargin, -kOuterMargin);

  if (!title_.isEmpty()) {
    layout.titleRect = QRect(working.left(), working.top(), working.width(),
        titleHeight);
    working.setTop(layout.titleRect.bottom() + 2);
  }

  const int yLabelWidth = std::max(axisLabelHeight + 6, 18);
  const int leftMargin = yLabelWidth + kTickGap
      + std::max(leftTickWidth, 24) + 6;
  const int bottomMargin = tickLabelHeight + kTickGap + axisLabelHeight + 2;
  const int rightMargin = showLegend_
      ? (kLegendWidth + kLegendPadding * 3 + std::max(legendTextWidth, 24))
      : 8;
  layout.yLabelRect = QRect(working.left(), working.top(), yLabelWidth,
      std::max(1, working.height() - bottomMargin));
  layout.xLabelRect = QRect(working.left() + leftMargin, working.bottom()
      - axisLabelHeight + 1,
      std::max(1, working.width() - leftMargin - rightMargin),
      axisLabelHeight);
  layout.plotRect = QRect(working.left() + leftMargin, working.top(),
      std::max(1, working.width() - leftMargin - rightMargin),
      std::max(1, working.height() - bottomMargin));
  if (showLegend_) {
    layout.legendRect = QRect(layout.plotRect.right() + kLegendPadding,
        layout.plotRect.top(),
        kLegendWidth + kLegendPadding * 3 + std::max(legendTextWidth, 24),
        layout.plotRect.height());
  }
  return layout;
}

QString WaterfallPlotElement::physicalXAxisLabel() const
{
  return isHorizontalScroll(scrollDirection_) ? yLabel_ : xLabel_;
}

QString WaterfallPlotElement::physicalYAxisLabel() const
{
  return isHorizontalScroll(scrollDirection_) ? xLabel_ : yLabel_;
}

double WaterfallPlotElement::configuredSamplePeriodSeconds() const
{
  if (!(samplePeriod_ > 0.0)) {
    return 0.0;
  }

  switch (units_) {
  case TimeUnits::kMilliseconds:
    return samplePeriod_ / 1000.0;
  case TimeUnits::kMinutes:
    return samplePeriod_ * 60.0;
  case TimeUnits::kSeconds:
  default:
    return samplePeriod_;
  }
}

double WaterfallPlotElement::estimatedSamplePeriodSeconds() const
{
  const double configuredPeriodSeconds = configuredSamplePeriodSeconds();
  if (configuredPeriodSeconds > 0.0) {
    return configuredPeriodSeconds;
  }
  if (bufferedSamples_ > 1) {
    const qint64 oldest = sampleTimestampMs(0);
    const qint64 newest = sampleTimestampMs(bufferedSamples_ - 1);
    if (oldest > 0 && newest > oldest) {
      return (newest - oldest) / 1000.0 / (bufferedSamples_ - 1);
    }
  }
  return 1.0;
}

double WaterfallPlotElement::ageSecondsForTimeIndex(int timeIndex) const
{
  const int sampleIndex = chronologicalSampleForTimeIndex(timeIndex);
  if (sampleIndex >= 0) {
    return ageSecondsForSample(sampleIndex);
  }
  const double period = estimatedSamplePeriodSeconds();
  if (newestAtLeadingEdge(scrollDirection_)) {
    return timeIndex * period;
  }
  return (historyCount_ - 1 - timeIndex) * period;
}

double WaterfallPlotElement::ageSecondsForSample(int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= bufferedSamples_) {
    return 0.0;
  }
  const qint64 timestamp = sampleTimestampMs(sampleIndex);
  const double configuredPeriodSeconds = configuredSamplePeriodSeconds();
  if (configuredPeriodSeconds > 0.0) {
    return (bufferedSamples_ - 1 - sampleIndex) * configuredPeriodSeconds;
  }
  if (timestamp > 0) {
    return std::max(0.0,
        (QDateTime::currentMSecsSinceEpoch() - timestamp) / 1000.0);
  }
  return (bufferedSamples_ - 1 - sampleIndex) * estimatedSamplePeriodSeconds();
}

double WaterfallPlotElement::convertSecondsForUnits(double seconds) const
{
  switch (units_) {
  case TimeUnits::kMilliseconds:
    return seconds * 1000.0;
  case TimeUnits::kMinutes:
    return seconds / 60.0;
  case TimeUnits::kSeconds:
  default:
    return seconds;
  }
}

QString WaterfallPlotElement::timeAxisValueText(int timeIndex) const
{
  return QString::number(convertSecondsForUnits(ageSecondsForTimeIndex(timeIndex)),
      'g', 4);
}

QString WaterfallPlotElement::intensityValueText(double value) const
{
  return QString::number(value, 'g', 6);
}

QString WaterfallPlotElement::tooltipTextForPosition(const QPoint &position) const
{
  if (!executeMode_ || !lastImageRect_.contains(position) || waveformLength() <= 0) {
    return QString();
  }

  const bool horizontal = isHorizontalScroll(scrollDirection_);
  const int visibleTime = std::max(1, lastVisibleTimeEnd_ - lastVisibleTimeStart_);
  const double xFraction = std::clamp((position.x() - lastImageRect_.left())
      / std::max(1.0, static_cast<double>(lastImageRect_.width())), 0.0, 0.999999);
  const double yFraction = std::clamp((position.y() - lastImageRect_.top())
      / std::max(1.0, static_cast<double>(lastImageRect_.height())), 0.0, 0.999999);

  int timeIndex = 0;
  int waveformIndex = 0;
  if (horizontal) {
    timeIndex = lastVisibleTimeStart_
        + static_cast<int>(xFraction * visibleTime);
    waveformIndex = waveformLength() - 1
        - static_cast<int>(yFraction * waveformLength());
  } else {
    timeIndex = lastVisibleTimeStart_
        + static_cast<int>(yFraction * visibleTime);
    waveformIndex = static_cast<int>(xFraction * waveformLength());
  }

  waveformIndex = std::clamp(waveformIndex, 0, std::max(0, waveformLength() - 1));
  timeIndex = std::clamp(timeIndex, 0, std::max(0, historyCount_ - 1));
  const int sampleIndex = chronologicalSampleForTimeIndex(timeIndex);
  if (sampleIndex < 0 || waveformIndex >= sampleLength(sampleIndex)) {
    return QString();
  }

  const double value = sampleValue(sampleIndex, waveformIndex);
  if (!std::isfinite(value)) {
    return QString();
  }

  return QStringLiteral("Index: %1\nAge: %2\nIntensity: %3")
      .arg(waveformIndex)
      .arg(QString::number(convertSecondsForUnits(ageSecondsForSample(sampleIndex)),
          'g', 4))
      .arg(intensityValueText(value));
}

bool WaterfallPlotElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  if (!event) {
    return false;
  }
  QWidget *target = window();
  if (!target) {
    return false;
  }
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

void WaterfallPlotElement::drawSelectionOutline(QPainter &painter,
    const QRect &rect) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect);
}
