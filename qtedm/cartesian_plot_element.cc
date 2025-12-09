#include "cartesian_plot_element.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <QtGlobal>

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QWheelEvent>

#include "medm_colors.h"
#include "text_font_utils.h"
#include "window_utils.h"

namespace {

constexpr int kOuterMargin = 4;
constexpr int kInnerMargin = 4;
constexpr int kHorizontalMargin = 12;
// Unused: constexpr int kGridLines = 5;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kMinimumSampleCount = 8;
constexpr int kMaximumSampleCount = kCartesianPlotMaximumSampleCount;

constexpr int kDefaultTraceColorIndex = 14;
constexpr qreal kAxisCueThickness = 3.0;
constexpr qreal kAxisCueGap = 2.0;

// Font size constants matching medm's SciPlot defaults
// (XtNtitleFont = XtFONT_HELVETICA | 24, XtNlabelFont = XtFONT_TIMES | 18, XtNaxisFont = XtFONT_TIMES | 10)
constexpr int kTitleFontHeight = 24;    // For title text
constexpr int kLabelFontHeight = 18;    // For axis labels (X, Y1, Y2, Y3, Y4)
constexpr int kAxisNumberFontHeight = 10;  // For axis tick numbers

QString labelTextOrSpace(const QString &text)
{
  const QString trimmed = text.trimmed();
  if (!trimmed.isEmpty()) {
    return trimmed;
  }
  return QStringLiteral(" ");
}

QColor defaultTraceColor(int index)
{
  Q_UNUSED(index);
  const auto &palette = MedmColors::palette();
  if (palette.size() > kDefaultTraceColorIndex) {
    return palette[kDefaultTraceColorIndex];
  }
  if (!palette.empty()) {
    return palette.back();
  }
  return QColor(Qt::black);
}

} // namespace

// Nice axis range computation matching MEDM's SciPlot ComputeAxis algorithm
CartesianPlotElement::NiceAxisRange CartesianPlotElement::computeNiceAxisRange(
    double min, double max, bool isLog)
{
  NiceAxisRange result;
  
  // Delta values for nice tick intervals (from MEDM's CAdeltas)
  static const double deltas[] = {0.2, 0.5, 1.0, 2.0, 5.0};
  static const int minors[] = {4, 5, 4, 4, 5};
  static const int maxMajor = 8;

  const double minMag = std::fabs(min);
  const double maxMag = std::fabs(max);
  double mag = std::max(minMag, maxMag);
  double range = max - min;
  double relRange = (mag > 0.0) ? range / mag : range;
  
  if (isLog && min > 0 && max > 0) {
    // Logarithmic axis
    double drawMin = 0.0;
    double drawMax = 0.0;
    if (relRange < std::numeric_limits<double>::epsilon()) {
      drawMin = (min > 0.0) ? std::pow(10.0, std::floor(std::log10(min))) : 1.0;
      drawMax = drawMin * 10.0;
    } else {
      const double logMin = std::log10(min);
      const double logMax = std::log10(max);
      drawMin = std::pow(10.0, std::floor(logMin));
      drawMax = std::pow(10.0, std::ceil(logMax));
    }
    result.drawMin = drawMin;
    result.drawMax = drawMax;
    result.majorInc = 10.0;
    result.numMajor = static_cast<int>(std::log10(result.drawMax / result.drawMin) + 0.0001);
    result.numMinor = 10;
    return result;
  }

  // Linear axis, match SciPlot's degenerate handling
  if (relRange < std::numeric_limits<double>::epsilon()) {
    const double halfRange = (mag > 0.0) ? mag * 0.02 : 0.5;
    min -= halfRange;
    max += halfRange;
    range = max - min;
    mag = std::max(std::fabs(min), std::fabs(max));
  }
  
  // Normalize the range to determine nice delta
  const int exponent = static_cast<int>(std::floor(std::log10(range)));
  const double normalizedRange = range / std::pow(10.0, exponent);
  
  // Find appropriate delta from the deltas array
  double delta = deltas[0];
  int minorNum = minors[0];
  for (size_t i = 0; i < sizeof(deltas) / sizeof(deltas[0]); ++i) {
    const int majorNum = static_cast<int>(std::ceil(normalizedRange / deltas[i]));
    if (majorNum <= maxMajor) {
      delta = deltas[i];
      minorNum = minors[i];
      break;
    }
  }
  delta *= std::pow(10.0, exponent);
  
  // Round min down and max up to nearest delta
  result.drawMin = std::floor(min / delta) * delta;
  result.drawMax = std::ceil(max / delta) * delta;
  result.majorInc = delta;
  result.numMajor = static_cast<int>(std::floor((result.drawMax - result.drawMin) / delta + 0.5));
  result.numMinor = minorNum;
  
  return result;
}

CartesianPlotElement::CartesianPlotElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  setContextMenuPolicy(Qt::NoContextMenu);  // Handle right-clicks in mousePressEvent
  setMouseTracking(true);  // Enable mouse tracking for panning
  title_ = QString();  // Empty by default - only shown if specified in ADL
  xLabel_ = QString();  // Empty by default - only shown if specified in ADL
  yLabels_[0] = QString();  // Empty by default
  yLabels_[1] = QString();  // Empty by default
  yLabels_[2] = QString();  // Empty by default
  yLabels_[3] = QString();  // Empty by default
  for (int i = 0; i < traceCount(); ++i) {
    traces_[i].color = defaultTraceColor(i);
  }
  for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
    axisStyles_[axis] = CartesianPlotAxisStyle::kLinear;
    axisRangeStyles_[axis] = CartesianPlotRangeStyle::kChannel;
    axisMinimums_[axis] = 0.0;
    axisMaximums_[axis] = 1.0;
    axisTimeFormats_[axis] = CartesianPlotTimeFormat::kHhMmSs;
    zoomMinimums_[axis] = 0.0;
    zoomMaximums_[axis] = 1.0;
  }
}

void CartesianPlotElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool CartesianPlotElement::isSelected() const
{
  return selected_;
}

QColor CartesianPlotElement::foregroundColor() const
{
  return foregroundColor_;
}

void CartesianPlotElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor CartesianPlotElement::backgroundColor() const
{
  return backgroundColor_;
}

void CartesianPlotElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

QString CartesianPlotElement::title() const
{
  return title_;
}

void CartesianPlotElement::setTitle(const QString &title)
{
  if (title_ == title) {
    return;
  }
  title_ = title;
  update();
}

QString CartesianPlotElement::xLabel() const
{
  return xLabel_;
}

void CartesianPlotElement::setXLabel(const QString &label)
{
  if (xLabel_ == label) {
    return;
  }
  xLabel_ = label;
  update();
}

QString CartesianPlotElement::yLabel(int index) const
{
  if (index < 0 || index >= static_cast<int>(yLabels_.size())) {
    return QString();
  }
  return yLabels_[index];
}

void CartesianPlotElement::setYLabel(int index, const QString &label)
{
  if (index < 0 || index >= static_cast<int>(yLabels_.size())) {
    return;
  }
  if (yLabels_[index] == label) {
    return;
  }
  yLabels_[index] = label;
  update();
}

CartesianPlotStyle CartesianPlotElement::style() const
{
  return style_;
}

void CartesianPlotElement::setStyle(CartesianPlotStyle style)
{
  if (style_ == style) {
    return;
  }
  style_ = style;
  update();
}

bool CartesianPlotElement::eraseOldest() const
{
  return eraseOldest_;
}

void CartesianPlotElement::setEraseOldest(bool eraseOldest)
{
  if (eraseOldest_ == eraseOldest) {
    return;
  }
  eraseOldest_ = eraseOldest;
  update();
}

int CartesianPlotElement::count() const
{
  return count_;
}

void CartesianPlotElement::setCount(int count)
{
  const int clamped = std::clamp(count, 0, kMaximumSampleCount);
  if (count_ == clamped) {
    return;
  }
  count_ = clamped;
  if (executeMode_) {
    clearRuntimeState();
  }
  update();
}

CartesianPlotEraseMode CartesianPlotElement::eraseMode() const
{
  return eraseMode_;
}

void CartesianPlotElement::setEraseMode(CartesianPlotEraseMode mode)
{
  if (eraseMode_ == mode) {
    return;
  }
  eraseMode_ = mode;
  update();
}

QString CartesianPlotElement::triggerChannel() const
{
  return triggerChannel_;
}

void CartesianPlotElement::setTriggerChannel(const QString &channel)
{
  if (triggerChannel_ == channel) {
    return;
  }
  triggerChannel_ = channel;
  update();
}

QString CartesianPlotElement::eraseChannel() const
{
  return eraseChannel_;
}

