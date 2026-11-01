#include "scale_monitor_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"
#include "pv_name_utils.h"
#include "medm_widget_state.h"

namespace {

constexpr double kSampleNormalizedValue = 0.65;
constexpr short kInvalidSeverity = 3;
constexpr short kDisconnectedSeverity = kInvalidSeverity + 1;
/* Xc Control's default XcNshadeDepth. */
constexpr int kXcShadeDepth = 2;
/* INDICATOR_OKAY_SIZE in medmWidget.h: larger indicators get 11 scale
 * segments instead of 5. */
constexpr int kIndicatorOkaySize = 80;

/* Rect3d from xc/Control.c: shadow lines inside the rectangle, light at
 * the top and left when raised and swapped when depressed, then the
 * interior filled with the control background. */
void drawXcRect3d(QPainter &painter, int x, int y, int width, int height,
    bool raised, const QColor &shade1, const QColor &shade2,
    const QColor &background)
{
  if (height > 0) {
    --height;
  }
  if (width > 0) {
    --width;
  }
  for (int j = 0; j < kXcShadeDepth; ++j) {
    painter.setPen(raised ? shade1 : shade2);
    painter.drawLine(x + j, y + j, x + j, y + height - j);
    painter.drawLine(x + j, y + j, x + width - j, y + j);
    painter.setPen(raised ? shade2 : shade1);
    painter.drawLine(x + j, y + height - j, x + width - j, y + height - j);
    painter.drawLine(x + width - j, y + j, x + width - j, y + height - j);
  }
  if (width > 2 * kXcShadeDepth && height > 2 * kXcShadeDepth) {
    painter.fillRect(x + kXcShadeDepth, y + kXcShadeDepth,
        width - 2 * kXcShadeDepth, height - 2 * kXcShadeDepth, background);
  }
}

} // namespace


ScaleMonitorElement::ScaleMonitorElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  limits_.lowSource = PvLimitSource::kDefault;
  limits_.highSource = PvLimitSource::kDefault;
  limits_.precisionSource = PvLimitSource::kDefault;
  limits_.lowDefault = 0.0;
  limits_.highDefault = 1.0;  /* MEDM's HOPR_DEFAULT */
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
  switch (direction) {
  case BarDirection::kDown:
    direction = BarDirection::kUp;
    break;
  case BarDirection::kLeft:
    direction = BarDirection::kRight;
    break;
  case BarDirection::kUp:
  case BarDirection::kRight:
    break;
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

double ScaleMonitorElement::displayLowLimit() const
{
  return effectiveLowLimit();
}

double ScaleMonitorElement::displayHighLimit() const
{
  return effectiveHighLimit();
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

bool ScaleMonitorElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void ScaleMonitorElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool ScaleMonitorElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void ScaleMonitorElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool ScaleMonitorElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void ScaleMonitorElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool ScaleMonitorElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void ScaleMonitorElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool ScaleMonitorElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void ScaleMonitorElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString ScaleMonitorElement::channel() const
{
  return channel_;
}

void ScaleMonitorElement::setChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  update();
}

void ScaleMonitorElement::setExecuteMode(bool execute)
{
  const MedmWidgetUnmanagedUpdate unmanaged(*this,
      [this]() { return executeMode_ && !runtimeConnected_; });
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  applyMedmMinimumSize();
  clearRuntimeState();
}

bool ScaleMonitorElement::isExecuteMode() const
{
  return executeMode_;
}

void ScaleMonitorElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  medmMinimumSize_.noteResize(*this);
  applyMedmMinimumSize();
}

/* MEDM's Xc Indicator is at least 20x20 whatever its label. While the
 * channel is disconnected MEDM unmanages it and paints only the object
 * white, so the indicator keeps the object size then. */
void ScaleMonitorElement::applyMedmMinimumSize()
{
  medmMinimumSize_.apply(*this, executeMode_ && runtimeConnected_);
}

