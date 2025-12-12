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
#include "window_utils.h"

namespace {

constexpr int kShadowThickness = 3;
constexpr int kOuterMargin = 3;
constexpr int kInnerMargin = 6;
constexpr int kGridLines = 5;
constexpr int kMaxTickMarks = 10;
constexpr double kPenSampleCount = 24.0;
constexpr int kDefaultRefreshIntervalMs = 100;
constexpr int kMaxRefreshIntervalMs = 1000;
constexpr int kLateThresholdMs = 100;
constexpr int kLateCountThreshold = 5;
constexpr int kOnTimeCountThreshold = 20;
constexpr int kIntervalIncrementMs = 100;
constexpr double kMinimumRangeEpsilon = 1e-9;
constexpr int kMaxSampleBurst = 32;

constexpr int kDefaultPenColorIndex = 14;

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

void drawRaisedBevel(QPainter &painter, const QRect &rect,
    const QColor &baseColor, int depth)
{
  if (!rect.isValid() || depth <= 0) {
    return;
  }

  const QColor lightShade = baseColor.lighter(150);
  const QColor darkShade = baseColor.darker(150);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);

  for (int i = 0; i < depth; ++i) {
    const int offset = i;
    const int x = rect.x() + offset;
    const int y = rect.y() + offset;
    const int w = rect.width() - 1 - 2 * offset;
    const int h = rect.height() - 1 - 2 * offset;

    painter.setPen(lightShade);
    painter.drawLine(QPoint(x, y), QPoint(x + w, y));
    painter.drawLine(QPoint(x, y), QPoint(x, y + h));

    painter.setPen(darkShade);
    painter.drawLine(QPoint(x, y + h), QPoint(x + w, y + h));
    painter.drawLine(QPoint(x + w, y), QPoint(x + w, y + h));
  }

  painter.restore();
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
    pens_[i].limits.lowSource = PvLimitSource::kDefault;
    pens_[i].limits.highSource = PvLimitSource::kDefault;
    pens_[i].limits.lowDefault = 0.0;
    pens_[i].limits.highDefault = 100.0;
    pens_[i].limits.precisionSource = PvLimitSource::kChannel;
    pens_[i].limits.precisionDefault = 0;
    pens_[i].color = defaultPenColor(i);
    pens_[i].runtimeConnected = false;
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
  const double clamped = period > 0.0 ? period : kDefaultStripChartPeriod;
  if (std::abs(period_ - clamped) < 1e-6) {
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
  if (pens_[index].channel == channel) {
    return;
  }
  pens_[index].channel = channel;
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
    pen.runtimeLimitsValid = false;
    pen.runtimeLow = pen.limits.lowDefault;
    pen.runtimeHigh = pen.limits.highDefault;
    pen.hasRuntimeValue = false;
  }
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
  if (std::abs(high - low) < kMinimumRangeEpsilon) {
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
  Q_UNUSED(timestampMs);
  if (!executeMode_ || index < 0 || index >= penCount()) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }
  Pen &pen = pens_[index];
  if (!pen.runtimeConnected) {
    return;
  }

  pen.runtimeValue = value;
  pen.hasRuntimeValue = true;
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
  for (int i = 0; i < penCount(); ++i) {
    Pen &pen = pens_[i];
    pen.runtimeConnected = false;
    pen.runtimeLimitsValid = false;
    pen.runtimeLow = pen.limits.lowDefault;
    pen.runtimeHigh = pen.limits.highDefault;
    pen.samples.clear();
    pen.runtimeValue = 0.0;
    pen.hasRuntimeValue = false;
  }
  invalidatePenCache();
  updateRefreshTimer();
  update();
}

void StripChartElement::clearPenRuntimeState(int index)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  Pen &pen = pens_[index];
  pen.runtimeConnected = false;
  pen.runtimeLimitsValid = false;
  pen.runtimeLow = pen.limits.lowDefault;
  pen.runtimeHigh = pen.limits.highDefault;
  pen.runtimeValue = 0.0;
  pen.hasRuntimeValue = false;
  if (sampleHistoryLength_ > 0) {
    pen.samples.assign(static_cast<std::size_t>(sampleHistoryLength_),
        std::numeric_limits<double>::quiet_NaN());
  } else {
    pen.samples.clear();
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
          // After full redraw, data fills the entire cache - just display it directly
          // The paintRuntimePens function handles positioning correctly
          painter.drawPixmap(plotArea.topLeft(), penCache_);
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
      paintTickMarks(painter, layout.chartRect);
      paintAxisScales(painter, layout.chartRect, metrics, layout.yAxisLabelOffset);
      if (layout.chartRect.width() > 2 && layout.chartRect.height() > 2) {
        paintGrid(painter, layout.chartRect);
        const QRect plotArea = layout.chartRect.adjusted(1, 1, -1, -1);
        paintPens(painter, plotArea);
      }
    }

    paintLabels(painter, layout, metrics);
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