void CartesianPlotElement::setEraseChannel(const QString &channel)
{
  if (eraseChannel_ == channel) {
    return;
  }
  eraseChannel_ = channel;
  update();
}

QString CartesianPlotElement::countChannel() const
{
  return countChannel_;
}

void CartesianPlotElement::setCountChannel(const QString &channel)
{
  if (countChannel_ == channel) {
    return;
  }
  countChannel_ = channel;
  update();
}

int CartesianPlotElement::traceCount() const
{
  return static_cast<int>(traces_.size());
}

QString CartesianPlotElement::traceXChannel(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return QString();
  }
  return traces_[index].xChannel;
}

void CartesianPlotElement::setTraceXChannel(int index, const QString &channel)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].xChannel == channel) {
    return;
  }
  traces_[index].xChannel = channel;
  update();
}

QString CartesianPlotElement::traceYChannel(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return QString();
  }
  return traces_[index].yChannel;
}

void CartesianPlotElement::setTraceYChannel(int index, const QString &channel)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].yChannel == channel) {
    return;
  }
  traces_[index].yChannel = channel;
  update();
}

QColor CartesianPlotElement::traceColor(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return QColor();
  }
  return traces_[index].color;
}

void CartesianPlotElement::setTraceColor(int index, const QColor &color)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].color == color) {
    return;
  }
  traces_[index].color = color;
  update();
}

CartesianPlotYAxis CartesianPlotElement::traceYAxis(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return CartesianPlotYAxis::kY1;
  }
  return traces_[index].yAxis;
}

void CartesianPlotElement::setTraceYAxis(int index, CartesianPlotYAxis axis)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].yAxis == axis) {
    return;
  }
  traces_[index].yAxis = axis;
  update();
}

bool CartesianPlotElement::traceUsesRightAxis(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return false;
  }
  return traces_[index].usesRightAxis;
}

void CartesianPlotElement::setTraceUsesRightAxis(int index, bool usesRightAxis)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].usesRightAxis == usesRightAxis) {
    return;
  }
  traces_[index].usesRightAxis = usesRightAxis;
  update();
}

CartesianPlotAxisStyle CartesianPlotElement::axisStyle(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return CartesianPlotAxisStyle::kLinear;
  }
  return axisStyles_[axisIndex];
}

void CartesianPlotElement::setAxisStyle(int axisIndex,
    CartesianPlotAxisStyle style)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisStyles_[axisIndex] == style) {
    return;
  }
  axisStyles_[axisIndex] = style;
  update();
}

CartesianPlotRangeStyle CartesianPlotElement::axisRangeStyle(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return CartesianPlotRangeStyle::kChannel;
  }
  return axisRangeStyles_[axisIndex];
}

void CartesianPlotElement::setAxisRangeStyle(int axisIndex,
    CartesianPlotRangeStyle style)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisRangeStyles_[axisIndex] == style) {
    return;
  }
  axisRangeStyles_[axisIndex] = style;
  update();
}

double CartesianPlotElement::axisMinimum(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return 0.0;
  }
  return axisMinimums_[axisIndex];
}

void CartesianPlotElement::setAxisMinimum(int axisIndex, double value)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisMinimums_[axisIndex] == value) {
    return;
  }
  axisMinimums_[axisIndex] = value;
  
  // Also update runtime minimum if in execute mode
  if (executeMode_) {
    axisRuntimeMinimums_[axisIndex] = value;
    axisRuntimeValid_[axisIndex] = true;
  }
  
  update();
}

double CartesianPlotElement::axisMaximum(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return 1.0;
  }
  return axisMaximums_[axisIndex];
}

void CartesianPlotElement::setAxisMaximum(int axisIndex, double value)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisMaximums_[axisIndex] == value) {
    return;
  }
  axisMaximums_[axisIndex] = value;
  
  // Also update runtime maximum if in execute mode
  if (executeMode_) {
    axisRuntimeMaximums_[axisIndex] = value;
    axisRuntimeValid_[axisIndex] = true;
  }
  
  update();
}

CartesianPlotTimeFormat CartesianPlotElement::axisTimeFormat(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return CartesianPlotTimeFormat::kHhMmSs;
  }
  return axisTimeFormats_[axisIndex];
}

void CartesianPlotElement::setAxisTimeFormat(int axisIndex,
    CartesianPlotTimeFormat format)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisTimeFormats_[axisIndex] == format) {
    return;
  }
  axisTimeFormats_[axisIndex] = format;
  update();
}

QString CartesianPlotElement::axisLabel(int axisIndex) const
{
  if (axisIndex == 0) {
    return xLabel_;
  }
  const int yIndex = axisIndex - 1;
  if (yIndex < 0 || yIndex >= static_cast<int>(yLabels_.size())) {
    return {};
  }
  return yLabels_[yIndex];
}

bool CartesianPlotElement::isAxisDrawnOnLeft(int axisIndex) const
{
  if (axisIndex == 0) {
    return true;
  }
  return !isYAxisOnRight(axisIndex - 1);
}

void CartesianPlotElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (!execute) {
    // Reset zoom state when leaving execute mode
    zoomed_ = false;
    panning_ = false;
  }
  clearRuntimeState();
  update();
}

bool CartesianPlotElement::isExecuteMode() const
{
  return executeMode_;
}

void CartesianPlotElement::setTraceRuntimeMode(int index,
    CartesianPlotTraceMode mode)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  traces_[index].runtimeMode = mode;
}

void CartesianPlotElement::setTraceRuntimeConnected(int index, bool connected)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].runtimeConnected == connected) {
    return;
  }
  traces_[index].runtimeConnected = connected;
  update();
}

void CartesianPlotElement::updateTraceRuntimeData(int index,
    QVector<QPointF> points)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  traces_[index].runtimePoints = std::move(points);
  update();
}

void CartesianPlotElement::clearTraceRuntimeData(int index)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].runtimePoints.isEmpty()) {
    return;
  }
  traces_[index].runtimePoints.clear();
  update();
}

void CartesianPlotElement::clearRuntimeState()
{
  runtimeCountValid_ = false;
  runtimeCount_ = 0;
  axisRuntimeValid_.fill(false);
  axisRuntimeMinimums_.fill(0.0);
  axisRuntimeMaximums_.fill(0.0);
  cachedAxisRangesValid_ = false;
  for (Trace &trace : traces_) {
    trace.runtimePoints.clear();
    trace.runtimeConnected = false;
    trace.runtimeMode = CartesianPlotTraceMode::kNone;
  }
  update();
}

void CartesianPlotElement::setRuntimeCount(int count)
{
  if (count <= 0) {
    if (!runtimeCountValid_) {
      return;
    }
    runtimeCountValid_ = false;
    runtimeCount_ = 0;
    update();
    return;
  }
  const int clamped = std::clamp(count, 1, kMaximumSampleCount);
  if (runtimeCountValid_ && runtimeCount_ == clamped) {
    return;
  }
  runtimeCountValid_ = true;
  runtimeCount_ = clamped;
  update();
}

int CartesianPlotElement::effectiveSampleCapacity() const
{
  if (runtimeCountValid_) {
    return runtimeCount_;
  }
  if (count_ > 0) {
    return std::clamp(count_, 1, kMaximumSampleCount);
  }
  return kMaximumSampleCount;
}

void CartesianPlotElement::setAxisRuntimeLimits(int axisIndex,
    double minimum, double maximum, bool valid)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (!valid || !std::isfinite(minimum) || !std::isfinite(maximum)
      || maximum < minimum) {
    if (!axisRuntimeValid_[axisIndex]) {
      return;
    }
    axisRuntimeValid_[axisIndex] = false;
    update();
    return;
  }
  if (axisRuntimeValid_[axisIndex]
      && qFuzzyCompare(axisRuntimeMinimums_[axisIndex], minimum)
      && qFuzzyCompare(axisRuntimeMaximums_[axisIndex], maximum)) {
    return;
  }
  axisRuntimeValid_[axisIndex] = true;
  axisRuntimeMinimums_[axisIndex] = minimum;
  axisRuntimeMaximums_[axisIndex] = maximum;
  update();
}

int CartesianPlotElement::dataPointCount(int traceIndex) const
{
  if (traceIndex < 0 || traceIndex >= traceCount()) {
    return 0;
  }
  return traces_[traceIndex].runtimePoints.size();
}

QPointF CartesianPlotElement::dataPoint(int traceIndex, int pointIndex) const
{
  if (traceIndex < 0 || traceIndex >= traceCount()) {
    return QPointF();
  }
  const auto &points = traces_[traceIndex].runtimePoints;
  if (pointIndex < 0 || pointIndex >= points.size()) {
    return QPointF();
  }
  return points[pointIndex];
}