void ScaleMonitorElement::setRuntimeConnected(bool connected)
{
  const MedmWidgetUnmanagedUpdate unmanaged(*this,
      [this]() { return executeMode_ && !runtimeConnected_; });
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
  applyMedmMinimumSize();
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
  /* Keep the channel's value; sampleValue() limits it when drawn. Clamping
   * here kept a value that arrived before the limits at the default range. */
  const bool firstValue = !hasRuntimeValue_;
  const bool changed = firstValue
      || std::abs(value - runtimeValue_) > valueEpsilon();
  runtimeValue_ = value;
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
  const MedmWidgetUnmanagedUpdate unmanaged(*this,
      [this]() { return executeMode_ && !runtimeConnected_; });
  runtimeConnected_ = false;
  runtimeLimitsValid_ = false;
  hasRuntimeValue_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimePrecision_ = -1;
  runtimeValue_ = defaultSampleValue();
  runtimeSeverity_ = kInvalidSeverity;
  applyMedmMinimumSize();
  if (executeMode_) {
    update();
  } else {
    update();
  }
}

void ScaleMonitorElement::paintEvent(QPaintEvent *event)
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

  paintXcIndicator(painter, QFontMetrics(labelFont));

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

/* Draw the Xc Indicator widget that MEDM uses for a scale monitor, following
 * the Resize, Redisplay and Draw_display methods in xc/Indicator.c.  MEDM
 * draws direction "down" as "up" and "left" as "right". */