StripChartElement::Layout StripChartElement::calculateLayout(
    const QFontMetrics &metrics) const
{
  Layout layout;
  layout.innerRect = rect().adjusted(kOuterMargin, kOuterMargin,
      -kOuterMargin, -kOuterMargin);
  layout.titleText = title_.trimmed();
  layout.yLabelText = yLabel_.trimmed();
  
  // X-axis label: auto-generate based on time units if empty (like MEDM)
  layout.xLabelText = xLabel_.trimmed();
  if (layout.xLabelText.isEmpty()) {
    switch (units_) {
      case TimeUnits::kMilliseconds:
        layout.xLabelText = "time (ms)";
        break;
      case TimeUnits::kSeconds:
        layout.xLabelText = "time (sec)";
        break;
      case TimeUnits::kMinutes:
        layout.xLabelText = "time (min)";
        break;
      default:
        layout.xLabelText = "time (sec)";
        break;
    }
  }

  if (!layout.innerRect.isValid() || layout.innerRect.isEmpty()) {
    return layout;
  }

  int left = layout.innerRect.left();
  const int right = layout.innerRect.right();
  int top = layout.innerRect.top();
  int bottom = layout.innerRect.bottom();

  if (!layout.titleText.isEmpty()) {
    // Use title font metrics for title (like MEDM)
    const QFontMetrics titleMetrics(titleFont());
    const int height = titleMetrics.height();
    layout.titleRect = QRect(left, top, layout.innerRect.width(), height);
    top += height + 2;  // Reduced margin between title and Y-axis label
  }

  // Reserve space for Y-axis label (horizontal, like medm) if present
  int yLabelTop = -1;  // Track where to place y-label
  if (!layout.yLabelText.isEmpty()) {
    const int height = metrics.height();
    yLabelTop = top;
    top += height + 2;  // Reduced margin between Y-axis label and chart
  }

  if (!layout.xLabelText.isEmpty()) {
    const int height = metrics.height();
    layout.xLabelRect = QRect(left, bottom - height + 1,
        layout.innerRect.width(), height);
    bottom -= height + 0;  // Reduced margin between X-axis label and X-axis tick labels
  }

  // Calculate space needed for Y-axis labels and tick marks
  const int markerHeight = calculateMarkerHeight(width(), height());
  const int yAxisLabelWidth = calculateYAxisLabelWidth(metrics);
  const int yAxisSpace = yAxisLabelWidth + markerHeight + 2 + kInnerMargin;
  left += yAxisSpace;

  // Use symmetric margins (like medm): right margin mirrors left margin
  // This creates visual balance in the widget
  int rightMargin = yAxisSpace;
  
  // Calculate space needed for X-axis labels and tick marks
  const int xAxisSpace = metrics.height() + markerHeight + 2 + kInnerMargin;
  bottom -= xAxisSpace;

  // Calculate Y-axis label height extension (like MEDM's heightExt).
  // Y-axis labels are stacked vertically and centered on each tick mark.
  // The topmost tick's labels extend above the chart; ensure we have room.
  const int yAxisHeightExt = calculateYAxisLabelHeightExtension(metrics);
  const int minTopMargin = yAxisHeightExt + kShadowThickness;
  const int currentTopMargin = top - layout.innerRect.top();
  if (currentTopMargin < minTopMargin) {
    // Increase top margin to accommodate Y-axis labels extending above the chart
    top = layout.innerRect.top() + minTopMargin;
  }
  
  // Apply symmetric right margin, but use the smaller of left or top margin
  // if the top margin is less (for visual consistency)
  const int topMargin = top - layout.innerRect.top();
  if (topMargin < yAxisSpace) {
    // Shrink right margin to match top margin for better proportions
    rightMargin = topMargin;
  }
  
  // Also ensure right margin is at least as large as bottom margin
  const int bottomMargin = layout.innerRect.bottom() - bottom;
  if (bottomMargin > rightMargin) {
    rightMargin = bottomMargin;
  }
  
  // Apply the calculated right margin
  const int adjustedRight = right - rightMargin;

  if (adjustedRight >= left && bottom >= top) {
    layout.chartRect = QRect(left, top, adjustedRight - left + 1,
        bottom - top + 1);
  }

  // Check for overlap between leftmost X-axis label and Y-axis labels (like MEDM)
  // This prevents the X-axis and Y-axis labels from overlapping
  if (layout.chartRect.isValid()) {
    const int nDivX = calculateXAxisTickCount(layout.chartRect.width(), metrics);
    const double periodValue = period_;
    const NumberFormat xFmt = calculateNumberFormat(periodValue);
    const double xStep = periodValue / std::max(nDivX, 1);
    const double leftmostValue = -xStep * nDivX;  // Leftmost X tick value
    const QString leftmostText = formatNumber(leftmostValue, xFmt.format, xFmt.decimal);
    const int leftmostTextWidth = metrics.horizontalAdvance(leftmostText);
    
    // The leftmost X-axis label is centered on chartRect.left()
    // So it extends from (chartRect.left() - leftmostTextWidth/2) to (chartRect.left() + leftmostTextWidth/2)
    const int xLabelLeftEdge = layout.chartRect.left() - leftmostTextWidth / 2;
    
    // The Y-axis labels extend from the left edge to (chartRect.left() - 2 - markerHeight - 1)
    const int yLabelRightEdge = layout.chartRect.left() - 2 - markerHeight - 1;
    
    // Check if they overlap (with a small gap for safety)
    const int overlapAmount = yLabelRightEdge - xLabelLeftEdge + 3;  // +3 for small gap
    
    if (overlapAmount > 0) {
      // Store the offset and shrink the chart from the left (like MEDM)
      // This moves Y-axis labels left by the offset amount
      layout.yAxisLabelOffset = overlapAmount;
      left += overlapAmount;
      if (left < adjustedRight) {
        layout.chartRect = QRect(left, top, adjustedRight - left + 1,
            bottom - top + 1);
      }
    }
  }

  // Position Y-axis label at the left edge of the chart area (like medm)
  if (yLabelTop >= 0 && !layout.yLabelText.isEmpty()) {
    const int yLabelWidth = layout.chartRect.isValid() ? layout.chartRect.width() : 0;
    const int yLabelHeight = metrics.height();
    const int yLabelLeft = layout.chartRect.isValid() ? layout.chartRect.left() : left;
    layout.yLabelRect = QRect(yLabelLeft, yLabelTop, yLabelWidth, yLabelHeight);
  }

  return layout;
}

