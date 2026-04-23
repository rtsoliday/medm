#include "thermometer_element.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <QApplication>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

#include <cvtFast.h>

#include "medm_colors.h"
#include "pv_name_utils.h"
#include "text_format_utils.h"
namespace {

constexpr double kSampleNormalizedValue = 0.65;
constexpr short kInvalidSeverity = 3;
constexpr int kAxisTickCount = 5;
constexpr qreal kAxisTickLength = 6.0;
constexpr qreal kMinimumTrackExtent = 24.0;
constexpr qreal kMinimumTrackExtentNoDecorations = 12.0;
constexpr qreal kMinimumAxisExtent = 12.0;
constexpr qreal kAxisSpacing = 4.0;
constexpr qreal kLayoutPadding = 4.0;
constexpr qreal kMinimumStemWidth = 4.0;
constexpr qreal kMinimumBulbDiameter = 10.0;

bool useLightOverlayText(const QColor &background)
{
  if (!background.isValid()) {
    return false;
  }
  const QColor rgb = background.toRgb();
  const int luma = (299 * rgb.red() + 587 * rgb.green() + 114 * rgb.blue())
      / 1000;
  return luma <= 56;
}

QColor blendColors(const QColor &first, const QColor &second, qreal ratio)
{
  const qreal clamped = std::clamp<qreal>(ratio, 0.0, 1.0);
  return QColor::fromRgbF(
      first.redF() * (1.0 - clamped) + second.redF() * clamped,
      first.greenF() * (1.0 - clamped) + second.greenF() * clamped,
      first.blueF() * (1.0 - clamped) + second.blueF() * clamped,
      first.alphaF() * (1.0 - clamped) + second.alphaF() * clamped);
}

QColor withAlpha(const QColor &color, int alpha)
{
  QColor adjusted = color;
  adjusted.setAlpha(std::clamp(alpha, 0, 255));
  return adjusted;
}

QPainterPath makeThermometerPath(const QRectF &stemRect, const QRectF &bulbRect)
{
  QPainterPath stemPath;
  stemPath.addRoundedRect(stemRect, stemRect.width() * 0.5,
      stemRect.width() * 0.5);
  QPainterPath bulbPath;
  bulbPath.addEllipse(bulbRect);
  return stemPath.united(bulbPath);
}

QString formatNumericValue(TextMonitorFormat format, double value, int precision)
{
  if (!std::isfinite(value)) {
    return QStringLiteral("--");
  }

  const unsigned short digits = static_cast<unsigned short>(
      TextFormatUtils::clampPrecision(precision));
  char buffer[TextFormatUtils::kMaxTextField];
  buffer[0] = '\0';

  switch (format) {
  case TextMonitorFormat::kExponential:
    std::snprintf(buffer, sizeof(buffer), "%.*e", digits, value);
    return QString::fromLatin1(buffer);
  case TextMonitorFormat::kEngineering:
    TextFormatUtils::localCvtDoubleToExpNotationString(value, buffer, digits);
    return QString::fromLatin1(buffer);
  case TextMonitorFormat::kCompact:
    cvtDoubleToCompactString(value, buffer, digits);
    return QString::fromLatin1(buffer);
  case TextMonitorFormat::kTruncated:
    cvtLongToString(static_cast<long>(value), buffer);
    return QString::fromLatin1(buffer);
  case TextMonitorFormat::kHexadecimal:
    return TextFormatUtils::formatHex(static_cast<long>(value));
  case TextMonitorFormat::kOctal:
    return TextFormatUtils::formatOctal(static_cast<long>(value));
  case TextMonitorFormat::kSexagesimal:
    return TextFormatUtils::makeSexagesimal(value, digits);
  case TextMonitorFormat::kSexagesimalHms:
    return TextFormatUtils::makeSexagesimal(
        value * 12.0 / TextFormatUtils::kPi, digits);
  case TextMonitorFormat::kSexagesimalDms:
    return TextFormatUtils::makeSexagesimal(
        value * 180.0 / TextFormatUtils::kPi, digits);
  case TextMonitorFormat::kString:
  case TextMonitorFormat::kDecimal:
    cvtDoubleToString(value, buffer, digits);
    return QString::fromLatin1(buffer);
  }

  cvtDoubleToString(value, buffer, digits);
  return QString::fromLatin1(buffer);
}

} // namespace

