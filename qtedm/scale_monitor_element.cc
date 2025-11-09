#include "scale_monitor_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPolygonF>

#include "medm_colors.h"

namespace {

constexpr int kTickCount = 10;
constexpr double kSampleNormalizedValue = 0.65;
constexpr qreal kAxisSpacing = 4.0;
constexpr qreal kMinimumChartExtent = 16.0;
constexpr qreal kMinimumAxisExtent = 14.0;
constexpr qreal kOutlineMargin = 4.0;
constexpr qreal kLabelTextPadding = 2.0;
constexpr qreal kBevelWidth = 2.0;
constexpr qreal kLayoutPadding = 3.0;
constexpr short kInvalidSeverity = 3;
constexpr short kDisconnectedSeverity = kInvalidSeverity + 1;

} // namespace

struct ScaleMonitorElement::Layout
{
  QRectF chartRect;
  QRectF axisRect;
  QRectF readbackRect;
  QRectF channelRect;
  QString lowLabel;
  QString highLabel;
  QString readbackText;
  QString channelText;
  qreal lineHeight = 0.0;
  bool showAxis = false;
  bool showLimits = false;
  bool showReadback = false;
  bool showChannel = false;
  bool vertical = true;
};

ScaleMonitorElement::ScaleMonitorElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  limits_.lowSource = PvLimitSource::kDefault;
  limits_.highSource = PvLimitSource::kDefault;
  limits_.precisionSource = PvLimitSource::kDefault;
  limits_.lowDefault = 0.0;
  limits_.highDefault = 100.0;
  limits_.precisionDefault = 1;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimePrecision_ = -1;
  runtimeValue_ = defaultSampleValue();
  runtimeSeverity_ = kInvalidSeverity;
}

void ScaleMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ScaleMonitorElement::isSelected() const
{
  return selected_;
}

QColor ScaleMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void ScaleMonitorElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor ScaleMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void ScaleMonitorElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode ScaleMonitorElement::colorMode() const
{
  return colorMode_;
}

void ScaleMonitorElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

MeterLabel ScaleMonitorElement::label() const
{
  return label_;
}

void ScaleMonitorElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

BarDirection ScaleMonitorElement::direction() const
{
  return direction_;
}

void ScaleMonitorElement::setDirection(BarDirection direction)
{
  if (direction != BarDirection::kUp && direction != BarDirection::kRight) {
    direction = BarDirection::kRight;
  }
  if (direction_ == direction) {
    return;
  }
  direction_ = direction;
  update();
}

const PvLimits &ScaleMonitorElement::limits() const
{
  return limits_;
}

void ScaleMonitorElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  if (limits_.precisionSource == PvLimitSource::kUser) {
    limits_.precisionSource = PvLimitSource::kDefault;
  }
  if (limits_.lowSource == PvLimitSource::kUser) {
    limits_.lowSource = PvLimitSource::kDefault;
  }
  if (limits_.highSource == PvLimitSource::kUser) {
    limits_.highSource = PvLimitSource::kDefault;
  }
  if (!executeMode_) {
    runtimeLow_ = limits_.lowDefault;
    runtimeHigh_ = limits_.highDefault;
    runtimePrecision_ = -1;
    runtimeValue_ = defaultSampleValue();
  } else if (!runtimeLimitsValid_) {
    runtimeLow_ = limits_.lowDefault;
    runtimeHigh_ = limits_.highDefault;
  }
  update();
}

QString ScaleMonitorElement::channel() const
{
  return channel_;
}

void ScaleMonitorElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  update();
}

void ScaleMonitorElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
}

bool ScaleMonitorElement::isExecuteMode() const
{
  return executeMode_;
}

void ScaleMonitorElement::setRuntimeConnected(bool connected)
{
  if (!executeMode_) {
    return;
  }
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeSeverity_ = kInvalidSeverity;
    runtimeLimitsValid_ = false;
    runtimePrecision_ = -1;
    hasRuntimeValue_ = false;
    runtimeLow_ = limits_.lowDefault;
    runtimeHigh_ = limits_.highDefault;
    runtimeValue_ = defaultSampleValue();
  }
  update();
}

