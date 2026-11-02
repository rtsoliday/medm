#include "strip_chart_element.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include "medm_colors.h"
#include "pv_name_utils.h"
#include "text_font_utils.h"
#include "window_utils.h"

namespace {

constexpr int kShadowThickness = 3;
/* Space per pen color bar beside stacked y-axis labels (sccd.lineSpace). */
constexpr int kAxisLineSpace = 3;
constexpr double kPenSampleCount = 24.0;
constexpr int kDefaultRefreshIntervalMs = 100;
constexpr int kMaxRefreshIntervalMs = 1000;
constexpr int kLateThresholdMs = 100;
constexpr int kLateCountThreshold = 5;
constexpr int kOnTimeCountThreshold = 20;
constexpr int kIntervalIncrementMs = 100;
constexpr int kMaxSampleBurst = 32;

/* The span a pen is drawn over. Small-unit channels, such as vacuum
 * pressures near 1e-10 Torr, keep their own span; only equal limits, which
 * would divide by zero, are drawn over a span of 1. */
double drawRange(double low, double high)
{
  const double range = std::abs(high - low);
  return range > 0.0 ? range : 1.0;
}

constexpr int kDefaultPenColorIndex = 14;

qint64 roundedPositiveInterval(double value)
{
  if (!std::isfinite(value) || value <= 0.0) {
    return 1;
  }
  if (value >= static_cast<double>(std::numeric_limits<qint64>::max())) {
    return std::numeric_limits<qint64>::max();
  }
  return std::max<qint64>(1, static_cast<qint64>(std::llround(value)));
}

qint64 saturatingAdd(qint64 base, qint64 increment)
{
  if (increment > std::numeric_limits<qint64>::max() - base) {
    return std::numeric_limits<qint64>::max();
  }
  return base + increment;
}

// Calculate axis label font size based on widget dimensions (mimics MEDM)
int calculateLabelFontSize(int widgetWidth, int widgetHeight)
{
  const int minDim = std::min(widgetWidth, widgetHeight);
  if (minDim > 1000) {
    return 18;
  } else if (minDim > 900) {
    return 16;
  } else if (minDim > 750) {
    return 14;
  } else if (minDim > 600) {
    return 12;
  } else if (minDim > 400) {
    return 10;
  }
  return 8;  // Target pixel height, not point size
}

// Calculate title font size based on widget dimensions (mimics MEDM)
int calculateTitleFontSize(int widgetWidth, int widgetHeight)
{
  const int minDim = std::min(widgetWidth, widgetHeight);
  if (minDim > 1000) {
    return 26;
  } else if (minDim > 900) {
    return 24;
  } else if (minDim > 750) {
    return 22;
  } else if (minDim > 600) {
    return 20;
  } else if (minDim > 500) {
    return 18;
  } else if (minDim > 400) {
    return 16;
  } else if (minDim > 300) {
    return 14;
  } else if (minDim > 250) {
    return 12;
  } else if (minDim > 200) {
    return 10;
  }
  return 8;
}

// Calculate the spacing between chart parts (mimics MEDM's calcMargin)
int calculateMargin(int widgetWidth, int widgetHeight)
{
  const int minDimension = std::min(widgetWidth, widgetHeight);
  if (minDimension > 1000) {
    return 6;
  } else if (minDimension > 800) {
    return 5;
  } else if (minDimension > 600) {
    return 4;
  } else if (minDimension > 400) {
    return 3;
  } else if (minDimension > 300) {
    return 2;
  } else if (minDimension > 200) {
    return 1;
  }
  return 0;
}

int calculateMarkerHeight(int widgetWidth, int widgetHeight)
{
  const int minDimension = std::min(widgetWidth, widgetHeight);
  if (minDimension > 1000) {
    return 6;
  } else if (minDimension > 800) {
    return 5;
  } else if (minDimension > 600) {
    return 4;
  } else if (minDimension > 400) {
    return 3;
  } else if (minDimension > 300) {
    return 2;
  }
  return 1;
}

QColor defaultPenColor(int index)
{
  Q_UNUSED(index);
  const auto &palette = MedmColors::palette();
  if (palette.size() > kDefaultPenColorIndex) {
    return palette[kDefaultPenColorIndex];
  }
  if (!palette.empty()) {
    return palette.back();
  }
  return QColor(Qt::black);
}

struct NumberFormat
{
  char format;     // 'f' for fixed, 'e' for scientific
  int decimal;     // decimal places
  int width;       // field width
};

NumberFormat calculateNumberFormat(double value)
{
  NumberFormat fmt;
  if (value == 0.0) {
    fmt.format = 'f';
    fmt.decimal = 1;
    fmt.width = 3;
    return fmt;
  }

  const double order = std::log10(std::abs(value));

  if (order > 5.0 || order < -4.0) {
    fmt.format = 'e';
    fmt.decimal = 1;
  } else {
    fmt.format = 'f';
    if (order < 0.0) {
      fmt.decimal = static_cast<int>(order) * -1 + 2;
    } else {
      fmt.decimal = 1;
    }
  }

  if (order >= 4.0) {
    fmt.width = 7;
  } else if (order >= 3.0) {
    fmt.width = 6;
  } else if (order >= 2.0) {
    fmt.width = 5;
  } else if (order >= 1.0) {
    fmt.width = 4;
  } else if (order >= 0.0) {
    fmt.width = 3;
  } else if (order >= -1.0) {
    fmt.width = 4;
  } else if (order >= -2.0) {
    fmt.width = 5;
  } else if (order >= -3.0) {
    fmt.width = 6;
  } else {
    fmt.width = 7;
  }

  return fmt;
}

QString formatNumber(double value, char format, int decimal)
{
  if (format == 'e') {
    return QString::asprintf("%.1e", value);
  } else {
    return QString::asprintf("%.*f", decimal, value);
  }
}

} // namespace

StripChartElement::StripChartElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  // Start with empty labels (like MEDM) - they get set from ADL file if defined
  // X-axis label will auto-generate based on time units if left empty
  title_ = QString();
  xLabel_ = QString();
  yLabel_ = QString();
  for (int i = 0; i < static_cast<int>(pens_.size()); ++i) {
    /* MEDM's penAttributeInit ranges a pen over its channel's limits; a
     * pen without a limits block keeps that source. */
    pens_[i].limits.lowSource = PvLimitSource::kChannel;
    pens_[i].limits.highSource = PvLimitSource::kChannel;
    pens_[i].limits.lowDefault = 0.0;
    pens_[i].limits.highDefault = 1.0;  /* MEDM's HOPR_DEFAULT */
    pens_[i].limits.precisionSource = PvLimitSource::kChannel;
    pens_[i].limits.precisionDefault = 0;
    pens_[i].color = defaultPenColor(i);
    pens_[i].runtimeConnected = false;
    pens_[i].runtimeReadAccessKnown = false;
    pens_[i].runtimeReadAccess = false;
    pens_[i].runtimeLimitsValid = false;
    pens_[i].runtimeLow = pens_[i].limits.lowDefault;
    pens_[i].runtimeHigh = pens_[i].limits.highDefault;
  }
}

void StripChartElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool StripChartElement::isSelected() const
{
  return selected_;
}

QColor StripChartElement::foregroundColor() const
{
  return foregroundColor_;
}

void StripChartElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  invalidateStaticCache();
  update();
}

QColor StripChartElement::backgroundColor() const
{
  return backgroundColor_;
}

void StripChartElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  invalidateStaticCache();
  update();
}

QString StripChartElement::title() const
{
  return title_;
}

void StripChartElement::setTitle(const QString &title)
{
  if (title_ == title) {
    return;
  }
  title_ = title;
  invalidateStaticCache();
  update();
}

QString StripChartElement::xLabel() const
{
  return xLabel_;
}

void StripChartElement::setXLabel(const QString &label)
{
  if (xLabel_ == label) {
    return;
  }
  xLabel_ = label;
  invalidateStaticCache();
  update();
}

QString StripChartElement::yLabel() const
{
  return yLabel_;
}

void StripChartElement::setYLabel(const QString &label)
{
  if (yLabel_ == label) {
    return;
  }
  yLabel_ = label;
  invalidateStaticCache();
  update();
}

double StripChartElement::period() const
{
  return period_;
}

void StripChartElement::setPeriod(double period)
{
  const double clamped = std::isfinite(period) && period > 0.0
      ? period : kDefaultStripChartPeriod;
  if (period_ == clamped) {
    return;
  }
  period_ = clamped;
  lastSampleMs_ = 0;
  nextAdvanceTimeMs_ = 0;
  sampleIntervalMs_ = periodMilliseconds();
  cachedChartWidth_ = 0;
  // Clear sample history since samples were taken at the old interval
  sampleHistoryLength_ = 0;
  for (Pen &pen : pens_) {
    pen.samples.clear();
    pen.minSamples.clear();
    pen.maxSamples.clear();
    pen.sampleTimestampsMs.clear();
    pen.hasIntervalRange = false;
  }
  updateSamplingGeometry(chartRect().width());
  invalidateStaticCache();
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

TimeUnits StripChartElement::units() const
{
  return units_;
}

void StripChartElement::setUnits(TimeUnits units)
{
  if (units_ == units) {
    return;
  }
  units_ = units;
  lastSampleMs_ = 0;
  nextAdvanceTimeMs_ = 0;
  sampleIntervalMs_ = periodMilliseconds();
  cachedChartWidth_ = 0;
  // Clear sample history since samples were taken at the old interval
  sampleHistoryLength_ = 0;
  for (Pen &pen : pens_) {
    pen.samples.clear();
    pen.minSamples.clear();
    pen.maxSamples.clear();
    pen.sampleTimestampsMs.clear();
    pen.hasIntervalRange = false;
  }
  updateSamplingGeometry(chartRect().width());
  invalidateStaticCache();
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

int StripChartElement::penCount() const
{
  return static_cast<int>(pens_.size());
}

QString StripChartElement::channel(int index) const
{
  if (index < 0 || index >= penCount()) {
    return QString();
  }
  return pens_[index].channel;
}

void StripChartElement::setChannel(int index, const QString &channel)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (pens_[index].channel == normalized) {
    return;
  }
  pens_[index].channel = normalized;
  if (executeMode_) {
    clearPenRuntimeState(index);
  }
  updateRefreshTimer();
  update();
}

QColor StripChartElement::penColor(int index) const
{
  if (index < 0 || index >= penCount()) {
    return QColor();
  }
  return pens_[index].color;
}

void StripChartElement::setPenColor(int index, const QColor &color)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  if (pens_[index].color == color) {
    return;
  }
  pens_[index].color = color;
  invalidatePenCache();
  update();
}

