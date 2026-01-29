#include "bar_monitor_element.h"

#include "medm_colors.h"
#include "update_coordinator.h"
#include "pv_name_utils.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

namespace {

constexpr double kSampleNormalizedValue = 0.65;
constexpr short kInvalidSeverity = 3;
constexpr int kAxisTickCount = 5;
constexpr qreal kAxisTickLength = 6.0;
constexpr qreal kMinimumTrackExtent = 8.0;
constexpr qreal kMinimumTrackExtentNoDecorations = 1.0;
constexpr qreal kMinimumAxisExtent = 12.0;
constexpr qreal kAxisSpacing = 4.0;
constexpr qreal kBevelWidth = 2.0;
constexpr qreal kLayoutPadding = 3.0;
// Unused: kMinimumLabelPointSize, kFontShrinkFactor, kFontGrowFactor, kLabelTextPadding, kMaxFontSizeIterations

} // namespace

struct BarMonitorElement::Layout
{
  QRectF trackRect;
  QRectF axisRect;
  QRectF readbackRect;
  QRectF channelRect;
  QString channelText;
  QString readbackText;
  QString lowLabel;
  QString highLabel;
  qreal lineHeight = 0.0;
  bool showAxis = false;
  bool showLimits = false;
  bool showReadback = false;
  bool showChannel = false;
  bool vertical = true;
};

BarMonitorElement::BarMonitorElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  limits_.lowDefault = 0.0;
  limits_.highDefault = 100.0;
  limits_.precisionDefault = 1;
  clearRuntimeState();
}

void BarMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool BarMonitorElement::isSelected() const
{
  return selected_;
}

QColor BarMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void BarMonitorElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor BarMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void BarMonitorElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode BarMonitorElement::colorMode() const
{
  return colorMode_;
}

void BarMonitorElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

MeterLabel BarMonitorElement::label() const
{
  return label_;
}

void BarMonitorElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

BarDirection BarMonitorElement::direction() const
{
  return direction_;
}

void BarMonitorElement::setDirection(BarDirection direction)
{
  if (direction_ == direction) {
    return;
  }
  direction_ = direction;
  update();
}

BarFill BarMonitorElement::fillMode() const
{
  return fillMode_;
}

void BarMonitorElement::setFillMode(BarFill mode)
{
  if (fillMode_ == mode) {
    return;
  }
  fillMode_ = mode;
  update();
}

const PvLimits &BarMonitorElement::limits() const
{
  return limits_;
}

void BarMonitorElement::setLimits(const PvLimits &limits)
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
  runtimeLimitsValid_ = false;
  if (!executeMode_) {
    runtimeLow_ = limits_.lowDefault;
    runtimeHigh_ = limits_.highDefault;
    runtimePrecision_ = limits_.precisionDefault;
    runtimeValue_ = defaultSampleValue();
  }
  update();
}

bool BarMonitorElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void BarMonitorElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool BarMonitorElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void BarMonitorElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool BarMonitorElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void BarMonitorElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool BarMonitorElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void BarMonitorElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool BarMonitorElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void BarMonitorElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString BarMonitorElement::channel() const
{
  return channel_;
}

void BarMonitorElement::setChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  setToolTip(channel_.trimmed());
  update();
}

void BarMonitorElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
}

bool BarMonitorElement::isExecuteMode() const
{
  return executeMode_;
}

void BarMonitorElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeSeverity_ = kInvalidSeverity;
    hasRuntimeValue_ = false;
  }
  if (executeMode_) {
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void BarMonitorElement::setRuntimeSeverity(short severity)
{
  short clamped = std::clamp<short>(severity, 0, 3);
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void BarMonitorElement::setRuntimeValue(double value)
{
  if (!std::isfinite(value)) {
    return;
  }
  const double clamped = clampToLimits(value);
  const bool firstValue = !hasRuntimeValue_;
  const bool changed = firstValue
      || std::abs(clamped - runtimeValue_) > valueEpsilon();
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  if (executeMode_ && runtimeConnected_ && changed) {
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void BarMonitorElement::setRuntimeLimits(double low, double high)
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
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void BarMonitorElement::setRuntimePrecision(int precision)
{
  const int clamped = std::clamp(precision, 0, 17);
  if (runtimePrecision_ == clamped) {
    return;
  }
  runtimePrecision_ = clamped;
  if (executeMode_) {
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void BarMonitorElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  runtimeLimitsValid_ = false;
  hasRuntimeValue_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimePrecision_ = -1;
  runtimeValue_ = defaultSampleValue();
  runtimeSeverity_ = kInvalidSeverity;
  if (executeMode_) {
    UpdateCoordinator::instance().requestUpdate(this);
  } else {
    update();
  }
}

void BarMonitorElement::paintEvent(QPaintEvent *event)
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

  // Font setup for scale labels - match MEDM's algorithm:
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
  if (!layout.trackRect.isValid() || layout.trackRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  paintTrack(painter, layout.trackRect);
  paintFill(painter, layout.trackRect);
  if (layout.showAxis) {
    paintAxis(painter, layout);
  }
  paintLabels(painter, layout);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

BarMonitorElement::Layout BarMonitorElement::calculateLayout(
    const QRectF &bounds, const QFontMetricsF &metrics) const
{
  Layout layout;
  layout.vertical = (direction_ == BarDirection::kUp
      || direction_ == BarDirection::kDown);

  if (!bounds.isValid() || bounds.isEmpty()) {
    return layout;
  }

  layout.lineHeight = std::max<qreal>(metrics.height(), 8.0);
  const qreal spacing = std::max<qreal>(layout.lineHeight * 0.25, kAxisSpacing);

  layout.showAxis = (label_ == MeterLabel::kOutline
      || label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel);
  layout.showLimits = (label_ == MeterLabel::kOutline
      || label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel);
  layout.showReadback = (label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel);
  layout.channelText = (label_ == MeterLabel::kChannel)
      ? channel_.trimmed() : QString();
  layout.showChannel = !layout.channelText.isEmpty();

  if (layout.showLimits) {
    layout.lowLabel = axisLabelText(effectiveLowLimit());
    layout.highLabel = axisLabelText(effectiveHighLimit());
  }

  if (layout.showReadback) {
    layout.readbackText = formattedSampleValue();
  }

  const qreal minTrackExtent = (label_ == MeterLabel::kNoDecorations)
      ? kMinimumTrackExtentNoDecorations : kMinimumTrackExtent;

  qreal left = bounds.left();
  qreal right = bounds.right();
  qreal top = bounds.top();
  qreal bottom = bounds.bottom();
  qreal readbackTop = bottom;

  if (layout.vertical) {
    if (layout.showChannel) {
      layout.channelRect = QRectF(left, top, bounds.width(), layout.lineHeight);
      top += layout.lineHeight + spacing;
    }

    // When showing limits (Outline or Limits mode), reserve space for upper limit text
    // The text is centered at the top position, so it extends lineHeight*0.5 above
    if (layout.showLimits && !layout.showChannel) {
      top += layout.lineHeight * 0.5;
    }

    if (layout.showReadback) {
      readbackTop = bottom - layout.lineHeight;
      // Reserve space for readback plus half line height for lower limit text to sit above
      bottom = readbackTop - spacing - (layout.lineHeight * 0.5);
    } else if (layout.showLimits) {
      // When showing limits without readback (Outline mode), reserve space for lower limit text
      // The text is centered at the bottom position, so it extends lineHeight*0.5 below
      bottom -= layout.lineHeight * 0.5;
    }

    if (bottom - top < minTrackExtent) {
      layout.trackRect = QRectF();
      layout.axisRect = QRectF();
      return layout;
    }

    if (layout.showAxis) {
      qreal axisWidth = kMinimumAxisExtent;
      if (layout.showLimits) {
        const qreal lowWidth = metrics.horizontalAdvance(layout.lowLabel);
        const qreal highWidth = metrics.horizontalAdvance(layout.highLabel);
        axisWidth = std::max(axisWidth, lowWidth + 6.0);
        axisWidth = std::max(axisWidth, highWidth + 6.0);
      }
      const qreal available = bounds.width() - spacing - minTrackExtent;
      axisWidth = std::min(axisWidth, std::max<qreal>(available, kMinimumAxisExtent));
      if (axisWidth < kMinimumAxisExtent || axisWidth >= bounds.width()) {
        layout.showAxis = false;
        layout.axisRect = QRectF();
      } else {
        layout.axisRect = QRectF(left, top, axisWidth, bottom - top);
        left = layout.axisRect.right() + spacing;
      }
    }

    const qreal trackWidth = right - left;
    if (trackWidth < minTrackExtent) {
      layout.trackRect = QRectF();
      layout.axisRect = QRectF();
      return layout;
    }
    
    // For vertical bars, extend track to the left by tick length since we removed tick marks
    qreal trackLeft = left;
    qreal trackWidthAdjusted = trackWidth;
    if (layout.showAxis) {
      const qreal tickLength = std::max<qreal>(3.0,
          std::min<qreal>(kAxisTickLength, layout.axisRect.width() * 0.6));
      trackLeft = left - tickLength;
      trackWidthAdjusted = trackWidth + tickLength;
    }
    
    layout.trackRect = QRectF(trackLeft, top, trackWidthAdjusted, bottom - top);

    if (layout.showReadback) {
      // Center readback text across entire widget width
      layout.readbackRect = QRectF(bounds.left(), readbackTop,
          bounds.width(), layout.lineHeight);
    }
  } else {
    if (layout.showChannel) {
      layout.channelRect = QRectF(left, top, bounds.width(), layout.lineHeight);
      top += layout.lineHeight + spacing;
    }

    if (layout.showReadback) {
      readbackTop = bottom - layout.lineHeight;
      bottom = readbackTop - spacing;
    }

    if (bottom - top < minTrackExtent) {
      layout.trackRect = QRectF();
      layout.axisRect = QRectF();
      return layout;
    }

    if (layout.showAxis) {
      qreal axisHeight = std::max(layout.lineHeight + 4.0, kMinimumAxisExtent);
      const qreal available = (bottom - top) - minTrackExtent;
      axisHeight = std::min(axisHeight, std::max<qreal>(available, kMinimumAxisExtent));
      if (axisHeight < kMinimumAxisExtent || axisHeight >= (bottom - top)) {
        layout.showAxis = false;
        layout.axisRect = QRectF();
      } else {
        layout.axisRect = QRectF(left, top, bounds.width(), axisHeight);
        top += axisHeight + spacing;
      }
    }

    const qreal trackHeight = bottom - top;
    if (trackHeight < minTrackExtent) {
      layout.trackRect = QRectF();
      layout.axisRect = QRectF();
      return layout;
    }
    layout.trackRect = QRectF(left, top, bounds.width(), trackHeight);

    if (layout.showReadback) {
      // Center readback text across entire widget width
      layout.readbackRect = QRectF(bounds.left(), readbackTop,
          bounds.width(), layout.lineHeight);
    }
  }

  return layout;
}

void BarMonitorElement::paintTrack(QPainter &painter,
    const QRectF &trackRect) const
{
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(barTrackColor());
  painter.drawRect(trackRect);

  if (label_ != MeterLabel::kNoDecorations) {
    // Paint 2-pixel sunken bevel around track using absolute colors for visibility on dark backgrounds
    QRectF bevelOuter = trackRect.adjusted(0.5, 0.5, -0.5, -0.5);
    
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

void BarMonitorElement::paintFill(QPainter &painter,
    const QRectF &trackRect) const
{
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    return;
  }

  const double normalized = std::clamp(normalizedSampleValue(), 0.0, 1.0);
  QRectF fillRect;

  if (direction_ == BarDirection::kUp || direction_ == BarDirection::kDown) {
    const double length = trackRect.height();
    if (length <= 0.0) {
      return;
    }
    const double d = std::clamp(normalized * length, 0.0, length);
    const double mid = length / 2.0;
    if (fillMode_ == BarFill::kFromCenter) {
      if (direction_ == BarDirection::kUp) {
        if (d >= mid) {
          const double height = d - mid;
          const double top = trackRect.bottom() - d;
          fillRect = QRectF(trackRect.left(), top,
              trackRect.width(), height);
        } else {
          const double height = mid - d;
          const double top = trackRect.bottom() - mid;
          fillRect = QRectF(trackRect.left(), top,
              trackRect.width(), height);
        }
      } else { // Down
        if (d >= mid) {
          const double height = d - mid;
          const double top = trackRect.top() + mid;
          fillRect = QRectF(trackRect.left(), top,
              trackRect.width(), height);
        } else {
          const double height = mid - d;
          const double top = trackRect.top() + d;
          fillRect = QRectF(trackRect.left(), top,
              trackRect.width(), height);
        }
      }
    } else {
      const double height = d;
      if (direction_ == BarDirection::kUp) {
        const double top = trackRect.bottom() - height;
        fillRect = QRectF(trackRect.left(), top,
            trackRect.width(), height);
      } else { // Down
        fillRect = QRectF(trackRect.left(), trackRect.top(),
            trackRect.width(), height);
      }
    }
  } else {
    const double length = trackRect.width();
    if (length <= 0.0) {
      return;
    }
    const double d = std::clamp(normalized * length, 0.0, length);
    const double mid = length / 2.0;
    if (fillMode_ == BarFill::kFromCenter) {
      if (direction_ == BarDirection::kRight) {
        if (d >= mid) {
          const double width = d - mid;
          const double left = trackRect.left() + mid;
          fillRect = QRectF(left, trackRect.top(),
              width, trackRect.height());
        } else {
          const double width = mid - d;
          const double left = trackRect.left() + d;
          fillRect = QRectF(left, trackRect.top(),
              width, trackRect.height());
        }
      } else { // Left
        if (d >= mid) {
          const double width = d - mid;
          const double left = trackRect.right() - d;
          fillRect = QRectF(left, trackRect.top(),
              width, trackRect.height());
        } else {
          const double width = mid - d;
          const double left = trackRect.left() + mid;
          fillRect = QRectF(left, trackRect.top(),
              width, trackRect.height());
        }
      }
    } else {
      const double width = d;
      if (direction_ == BarDirection::kRight) {
        fillRect = QRectF(trackRect.left(), trackRect.top(),
            width, trackRect.height());
      } else { // Left
        const double left = trackRect.right() - width;
        fillRect = QRectF(left, trackRect.top(),
            width, trackRect.height());
      }
    }
  }

  fillRect = fillRect.intersected(trackRect);
  if (!fillRect.isValid() || fillRect.isEmpty()) {
    return;
  }

  // Inset fill by 2 pixels to avoid overlapping the track's sunken bevel
  const qreal bevelInset = (label_ != MeterLabel::kNoDecorations) ? 2.0 : 0.0;
  fillRect = fillRect.adjusted(bevelInset, bevelInset, -bevelInset, -bevelInset);
  
  if (!fillRect.isValid() || fillRect.isEmpty()) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(barFillColor());
  painter.drawRect(fillRect);

  if (label_ != MeterLabel::kNoDecorations) {
    // Add sunken bevel around the fill
    const QColor fillColor = barFillColor();
    const bool isFull = (normalized >= 0.999); // Consider 99.9% as "full" to handle floating point
    
    // Determine which edges to draw based on direction and fill amount
    bool drawTop = false, drawBottom = false, drawLeft = false, drawRight = false;
    
    if (direction_ == BarDirection::kUp) {
      drawTop = true;
      drawLeft = true;
      drawRight = true;
      drawBottom = isFull; // Only draw bottom if completely filled
    } else if (direction_ == BarDirection::kDown) {
      drawBottom = true;
      drawLeft = true;
      drawRight = true;
      drawTop = isFull;
    } else if (direction_ == BarDirection::kRight) {
      drawTop = true;
      drawBottom = true;
      drawRight = true;
      drawLeft = isFull;
    } else { // Left
      drawTop = true;
      drawBottom = true;
      drawLeft = true;
      drawRight = isFull;
    }
    
    QRectF bevelRect = fillRect.adjusted(0.5, 0.5, -0.5, -0.5);
    
    // Draw sunken bevel (darker on top/left, lighter on bottom/right)
    if (drawTop) {
      painter.setPen(QPen(fillColor.darker(160), 1));
      painter.drawLine(bevelRect.topLeft(), bevelRect.topRight());
    }
    if (drawLeft) {
      painter.setPen(QPen(fillColor.darker(160), 1));
      painter.drawLine(bevelRect.topLeft(), bevelRect.bottomLeft());
    }
    if (drawBottom) {
      painter.setPen(QPen(fillColor.lighter(140), 1));
      painter.drawLine(bevelRect.bottomLeft(), bevelRect.bottomRight());
    }
    if (drawRight) {
      painter.setPen(QPen(fillColor.lighter(140), 1));
      painter.drawLine(bevelRect.topRight(), bevelRect.bottomRight());
    }
  }
  painter.restore();
}

void BarMonitorElement::paintAxis(QPainter &painter, const Layout &layout) const
{
  if (!layout.showAxis || !layout.axisRect.isValid() || layout.axisRect.isEmpty()) {
    return;
  }

  painter.save();
  const QColor axisColor(Qt::black);
  QPen axisPen(axisColor);
  axisPen.setWidth(1);
  painter.setPen(axisPen);
  painter.setBrush(Qt::NoBrush);

  const QFontMetricsF metrics(painter.font());

  if (layout.vertical) {
    const qreal axisX = layout.axisRect.right();
    const qreal axisHeight = layout.axisRect.height();
    const qreal tickLength = std::max<qreal>(3.0,
        std::min<qreal>(kAxisTickLength, layout.axisRect.width() * 0.6));

    // Shift axis line left by tickLength to align with tick marks
    const qreal axisLineX = axisX - tickLength;
    painter.drawLine(QPointF(axisLineX, layout.axisRect.top()),
        QPointF(axisLineX, layout.axisRect.bottom()));

    auto positionForNormalized = [&](double normalized) {
      if (direction_ == BarDirection::kUp) {
        return layout.axisRect.bottom() - normalized * axisHeight;
      }
      return layout.axisRect.top() + normalized * axisHeight;
    };

    for (int i = 0; i <= kAxisTickCount; ++i) {
      // Skip top and bottom tick marks for vertical bars
      if (i == 0 || i == kAxisTickCount) {
        continue;
      }
      const double normalized = static_cast<double>(i) / kAxisTickCount;
      const qreal y = positionForNormalized(normalized);
      // Shift tick marks left by tickLength to align with widened track
      painter.drawLine(QPointF(axisX - tickLength * 2.0, y), 
          QPointF(axisX - tickLength, y));
    }

    if (layout.showLimits) {
      const qreal textRight = axisX - tickLength - 2.0;
      const qreal available = std::max<qreal>(textRight - layout.axisRect.left(), 1.0);

      if (!layout.lowLabel.isEmpty()) {
        const qreal yLow = positionForNormalized(0.0);
        QRectF lowRect(layout.axisRect.left(),
            yLow - layout.lineHeight * 0.5, available, layout.lineHeight);
        painter.drawText(lowRect, Qt::AlignRight | Qt::AlignVCenter,
            layout.lowLabel);
      }

      if (!layout.highLabel.isEmpty()) {
        const qreal yHigh = positionForNormalized(1.0);
        QRectF highRect(layout.axisRect.left(),
            yHigh - layout.lineHeight * 0.5, available, layout.lineHeight);
        painter.drawText(highRect, Qt::AlignRight | Qt::AlignVCenter,
            layout.highLabel);
      }
    }
  } else {
    const qreal axisY = layout.axisRect.bottom();
    const qreal axisWidth = layout.axisRect.width();
    const qreal tickLength = std::max<qreal>(3.0,
        std::min<qreal>(kAxisTickLength, layout.axisRect.height() * 0.6));

    painter.drawLine(QPointF(layout.axisRect.left(), axisY),
        QPointF(layout.axisRect.right(), axisY));

    auto positionForNormalized = [&](double normalized) {
      if (direction_ == BarDirection::kRight) {
        return layout.axisRect.left() + normalized * axisWidth;
      }
      return layout.axisRect.right() - normalized * axisWidth;
    };

    for (int i = 0; i <= kAxisTickCount; ++i) {
      const double normalized = static_cast<double>(i) / kAxisTickCount;
      const qreal x = positionForNormalized(normalized);
      painter.drawLine(QPointF(x, axisY), QPointF(x, axisY - tickLength));
    }

    if (layout.showLimits) {
      const qreal textHeight = std::max<qreal>(layout.axisRect.height() - tickLength - 2.0,
          metrics.height());
      const qreal textTop = axisY - tickLength - textHeight;

      if (!layout.lowLabel.isEmpty()) {
        const qreal width = metrics.horizontalAdvance(layout.lowLabel) + 6.0;
        QRectF lowRect((direction_ == BarDirection::kRight)
                ? layout.axisRect.left()
                : layout.axisRect.right() - width,
            textTop, width, textHeight);
        Qt::Alignment align = (direction_ == BarDirection::kRight)
            ? Qt::AlignLeft : Qt::AlignRight;
        painter.drawText(lowRect, align | Qt::AlignVCenter, layout.lowLabel);
      }

      if (!layout.highLabel.isEmpty()) {
        const qreal width = metrics.horizontalAdvance(layout.highLabel) + 6.0;
        QRectF highRect((direction_ == BarDirection::kRight)
                ? layout.axisRect.right() - width
                : layout.axisRect.left(),
            textTop, width, textHeight);
        Qt::Alignment align = (direction_ == BarDirection::kRight)
            ? Qt::AlignRight : Qt::AlignLeft;
        painter.drawText(highRect, align | Qt::AlignVCenter, layout.highLabel);
      }
    }
  }

  painter.restore();
}

void BarMonitorElement::paintLabels(QPainter &painter, const Layout &layout) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  painter.save();
  painter.setPen(Qt::black);
  painter.setBrush(Qt::NoBrush);

  if (label_ == MeterLabel::kOutline && layout.trackRect.isValid()
      && !layout.trackRect.isEmpty()) {
    QPen pen(Qt::black);
    pen.setStyle(Qt::DotLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(layout.trackRect.adjusted(3.0, 3.0, -3.0, -3.0));
  }

  if (layout.showChannel && layout.channelRect.isValid()
      && !layout.channelRect.isEmpty()) {
    painter.drawText(layout.channelRect.adjusted(2.0, 0.0, -2.0, 0.0),
        Qt::AlignHCenter | Qt::AlignVCenter, layout.channelText);
  }

  if (layout.showReadback && layout.readbackRect.isValid()
      && !layout.readbackRect.isEmpty()) {
    // Calculate tight bounding box for readback text
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
    painter.drawText(layout.readbackRect.adjusted(2.0, 0.0, -2.0, 0.0),
        Qt::AlignHCenter | Qt::AlignVCenter, layout.readbackText);
  }

  painter.restore();
}

QColor BarMonitorElement::effectiveForeground() const
{
  if (executeMode_) {
    if (colorMode_ == TextColorMode::kAlarm) {
      if (!runtimeConnected_) {
        return QColor(204, 204, 204);
      }
      return MedmColors::alarmColorForSeverity(runtimeSeverity_);
    }
  }
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

QColor BarMonitorElement::effectiveBackground() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
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

QColor BarMonitorElement::barTrackColor() const
{
  QColor color = effectiveBackground();
  if (!color.isValid()) {
    color = QColor(Qt::white);
  }
  return color;
}

QColor BarMonitorElement::barFillColor() const
{
  QColor color = effectiveForeground();
  if (!color.isValid()) {
    color = QColor(Qt::black);
  }
  return color;
}

double BarMonitorElement::normalizedSampleValue() const
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

double BarMonitorElement::sampleValue() const
{
  return clampToLimits(currentValue());
}

QString BarMonitorElement::formattedSampleValue() const
{
  if (executeMode_) {
    if (!runtimeConnected_ || !hasRuntimeValue_) {
      return QStringLiteral("--");
    }
  }
  return formatValue(sampleValue());
}

double BarMonitorElement::effectiveLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double BarMonitorElement::effectiveHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

int BarMonitorElement::effectivePrecision() const
{
  if (executeMode_ && limits_.precisionSource == PvLimitSource::kChannel
      && runtimePrecision_ >= 0) {
    return std::clamp(runtimePrecision_, 0, 17);
  }
  return std::clamp(limits_.precisionDefault, 0, 17);
}

double BarMonitorElement::currentValue() const
{
  if (executeMode_ && runtimeConnected_ && hasRuntimeValue_) {
    return runtimeValue_;
  }
  return defaultSampleValue();
}

double BarMonitorElement::defaultSampleValue() const
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

double BarMonitorElement::clampToLimits(double value) const
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

QString BarMonitorElement::formatValue(double value, char format, int precision) const
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

QString BarMonitorElement::axisLabelText(double value) const
{
  return formatValue(value, 'f', -1);
}

double BarMonitorElement::valueEpsilon() const
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

void BarMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}