struct ThermometerElement::Layout
{
  QRectF bodyRect;
  QRectF axisRect;
  QRectF readbackRect;
  QRectF channelRect;
  QRectF outerStemRect;
  QRectF outerBulbRect;
  QRectF innerStemRect;
  QRectF innerBulbRect;
  QPainterPath outerPath;
  QPainterPath innerPath;
  QString channelText;
  QString readbackText;
  QString lowLabel;
  QString highLabel;
  qreal scaleTop = 0.0;
  qreal scaleBottom = 0.0;
  qreal lineHeight = 0.0;
  bool showAxis = false;
  bool showLimits = false;
  bool showReadback = false;
  bool showChannel = false;
};

ThermometerElement::ThermometerElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  foregroundColor_ = MedmColors::palette()[24];
  limits_.lowDefault = 0.0;
  limits_.highDefault = 100.0;
  limits_.precisionDefault = 1;
  clearRuntimeState();
}

void ThermometerElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ThermometerElement::isSelected() const
{
  return selected_;
}

QColor ThermometerElement::foregroundColor() const
{
  return foregroundColor_;
}

void ThermometerElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor ThermometerElement::backgroundColor() const
{
  return backgroundColor_;
}

void ThermometerElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

QColor ThermometerElement::textColor() const
{
  return textColor_;
}

void ThermometerElement::setTextColor(const QColor &color)
{
  if (textColor_ == color) {
    return;
  }
  textColor_ = color;
  update();
}

TextColorMode ThermometerElement::colorMode() const
{
  return colorMode_;
}

void ThermometerElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

TextVisibilityMode ThermometerElement::visibilityMode() const
{
  return visibilityMode_;
}

void ThermometerElement::setVisibilityMode(TextVisibilityMode mode)
{
  if (visibilityMode_ == mode) {
    return;
  }
  visibilityMode_ = mode;
  update();
}

QString ThermometerElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void ThermometerElement::setVisibilityCalc(const QString &calc)
{
  const QString trimmed = calc.trimmed();
  if (visibilityCalc_ == trimmed) {
    return;
  }
  visibilityCalc_ = trimmed;
  update();
}

MeterLabel ThermometerElement::label() const
{
  return label_;
}

void ThermometerElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

BarDirection ThermometerElement::direction() const
{
  return direction_;
}

void ThermometerElement::setDirection(BarDirection direction)
{
  Q_UNUSED(direction);
  if (direction_ == BarDirection::kUp) {
    return;
  }
  direction_ = BarDirection::kUp;
  update();
}

TextMonitorFormat ThermometerElement::format() const
{
  return format_;
}

void ThermometerElement::setFormat(TextMonitorFormat format)
{
  if (format_ == format) {
    return;
  }
  format_ = format;
  update();
}

bool ThermometerElement::showValue() const
{
  return showValue_;
}

void ThermometerElement::setShowValue(bool showValue)
{
  if (showValue_ == showValue) {
    return;
  }
  showValue_ = showValue;
  update();
}

const PvLimits &ThermometerElement::limits() const
{
  return limits_;
}

double ThermometerElement::displayLowLimit() const
{
  return effectiveLowLimit();
}

double ThermometerElement::displayHighLimit() const
{
  return effectiveHighLimit();
}

void ThermometerElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  runtimeLimitsValid_ = false;
  if (!executeMode_) {
    runtimeLow_ = limits_.lowDefault;
    runtimeHigh_ = limits_.highDefault;
    runtimePrecision_ = limits_.precisionDefault;
    runtimeValue_ = defaultSampleValue();
  }
  update();
}