PvLimits StripChartElement::penLimits(int index) const
{
  if (index < 0 || index >= penCount()) {
    return PvLimits{};
  }
  PvLimits limits = pens_[index].limits;
  limits.precisionSource = PvLimitSource::kChannel;
  limits.precisionDefault = 0;
  return limits;
}

double StripChartElement::penDisplayLowValue(int index,
    PvLimitSource source) const
{
  if (index < 0 || index >= penCount()) {
    return 0.0;
  }
  const Pen &pen = pens_[index];
  if (source == PvLimitSource::kChannel && pen.runtimeLimitsValid) {
    return pen.runtimeLow;
  }
  return pen.limits.lowDefault;
}

double StripChartElement::penDisplayHighValue(int index,
    PvLimitSource source) const
{
  if (index < 0 || index >= penCount()) {
    return 1.0;
  }
  const Pen &pen = pens_[index];
  if (source == PvLimitSource::kChannel && pen.runtimeLimitsValid) {
    return pen.runtimeHigh;
  }
  return pen.limits.highDefault;
}

void StripChartElement::setPenLimits(int index, const PvLimits &limits)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  PvLimits sanitized = limits;
  sanitized.precisionSource = PvLimitSource::kChannel;
  sanitized.precisionDefault = 0;

  Pen &pen = pens_[index];
  PvLimits &stored = pen.limits;
  const bool changed = stored.lowSource != sanitized.lowSource
      || stored.highSource != sanitized.highSource
      || stored.lowDefault != sanitized.lowDefault
      || stored.highDefault != sanitized.highDefault
      || stored.precisionSource != sanitized.precisionSource
      || stored.precisionDefault != sanitized.precisionDefault;
  if (!changed) {
    return;
  }
  stored = sanitized;
  pen.runtimeLimitsValid = false;
  if (pen.limits.lowSource != PvLimitSource::kChannel) {
    pen.runtimeLow = pen.limits.lowDefault;
  }
  if (pen.limits.highSource != PvLimitSource::kChannel) {
    pen.runtimeHigh = pen.limits.highDefault;
  }
  invalidateStaticCache();
  invalidatePenCache();
  update();
}

void StripChartElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
  updateSamplingGeometry(chartRect().width());
  invalidateStaticCache();
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

bool StripChartElement::isExecuteMode() const
{
  return executeMode_;
}

void StripChartElement::setRuntimeConnected(int index, bool connected)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  if (pen.runtimeConnected == connected) {
    return;
  }
  pen.runtimeConnected = connected;
  if (!connected) {
    pen.runtimeReadAccessKnown = false;
    pen.runtimeReadAccess = false;
    pen.runtimeLimitsValid = false;
    pen.runtimeLow = pen.limits.lowDefault;
    pen.runtimeHigh = pen.limits.highDefault;
    pen.hasRuntimeValue = false;
    pen.hasIntervalRange = false;
  }
  invalidateStaticCache();
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

void StripChartElement::setRuntimeReadAccessKnown(int index, bool known)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  if (pen.runtimeReadAccessKnown == known) {
    return;
  }
  pen.runtimeReadAccessKnown = known;
  if (!known) {
    pen.runtimeReadAccess = false;
  }
  invalidateStaticCache();
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

void StripChartElement::setRuntimeReadAccess(int index, bool readAccess)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  const bool normalized = pen.runtimeReadAccessKnown && readAccess;
  if (pen.runtimeReadAccess == normalized) {
    return;
  }
  pen.runtimeReadAccess = normalized;
  invalidateStaticCache();
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

void StripChartElement::setRuntimeLimits(int index, double low, double high)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return;
  }
  /* MEDM raises an upper limit only when both limits are 0; a narrow
   * channel range such as 1e-11 to 1e-9 is kept. */
  if (high == low) {
    high = low + 1.0;
  }
  Pen &pen = pens_[index];
  pen.runtimeLow = low;
  pen.runtimeHigh = high;
  pen.runtimeLimitsValid = true;
  invalidateStaticCache();
  invalidatePenCache();
  update();
}

void StripChartElement::addRuntimeSample(int index, double value, qint64 timestampMs)
{
  if (!executeMode_ || index < 0 || index >= penCount()) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }
  Pen &pen = pens_[index];
  if (!pen.runtimeConnected || (pen.runtimeReadAccessKnown && !pen.runtimeReadAccess)) {
    return;
  }

  pen.runtimeValue = value;
  pen.hasRuntimeValue = true;
  pen.runtimeTimestampMs = timestampMs;
  pen.hasRuntimeTimestamp = timestampMs > 0;
  if (pen.hasIntervalRange) {
    pen.intervalLow = std::min(pen.intervalLow, value);
    pen.intervalHigh = std::max(pen.intervalHigh, value);
  } else {
    pen.intervalLow = value;
    pen.intervalHigh = value;
    pen.hasIntervalRange = true;
  }
}

void StripChartElement::replaceRuntimeHistory(int index,
    const QVector<double> &values, const QVector<qint64> &timestamps,
    qint64 windowStartMs, qint64 windowEndMs)
{
  if (!executeMode_ || index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  pen.samples.clear();
  pen.minSamples.clear();
  pen.maxSamples.clear();
  pen.sampleTimestampsMs.clear();
  const int capacity = std::max(chartRect().width(), 1);
  const qint64 span = windowEndMs - windowStartMs;
  const qsizetype count = std::min(values.size(), timestamps.size());
  const double missingValue = std::numeric_limits<double>::quiet_NaN();
  if (span > 0 && count > 0) {
    pen.samples.assign(static_cast<std::size_t>(capacity), missingValue);
    pen.minSamples.assign(static_cast<std::size_t>(capacity), missingValue);
    pen.maxSamples.assign(static_cast<std::size_t>(capacity), missingValue);
    pen.sampleTimestampsMs.assign(static_cast<std::size_t>(capacity), 0);
  }
  bool haveSample = false;
  double lastValue = 0.0;
  qint64 lastTimestampMs = 0;
  for (qsizetype valueIndex = 0; valueIndex < count; ++valueIndex) {
    const double value = values.at(valueIndex);
    const qint64 timestamp = timestamps.at(valueIndex);
    if (!std::isfinite(value) || timestamp < windowStartMs
        || timestamp > windowEndMs || span <= 0) {
      continue;
    }
    const long double position =
        static_cast<long double>(timestamp - windowStartMs)
        / static_cast<long double>(span);
    const int slot = std::clamp(
        static_cast<int>(position * capacity), 0, capacity - 1);
    const std::size_t sampleIndex = static_cast<std::size_t>(slot);
    if (std::isfinite(pen.samples[sampleIndex])) {
      pen.minSamples[sampleIndex] =
          std::min(pen.minSamples[sampleIndex], value);
      pen.maxSamples[sampleIndex] =
          std::max(pen.maxSamples[sampleIndex], value);
    } else {
      pen.minSamples[sampleIndex] = value;
      pen.maxSamples[sampleIndex] = value;
    }
    pen.samples[sampleIndex] = value;
    pen.sampleTimestampsMs[sampleIndex] = timestamp;
    lastValue = value;
    lastTimestampMs = timestamp;
    haveSample = true;
  }
  if (!haveSample) {
    pen.samples.clear();
    pen.minSamples.clear();
    pen.maxSamples.clear();
    pen.sampleTimestampsMs.clear();
  } else {
    pen.runtimeConnected = true;
    pen.runtimeReadAccessKnown = true;
    pen.runtimeReadAccess = true;
    pen.runtimeValue = lastValue;
    pen.hasRuntimeValue = !archivePlot_ || archiveLiveMerge_;
    pen.runtimeTimestampMs = lastTimestampMs;
    pen.hasRuntimeTimestamp = pen.runtimeTimestampMs > 0;
  }
  int newLength = 0;
  for (const Pen &candidate : pens_) {
    newLength = std::max(newLength,
        static_cast<int>(candidate.samples.size()));
  }
  sampleHistoryLength_ = newLength;
  cachedChartWidth_ = capacity;
  sampleIntervalMs_ = std::max(
      static_cast<double>(span) / static_cast<double>(capacity), 10.0);
  lastSampleMs_ = windowEndMs;
  nextAdvanceTimeMs_ = saturatingAdd(windowEndMs,
      roundedPositiveInterval(sampleIntervalMs_));
  newSampleColumns_ = 0;
  autoscaleLimitsValid_ = false;
  invalidatePenCache();
  invalidateStaticCache();
  update();
}

bool StripChartElement::isArchivePlot() const
{
  return archivePlot_;
}

void StripChartElement::setArchivePlot(bool enabled)
{
  archivePlot_ = enabled;
  if (!archivePlot_) {
    archiveStatus_.clear();
  }
  update();
}

int StripChartElement::archiveMaximumPoints() const
{
  return archiveMaximumPoints_;
}

void StripChartElement::setArchiveMaximumPoints(int maximumPoints)
{
  archiveMaximumPoints_ = std::clamp(maximumPoints, 2, 100000);
}

bool StripChartElement::archiveLiveMerge() const
{
  return archiveLiveMerge_;
}

void StripChartElement::setArchiveLiveMerge(bool enabled)
{
  archiveLiveMerge_ = enabled;
}

QString StripChartElement::archiveStatus() const
{
  return archiveStatus_;
}

void StripChartElement::setArchiveStatus(const QString &status)
{
  archiveStatus_ = status;
  setToolTip(status);
  update();
}

double StripChartElement::historyDurationSeconds() const
{
  switch (units_) {
  case TimeUnits::kMilliseconds:
    return period_ / 1000.0;
  case TimeUnits::kMinutes:
    return period_ * 60.0;
  case TimeUnits::kSeconds:
  default:
    return period_;
  }
}

void StripChartElement::clearRuntimeState()
{
  sampleHistoryLength_ = 0;
  cachedChartWidth_ = 0;
  sampleIntervalMs_ = periodMilliseconds();
  lastSampleMs_ = 0;
  nextAdvanceTimeMs_ = 0;
  newSampleColumns_ = 0;
  // Reset adaptive refresh rate state
  currentRefreshIntervalMs_ = kDefaultRefreshIntervalMs;
  lateRefreshCount_ = 0;
  onTimeRefreshCount_ = 0;
  expectedRefreshTimeMs_ = 0;
  // Reset zoom/pan state
  zoomed_ = false;
  zoomYFactor_ = 1.0;
  zoomYCenter_ = 0.5;
  panning_ = false;
  // Reset autoscale cached limits (but keep autoscale_ mode setting)
  autoscaleLimitsValid_ = false;
  for (int i = 0; i < penCount(); ++i) {
    Pen &pen = pens_[i];
    pen.runtimeConnected = false;
    pen.runtimeReadAccessKnown = false;
    pen.runtimeReadAccess = false;
    pen.runtimeLimitsValid = false;
    pen.runtimeLow = pen.limits.lowDefault;
    pen.runtimeHigh = pen.limits.highDefault;
    pen.samples.clear();
    pen.minSamples.clear();
    pen.maxSamples.clear();
    pen.sampleTimestampsMs.clear();
    pen.runtimeValue = 0.0;
    pen.hasRuntimeValue = false;
    pen.runtimeTimestampMs = 0;
    pen.hasRuntimeTimestamp = false;
    pen.hasIntervalRange = false;
  }
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

/* Keep a pen's history but stop extending it with the last live value, so
 * the columns added while its channel is gone are gaps, not a flat line
 * that looks like current data. */
void StripChartElement::clearPenRuntimeValue(int index)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  pen.hasRuntimeValue = false;
  pen.hasIntervalRange = false;
}

void StripChartElement::clearPenRuntimeState(int index)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  pen.runtimeConnected = false;
  pen.runtimeReadAccessKnown = false;
  pen.runtimeReadAccess = false;
  pen.runtimeLimitsValid = false;
  pen.runtimeLow = pen.limits.lowDefault;
  pen.runtimeHigh = pen.limits.highDefault;
  pen.runtimeValue = 0.0;
  pen.hasRuntimeValue = false;
  pen.runtimeTimestampMs = 0;
  pen.hasRuntimeTimestamp = false;
  pen.hasIntervalRange = false;
  const double missingValue = std::numeric_limits<double>::quiet_NaN();
  if (sampleHistoryLength_ > 0) {
    pen.samples.assign(static_cast<std::size_t>(sampleHistoryLength_),
        missingValue);
    pen.minSamples.assign(static_cast<std::size_t>(sampleHistoryLength_),
        missingValue);
    pen.maxSamples.assign(static_cast<std::size_t>(sampleHistoryLength_),
        missingValue);
    pen.sampleTimestampsMs.assign(
        static_cast<std::size_t>(sampleHistoryLength_), 0);
  } else {
    pen.samples.clear();
    pen.minSamples.clear();
    pen.maxSamples.clear();
    pen.sampleTimestampsMs.clear();
  }
}

