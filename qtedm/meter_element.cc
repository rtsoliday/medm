#include "meter_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPalette>

namespace {

constexpr double kStartAngleDegrees = 180.0;
constexpr double kSpanAngleDegrees = 180.0;
constexpr int kTickCount = 10;
constexpr double kInnerTickRatio = 0.78;
constexpr double kNeedleRatio = 0.8;
constexpr double kSampleNormalizedValue = 0.65;
constexpr double kDialInsetRatio = 0.14;
constexpr double kMinimumDialHeight = 24.0;
constexpr short kInvalidSeverity = 3;
constexpr double kValueEpsilonFactor = 1e-6;

inline double degreesToRadians(double degrees)
{
  return degrees * M_PI / 180.0;
}

struct MeterLayout
{
  QRectF dialRect;
  QRectF limitsRect;
  QRectF readbackRect;
  QRectF channelRect;
  bool showReadback = false;
};

MeterLayout calculateLayout(const QRectF &bounds, MeterLabel label,
    const QFontMetricsF &metrics)
{
  MeterLayout layout;
  if (!bounds.isValid() || bounds.isEmpty()) {
    return layout;
  }

  const qreal lineHeight = std::max<qreal>(metrics.height(), 1.0);
  const qreal spacing = std::max<qreal>(2.0, lineHeight * 0.2);

  qreal top = bounds.top();
  qreal bottom = bounds.bottom();

  if (label == MeterLabel::kChannel) {
    layout.channelRect = QRectF(bounds.left(), top, bounds.width(), lineHeight);
    top += lineHeight + spacing;
  }

  layout.showReadback = (label == MeterLabel::kLimits || label == MeterLabel::kChannel);

  if (layout.showReadback) {
    layout.readbackRect = QRectF(bounds.left(), bottom - lineHeight,
        bounds.width(), lineHeight);
    bottom -= lineHeight + spacing;
  }

  layout.limitsRect = QRectF(bounds.left(), bottom - lineHeight,
      bounds.width(), lineHeight);
  bottom -= lineHeight + spacing;

  if (bottom <= top) {
    layout.dialRect = QRectF();
    return layout;
  }

  const QRectF dialArea(bounds.left(), top, bounds.width(), bottom - top);
  if (dialArea.height() < kMinimumDialHeight || dialArea.width() < kMinimumDialHeight) {
    layout.dialRect = QRectF();
    return layout;
  }

  const qreal radius = std::min<qreal>(dialArea.width() / 2.0, dialArea.height());
  if (radius <= 0.0) {
    layout.dialRect = QRectF();
    return layout;
  }

  const qreal diameter = radius * 2.0;
  const qreal centerX = dialArea.center().x();
  const qreal baseY = dialArea.bottom();
  layout.dialRect = QRectF(centerX - radius, baseY - radius, diameter, diameter);

  return layout;
}

QColor alarmColorForSeverity(short severity)
{
  switch (severity) {
  case 0:
    return QColor(0, 205, 0);
  case 1:
    return QColor(255, 255, 0);
  case 2:
    return QColor(255, 0, 0);
  case 3:
    return QColor(255, 255, 255);
  default:
    return QColor(204, 204, 204);
  }
}

} // namespace

MeterElement::MeterElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  clearRuntimeState();
  runtimeValue_ = defaultSampleValue();
}

void MeterElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool MeterElement::isSelected() const
{
  return selected_;
}

QColor MeterElement::foregroundColor() const
{
  return foregroundColor_;
}

void MeterElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor MeterElement::backgroundColor() const
{
  return backgroundColor_;
}

void MeterElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode MeterElement::colorMode() const
{
  return colorMode_;
}

void MeterElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

MeterLabel MeterElement::label() const
{
  return label_;
}

void MeterElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

const PvLimits &MeterElement::limits() const
{
  return limits_;
}

void MeterElement::setLimits(const PvLimits &limits)
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

QString MeterElement::channel() const
{
  return channel_;
}

void MeterElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  setToolTip(channel_.trimmed());
  update();
}

void MeterElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
}

bool MeterElement::isExecuteMode() const
{
  return executeMode_;
}

void MeterElement::setRuntimeConnected(bool connected)
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

void MeterElement::setRuntimeSeverity(short severity)
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

void MeterElement::setRuntimeValue(double value)
{
  if (!std::isfinite(value)) {
    return;
  }
  double clamped = clampToLimits(value);
  bool firstValue = !hasRuntimeValue_;
  bool changed = firstValue || std::abs(clamped - runtimeValue_) > meterEpsilon();
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  if (executeMode_ && runtimeConnected_ && changed) {
    update();
  }
}

void MeterElement::setRuntimeLimits(double low, double high)
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