void ScaleMonitorElement::setRuntimeSeverity(short severity)
{
  if (!executeMode_) {
    return;
  }
  if (severity < 0) {
    severity = 0;
  }
  if (runtimeSeverity_ == severity) {
    return;
  }
  runtimeSeverity_ = severity;
  if (colorMode_ == TextColorMode::kAlarm) {
    update();
  }
}

void ScaleMonitorElement::setRuntimeValue(double value)
{
  if (!executeMode_ || !std::isfinite(value)) {
    return;
  }
  const double clamped = clampToLimits(value);
  const bool firstValue = !hasRuntimeValue_;
  const bool changed = firstValue
      || std::abs(clamped - runtimeValue_) > valueEpsilon();
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  if (runtimeConnected_ && changed) {
    update();
  }
}

void ScaleMonitorElement::setRuntimeLimits(double low, double high)
{
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return;
  }
  if (std::abs(high - low) < 1e-12) {
    high = low + 1.0;
  }
  runtimeLow_ = low;
  runtimeHigh_ = high;
  runtimeLimitsValid_ = true;
  if (executeMode_) {
    runtimeValue_ = clampToLimits(runtimeValue_);
    update();
  }
}

void ScaleMonitorElement::setRuntimePrecision(int precision)
{
  const int clamped = std::clamp(precision, 0, 17);
  if (runtimePrecision_ == clamped) {
    return;
  }
  runtimePrecision_ = clamped;
  if (executeMode_) {
    update();
  }
}

void ScaleMonitorElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  runtimeLimitsValid_ = false;
  hasRuntimeValue_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimePrecision_ = -1;
  runtimeValue_ = defaultSampleValue();
  runtimeSeverity_ = kInvalidSeverity;
  update();
}

void ScaleMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  painter.fillRect(rect(), effectiveBackground());

  // Paint 2-pixel raised bevel around outer edge (matching MEDM appearance)
  const QColor bg = effectiveBackground();
  QRect bevelOuter = rect().adjusted(0, 0, -1, -1);
  painter.setPen(QPen(bg.lighter(135), 1));
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.topRight());
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.bottomLeft());
  painter.setPen(QPen(bg.darker(145), 1));
  painter.drawLine(bevelOuter.bottomLeft(), bevelOuter.bottomRight());
  painter.drawLine(bevelOuter.topRight(), bevelOuter.bottomRight());

  QRect bevelInner = bevelOuter.adjusted(1, 1, -1, -1);
  painter.setPen(QPen(bg.lighter(150), 1));
  painter.drawLine(bevelInner.topLeft(), bevelInner.topRight());
  painter.drawLine(bevelInner.topLeft(), bevelInner.bottomLeft());
  painter.setPen(QPen(bg.darker(170), 1));
  painter.drawLine(bevelInner.bottomLeft(), bevelInner.bottomRight());
  painter.drawLine(bevelInner.topRight(), bevelInner.bottomRight());

  if (executeMode_ && !runtimeConnected_) {
    painter.fillRect(rect(), Qt::white);
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const qreal padding = (label_ == MeterLabel::kNoDecorations) 
      ? 0.0 
      : (kLayoutPadding + kBevelWidth);
  const QRectF contentRect = rect().adjusted(padding, padding, -padding, -padding);
  if (!contentRect.isValid() || contentRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const QFont baseFont = painter.font();

  // Font setup for scale labels - match MEDM's algorithm (same as Bar widget):
  // 1. preferredHeight = widget_height / INDICATOR_FONT_DIVISOR (8)
  // 2. Find closest match from MEDM's fixed font table using binary search
  // MEDM's fontSizeTable from siteSpecific.h:
  static constexpr int kFontSizeTable[] = {
    4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34
  };
  static constexpr int kNumFonts = sizeof(kFontSizeTable) / sizeof(kFontSizeTable[0]);
  
  const qreal widgetHeight = rect().height();
  constexpr qreal kIndicatorFontDivisor = 8.0;
  const int preferredPixelHeight = static_cast<int>(
      std::max(1.0, widgetHeight / kIndicatorFontDivisor));

  // Binary search to find best font (matches dmGetBestFontWithInfo)
  int upper = kNumFonts - 1;
  int lower = 0;
  int i = kNumFonts / 2;
  int count = 0;
  
  while ((i > 0) && (i < kNumFonts) && ((upper - lower) > 2) && (count < kNumFonts / 2)) {
    count++;
    if (kFontSizeTable[i] > preferredPixelHeight) {
      upper = i;
      i = upper - (upper - lower) / 2;
    } else if (kFontSizeTable[i] < preferredPixelHeight) {
      lower = i;
      i = lower + (upper - lower) / 2;
    } else {
      break;  // exact match
    }
  }
  
  if (i < 0) i = 0;
  if (i >= kNumFonts) i = kNumFonts - 1;
  
  const int chosenPixelSize = kFontSizeTable[i];

  QFont labelFont = baseFont;
  labelFont.setPixelSize(chosenPixelSize);
  painter.setFont(labelFont);
  const QFontMetricsF metrics(labelFont);

  const Layout layout = calculateLayout(contentRect, metrics);
  if (!layout.chartRect.isValid() || layout.chartRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  paintScale(painter, layout.chartRect);
  if (layout.showAxis) {
    paintAxis(painter, layout);
  } else {
    paintInternalTicks(painter, layout.chartRect);
  }
  paintPointer(painter, layout);
  paintLabels(painter, layout);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

ScaleMonitorElement::Layout ScaleMonitorElement::calculateLayout(
    const QRectF &bounds, const QFontMetricsF &metrics) const
{
  Layout layout;
  layout.vertical = isVertical();

  if (!bounds.isValid() || bounds.isEmpty()) {
    return layout;
  }

  layout.lineHeight = std::max<qreal>(metrics.height(), 8.0);
  const qreal spacing = std::max<qreal>(layout.lineHeight * 0.25, kAxisSpacing);
  
  layout.showAxis = (label_ == MeterLabel::kOutline
      || label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel);
  layout.showLimits = (label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel
      || label_ == MeterLabel::kOutline);
  layout.showReadback = (label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel);
  if (layout.showLimits) {
    layout.lowLabel = axisLabelText(effectiveLowLimit());
    layout.highLabel = axisLabelText(effectiveHighLimit());
  }
  if (layout.showReadback) {
    layout.readbackText = formattedSampleValue();
  }
  if (label_ == MeterLabel::kChannel) {
    layout.channelText = channel_.trimmed();
    layout.showChannel = !layout.channelText.isEmpty();
  }

  qreal left = bounds.left();
  const qreal right = bounds.right();
  qreal top = bounds.top();
  qreal bottom = bounds.bottom();

  if (layout.vertical) {
    if (layout.showChannel) {
      layout.channelRect = QRectF(left, top, bounds.width(), layout.lineHeight);
      top += layout.lineHeight + spacing;
    }

    // When showing limits (Outline or Limits mode), reserve space for upper limit text
    // The text is centered at the top position, so it extends lineHeight*0.5 above
    if (layout.showLimits && !layout.showChannel) {
      top += layout.lineHeight * 0.25;
    }

    if (layout.showReadback) {
      const qreal readbackTop = bottom - layout.lineHeight;
      if (readbackTop > top) {
        layout.readbackRect = QRectF(left, readbackTop, bounds.width(),
            layout.lineHeight);
        // Reserve space for readback plus half line height for lower limit text to sit above
        bottom = readbackTop - spacing - (layout.lineHeight * 0.25);
      } else {
        layout.showReadback = false;
        layout.readbackRect = QRectF();
      }
    } else if (layout.showLimits) {
      // When showing limits without readback (Outline mode), reserve space for lower limit text
      // The text is centered at the bottom position, so it extends lineHeight*0.5 below
      bottom -= layout.lineHeight * 0.25;
    }

    if (bottom <= top) {
      return layout;
    }

    const qreal chartHeight = bottom - top;
    if (chartHeight < 4.0) {
      layout.chartRect = QRectF();
      layout.axisRect = QRectF();
      layout.showAxis = false;
      return layout;
    }

    if (layout.showAxis) {
      qreal axisWidth = std::max<qreal>(kMinimumAxisExtent, layout.lineHeight);
      if (layout.showLimits) {
        axisWidth = std::max(axisWidth,
            metrics.horizontalAdvance(layout.lowLabel) + 6.0);
        axisWidth = std::max(axisWidth,
            metrics.horizontalAdvance(layout.highLabel) + 6.0);
      }
      const qreal availableWidth = (right - left) - axisWidth - spacing;
      const qreal minimumTotal = kMinimumAxisExtent + spacing
          + kMinimumChartExtent;
      if ((right - left) < minimumTotal) {
        const qreal reducedSpacing = std::min<qreal>(spacing, 2.0);
        const qreal reducedAxisWidth = std::max<qreal>(8.0, axisWidth * 0.6);
        const qreal reducedChartWidth = std::max<qreal>(8.0,
            (right - left) - reducedAxisWidth - reducedSpacing);
        if (reducedAxisWidth + reducedSpacing + reducedChartWidth
            <= (right - left)) {
          layout.axisRect = QRectF(left, top, reducedAxisWidth, chartHeight);
          const qreal chartLeft = layout.axisRect.right() + reducedSpacing;
          layout.chartRect = QRectF(chartLeft, top, reducedChartWidth,
              chartHeight);
        } else {
          layout.showAxis = false;
          layout.axisRect = QRectF();
          layout.chartRect = QRectF(left, top, right - left, chartHeight);
        }
      } else if (availableWidth < kMinimumChartExtent) {
        layout.showAxis = false;
        layout.axisRect = QRectF();
        layout.chartRect = QRectF(left, top, right - left, chartHeight);
      } else {
        layout.axisRect = QRectF(left, top, axisWidth, chartHeight);
        const qreal chartLeft = layout.axisRect.right() + spacing;
        layout.chartRect = QRectF(chartLeft, top, availableWidth, chartHeight);
      }
    } else {
      layout.chartRect = QRectF(left, top, right - left, chartHeight);
    }
  } else {
    if (layout.showChannel) {
      layout.channelRect = QRectF(left, top, bounds.width(), layout.lineHeight);
      top += layout.lineHeight + spacing;
    }

    if (layout.showReadback) {
      const qreal readbackTop = bottom - layout.lineHeight;
      if (readbackTop > top) {
        layout.readbackRect = QRectF(left, readbackTop, bounds.width(),
            layout.lineHeight);
        bottom = readbackTop - spacing;
      } else {
        layout.showReadback = false;
        layout.readbackRect = QRectF();
      }
    }

    if (bottom <= top) {
      return layout;
    }

    qreal availableHeight = bottom - top;
    if (layout.showAxis) {
      qreal axisHeight = std::max<qreal>(kMinimumAxisExtent,
          layout.lineHeight + 4.0);
      const qreal minimumTotal = kMinimumAxisExtent + spacing
          + kMinimumChartExtent;
      if (availableHeight < minimumTotal) {
        const qreal reducedSpacing = std::min<qreal>(spacing, 2.0);
        const qreal reducedAxisHeight = std::max<qreal>(8.0, axisHeight * 0.6);
        const qreal reducedChartHeight = std::max<qreal>(8.0,
            availableHeight - reducedAxisHeight - reducedSpacing);
        if (reducedAxisHeight + reducedSpacing + reducedChartHeight
            <= availableHeight) {
          layout.axisRect = QRectF(left, top, bounds.width(),
              reducedAxisHeight);
          top += reducedAxisHeight + reducedSpacing;
          availableHeight = bottom - top;
        } else {
          layout.showAxis = false;
          layout.axisRect = QRectF();
        }
      } else {
        layout.axisRect = QRectF(left, top, bounds.width(), axisHeight);
        top += axisHeight + spacing;
        availableHeight = bottom - top;
      }
    }

    if (availableHeight < 4.0) {
      layout.chartRect = QRectF();
      return layout;
    }

    layout.chartRect = QRectF(left, top, bounds.width(), availableHeight);
  }

  return layout;
}

void ScaleMonitorElement::paintScale(
    QPainter &painter, const QRectF &chartRect) const
{
  if (!chartRect.isValid() || chartRect.isEmpty()) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  
  // Fill the chart area with a slightly lighter background
  QColor fillColor = effectiveBackground().lighter(108);
  painter.setBrush(fillColor);
  painter.drawRect(chartRect);

  if (label_ != MeterLabel::kNoDecorations) {
    // Paint 2-pixel sunken bevel around chart using absolute colors for visibility on dark backgrounds
    QRectF bevelOuter = chartRect.adjusted(0.5, 0.5, -0.5, -0.5);
    
    // Outer bevel - dark on top/left for sunken effect
    painter.setPen(QPen(QColor(0, 0, 0, 180), 1));  // Semi-transparent black
    painter.drawLine(bevelOuter.topLeft(), bevelOuter.topRight());
    painter.drawLine(bevelOuter.topLeft(), bevelOuter.bottomLeft());
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1));  // Semi-transparent white
    painter.drawLine(bevelOuter.bottomLeft(), bevelOuter.bottomRight());
    painter.drawLine(bevelOuter.topRight(), bevelOuter.bottomRight());
    
    // Inner bevel - slightly less contrast
    QRectF bevelInner = bevelOuter.adjusted(1.0, 1.0, -1.0, -1.0);
    painter.setPen(QPen(QColor(0, 0, 0, 120), 1));  // Lighter black
    painter.drawLine(bevelInner.topLeft(), bevelInner.topRight());
    painter.drawLine(bevelInner.topLeft(), bevelInner.bottomLeft());
    painter.setPen(QPen(QColor(255, 255, 255, 80), 1));  // More transparent white
    painter.drawLine(bevelInner.bottomLeft(), bevelInner.bottomRight());
    painter.drawLine(bevelInner.topRight(), bevelInner.bottomRight());
  }
  painter.restore();
}

void ScaleMonitorElement::paintAxis(QPainter &painter,
    const Layout &layout) const
{
  if (!layout.showAxis || !layout.axisRect.isValid()
      || layout.axisRect.isEmpty()) {
    return;
  }

  painter.save();
  const QColor axisColor(Qt::black);
  QPen axisPen(axisColor);
  axisPen.setWidth(1);
  painter.setPen(axisPen);
  painter.setBrush(Qt::NoBrush);

  if (layout.vertical) {
    const qreal axisX = layout.axisRect.right();
    const qreal axisHeight = layout.axisRect.height();
    const qreal tickLength = std::min<qreal>(layout.axisRect.width(), 10.0);

    painter.drawLine(QPointF(axisX, layout.axisRect.top()),
        QPointF(axisX, layout.axisRect.bottom()));

    auto positionForNormalized = [&](double normalized) {
      if (direction_ == BarDirection::kUp) {
        return layout.axisRect.bottom() - normalized * axisHeight;
      }
      return layout.axisRect.top() + normalized * axisHeight;
    };

    for (int i = 0; i <= kTickCount; ++i) {
      const double normalized = static_cast<double>(i) / kTickCount;
      const qreal y = positionForNormalized(normalized);
      painter.drawLine(QPointF(axisX, y), QPointF(axisX - tickLength, y));
    }

    if (layout.showLimits) {
      const QFontMetricsF metrics(painter.font());
      const qreal textRight = axisX - tickLength - 2.0;
      const qreal maxWidth = std::max<qreal>(
          textRight - layout.axisRect.left(), 0.0);
      auto labelRectForWidth = [&](const QString &label, qreal centerY) {
        const qreal textWidth = metrics.horizontalAdvance(label);
        const qreal paddedWidth = std::clamp(
            textWidth + 2.0 * kLabelTextPadding,
            std::max(metrics.averageCharWidth(), 1.0),
            std::max(maxWidth, metrics.averageCharWidth()));
        const qreal rectWidth = std::min(paddedWidth, maxWidth);
        const qreal left = textRight - rectWidth;
        return QRectF(left, centerY - layout.lineHeight * 0.5,
            rectWidth, layout.lineHeight);
      };

      if (!layout.lowLabel.isEmpty()) {
        const qreal yLow = positionForNormalized(0.0);
        const QRectF lowRect = labelRectForWidth(layout.lowLabel, yLow);
        painter.drawText(lowRect, Qt::AlignRight | Qt::AlignVCenter,
            layout.lowLabel);
      }

      if (!layout.highLabel.isEmpty()) {
        const qreal yHigh = positionForNormalized(1.0);
        const QRectF highRect = labelRectForWidth(layout.highLabel, yHigh);
        painter.drawText(highRect, Qt::AlignRight | Qt::AlignVCenter,
            layout.highLabel);
      }
    }
  } else {
    const qreal axisY = layout.axisRect.bottom();
    const qreal axisWidth = layout.axisRect.width();
    const qreal tickLength = (height() < 50) ? 2.0 
        : std::min<qreal>(layout.axisRect.height(), 10.0);

    painter.drawLine(QPointF(layout.axisRect.left(), axisY),
        QPointF(layout.axisRect.right(), axisY));

    auto positionForNormalized = [&](double normalized) {
      if (direction_ == BarDirection::kRight) {
        return layout.axisRect.left() + normalized * axisWidth;
      }
      return layout.axisRect.right() - normalized * axisWidth;
    };

    for (int i = 0; i <= kTickCount; ++i) {
      const double normalized = static_cast<double>(i) / kTickCount;
      const qreal x = positionForNormalized(normalized);
      painter.drawLine(QPointF(x, axisY), QPointF(x, axisY - tickLength));
    }

    if (layout.showLimits) {
      const QFontMetricsF metrics(painter.font());
      const qreal textHeight = std::max<qreal>(
          layout.axisRect.height() - tickLength - 2.0, metrics.height());
      const qreal textTop = axisY - tickLength - textHeight;

      if (!layout.lowLabel.isEmpty()) {
        const qreal width = metrics.horizontalAdvance(layout.lowLabel) + 6.0;
        QRectF lowRect((direction_ == BarDirection::kRight)
                ? layout.axisRect.left()
                : layout.axisRect.right() - width,
            textTop, width, textHeight);
        Qt::Alignment align = (direction_ == BarDirection::kRight)
            ? Qt::AlignLeft : Qt::AlignRight;
        painter.drawText(lowRect, align | Qt::AlignBottom, layout.lowLabel);
      }

      if (!layout.highLabel.isEmpty()) {
        const qreal width = metrics.horizontalAdvance(layout.highLabel) + 6.0;
        QRectF highRect((direction_ == BarDirection::kRight)
                ? layout.axisRect.right() - width
                : layout.axisRect.left(),
            textTop, width, textHeight);
        Qt::Alignment align = (direction_ == BarDirection::kRight)
            ? Qt::AlignRight : Qt::AlignLeft;
        painter.drawText(highRect, align | Qt::AlignBottom, layout.highLabel);
      }
    }
  }

  painter.restore();
}

void ScaleMonitorElement::paintInternalTicks(
    QPainter &painter, const QRectF &chartRect) const
{
  if (!chartRect.isValid() || chartRect.isEmpty()) {
    return;
  }

  const QColor tickColor(Qt::black);
  QPen tickPen(tickColor);
  tickPen.setWidth(1);
  painter.setPen(tickPen);

  const bool vertical = isVertical();
  qreal tickLength;
  if (!vertical && height() < 50) {
    tickLength = 2.0;
  } else {
    const qreal majorLength = vertical ? chartRect.width() * 0.45
                                       : chartRect.height() * 0.45;
    tickLength = std::min<qreal>(majorLength, 10.0);
  }

  for (int i = 0; i <= kTickCount; ++i) {
    const double ratio = static_cast<double>(i) / kTickCount;
    if (vertical) {
      const qreal y = chartRect.bottom() - ratio * chartRect.height();
      painter.drawLine(QPointF(chartRect.left(), y),
          QPointF(chartRect.left() + tickLength, y));
      painter.drawLine(QPointF(chartRect.right(), y),
          QPointF(chartRect.right() - tickLength, y));
    } else {
      const qreal x = chartRect.left() + ratio * chartRect.width();
      painter.drawLine(QPointF(x, chartRect.top()),
          QPointF(x, chartRect.top() + tickLength));
      painter.drawLine(QPointF(x, chartRect.bottom()),
          QPointF(x, chartRect.bottom() - tickLength));
    }
  }
}

void ScaleMonitorElement::paintPointer(QPainter &painter,
    const Layout &layout) const
{
  if (!layout.chartRect.isValid() || layout.chartRect.isEmpty()) {
    return;
  }

  if (executeMode_ && !runtimeConnected_) {
    return;
  }

  const bool vertical = layout.vertical;
  double ratio = normalizedSampleValue();
  if (isDirectionInverted()) {
    ratio = 1.0 - ratio;
  }
  ratio = std::clamp(ratio, 0.0, 1.0);

  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveForeground());

  // Inset for bevel (2 pixels when decorations are on)
  const qreal bevelInset = (label_ != MeterLabel::kNoDecorations) ? 2.0 : 0.0;
  
  // Calculate indicator_size (diamond size) based on chart dimensions
  const qreal indicator_size = vertical
    ? std::min<qreal>(layout.chartRect.width() * 0.8, 16.0)
    : std::min<qreal>(layout.chartRect.height() * 0.8, 16.0);

  // Set clipping to the chart rect inset by bevel to prevent diamond from extending into bevel
  painter.save();
  const QRectF clipRect = layout.chartRect.adjusted(bevelInset, bevelInset, 
      -bevelInset, -bevelInset);
  painter.setClipRect(clipRect);

  if (vertical) {
    const qreal y = layout.chartRect.bottom() - ratio * layout.chartRect.height();
    
    // Draw horizontal line across chart (inset from bevel edges)
    QPen linePen(effectiveForeground());
    linePen.setWidth(2);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(layout.chartRect.left() + bevelInset, y),
        QPointF(layout.chartRect.right() - bevelInset, y));

    // Draw filled diamond marker (like MEDM Indicator widget)
    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveForeground());
    
    // Diamond with 4 points: left, top, right, bottom (inset from bevel)
    const qreal chartCenterX = layout.chartRect.left() + layout.chartRect.width() / 2.0;
    QPolygonF diamond;
    diamond << QPointF(layout.chartRect.left() + bevelInset, y)  // left corner at value
            << QPointF(chartCenterX, y - indicator_size / 2.0)    // top corner
            << QPointF(layout.chartRect.right() - bevelInset, y)  // right corner
            << QPointF(chartCenterX, y + indicator_size / 2.0);   // bottom corner
    painter.drawPolygon(diamond);
  } else {
    const qreal x = layout.chartRect.left() + ratio * layout.chartRect.width();
    
    // Draw vertical line across chart (inset from bevel edges)
    QPen linePen(effectiveForeground());
    linePen.setWidth(2);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(x, layout.chartRect.top() + bevelInset),
        QPointF(x, layout.chartRect.bottom() - bevelInset));

    // Draw filled diamond marker (like MEDM Indicator widget)
    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveForeground());
    
    // Diamond with 4 points: top, right, bottom, left (inset from bevel)
    const qreal chartCenterY = layout.chartRect.top() + layout.chartRect.height() / 2.0;
    QPolygonF diamond;
    diamond << QPointF(x - indicator_size / 2.0, chartCenterY)           // left corner
            << QPointF(x, layout.chartRect.top() + bevelInset)           // top corner
            << QPointF(x + indicator_size / 2.0, chartCenterY)           // right corner
            << QPointF(x, layout.chartRect.bottom() - bevelInset);       // bottom corner
    painter.drawPolygon(diamond);
  }
  
  painter.restore();
}