int StripChartElement::sampleCount() const
{
  return sampleHistoryLength_;
}

double StripChartElement::sampleValue(int penIndex, int sampleIndex) const
{
  if (penIndex < 0 || penIndex >= penCount()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const Pen &pen = pens_[penIndex];
  if (sampleIndex < 0 || sampleIndex >= static_cast<int>(pen.samples.size())) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return pen.samples[static_cast<std::size_t>(sampleIndex)];
}

qint64 StripChartElement::sampleTimestampMs(int sampleIndex) const
{
  if (sampleIndex < 0) {
    return 0;
  }
  qint64 latestTimestampMs = 0;
  for (const Pen &pen : pens_) {
    const std::size_t index = static_cast<std::size_t>(sampleIndex);
    if (index >= pen.samples.size() || index >= pen.sampleTimestampsMs.size()
        || !std::isfinite(pen.samples[index])) {
      continue;
    }
    latestTimestampMs = std::max(latestTimestampMs,
        pen.sampleTimestampsMs[index]);
  }
  return latestTimestampMs;
}

double StripChartElement::sampleIntervalSeconds() const
{
  return sampleIntervalMs_ / 1000.0;
}

bool StripChartElement::penHasData(int index) const
{
  if (index < 0 || index >= penCount()) {
    return false;
  }
  const Pen &pen = pens_[index];
  return !pen.channel.isEmpty() && pen.runtimeConnected && !pen.samples.empty();
}

void StripChartElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  if (executeMode_) {
    QColor fallbackColor;
    if (!hasDefinedPens() || !allDefinedPensConnected()) {
      fallbackColor = Qt::white;
    } else if (anyDefinedPenWithoutReadAccess()) {
      fallbackColor = Qt::black;
    }
    if (fallbackColor.isValid()) {
      painter.fillRect(rect(), fallbackColor);
      if (selected_) {
        paintSelectionOverlay(painter);
      }
      return;
    }
  }

  const QFont labelsFont = labelFont();
  painter.setFont(labelsFont);
  const QFontMetrics metrics(labelsFont);

  // Use cached static content when in execute mode for better performance
  if (executeMode_) {
    ensureStaticCache(labelsFont, metrics);
    if (!staticCache_.isNull()) {
      painter.drawPixmap(0, 0, staticCache_);
      // Draw pen data using circular buffer cache for performance
      if (cachedLayout_.chartRect.width() > 2 && cachedLayout_.chartRect.height() > 2) {
        const QRect plotArea = cachedLayout_.chartRect.adjusted(1, 1, -1, -1);
        ensurePenCache(plotArea);
        if (!penCache_.isNull()) {
          const int cacheWidth = penCache_.width();
          const int cacheHeight = penCache_.height();
          if (sampleHistoryLength_ >= cacheWidth && circularWriteSlot_ > 0
              && circularWriteSlot_ < cacheWidth) {
            const auto segmentWidths =
                circularDisplayWidths(cacheWidth, circularWriteSlot_);
            const int rightWidth = segmentWidths[0];
            const int leftWidth = segmentWidths[1];
            painter.drawPixmap(
                QRect(plotArea.left(), plotArea.top(), rightWidth, cacheHeight),
                penCache_,
                QRect(circularWriteSlot_, 0, rightWidth, cacheHeight));
            painter.drawPixmap(
                QRect(plotArea.left() + rightWidth, plotArea.top(),
                    leftWidth, cacheHeight),
                penCache_,
                QRect(0, 0, leftWidth, cacheHeight));
          } else {
            painter.drawPixmap(plotArea.topLeft(), penCache_);
          }
        }
      }
    } else {
      // Fallback if cache creation failed
      const Layout layout = calculateLayout(metrics);
      paintStaticContent(painter, layout, metrics);
      if (layout.chartRect.width() > 2 && layout.chartRect.height() > 2) {
        const QRect plotArea = layout.chartRect.adjusted(1, 1, -1, -1);
        paintPens(painter, plotArea);
      }
    }
  } else {
    // Design mode: draw everything directly (no caching needed)
    paintFrame(painter);

    const Layout layout = calculateLayout(metrics);

    if (layout.innerRect.isValid() && !layout.innerRect.isEmpty()) {
      painter.fillRect(layout.innerRect, effectiveBackground());
    }

    if (layout.chartRect.width() > 0 && layout.chartRect.height() > 0) {
      painter.fillRect(layout.chartRect, effectiveBackground());
      paintTickMarks(painter, layout, metrics);
      paintAxisScales(painter, layout, metrics);
      if (layout.chartRect.width() > 2 && layout.chartRect.height() > 2) {
        paintGrid(painter, layout.chartRect);
        const QRect plotArea = layout.chartRect.adjusted(1, 1, -1, -1);
        paintPens(painter, plotArea);
      }
    }

    paintLabels(painter, layout);
  }

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

void StripChartElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  invalidateStaticCache();
  invalidatePenCache();
  const QRect chart = chartRect();
  if (chart.width() > 0) {
    updateSamplingGeometry(chart.width());
  } else {
    cachedChartWidth_ = 0;
  }
}

void StripChartElement::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  invalidateStaticCache();
  invalidatePenCache();
}

/* Layout from MEDM's stripChartConfig() in medmStripChart.c: margins sized
 * by the y-axis labels, a title row, room below for the x-axis numbers and
 * unit label, and a second pass that squeezes a small graph by dropping
 * rows for absent title and axis labels. */