bool CartesianPlotElement::traceHasData(int traceIndex) const
{
  if (traceIndex < 0 || traceIndex >= traceCount()) {
    return false;
  }
  return !traces_[traceIndex].runtimePoints.isEmpty();
}

bool CartesianPlotElement::drawMajorGrid() const
{
  return drawMajorGrid_;
}

void CartesianPlotElement::setDrawMajorGrid(bool draw)
{
  if (drawMajorGrid_ == draw) {
    return;
  }
  drawMajorGrid_ = draw;
  update();
}

bool CartesianPlotElement::drawMinorGrid() const
{
  return drawMinorGrid_;
}

void CartesianPlotElement::setDrawMinorGrid(bool draw)
{
  if (drawMinorGrid_ == draw) {
    return;
  }
  drawMinorGrid_ = draw;
  update();
}

void CartesianPlotElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  painter.fillRect(rect(), effectiveBackground());

  // In execute mode, if any PV is not connected or no PVs are defined, fill with solid white
  if (executeMode_) {
    bool anyChannelsDefined = false;
    bool anyDisconnected = false;
    
    for (const auto &trace : traces_) {
      // Check if this trace has any channels configured
      const bool hasChannels = !trace.xChannel.trimmed().isEmpty() || 
                               !trace.yChannel.trimmed().isEmpty();
      if (hasChannels) {
        anyChannelsDefined = true;
        if (!trace.runtimeConnected) {
          anyDisconnected = true;
          break;
        }
      }
    }
    
    // Draw white if no channels defined or any are disconnected
    if (!anyChannelsDefined || anyDisconnected) {
      painter.fillRect(rect(), Qt::white);
      return;  // Don't draw anything else
    }
  }

  const QRectF chart = chartRect();
  paintFrame(painter);
  paintGrid(painter, chart);
  paintTraces(painter, chart);
  paintAxes(painter, chart);
  paintLabels(painter, chart);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

void CartesianPlotElement::mousePressEvent(QMouseEvent *event)
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
    if (event->button() == Qt::LeftButton) {
      // Start panning
      const QRectF chart = chartRect();
      if (chart.contains(event->pos())) {
        panning_ = true;
        panStartPos_ = event->pos();
        // Initialize zoom state if not already zoomed
        if (!zoomed_) {
          for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
            if (cachedAxisRangesValid_) {
              zoomMinimums_[axis] = cachedAxisRanges_[axis].minimum;
              zoomMaximums_[axis] = cachedAxisRanges_[axis].maximum;
            }
          }
        }
        for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
          panStartMinimums_[axis] = zoomMinimums_[axis];
          panStartMaximums_[axis] = zoomMaximums_[axis];
        }
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

void CartesianPlotElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (panning_ && event->button() == Qt::LeftButton) {
    panning_ = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void CartesianPlotElement::mouseMoveEvent(QMouseEvent *event)
{
  if (panning_ && executeMode_) {
    const QRectF chart = chartRect();
    if (chart.width() <= 0 || chart.height() <= 0) {
      return;
    }
    
    const QPointF delta = event->pos() - panStartPos_;
    
    // Convert pixel delta to data delta for each axis
    // X axis (index 0)
    {
      const double xRange = panStartMaximums_[0] - panStartMinimums_[0];
      const double xDelta = -delta.x() * xRange / chart.width();
      zoomMinimums_[0] = panStartMinimums_[0] + xDelta;
      zoomMaximums_[0] = panStartMaximums_[0] + xDelta;
    }
    
    // Y axes - apply same delta to all visible Y axes
    for (int axis = 1; axis < kCartesianAxisCount; ++axis) {
      if (!isYAxisVisible(axis - 1)) {
        continue;
      }
      const double yRange = panStartMaximums_[axis] - panStartMinimums_[axis];
      const double yDelta = delta.y() * yRange / chart.height();  // Y is inverted
      zoomMinimums_[axis] = panStartMinimums_[axis] + yDelta;
      zoomMaximums_[axis] = panStartMaximums_[axis] + yDelta;
    }
    
    zoomed_ = true;
    cachedAxisRangesValid_ = false;
    update();
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void CartesianPlotElement::wheelEvent(QWheelEvent *event)
{
  if (!executeMode_) {
    QWidget::wheelEvent(event);
    return;
  }
  
  const QRectF chart = chartRect();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->posF();
#endif
  
  if (!chart.contains(pos)) {
    QWidget::wheelEvent(event);
    return;
  }
  
  // Initialize zoom state from current display if not already zoomed
  if (!zoomed_) {
    for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
      if (cachedAxisRangesValid_) {
        zoomMinimums_[axis] = cachedAxisRanges_[axis].minimum;
        zoomMaximums_[axis] = cachedAxisRanges_[axis].maximum;
      }
    }
  }
  
  // Calculate zoom factor (positive delta = zoom in)
  const double degrees = event->angleDelta().y() / 8.0;
  const double steps = degrees / 15.0;
  const double factor = std::pow(0.9, steps);  // 0.9 = zoom in, 1.11 = zoom out
  
  // Determine which axes to zoom based on modifiers
  const bool zoomX = !event->modifiers().testFlag(Qt::ControlModifier);
  const bool zoomY = !event->modifiers().testFlag(Qt::ShiftModifier);
  
  // Calculate mouse position in chart coordinates (0-1 range)
  const double chartX = (pos.x() - chart.left()) / chart.width();
  const double chartY = 1.0 - (pos.y() - chart.top()) / chart.height();  // Y inverted
  
  // Zoom X axis centered on mouse position
  if (zoomX) {
    const double xMin = zoomMinimums_[0];
    const double xMax = zoomMaximums_[0];
    const double xCenter = xMin + chartX * (xMax - xMin);
    zoomAxis(0, factor, xCenter);
  }
  
  // Zoom Y axes centered on mouse position
  if (zoomY) {
    for (int axis = 1; axis < kCartesianAxisCount; ++axis) {
      if (!isYAxisVisible(axis - 1)) {
        continue;
      }
      const double yMin = zoomMinimums_[axis];
      const double yMax = zoomMaximums_[axis];
      const double yCenter = yMin + chartY * (yMax - yMin);
      zoomAxis(axis, factor, yCenter);
    }
  }
  
  zoomed_ = true;
  cachedAxisRangesValid_ = false;
  update();
  event->accept();
}

void CartesianPlotElement::zoomAxis(int axisIndex, double factor, double center)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  
  const double oldMin = zoomMinimums_[axisIndex];
  const double oldMax = zoomMaximums_[axisIndex];
  const double oldRange = oldMax - oldMin;
  const double newRange = oldRange * factor;
  
  // Keep center point at same position
  const double centerRatio = (center - oldMin) / oldRange;
  zoomMinimums_[axisIndex] = center - centerRatio * newRange;
  zoomMaximums_[axisIndex] = center + (1.0 - centerRatio) * newRange;
}

bool CartesianPlotElement::isZoomed() const
{
  return zoomed_;
}

void CartesianPlotElement::resetZoom()
{
  zoomed_ = false;
  panning_ = false;
  cachedAxisRangesValid_ = false;
  update();
}

void CartesianPlotElement::applyZoomToRange(AxisRange &range, int axisIndex) const
{
  if (!zoomed_ || axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  range.minimum = zoomMinimums_[axisIndex];
  range.maximum = zoomMaximums_[axisIndex];
}

bool CartesianPlotElement::event(QEvent *event)
{
  return QWidget::event(event);
}

bool CartesianPlotElement::forwardMouseEventToParent(QMouseEvent *event) const
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

QColor CartesianPlotElement::effectiveForeground() const
{
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return palette().color(QPalette::WindowText);
}

QColor CartesianPlotElement::effectiveBackground() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return palette().color(QPalette::Window);
}

QColor CartesianPlotElement::effectiveTraceColor(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return effectiveForeground();
  }
  const QColor color = traces_[index].color;
  if (color.isValid()) {
    return color;
  }
  return defaultTraceColor(index);
}

