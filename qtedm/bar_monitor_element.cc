#include "bar_monitor_element.h"

#include "medm_colors.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFont>
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
constexpr qreal kLayoutPadding = 6.0;
constexpr qreal kMinimumLabelPointSize = 10.0;
constexpr qreal kFontShrinkFactor = 0.9;
constexpr qreal kFontGrowFactor = 1.05;
constexpr qreal kLabelTextPadding = 2.0;
constexpr int kMaxFontSizeIterations = 12;

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

QString BarMonitorElement::channel() const
{
  return channel_;
}

void BarMonitorElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
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
  update();
}

void BarMonitorElement::setRuntimeSeverity(short severity)
{
  short clamped = std::clamp<short>(severity, 0, 3);
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    update();
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
    update();
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
    update();
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
    update();
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
  update();
}

void BarMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  painter.fillRect(rect(), effectiveBackground());

  if (executeMode_ && !runtimeConnected_) {
    painter.fillRect(rect(), Qt::white);
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const qreal padding = (label_ == MeterLabel::kNoDecorations) ? 0.0 : kLayoutPadding;
  const QRectF contentRect = rect().adjusted(padding, padding, -padding, -padding);
  if (!contentRect.isValid() || contentRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const QFont baseFont = painter.font();
  const bool vertical = (direction_ == BarDirection::kUp
      || direction_ == BarDirection::kDown);

  qreal referenceExtent = vertical ? contentRect.width()
      : contentRect.height();
  if (referenceExtent <= 0.0) {
    const qreal secondaryExtent = vertical ? contentRect.height()
        : contentRect.width();
    referenceExtent = (secondaryExtent > 0.0)
        ? secondaryExtent : std::max(contentRect.width(), contentRect.height());
  }
  if (referenceExtent <= 0.0) {
    referenceExtent = 32.0;
  }

  qreal candidatePoint = std::max(kMinimumLabelPointSize,
      referenceExtent / 5.0);
  qreal chosenPoint = -1.0;

  auto fitsWithPointSize = [&](qreal pointSize) -> bool {
    if (pointSize <= 0.0) {
      return false;
    }
    QFont testFont = baseFont;
    testFont.setPointSizeF(pointSize);
    const QFontMetricsF testMetrics(testFont);
    const Layout testLayout = calculateLayout(contentRect, testMetrics);
    if (!testLayout.trackRect.isValid() || testLayout.trackRect.isEmpty()) {
      return false;
    }

    auto fitsSpan = [&](const QString &text, const QRectF &rect) {
      if (text.isEmpty() || !rect.isValid() || rect.isEmpty()) {
        return true;
      }
      const qreal available = std::max<qreal>(rect.width() - kLabelTextPadding,
          0.0);
      return testMetrics.horizontalAdvance(text) <= available;
    };

    if (testLayout.showChannel
        && !fitsSpan(testLayout.channelText, testLayout.channelRect)) {
      return false;
    }
    if (testLayout.showReadback
        && !fitsSpan(testLayout.readbackText, testLayout.readbackRect)) {
      return false;
    }
    if (testLayout.showAxis && testLayout.showLimits && testLayout.vertical) {
      const qreal tickLength = std::max<qreal>(3.0,
          std::min<qreal>(kAxisTickLength, testLayout.axisRect.width() * 0.6));
      const qreal available = std::max<qreal>(
          testLayout.axisRect.width() - tickLength - 2.0, 0.0);
      if ((!testLayout.lowLabel.isEmpty()
              && testMetrics.horizontalAdvance(testLayout.lowLabel) > available)
          || (!testLayout.highLabel.isEmpty()
              && testMetrics.horizontalAdvance(testLayout.highLabel) > available)) {
        return false;
      }
    }
    if (testLayout.showAxis && testLayout.showLimits && !testLayout.vertical) {
      const qreal tickLength = std::max<qreal>(3.0,
          std::min<qreal>(kAxisTickLength, testLayout.axisRect.height() * 0.6));
      const qreal textHeight = std::max<qreal>(
          testLayout.axisRect.height() - tickLength - 2.0,
          testMetrics.height());
      if (testMetrics.height() > textHeight) {
        return false;
      }
      auto fitsHorizontalLabel = [&](const QString &label) {
        if (label.isEmpty()) {
          return true;
        }
        const qreal width = testMetrics.horizontalAdvance(label) + 6.0;
        return width <= std::max<qreal>(testLayout.axisRect.width() - 2.0, 0.0);
      };
      if (!fitsHorizontalLabel(testLayout.lowLabel)
          || !fitsHorizontalLabel(testLayout.highLabel)) {
        return false;
      }
    }
    return true;
  };

  for (int i = 0; i < kMaxFontSizeIterations; ++i) {
    if (fitsWithPointSize(candidatePoint)) {
      chosenPoint = candidatePoint;
      break;
    }
    if (candidatePoint <= kMinimumLabelPointSize) {
      break;
    }
    candidatePoint = std::max(kMinimumLabelPointSize,
        candidatePoint * kFontShrinkFactor);
  }

  if (chosenPoint < 0.0) {
    const qreal fallback = std::max(kMinimumLabelPointSize,
        referenceExtent / 10.0);
    chosenPoint = fitsWithPointSize(fallback) ? fallback : kMinimumLabelPointSize;
  }

  if (chosenPoint > 0.0) {
    for (int i = 0; i < kMaxFontSizeIterations; ++i) {
      const qreal nextPoint = chosenPoint * kFontGrowFactor;
      if (nextPoint <= chosenPoint) {
        break;
      }
      if (!fitsWithPointSize(nextPoint)) {
        break;
      }
      chosenPoint = nextPoint;
    }
  }

  QFont labelFont = baseFont;
  labelFont.setPointSizeF(chosenPoint);
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
  layout.showReadback = (label_ == MeterLabel::kOutline
      || label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel);
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
    layout.trackRect = QRectF(left, top, trackWidth, bottom - top);

    if (layout.showReadback) {
      layout.readbackRect = QRectF(layout.trackRect.left(), readbackTop,
          layout.trackRect.width(), layout.lineHeight);
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
      layout.readbackRect = QRectF(layout.trackRect.left(), readbackTop,
          layout.trackRect.width(), layout.lineHeight);
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
    QPen framePen(Qt::black);
    framePen.setWidth(1);
    painter.setPen(framePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(trackRect.adjusted(0.5, 0.5, -0.5, -0.5));
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

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(barFillColor());
  painter.drawRect(fillRect);

  if (label_ != MeterLabel::kNoDecorations) {
    QPen edgePen(barFillColor().darker(160));
    edgePen.setWidth(1);
    painter.setPen(edgePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(fillRect.adjusted(0.5, 0.5, -0.5, -0.5));
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

    painter.drawLine(QPointF(axisX, layout.axisRect.top()),
        QPointF(axisX, layout.axisRect.bottom()));

    auto positionForNormalized = [&](double normalized) {
      if (direction_ == BarDirection::kUp) {
        return layout.axisRect.bottom() - normalized * axisHeight;
      }
      return layout.axisRect.top() + normalized * axisHeight;
    };

    for (int i = 0; i <= kAxisTickCount; ++i) {
      const double normalized = static_cast<double>(i) / kAxisTickCount;
      const qreal y = positionForNormalized(normalized);
      painter.drawLine(QPointF(axisX - tickLength, y), QPointF(axisX, y));
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
  if (limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
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
  return formatValue(value, 'g', 5);
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