StripChartElement::Layout StripChartElement::calculateLayout(
    const QFontMetrics &metrics) const
{
  Layout layout;
  /* Inside the frame's shadow. */
  layout.innerRect = rect().adjusted(kShadowThickness - 1,
      kShadowThickness - 1, 1 - kShadowThickness, 1 - kShadowThickness);
  layout.titleText = title_.trimmed();
  layout.yLabelText = yLabel_.trimmed();

  const int w = width();
  const int h = height();
  const int shadow = kShadowThickness;
  layout.margin = calculateMargin(w, h);
  layout.markerHeight = calculateMarkerHeight(w, h);
  layout.axisFontHeight = metrics.ascent() + metrics.descent();
  const int margin = layout.margin;
  const int markerHeight = layout.markerHeight;
  const int fontHeight = layout.axisFontHeight;

  const std::vector<YAxisRange> ranges = yAxisRanges(false);
  layout.numYAxisLabel = static_cast<int>(ranges.size());
  int maxWidth = 0;
  int maxDot = 0;
  for (const YAxisRange &range : ranges) {
    maxWidth = std::max(maxWidth, metrics.horizontalAdvance(
        QStringLiteral("-8.8888888888").left(range.width)));
    maxDot = std::max(maxDot, range.numDot);
  }
  layout.yAxisLabelTextWidth = maxWidth;
  const int yAxisLabelWidth = maxWidth + maxDot * kAxisLineSpace;

  /* Use the width of the y-axis labels to set all margins. */
  int dataX0 = shadow + yAxisLabelWidth + margin * 2 + markerHeight + 1;
  int dataY0 = dataX0;
  int dataX1 = w - dataX0 - 1;
  int dataY1 = h - dataY0 - 1;

  /* Set the top margin, shrinking the right and bottom if applicable. */
  const QFontMetrics titleMetrics(titleFont());
  const int titleFontHeight = titleMetrics.ascent() + titleMetrics.descent();
  dataY0 = shadow + titleFontHeight + fontHeight + margin * 3 + 1;
  if (dataY0 < dataX0) {
    dataX1 = w - dataY0 - 1;
    dataY1 = h - dataY0;
  }

  /* The bottom margin expands if necessary. */
  const int bottomHeight = shadow + fontHeight * 2 + margin * 3
      + markerHeight + 2;
  if (h - dataY1 - 1 < bottomHeight) {
    dataY1 = h - bottomHeight - 1;
  }
  if (w - dataX1 - 1 < bottomHeight) {
    dataX1 = w - bottomHeight - 1;
  }

  /* The stacked y-axis labels must fit beside the graph. */
  const int heightExt = (fontHeight * layout.numYAxisLabel + 1) / 2;
  if (heightExt + shadow > dataY0) {
    dataY0 = heightExt + shadow;
  }
  if (heightExt + shadow > h - dataY1) {
    dataY1 = h - heightExt - shadow - 1;
  }

  /* Keep the x-axis and y-axis labels from overlapping. */
  const NumberFormat xFormat = calculateNumberFormat(period_);
  const int widthExt = (metrics.horizontalAdvance(
      QStringLiteral("-0.0000000").left(xFormat.width + 1)) + 1) / 2;
  if (widthExt > margin + markerHeight
      && bottomHeight > margin + markerHeight) {
    layout.yAxisLabelOffset = widthExt - margin - markerHeight;
    dataX0 += layout.yAxisLabelOffset;
  }
  if (widthExt + shadow > w - dataX1 - 1) {
    dataX1 = w - widthExt - shadow - 1;
  }

  /* Second pass: a small graph gets the rows of absent labels. */
  bool dropXLabel = false;
  if (dataY1 - dataY0 + 1 < h / 2) {
    layout.squeezed = true;
    if (layout.titleText.isEmpty()) {
      dataY0 -= titleFontHeight;
      if (w - dataX1 - 1 > dataY0) {
        dataX1 = w - dataY0 - 1;
      }
    }
    if (xLabel_.trimmed().isEmpty()) {
      dropXLabel = true;
      dataY1 += fontHeight;
      if (w - dataX1 - 1 > h - dataY1 - 1) {
        dataX1 = w - (h - dataY1 - 1) - 1;
      }
    }
    if (layout.yLabelText.isEmpty()) {
      dataY0 -= fontHeight;
      if (w - dataX1 - 1 > dataY0) {
        dataX1 = w - dataY0 - 1;
      }
    }
    if (heightExt + shadow > dataY0) {
      dataY0 = heightExt + shadow;
    }
    if (heightExt + shadow > h - dataY1) {
      dataY1 = h - heightExt - shadow - 1;
    }
    if (widthExt + shadow > w - dataX1 - 1) {
      dataX1 = w - widthExt - shadow - 1;
    }
  }
  if (dataX1 <= dataX0) {
    dataX1 = dataX0 + 1;
  }
  if (dataY1 <= dataY0) {
    dataY1 = dataY0 + 1;
  }
  const int dataWidth = dataX1 - dataX0 + 1;
  const int dataHeight = dataY1 - dataY0 + 1;

  /* The data area is inside the one-pixel graph border. */
  layout.chartRect = QRect(dataX0 - 1, dataY0 - 1, dataWidth + 2,
      dataHeight + 2);

  if (!dropXLabel) {
    layout.xLabelText = xLabel_.trimmed();
    if (layout.xLabelText.isEmpty()) {
      switch (units_) {
        case TimeUnits::kMilliseconds:
          layout.xLabelText = QStringLiteral("time (ms)");
          break;
        case TimeUnits::kMinutes:
          layout.xLabelText = QStringLiteral("time (min)");
          break;
        case TimeUnits::kSeconds:
        default:
          layout.xLabelText = QStringLiteral("time (sec)");
          break;
      }
    }
  }
  layout.titlePos = QPoint(
      (w - titleMetrics.horizontalAdvance(layout.titleText)) / 2,
      shadow + margin + titleMetrics.ascent());
  layout.yLabelPos = QPoint(dataX0,
      shadow + margin * 2 + titleFontHeight + metrics.ascent());
  layout.xLabelPos = QPoint(
      dataX0 + (dataWidth - metrics.horizontalAdvance(layout.xLabelText)) / 2,
      dataY1 + 1 + fontHeight + margin * 2 + markerHeight + metrics.ascent()
          + 1);
  return layout;
}

/* The distinct pen ranges, each labeled on the y axis, as in MEDM's
 * calcYAxisLabelWidth(). Without pens a 0 to 100 range is shown. */
std::vector<StripChartElement::YAxisRange> StripChartElement::yAxisRanges(
    bool zoomed) const
{
  std::vector<YAxisRange> ranges;
  for (int p = 0; p < penCount(); ++p) {
    if (pens_[p].channel.trimmed().isEmpty()) {
      continue;
    }
    const double low = zoomed ? zoomedPenLow(p) : effectivePenLow(p);
    const double high = zoomed ? zoomedPenHigh(p) : effectivePenHigh(p);
    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }
    bool found = false;
    for (YAxisRange &range : ranges) {
      if (range.high == high && range.low == low) {
        range.penMask |= (1 << p);
        ++range.numDot;
        found = true;
        break;
      }
    }
    if (!found) {
      ranges.push_back(makeYAxisRange(low, high, 1 << p));
    }
  }
  if (ranges.empty()) {
    double low = 0.0;
    double high = 100.0;
    if (zoomed) {
      applyZoomToRange(low, high);
    }
    YAxisRange range = makeYAxisRange(low, high, 0);
    range.numDot = 0;
    ranges.push_back(range);
  }
  if (ranges.size() == 1) {
    ranges.front().numDot = 0;
  }
  return ranges;
}

StripChartElement::YAxisRange StripChartElement::makeYAxisRange(double low,
    double high, int penMask)
{
  YAxisRange range;
  range.low = low;
  range.high = high;
  range.penMask = penMask;
  range.numDot = 1;
  const NumberFormat hiFormat = calculateNumberFormat(high);
  const NumberFormat loFormat = calculateNumberFormat(low);
  range.format = (hiFormat.format == 'e' || loFormat.format == 'e')
      ? 'e' : 'f';
  range.decimal = std::max(hiFormat.decimal, loFormat.decimal);
  range.width = std::max(hiFormat.width + (high < 0.0 ? 1 : 0),
      loFormat.width + (low < 0.0 ? 1 : 0));
  return range;
}

/* MEDM's division counts: as many as the labels allow, at most 10, and
 * never 9 or 7. */
int StripChartElement::limitDivisions(int nDiv)
{
  if (nDiv > 10) {
    nDiv = 10;
  } else if (nDiv == 9) {
    nDiv = 8;
  } else if (nDiv == 7) {
    nDiv = 6;
  }
  return std::max(nDiv, 1);
}

int StripChartElement::xAxisDivisions(int dataWidth,
    const QFontMetrics &metrics) const
{
  const NumberFormat format = calculateNumberFormat(period_);
  const int textWidth = std::max(1, metrics.horizontalAdvance(
      QStringLiteral("-0.000000").left(format.width + 1)));
  return limitDivisions((dataWidth - 1) / textWidth);
}

int StripChartElement::yAxisDivisions(const Layout &layout,
    int dataHeight) const
{
  const double verticalSpacing = layout.squeezed ? 1.0 : 2.0;
  const int labelHeight = std::max(1, static_cast<int>(
      (layout.numYAxisLabel + verticalSpacing) * layout.axisFontHeight));
  return limitDivisions((dataHeight - 1) / labelHeight);
}