void ScaleMonitorElement::paintLabels(
    QPainter &painter, const Layout &layout) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  painter.save();
  const QColor fg(Qt::black);
  painter.setPen(fg);
  painter.setBrush(Qt::NoBrush);

  if (label_ == MeterLabel::kOutline && layout.chartRect.isValid()
      && !layout.chartRect.isEmpty()) {
    QPen outlinePen(fg.darker(160));
    outlinePen.setStyle(Qt::DotLine);
    outlinePen.setWidth(1);
    painter.setPen(outlinePen);
    painter.drawRect(layout.chartRect.adjusted(-kOutlineMargin, -kOutlineMargin,
        kOutlineMargin, kOutlineMargin));
    painter.restore();
    return;
  }

  if (layout.showChannel && layout.channelRect.isValid()
      && !layout.channelRect.isEmpty()) {
    painter.drawText(layout.channelRect, Qt::AlignCenter | Qt::AlignVCenter,
        layout.channelText);
  }

  if (layout.showReadback && layout.readbackRect.isValid()
      && !layout.readbackRect.isEmpty()) {
    QFontMetricsF fm(painter.font());
    const qreal textWidth = fm.boundingRect(layout.readbackText).width();
    const qreal padding = 4.0;  // 2 pixels on each side
    const qreal bgWidth = textWidth + padding;
    
    // Center the white background around the text
    const qreal centerX = layout.readbackRect.center().x();
    const qreal bgLeft = centerX - bgWidth * 0.5;
    QRectF bgRect(bgLeft, layout.readbackRect.top(), 
        bgWidth, layout.readbackRect.height());
    
    // Paint white background for readback text
    painter.fillRect(bgRect, Qt::white);
    painter.drawText(layout.readbackRect, Qt::AlignCenter | Qt::AlignVCenter,
        layout.readbackText);
  }

  painter.restore();
}

