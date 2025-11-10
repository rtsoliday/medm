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
constexpr int kMinimumLabelPointSize = 10;
constexpr int kRefreshIntervalMs = 100;
constexpr double kMinimumRangeEpsilon = 1e-9;
constexpr int kMaxSampleBurst = 32;

constexpr int kDefaultPenColorIndex = 14;

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
  title_ = QStringLiteral("Strip Chart");
  xLabel_ = QStringLiteral("Time");
  yLabel_ = QStringLiteral("Value");
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

  paintFrame(painter);

  const Layout layout = calculateLayout(metrics);

  if (layout.innerRect.isValid() && !layout.innerRect.isEmpty()) {
    painter.fillRect(layout.innerRect, effectiveBackground());
  }

  if (layout.chartRect.width() > 0 && layout.chartRect.height() > 0) {
    painter.fillRect(layout.chartRect, effectiveBackground());
    paintTickMarks(painter, layout.chartRect);
    paintAxisScales(painter, layout.chartRect, metrics);
    if (layout.chartRect.width() > 2 && layout.chartRect.height() > 2) {
      paintGrid(painter, layout.chartRect);
      const QRect plotArea = layout.chartRect.adjusted(1, 1, -1, -1);
      paintPens(painter, plotArea);
    }
  }

  paintLabels(painter, layout, metrics);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

void StripChartElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
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
  layout.xLabelText = xLabel_.trimmed();
  layout.yLabelText = yLabel_.trimmed();

  if (!layout.innerRect.isValid() || layout.innerRect.isEmpty()) {
    return layout;
  }

  int left = layout.innerRect.left();
  const int right = layout.innerRect.right();
  int top = layout.innerRect.top();
  int bottom = layout.innerRect.bottom();

  if (!layout.titleText.isEmpty()) {
    const int height = metrics.height();
    layout.titleRect = QRect(left, top, layout.innerRect.width(), height);
    top += height + kInnerMargin;
  }

  if (!layout.xLabelText.isEmpty()) {
    const int height = metrics.height();
    layout.xLabelRect = QRect(left, bottom - height + 1,
        layout.innerRect.width(), height);
    bottom -= height + kInnerMargin;
  }

  const int verticalExtent = std::max(0, bottom - top + 1);

  if (!layout.yLabelText.isEmpty() && verticalExtent > 0) {
    int textWidth = 0;
    for (const QChar &ch : layout.yLabelText) {
      const int charWidth = metrics.horizontalAdvance(QString(ch));
      textWidth = std::max(textWidth, charWidth);
    }
    if (textWidth > 0) {
      layout.yLabelRect = QRect(left, top, textWidth, verticalExtent);
      left += textWidth + kInnerMargin;
    }
  }

  if (right >= left && bottom >= top) {
    layout.chartRect = QRect(left, top, right - left + 1,
        bottom - top + 1);
  }

  return layout;
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
  QFont adjusted = font();
  if (adjusted.pointSizeF() > 0.0) {
    adjusted.setPointSizeF(std::max(adjusted.pointSizeF(),
        static_cast<qreal>(kMinimumLabelPointSize)));
  } else if (adjusted.pointSize() > 0) {
    adjusted.setPointSize(std::max(adjusted.pointSize(), kMinimumLabelPointSize));
  } else {
    adjusted.setPointSize(kMinimumLabelPointSize);
  }
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
  int nDivX = std::min(kMaxTickMarks, kGridLines);
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
    const QFontMetrics &metrics) const
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

  const int nDivX = std::min(kMaxTickMarks, kGridLines);
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

  // Draw Y-axis scale numbers (left side)
  {
    // Determine the range for Y-axis
    double yLow = 0.0;
    double yHigh = 100.0;

    // Try to get range from first connected pen
    bool foundRange = false;
    for (int p = 0; p < penCount(); ++p) {
      if (!pens_[p].channel.trimmed().isEmpty()) {
        yLow = effectivePenLow(p);
        yHigh = effectivePenHigh(p);
        foundRange = true;
        break;
      }
    }

    if (!foundRange || !std::isfinite(yLow) || !std::isfinite(yHigh)) {
      yLow = 0.0;
      yHigh = 100.0;
    }

    const double range = yHigh - yLow;
    const double step = range / nDivY;
    const NumberFormat fmt = calculateNumberFormat(std::max(std::abs(yHigh), std::abs(yLow)));

    for (int i = 0; i <= nDivY; ++i) {
      const double value = yHigh - step * i;  // Count down from high to low
      const QString text = formatNumber(value, fmt.format, fmt.decimal);
      const int textWidth = metrics.horizontalAdvance(text);
      
      const int tickY = chartRect.top() + i * chartRect.height() / nDivY;
      const int textX = chartRect.left() - 2 - markerHeight - 1 - textWidth;
      const int textY = tickY + metrics.ascent() / 2;
      
      painter.drawText(textX, textY, text);
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
      double normalized = (sampleValue - low) / range;
      normalized = std::clamp(normalized, 0.0, 1.0);
      const double x = content.left()
          + (static_cast<double>(offsetColumns + s) / denominator) * width;
      const double y = content.bottom() - normalized * height;

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
    painter.drawText(layout.titleRect, Qt::AlignHCenter | Qt::AlignTop,
        layout.titleText);
  }

  if (!layout.xLabelText.isEmpty() && layout.xLabelRect.isValid()
      && !layout.xLabelRect.isEmpty()) {
    painter.drawText(layout.xLabelRect, Qt::AlignHCenter | Qt::AlignBottom,
        layout.xLabelText);
  }

  if (!layout.yLabelText.isEmpty() && layout.yLabelRect.isValid()
      && !layout.yLabelRect.isEmpty()) {
    painter.save();
    const int charHeight = metrics.height();
    const int charCount = layout.yLabelText.size();
    if (charHeight > 0 && charCount > 0) {
      const int totalHeight = charHeight * charCount;
      int startY = layout.yLabelRect.top();
      if (layout.yLabelRect.height() > totalHeight) {
        startY += (layout.yLabelRect.height() - totalHeight) / 2;
      }
      for (int i = 0; i < charCount; ++i) {
        const QString ch(layout.yLabelText.mid(i, 1));
        QRect cell(layout.yLabelRect.left(), startY + i * charHeight,
            layout.yLabelRect.width(), charHeight);
        painter.drawText(cell, Qt::AlignCenter | Qt::AlignVCenter, ch);
      }
    }
    painter.restore();
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