QRectF CartesianPlotElement::chartRect() const
{
  QRectF frame = rect().adjusted(kOuterMargin, kOuterMargin,
      -kOuterMargin, -kOuterMargin);
  if (frame.width() <= 0.0 || frame.height() <= 0.0) {
    return frame;
  }

  const QFont titleFont = medmTextFieldFont(kTitleFontHeight);
  const QFont labelFont = medmTextFieldFont(kLabelFontHeight);
  const QFont axisFont = medmTextFieldFont(kAxisNumberFontHeight);
  
  const QFontMetrics titleMetrics(titleFont);
  const QFontMetrics labelMetrics(labelFont);
  const QFontMetrics axisMetrics(axisFont);
  
  qreal topMargin = kInnerMargin;

  qreal bottomMargin = kInnerMargin;
  // Title appears at bottom in medm
  if (!title_.trimmed().isEmpty()) {
    bottomMargin += titleMetrics.height();
  }
  // X label below the axis numbers
  if (!xLabel_.trimmed().isEmpty()) {
    const qreal axisnumbersize = kInnerMargin + axisMetrics.height();
    bottomMargin += axisnumbersize + labelMetrics.height();
  } else {
    // Just the axis numbers
    bottomMargin += kInnerMargin + axisMetrics.height();
  }

  qreal leftMargin = kHorizontalMargin;
  qreal rightMargin = kHorizontalMargin;
  
  // Calculate Y-axis positions to determine actual margins needed
  const YAxisPositions axisPos = calculateYAxisPositions(frame);
  
  // Expand the chart area to the innermost Y-axis position on each side
  if (!axisPos.leftAxes.empty()) {
    const qreal innermostAxis = axisPos.leftAxes.back().second;
    leftMargin = innermostAxis - frame.left();
  }
  
  if (!axisPos.rightAxes.empty()) {
    const qreal innermostAxis = axisPos.rightAxes.back().second;
    rightMargin = frame.right() - innermostAxis;
  }

  frame.adjust(leftMargin, topMargin, -rightMargin, -bottomMargin);
  return frame;
}

CartesianPlotElement::YAxisPositions 
CartesianPlotElement::calculateYAxisPositions(const QRectF &widgetBounds) const
{
  YAxisPositions positions;
  
  const QFont axisFont = medmTextFieldFont(kAxisNumberFontHeight);
  const QFontMetrics axisMetrics(axisFont);
  const QFont labelFont = medmTextFieldFont(kLabelFontHeight);
  const QFontMetrics labelMetrics(labelFont);
  
  const qreal axisnumberwidth = axisMetrics.horizontalAdvance(QStringLiteral("0.88"));
  const qreal axisSpacing = axisnumberwidth + kHorizontalMargin;
  const qreal labelGap = 1.0;  // Gap between axis numbers and labels
  
  // Start from the widget edges (after outer margin) and work inward
  // Reserve space at edges for the outermost labels
  // Add extra space for the outermost label to prevent crowding
  qreal leftX = widgetBounds.left() + kHorizontalMargin * 1.5;
  qreal rightX = widgetBounds.right() - kHorizontalMargin * 1.5;
  
  // Process axes in reverse order (Y4, Y3, Y2, Y1) like MEDM
  for (int i = 3; i >= 0; --i) {
    if (!isYAxisVisible(i)) {
      continue;
    }
    
    if (isYAxisOnRight(i)) {
      // Always reserve label clearance when the axis is shown
      rightX -= labelMetrics.height() + labelGap;
      // Position the axis line itself
      positions.rightAxes.push_back({i, rightX});
      // Reserve space for this axis's numbers and move inward
      rightX -= axisSpacing;
    } else {
      // Always reserve label clearance when the axis is shown
      leftX += labelMetrics.height() + labelGap;
      // Position the axis line itself
      positions.leftAxes.push_back({i, leftX});
      // Reserve space for this axis's numbers and move inward
      leftX += axisSpacing;
    }
  }
  
  return positions;
}

void CartesianPlotElement::paintFrame(QPainter &painter) const
{
  const QRect outerRect = rect();
  
  // Draw 3D raised shadow border (mimicking medm's XmSHADOW_OUT)
  const int shadowThickness = 2;
  
  // Get light and dark shadow colors from the background
  const QColor bg = effectiveBackground();
  const QColor lightShadow = bg.lighter(150);
  const QColor darkShadow = bg.darker(150);
  
  // Draw shadow lines (top and left are light, bottom and right are dark)
  for (int i = 0; i < shadowThickness; ++i) {
    // Light shadow on top and left (raised effect)
    painter.setPen(QPen(lightShadow, 1));
    // Top edge
    painter.drawLine(outerRect.left() + i, outerRect.top() + i,
                     outerRect.right() - i, outerRect.top() + i);
    // Left edge
    painter.drawLine(outerRect.left() + i, outerRect.top() + i,
                     outerRect.left() + i, outerRect.bottom() - i);
    
    // Dark shadow on bottom and right
    painter.setPen(QPen(darkShadow, 1));
    // Bottom edge
    painter.drawLine(outerRect.left() + i, outerRect.bottom() - i,
                     outerRect.right() - i, outerRect.bottom() - i);
    // Right edge
    painter.drawLine(outerRect.right() - i, outerRect.top() + i,
                     outerRect.right() - i, outerRect.bottom() - i);
  }
}