double ScaleMonitorElement::sampleValue() const
{
  return clampToLimits(currentValue());
}

QString ScaleMonitorElement::formattedSampleValue() const
{
  if (executeMode_) {
    if (!runtimeConnected_ || !hasRuntimeValue_) {
      return QStringLiteral("--");
    }
  }
  return formatValue(sampleValue());
}

QColor ScaleMonitorElement::effectiveForeground() const
{
  if (executeMode_) {
    if (colorMode_ == TextColorMode::kAlarm) {
      if (!runtimeConnected_) {
        return MedmColors::alarmColorForSeverity(kDisconnectedSeverity);
      }
      return MedmColors::alarmColorForSeverity(runtimeSeverity_);
    }
  }
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return defaultForeground();
}

QColor ScaleMonitorElement::effectiveBackground() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackground();
}

bool ScaleMonitorElement::isVertical() const
{
  return direction_ == BarDirection::kUp || direction_ == BarDirection::kDown;
}

bool ScaleMonitorElement::isDirectionInverted() const
{
  return direction_ == BarDirection::kDown || direction_ == BarDirection::kLeft;
}

void ScaleMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

double ScaleMonitorElement::normalizedSampleValue() const
{
  const double low = effectiveLowLimit();
  const double high = effectiveHighLimit();
  const double value = sampleValue();
  if (!std::isfinite(low) || !std::isfinite(high) || !std::isfinite(value)) {
    return std::clamp(kSampleNormalizedValue, 0.0, 1.0);
  }
  const double span = high - low;
  if (std::abs(span) < 1e-12) {
    return 0.0;
  }
  double normalized = (value - low) / span;
  return std::clamp(normalized, 0.0, 1.0);
}