int StripChartElement::calculateYAxisLabelWidth(const QFontMetrics &metrics) const
{
  // Build list of unique ranges (like medm's calcYAxisLabelWidth)
  struct YAxisRange {
    double low;
    double high;
    int numPens;
  };
  std::vector<YAxisRange> ranges;
  
  for (int p = 0; p < penCount(); ++p) {
    if (pens_[p].channel.trimmed().isEmpty()) {
      continue;
    }
    
    const double low = effectivePenLow(p);
    const double high = effectivePenHigh(p);
    
    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }
    
    // Check if this range already exists
    bool found = false;
    for (auto &range : ranges) {
      if (std::abs(range.low - low) < 1e-9 && std::abs(range.high - high) < 1e-9) {
        range.numPens++;
        found = true;
        break;
      }
    }
    
    if (!found) {
      YAxisRange newRange;
      newRange.low = low;
      newRange.high = high;
      newRange.numPens = 1;
      ranges.push_back(newRange);
    }
  }
  
  // If no ranges found, use default
  if (ranges.empty()) {
    YAxisRange defaultRange;
    defaultRange.low = 0.0;
    defaultRange.high = 100.0;
    defaultRange.numPens = 0;
    ranges.push_back(defaultRange);
  }
  
  // Find the maximum text width needed for all ranges
  int maxWidth = 0;
  int maxDots = 0;
  
  for (const auto &yRange : ranges) {
    const NumberFormat fmt = calculateNumberFormat(
        std::max(std::abs(yRange.high), std::abs(yRange.low)));
    
    // Check width for both high and low values
    const QString highText = formatNumber(yRange.high, fmt.format, fmt.decimal);
    const QString lowText = formatNumber(yRange.low, fmt.format, fmt.decimal);
    
    const int highWidth = metrics.horizontalAdvance(highText);
    const int lowWidth = metrics.horizontalAdvance(lowText);
    
    maxWidth = std::max(maxWidth, std::max(highWidth, lowWidth));
    
    // Count color indicators (only if multiple ranges)
    if (ranges.size() > 1) {
      maxDots = std::max(maxDots, yRange.numPens);
    }
  }
  
  // Total width = text width + space for color indicators
  constexpr int kLineSpace = 3;
  return maxWidth + (maxDots * kLineSpace);
}

int StripChartElement::calculateYAxisLabelHeightExtension(
    const QFontMetrics &metrics) const
{
  // Count the number of unique Y-axis ranges (like MEDM's calcYAxisLabelHeight).
  // Y-axis labels are vertically stacked at each tick mark, centered on the tick.
  // This function returns how much the labels extend above (or below) the tick,
  // which determines the minimum top/bottom margin needed to avoid clipping.
  int numRanges = 0;

  struct YAxisRange {
    double low;
    double high;
  };
  std::vector<YAxisRange> ranges;

  for (int p = 0; p < penCount(); ++p) {
    if (pens_[p].channel.trimmed().isEmpty()) {
      continue;
    }

    const double low = effectivePenLow(p);
    const double high = effectivePenHigh(p);

    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }

    // Check if this range already exists
    bool found = false;
    for (const auto &range : ranges) {
      if (std::abs(range.low - low) < 1e-9 && std::abs(range.high - high) < 1e-9) {
        found = true;
        break;
      }
    }

    if (!found) {
      YAxisRange newRange;
      newRange.low = low;
      newRange.high = high;
      ranges.push_back(newRange);
    }
  }

  numRanges = ranges.empty() ? 1 : static_cast<int>(ranges.size());

  // The total height of stacked labels at each tick is numRanges * fontHeight.
  // Labels are centered on the tick, so they extend (totalHeight / 2) above.
  const int labelHeight = metrics.height();
  const int totalLabelsHeight = numRanges * labelHeight;
  return (totalLabelsHeight + 1) / 2;  // Half extends above the tick mark
}