void CartesianPlotElement::paintGrid(QPainter &painter, const QRectF &rect) const
{
  if (rect.width() <= 0.0 || rect.height() <= 0.0) {
    return;
  }
  
  // Only draw grid if at least one option is enabled
  if (!drawMajorGrid_ && !drawMinorGrid_) {
    return;
  }
  
  const int numMajorDivisions = 5;
  const int numMinorDivisions = 4; // minor divisions between each major division
  
  // Draw minor grid lines if enabled
  if (drawMinorGrid_) {
    QColor minorGridColor = effectiveForeground();
    minorGridColor.setAlpha(40); // More subtle than major grid
    QPen minorPen(minorGridColor);
    minorPen.setStyle(Qt::DotLine);
    minorPen.setWidth(1);
    painter.setPen(minorPen);
    
    // Vertical minor grid lines
    for (int i = 0; i < numMajorDivisions; ++i) {
      for (int j = 1; j <= numMinorDivisions; ++j) {
        const qreal x = rect.left() + (i + j / static_cast<qreal>(numMinorDivisions + 1)) 
                        * rect.width() / numMajorDivisions;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
      }
    }
    
    // Horizontal minor grid lines
    for (int i = 0; i < numMajorDivisions; ++i) {
      for (int j = 1; j <= numMinorDivisions; ++j) {
        const qreal y = rect.top() + (i + j / static_cast<qreal>(numMinorDivisions + 1)) 
                        * rect.height() / numMajorDivisions;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
      }
    }
  }
  
  // Draw major grid lines if enabled
  if (drawMajorGrid_) {
    QColor majorGridColor = effectiveForeground();
    majorGridColor.setAlpha(80);
    QPen majorPen(majorGridColor);
    majorPen.setStyle(Qt::DotLine);
    majorPen.setWidth(1);
    painter.setPen(majorPen);
    
    // Vertical major grid lines (skip first and last as they're on the axes)
    for (int i = 1; i < numMajorDivisions; ++i) {
      const qreal x = rect.left() + i * rect.width() / numMajorDivisions;
      painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    
    // Horizontal major grid lines (skip first and last as they're on the axes)
    for (int j = 1; j < numMajorDivisions; ++j) {
      const qreal y = rect.top() + j * rect.height() / numMajorDivisions;
      painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
  }
}

void CartesianPlotElement::paintAxes(QPainter &painter, const QRectF &rect) const
{
  if (rect.width() <= 0.0 || rect.height() <= 0.0) {
    return;
  }
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  const QFont axisFont = medmTextFieldFont(kAxisNumberFontHeight);
  const QFontMetrics axisMetrics(axisFont);
  const bool useCachedRanges = executeMode_ && cachedAxisRangesValid_;
  
  // Get pre-calculated axis positions (working from widget edges inward)
  const QRectF widgetBounds = this->rect().adjusted(kOuterMargin, kOuterMargin,
      -kOuterMargin, -kOuterMargin);
  const YAxisPositions axisPos = calculateYAxisPositions(widgetBounds);
  
  // Draw all left-side Y-axes
  for (const auto &[axisIndex, xPosition] : axisPos.leftAxes) {
    const int arrayIndex = axisIndex + 1;
    const AxisRange *precomputed = (useCachedRanges
        && arrayIndex >= 0 && arrayIndex < kCartesianAxisCount)
        ? &cachedAxisRanges_[arrayIndex]
        : nullptr;
    paintYAxis(painter, rect, axisIndex, xPosition, true, precomputed);
  }
  
  // Draw all right-side Y-axes
  for (const auto &[axisIndex, xPosition] : axisPos.rightAxes) {
    const int arrayIndex = axisIndex + 1;
    const AxisRange *precomputed = (useCachedRanges
        && arrayIndex >= 0 && arrayIndex < kCartesianAxisCount)
        ? &cachedAxisRanges_[arrayIndex]
        : nullptr;
    paintYAxis(painter, rect, axisIndex, xPosition, false, precomputed);
  }

  // Draw X-axis (bottom)
  painter.setFont(axisFont);
  const int majorTickSize = 4;
  const int minorTickSize = 2;
  const int numMajorTicks = 5;
  const int numMinorTicks = 4;
  
  // Determine X-axis extent: extend to innermost Y-axis on each side if present
  qreal xAxisLeft = rect.left();
  qreal xAxisRight = rect.right();
  
  // Extend to innermost (last) left Y-axis if present
  if (!axisPos.leftAxes.empty()) {
    xAxisLeft = axisPos.leftAxes.back().second;
  }
  
  // Extend to innermost (last) right Y-axis if present
  if (!axisPos.rightAxes.empty()) {
    xAxisRight = axisPos.rightAxes.back().second;
  }
  
  // Bottom horizontal line - extends to innermost Y-axes
  painter.drawLine(QPointF(xAxisLeft, rect.bottom()),
      QPointF(xAxisRight, rect.bottom()));
  
  // Determine X-axis range for value labels
  double xAxisMin = 0.0;
  double xAxisMax = 1.0;
  const int xAxisIndex = 0; // X-axis is index 0
  const AxisRange *xRange = useCachedRanges ? &cachedAxisRanges_[xAxisIndex] : nullptr;
  CartesianPlotAxisStyle xAxisStyle = axisStyles_[xAxisIndex];
  if (xRange && xRange->valid) {
    xAxisMin = xRange->minimum;
    xAxisMax = xRange->maximum;
    xAxisStyle = xRange->style;
  } else if (axisRangeStyles_[xAxisIndex] == CartesianPlotRangeStyle::kUserSpecified) {
    xAxisMin = axisMinimums_[xAxisIndex];
    xAxisMax = axisMaximums_[xAxisIndex];
  } else if (executeMode_ && axisRuntimeValid_[xAxisIndex]) {
    xAxisMin = axisRuntimeMinimums_[xAxisIndex];
    xAxisMax = axisRuntimeMaximums_[xAxisIndex];
  }
  const bool xAxisIsLog10 = (xAxisStyle == CartesianPlotAxisStyle::kLog10);
  
  // X-axis ticks and numbers - only within chart area
  if (xAxisIsLog10 && xAxisMin > 0.0 && xAxisMax > 0.0) {
    // Logarithmic X-axis tick marks
    const NiceAxisRange nice = computeNiceAxisRange(xAxisMin, xAxisMax, true);
    
    double majorValue = nice.drawMin;
    for (int i = 0; i <= nice.numMajor; ++i) {
      // Calculate screen position from log scale
      const double logMin = std::log10(nice.drawMin);
      const double logMax = std::log10(nice.drawMax);
      const double logValue = std::log10(majorValue);
      const double normalizedPos = (logValue - logMin) / (logMax - logMin);
      const qreal x = rect.left() + normalizedPos * rect.width();
      
      // Major tick
      painter.drawLine(QPointF(x, rect.bottom() - majorTickSize),
                       QPointF(x, rect.bottom() + majorTickSize));
      
      // Major tick label
      QString label = QString::number(majorValue, 'g', 3);
      const qreal textWidth = axisMetrics.horizontalAdvance(label);
      const qreal textX = x - textWidth / 2.0;
      const qreal textY = rect.bottom() + majorTickSize + axisMetrics.ascent() + 2.0;
      painter.drawText(QPointF(textX, textY), label);
      
      // Minor ticks (2x, 3x, 4x, 5x, 6x, 7x, 8x, 9x within this decade)
      if (i < nice.numMajor && nice.numMinor > 0) {
        for (int j = 2; j < nice.numMinor + 1; ++j) {
          const double minorValue = majorValue * j;
          if (minorValue < nice.drawMax) {
            const double logMinor = std::log10(minorValue);
            const double minorNormPos = (logMinor - logMin) / (logMax - logMin);
            const qreal minorX = rect.left() + minorNormPos * rect.width();
            painter.drawLine(QPointF(minorX, rect.bottom() - minorTickSize),
                             QPointF(minorX, rect.bottom() + minorTickSize));
          }
        }
      }
      
      // Advance to next decade
      majorValue *= 10.0;
    }
  } else {
    // Linear X-axis tick marks
    for (int i = 0; i <= numMajorTicks; ++i) {
      const qreal x = rect.left() + i * rect.width() / numMajorTicks;
      // Major tick
      painter.drawLine(QPointF(x, rect.bottom() - majorTickSize),
                       QPointF(x, rect.bottom() + majorTickSize));
      
      // Axis number - map from normalized position to actual value
      const double normalizedValue = static_cast<double>(i) / numMajorTicks;
      const double value = xAxisMin + normalizedValue * (xAxisMax - xAxisMin);
      QString label = QString::number(value, 'g', 3);
      const qreal textWidth = axisMetrics.horizontalAdvance(label);
      const qreal textX = x - textWidth / 2.0;
      const qreal textY = rect.bottom() + majorTickSize + axisMetrics.ascent() + 2.0;
      painter.drawText(QPointF(textX, textY), label);
      
      // Minor ticks
      if (i < numMajorTicks) {
        for (int j = 1; j <= numMinorTicks; ++j) {
          const qreal minorX = x + j * rect.width() / (numMajorTicks * (numMinorTicks + 1));
          painter.drawLine(QPointF(minorX, rect.bottom() - minorTickSize),
                           QPointF(minorX, rect.bottom() + minorTickSize));
        }
      }
    }
  }
}

void CartesianPlotElement::paintYAxis(QPainter &painter, const QRectF &rect,
  int yAxisIndex, qreal axisX, bool onLeft,
  const AxisRange *precomputedRange) const
{
  const QFont axisFont = medmTextFieldFont(kAxisNumberFontHeight);
  const QFontMetrics metrics(axisFont);
  painter.setFont(axisFont);
  
  const int majorTickSize = 4;
  const int minorTickSize = 2;
  
  // Convert yAxisIndex (0=Y1, 1=Y2, 2=Y3, 3=Y4) to array index (1=Y1, 2=Y2, 3=Y3, 4=Y4)
  const int arrayIndex = yAxisIndex + 1;
  
  // Determine axis range for value labels
  double axisMin = 0.0;
  double axisMax = 1.0;
  CartesianPlotAxisStyle axisStyle = axisStyles_[arrayIndex];
  if (precomputedRange && precomputedRange->valid) {
    axisMin = precomputedRange->minimum;
    axisMax = precomputedRange->maximum;
    axisStyle = precomputedRange->style;
  } else if (axisRangeStyles_[arrayIndex] == CartesianPlotRangeStyle::kUserSpecified) {
    axisMin = axisMinimums_[arrayIndex];
    axisMax = axisMaximums_[arrayIndex];
  } else if (executeMode_ && axisRuntimeValid_[arrayIndex]) {
    axisMin = axisRuntimeMinimums_[arrayIndex];
    axisMax = axisRuntimeMaximums_[arrayIndex];
  }
  
  // Draw the vertical axis line
  painter.drawLine(QPointF(axisX, rect.top()),
                   QPointF(axisX, rect.bottom()));
  
  // Check if this is a Log10 axis
  const bool isLog10 = (axisStyle == CartesianPlotAxisStyle::kLog10);
  
  // Compute "nice" axis range matching MEDM's behavior
  const NiceAxisRange nice = computeNiceAxisRange(axisMin, axisMax, isLog10);
  
  // Draw ticks and numbers
  if (isLog10 && nice.drawMin > 0 && nice.drawMax > 0) {
    // Logarithmic axis - match MEDM's tick drawing behavior
    const double logMin = std::log10(nice.drawMin);
    const double logMax = std::log10(nice.drawMax);
    
    // Draw first major tick at drawMin
    {
      const qreal y = rect.bottom();  // At the bottom (min value)
      
      // Major tick
      painter.drawLine(QPointF(axisX - majorTickSize, y),
                       QPointF(axisX + majorTickSize, y));
      
      // Axis number
      QString label = QString::number(nice.drawMin, 'g', 3);
      const qreal textWidth = metrics.horizontalAdvance(label);
      
      if (onLeft) {
        const qreal textX = axisX - majorTickSize - textWidth - 2.0;
        const qreal textY = y + metrics.ascent() / 2.0;
        painter.drawText(QPointF(textX, textY), label);
      } else {
        const qreal textX = axisX + majorTickSize + 2.0;
        const qreal textY = y + metrics.ascent() / 2.0;
        painter.drawText(QPointF(textX, textY), label);
      }
    }
    
    // Start with the first major value (drawMin)
    double majorValue = nice.drawMin;
    
    // Draw ticks for each decade
    for (int i = 0; i < nice.numMajor; ++i) {
      // Draw minor ticks at 2x, 3x, 4x, 5x, 6x, 7x, 8x, 9x within this decade
      // (matching MEDM's: for (j = 2; j < num_y_minor; j++))
      for (int j = 2; j < nice.numMinor; ++j) {
        const double minorValue = majorValue * j;
        if (minorValue <= nice.drawMax) {
          const double minorLogValue = std::log10(minorValue);
          const double minorNormalizedLog = (minorLogValue - logMin) / (logMax - logMin);
          const qreal minorY = rect.bottom() - minorNormalizedLog * rect.height();
          painter.drawLine(QPointF(axisX - minorTickSize, minorY),
                           QPointF(axisX + minorTickSize, minorY));
        }
      }
      
      // Advance to next major value (multiply by 10)
      majorValue *= nice.majorInc;
      
      // Draw major tick and label at this position
      if (majorValue <= nice.drawMax * 1.0001) {  // Small tolerance for floating point
        const double logValue = std::log10(majorValue);
        const double normalizedLog = (logValue - logMin) / (logMax - logMin);
        const qreal y = rect.bottom() - normalizedLog * rect.height();
        
        // Major tick
        painter.drawLine(QPointF(axisX - majorTickSize, y),
                         QPointF(axisX + majorTickSize, y));
        
        // Axis number
        QString label = QString::number(majorValue, 'g', 3);
        const qreal textWidth = metrics.horizontalAdvance(label);
        
        if (onLeft) {
          const qreal textX = axisX - majorTickSize - textWidth - 2.0;
          const qreal textY = y + metrics.ascent() / 2.0;
          painter.drawText(QPointF(textX, textY), label);
        } else {
          const qreal textX = axisX + majorTickSize + 2.0;
          const qreal textY = y + metrics.ascent() / 2.0;
          painter.drawText(QPointF(textX, textY), label);
        }
      }
    }
  } else {
    // Linear axis - use nice axis values for tick labels
    for (int i = 0; i <= nice.numMajor; ++i) {
      const double value = nice.drawMin + i * nice.majorInc;
      
      // Position based on nice range
      const double normalized = (value - nice.drawMin) / (nice.drawMax - nice.drawMin);
      const qreal y = rect.bottom() - normalized * rect.height();
      
      // Major tick
      painter.drawLine(QPointF(axisX - majorTickSize, y),
                       QPointF(axisX + majorTickSize, y));
      
      // Axis number
      QString label = QString::number(value, 'g', 3);
      const qreal textWidth = metrics.horizontalAdvance(label);
      
      if (onLeft) {
        const qreal textX = axisX - majorTickSize - textWidth - 2.0;
        const qreal textY = y + metrics.ascent() / 2.0;
        painter.drawText(QPointF(textX, textY), label);
      } else {
        const qreal textX = axisX + majorTickSize + 2.0;
        const qreal textY = y + metrics.ascent() / 2.0;
        painter.drawText(QPointF(textX, textY), label);
      }
      
      // Minor ticks
      if (i < nice.numMajor) {
        for (int j = 1; j <= nice.numMinor; ++j) {
          const double minorValue = value + j * nice.majorInc / (nice.numMinor + 1);
          const double minorNormalized = (minorValue - nice.drawMin) / (nice.drawMax - nice.drawMin);
          const qreal minorY = rect.bottom() - minorNormalized * rect.height();
          painter.drawLine(QPointF(axisX - minorTickSize, minorY),
                           QPointF(axisX + minorTickSize, minorY));
        }
      }
    }
  }
}

void CartesianPlotElement::paintLabels(QPainter &painter, const QRectF &rect) const
{
  painter.save();
  painter.setPen(effectiveForeground());
  
  const QFont titleFont = medmTextFieldFont(kTitleFontHeight);
  const QFont labelFont = medmTextFieldFont(kLabelFontHeight);
  const QFont axisFont = medmTextFieldFont(kAxisNumberFontHeight);
  const QFontMetrics titleMetrics(titleFont);
  const QFontMetrics labelMetrics(labelFont);
  const QFontMetrics axisMetrics(axisFont);

  // Title positioning: medm places title at bottom-left corner
  // y.TitlePos = height - ymargin, x.TitlePos = xmargin
  if (!title_.trimmed().isEmpty()) {
    painter.setFont(titleFont);
    const qreal titleX = kOuterMargin + kHorizontalMargin;
    const qreal titleY = this->rect().height() - kInnerMargin;
    painter.drawText(QPointF(titleX, titleY), title_.trimmed());
  }

  painter.setFont(labelFont);
  
  // X label positioning: medm centers it below the chart area
  // TextCenter at (x.Origin + x.Size / 2.0, y.LabelPos)
  // where y.LabelPos = y.Origin + y.Size + ymargin + axisnumbersize
  if (!xLabel_.trimmed().isEmpty()) {
    const qreal axisnumbersize = kInnerMargin + axisMetrics.height();
    const qreal labelY = rect.bottom() + axisnumbersize + labelMetrics.ascent();
    const qreal labelX = rect.left() + rect.width() / 2.0;
    const qreal textWidth = labelMetrics.horizontalAdvance(xLabel_.trimmed());
    painter.drawText(QPointF(labelX - textWidth / 2.0, labelY), xLabel_.trimmed());
  }

  // Y axis labels: positioned dynamically based on calculated axis positions
  const QRectF widgetBounds = this->rect().adjusted(kOuterMargin, kOuterMargin,
      -kOuterMargin, -kOuterMargin);
  const YAxisPositions axisPos = calculateYAxisPositions(widgetBounds);
  
  const qreal axisnumberwidth = axisMetrics.horizontalAdvance(QStringLiteral("0.88"));
  const qreal labelGap = 1.0;  // Gap between axis numbers and labels
  const qreal centerY = rect.top() + rect.height() / 2.0;
  
  // Draw labels for left-side axes
  for (const auto &[axisIndex, xPosition] : axisPos.leftAxes) {
    const QString labelText = labelTextOrSpace(yLabels_[axisIndex]);
    
    painter.save();
    const qreal textWidth = labelMetrics.horizontalAdvance(labelText);
    const int pixWidth = static_cast<int>(std::ceil(textWidth));
    const int pixHeight = static_cast<int>(std::ceil(labelMetrics.height()));
    
    // Create rotated text image
    QImage textImage(pixWidth, pixHeight, QImage::Format_ARGB32_Premultiplied);
    textImage.fill(Qt::transparent);
    
    QPainter textPainter(&textImage);
    textPainter.setFont(labelFont);
    textPainter.setPen(effectiveForeground());
    textPainter.drawText(QPointF(0, labelMetrics.ascent()), labelText);
    textPainter.end();
    
    QTransform transform;
    transform.rotate(-90.0);
    QImage rotatedImage = textImage.transformed(transform);
    
    // Position label to the left of the axis line with reduced gap
    const qreal labelX = xPosition - axisnumberwidth - 4 - labelGap - labelMetrics.height() / 2.0;
    const qreal drawX = labelX - rotatedImage.width() / 2.0;
    const qreal drawY = centerY - rotatedImage.height() / 2.0;
    
    // Ensure label stays within visible bounds
    const qreal minX = kOuterMargin;
    const qreal clippedDrawX = std::max(drawX, minX);
    painter.drawImage(QPointF(clippedDrawX, drawY), rotatedImage);

    if (shouldPaintYAxisCue(axisIndex)) {
      if (const auto cueColor = axisCueColor(axisIndex)) {
        const QRectF labelBounds(QPointF(clippedDrawX, drawY), QSizeF(rotatedImage.size()));
        paintAxisColorCue(painter, labelBounds, *cueColor);
      }
    }
    
    painter.restore();
  }
  
  // Draw labels for right-side axes
  for (const auto &[axisIndex, xPosition] : axisPos.rightAxes) {
    const QString labelText = labelTextOrSpace(yLabels_[axisIndex]);
    
    painter.save();
    const qreal textWidth = labelMetrics.horizontalAdvance(labelText);
    const int pixWidth = static_cast<int>(std::ceil(textWidth));
    const int pixHeight = static_cast<int>(std::ceil(labelMetrics.height()));
    
    // Create rotated text image
    QImage textImage(pixWidth, pixHeight, QImage::Format_ARGB32_Premultiplied);
    textImage.fill(Qt::transparent);
    
    QPainter textPainter(&textImage);
    textPainter.setFont(labelFont);
    textPainter.setPen(effectiveForeground());
    textPainter.drawText(QPointF(0, labelMetrics.ascent()), labelText);
    textPainter.end();
    
    QTransform transform;
    transform.rotate(-90.0);
    QImage rotatedImage = textImage.transformed(transform);
    
    // Position label to the right of the axis line with reduced gap
    const qreal labelX = xPosition + axisnumberwidth + 4 + labelGap + labelMetrics.height() / 2.0;
    const qreal drawX = labelX - rotatedImage.width() / 2.0;
    const qreal drawY = centerY - rotatedImage.height() / 2.0;
    
    // Ensure label stays within visible bounds
    const qreal maxX = this->rect().width() - kOuterMargin - rotatedImage.width();
    const qreal clippedDrawX = std::min(drawX, maxX);
    painter.drawImage(QPointF(clippedDrawX, drawY), rotatedImage);

    if (shouldPaintYAxisCue(axisIndex)) {
      if (const auto cueColor = axisCueColor(axisIndex)) {
        const QRectF labelBounds(QPointF(clippedDrawX, drawY), QSizeF(rotatedImage.size()));
        paintAxisColorCue(painter, labelBounds, *cueColor);
      }
    }
    
    painter.restore();
  }

  painter.restore();
}

void CartesianPlotElement::paintTraces(QPainter &painter, const QRectF &rect) const
{
  if (rect.width() <= 0.0 || rect.height() <= 0.0) {
    return;
  }

  if (executeMode_) {
    paintTracesExecute(painter, rect);
    return;
  }

  const int baseSamples = count_ > 0 ? count_ : kMinimumSampleCount;
  const int samples = std::clamp(baseSamples, kMinimumSampleCount,
    kMaximumSampleCount);

  for (int i = 0; i < traceCount(); ++i) {
    const QString yChannel = traces_[i].yChannel.trimmed();
    const QString xChannel = traces_[i].xChannel.trimmed();
    if (yChannel.isEmpty() && xChannel.isEmpty() && i > 0) {
      continue;
    }
    const QVector<QPointF> points = syntheticTracePoints(rect, i, samples);
    if (points.isEmpty()) {
      continue;
    }

    const QColor color = effectiveTraceColor(i);
    QPen pen(color);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (style_) {
    case CartesianPlotStyle::kPoint: {
      painter.save();
      painter.setBrush(color);
      for (const QPointF &point : points) {
        painter.drawEllipse(point, 2.0, 2.0);
      }
      painter.restore();
      break;
    }
    case CartesianPlotStyle::kLine: {
      QPainterPath path(points.front());
      for (int j = 1; j < points.size(); ++j) {
        path.lineTo(points[j]);
      }
      painter.drawPath(path);
      break;
    }
    case CartesianPlotStyle::kStep: {
      QPainterPath path(points.front());
      for (int j = 1; j < points.size(); ++j) {
        const QPointF &prev = points[j - 1];
        const QPointF &curr = points[j];
        path.lineTo(curr.x(), prev.y());
        path.lineTo(curr);
      }
      painter.drawPath(path);
      break;
    }
    case CartesianPlotStyle::kFillUnder: {
      QPainterPath path(points.front());
      for (int j = 1; j < points.size(); ++j) {
        path.lineTo(points[j]);
      }
      path.lineTo(QPointF(points.back().x(), rect.bottom()));
      path.lineTo(QPointF(points.front().x(), rect.bottom()));
      path.closeSubpath();
      QColor fillColor = color;
      fillColor.setAlpha(80);
      painter.save();
      painter.setBrush(fillColor);
      painter.drawPath(path);
      painter.restore();
      break;
    }
    }
  }
}

void CartesianPlotElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

QVector<QPointF> CartesianPlotElement::syntheticTracePoints(const QRectF &rect,
    int traceIndex, int sampleCount) const
{
  QVector<QPointF> points;
  if (sampleCount < 2 || rect.width() <= 0.0 || rect.height() <= 0.0) {
    return points;
  }
  points.reserve(sampleCount);
  const double phaseOffset = static_cast<double>(traceIndex) * 0.7;
  const double amplitude = 0.35 + 0.1 * (traceIndex % 3);
  const double offset = 0.5 + 0.1 * ((traceIndex % 4) - 1.5);
  for (int i = 0; i < sampleCount; ++i) {
    const double t = sampleCount > 1 ? static_cast<double>(i)
            / static_cast<double>(sampleCount - 1)
                                     : 0.0;
    double value = offset + amplitude * std::sin((t * kTwoPi) + phaseOffset);
    value = std::clamp(value, 0.0, 1.0);
    const double x = rect.left() + t * rect.width();
    const double y = rect.bottom() - value * rect.height();
    points.append(QPointF(x, y));
  }
  return points;
}

void CartesianPlotElement::paintTracesExecute(QPainter &painter,
    const QRectF &rect) const
{
  cachedAxisRangesValid_ = false;
  std::array<double, kCartesianAxisCount> autoMinimums{};
  std::array<double, kCartesianAxisCount> autoMaximums{};
  std::array<bool, kCartesianAxisCount> hasData{};
  for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
    autoMinimums[axis] = std::numeric_limits<double>::infinity();
    autoMaximums[axis] = -std::numeric_limits<double>::infinity();
    hasData[axis] = false;
  }

  for (int i = 0; i < traceCount(); ++i) {
    const QVector<QPointF> &points = traces_[i].runtimePoints;
    if (points.isEmpty()) {
      continue;
    }
    const int yAxisIndex = axisIndexForTrace(i);
    for (const QPointF &valuePoint : points) {
      const double xValue = valuePoint.x();
      const double yValue = valuePoint.y();
      if (std::isfinite(xValue)) {
        if (!hasData[0]) {
          autoMinimums[0] = xValue;
          autoMaximums[0] = xValue;
          hasData[0] = true;
        } else {
          autoMinimums[0] = std::min(autoMinimums[0], xValue);
          autoMaximums[0] = std::max(autoMaximums[0], xValue);
        }
      }
      if (yAxisIndex >= 0 && yAxisIndex < kCartesianAxisCount
          && std::isfinite(yValue)) {
        if (!hasData[yAxisIndex]) {
          autoMinimums[yAxisIndex] = yValue;
          autoMaximums[yAxisIndex] = yValue;
          hasData[yAxisIndex] = true;
        } else {
          autoMinimums[yAxisIndex] = std::min(autoMinimums[yAxisIndex], yValue);
          autoMaximums[yAxisIndex] = std::max(autoMaximums[yAxisIndex], yValue);
        }
      }
    }
  }

  std::array<AxisRange, kCartesianAxisCount> ranges{};
  for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
    ranges[axis] = computeAxisRange(axis, hasData, autoMinimums, autoMaximums);
  }

  if (!ranges[0].valid) {
    return;
  }

  // Apply zoom state if zoomed
  if (zoomed_) {
    for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
      if (ranges[axis].valid) {
        applyZoomToRange(ranges[axis], axis);
      }
    }
  }

  cachedAxisRanges_ = ranges;
  cachedAxisRangesValid_ = true;

  for (int i = 0; i < traceCount(); ++i) {
    const QVector<QPointF> &valuePoints = traces_[i].runtimePoints;
    if (valuePoints.isEmpty()) {
      continue;
    }
    const int yAxisIndex = axisIndexForTrace(i);
    if (yAxisIndex < 0 || yAxisIndex >= kCartesianAxisCount) {
      continue;
    }
    const AxisRange &xRange = ranges[0];
    const AxisRange &yRange = ranges[yAxisIndex];
    if (!yRange.valid) {
      continue;
    }

    QVector<QPointF> mappedPoints;
    mappedPoints.reserve(valuePoints.size());
    for (const QPointF &value : valuePoints) {
      QPointF mapped;
      if (!mapPointToChart(value, xRange, yRange, rect, &mapped)) {
        continue;
      }
      mappedPoints.append(mapped);
    }
    if (mappedPoints.isEmpty()) {
      continue;
    }

    const QColor color = effectiveTraceColor(i);
    QPen pen(color);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (style_) {
    case CartesianPlotStyle::kPoint: {
      painter.save();
      painter.setBrush(color);
      for (const QPointF &point : mappedPoints) {
        painter.drawEllipse(point, 2.0, 2.0);
      }
      painter.restore();
      break;
    }
    case CartesianPlotStyle::kLine: {
      QPainterPath path(mappedPoints.front());
      for (int j = 1; j < mappedPoints.size(); ++j) {
        path.lineTo(mappedPoints[j]);
      }
      painter.drawPath(path);
      break;
    }
    case CartesianPlotStyle::kStep: {
      QPainterPath path(mappedPoints.front());
      for (int j = 1; j < mappedPoints.size(); ++j) {
        const QPointF &prev = mappedPoints[j - 1];
        const QPointF &curr = mappedPoints[j];
        path.lineTo(curr.x(), prev.y());
        path.lineTo(curr);
      }
      painter.drawPath(path);
      break;
    }
    case CartesianPlotStyle::kFillUnder: {
      QPainterPath path(mappedPoints.front());
      for (int j = 1; j < mappedPoints.size(); ++j) {
        path.lineTo(mappedPoints[j]);
      }
      path.lineTo(QPointF(mappedPoints.back().x(), rect.bottom()));
      path.lineTo(QPointF(mappedPoints.front().x(), rect.bottom()));
      path.closeSubpath();
      QColor fillColor = color;
      fillColor.setAlpha(80);
      painter.save();
      painter.setBrush(fillColor);
      painter.drawPath(path);
      painter.restore();
      break;
    }
    }
  }
}

