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

namespace {

constexpr int kTickCount = 10;
constexpr double kPointerSampleValue = 0.65;
constexpr qreal kOuterPadding = 4.0;
constexpr qreal kAxisSpacing = 4.0;
constexpr qreal kMinimumChartExtent = 16.0;
constexpr qreal kMinimumAxisExtent = 14.0;
constexpr qreal kOutlineMargin = 4.0;
constexpr qreal kMinimumLabelPointSize = 10.0;
constexpr qreal kFontShrinkFactor = 0.9;
constexpr qreal kFontGrowFactor = 1.05;
constexpr qreal kLabelTextPadding = 2.0;
constexpr int kMaxFontSizeIterations = 12;

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

void ScaleMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  painter.fillRect(rect(), effectiveBackground());

  const QRectF contentRect = rect().adjusted(
      kOuterPadding, kOuterPadding, -kOuterPadding, -kOuterPadding);
  if (!contentRect.isValid() || contentRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const QFont baseFont = painter.font();
  const bool vertical = isVertical();

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
    if (!testLayout.chartRect.isValid() || testLayout.chartRect.isEmpty()) {
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

    if (testLayout.showAxis) {
      if (!testLayout.axisRect.isValid()
          || testLayout.axisRect.isEmpty()) {
        return false;
      }

      if (testLayout.showLimits && testLayout.vertical) {
        const qreal tickLength = std::max<qreal>(3.0,
            std::min<qreal>(testLayout.axisRect.width(), 10.0));
        const qreal available = std::max<qreal>(
            testLayout.axisRect.width() - tickLength - 2.0, 0.0);
        if ((!testLayout.lowLabel.isEmpty() &&
                testMetrics.horizontalAdvance(testLayout.lowLabel) > available)
            || (!testLayout.highLabel.isEmpty() &&
                testMetrics.horizontalAdvance(testLayout.highLabel) > available)) {
          return false;
        }
      }

      if (testLayout.showLimits && !testLayout.vertical) {
        const qreal tickLength = std::max<qreal>(3.0,
            std::min<qreal>(testLayout.axisRect.height(), 10.0));
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
          return width <= std::max<qreal>(
              testLayout.axisRect.width() - 2.0, 0.0);
        };
        if (!fitsHorizontalLabel(testLayout.lowLabel)
            || !fitsHorizontalLabel(testLayout.highLabel)) {
          return false;
        }
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
    chosenPoint = fitsWithPointSize(fallback)
        ? fallback : kMinimumLabelPointSize;
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
  layout.showAxis = (label_ == MeterLabel::kOutline
      || label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel);
  layout.showLimits = (label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel
      || label_ == MeterLabel::kOutline);
  layout.showReadback = (label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel);
  if (layout.showLimits) {
    layout.lowLabel = QString::number(limits_.lowDefault, 'g', 5);
    layout.highLabel = QString::number(limits_.highDefault, 'g', 5);
  }
  if (layout.showReadback) {
    layout.readbackText = QString::number(0.0, 'f',
        std::clamp(limits_.precisionDefault, 0, 17));
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
      top += layout.lineHeight + kAxisSpacing;
    }

    if (layout.showReadback) {
      const qreal readbackTop = bottom - layout.lineHeight;
      if (readbackTop > top) {
        layout.readbackRect = QRectF(left, readbackTop, bounds.width(),
            layout.lineHeight);
        bottom = readbackTop - kAxisSpacing;
      } else {
        layout.showReadback = false;
        layout.readbackRect = QRectF();
      }
    }

    if (bottom <= top) {
      return layout;
    }

    const qreal chartHeight = bottom - top;
    if (chartHeight < kMinimumChartExtent) {
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
      const qreal availableWidth = (right - left) - axisWidth - kAxisSpacing;
      if (availableWidth < kMinimumChartExtent) {
        layout.showAxis = false;
        layout.axisRect = QRectF();
        layout.chartRect = QRectF(left, top, right - left, chartHeight);
      } else {
        layout.axisRect = QRectF(left, top, axisWidth, chartHeight);
        const qreal chartLeft = layout.axisRect.right() + kAxisSpacing;
        layout.chartRect = QRectF(chartLeft, top, availableWidth, chartHeight);
      }
    } else {
      layout.chartRect = QRectF(left, top, right - left, chartHeight);
    }
  } else {
    if (layout.showChannel) {
      layout.channelRect = QRectF(left, top, bounds.width(), layout.lineHeight);
      top += layout.lineHeight + kAxisSpacing;
    }

    if (layout.showReadback) {
      const qreal readbackTop = bottom - layout.lineHeight;
      if (readbackTop > top) {
        layout.readbackRect = QRectF(left, readbackTop, bounds.width(),
            layout.lineHeight);
        bottom = readbackTop - kAxisSpacing;
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
      if (axisHeight + kAxisSpacing >= availableHeight) {
        layout.showAxis = false;
        layout.axisRect = QRectF();
      } else {
        layout.axisRect = QRectF(left, top, bounds.width(), axisHeight);
        top += axisHeight + kAxisSpacing;
        availableHeight = bottom - top;
      }
    }

    if (availableHeight < kMinimumChartExtent) {
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

  QColor frameColor = effectiveForeground().darker(140);
  QColor fillColor = effectiveBackground().lighter(108);

  QPen framePen(frameColor);
  framePen.setWidth(1);
  painter.setPen(framePen);
  painter.setBrush(fillColor);
  painter.drawRect(chartRect);
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
    const qreal tickLength = std::min<qreal>(layout.axisRect.height(), 10.0);

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
  const qreal majorLength = vertical ? chartRect.width() * 0.45
                                     : chartRect.height() * 0.45;
  const qreal tickLength = std::min<qreal>(majorLength, 10.0);

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

  const bool vertical = layout.vertical;
  double ratio = normalizedSampleValue();
  if (isDirectionInverted()) {
    ratio = 1.0 - ratio;
  }
  ratio = std::clamp(ratio, 0.0, 1.0);

  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveForeground());

  if (vertical) {
    const qreal y = layout.chartRect.bottom() - ratio * layout.chartRect.height();
    const qreal arrowWidth = std::min<qreal>(layout.chartRect.width() * 0.8, 14.0);
    const qreal arrowHeight = std::min<qreal>(layout.chartRect.height() * 0.16, 16.0);

    QPen linePen(effectiveForeground());
    linePen.setWidth(2);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(layout.chartRect.left(), y),
        QPointF(layout.chartRect.right(), y));

    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveForeground());
    const qreal baseX = layout.chartRect.left();
    qreal tipX = baseX - arrowWidth;
    if (layout.showAxis && layout.axisRect.isValid()) {
      tipX = std::max(layout.axisRect.left(), tipX);
    }
    QPolygonF arrow;
    arrow << QPointF(tipX, y)
          << QPointF(baseX, y - arrowHeight / 2.0)
          << QPointF(baseX, y + arrowHeight / 2.0);
    painter.drawPolygon(arrow);
  } else {
    const qreal x = layout.chartRect.left() + ratio * layout.chartRect.width();
    const qreal arrowHeight = std::min<qreal>(layout.chartRect.height() * 0.8, 16.0);
    const qreal arrowWidth = std::min<qreal>(layout.chartRect.width() * 0.16, 16.0);

    QPen linePen(effectiveForeground());
    linePen.setWidth(2);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(x, layout.chartRect.top()),
        QPointF(x, layout.chartRect.bottom()));

    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveForeground());
    QPolygonF arrow;
    if (layout.showAxis && layout.axisRect.isValid()) {
      qreal tipY = layout.chartRect.top() - arrowWidth;
      tipY = std::max(layout.axisRect.top(), tipY);
      arrow << QPointF(x, tipY)
            << QPointF(x - arrowHeight / 2.0, layout.chartRect.top())
            << QPointF(x + arrowHeight / 2.0, layout.chartRect.top());
    } else {
      const qreal tipY = layout.chartRect.bottom() + arrowWidth;
      arrow << QPointF(x, tipY)
            << QPointF(x - arrowHeight / 2.0, layout.chartRect.bottom())
            << QPointF(x + arrowHeight / 2.0, layout.chartRect.bottom());
    }
    painter.drawPolygon(arrow);
  }
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
    painter.drawText(layout.readbackRect, Qt::AlignCenter | Qt::AlignVCenter,
        layout.readbackText);
  }

  painter.restore();
}

double ScaleMonitorElement::sampleValue() const
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

QString ScaleMonitorElement::formattedSampleValue() const
{
  const double value = sampleValue();
  const int precision = std::clamp(limits_.precisionDefault, 0, 17);
  if (precision > 0) {
    return QString::number(value, 'f', precision);
  }
  return QString::number(value, 'g', 5);
}

QColor ScaleMonitorElement::effectiveForeground() const
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

QColor ScaleMonitorElement::effectiveBackground() const
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
  return kPointerSampleValue;
}
