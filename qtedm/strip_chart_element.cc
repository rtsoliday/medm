#include "strip_chart_element.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QDateTime>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
#include <QTimer>

#include "medm_colors.h"

namespace {

constexpr int kShadowThickness = 3;
constexpr int kOuterMargin = 3;
constexpr int kInnerMargin = 6;
constexpr int kGridLines = 5;
constexpr int kMaxTickMarks = 10;
constexpr double kPenSampleCount = 24.0;
constexpr int kRefreshIntervalMs = 100;
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
  sampleIntervalMs_ = periodMilliseconds();
  cachedChartWidth_ = 0;
  updateSamplingGeometry(chartRect().width());
  invalidateStaticCache();
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
  sampleIntervalMs_ = periodMilliseconds();
  cachedChartWidth_ = 0;
  updateSamplingGeometry(chartRect().width());
  invalidateStaticCache();
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
  newSampleColumns_ = 0;
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
      // Draw pen data using incremental cache for performance
      if (cachedLayout_.chartRect.width() > 2 && cachedLayout_.chartRect.height() > 2) {
        const QRect plotArea = cachedLayout_.chartRect.adjusted(1, 1, -1, -1);
        ensurePenCache(plotArea);
        if (!penCache_.isNull()) {
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
  const int nDivX = calculateXAxisTickCount(chartRect.width(), QFontMetrics(labelFont()));
  int nDivY = std::min(kMaxTickMarks, kGridLines);

  // Draw Y-axis tick marks (left side of chart)
  for (int i = 0; i <= nDivY; ++i) {
    const int tickY = chartRect.top() + i * chartRect.height() / nDivY;
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
  const int nDivY = std::min(kMaxTickMarks, kGridLines);

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
      
      const double low = effectivePenLow(p);
      const double high = effectivePenHigh(p);
      
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
    
    // If no ranges found, use default
    if (ranges.empty()) {
      YAxisRange defaultRange;
      defaultRange.low = 0.0;
      defaultRange.high = 100.0;
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
        
        const int tickY = chartRect.top() + i * (chartRect.height() - 1) / nDivY;
        
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

  int capacity = cachedChartWidth_;
  if (capacity <= 0) {
    capacity = std::max(sampleHistoryLength_, 1);
  }
  capacity = std::max(capacity, 1);
  const int denominator = std::max(capacity - 1, 1);

  for (int i = 0; i < penCount(); ++i) {
    const Pen &pen = pens_[i];
    if (pen.samples.empty()) {
      continue;
    }

    const double low = effectivePenLow(i);
    const double high = effectivePenHigh(i);
    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }
    const double range = std::max(std::abs(high - low), kMinimumRangeEpsilon);

    QPainterPath path;
    bool segmentStarted = false;
    bool singlePointPending = false;
    QPointF singlePoint;

    const int sampleCount = static_cast<int>(pen.samples.size());
    const int offsetColumns = std::max(capacity - sampleCount, 0);
    for (int s = 0; s < sampleCount; ++s) {
      const double sampleValue = pen.samples[static_cast<std::size_t>(s)];
      if (!std::isfinite(sampleValue)) {
        segmentStarted = false;
        continue;
      }
      // Skip values outside the LOPR-HOPR range instead of clamping them
      if (sampleValue < low || sampleValue > high) {
        segmentStarted = false;
        continue;
      }
      const double normalized = (sampleValue - low) / range;
      const double x = content.left()
          + (static_cast<double>(offsetColumns + s) / denominator) * width;
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

void StripChartElement::ensureRefreshTimer()
{
  if (refreshTimer_) {
    return;
  }
  refreshTimer_ = new QTimer(this);
  refreshTimer_->setTimerType(Qt::PreciseTimer);
  refreshTimer_->setInterval(kRefreshIntervalMs);
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
  maybeAppendSamples(nowMs);
  update();
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
    return;
  }

  if (!anyPenReady()) {
    lastSampleMs_ = nowMs;
    return;
  }

  if (cachedChartWidth_ <= 0) {
    const int width = chartRect().width();
    if (width <= 0) {
      lastSampleMs_ = nowMs;
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

  if (lastSampleMs_ == 0) {
    appendSampleColumn();
    lastSampleMs_ = nowMs;
    return;
  }

  const double interval = sampleIntervalMs_;
  const qint64 elapsedMs = nowMs - lastSampleMs_;
  if (elapsedMs <= 0) {
    return;
  }
  if (elapsedMs < static_cast<qint64>(interval)) {
    return;
  }

  int columns = static_cast<int>(elapsedMs / interval);
  columns = std::clamp(columns, 1, kMaxSampleBurst);

  for (int i = 0; i < columns; ++i) {
    appendSampleColumn();
  }

  const qint64 advanced = static_cast<qint64>(std::llround(interval * columns));
  lastSampleMs_ += advanced;
  if (lastSampleMs_ > nowMs) {
    lastSampleMs_ = nowMs;
  }
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
}

void StripChartElement::ensurePenCache(const QRect &plotArea)
{
  const QSize plotSize = plotArea.size();
  if (plotSize.isEmpty()) {
    penCache_ = QPixmap();
    penCacheDirty_ = true;
    return;
  }

  // Check if we need a full redraw
  const bool sizeChanged = penCache_.isNull() || penCache_.size() != plotSize;
  const bool plotAreaMoved = penCachePlotArea_ != plotArea;
  const bool needsFullRedraw = penCacheDirty_ || sizeChanged || plotAreaMoved;

  if (needsFullRedraw) {
    // Full redraw: create new cache and paint all pens
    penCache_ = QPixmap(plotSize);
    penCache_.fill(Qt::transparent);

    QPainter cachePainter(&penCache_);
    cachePainter.setRenderHint(QPainter::Antialiasing, false);

    // Create a normalized rect at (0,0) for painting
    const QRect normalizedRect(0, 0, plotSize.width(), plotSize.height());
    paintRuntimePens(cachePainter, normalizedRect);

    cachePainter.end();
    penCachePlotArea_ = plotArea;
    penCacheDirty_ = false;
    newSampleColumns_ = 0;
    return;
  }

  // Incremental update: scroll existing content and draw new columns
  const int columnsToAdd = newSampleColumns_;
  if (columnsToAdd <= 0) {
    return;  // Nothing new to draw
  }

  // If too many new columns, just do a full redraw
  const int width = plotSize.width();
  if (columnsToAdd >= width) {
    penCacheDirty_ = true;
    ensurePenCache(plotArea);
    return;
  }

  scrollPenCache(columnsToAdd, plotArea);
  paintIncrementalPens(plotArea, columnsToAdd);
  newSampleColumns_ = 0;
}

void StripChartElement::scrollPenCache(int columns, const QRect &plotArea)
{
  if (columns <= 0 || penCache_.isNull()) {
    return;
  }

  const int width = plotArea.width();
  const int height = plotArea.height();
  if (columns >= width) {
    return;  // Will be handled by full redraw
  }

  // Scroll existing content left by 'columns' pixels
  QPixmap newCache(penCache_.size());
  newCache.fill(Qt::transparent);

  QPainter painter(&newCache);
  // Copy the right portion of the old cache, shifted left
  painter.drawPixmap(0, 0, penCache_, columns, 0, width - columns, height);
  painter.end();

  penCache_ = newCache;
}

void StripChartElement::paintIncrementalPens(const QRect &plotArea, int newColumns)
{
  if (newColumns <= 0 || penCache_.isNull()) {
    return;
  }

  const double width = static_cast<double>(plotArea.width());
  const double height = static_cast<double>(plotArea.height());
  if (width <= 0.0 || height <= 0.0) {
    return;
  }

  QPainter painter(&penCache_);
  painter.setRenderHint(QPainter::Antialiasing, false);

  int capacity = cachedChartWidth_;
  if (capacity <= 0) {
    capacity = std::max(sampleHistoryLength_, 1);
  }
  capacity = std::max(capacity, 1);
  const int denominator = std::max(capacity - 1, 1);

  // Calculate the starting sample index for the new columns
  // We only need to draw the last 'newColumns' samples
  const int startColumn = static_cast<int>(width) - newColumns;

  for (int i = 0; i < penCount(); ++i) {
    const Pen &pen = pens_[i];
    if (pen.samples.empty()) {
      continue;
    }

    const double low = effectivePenLow(i);
    const double high = effectivePenHigh(i);
    if (!std::isfinite(low) || !std::isfinite(high)) {
      continue;
    }
    const double range = std::max(std::abs(high - low), kMinimumRangeEpsilon);

    QPen penColor(effectivePenColor(i));
    penColor.setWidth(1);
    painter.setPen(penColor);
    painter.setBrush(Qt::NoBrush);

    const int sampleCount = static_cast<int>(pen.samples.size());
    const int offsetColumns = std::max(capacity - sampleCount, 0);

    // Determine which samples correspond to the new columns
    // We need to find samples that map to x >= startColumn
    // x = (offsetColumns + s) / denominator * width
    // so we need: (offsetColumns + s) / denominator * width >= startColumn
    // s >= (startColumn * denominator / width) - offsetColumns

    const int minSampleIdx = std::max(0,
        static_cast<int>(std::floor((startColumn * denominator / width) - offsetColumns)));

    // Draw the new segment, including one sample before for continuity
    const int drawStartIdx = std::max(0, minSampleIdx - 1);

    QPainterPath path;
    bool segmentStarted = false;
    bool singlePointPending = false;
    QPointF singlePoint;

    for (int s = drawStartIdx; s < sampleCount; ++s) {
      const double sampleValue = pen.samples[static_cast<std::size_t>(s)];
      if (!std::isfinite(sampleValue)) {
        segmentStarted = false;
        continue;
      }
      // Skip values outside the LOPR-HOPR range
      if (sampleValue < low || sampleValue > high) {
        segmentStarted = false;
        continue;
      }
      const double normalized = (sampleValue - low) / range;
      const double x = (static_cast<double>(offsetColumns + s) / denominator) * width;
      const double y = (height - 1.0) * (1.0 - normalized);

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
      painter.drawPath(path);
    } else if (singlePointPending) {
      painter.drawPoint(singlePoint);
    }
  }

  painter.end();
}