CartesianPlotElement::AxisRange CartesianPlotElement::computeAxisRange(
    int axisIndex, const std::array<bool, kCartesianAxisCount> &hasData,
    const std::array<double, kCartesianAxisCount> &autoMinimums,
    const std::array<double, kCartesianAxisCount> &autoMaximums) const
{
  AxisRange range;
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return range;
  }

  range.style = axisStyles_[axisIndex];
  const CartesianPlotRangeStyle rangeStyle = axisRangeStyles_[axisIndex];

  double minimum = axisMinimums_[axisIndex];
  double maximum = axisMaximums_[axisIndex];
  bool valid = false;

  switch (rangeStyle) {
  case CartesianPlotRangeStyle::kUserSpecified:
    valid = std::isfinite(minimum) && std::isfinite(maximum)
        && maximum >= minimum;
    break;
  case CartesianPlotRangeStyle::kChannel:
    if (axisRuntimeValid_[axisIndex]) {
      minimum = axisRuntimeMinimums_[axisIndex];
      maximum = axisRuntimeMaximums_[axisIndex];
      valid = true;
    } else {
      valid = std::isfinite(minimum) && std::isfinite(maximum)
          && maximum > minimum;
    }
    break;
  case CartesianPlotRangeStyle::kAutoScale:
    if (hasData[axisIndex]) {
      minimum = autoMinimums[axisIndex];
      maximum = autoMaximums[axisIndex];
      valid = (maximum > minimum)
          && std::isfinite(minimum) && std::isfinite(maximum);
    } else if (axisRuntimeValid_[axisIndex]) {
      minimum = axisRuntimeMinimums_[axisIndex];
      maximum = axisRuntimeMaximums_[axisIndex];
      valid = true;
    } else {
      valid = std::isfinite(minimum) && std::isfinite(maximum)
          && maximum > minimum;
    }
    break;
  }

  if (!valid) {
    minimum = 0.0;
    maximum = 1.0;
    valid = true;
  }

  if (range.style == CartesianPlotAxisStyle::kLog10) {
    if (minimum <= 0.0) {
      minimum = 1e-3;
    }
    if (maximum <= minimum) {
      maximum = minimum * 10.0;
    }
  }

  // Apply "nice" axis rounding matching MEDM's SciPlot behavior
  const NiceAxisRange nice = computeNiceAxisRange(
      minimum, maximum, range.style == CartesianPlotAxisStyle::kLog10);
  
  range.minimum = nice.drawMin;
  range.maximum = nice.drawMax;
  range.valid = true;
  return range;
}