// Calculate the optimal number of X-axis tick divisions based on available width.
// This function ensures tick labels don't overlap by reducing the number of ticks
// when space is constrained. The algorithm:
// 1. Finds the widest label that would appear for any tick count
// 2. Calculates minimum spacing (label width + small gap)
// 3. Tests divisor counts from max down to 2, returning the first that fits
// 4. Guarantees at least 2 ticks (min and max) are always shown
int StripChartElement::calculateXAxisTickCount(int chartWidth,
    const QFontMetrics &metrics) const
{
  if (chartWidth <= 0) {
    return kGridLines;
  }

  // Start with the maximum tick count
  int maxTicks = std::min(kMaxTickMarks, kGridLines);
  
  // Calculate the number format for our period value
  const double periodValue = period_;
  const NumberFormat fmt = calculateNumberFormat(periodValue);
  
  // Find the widest label we'll display by checking all potential tick values
  int maxLabelWidth = 0;
  for (int nDiv = 2; nDiv <= maxTicks; ++nDiv) {
    const double step = periodValue / nDiv;
    for (int i = 0; i <= nDiv; ++i) {
      const double value = -step * i;
      const QString text = formatNumber(value, fmt.format, fmt.decimal);
      const int textWidth = metrics.horizontalAdvance(text);
      maxLabelWidth = std::max(maxLabelWidth, textWidth);
    }
  }
  
  // Add small spacing between labels (2-3 pixels minimum gap)
  constexpr int kMinLabelGap = 3;
  const int minLabelSpacing = maxLabelWidth + kMinLabelGap;
  
  // Calculate how many divisions we can fit
  // Each division takes up (chartWidth / nDiv) pixels
  // We need that to be >= minLabelSpacing
  for (int nDiv = maxTicks; nDiv >= 2; --nDiv) {
    const int spacingPerLabel = chartWidth / nDiv;
    if (spacingPerLabel >= minLabelSpacing) {
      return nDiv;
    }
  }
  
  // Minimum is 2 (show min and max only)
  return 2;
}

// Calculate the optimal number of Y-axis tick divisions based on available height.
// This function mimics MEDM's behavior: it calculates how many sets of stacked
// labels (one per unique Y-axis range) can fit vertically without overlapping.
// The algorithm:
// 1. Counts the number of unique Y-axis ranges (numRanges)
// 2. Calculates the height needed for one row of stacked labels plus spacing
// 3. Divides the available chart height by this to get the number of divisions
// 4. Caps at reasonable values (max 10, skip 7 and 9 like MEDM)
// 5. Guarantees at least 1 division (min and max always shown)
int StripChartElement::calculateYAxisTickCount(int chartHeight,
    const QFontMetrics &metrics) const
{
  if (chartHeight <= 0) {
    return kGridLines;
  }

  // Count the number of unique Y-axis ranges (like MEDM's sccd.numYAxisLabel)
  int numRanges = 0;
  struct YAxisRange {
    double low;
    double high;
  };
  std::vector<YAxisRange> ranges;

  for (int p = 0; p < penCount(); ++p) {
    if (pens_[p].channel.trimmed().isEmpty()) {
      continue;
    }

    const double low = effectivePenLow(p);
    const double high = effectivePenHigh(p);

    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }

    // Check if this range already exists
    bool found = false;
    for (const auto &range : ranges) {
      if (std::abs(range.low - low) < 1e-9 && std::abs(range.high - high) < 1e-9) {
        found = true;
        break;
      }
    }

    if (!found) {
      YAxisRange newRange;
      newRange.low = low;
      newRange.high = high;
      ranges.push_back(newRange);
    }
  }

  numRanges = ranges.empty() ? 1 : static_cast<int>(ranges.size());

  // Calculate the height needed for one set of stacked labels plus vertical spacing.
  // MEDM uses: labelHeight = (numYAxisLabel + verticalSpacing) * axisLabelFontHeight
  // where verticalSpacing is typically 2.0
  constexpr double kVerticalSpacing = 2.0;
  const int fontHeight = metrics.height();
  const int labelHeight = static_cast<int>((static_cast<double>(numRanges) + kVerticalSpacing) 
                                           * static_cast<double>(fontHeight));

  // Calculate number of divisions that can fit
  // MEDM uses: nDiv = (dataHeight - 1) / labelHeight
  int nDiv = (chartHeight - 1) / labelHeight;

  // Cap at reasonable values like MEDM does
  if (nDiv > 10) {
    nDiv = 10;
  } else if (nDiv == 9) {
    nDiv = 8;
  } else if (nDiv == 7) {
    nDiv = 6;
  }

  // Ensure at least 1 division (shows min and max)
  if (nDiv < 1) {
    nDiv = 1;
  }

  return nDiv;
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