bool ThermometerElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void ThermometerElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool ThermometerElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void ThermometerElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool ThermometerElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void ThermometerElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool ThermometerElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void ThermometerElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool ThermometerElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void ThermometerElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString ThermometerElement::channel() const
{
  return channel_;
}

void ThermometerElement::setChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  setToolTip(QString());
  update();
}

QString ThermometerElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(visibilityChannels_.size())) {
    return QString();
  }
  return visibilityChannels_[static_cast<std::size_t>(index)];
}

void ThermometerElement::setChannel(int index, const QString &channel)
{
  if (index < 0 || index >= static_cast<int>(visibilityChannels_.size())) {
    return;
  }
  const QString normalized = PvNameUtils::normalizePvName(channel);
  QString &target = visibilityChannels_[static_cast<std::size_t>(index)];
  if (target == normalized) {
    return;
  }
  target = normalized;
  update();
}

void ThermometerElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
  applyRuntimeVisibility();
}

bool ThermometerElement::isExecuteMode() const
{
  return executeMode_;
}

void ThermometerElement::setRuntimeConnected(bool connected)
{
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
  if (executeMode_) {
    update();
  }
}

void ThermometerElement::setRuntimeSeverity(short severity)
{
  const short clamped = std::clamp<short>(severity, 0, 3);
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    update();
  }
}

void ThermometerElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  applyRuntimeVisibility();
}

void ThermometerElement::setRuntimeValue(double value)
{
  if (!std::isfinite(value)) {
    return;
  }
  const bool firstValue = !hasRuntimeValue_;
  const bool changed = firstValue
      || std::abs(value - runtimeValue_) > valueEpsilon();
  runtimeValue_ = value;
  hasRuntimeValue_ = true;
  if (executeMode_ && runtimeConnected_ && changed) {
    update();
  }
}

void ThermometerElement::setRuntimeLimits(double low, double high)
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
    update();
  }
}

void ThermometerElement::setRuntimePrecision(int precision)
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

void ThermometerElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  runtimeLimitsValid_ = false;
  hasRuntimeValue_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimePrecision_ = -1;
  runtimeValue_ = defaultSampleValue();
  runtimeSeverity_ = kInvalidSeverity;
  runtimeVisible_ = true;
  direction_ = BarDirection::kUp;
  applyRuntimeVisibility();
  update();
}

void ThermometerElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.fillRect(rect(), effectiveBackground());

  if (executeMode_ && !runtimeConnected_) {
    painter.fillRect(rect(), Qt::white);
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const qreal padding = (label_ == MeterLabel::kNoDecorations) ? 1.0
      : kLayoutPadding;
  const QRectF contentRect = rect().adjusted(padding, padding, -padding, -padding);
  if (!contentRect.isValid() || contentRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  QFont labelFont = painter.font();
  const int preferredPixelHeight = static_cast<int>(
      std::max(1.0, rect().height() / 9.0));
  labelFont.setPixelSize(std::clamp(preferredPixelHeight, 4, 34));
  painter.setFont(labelFont);
  const QFontMetricsF metrics(labelFont);

  const Layout layout = calculateLayout(contentRect, metrics);
  if (layout.outerPath.isEmpty() || layout.innerPath.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  paintTrack(painter, layout);
  paintFill(painter, layout);
  if (layout.showAxis) {
    paintAxis(painter, layout);
  }
  paintLabels(painter, layout);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

ThermometerElement::Layout ThermometerElement::calculateLayout(
    const QRectF &bounds, const QFontMetricsF &metrics) const
{
  Layout layout;

  if (!bounds.isValid() || bounds.isEmpty()) {
    return layout;
  }

  layout.lineHeight = std::max<qreal>(metrics.height(), 8.0);
  const qreal spacing = std::max<qreal>(layout.lineHeight * 0.25, kAxisSpacing);
  layout.showAxis = (label_ == MeterLabel::kOutline
      || label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel);
  layout.showLimits = layout.showAxis;
  layout.showReadback = showValue_
      && label_ != MeterLabel::kNone
      && label_ != MeterLabel::kNoDecorations;
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
  const qreal right = bounds.right();
  qreal top = bounds.top();
  qreal bottom = bounds.bottom();
  qreal readbackTop = bottom;

  if (layout.showChannel) {
    layout.channelRect = QRectF(left, top, bounds.width(), layout.lineHeight);
    top += layout.lineHeight + spacing;
  }
  if (layout.showLimits && !layout.showChannel) {
    top += layout.lineHeight * 0.5;
  }
  if (layout.showReadback) {
    readbackTop = bottom - layout.lineHeight;
    bottom = readbackTop - spacing - (layout.lineHeight * 0.5);
  } else if (layout.showLimits) {
    bottom -= layout.lineHeight * 0.5;
  }
  if (bottom - top < minTrackExtent) {
    return layout;
  }

  if (layout.showAxis) {
    qreal axisWidth = kMinimumAxisExtent;
    if (layout.showLimits) {
      axisWidth = std::max(axisWidth,
          metrics.horizontalAdvance(layout.lowLabel) + 8.0);
      axisWidth = std::max(axisWidth,
          metrics.horizontalAdvance(layout.highLabel) + 8.0);
    }
    const qreal available = bounds.width() - spacing - minTrackExtent;
    axisWidth = std::min(axisWidth,
        std::max<qreal>(available, kMinimumAxisExtent));
    if (axisWidth >= bounds.width()) {
      layout.showAxis = false;
      layout.showLimits = false;
    } else {
      layout.axisRect = QRectF(left, top, axisWidth, bottom - top);
      left = layout.axisRect.right() + spacing;
    }
  }

  layout.bodyRect = QRectF(left, top, right - left, bottom - top);
  if (!layout.bodyRect.isValid() || layout.bodyRect.width() < minTrackExtent
      || layout.bodyRect.height() < minTrackExtent) {
    return layout;
  }

  const qreal bodyWidth = layout.bodyRect.width();
  const qreal bodyHeight = layout.bodyRect.height();
  qreal bulbDiameter = std::min(bodyWidth * 0.8, bodyHeight * 0.3);
  bulbDiameter = std::min(bulbDiameter, bodyWidth);
  if (bulbDiameter < kMinimumBulbDiameter) {
    bulbDiameter = std::min(bodyWidth, bodyHeight);
  }

  qreal stemWidth = std::max<qreal>(kMinimumStemWidth, bulbDiameter * 0.33);
  stemWidth = std::min(stemWidth, bodyWidth * 0.5);
  if (stemWidth < kMinimumStemWidth || bulbDiameter < kMinimumBulbDiameter) {
    return layout;
  }

  const qreal centerX = layout.bodyRect.center().x();
  layout.outerBulbRect = QRectF(centerX - bulbDiameter * 0.5,
      layout.bodyRect.bottom() - bulbDiameter, bulbDiameter, bulbDiameter);

  const qreal stemTop = layout.bodyRect.top();
  const qreal stemBottom = layout.outerBulbRect.center().y();
  if (stemBottom - stemTop < minTrackExtent) {
    return layout;
  }
  layout.outerStemRect = QRectF(centerX - stemWidth * 0.5, stemTop,
      stemWidth, stemBottom - stemTop);

  const qreal glassInset = std::max<qreal>(1.5, stemWidth * 0.16);
  layout.innerBulbRect = layout.outerBulbRect.adjusted(glassInset, glassInset,
      -glassInset, -glassInset);
  const qreal innerStemWidth = std::max<qreal>(2.0, stemWidth - 2.0 * glassInset);
  const qreal innerStemTop = layout.outerStemRect.top() + glassInset;
  const qreal innerStemBottom = layout.innerBulbRect.center().y();
  if (innerStemBottom - innerStemTop < 4.0) {
    return layout;
  }
  layout.innerStemRect = QRectF(centerX - innerStemWidth * 0.5, innerStemTop,
      innerStemWidth, innerStemBottom - innerStemTop);

  layout.outerPath = makeThermometerPath(layout.outerStemRect, layout.outerBulbRect);
  layout.innerPath = makeThermometerPath(layout.innerStemRect, layout.innerBulbRect);
  layout.scaleTop = layout.innerStemRect.top();
  layout.scaleBottom = layout.innerBulbRect.top();

  if (layout.showReadback) {
    layout.readbackRect = QRectF(bounds.left(), readbackTop,
        bounds.width(), layout.lineHeight);
  }

  return layout;
}

void ThermometerElement::paintTrack(QPainter &painter,
    const Layout &layout) const
{
  if (layout.outerPath.isEmpty() || layout.innerPath.isEmpty()) {
    return;
  }

  const QColor base = trackColor();
  const QColor glassEdge = blendColors(base.darker(180), QColor(Qt::black), 0.25);
  const QColor cavityBase = blendColors(base, QColor(Qt::white), 0.18);

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);

  QLinearGradient outerGradient(layout.bodyRect.left(), 0.0,
      layout.bodyRect.right(), 0.0);
  outerGradient.setColorAt(0.0, base.lighter(145));
  outerGradient.setColorAt(0.35, base.lighter(118));
  outerGradient.setColorAt(1.0, base.darker(110));
  painter.fillPath(layout.outerPath, outerGradient);

  QLinearGradient innerGradient(layout.innerStemRect.left(), 0.0,
      layout.innerStemRect.right(), 0.0);
  innerGradient.setColorAt(0.0, cavityBase.lighter(120));
  innerGradient.setColorAt(0.5, cavityBase);
  innerGradient.setColorAt(1.0, cavityBase.darker(106));
  painter.fillPath(layout.innerPath, innerGradient);

  painter.setPen(QPen(glassEdge, 1.0));
  painter.drawPath(layout.outerPath);
  painter.setPen(QPen(withAlpha(Qt::white, 120), 1.0));
  painter.drawLine(QPointF(layout.outerStemRect.left() + 0.8,
          layout.outerStemRect.top() + 1.0),
      QPointF(layout.outerStemRect.left() + 0.8,
          layout.outerStemRect.bottom() - 1.0));
  painter.drawArc(layout.outerBulbRect.adjusted(1.0, 1.0, -1.0, -1.0),
      135 * 16, 90 * 16);

  painter.setPen(QPen(withAlpha(Qt::black, 55), 1.0));
  painter.drawPath(layout.innerPath);

  painter.restore();
}

void ThermometerElement::paintFill(QPainter &painter,
    const Layout &layout) const
{
  if (layout.innerPath.isEmpty()) {
    return;
  }
  if (executeMode_ && (!runtimeConnected_ || !hasRuntimeValue_)) {
    return;
  }

  const double normalized = std::clamp(normalizedSampleValue(), 0.0, 1.0);
  const QColor liquidColor = fillColor();
  QPainterPath liquidPath;
  liquidPath.addEllipse(layout.innerBulbRect);

  QRectF liquidStemRect;
  const qreal liquidSpan = std::max<qreal>(
      layout.scaleBottom - layout.scaleTop, 0.0);
  const qreal liquidHeight = normalized * liquidSpan;
  if (liquidHeight > 0.0) {
    liquidStemRect = QRectF(layout.innerStemRect.left(),
        layout.scaleBottom - liquidHeight,
        layout.innerStemRect.width(), liquidHeight)
            .intersected(layout.innerStemRect);
    if (liquidStemRect.isValid() && !liquidStemRect.isEmpty()) {
      QPainterPath columnPath;
      columnPath.addRoundedRect(liquidStemRect,
          layout.innerStemRect.width() * 0.5,
          layout.innerStemRect.width() * 0.5);
      liquidPath = liquidPath.united(columnPath);
    }
  }
  liquidPath = liquidPath.intersected(layout.innerPath);
  if (liquidPath.isEmpty()) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);

  QLinearGradient liquidGradient(layout.innerStemRect.left(), 0.0,
      layout.innerStemRect.right(), 0.0);
  liquidGradient.setColorAt(0.0, liquidColor.lighter(150));
  liquidGradient.setColorAt(0.45, liquidColor);
  liquidGradient.setColorAt(1.0, liquidColor.darker(120));
  painter.fillPath(liquidPath, liquidGradient);

  painter.setClipPath(liquidPath);
  painter.setPen(QPen(withAlpha(Qt::white, 90), 1.0));
  painter.drawLine(QPointF(layout.innerStemRect.left() + 0.8,
          layout.innerStemRect.top() + 1.0),
      QPointF(layout.innerStemRect.left() + 0.8,
          layout.innerStemRect.bottom() - 1.0));
  painter.drawArc(layout.innerBulbRect.adjusted(1.0, 1.0, -1.0, -1.0),
      135 * 16, 90 * 16);

  if (liquidStemRect.isValid() && !liquidStemRect.isEmpty()) {
    const qreal meniscusHeight = std::max<qreal>(2.0,
        layout.innerStemRect.width() * 0.5);
    QRectF meniscusRect(layout.innerStemRect.left() + 0.5,
        liquidStemRect.top() - meniscusHeight * 0.45,
        std::max<qreal>(1.0, layout.innerStemRect.width() - 1.0),
        meniscusHeight);
    painter.setBrush(withAlpha(liquidColor.lighter(110), 110));
    painter.drawEllipse(meniscusRect);
  }

  painter.restore();
}

void ThermometerElement::paintAxis(QPainter &painter, const Layout &layout) const
{
  if (!layout.showAxis || !layout.axisRect.isValid() || layout.axisRect.isEmpty()) {
    return;
  }

  painter.save();
  const QColor axisColor = useLightOverlayText(effectiveBackground())
      ? QColor(Qt::white) : QColor(Qt::black);
  painter.setPen(QPen(axisColor, 1));
  painter.setBrush(Qt::NoBrush);
  const qreal tickLength = std::max<qreal>(3.0,
      std::min<qreal>(kAxisTickLength, layout.axisRect.width() * 0.6));
  const qreal axisLineX = layout.outerBulbRect.left();
  const qreal span = std::max<qreal>(layout.scaleBottom - layout.scaleTop, 1.0);

  painter.drawLine(QPointF(axisLineX, layout.scaleTop),
      QPointF(axisLineX, layout.scaleBottom));

  for (int i = 0; i <= kAxisTickCount; ++i) {
    const qreal normalized = static_cast<qreal>(i) / kAxisTickCount;
    const qreal y = layout.scaleBottom - normalized * span;
    painter.drawLine(QPointF(axisLineX - tickLength, y),
        QPointF(axisLineX, y));
  }

  if (layout.showLimits) {
    const QFontMetricsF metrics(painter.font());
    const qreal textRight = axisLineX
        - std::max<qreal>(2.0, metrics.horizontalAdvance(QLatin1Char('0')) * 0.5);
    const qreal available = std::max<qreal>(
        textRight - layout.axisRect.left(), 1.0);
    if (!layout.lowLabel.isEmpty()) {
      QRectF lowRect(layout.axisRect.left(),
          layout.scaleBottom - layout.lineHeight * 0.5,
          available, layout.lineHeight);
      painter.drawText(lowRect, Qt::AlignRight | Qt::AlignVCenter,
          layout.lowLabel);
    }
    if (!layout.highLabel.isEmpty()) {
      QRectF highRect(layout.axisRect.left(),
          layout.scaleTop - layout.lineHeight * 0.5,
          available, layout.lineHeight);
      painter.drawText(highRect, Qt::AlignRight | Qt::AlignVCenter,
          layout.highLabel);
    }
  }

  painter.restore();
}

void ThermometerElement::paintLabels(QPainter &painter,
    const Layout &layout) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  painter.save();
  const QColor overlayTextColor = textColor_.isValid()
      ? textColor_
      : (useLightOverlayText(effectiveBackground())
              ? QColor(Qt::white)
              : QColor(Qt::black));
  const QColor readbackTextColor = textColor_.isValid()
      ? textColor_
      : QColor(Qt::black);
  painter.setBrush(Qt::NoBrush);

  if (label_ == MeterLabel::kOutline && !layout.outerPath.isEmpty()) {
    QPen pen(Qt::black);
    pen.setStyle(Qt::DotLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawPath(layout.outerPath);
  }

  if (layout.showChannel && layout.channelRect.isValid()
      && !layout.channelRect.isEmpty()) {
    painter.setPen(overlayTextColor);
    painter.drawText(layout.channelRect.adjusted(2.0, 0.0, -2.0, 0.0),
        Qt::AlignHCenter | Qt::AlignVCenter, layout.channelText);
  }

  if (layout.showReadback && layout.readbackRect.isValid()
      && !layout.readbackRect.isEmpty()) {
    const QFontMetricsF fm(painter.font());
    const qreal textWidth = fm.boundingRect(layout.readbackText).width();
    const qreal padding = 4.0;
    const qreal bgWidth = textWidth + padding;
    const qreal centerX = layout.readbackRect.center().x();
    const qreal bgLeft = centerX - bgWidth * 0.5;
    QRectF bgRect(bgLeft, layout.readbackRect.top(),
        bgWidth, layout.readbackRect.height());
    painter.fillRect(bgRect, Qt::white);
    painter.setPen(readbackTextColor);
    painter.drawText(layout.readbackRect.adjusted(2.0, 0.0, -2.0, 0.0),
        Qt::AlignHCenter | Qt::AlignVCenter, layout.readbackText);
  }

  painter.restore();
}

QColor ThermometerElement::effectiveForeground() const
{
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    return MedmColors::alarmColorForSeverity(runtimeSeverity_);
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

QColor ThermometerElement::effectiveBackground() const
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

QColor ThermometerElement::trackColor() const
{
  QColor color = effectiveBackground();
  if (!color.isValid()) {
    color = QColor(Qt::white);
  }
  return color;
}

QColor ThermometerElement::fillColor() const
{
  QColor color = effectiveForeground();
  if (!color.isValid()) {
    color = QColor(Qt::black);
  }
  return color;
}

double ThermometerElement::normalizedSampleValue() const
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
  return std::clamp((value - low) / span, 0.0, 1.0);
}

double ThermometerElement::sampleValue() const
{
  return clampToLimits(currentValue());
}

QString ThermometerElement::formattedSampleValue() const
{
  if (executeMode_ && (!runtimeConnected_ || !hasRuntimeValue_)) {
    return QStringLiteral("--");
  }
  return formatValue(currentValue());
}

double ThermometerElement::effectiveLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double ThermometerElement::effectiveHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

int ThermometerElement::effectivePrecision() const
{
  if (executeMode_ && limits_.precisionSource == PvLimitSource::kChannel
      && runtimePrecision_ >= 0) {
    return std::clamp(runtimePrecision_, 0, 17);
  }
  return std::clamp(limits_.precisionDefault, 0, 17);
}

double ThermometerElement::currentValue() const
{
  if (executeMode_ && runtimeConnected_ && hasRuntimeValue_) {
    return runtimeValue_;
  }
  return defaultSampleValue();
}

double ThermometerElement::defaultSampleValue() const
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
  return low + span * std::clamp(kSampleNormalizedValue, 0.0, 1.0);
}

double ThermometerElement::clampToLimits(double value) const
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

QString ThermometerElement::formatValue(double value) const
{
  return formatNumericValue(format_, value, effectivePrecision());
}

QString ThermometerElement::axisLabelText(double value) const
{
  return formatNumericValue(TextMonitorFormat::kDecimal, value, effectivePrecision());
}

double ThermometerElement::valueEpsilon() const
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

void ThermometerElement::applyRuntimeVisibility()
{
  QWidget::setVisible(!executeMode_ || runtimeVisible_);
}

void ThermometerElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}