bool CartesianPlotElement::mapPointToChart(const QPointF &value,
    const AxisRange &xRange, const AxisRange &yRange, const QRectF &rect,
    QPointF *mapped) const
{
  if (!xRange.valid || !yRange.valid || !mapped) {
    return false;
  }

  auto normalize = [](double v, const AxisRange &range) -> std::optional<double> {
    if (!std::isfinite(v)) {
      return std::nullopt;
    }
    if (range.style == CartesianPlotAxisStyle::kLog10) {
      if (v <= 0.0) {
        return std::nullopt;
      }
      const double logMin = std::log10(range.minimum);
      const double logMax = std::log10(range.maximum);
      if (!std::isfinite(logMin) || !std::isfinite(logMax)
          || logMax <= logMin) {
        return std::nullopt;
      }
      const double logValue = std::log10(v);
      return (logValue - logMin) / (logMax - logMin);
    }
    const double span = range.maximum - range.minimum;
    if (span <= 0.0) {
      return std::nullopt;
    }
    return (v - range.minimum) / span;
  };

  const std::optional<double> xNorm = normalize(value.x(), xRange);
  const std::optional<double> yNorm = normalize(value.y(), yRange);
  if (!xNorm.has_value() || !yNorm.has_value()) {
    return false;
  }

  *mapped = QPointF(rect.left() + xNorm.value() * rect.width(),
      rect.bottom() - yNorm.value() * rect.height());
  return true;
}