QColor ScaleMonitorElement::defaultForeground() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor ScaleMonitorElement::defaultBackground() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

double ScaleMonitorElement::effectiveLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double ScaleMonitorElement::effectiveHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

int ScaleMonitorElement::effectivePrecision() const
{
  if (executeMode_ && limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
  }
  return std::clamp(limits_.precisionDefault, 0, 17);
}

double ScaleMonitorElement::currentValue() const
{
  if (executeMode_ && runtimeConnected_ && hasRuntimeValue_) {
    return runtimeValue_;
  }
  return defaultSampleValue();
}

double ScaleMonitorElement::defaultSampleValue() const
{
  const double low = limits_.lowDefault;
  const double high = limits_.highDefault;
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return 0.0;
  }
  const double span = high - low;
  if (std::abs(span) < 1e-12) {
    return low;
  }
  const double normalized = std::clamp(kSampleNormalizedValue, 0.0, 1.0);
  return low + span * normalized;
}

QString ScaleMonitorElement::formatValue(double value, char format, int precision) const
{
  if (!std::isfinite(value)) {
    return QStringLiteral("--");
  }
  int digits = precision;
  if (digits < 0) {
    digits = effectivePrecision();
  } else {
    digits = std::clamp(digits, 0, 17);
  }
  return QString::number(value, format, digits);
}

QString ScaleMonitorElement::axisLabelText(double value) const
{
  return formatValue(value, 'f', -1);
}

double ScaleMonitorElement::clampToLimits(double value) const
{
  double low = effectiveLowLimit();
  double high = effectiveHighLimit();
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return value;
  }
  if (low > high) {
    std::swap(low, high);
  }
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

double ScaleMonitorElement::valueEpsilon() const
{
  double span = effectiveHighLimit() - effectiveLowLimit();
  if (!std::isfinite(span)) {
    span = 1.0;
  }
  span = std::abs(span);
  double epsilon = span * 1e-6;
  if (!std::isfinite(epsilon) || epsilon <= 0.0) {
    epsilon = 1e-9;
  }
  return epsilon;
}