void ScaleMonitorElement::paintXcIndicator(QPainter &painter,
    const QFontMetrics &metrics) const
{
  const int width = this->width();
  const int height = this->height();
  const int d = kXcShadeDepth;
  const bool vertical = isVertical();
  const bool decorated = label_ != MeterLabel::kNone
      && label_ != MeterLabel::kNoDecorations;
  const int segments = !decorated ? 0
      : ((vertical ? width : height) > kIndicatorOkaySize ? 11 : 5);
  const bool valueVisible = label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel;
  const QString label = label_ == MeterLabel::kChannel
      ? channel_.trimmed() : QString();
  const int ascent = metrics.ascent();
  const int descent = metrics.descent();
  /* For numbers Xc ignores the descent to save space. */
  const int fontHeight = ascent;

  const QRect face(d, d, width - 2 * d, height - 2 * d);
  const QString upperText = formatValue(effectiveHighLimit());
  const QString lowerText = formatValue(effectiveLowLimit());
  const int maxValWidth = metrics.horizontalAdvance(upperText);
  const int minValWidth = metrics.horizontalAdvance(lowerText);
  int maxWidth = std::max(minValWidth, maxValWidth) + 2 * d;
  if (segments == 0) {
    maxWidth = face.width() - 2 * d;
  }

  QRect valueBox(0, height - 2 * d, 0, 0);
  if (valueVisible) {
    valueBox = QRect(width / 2 - maxWidth / 2,
        height - fontHeight - (vertical ? 3 * d : 2 * d + 2),
        maxWidth, fontHeight);
  }

  QPoint labelPos(face.x(), face.y());
  if (!label.isEmpty()) {
    labelPos = QPoint(width / 2 - metrics.horizontalAdvance(label) / 2,
        face.y() + ascent + 1);
  }

  int barX = 0;
  int barY = 0;
  int barWidth = 0;
  int barHeight = 0;
  if (vertical) {
    barX = face.x()
        + (segments > 0 ? maxWidth + face.width() / 32 : 0) + 2 * d;
    barY = labelPos.y() + descent + 2 * d;
    barWidth = std::max(0, face.width() - barX - d);
    barHeight = std::max(0, valueBox.y() - barY
        - (valueVisible ? fontHeight - descent : 2 * d));
  } else {
    barX = face.x() + face.width() / 16 + d;
    barY = labelPos.y()
        + (segments > 0 ? fontHeight / 2 + face.height() / 8 + 1 : 0)
        + 2 * d;
    barWidth = std::max(0, (9 * face.width()) / 10 - 2 * d);
    barHeight = std::max(1, valueBox.y() - barY
        - (valueVisible ? fontHeight : 0) - d);
  }

  int x1 = 0;
  int y1 = 0;
  int x2 = 0;
  int y2 = 0;
  if (vertical) {
    x1 = barX - d - face.width() / 32;
    y1 = barY;
    x2 = x1;
    y2 = barY + barHeight;
  } else {
    x1 = barX;
    y1 = barY - d - face.height() / 32;
    x2 = barX + barWidth;
    y2 = y1;
  }
  const int segLength = vertical ? face.width() / 16 : face.height() / 16;

  QColor shade1;
  QColor shade2;
  const QColor bg = effectiveBackground();
  MedmColors::computeShadowColors(bg, shade1, shade2);

  drawXcRect3d(painter, 0, 0, width, height, true, shade1, shade2, bg);

  /* Xc draws the label, scale and bar border clipped to the face. */
  painter.save();
  painter.setClipRect(face);
  painter.setPen(Qt::black);
  if (!label.isEmpty()) {
    painter.drawText(labelPos, label);
  }
  if (segments > 0) {
    painter.drawLine(x1, y1, x2, y2);
    if (!vertical) {
      painter.drawLine(x1, y1 - segLength, x1, y1);
      painter.drawLine(x2, y2 - segLength, x2, y2);
    }
    const int spacing = (vertical ? barHeight : barWidth) / (segments + 1);
    for (int j = 0; j < segments; ++j) {
      if (vertical) {
        const int y = y1 + (j + 1) * spacing;
        painter.drawLine(x1 - segLength, y, x1, y);
      } else {
        const int x = x1 + (j + 1) * spacing;
        painter.drawLine(x, y1 - segLength, x, y1);
      }
    }

    QPoint maxPos;
    QPoint minPos;
    if (vertical) {
      const int fontCenter = (ascent + descent) / 2 - descent;
      maxPos = QPoint(std::max(d, x2 - maxValWidth), y1 + fontCenter);
      minPos = QPoint(std::max(d, x2 - minValWidth), y2 + fontCenter);
    } else {
      const int textY = y1 - segLength - 1;
      maxPos = QPoint(std::min(face.width() + d - maxValWidth,
                          x2 - maxValWidth / 2),
          textY);
      minPos = QPoint(std::max(d, x1 - minValWidth / 2), textY);
    }
    painter.drawText(maxPos, upperText);
    painter.drawText(minPos, lowerText);
  }
  drawXcRect3d(painter, barX - d, barY - d, barWidth + 2 * d,
      barHeight + 2 * d, false, shade1, shade2, bg);
  painter.restore();

  /* Draw_display: the bar background, then the diamond at the value. */
  const QRect bar(barX, barY, barWidth, barHeight);
  painter.fillRect(bar, bg);
  const double low = effectiveLowLimit();
  const double span = effectiveHighLimit() - low;
  double dim = 0.0;
  if (span != 0.0 && std::isfinite(span)) {
    const double percent = std::clamp(
        (sampleValue() - low) * 100.0 / span, 0.0, 100.0);
    dim = (vertical ? barHeight : barWidth) * (percent / 100.0);
  }
  const int position = std::max(1, static_cast<int>(dim));
  const int indicatorSize = std::max(5, (vertical ? barHeight : barWidth) / 10);
  QPoint points[4];
  if (vertical) {
    points[0] = QPoint(barX, barY + barHeight - position);
    points[1] = QPoint(points[0].x() + barWidth / 2,
        points[0].y() - indicatorSize / 2);
  } else {
    points[0] = QPoint(barX + position - indicatorSize / 2 - 1,
        barY + barHeight / 2);
    points[1] = QPoint(points[0].x() + indicatorSize / 2,
        points[0].y() - barHeight / 2);
  }
  points[2] = QPoint(points[1].x() + (points[1].x() - points[0].x()),
      points[0].y());
  points[3] = QPoint(points[1].x(),
      points[0].y() + (points[0].y() - points[1].y()));
  painter.save();
  painter.setClipRect(bar);
  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveForeground());
  painter.drawPolygon(points, 4);
  painter.restore();

  if (valueVisible) {
    painter.fillRect(valueBox, Qt::white);
    const QString text = formattedSampleValue();
    painter.save();
    painter.setClipRect(valueBox);
    painter.setPen(Qt::black);
    painter.drawText(QPoint(valueBox.x() + valueBox.width() / 2
            - metrics.horizontalAdvance(text) / 2,
        valueBox.y() + (valueBox.height() - (ascent + descent)) / 2
            + ascent + 1),
        text);
    painter.restore();
  }
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


void ScaleMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
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