int CartesianPlotElement::axisIndexForTrace(int traceIndex) const
{
  if (traceIndex < 0 || traceIndex >= traceCount()) {
    return 1;
  }
  switch (traces_[traceIndex].yAxis) {
  case CartesianPlotYAxis::kY1:
    return 1;
  case CartesianPlotYAxis::kY2:
    return 2;
  case CartesianPlotYAxis::kY3:
    return 3;
  case CartesianPlotYAxis::kY4:
    return 4;
  }
  return 1;
}

bool CartesianPlotElement::isYAxisOnRight(int yAxisIndex) const
{
  // Check if any trace using this Y-axis is set to use the right side
  CartesianPlotYAxis targetAxis;
  switch (yAxisIndex) {
  case 0: targetAxis = CartesianPlotYAxis::kY1; break;
  case 1: targetAxis = CartesianPlotYAxis::kY2; break;
  case 2: targetAxis = CartesianPlotYAxis::kY3; break;
  case 3: targetAxis = CartesianPlotYAxis::kY4; break;
  default: return false;
  }
  
  for (const auto &trace : traces_) {
    if (trace.yAxis == targetAxis && trace.usesRightAxis) {
      return true;
    }
  }
  return false;
}

bool CartesianPlotElement::isYAxisVisible(int yAxisIndex) const
{
  // An axis is visible only if any trace uses it (matching MEDM behavior)
  CartesianPlotYAxis targetAxis;
  switch (yAxisIndex) {
  case 0: targetAxis = CartesianPlotYAxis::kY1; break;
  case 1: targetAxis = CartesianPlotYAxis::kY2; break;
  case 2: targetAxis = CartesianPlotYAxis::kY3; break;
  case 3: targetAxis = CartesianPlotYAxis::kY4; break;
  default: return false;
  }
  
  for (const auto &trace : traces_) {
    if (trace.yAxis == targetAxis) {
      return true;
    }
  }
  return false;
}

bool CartesianPlotElement::shouldPaintYAxisCue(int yAxisIndex) const
{
  if (!isYAxisVisible(yAxisIndex)) {
    return false;
  }
  for (int i = 0; i < static_cast<int>(yLabels_.size()); ++i) {
    if (i == yAxisIndex) {
      continue;
    }
    if (isYAxisVisible(i)) {
      return true;
    }
  }
  return false;
}

std::optional<QColor> CartesianPlotElement::axisCueColor(int yAxisIndex) const
{
  CartesianPlotYAxis targetAxis;
  switch (yAxisIndex) {
  case 0: targetAxis = CartesianPlotYAxis::kY1; break;
  case 1: targetAxis = CartesianPlotYAxis::kY2; break;
  case 2: targetAxis = CartesianPlotYAxis::kY3; break;
  case 3: targetAxis = CartesianPlotYAxis::kY4; break;
  default: return std::nullopt;
  }

  for (int i = 0; i < traceCount(); ++i) {
    const auto &trace = traces_[i];
    const bool hasChannels = !trace.yChannel.trimmed().isEmpty()
        || !trace.xChannel.trimmed().isEmpty();
    if (!hasChannels) {
      continue;
    }
    if (trace.yAxis != targetAxis) {
      continue;
    }
    const QColor color = effectiveTraceColor(i);
    if (color.isValid()) {
      return color;
    }
  }
  return std::nullopt;
}

void CartesianPlotElement::paintAxisColorCue(QPainter &painter,
    const QRectF &labelBounds, const QColor &color) const
{
  if (!color.isValid()) {
    return;
  }
  const QRectF widgetBounds(rect());
  const auto drawCue = [&](const QRectF &cueRect) {
    const QRectF clipped = cueRect.intersected(widgetBounds);
    if (clipped.width() > 0.0 && clipped.height() > 0.0) {
      painter.fillRect(clipped, color);
    }
  };

  const QRectF topCue(labelBounds.left(),
      labelBounds.top() - kAxisCueGap - kAxisCueThickness,
      labelBounds.width(), kAxisCueThickness);
  const QRectF bottomCue(labelBounds.left(),
      labelBounds.bottom() + kAxisCueGap,
      labelBounds.width(), kAxisCueThickness);

  drawCue(topCue);
  drawCue(bottomCue);
}

// Include moc output for Q_OBJECT macro
#include "moc_cartesian_plot_element.cpp"