void MeterElement::setRuntimePrecision(int precision)
{
  int clamped = std::clamp(precision, 0, 17);
  if (runtimePrecision_ == clamped) {
    return;
  }
  runtimePrecision_ = clamped;
  if (executeMode_) {
    update();
  }
}

void MeterElement::clearRuntimeState()
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

void MeterElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.fillRect(rect(), effectiveBackground());

  const QRectF bounds = rect().adjusted(6.0, 6.0, -6.0, -6.0);
  if (bounds.width() <= 0.0 || bounds.height() <= 0.0) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  QFont labelFont = painter.font();
  labelFont.setPointSizeF(std::max(8.0, bounds.height() / 8.0));
  painter.setFont(labelFont);
  const QFontMetricsF metrics(labelFont);

  const MeterLayout layout = calculateLayout(bounds, label_, metrics);

  if (layout.dialRect.isValid() && !layout.dialRect.isEmpty()) {
    paintDial(painter, layout.dialRect);
    paintTicks(painter, layout.dialRect);
    paintNeedle(painter, layout.dialRect);
  }
  paintLabels(painter, layout.dialRect, layout.limitsRect,
      layout.readbackRect, layout.channelRect);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor MeterElement::effectiveForeground() const
{
  if (executeMode_) {
    if (colorMode_ == TextColorMode::kAlarm) {
      if (!runtimeConnected_) {
        return QColor(204, 204, 204);
      }
      return alarmColorForSeverity(runtimeSeverity_);
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

QColor MeterElement::effectiveBackground() const
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

double MeterElement::effectiveLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double MeterElement::effectiveHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

int MeterElement::effectivePrecision() const
{
  if (limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
    return std::clamp(limits_.precisionDefault, 0, 17);
  }
  return std::clamp(limits_.precisionDefault, 0, 17);
}

double MeterElement::currentValue() const
{
  if (executeMode_ && runtimeConnected_ && hasRuntimeValue_) {
    return runtimeValue_;
  }
  return defaultSampleValue();
}

double MeterElement::defaultSampleValue() const
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
  const double clamped = std::clamp(kSampleNormalizedValue, 0.0, 1.0);
  return low + span * clamped;
}

double MeterElement::clampToLimits(double value) const
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

double MeterElement::meterEpsilon() const
{
  double span = effectiveHighLimit() - effectiveLowLimit();
  if (!std::isfinite(span)) {
    span = 1.0;
  }
  span = std::abs(span);
  double epsilon = span * kValueEpsilonFactor;
  if (!std::isfinite(epsilon) || epsilon <= 0.0) {
    epsilon = 1e-9;
  }
  return epsilon;
}

QString MeterElement::formatValue(double value) const
{
  if (!std::isfinite(value)) {
    return QStringLiteral("--");
  }
  const int digits = effectivePrecision();
  return QString::number(value, 'f', digits);
}

void MeterElement::paintSelectionOverlay(QPainter &painter)
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MeterElement::paintDial(QPainter &painter, const QRectF &dialRect) const
{
  if (!dialRect.isValid() || dialRect.isEmpty()) {
    return;
  }

  QColor rimColor = effectiveForeground().darker(140);
  QColor faceColor = effectiveBackground().lighter(110);

  QPen rimPen(rimColor);
  rimPen.setWidth(2);
  painter.save();
  painter.setPen(rimPen);
  painter.setBrush(faceColor);

  QPainterPath outer;
  outer.moveTo(dialRect.left(), dialRect.center().y());
  outer.arcTo(dialRect, 180.0, -kSpanAngleDegrees);
  outer.closeSubpath();
  painter.drawPath(outer);

  QRectF inner = dialRect.adjusted(dialRect.width() * kDialInsetRatio,
      dialRect.width() * kDialInsetRatio, -dialRect.width() * kDialInsetRatio,
      -dialRect.width() * kDialInsetRatio);
  if (inner.isValid() && inner.width() > 0.0 && inner.height() > 0.0) {
    QPen innerPen(faceColor.darker(125));
    innerPen.setWidth(1);
    painter.setPen(innerPen);
    painter.setBrush(faceColor.lighter(105));
    QPainterPath innerPath;
    innerPath.moveTo(inner.left(), inner.center().y());
    innerPath.arcTo(inner, 180.0, -kSpanAngleDegrees);
    innerPath.closeSubpath();
    painter.drawPath(innerPath);
  }

  painter.restore();
}

double MeterElement::normalizedSampleValue() const
{
  const double low = effectiveLowLimit();
  const double high = effectiveHighLimit();
  const double value = currentValue();
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

QString MeterElement::formattedSampleValue() const
{
  if (executeMode_) {
    if (!runtimeConnected_ || !hasRuntimeValue_) {
      return QStringLiteral("--");
    }
  }
  return formatValue(currentValue());
}

void MeterElement::paintTicks(QPainter &painter, const QRectF &dialRect) const
{
  if (!dialRect.isValid() || dialRect.isEmpty()) {
    return;
  }

  const QPointF center = dialRect.center();
  const double radius = dialRect.width() / 2.0;
  const QColor tickColor = effectiveForeground().darker(130);
  QPen tickPen(tickColor);
  tickPen.setWidth(2);
  painter.setPen(tickPen);

  for (int i = 0; i <= kTickCount; ++i) {
    const double ratio = static_cast<double>(i) / kTickCount;
    const double angle = degreesToRadians(
        kStartAngleDegrees - ratio * kSpanAngleDegrees);
    const double outerX = center.x() + std::cos(angle) * radius * 0.92;
    const double outerY = center.y() - std::sin(angle) * radius * 0.92;
    const double innerX = center.x() + std::cos(angle) * radius * kInnerTickRatio;
    const double innerY = center.y() - std::sin(angle) * radius * kInnerTickRatio;
    painter.drawLine(QPointF(innerX, innerY), QPointF(outerX, outerY));
  }
}

void MeterElement::paintNeedle(QPainter &painter, const QRectF &dialRect) const
{
  if (!dialRect.isValid() || dialRect.isEmpty()) {
    return;
  }

  const QPointF center = dialRect.center();
  const double radius = dialRect.width() / 2.0;

  const double normalizedValue = std::clamp(normalizedSampleValue(), 0.0, 1.0);
  const double angle = degreesToRadians(
      kStartAngleDegrees - normalizedValue * kSpanAngleDegrees);
  const double tipX = center.x() + std::cos(angle) * radius * kNeedleRatio;
  const double tipY = center.y() - std::sin(angle) * radius * kNeedleRatio;

  QPen needlePen(effectiveForeground());
  needlePen.setWidth(3);
  painter.setPen(needlePen);
  painter.drawLine(center, QPointF(tipX, tipY));

  painter.setBrush(effectiveForeground());
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(center, radius * 0.08, radius * 0.08);
}

void MeterElement::paintLabels(QPainter &painter, const QRectF &dialRect,
    const QRectF &limitsRect, const QRectF &valueRect,
    const QRectF &channelRect) const
{
  const QColor foreground = effectiveForeground();
  painter.save();
  painter.setBrush(Qt::NoBrush);
  painter.setPen(foreground);

  if (label_ == MeterLabel::kOutline && dialRect.isValid() && !dialRect.isEmpty()) {
    QPen outlinePen(foreground.darker(150));
    outlinePen.setStyle(Qt::DotLine);
    outlinePen.setWidth(1);
    painter.setPen(outlinePen);

    QRectF outlineRect = dialRect.adjusted(dialRect.width() * 0.08,
        dialRect.width() * 0.08, -dialRect.width() * 0.08,
        -dialRect.width() * 0.08);
    if (outlineRect.isValid() && outlineRect.width() > 0.0 && outlineRect.height() > 0.0) {
      QPainterPath outlinePath;
      outlinePath.moveTo(outlineRect.left(), outlineRect.center().y());
      outlinePath.arcTo(outlineRect, 180.0, -kSpanAngleDegrees);
      outlinePath.closeSubpath();
      painter.drawPath(outlinePath);
    }
    painter.setPen(foreground);
  }

  if (label_ == MeterLabel::kChannel) {
    const QString text = channel_.trimmed();
    if (!text.isEmpty()) {
      QRectF textRect = channelRect;
      if (!textRect.isValid() || textRect.isEmpty()) {
        textRect = QRectF(rect().left() + 6.0, rect().top() + 4.0,
            rect().width() - 12.0, painter.fontMetrics().height());
      }
      painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, text);
    }
  }

  QRectF limitsArea = limitsRect;
  if (!limitsArea.isValid() || limitsArea.isEmpty()) {
    limitsArea = QRectF(rect().left() + 6.0,
        rect().bottom() - painter.fontMetrics().height() - 6.0,
        rect().width() - 12.0, painter.fontMetrics().height());
  }
  const double lowLimit = effectiveLowLimit();
  const double highLimit = effectiveHighLimit();
  const QString lowText = formatValue(lowLimit);
  const QString highText = formatValue(highLimit);
  painter.drawText(limitsArea, Qt::AlignLeft | Qt::AlignVCenter, lowText);
  painter.drawText(limitsArea, Qt::AlignRight | Qt::AlignVCenter, highText);

  if (label_ == MeterLabel::kLimits || label_ == MeterLabel::kChannel) {
    QRectF valueArea = valueRect;
    if (!valueArea.isValid() || valueArea.isEmpty()) {
      valueArea = QRectF(limitsArea.left(), limitsArea.bottom() + 2.0,
          limitsArea.width(), limitsArea.height());
    }
    const QString valueText = formattedSampleValue();
    painter.drawText(valueArea, Qt::AlignHCenter | Qt::AlignVCenter, valueText);
  }

  painter.restore();
}