QColor StripChartElement::effectiveForeground() const
{
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor StripChartElement::effectiveBackground() const
{
  /* Match MEDM execute-mode fallback states before drawing any chart content. */
  if (executeMode_) {
    if (!hasDefinedPens() || !allDefinedPensConnected()) {
      return QColor(Qt::white);
    }
    if (anyDefinedPenWithoutReadAccess()) {
      return QColor(Qt::black);
    }
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

QColor StripChartElement::effectivePenColor(int index) const
{
  if (index < 0 || index >= penCount()) {
    return QColor();
  }
  if (pens_[index].color.isValid()) {
    return pens_[index].color;
  }
  return defaultPenColor(index);
}

QRect StripChartElement::chartRect() const
{
  const QFont labelsFont = labelFont();
  const QFontMetrics metrics(labelsFont);
  const Layout layout = calculateLayout(metrics);
  return layout.chartRect;
}

/* MEDM's calcLabelFontSize() and calcTitleFontSize() pick the largest
 * font whose ascent plus descent fits the height for the chart size. */
QFont StripChartElement::labelFont() const
{
  const QFont font = medmMessageButtonFont(
      calculateLabelFontSize(width(), height()));
  return font.family().isEmpty() ? this->font() : font;
}

QFont StripChartElement::titleFont() const
{
  const QFont font = medmMessageButtonFont(
      calculateTitleFontSize(width(), height()));
  return font.family().isEmpty() ? this->font() : font;
}

/* MEDM draws the strip chart's Motif drawing-area shadow, two pixels deep
 * (shadowThickness - 1), in the shadow colors of the background. */
void StripChartElement::paintFrame(QPainter &painter) const
{
  const QColor bgColor = effectiveBackground();
  painter.fillRect(rect(), bgColor);
  QColor topShadow;
  QColor bottomShadow;
  MedmColors::computeShadowColors(bgColor, topShadow, bottomShadow);
  const int right = width() - 1;
  const int bottom = height() - 1;
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  for (int j = 0; j < kShadowThickness - 1; ++j) {
    painter.setPen(topShadow);
    painter.drawLine(j, j, right - j, j);
    painter.drawLine(j, j, j, bottom - j);
    painter.setPen(bottomShadow);
    painter.drawLine(j, bottom - j, right - j, bottom - j);
    painter.drawLine(right - j, j, right - j, bottom - j);
  }
  painter.restore();
}

/* The graph border: XDrawRectangle around the data area. */
void StripChartElement::paintGrid(QPainter &painter, const QRect &content) const
{
  if (content.width() <= 0 || content.height() <= 0) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(effectiveForeground(), 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(content.adjusted(0, 0, -1, -1));
  painter.restore();
}

void StripChartElement::paintTickMarks(QPainter &painter,
    const Layout &layout, const QFontMetrics &metrics) const
{
  const QRect data = layout.chartRect.adjusted(1, 1, -1, -1);
  if (data.width() <= 0 || data.height() <= 0 || layout.markerHeight <= 0) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(effectiveForeground(), 1));

  const int nDivY = yAxisDivisions(layout, data.height());
  const double interval = static_cast<double>(data.height() - 1) / nDivY;
  double nextTick = 0.0;
  for (int i = 0; i <= nDivY; ++i) {
    const int tickY = data.top() + static_cast<int>(nextTick);
    painter.drawLine(data.left() - 2 - (layout.markerHeight - 1), tickY,
        data.left() - 2, tickY);
    nextTick += interval;
  }

  const int nDivX = xAxisDivisions(data.width(), metrics);
  for (int i = 0; i <= nDivX; ++i) {
    const int tickX = data.right() - static_cast<int>(
        static_cast<double>(data.width() - 1) * i / nDivX);
    painter.drawLine(tickX, data.bottom() + 2, tickX,
        data.bottom() + 2 + layout.markerHeight);
  }

  painter.restore();
}

/* The axis numbers where MEDM's stripChartConfig() draws them: the x axis
 * counts back from 0 under its ticks, and each distinct pen range is
 * stacked at every y tick, with pen color bars when there are several. */
void StripChartElement::paintAxisScales(QPainter &painter,
    const Layout &layout, const QFontMetrics &metrics) const
{
  const QRect data = layout.chartRect.adjusted(1, 1, -1, -1);
  if (data.width() <= 0 || data.height() <= 0) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(effectiveForeground(), 1));

  {
    const NumberFormat format = calculateNumberFormat(period_);
    const int nDivX = xAxisDivisions(data.width(), metrics);
    const double step = period_ / nDivX;
    const int textY = data.bottom() + 1 + layout.axisFontHeight
        + layout.margin + layout.markerHeight + 1;
    double value = 0.0;
    for (int i = 0; i <= nDivX; ++i) {
      const double nextTick = static_cast<double>(data.width() - 1) * i
          / nDivX;
      const QString text = formatNumber(value, format.format, format.decimal);
      const int textWidth = metrics.horizontalAdvance(text);
      painter.drawText(data.right()
              - static_cast<int>(nextTick + textWidth / 2.0),
          textY, text);
      value -= step;
    }
  }

  {
    std::vector<YAxisRange> ranges = yAxisRanges(true);
    const int nDivY = yAxisDivisions(layout, data.height());
    const double verticalSpacing = layout.squeezed ? 1.0 : 2.0;
    const int labelHeight = static_cast<int>(
        (layout.numYAxisLabel + verticalSpacing) * layout.axisFontHeight);
    const double interval = static_cast<double>(data.height() - 1) / nDivY;
    std::vector<double> steps;
    std::vector<double> values;
    for (const YAxisRange &range : ranges) {
      steps.push_back((range.high - range.low) / nDivY);
      values.push_back(range.high);
    }
    double nextTick = 0.0;
    for (int i = 0; i <= nDivY; ++i) {
      for (std::size_t j = 0; j < ranges.size(); ++j) {
        const YAxisRange &range = ranges[j];
        if (i == nDivY) {
          values[j] = range.low;
        }
        const QString text = range.format == 'e'
            ? QString::asprintf("%.1e", values[j])
            : QString::asprintf("%.*f", range.decimal, values[j]);
        const int textWidth = metrics.horizontalAdvance(text);
        const int textX = data.left() - 1 - layout.markerHeight
            - layout.yAxisLabelOffset - layout.margin - textWidth;
        const double yOffset = nextTick - labelHeight / 2.0
            + (j + verticalSpacing / 2.0) * layout.axisFontHeight
            + metrics.ascent();
        const int textY = data.top() + static_cast<int>(yOffset);
        painter.setPen(effectiveForeground());
        painter.drawText(textX, textY, text);
        values[j] -= steps[j];

        if (range.numDot > 0) {
          const int barX = data.left() - layout.yAxisLabelTextWidth
              - layout.markerHeight - layout.margin - layout.yAxisLabelOffset;
          int count = 0;
          for (int k = penCount() - 1; k >= 0; --k) {
            if (range.penMask & (1 << k)) {
              painter.fillRect(barX - (count + 1) * kAxisLineSpace,
                  textY - metrics.ascent(), 2, metrics.ascent(),
                  effectivePenColor(k));
              ++count;
            }
          }
        }
      }
      nextTick += interval;
    }
  }

  painter.restore();
}

void StripChartElement::paintPens(QPainter &painter, const QRect &content) const
{
  if (content.width() <= 0 || content.height() <= 0) {
    return;
  }
  if (!executeMode_) {
    paintDesignPens(painter, content);
  } else {
    paintRuntimePens(painter, content);
  }
}

void StripChartElement::paintDesignPens(QPainter &painter, const QRect &content) const
{
  for (int i = 0; i < penCount(); ++i) {
    const QString channelName = pens_[i].channel.trimmed();
    if (channelName.isEmpty() && i > 0) {
      continue;
    }
    QPen pen(effectivePenColor(i));
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    QPainterPath path;
    const int samples = static_cast<int>(kPenSampleCount);
    for (int s = 0; s <= samples; ++s) {
      const double t = static_cast<double>(s) / samples;
      const double phase = static_cast<double>(i) * 0.6;
      const double value = 0.5 + 0.4 * std::sin((t * 6.28318) + phase);
      const double yValue = content.bottom() - value * content.height();
      const double xValue = content.left() + t * content.width();
      if (s == 0) {
        path.moveTo(xValue, yValue);
      } else {
        path.lineTo(xValue, yValue);
      }
    }
    painter.drawPath(path);
  }
}

void StripChartElement::paintRuntimePens(QPainter &painter, const QRect &content) const
{
  const double width = static_cast<double>(content.width());
  const double height = static_cast<double>(content.height());
  if (width <= 0.0 || height <= 0.0) {
    return;
  }

  // Use content width as capacity - this ensures samples are drawn within bounds
  const int capacity = std::max(static_cast<int>(width), 1);

  for (int i = 0; i < penCount(); ++i) {
    const Pen &pen = pens_[i];
    if (pen.samples.empty()) {
      continue;
    }

    // Use zoomed range for rendering when zoomed
    const double low = zoomedPenLow(i);
    const double high = zoomedPenHigh(i);
    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }
    const double range = drawRange(low, high);
    QPen penColor(effectivePenColor(i));
    penColor.setWidth(1);
    painter.setPen(penColor);
    painter.setBrush(Qt::NoBrush);

    const int sampleCount = static_cast<int>(pen.samples.size());
    // Only draw the most recent 'capacity' samples to fit the content width
    const int startSample = std::max(0, sampleCount - capacity);
    const int samplesToRender = sampleCount - startSample;
    
    // Position data on the RIGHT side of the chart (newest at right edge)
    // offsetColumns shifts data to the right when buffer isn't full
    const int offsetColumns = capacity - samplesToRender;
    const int denominator = std::max(capacity - 1, 1);
    QPointF previousPoint;
    bool havePreviousPoint = false;

    for (int s = startSample; s < sampleCount; ++s) {
      const double sampleValue = pen.samples[static_cast<std::size_t>(s)];
      if (!std::isfinite(sampleValue)) {
        continue;
      }
      double sampleLow = sampleValue;
      double sampleHigh = sampleValue;
      if (static_cast<std::size_t>(s) < pen.minSamples.size()
          && std::isfinite(pen.minSamples[static_cast<std::size_t>(s)])) {
        sampleLow = pen.minSamples[static_cast<std::size_t>(s)];
      }
      if (static_cast<std::size_t>(s) < pen.maxSamples.size()
          && std::isfinite(pen.maxSamples[static_cast<std::size_t>(s)])) {
        sampleHigh = pen.maxSamples[static_cast<std::size_t>(s)];
      }
      if (sampleLow > sampleHigh) {
        std::swap(sampleLow, sampleHigh);
      }
      // When zoomed, allow values outside the visible range to be drawn
      // (they will extend beyond the chart area but get clipped by Qt)
      const double normalizedLow = (sampleLow - low) / range;
      const double normalizedHigh = (sampleHigh - low) / range;
      const int renderIndex = s - startSample;
      const double x = content.left()
          + (static_cast<double>(offsetColumns + renderIndex) / denominator) * (width - 1.0);
      const double yLow = content.top() + (height - 1.0) * (1.0 - normalizedLow);
      const double yHigh = content.top() + (height - 1.0) * (1.0 - normalizedHigh);
      const double normalizedValue = (sampleValue - low) / range;
      const double yValue = content.top()
          + (height - 1.0) * (1.0 - normalizedValue);
      const QPointF samplePoint(x, yValue);

      if (archivePlot_) {
        if (havePreviousPoint) {
          painter.drawLine(previousPoint, samplePoint);
        } else {
          painter.drawPoint(samplePoint);
        }
      } else if (std::abs(yLow - yHigh) < 0.5) {
        painter.drawPoint(samplePoint);
      }
      if (std::abs(yLow - yHigh) >= 0.5) {
        painter.drawLine(QPointF(x, yLow), QPointF(x, yHigh));
      }
      previousPoint = samplePoint;
      havePreviousPoint = true;
    }
  }
}

void StripChartElement::paintLabels(QPainter &painter,
    const Layout &layout) const
{
  painter.save();
  painter.setPen(effectiveForeground());
  if (!layout.titleText.isEmpty()) {
    painter.setFont(titleFont());
    painter.drawText(layout.titlePos, layout.titleText);
  }
  painter.setFont(labelFont());
  if (!layout.yLabelText.isEmpty()) {
    painter.drawText(layout.yLabelPos, layout.yLabelText);
  }
  if (!layout.xLabelText.isEmpty()) {
    painter.drawText(layout.xLabelPos, layout.xLabelText);
  }
  painter.restore();
}

void StripChartElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

double StripChartElement::periodMilliseconds() const
{
  switch (units_) {
  case TimeUnits::kMilliseconds:
    return period_;
  case TimeUnits::kSeconds:
    return period_ * 1000.0;
  case TimeUnits::kMinutes:
    return period_ * 60000.0;
  }
  return period_ * 1000.0;
}

double StripChartElement::effectivePenLow(int index) const
{
  if (index < 0 || index >= penCount()) {
    return 0.0;
  }
  // When autoscale is enabled in execute mode, use computed limits for all pens
  if (autoscale_ && executeMode_) {
    computeAutoscaleLimits();
    return autoscaleLow_;
  }
  const Pen &pen = pens_[index];
  if (pen.limits.lowSource == PvLimitSource::kChannel && pen.runtimeLimitsValid) {
    return pen.runtimeLow;
  }
  return pen.limits.lowDefault;
}

double StripChartElement::effectivePenHigh(int index) const
{
  if (index < 0 || index >= penCount()) {
    return 1.0;
  }
  // When autoscale is enabled in execute mode, use computed limits for all pens
  if (autoscale_ && executeMode_) {
    computeAutoscaleLimits();
    return autoscaleHigh_;
  }
  const Pen &pen = pens_[index];
  if (pen.limits.highSource == PvLimitSource::kChannel && pen.runtimeLimitsValid) {
    return pen.runtimeHigh;
  }
  return pen.limits.highDefault;
}

void StripChartElement::applyZoomToRange(double &low, double &high) const
{
  if (!zoomed_) {
    return;
  }
  
  const double range = high - low;
  // The visible portion is centered at zoomYCenter_ and spans zoomYFactor_ of the full range
  // zoomYCenter_ = 0.5 means center is at the middle of the original range
  // zoomYFactor_ = 1.0 means we see the full range, 0.5 means we see half
  const double visibleRange = range * zoomYFactor_;
  const double center = low + zoomYCenter_ * range;
  low = center - visibleRange / 2.0;
  high = center + visibleRange / 2.0;
}

double StripChartElement::zoomedPenLow(int index) const
{
  double low = effectivePenLow(index);
  double high = effectivePenHigh(index);
  applyZoomToRange(low, high);
  return low;
}

double StripChartElement::zoomedPenHigh(int index) const
{
  double low = effectivePenLow(index);
  double high = effectivePenHigh(index);
  applyZoomToRange(low, high);
  return high;
}

void StripChartElement::ensureRefreshTimer()
{
  if (refreshTimer_) {
    return;
  }
  refreshTimer_ = new QTimer(this);
  refreshTimer_->setTimerType(Qt::CoarseTimer);
  // Initial interval; will be adjusted dynamically based on network performance
  refreshTimer_->setInterval(currentRefreshIntervalMs_);
  QObject::connect(refreshTimer_, &QTimer::timeout, this,
      &StripChartElement::handleRefreshTimer);
}

void StripChartElement::updateRefreshTimer()
{
  const bool needTimer = executeMode_ && allDefinedPensConnectedAndReadable();
  if (needTimer) {
    ensureRefreshTimer();
    if (refreshTimer_ && !refreshTimer_->isActive()) {
      refreshTimer_->start();
    }
  } else if (refreshTimer_) {
    refreshTimer_->stop();
  }
}

void StripChartElement::handleRefreshTimer()
{
  if (!executeMode_) {
    if (refreshTimer_) {
      refreshTimer_->stop();
    }
    return;
  }

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

  // Track late refreshes for adaptive refresh rate adjustment
  // This helps smooth performance over slow/variable network connections
  if (expectedRefreshTimeMs_ > 0) {
    const qint64 deltaMs = nowMs - expectedRefreshTimeMs_;
    if (deltaMs > kLateThresholdMs) {
      ++lateRefreshCount_;
      onTimeRefreshCount_ = 0;  // Reset on-time counter when late
      if (lateRefreshCount_ > kLateCountThreshold) {
        // Increase refresh interval to reduce load
        currentRefreshIntervalMs_ += kIntervalIncrementMs;
        if (currentRefreshIntervalMs_ > kMaxRefreshIntervalMs) {
          // Reset to default if we've slowed down too much
          currentRefreshIntervalMs_ = kDefaultRefreshIntervalMs;
        }
        lateRefreshCount_ = 0;
      }
    } else {
      // Refresh was on time - track consecutive on-time refreshes
      lateRefreshCount_ = 0;
      if (currentRefreshIntervalMs_ > kDefaultRefreshIntervalMs) {
        ++onTimeRefreshCount_;
        if (onTimeRefreshCount_ >= kOnTimeCountThreshold) {
          // Network has been stable for a while, try speeding up
          currentRefreshIntervalMs_ -= kIntervalIncrementMs / 2;
          if (currentRefreshIntervalMs_ < kDefaultRefreshIntervalMs) {
            currentRefreshIntervalMs_ = kDefaultRefreshIntervalMs;
          }
          onTimeRefreshCount_ = 0;
        }
      } else {
        onTimeRefreshCount_ = 0;
      }
    }
  }
  expectedRefreshTimeMs_ = nowMs + currentRefreshIntervalMs_;

  const int appendedColumns = maybeAppendSamples(nowMs);

  // Use update() to queue a paint event. This doesn't block the timer
  // callback, allowing the event loop to process other events. Multiple
  // update() calls will be coalesced into a single paint, and the paint
  // will draw all accumulated columns at once.
  if (appendedColumns > 0) {
    update();
  }

  // Always use the adaptive refresh interval for the timer
  // This ensures consistent timing that can be adjusted for slow networks
  if (refreshTimer_) {
    refreshTimer_->setInterval(currentRefreshIntervalMs_);
  }
}

void StripChartElement::updateSamplingGeometry(int chartWidth)
{
  if (chartWidth <= 0) {
    cachedChartWidth_ = 0;
    sampleHistoryLength_ = 0;
    return;
  }

  const int width = std::max(chartWidth, 1);
  if (cachedChartWidth_ == width) {
    return;
  }

  cachedChartWidth_ = width;

  const double totalMs = periodMilliseconds();
  double interval = (width > 0) ? (totalMs / static_cast<double>(width)) : totalMs;
  if (!std::isfinite(interval) || interval <= 0.0) {
    interval = 1000.0;
  }
  sampleIntervalMs_ = std::max(interval, 10.0);

  enforceSampleCapacity(width);
}

void StripChartElement::enforceSampleCapacity(int capacity)
{
  if (capacity <= 0) {
    sampleHistoryLength_ = 0;
    for (Pen &pen : pens_) {
      pen.samples.clear();
      pen.minSamples.clear();
      pen.maxSamples.clear();
      pen.sampleTimestampsMs.clear();
    }
    return;
  }

  int newLength = 0;
  for (Pen &pen : pens_) {
    while (static_cast<int>(pen.samples.size()) > capacity) {
      pen.samples.pop_front();
    }
    while (static_cast<int>(pen.minSamples.size()) > capacity) {
      pen.minSamples.pop_front();
    }
    while (static_cast<int>(pen.maxSamples.size()) > capacity) {
      pen.maxSamples.pop_front();
    }
    while (static_cast<int>(pen.sampleTimestampsMs.size()) > capacity) {
      pen.sampleTimestampsMs.pop_front();
    }
    newLength = std::max(newLength, static_cast<int>(pen.samples.size()));
  }
  sampleHistoryLength_ = newLength;
}

int StripChartElement::maybeAppendSamples(qint64 nowMs)
{
  if (!allDefinedPensConnectedAndReadable()) {
    lastSampleMs_ = nowMs;
    nextAdvanceTimeMs_ = 0;
    return 0;
  }

  if (!anyPenReady()) {
    lastSampleMs_ = nowMs;
    nextAdvanceTimeMs_ = 0;
    return 0;
  }

  /* The chart is as wide as its Y labels leave it, and they widen or
   * narrow when a channel's limits arrive or the autoscaled range changes.
   * Keep one column per period / width of the chart being drawn, or the
   * time axis labels the wrong span. */
  const int width = chartRect().width();
  if (width <= 0) {
    lastSampleMs_ = nowMs;
    nextAdvanceTimeMs_ = 0;
    return 0;
  }
  if (width != cachedChartWidth_) {
    updateSamplingGeometry(width);
  }

  if (!std::isfinite(sampleIntervalMs_) || sampleIntervalMs_ <= 0.0) {
    const int width = std::max(cachedChartWidth_, 1);
    double interval = periodMilliseconds() / static_cast<double>(width);
    if (!std::isfinite(interval) || interval <= 0.0) {
      interval = 1000.0;
    }
    sampleIntervalMs_ = std::max(interval, 10.0);
  }

  // Initialize nextAdvanceTimeMs_ on first sample (MEDM-style)
  if (nextAdvanceTimeMs_ == 0) {
    appendSampleColumn();
    lastSampleMs_ = nowMs;
    nextAdvanceTimeMs_ = saturatingAdd(nowMs,
        roundedPositiveInterval(sampleIntervalMs_));
    return 1;
  }

  // MEDM-style: check if current time exceeds nextAdvanceTime
  if (nowMs < nextAdvanceTimeMs_) {
    return 0;
  }

  // Calculate how many pixels (columns) need to be drawn
  const double interval = sampleIntervalMs_;
  int totalPixels = 1 + static_cast<int>((nowMs - nextAdvanceTimeMs_) / interval);
  totalPixels = std::clamp(totalPixels, 1, kMaxSampleBurst);

  for (int i = 0; i < totalPixels; ++i) {
    appendSampleColumn();
  }

  // Advance nextAdvanceTimeMs_ by the interval * totalPixels (MEDM-style)
  nextAdvanceTimeMs_ = saturatingAdd(nextAdvanceTimeMs_,
      roundedPositiveInterval(interval * totalPixels));
  lastSampleMs_ = nowMs;
  return totalPixels;
}

void StripChartElement::appendSampleColumn()
{
  const int capacity = std::max(cachedChartWidth_, 1);
  if (capacity <= 0) {
    return;
  }

  const bool bufferWasFull = sampleHistoryLength_ >= capacity;
  const double missingValue = std::numeric_limits<double>::quiet_NaN();
  for (Pen &pen : pens_) {
    double sampleValue = missingValue;
    double sampleLow = missingValue;
    double sampleHigh = missingValue;
    if (pen.runtimeConnected && pen.hasRuntimeValue) {
      sampleValue = pen.runtimeValue;
      if (pen.hasIntervalRange) {
        sampleLow = pen.intervalLow;
        sampleHigh = pen.intervalHigh;
      } else {
        sampleLow = sampleValue;
        sampleHigh = sampleValue;
      }
    }
    pen.samples.push_back(sampleValue);
    pen.minSamples.push_back(sampleLow);
    pen.maxSamples.push_back(sampleHigh);
    pen.sampleTimestampsMs.push_back(
        pen.hasRuntimeTimestamp ? pen.runtimeTimestampMs : 0);
    if (static_cast<int>(pen.samples.size()) > capacity) {
      pen.samples.pop_front();
    }
    if (static_cast<int>(pen.minSamples.size()) > capacity) {
      pen.minSamples.pop_front();
    }
    if (static_cast<int>(pen.maxSamples.size()) > capacity) {
      pen.maxSamples.pop_front();
    }
    if (static_cast<int>(pen.sampleTimestampsMs.size()) > capacity) {
      pen.sampleTimestampsMs.pop_front();
    }
    if (pen.runtimeConnected && pen.hasRuntimeValue) {
      pen.intervalLow = pen.runtimeValue;
      pen.intervalHigh = pen.runtimeValue;
      pen.hasIntervalRange = true;
    } else {
      pen.hasIntervalRange = false;
    }
  }

  int newLength = 0;
  for (const Pen &pen : pens_) {
    newLength = std::max(newLength, static_cast<int>(pen.samples.size()));
  }
  sampleHistoryLength_ = newLength;

  // Invalidate autoscale limits when new data arrives
  if (autoscale_) {
    autoscaleLimitsValid_ = false;
    invalidateStaticCache();  // Y-axis labels may change
    penCacheDirty_ = true;
  }

  // Track new columns for incremental pen drawing
  ++newSampleColumns_;
  
  // Before the history fills the plot, samples are right-aligned and require a
  // full redraw. Afterwards the cache is a true circular image and only the
  // overwritten columns need repainting.
  if (!bufferWasFull) {
    penCacheDirty_ = true;
  }
}

bool StripChartElement::anyPenConnected() const
{
  for (const Pen &pen : pens_) {
    if (pen.runtimeConnected) {
      return true;
    }
  }
  return false;
}

bool StripChartElement::hasDefinedPens() const
{
  for (const Pen &pen : pens_) {
    if (!pen.channel.trimmed().isEmpty()) {
      return true;
    }
  }
  return false;
}

bool StripChartElement::anyPenReady() const
{
  for (const Pen &pen : pens_) {
    if (pen.runtimeConnected && pen.hasRuntimeValue) {
      return true;
    }
  }
  return false;
}

bool StripChartElement::anyDefinedPenWithoutReadAccess() const
{
  for (const Pen &pen : pens_) {
    if (!pen.channel.trimmed().isEmpty()
        && pen.runtimeConnected
        && pen.runtimeReadAccessKnown
        && !pen.runtimeReadAccess) {
      return true;
    }
  }
  return false;
}

bool StripChartElement::allDefinedPensConnected() const
{
  for (const Pen &pen : pens_) {
    if (!pen.channel.trimmed().isEmpty()) {
      if (!pen.runtimeConnected) {
        return false;
      }
    }
  }
  return hasDefinedPens();
}

bool StripChartElement::allDefinedPensConnectedAndReadable() const
{
  bool hasDefinedPen = false;
  for (const Pen &pen : pens_) {
    if (pen.channel.trimmed().isEmpty()) {
      continue;
    }
    hasDefinedPen = true;
    if (!pen.runtimeConnected) {
      return false;
    }
    if (!pen.runtimeReadAccessKnown || !pen.runtimeReadAccess) {
      return false;
    }
  }
  return hasDefinedPen;
}

void StripChartElement::invalidateStaticCache()
{
  staticCacheDirty_ = true;
}

void StripChartElement::ensureStaticCache(const QFont &labelsFont,
    const QFontMetrics &metrics)
{
  const QSize widgetSize = size();
  if (!staticCacheDirty_ && !staticCache_.isNull()
      && staticCache_.size() == widgetSize) {
    return;
  }

  if (widgetSize.isEmpty()) {
    staticCache_ = QPixmap();
    staticCacheDirty_ = true;
    return;
  }

  staticCache_ = QPixmap(widgetSize);
  staticCache_.fill(Qt::transparent);

  QPainter cachePainter(&staticCache_);
  cachePainter.setRenderHint(QPainter::Antialiasing, false);
  cachePainter.setFont(labelsFont);

  paintFrame(cachePainter);

  cachedLayout_ = calculateLayout(metrics);

  if (cachedLayout_.innerRect.isValid() && !cachedLayout_.innerRect.isEmpty()) {
    cachePainter.fillRect(cachedLayout_.innerRect, effectiveBackground());
  }

  if (cachedLayout_.chartRect.width() > 0 && cachedLayout_.chartRect.height() > 0) {
    cachePainter.fillRect(cachedLayout_.chartRect, effectiveBackground());
    paintTickMarks(cachePainter, cachedLayout_, metrics);
    paintAxisScales(cachePainter, cachedLayout_, metrics);
    if (cachedLayout_.chartRect.width() > 2 && cachedLayout_.chartRect.height() > 2) {
      paintGrid(cachePainter, cachedLayout_.chartRect);
    }
  }

  paintLabels(cachePainter, cachedLayout_);

  cachePainter.end();
  staticCacheDirty_ = false;
}

void StripChartElement::paintStaticContent(QPainter &painter,
    const Layout &layout, const QFontMetrics &metrics) const
{
  paintFrame(painter);

  if (layout.innerRect.isValid() && !layout.innerRect.isEmpty()) {
    painter.fillRect(layout.innerRect, effectiveBackground());
  }

  if (layout.chartRect.width() > 0 && layout.chartRect.height() > 0) {
    painter.fillRect(layout.chartRect, effectiveBackground());
    paintTickMarks(painter, layout, metrics);
    paintAxisScales(painter, layout, metrics);
    if (layout.chartRect.width() > 2 && layout.chartRect.height() > 2) {
      paintGrid(painter, layout.chartRect);
    }
  }

  paintLabels(painter, layout);
}

void StripChartElement::invalidatePenCache()
{
  penCacheDirty_ = true;
  newSampleColumns_ = 0;
  circularWriteSlot_ = 0;
}

void StripChartElement::ensurePenCache(const QRect &plotArea)
{
  const QSize plotSize = plotArea.size();
  if (plotSize.isEmpty()) {
    penCache_ = QPixmap();
    penCacheDirty_ = true;
    circularWriteSlot_ = 0;
    return;
  }

  const int width = plotSize.width();
  const int height = plotSize.height();

  // Check if we need a full redraw
  // Always do full redraw when zoomed to ensure correct rendering
  const bool sizeChanged = penCache_.isNull() || penCache_.size() != plotSize;
  const bool plotAreaMoved = penCachePlotArea_ != plotArea;
  const bool needsFullRedraw = penCacheDirty_ || sizeChanged || plotAreaMoved || zoomed_;

  if (needsFullRedraw) {
    // Full redraw: create cache, paint all pens, and set write slot to end
    if (sizeChanged || penCache_.isNull()) {
      penCache_ = QPixmap(plotSize);
    }
    penCache_.fill(Qt::transparent);

    QPainter cachePainter(&penCache_);
    cachePainter.setRenderHint(QPainter::Antialiasing, false);

    // Paint all existing data
    const QRect normalizedRect(0, 0, width, height);
    paintRuntimePens(cachePainter, normalizedRect);

    cachePainter.end();

    // Set write slot based on where data ends in the cache
    // When buffer not full: data is at positions (capacity - samplesToRender) to (capacity - 1)
    // When buffer full: data fills entire cache
    const int samplesToRender = std::min(sampleHistoryLength_, width);
    if (samplesToRender >= width) {
      // Buffer is full, next write wraps to 0
      circularWriteSlot_ = 0;
    } else {
      // Buffer not full, next write is at the right edge (after the last sample)
      circularWriteSlot_ = width;  // This signals "write at end, need full redraw for new data"
    }

    penCachePlotArea_ = plotArea;
    penCacheDirty_ = false;
    newSampleColumns_ = 0;
  } else if (newSampleColumns_ > 0 && sampleHistoryLength_ >= width) {
    // Incremental update: only do this when buffer is full (has wrapped)
    // When buffer is not full, we need full redraws to position data correctly
    QPainter cachePainter(&penCache_);
    cachePainter.setRenderHint(QPainter::Antialiasing, false);

    const int columnsToAdd = std::min(newSampleColumns_, width);

    // Get pen data for the newest samples
    for (int col = 0; col < columnsToAdd; ++col) {
      const int writeX = circularWriteSlot_;

      // Clear this column with transparent (will show background through it)
      cachePainter.setCompositionMode(QPainter::CompositionMode_Source);
      cachePainter.fillRect(writeX, 0, 1, height, Qt::transparent);
      cachePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

      // Draw pen data for this column
      // The sample index for this column is (sampleHistoryLength_ - columnsToAdd + col)
      const int sampleIdx = sampleHistoryLength_ - columnsToAdd + col;
      if (sampleIdx >= 0) {
        for (int i = 0; i < penCount(); ++i) {
          const Pen &pen = pens_[i];
          if (sampleIdx >= static_cast<int>(pen.samples.size())) {
            continue;
          }

          // Use zoomed range for rendering when zoomed
          const double low = zoomedPenLow(i);
          const double high = zoomedPenHigh(i);
          if (!std::isfinite(low) || !std::isfinite(high)) {
            continue;
          }
          const double range = drawRange(low, high);

          const double value = pen.samples[static_cast<std::size_t>(sampleIdx)];
          if (!std::isfinite(value)) {
            continue;
          }
          double sampleLow = value;
          double sampleHigh = value;
          if (static_cast<std::size_t>(sampleIdx) < pen.minSamples.size()
              && std::isfinite(pen.minSamples[static_cast<std::size_t>(sampleIdx)])) {
            sampleLow = pen.minSamples[static_cast<std::size_t>(sampleIdx)];
          }
          if (static_cast<std::size_t>(sampleIdx) < pen.maxSamples.size()
              && std::isfinite(pen.maxSamples[static_cast<std::size_t>(sampleIdx)])) {
            sampleHigh = pen.maxSamples[static_cast<std::size_t>(sampleIdx)];
          }
          if (sampleLow > sampleHigh) {
            std::swap(sampleLow, sampleHigh);
          }

          /* A column is a vertical line in the cache image, so bounding it
           * just outside the chart draws the same pixels and keeps the
           * conversion to int defined for values far out of range. */
          const double normalizedLow =
              std::clamp((sampleLow - low) / range, -1.0, 2.0);
          const double normalizedHigh =
              std::clamp((sampleHigh - low) / range, -1.0, 2.0);
          const int yLow = static_cast<int>((height - 1) * (1.0 - normalizedLow));
          const int yHigh = static_cast<int>((height - 1) * (1.0 - normalizedHigh));

          QPen penColor(effectivePenColor(i));
          penColor.setWidth(1);
          cachePainter.setPen(penColor);

          if (yLow != yHigh) {
            cachePainter.drawLine(writeX, std::min(yLow, yHigh), writeX,
                std::max(yLow, yHigh));
          } else {
            cachePainter.drawPoint(writeX, yLow);
          }
        }
      }

      // Advance write slot with wraparound
      circularWriteSlot_ = (circularWriteSlot_ + 1) % width;
    }

    cachePainter.end();
    newSampleColumns_ = 0;
  }
}

void StripChartElement::mousePressEvent(QMouseEvent *event)
{
  if (executeMode_) {
    // Forward middle button events to parent window for PV info functionality
    if (event->button() == Qt::MiddleButton) {
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
    // Forward left clicks to parent when PV Info picking mode is active
    if (event->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
    // Forward left clicks to parent when PV Limits picking mode is active
    if (event->button() == Qt::LeftButton && isParentWindowInPvLimitsMode(this)) {
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
    if (event->button() == Qt::LeftButton) {
      // Start panning (Y-axis only) - only when not in autoscale mode
      if (!autoscale_) {
        const QRect chart = chartRect();
        if (chart.contains(event->pos())) {
          panning_ = true;
          panStartPos_ = event->pos();
          panStartYCenter_ = zoomYCenter_;
          setCursor(Qt::ClosedHandCursor);
          event->accept();
          return;
        }
      }
    } else if (event->button() == Qt::RightButton) {
      // Forward right-click events to parent window for context menu
      // The parent window's showExecuteContextMenu will handle strip chart specific options
      if (forwardMouseEventToParent(event)) {
        return;
      }
    }
  }
  QWidget::mousePressEvent(event);
}

void StripChartElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (panning_ && event->button() == Qt::LeftButton) {
    panning_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void StripChartElement::mouseMoveEvent(QMouseEvent *event)
{
  if (panning_ && executeMode_) {
    const QRect chart = chartRect();
    if (chart.height() <= 0) {
      return;
    }
    
    const QPointF delta = event->pos() - panStartPos_;
    
    // Convert pixel delta to normalized coordinate delta for Y axis
    // Positive delta.y() means dragging down, which should increase yCenter
    // (showing lower values at center)
    const double yDelta = delta.y() / static_cast<double>(chart.height());
    zoomYCenter_ = std::clamp(panStartYCenter_ + yDelta * zoomYFactor_, 0.0, 1.0);
    
    // Invalidate caches since axis labels may change with zoom
    invalidateStaticCache();
    invalidatePenCache();
    update();
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void StripChartElement::wheelEvent(QWheelEvent *event)
{
  if (!executeMode_) {
    QWidget::wheelEvent(event);
    return;
  }
  
  // Disable wheel zoom when autoscale is enabled
  if (autoscale_) {
    QWidget::wheelEvent(event);
    return;
  }
  
  const QRect chart = chartRect();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->posF();
#endif
  
  if (!chart.contains(pos.toPoint())) {
    QWidget::wheelEvent(event);
    return;
  }
  
  // Calculate zoom factor (positive delta = zoom in)
  const double degrees = event->angleDelta().y() / 8.0;
  const double steps = degrees / 15.0;
  const double factor = std::pow(0.9, steps);  // 0.9 = zoom in, 1.11 = zoom out
  
  // Calculate mouse position in chart coordinates (0-1 range, 0 at bottom)
  const double chartY = 1.0 - (pos.y() - chart.top()) / chart.height();
  
  // Apply zoom centered on mouse position
  // The visible range is [zoomYCenter_ - zoomYFactor_/2, zoomYCenter_ + zoomYFactor_/2]
  // We want the point under the mouse to stay at the same screen position
  const double visibleMin = zoomYCenter_ - zoomYFactor_ / 2.0;
  const double mouseDataY = visibleMin + chartY * zoomYFactor_;
  
  // Apply zoom factor
  const double newFactor = std::clamp(zoomYFactor_ * factor, 0.01, 10.0);
  
  // Calculate new center to keep mouseDataY at the same screen position
  // newVisibleMin + chartY * newFactor = mouseDataY
  // newVisibleMin = mouseDataY - chartY * newFactor
  // newCenter - newFactor/2 = mouseDataY - chartY * newFactor
  // newCenter = mouseDataY - chartY * newFactor + newFactor/2
  const double newCenter = mouseDataY - chartY * newFactor + newFactor / 2.0;
  
  zoomYFactor_ = newFactor;
  zoomYCenter_ = std::clamp(newCenter, 0.0, 1.0);
  zoomed_ = (std::abs(zoomYFactor_ - 1.0) > 0.001 || std::abs(zoomYCenter_ - 0.5) > 0.001);
  
  // Invalidate caches since axis labels change with zoom
  invalidateStaticCache();
  invalidatePenCache();
  update();
  event->accept();
}

bool StripChartElement::isZoomed() const
{
  return zoomed_;
}

void StripChartElement::resetZoom()
{
  zoomed_ = false;
  zoomYFactor_ = 1.0;
  zoomYCenter_ = 0.5;
  panning_ = false;
  invalidateStaticCache();
  invalidatePenCache();
  update();
}

bool StripChartElement::isAutoscale() const
{
  return autoscale_;
}

void StripChartElement::setAutoscale(bool enable)
{
  if (autoscale_ == enable) {
    return;
  }
  autoscale_ = enable;
  autoscaleLimitsValid_ = false;
  // Reset zoom when changing autoscale mode
  if (enable) {
    zoomed_ = false;
    zoomYFactor_ = 1.0;
    zoomYCenter_ = 0.5;
  }
  invalidateStaticCache();
  invalidatePenCache();
  update();
}

void StripChartElement::computeAutoscaleLimits() const
{
  if (autoscaleLimitsValid_) {
    return;
  }

  double minVal = std::numeric_limits<double>::max();
  double maxVal = std::numeric_limits<double>::lowest();
  bool foundData = false;

  // Scan all samples from all connected pens to find data range
  for (int p = 0; p < penCount(); ++p) {
    const Pen &pen = pens_[p];
    if (pen.channel.isEmpty() || !pen.runtimeConnected) {
      continue;
    }
    for (const double sample : pen.samples) {
      if (!std::isfinite(sample)) {
        continue;
      }
      minVal = std::min(minVal, sample);
      maxVal = std::max(maxVal, sample);
      foundData = true;
    }
    // Also consider the current runtime value if available
    if (pen.hasRuntimeValue && std::isfinite(pen.runtimeValue)) {
      minVal = std::min(minVal, pen.runtimeValue);
      maxVal = std::max(maxVal, pen.runtimeValue);
      foundData = true;
    }
  }

  if (!foundData) {
    // No data yet - use default range
    autoscaleLow_ = 0.0;
    autoscaleHigh_ = 1.0;
  } else if (!(maxVal > minVal)) {
    // All values are the same - expand range around that value
    const double value = (minVal + maxVal) / 2.0;
    const double margin = std::abs(value) * 0.1;
    autoscaleLow_ = value - (margin > 0 ? margin : 0.5);
    autoscaleHigh_ = value + (margin > 0 ? margin : 0.5);
  } else {
    // Add 5% margin above and below the data range for visual clarity
    const double range = maxVal - minVal;
    const double margin = range * 0.05;
    autoscaleLow_ = minVal - margin;
    autoscaleHigh_ = maxVal + margin;
  }

  autoscaleLimitsValid_ = true;
}

bool StripChartElement::forwardMouseEventToParent(QMouseEvent *event) const
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