QFont StripChartElement::labelFont() const
{
  // Calculate font size based on widget dimensions (like MEDM)
  // MEDM uses pixel height, not point size
  const int pixelHeight = calculateLabelFontSize(width(), height());
  QFont adjusted = font();
  adjusted.setPixelSize(pixelHeight);
  return adjusted;
}

QFont StripChartElement::titleFont() const
{
  // Calculate title font size based on widget dimensions (like MEDM)
  // MEDM uses pixel height, not point size
  const int pixelHeight = calculateTitleFontSize(width(), height());
  QFont adjusted = font();
  adjusted.setPixelSize(pixelHeight);
  return adjusted;
}

void StripChartElement::paintFrame(QPainter &painter) const
{
  const QColor bgColor = effectiveBackground();
  painter.fillRect(rect(), bgColor);
  drawRaisedBevel(painter, rect(), bgColor, kShadowThickness);
}

void StripChartElement::paintGrid(QPainter &painter, const QRect &content) const
{
  if (content.width() <= 0 || content.height() <= 0) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  
  // Draw solid rectangle border around the chart area (like medm)
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  pen.setStyle(Qt::SolidLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  
  // Draw the border at the edge of the content area
  // medm draws: XDrawRectangle at (dataX0-1, dataY0-1, dataWidth+1, dataHeight+1)
  // which creates a border just outside the data area
  const QRect borderRect = content.adjusted(-1, -1, 1, 1);
  painter.drawRect(borderRect);

  painter.restore();
}

void StripChartElement::paintTickMarks(QPainter &painter, const QRect &chartRect) const
{
  if (chartRect.width() <= 0 || chartRect.height() <= 0) {
    return;
  }

  const int markerHeight = calculateMarkerHeight(width(), height());
  if (markerHeight <= 0) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  pen.setStyle(Qt::SolidLine);
  painter.setPen(pen);

  // Calculate number of divisions (limit to reasonable range)
  const QFontMetrics labelMetrics(labelFont());
  const int nDivX = calculateXAxisTickCount(chartRect.width(), labelMetrics);
  const int nDivY = calculateYAxisTickCount(chartRect.height(), labelMetrics);

  // Data is drawn in plotArea = chartRect.adjusted(1, 1, -1, -1)
  // Data Y formula: plotArea.top() + (plotArea.height() - 1) * (1.0 - normalized)
  // Tick marks should align with where data at each division would be drawn
  const int plotTop = chartRect.top() + 1;
  const int plotHeight = chartRect.height() - 2;

  // Draw Y-axis tick marks (left side of chart)
  for (int i = 0; i <= nDivY; ++i) {
    // Match the data coordinate formula: plotTop + (plotHeight - 1) * (i / nDivY)
    const int tickY = plotTop + i * (plotHeight - 1) / nDivY;
    const int x1 = chartRect.left() - 2 - (markerHeight - 1);
    const int x2 = chartRect.left() - 2;
    painter.drawLine(x1, tickY, x2, tickY);
  }

  // Draw X-axis tick marks (bottom of chart)
  for (int i = 0; i <= nDivX; ++i) {
    const int tickX = chartRect.right() - i * chartRect.width() / nDivX;
    const int y1 = chartRect.bottom() + 2;
    const int y2 = chartRect.bottom() + 2 + markerHeight;
    painter.drawLine(tickX, y1, tickX, y2);
  }

  painter.restore();
}

void StripChartElement::paintAxisScales(QPainter &painter, const QRect &chartRect,
    const QFontMetrics &metrics, int yAxisLabelOffset) const
{
  if (chartRect.width() <= 0 || chartRect.height() <= 0) {
    return;
  }

  const int markerHeight = calculateMarkerHeight(width(), height());
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  pen.setStyle(Qt::SolidLine);
  painter.setPen(pen);

  const int nDivX = calculateXAxisTickCount(chartRect.width(), metrics);
  const int nDivY = calculateYAxisTickCount(chartRect.height(), metrics);

  // Draw X-axis scale numbers (bottom, counting backward from 0)
  {
    const double periodValue = period_;
    const NumberFormat fmt = calculateNumberFormat(periodValue);
    const double step = periodValue / nDivX;

    const int textY = chartRect.bottom() + 2 + markerHeight + metrics.ascent() + 1;

    for (int i = 0; i <= nDivX; ++i) {
      const double value = -step * i;  // Count backward from 0
      const QString text = formatNumber(value, fmt.format, fmt.decimal);
      const int textWidth = metrics.horizontalAdvance(text);
      
      const int tickX = chartRect.right() - i * chartRect.width() / nDivX;
      const int textX = tickX - textWidth / 2;
      
      painter.drawText(textX, textY, text);
    }
  }

  // Draw Y-axis scale numbers (left side) - mimicking medm's multi-range approach
  {
    // Build list of unique ranges (like medm's range array)
    struct YAxisRange {
      double low;
      double high;
      int penMask;  // Bitmask of which pens use this range
      int numPens;
    };
    std::vector<YAxisRange> ranges;
    
    for (int p = 0; p < penCount(); ++p) {
      if (pens_[p].channel.trimmed().isEmpty()) {
        continue;
      }
      
      // Use zoomed range for axis labels when zoomed
      const double low = zoomedPenLow(p);
      const double high = zoomedPenHigh(p);
      
      if (!std::isfinite(low) || !std::isfinite(high)) {
        continue;
      }
      
      // Check if this range already exists
      bool found = false;
      for (auto &range : ranges) {
        if (std::abs(range.low - low) < 1e-9 && std::abs(range.high - high) < 1e-9) {
          range.penMask |= (1 << p);
          range.numPens++;
          found = true;
          break;
        }
      }
      
      if (!found) {
        YAxisRange newRange;
        newRange.low = low;
        newRange.high = high;
        newRange.penMask = (1 << p);
        newRange.numPens = 1;
        ranges.push_back(newRange);
      }
    }
    
    // If no ranges found, use default (apply zoom to default as well)
    if (ranges.empty()) {
      YAxisRange defaultRange;
      defaultRange.low = 0.0;
      defaultRange.high = 100.0;
      applyZoomToRange(defaultRange.low, defaultRange.high);
      defaultRange.penMask = 0;
      defaultRange.numPens = 0;
      ranges.push_back(defaultRange);
    }
    
    // If only one range, don't show pen color indicators (like medm)
    const bool showPenIndicators = ranges.size() > 1;
    
    // Draw scale for each range
    constexpr int kLineSpace = 3;  // Space for pen color indicators
    const int indicatorWidth = 2;  // Width of color indicator rectangle
    
    // Find maximum text width for left-aligning indicators (like MEDM)
    int maxTextWidth = 0;
    for (std::size_t rangeIdx = 0; rangeIdx < ranges.size(); ++rangeIdx) {
      const YAxisRange &yRange = ranges[rangeIdx];
      const double range = yRange.high - yRange.low;
      const double step = range / nDivY;
      const NumberFormat fmt = calculateNumberFormat(std::max(std::abs(yRange.high), std::abs(yRange.low)));
      
      for (int i = 0; i <= nDivY; ++i) {
        const double value = yRange.high - step * i;
        const QString text = formatNumber(value, fmt.format, fmt.decimal);
        const int textWidth = metrics.horizontalAdvance(text);
        maxTextWidth = std::max(maxTextWidth, textWidth);
      }
    }
    
    for (std::size_t rangeIdx = 0; rangeIdx < ranges.size(); ++rangeIdx) {
      const YAxisRange &yRange = ranges[rangeIdx];
      const double range = yRange.high - yRange.low;
      const double step = range / nDivY;
      const NumberFormat fmt = calculateNumberFormat(std::max(std::abs(yRange.high), std::abs(yRange.low)));
      
      for (int i = 0; i <= nDivY; ++i) {
        const double value = yRange.high - step * i;  // Count down from high to low
        const QString text = formatNumber(value, fmt.format, fmt.decimal);
        const int textWidth = metrics.horizontalAdvance(text);
        
        // Match tick mark and data coordinate system:
        // Data is drawn in plotArea = chartRect.adjusted(1, 1, -1, -1)
        const int plotTop = chartRect.top() + 1;
        const int plotHeight = chartRect.height() - 2;
        const int tickY = plotTop + i * (plotHeight - 1) / nDivY;
        
        // Calculate vertical offset for this range label
        const int labelHeight = metrics.height();
        const int totalLabelsHeight = static_cast<int>(ranges.size()) * labelHeight;
        const int startOffset = -totalLabelsHeight / 2;
        const int labelY = tickY + startOffset + static_cast<int>(rangeIdx) * labelHeight 
                         + metrics.ascent();
        
        // Draw text (right-aligned - each text uses its own width)
        painter.setPen(effectiveForeground());
        const int textX = chartRect.left() - 2 - markerHeight - 1 - yAxisLabelOffset - textWidth;
        painter.drawText(textX, labelY, text);
        
        // Draw pen color indicators if multiple ranges
        if (showPenIndicators) {
          // Indicators are left-aligned using maxTextWidth (like MEDM)
          const int indicatorBaseX = chartRect.left() - 2 - markerHeight - 1 - yAxisLabelOffset - maxTextWidth;
          
          int indicatorCount = 0;
          for (int p = penCount() - 1; p >= 0; --p) {
            if (yRange.penMask & (1 << p)) {
              const QColor penColor = effectivePenColor(p);
              const int indicatorX = indicatorBaseX - (indicatorCount + 1) * kLineSpace;
              const int indicatorY = labelY - metrics.ascent();
              const int indicatorHeight = metrics.ascent();
              
              painter.fillRect(indicatorX, indicatorY, indicatorWidth, indicatorHeight, penColor);
              indicatorCount++;
            }
          }
        }
      }
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
    const double range = std::max(std::abs(high - low), kMinimumRangeEpsilon);

    QPainterPath path;
    bool segmentStarted = false;
    bool singlePointPending = false;
    QPointF singlePoint;

    const int sampleCount = static_cast<int>(pen.samples.size());
    // Only draw the most recent 'capacity' samples to fit the content width
    const int startSample = std::max(0, sampleCount - capacity);
    const int samplesToRender = sampleCount - startSample;
    
    // Position data on the RIGHT side of the chart (newest at right edge)
    // offsetColumns shifts data to the right when buffer isn't full
    const int offsetColumns = capacity - samplesToRender;
    const int denominator = std::max(capacity - 1, 1);
    
    for (int s = startSample; s < sampleCount; ++s) {
      const double sampleValue = pen.samples[static_cast<std::size_t>(s)];
      if (!std::isfinite(sampleValue)) {
        segmentStarted = false;
        continue;
      }
      // When zoomed, allow values outside the visible range to be drawn
      // (they will extend beyond the chart area but get clipped by Qt)
      const double normalized = (sampleValue - low) / range;
      const int renderIndex = s - startSample;
      const double x = content.left()
          + (static_cast<double>(offsetColumns + renderIndex) / denominator) * (width - 1.0);
      const double y = content.top() + (height - 1.0) * (1.0 - normalized);

      if (!segmentStarted) {
        path.moveTo(x, y);
        segmentStarted = true;
        singlePointPending = true;
        singlePoint = QPointF(x, y);
      } else {
        path.lineTo(x, y);
        singlePointPending = false;
      }
    }

    if (path.elementCount() >= 2) {
      QPen penColor(effectivePenColor(i));
      penColor.setWidth(1);
      painter.setPen(penColor);
      painter.setBrush(Qt::NoBrush);
      painter.drawPath(path);
    } else if (singlePointPending) {
      QPen penColor(effectivePenColor(i));
      penColor.setWidth(1);
      painter.setPen(penColor);
      painter.drawPoint(singlePoint);
    }
  }
}

void StripChartElement::paintLabels(QPainter &painter, const Layout &layout,
    const QFontMetrics &metrics) const
{
  painter.save();
  painter.setPen(effectiveForeground());

  if (!layout.titleText.isEmpty() && layout.titleRect.isValid()
      && !layout.titleRect.isEmpty()) {
    // Use larger title font (like MEDM)
    painter.setFont(titleFont());
    painter.drawText(layout.titleRect, Qt::AlignHCenter | Qt::AlignTop,
        layout.titleText);
    // Restore to label font
    painter.setFont(labelFont());
  }

  if (!layout.xLabelText.isEmpty() && layout.xLabelRect.isValid()
      && !layout.xLabelRect.isEmpty()) {
    painter.drawText(layout.xLabelRect, Qt::AlignHCenter | Qt::AlignBottom,
        layout.xLabelText);
  }

  if (!layout.yLabelText.isEmpty() && layout.yLabelRect.isValid()
      && !layout.yLabelRect.isEmpty()) {
    // Draw Y-axis label horizontally (like medm), aligned to the left
    // Position it at the left edge of the chart area
    painter.drawText(layout.yLabelRect, Qt::AlignLeft | Qt::AlignTop,
        layout.yLabelText);
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
  const bool needTimer = executeMode_ && anyPenConnected();
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

  maybeAppendSamples(nowMs);

  // Use update() to queue a paint event. This doesn't block the timer
  // callback, allowing the event loop to process other events. Multiple
  // update() calls will be coalesced into a single paint, and the paint
  // will draw all accumulated columns at once.
  update();

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
    }
    return;
  }

  int newLength = 0;
  for (Pen &pen : pens_) {
    while (static_cast<int>(pen.samples.size()) > capacity) {
      pen.samples.pop_front();
    }
    newLength = std::max(newLength, static_cast<int>(pen.samples.size()));
  }
  sampleHistoryLength_ = newLength;
}

void StripChartElement::maybeAppendSamples(qint64 nowMs)
{
  if (!anyPenConnected()) {
    lastSampleMs_ = nowMs;
    nextAdvanceTimeMs_ = 0;
    return;
  }

  if (!anyPenReady()) {
    lastSampleMs_ = nowMs;
    nextAdvanceTimeMs_ = 0;
    return;
  }

  if (cachedChartWidth_ <= 0) {
    const int width = chartRect().width();
    if (width <= 0) {
      lastSampleMs_ = nowMs;
      nextAdvanceTimeMs_ = 0;
      return;
    }
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
    nextAdvanceTimeMs_ = nowMs + static_cast<qint64>(std::llround(sampleIntervalMs_));
    return;
  }

  // MEDM-style: check if current time exceeds nextAdvanceTime
  if (nowMs < nextAdvanceTimeMs_) {
    return;
  }

  // Calculate how many pixels (columns) need to be drawn
  const double interval = sampleIntervalMs_;
  int totalPixels = 1 + static_cast<int>((nowMs - nextAdvanceTimeMs_) / interval);
  totalPixels = std::clamp(totalPixels, 1, kMaxSampleBurst);

  for (int i = 0; i < totalPixels; ++i) {
    appendSampleColumn();
  }

  // Advance nextAdvanceTimeMs_ by the interval * totalPixels (MEDM-style)
  nextAdvanceTimeMs_ += static_cast<qint64>(std::llround(interval * totalPixels));
  lastSampleMs_ = nowMs;
}

void StripChartElement::appendSampleColumn()
{
  const int capacity = std::max(cachedChartWidth_, 1);
  if (capacity <= 0) {
    return;
  }

  const double missingValue = std::numeric_limits<double>::quiet_NaN();
  for (Pen &pen : pens_) {
    double sampleValue = missingValue;
    if (pen.runtimeConnected && pen.hasRuntimeValue) {
      sampleValue = pen.runtimeValue;
    }
    pen.samples.push_back(sampleValue);
    if (static_cast<int>(pen.samples.size()) > capacity) {
      pen.samples.pop_front();
    }
  }

  int newLength = 0;
  for (const Pen &pen : pens_) {
    newLength = std::max(newLength, static_cast<int>(pen.samples.size()));
  }
  sampleHistoryLength_ = newLength;

  // Track new columns for incremental pen drawing
  ++newSampleColumns_;
  
  // Always mark pen cache dirty so that paintRuntimePens is called
  // to properly render the scrolling data. The incremental update path
  // doesn't handle scrolling correctly.
  penCacheDirty_ = true;
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

bool StripChartElement::anyPenReady() const
{
  for (const Pen &pen : pens_) {
    if (pen.runtimeConnected && pen.hasRuntimeValue) {
      return true;
    }
  }
  return false;
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
    paintTickMarks(cachePainter, cachedLayout_.chartRect);
    paintAxisScales(cachePainter, cachedLayout_.chartRect, metrics,
        cachedLayout_.yAxisLabelOffset);
    if (cachedLayout_.chartRect.width() > 2 && cachedLayout_.chartRect.height() > 2) {
      paintGrid(cachePainter, cachedLayout_.chartRect);
    }
  }

  paintLabels(cachePainter, cachedLayout_, metrics);

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
    paintTickMarks(painter, layout.chartRect);
    paintAxisScales(painter, layout.chartRect, metrics, layout.yAxisLabelOffset);
    if (layout.chartRect.width() > 2 && layout.chartRect.height() > 2) {
      paintGrid(painter, layout.chartRect);
    }
  }

  paintLabels(painter, layout, metrics);
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
          const double range = std::max(std::abs(high - low), 1e-12);

          const double value = pen.samples[static_cast<std::size_t>(sampleIdx)];
          if (!std::isfinite(value)) {
            continue;
          }

          // When zoomed, allow values outside the visible range
          const double normalized = (value - low) / range;
          const int y = static_cast<int>((height - 1) * (1.0 - normalized));

          // Draw a vertical line for this sample (or point)
          // Check previous sample for line continuity
          int prevY = y;
          if (sampleIdx > 0 && static_cast<std::size_t>(sampleIdx - 1) < pen.samples.size()) {
            const double prevValue = pen.samples[static_cast<std::size_t>(sampleIdx - 1)];
            if (std::isfinite(prevValue)) {
              const double prevNorm = (prevValue - low) / range;
              prevY = static_cast<int>((height - 1) * (1.0 - prevNorm));
            }
          }

          QPen penColor(effectivePenColor(i));
          penColor.setWidth(1);
          cachePainter.setPen(penColor);

          // Draw vertical line from prevY to y (like MEDM does for min/max)
          if (prevY != y) {
            cachePainter.drawLine(writeX, std::min(y, prevY), writeX, std::max(y, prevY));
          } else {
            cachePainter.drawPoint(writeX, y);
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
      // Start panning (Y-axis only)
      const QRect chart = chartRect();
      if (chart.contains(event->pos())) {
        panning_ = true;
        panStartPos_ = event->pos();
        panStartYCenter_ = zoomYCenter_;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
      }
    } else if (event->button() == Qt::RightButton) {
      // Show context menu with reset zoom option only when zoomed
      if (zoomed_) {
        QMenu menu(this);
        QAction *resetAction = menu.addAction(tr("Reset Zoom"));
        QAction *selected = menu.exec(event->globalPos());
        if (selected == resetAction) {
          resetZoom();
        }
        event->accept();
        return;
      }
      // Forward right-click events to parent window for context menu
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
