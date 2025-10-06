#include "bar_monitor_element.h"

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
constexpr int kAxisTickCount = 5;
constexpr qreal kAxisTickLength = 6.0;
constexpr qreal kMinimumTrackExtent = 8.0;
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
  update();
}

void BarMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  painter.fillRect(rect(), effectiveBackground());

  const QRectF contentRect = rect().adjusted(kLayoutPadding, kLayoutPadding,
      -kLayoutPadding, -kLayoutPadding);
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
  layout.channelText = (label_ == MeterLabel::kChannel) ? channel_.trimmed() : QString();
  layout.showChannel = !layout.channelText.isEmpty();

  if (layout.showLimits) {
    layout.lowLabel = QString::number(limits_.lowDefault, 'g', 5);
    layout.highLabel = QString::number(limits_.highDefault, 'g', 5);
  }

  if (layout.showLimits) {
    layout.readbackText = QString::number(0.0, 'f',
        std::clamp(limits_.precisionDefault, 0, 17));
    layout.showReadback = !layout.readbackText.isEmpty();
  }

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

    if (bottom - top < kMinimumTrackExtent) {
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
      const qreal available = bounds.width() - spacing - kMinimumTrackExtent;
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
    if (trackWidth < kMinimumTrackExtent) {
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

    if (bottom - top < kMinimumTrackExtent) {
      layout.trackRect = QRectF();
      layout.axisRect = QRectF();
      return layout;
    }

    if (layout.showAxis) {
      qreal axisHeight = std::max(layout.lineHeight + 4.0, kMinimumAxisExtent);
      const qreal available = (bottom - top) - kMinimumTrackExtent;
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
    if (trackHeight < kMinimumTrackExtent) {
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

  QRectF fillRect = trackRect;
  if (direction_ == BarDirection::kUp || direction_ == BarDirection::kDown) {
    const double totalHeight = trackRect.height();
    if (totalHeight <= 0.0) {
      return;
    }
    const double fillFraction = std::clamp(normalizedSampleValue(), 0.0, 1.0);
    const double fillHeight = std::clamp(totalHeight * fillFraction, 0.0,
        totalHeight);
    if (fillMode_ == BarFill::kFromCenter) {
      const double half = fillHeight / 2.0;
      const double center = trackRect.center().y();
      fillRect.setTop(center - half);
      fillRect.setBottom(center + half);
    } else if (direction_ == BarDirection::kUp) {
      fillRect.setTop(trackRect.bottom() - fillHeight);
    } else { // Down
      fillRect.setBottom(trackRect.top() + fillHeight);
    }
  } else {
    const double totalWidth = trackRect.width();
    if (totalWidth <= 0.0) {
      return;
    }
    const double fillFraction = std::clamp(normalizedSampleValue(), 0.0, 1.0);
    const double fillWidth = std::clamp(totalWidth * fillFraction, 0.0,
        totalWidth);
    if (fillMode_ == BarFill::kFromCenter) {
      const double half = fillWidth / 2.0;
      const double center = trackRect.center().x();
      fillRect.setLeft(center - half);
      fillRect.setRight(center + half);
    } else if (direction_ == BarDirection::kLeft) {
      fillRect.setLeft(trackRect.right() - fillWidth);
    } else { // Right
      fillRect.setRight(trackRect.left() + fillWidth);
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

  QPen edgePen(barFillColor().darker(160));
  edgePen.setWidth(1);
  painter.setPen(edgePen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(fillRect.adjusted(0.5, 0.5, -0.5, -0.5));
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
  return color.darker(110);
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
  return kSampleNormalizedValue;
}

double BarMonitorElement::sampleValue() const
{
  const double normalized = std::clamp(normalizedSampleValue(), 0.0, 1.0);
  const double low = limits_.lowDefault;
  const double high = limits_.highDefault;

  if (!std::isfinite(low) || !std::isfinite(high)) {
    return normalized;
  }
  if (high <= low) {
    return low;
  }
  return low + normalized * (high - low);
}

QString BarMonitorElement::formattedSampleValue() const
{
  const double value = sampleValue();
  const int precision = std::clamp(limits_.precisionDefault, 0, 17);
  if (precision > 0) {
    return QString::number(value, 'f', precision);
  }
  return QString::number(value, 'g', 5);
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
