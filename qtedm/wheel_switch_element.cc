#include "wheel_switch_element.h"

#include "update_coordinator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QTimer>
#include <QWheelEvent>

#include "legacy_fonts.h"
#include "medm_colors.h"
#include "pv_name_utils.h"
#include "cursor_utils.h"
#include "window_utils.h"

namespace {

constexpr double kMinimumCenterHeight = 12.0;
constexpr double kMinimumButtonHeight = 7.0;
constexpr int kRepeatInitialDelayMs = 350;
constexpr int kRepeatIntervalMs = 90;
constexpr double kValueEpsilonFactor = 1e-9;
constexpr short kInvalidSeverity = 3;
constexpr int kWheelSwitchDefaultFormatWidth = 6;
constexpr int kWheelSwitchDefaultFormatPrecision = 2;
constexpr char kWheelSwitchDefaultFormat[] = "% 6.2f";

struct WheelSwitchFormatInfo
{
  QString formatString;
  QString zeroString;
  int prefixSize = 0;
  int postfixSize = 0;
  int digitSize = 0;
  int pointPosition = 0;
  int precision = 0;
  bool pointValid = false;
};

const std::array<QString, 16> kWheelSwitchFontAliases = {
  QStringLiteral("widgetDM_4"), QStringLiteral("widgetDM_6"),
  QStringLiteral("widgetDM_8"), QStringLiteral("widgetDM_10"),
  QStringLiteral("widgetDM_12"), QStringLiteral("widgetDM_14"),
  QStringLiteral("widgetDM_16"), QStringLiteral("widgetDM_18"),
  QStringLiteral("widgetDM_20"), QStringLiteral("widgetDM_22"),
  QStringLiteral("widgetDM_24"), QStringLiteral("widgetDM_30"),
  QStringLiteral("widgetDM_36"), QStringLiteral("widgetDM_40"),
  QStringLiteral("widgetDM_48"), QStringLiteral("widgetDM_60"),
};

QFont wheelSwitchFontForHeight(int widgetHeight)
{
  if (widgetHeight <= 0) {
    return QFont();
  }
  const double effHeight = std::max(0.0, static_cast<double>(widgetHeight) - 4.0);

  QFont fallback;
  for (const QString &alias : kWheelSwitchFontAliases) {
    const QFont candidate = LegacyFonts::font(alias);

    if (candidate.family().isEmpty()) {

      continue;
    }
    fallback = candidate;
    break;
  }

  for (auto it = kWheelSwitchFontAliases.rbegin();


       it != kWheelSwitchFontAliases.rend(); ++it) {
    const QFont font = LegacyFonts::font(*it);
    if (font.family().isEmpty()) {
      continue;
    }

    const QFontMetricsF metrics(font);
    const double totalFontHeight = metrics.ascent() + 2.0 * metrics.descent();


    const double buttonHeight = metrics.horizontalAdvance(QStringLiteral("0"));
    const double testHeight = std::max(0.0, effHeight - 2.0 * buttonHeight);

    if (totalFontHeight <= testHeight) {
      return font;
    }
  }

  return fallback;
}

QColor blendedColor(const QColor &base, int factor)
{

  if (!base.isValid()) {

    return QColor();
  }

  QColor adjusted = base;
  adjusted = factor > 100 ? adjusted.lighter(factor) : adjusted.darker(200 - factor);

  return adjusted;
}

QString calculatedWheelSwitchFormat(double low, double high, int precision)
{
  if (precision < 0) {
    precision = 0;
  }

  const double maxAbsLimit = std::max(std::abs(low), std::abs(high));
  int width = 2 + precision;
  if (maxAbsLimit > 1.0 && std::isfinite(maxAbsLimit)) {
    width = static_cast<int>(std::log10(maxAbsLimit)) + 3 + precision;
  }

  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%% %d.%df", width, precision);
  return QString::fromLatin1(buffer);
}

QString sanitizeExplicitWheelSwitchFormat(const QString &format)
{
  const QString trimmed = format.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }

  const int percentPos = trimmed.indexOf(QLatin1Char('%'));
  int fPos = -1;
  if (percentPos >= 0) {
    for (int i = percentPos + 1; i < trimmed.size(); ++i) {
      if (trimmed.at(i) == QLatin1Char('f')
          || trimmed.at(i) == QLatin1Char('F')) {
        fPos = i;
        break;
      }
    }
  }
  if (percentPos < 0 || fPos < 0) {
    return QString::fromLatin1(kWheelSwitchDefaultFormat);
  }

  const QString prefix = trimmed.left(percentPos);
  const QString spec = trimmed.mid(percentPos + 1, fPos - percentPos - 1);
  const QString suffix = trimmed.mid(fPos + 1);

  int pos = 0;
  QChar signFlag = QLatin1Char(' ');
  while (pos < spec.size()) {
    const QChar ch = spec.at(pos);
    if (ch == QLatin1Char('+')) {
      signFlag = ch;
      ++pos;
      continue;
    }
    if (ch == QLatin1Char(' ') || ch == QLatin1Char('#')
        || ch == QLatin1Char('0') || ch == QLatin1Char('-')) {
      ++pos;
      continue;
    }
    break;
  }

  int width = kWheelSwitchDefaultFormatWidth;
  int precision = kWheelSwitchDefaultFormatPrecision;
  bool parsed = false;
  const QString numeric = spec.mid(pos);
  const int dotPos = numeric.indexOf(QLatin1Char('.'));
  if (dotPos < 0) {
    bool ok = false;
    const int parsedWidth = numeric.toInt(&ok);
    if (ok) {
      width = parsedWidth;
      precision = 0;
      parsed = true;
    }
  } else {
    bool widthOk = false;
    bool precisionOk = false;
    const int parsedWidth = numeric.left(dotPos).toInt(&widthOk);
    const int parsedPrecision = numeric.mid(dotPos + 1).toInt(&precisionOk);
    if (widthOk && precisionOk) {
      width = parsedWidth;
      precision = parsedPrecision;
      parsed = true;
    }
  }

  if (!parsed || width < 0) {
    width = kWheelSwitchDefaultFormatWidth;
    precision = kWheelSwitchDefaultFormatPrecision;
  } else {
    if (precision < 0) {
      precision = kWheelSwitchDefaultFormatPrecision;
    }
    if (precision > width - 1) {
      precision = width - 1;
    }
    if (precision < 0) {
      precision = 0;
    }
  }

  return prefix + QLatin1Char('%') + signFlag
      + QString::number(width)
      + QLatin1Char('.')
      + QString::number(precision)
      + QLatin1Char('f')
      + suffix;
}

WheelSwitchFormatInfo wheelSwitchFormatInfo(const QString &format,
    double lowLimit, double highLimit, int precision)
{
  WheelSwitchFormatInfo info;
  if (format.trimmed().isEmpty()) {
    info.formatString = calculatedWheelSwitchFormat(lowLimit, highLimit, precision);
  } else {
    info.formatString = sanitizeExplicitWheelSwitchFormat(format);
  }

  const QByteArray formatBytes = info.formatString.toLatin1();
  char buffer[256];
  std::snprintf(buffer, sizeof(buffer), formatBytes.constData(), 0.0);
  info.zeroString = QString::fromLatin1(buffer);

  info.prefixSize = info.formatString.indexOf(QLatin1Char('%'));
  if (info.prefixSize < 0) {
    info.prefixSize = 0;
  }

  const int fPos = info.formatString.indexOf(QLatin1Char('f'), info.prefixSize);
  if (fPos >= 0) {
    info.postfixSize = info.formatString.size() - fPos - 1;
  }

  info.digitSize = std::max(0,
      info.zeroString.size() - info.prefixSize - info.postfixSize);
  for (int i = 0; i < info.digitSize; ++i) {
    if (info.zeroString.at(info.prefixSize + info.digitSize - 1 - i)
        == QLatin1Char('.')) {
      info.pointPosition = i;
      info.pointValid = true;
      break;
    }
  }
  info.precision = info.pointValid ? info.pointPosition : 0;
  return info;
}

QString formattedWheelSwitchValue(double value, double lowLimit, double highLimit,
    const WheelSwitchFormatInfo &info, bool *overflow = nullptr)
{
  if (overflow) {
    *overflow = false;
  }

  const int digitSlots = info.digitSize - 1 - (info.pointValid ? 1 : 0);
  double minmin = 0.0;
  double maxmax = 0.0;
  double smallestIncrement = 0.0;
  if (digitSlots > 0) {
    double increment = 1.0;
    for (int i = 0; i < info.pointPosition; ++i) {
      increment /= 10.0;
    }
    smallestIncrement = increment;
    for (int i = 0; i < digitSlots; ++i) {
      minmin -= increment * 9.0;
      maxmax += increment * 9.0;
      increment *= 10.0;
    }
  }

  const double formatMin = std::max(lowLimit, minmin);
  const double formatMax = std::min(highLimit, maxmax);
  const double roundoff = smallestIncrement * 0.1;

  const QByteArray formatBytes = info.formatString.toLatin1();
  if (value < formatMax + roundoff && value > formatMin - roundoff) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), formatBytes.constData(), value);
    return QString::fromLatin1(buffer);
  }

  if (overflow) {
    *overflow = true;
  }

  QString result = info.zeroString;
  const int imin = info.prefixSize;
  const int imax = info.prefixSize + info.digitSize;
  int pointIndex = -1;
  if (info.pointValid && info.pointPosition != 0) {
    pointIndex = imax - info.pointPosition - 1;
  }

  if (info.digitSize > 0 && imax <= result.size()) {
    if (value < 0.0) {
      result[imin] = QLatin1Char('-');
    }
    for (int i = imin + 1; i < imax; ++i) {
      result[i] = QLatin1Char('*');
    }
    if (pointIndex >= imin && pointIndex < imax) {
      result[pointIndex] = QLatin1Char('.');
    }
  }

  return result;
}

} // namespace

WheelSwitchElement::WheelSwitchElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  limits_.lowSource = PvLimitSource::kDefault;
  limits_.highSource = PvLimitSource::kDefault;
  limits_.precisionSource = PvLimitSource::kDefault;
  limits_.lowDefault = 0.0;

  limits_.highDefault = 100.0;
  limits_.precisionDefault = 0;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimeValue_ = defaultSampleValue();

  repeatTimer_ = new QTimer(this);
  repeatTimer_->setSingleShot(true);
  QObject::connect(repeatTimer_, &QTimer::timeout, this,
      [this]() { handleRepeatTimeout(); });
}

void WheelSwitchElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;

  }
  selected_ = selected;
  update();
}


bool WheelSwitchElement::isSelected() const
{
  return selected_;
}

QColor WheelSwitchElement::foregroundColor() const
{

  return foregroundColor_;
}

void WheelSwitchElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {

    return;
  }
  foregroundColor_ = color;

  update();
}

QColor WheelSwitchElement::backgroundColor() const
{
  return backgroundColor_;
}


void WheelSwitchElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}


TextColorMode WheelSwitchElement::colorMode() const
{
  return colorMode_;
}

void WheelSwitchElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {

    return;
  }
  colorMode_ = mode;
  update();
}

double WheelSwitchElement::precision() const
{
  return precision_;
}


void WheelSwitchElement::setPrecision(double precision)
{
  if (std::abs(precision_ - precision) < 1e-9) {
    return;
  }
  precision_ = precision;
  update();
}



QString WheelSwitchElement::format() const
{
  return format_;
}

void WheelSwitchElement::setFormat(const QString &format)
{
  QString trimmed = format.trimmed();
  if (!trimmed.isEmpty()) {
    trimmed = sanitizeExplicitWheelSwitchFormat(trimmed);
  }
  if (format_ == trimmed) {

    return;

  }
  format_ = trimmed;
  update();
}


const PvLimits &WheelSwitchElement::limits() const
{
  return limits_;
}

double WheelSwitchElement::displayLowLimit() const
{
  return effectiveLowLimit();
}

double WheelSwitchElement::displayHighLimit() const
{
  return effectiveHighLimit();
}

void WheelSwitchElement::setLimits(const PvLimits &limits)

{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  runtimeLimitsValid_ = false;
  runtimeLow_ = limits_.lowDefault;

  runtimeHigh_ = limits_.highDefault;
  if (!executeMode_) {
    runtimeValue_ = defaultSampleValue();
    hasRuntimeValue_ = false;
  }
  update();
}

bool WheelSwitchElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void WheelSwitchElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool WheelSwitchElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void WheelSwitchElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool WheelSwitchElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void WheelSwitchElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool WheelSwitchElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void WheelSwitchElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool WheelSwitchElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void WheelSwitchElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString WheelSwitchElement::channel() const
{
  return channel_;
}

void WheelSwitchElement::setChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  setToolTip(QString());
  update();
}

void WheelSwitchElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  stopRepeating();
  clearRuntimeState();
  updateCursor();
}

bool WheelSwitchElement::isExecuteMode() const
{
  return executeMode_;
}

void WheelSwitchElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = kInvalidSeverity;
  }
  updateCursor();
  update();
}

void WheelSwitchElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  updateCursor();
}

void WheelSwitchElement::setRuntimeSeverity(short severity)
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

void WheelSwitchElement::setRuntimeLimits(double low, double high)
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

void WheelSwitchElement::setRuntimePrecision(int precision)
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

void WheelSwitchElement::setRuntimeValue(double value)
{
  if (!executeMode_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }
  double clamped = clampToLimits(value);
  bool firstValue = !hasRuntimeValue_;
  bool changed = firstValue || std::abs(clamped - runtimeValue_) > valueEpsilon();
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  if (changed) {
    UpdateCoordinator::instance().requestUpdate(this);
  }
}

void WheelSwitchElement::clearRuntimeState()
{
  stopRepeating();
  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = kInvalidSeverity;
  runtimeLimitsValid_ = false;
  runtimePrecision_ = -1;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  hasRuntimeValue_ = false;
  runtimeValue_ = defaultSampleValue();
  hasLastSentValue_ = false;
  pressedSlotIndex_ = -1;
  pressedDirection_ = RepeatDirection::kNone;
  repeatDirection_ = RepeatDirection::kNone;
  repeatStep_ = 0.0;
  selectedSlotIndex_ = -1;
  keyboardEntryActive_ = false;
  keyboardEntryText_.clear();
  if (repeatTimer_) {
    repeatTimer_->stop();
    repeatTimer_->setSingleShot(true);
  }
  updateCursor();
  update();
}

void WheelSwitchElement::setActivationCallback(const std::function<void(double)> &callback)
{
  activationCallback_ = callback;
  hasLastSentValue_ = false;
  updateCursor();
}

void WheelSwitchElement::mousePressEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif

  if (QRectF(rect()).contains(pos)) {
    updateHoverState(pos);
  } else {
    clearHoverState();
  }

  // Forward middle button and right-click events to parent window for PV info functionality
  if (executeMode_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }

  // Forward left clicks to parent when PV Info picking mode is active
  if (executeMode_ && event->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }

  if (event->button() != Qt::LeftButton || !isInteractive()) {
    QWidget::mousePressEvent(event);
    return;
  }

  setFocus(Qt::MouseFocusReason);

  const QRectF outer = rect().adjusted(qreal(0.5), qreal(0.5), qreal(-0.5), qreal(-0.5));
  const Layout layout = layoutForRect(outer);
  bool handled = false;

  for (int i = 0; i < static_cast<int>(layout.columns.size()); ++i) {
    const Layout::Slot &column = layout.columns.at(i);
    if (!column.hasButtons) {
      continue;
    }
    const double baseStep = column.step;
    if (!std::isfinite(baseStep) || baseStep <= 0.0) {
      continue;
    }
    if (column.showUpButton && column.upButton.contains(pos)) {
      const double step = applyModifiersToStep(baseStep, event->modifiers());
      startRepeating(RepeatDirection::kUp, step, i);
      handled = true;
      break;
    }
    if (column.showDownButton && column.downButton.contains(pos)) {
      const double step = applyModifiersToStep(baseStep, event->modifiers());
      startRepeating(RepeatDirection::kDown, step, i);
      handled = true;
      break;
    }
  }

  if (handled) {
    update();
    event->accept();
  } else {
    QWidget::mousePressEvent(event);
  }
}

void WheelSwitchElement::mouseReleaseEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif

  if (QRectF(rect()).contains(pos)) {
    updateHoverState(pos);
  } else {
    clearHoverState();
  }

  if (event->button() != Qt::LeftButton || !isInteractive()) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  if (pressedSlotIndex_ >= 0 || repeatDirection_ != RepeatDirection::kNone) {
    stopRepeating();
    event->accept();
  } else {
    QWidget::mouseReleaseEvent(event);
  }
}

void WheelSwitchElement::mouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif

  updateHoverState(pos);
  QWidget::mouseMoveEvent(event);
}

void WheelSwitchElement::leaveEvent(QEvent *event)
{
  clearHoverState();
  QWidget::leaveEvent(event);
}

void WheelSwitchElement::focusInEvent(QFocusEvent *event)
{
  if (isInteractive()) {
    const QRectF outer = rect().adjusted(qreal(0.5), qreal(0.5), qreal(-0.5), qreal(-0.5));
    const Layout layout = layoutForRect(outer);
    if (selectedSlotIndex_ < 0 || selectedSlotIndex_ >= static_cast<int>(layout.columns.size())
        || !layout.columns.at(selectedSlotIndex_).hasButtons) {
      selectedSlotIndex_ = defaultSlotIndex(layout);
    }
  }
  update();
  QWidget::focusInEvent(event);
}

void WheelSwitchElement::focusOutEvent(QFocusEvent *event)
{
  stopRepeating();
  cancelKeyboardEntry();
  update();
  QWidget::focusOutEvent(event);
}

void WheelSwitchElement::wheelEvent(QWheelEvent *event)
{
  if (!isInteractive() || keyboardEntryActive_) {
    QWidget::wheelEvent(event);
    return;
  }

  const int delta = event->angleDelta().y();
  if (delta == 0) {
    QWidget::wheelEvent(event);
    return;
  }

  const QRectF outer = rect().adjusted(qreal(0.5), qreal(0.5), qreal(-0.5), qreal(-0.5));
  const Layout layout = layoutForRect(outer);
  int slotIndex = -1;
  double step = 0.0;
  if (!selectedSlotStep(layout, event->modifiers(), &slotIndex, &step)) {
    QWidget::wheelEvent(event);
    return;
  }

  selectedSlotIndex_ = slotIndex;
  if (delta > 0) {
    if (layout.columns.at(slotIndex).showUpButton) {
      activateValue(displayedValue() + step, true);
    }
  } else {
    if (layout.columns.at(slotIndex).showDownButton) {
      activateValue(displayedValue() - step, true);
    }
  }

  update();
  event->accept();
}

void WheelSwitchElement::keyPressEvent(QKeyEvent *event)
{
  if (!isInteractive()) {
    QWidget::keyPressEvent(event);
    return;
  }

  if (event->isAutoRepeat()) {
    event->accept();
    return;
  }

  if (keyboardEntryActive_) {
    if (handleKeyboardEntryKey(event)) {
      return;
    }
    QWidget::keyPressEvent(event);
    return;
  }

  const QRectF outer = rect().adjusted(qreal(0.5), qreal(0.5), qreal(-0.5), qreal(-0.5));
  const Layout layout = layoutForRect(outer);

  const QString text = event->text();
  if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier
          | Qt::MetaModifier))
      && !text.isEmpty()) {
    const QChar ch = text.at(0);
    if (ch.isDigit() || ch == QLatin1Char('.') || ch == QLatin1Char('-')
        || ch == QLatin1Char('+')) {
      beginKeyboardEntry();
      if (handleKeyboardEntryKey(event)) {
        return;
      }
    }
  }

  switch (event->key()) {
  case Qt::Key_Left: {
    if (moveSelectedSlot(layout, -1) >= 0) {
      event->accept();
      return;
    }
    break;
  }
  case Qt::Key_Right: {
    if (moveSelectedSlot(layout, 1) >= 0) {
      event->accept();
      return;
    }
    break;
  }
  case Qt::Key_Up: {
    int slotIndex = -1;
    double step = 0.0;
    if (selectedSlotStep(layout, event->modifiers(), &slotIndex, &step)
        && layout.columns.at(slotIndex).showUpButton) {
      startRepeating(RepeatDirection::kUp, step, slotIndex);
      update();
      event->accept();
      return;
    }
    break;
  }
  case Qt::Key_Down: {
    int slotIndex = -1;
    double step = 0.0;
    if (selectedSlotStep(layout, event->modifiers(), &slotIndex, &step)
        && layout.columns.at(slotIndex).showDownButton) {
      startRepeating(RepeatDirection::kDown, step, slotIndex);
      update();
      event->accept();
      return;
    }
    break;
  }
  case Qt::Key_PageUp:
    stopRepeating();
    activateValue(displayedValue() + valueStep(event->modifiers()) * 10.0, true);
    event->accept();
    return;
  case Qt::Key_PageDown:
    stopRepeating();
    activateValue(displayedValue() - valueStep(event->modifiers()) * 10.0, true);
    event->accept();
    return;
  case Qt::Key_Home:
    stopRepeating();
    activateValue(effectiveLowLimit(), true);
    event->accept();
    return;
  case Qt::Key_End:
    stopRepeating();
    activateValue(effectiveHighLimit(), true);
    event->accept();
    return;
  default:
    break;
  }

  QWidget::keyPressEvent(event);
}

void WheelSwitchElement::keyReleaseEvent(QKeyEvent *event)
{
  if (!isInteractive()) {
    QWidget::keyReleaseEvent(event);
    return;
  }

  if (event->isAutoRepeat()) {
    event->accept();
    return;
  }

  switch (event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Down:
    stopRepeating();
    event->accept();
    return;
  default:
    break;
  }

  QWidget::keyReleaseEvent(event);
}

void WheelSwitchElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF outer = rect().adjusted(qreal(0.5), qreal(0.5), qreal(-0.5), qreal(-0.5));
  painter.fillRect(outer, effectiveBackground());

  QPen borderPen(Qt::black);
  borderPen.setWidthF(1.0);
  painter.setPen(borderPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(outer);

  const Layout layout = layoutForRect(outer);
  const bool enabled = isInteractive();

  if (layout.valueRect.height() > 6.0 && layout.valueRect.width() > 6.0) {
    paintValueDisplay(painter, layout);
  }

  for (int i = 0; i < static_cast<int>(layout.columns.size()); ++i) {
    const Layout::Slot &column = layout.columns.at(i);
    if (!column.hasButtons) {
      continue;
    }
    const bool upPressed = (pressedSlotIndex_ == i
        && pressedDirection_ == RepeatDirection::kUp);
    const bool downPressed = (pressedSlotIndex_ == i
        && pressedDirection_ == RepeatDirection::kDown);
    const bool slotSelected = hasFocus() && !keyboardEntryActive_
        && selectedSlotIndex_ == i;

  const bool upHovered = (hoveredSlotIndex_ == i
    && hoveredDirection_ == RepeatDirection::kUp);
  const bool downHovered = (hoveredSlotIndex_ == i
    && hoveredDirection_ == RepeatDirection::kDown);

  if (column.showUpButton) {
    paintButton(painter, column.upButton, true, upPressed, enabled,
      upHovered || slotSelected);
  }
  if (column.showDownButton) {
    paintButton(painter, column.downButton, false, downPressed, enabled,
      downHovered || slotSelected);
  }
  }

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor WheelSwitchElement::effectiveForeground() const
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

QColor WheelSwitchElement::effectiveBackground() const
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

QColor WheelSwitchElement::valueForeground() const
{
  if (executeMode_) {
    if (colorMode_ == TextColorMode::kAlarm) {
      if (!runtimeConnected_) {
        return MedmColors::alarmColorForSeverity(kInvalidSeverity);
      }
      return MedmColors::alarmColorForSeverity(runtimeSeverity_);
    }
  }
  return effectiveForeground();
}

WheelSwitchElement::Layout WheelSwitchElement::layoutForRect(const QRectF &bounds) const
{
  Layout layout{};
  layout.outer = bounds;
  const WheelSwitchFormatInfo formatInfo = wheelSwitchFormatInfo(format_,
      effectiveLowLimit(), effectiveHighLimit(), effectivePrecision());
  const QString templateText = formatInfo.zeroString;
  layout.text = displayText();

  // Find decimal positions in both template and current text
  // The decimal is within the digit area (excluding prefix and postfix)
  int templateDecimalIndex = -1;
  const int prefixSize = formatInfo.prefixSize;
  const int digitSize = formatInfo.digitSize;
  for (int i = prefixSize; i < prefixSize + digitSize; ++i) {
    if (i < templateText.size() && templateText.at(i) == '.') {
      templateDecimalIndex = i;
      break;
    }
  }
  if (templateDecimalIndex < 0) {
    templateDecimalIndex = prefixSize + digitSize;
  }
  
  int currentDecimalIndex = layout.text.indexOf('.');
  if (currentDecimalIndex < 0) {
    currentDecimalIndex = layout.text.size();
  }

  // Count digits after decimal in template to determine how many go left vs right
  // (currently not used but may be needed for future enhancements)
  // int digitsAfterDecimal = 0;
  // for (int i = templateDecimalIndex + 1; i < templateText.size(); ++i) {
  //   if (templateText.at(i).isDigit()) {
  //     digitsAfterDecimal++;
  //   }
  // }

  layout.font = wheelSwitchFontForHeight(height());
  if (layout.font.family().isEmpty()) {
    layout.font = font();
  }

  const double totalHeight = std::max(0.0, bounds.height());
  double buttonHeight = std::max(kMinimumButtonHeight, totalHeight * 0.22);
  const double maxButtonHeight = std::max(kMinimumButtonHeight,
      (totalHeight - kMinimumCenterHeight) / 2.0);
  if (buttonHeight > maxButtonHeight) {
    buttonHeight = maxButtonHeight;
  }
  if (totalHeight - 2.0 * buttonHeight < kMinimumCenterHeight) {
    buttonHeight = std::max(kMinimumButtonHeight,
        (totalHeight - kMinimumCenterHeight) / 2.0);
  }
  buttonHeight = std::clamp(buttonHeight, kMinimumButtonHeight, totalHeight / 2.0);
  layout.buttonHeight = buttonHeight;

  const double centralHeight = std::max(0.0, totalHeight - 2.0 * buttonHeight);
  layout.valueRect = QRectF(bounds.left() + 4.0,
      bounds.top() + buttonHeight,
      std::max(0.0, bounds.width() - 8.0), centralHeight);

  layout.columns.clear();
  layout.columns.reserve(layout.text.size());

  if (layout.text.isEmpty()) {
    return layout;
  }

  const QFontMetricsF metrics(layout.font);
  const double zeroWidth = std::max(4.0, metrics.horizontalAdvance(QStringLiteral("0")));
  const double minimalWidth = std::max(4.0, zeroWidth * 0.6);

  std::vector<double> charWidths;
  charWidths.reserve(layout.text.size());
  double totalWidth = 0.0;
  for (QChar ch : layout.text) {
    double width = metrics.horizontalAdvance(ch);
    if (!std::isfinite(width) || width < minimalWidth) {
      if (ch == QLatin1Char('.') || ch == QLatin1Char('-')) {
        width = std::max(minimalWidth * 0.8, 4.0);
      } else {
        width = minimalWidth;
      }
    }
    charWidths.push_back(width);
    totalWidth += width;
  }

  // Calculate total width needed including button extensions
  // Buttons extend beyond character width by 1.5x zero width (uniformButtonWidth formula below)
  const double uniformButtonWidth = zeroWidth * 1.5;
  
  // Mark which positions in templateText should have buttons
  // Following MEDM's logic: exclude sign (first in digit area), decimal point, and postfix characters
  // (prefixSize, formatSize, postfixSize, and digitSize were calculated earlier)
  
  std::vector<bool> templateIsDigit(templateText.size(), false);
  for (int i = 0; i < templateText.size(); ++i) {
    // Skip prefix characters
    if (i < prefixSize) {
      continue;
    }
    // Skip postfix characters
    if (i >= prefixSize + digitSize) {
      continue;
    }
    // Skip sign (first character in digit area)
    if (i == prefixSize) {
      continue;
    }
    // Skip decimal point
    if (i == templateDecimalIndex) {
      continue;
    }
    // This position gets buttons
    templateIsDigit[i] = true;
  }
  
  // Calculate how much extra width buttons add beyond text
  double maxButtonExtension = 0.0;
  for (int i = 0; i < layout.text.size(); ++i) {
    int templatePos = -1;
    if (i < currentDecimalIndex) {
      int offsetFromDecimal = currentDecimalIndex - i;
      templatePos = templateDecimalIndex - offsetFromDecimal;
    } else if (i > currentDecimalIndex) {
      int offsetFromDecimal = i - currentDecimalIndex;
      templatePos = templateDecimalIndex + offsetFromDecimal;
    }
    
    if (!keyboardEntryActive_ && templatePos >= 0
        && templatePos < templateText.size() && templateIsDigit[templatePos]) {
      // This character will have buttons
      const double charWidth = (static_cast<size_t>(i) < charWidths.size()) ? charWidths[i] : minimalWidth;
      // Button extends beyond character on both sides
      const double buttonExtension = std::max(0.0, (uniformButtonWidth - charWidth) / 2.0);
      maxButtonExtension = std::max(maxButtonExtension, buttonExtension);
    }
  }
  
  // Total content width is text width plus button extensions on both sides
  const double totalContentWidth = totalWidth + 2.0 * maxButtonExtension;
  
  // Center the entire content (text + buttons) within the widget
  const double widgetCenterX = bounds.center().x();
  const double minContentLeft = bounds.left() + 2.0;
  const double maxContentRight = bounds.right() - 2.0;
  const double availableWidth = maxContentRight - minContentLeft;
  
  double contentLeft;
  double actualButtonExtension = maxButtonExtension;
  
  // Check if just the TEXT alone (without any button extensions) fits
  if (totalWidth > availableWidth) {
    // Text alone doesn't fit - no room for button extensions
    actualButtonExtension = 0.0;
    
    // Count leading spaces that we can clip without visual impact
    int leadingSpaces = 0;
    double leadingSpaceWidth = 0.0;
    for (int i = 0; i < layout.text.size(); ++i) {
      if (layout.text.at(i) == QLatin1Char(' ')) {
        leadingSpaces++;
        if (static_cast<size_t>(i) < charWidths.size()) {
          leadingSpaceWidth += charWidths[i];
        }
      } else {
        break;
      }
    }
    
    if (leadingSpaces > 0 && leadingSpaceWidth > 0.0) {
      // We have leading spaces - shift left to clip them instead of visible content
      double overflow = totalWidth - availableWidth;
      double shiftLeft = std::min(leadingSpaceWidth, overflow);
      
      // Position to clip the leading spaces
      contentLeft = minContentLeft - shiftLeft;
    } else {
      // No leading spaces - center the text as best we can
      contentLeft = widgetCenterX - totalWidth / 2.0;
      
      // Clamp so we don't go too far left
      if (contentLeft < minContentLeft) {
        contentLeft = minContentLeft;
      }
    }
  } else if (totalContentWidth > availableWidth) {
    // Text fits, but buttons extend beyond - reduce button extensions
    
    // Calculate how much we need to shrink button extensions
    // Available width = totalWidth + 2 * newButtonExtension
    // newButtonExtension = (availableWidth - totalWidth) / 2
    actualButtonExtension = std::max(0.0, (availableWidth - totalWidth) / 2.0);
    
    // Position at left edge
    contentLeft = minContentLeft;
  } else {
    // Everything fits - center the content
    contentLeft = widgetCenterX - totalContentWidth / 2.0;
    
    // Clamp to widget bounds
    if (contentLeft < minContentLeft) {
      contentLeft = minContentLeft;
    }
    if (contentLeft + totalContentWidth > maxContentRight) {
      contentLeft = std::max(minContentLeft, maxContentRight - totalContentWidth);
    }
  }
  
  // Position text within content area, accounting for actual button extension
  double textLeft = contentLeft + actualButtonExtension;
  
  layout.valueRect.setLeft(textLeft);
  layout.valueRect.setRight(textLeft + totalWidth);

  double startX = textLeft;
  
  // Store actualButtonExtension for use in button placement
  const double constrainedButtonExtension = actualButtonExtension;

  // Track which character positions in the TEMPLATE are digit positions.
  // Following MEDM's logic: all positions except position 0 (sign) and decimal point
  // (Already calculated above, reuse templateIsDigit)

  // Create slots for each character in the CURRENT text
  for (int i = 0; i < layout.text.size(); ++i) {
    const QChar ch = layout.text.at(i);
    const double width = charWidths.at(i);
    Layout::Slot slot;
    slot.character = ch;
    slot.charRect = QRectF(startX, layout.valueRect.top(), width,
        layout.valueRect.height());

    // Determine if this position in current text corresponds to a digit position in template.
    // We align both strings by their decimal points.
    bool canHaveButtons = false;
    int exponent = 0;
    
    int templatePos = -1;
    if (i < currentDecimalIndex) {
      // Before decimal in current text
      int offsetFromDecimal = currentDecimalIndex - i;
      templatePos = templateDecimalIndex - offsetFromDecimal;
    } else if (i == currentDecimalIndex) {
      // This is the decimal point
      templatePos = templateDecimalIndex;
    } else {
      // After decimal in current text
      int offsetFromDecimal = i - currentDecimalIndex;
      templatePos = templateDecimalIndex + offsetFromDecimal;
    }
    
    // Check if this template position exists and is a digit
    if (templatePos >= 0 && templatePos < templateText.size() && templateIsDigit[templatePos]) {
      // This position should have buttons
      canHaveButtons = true;
      
      // Calculate exponent based on distance from decimal in template
      if (templatePos < templateDecimalIndex) {
        // Before decimal - count digit positions to the right
        int digitsToRight = 0;
        for (int j = templatePos + 1; j < templateDecimalIndex; ++j) {
          if (templateIsDigit[j]) {
            digitsToRight++;
          }
        }
        exponent = digitsToRight;
      } else if (templatePos > templateDecimalIndex) {
        // After decimal - count digit position from decimal
        int digitsFromDecimal = 0;
        for (int j = templateDecimalIndex + 1; j <= templatePos; ++j) {
          if (templateIsDigit[j]) {
            digitsFromDecimal++;
          }
        }
        exponent = -digitsFromDecimal;
      } else {
        // This shouldn't happen (decimal point itself)
        canHaveButtons = false;
      }
    }

    if (canHaveButtons) {
      slot.hasButtons = true;
      slot.exponent = exponent;
      slot.step = std::pow(10.0, exponent);

      // Calculate button width accounting for the constrained extension
      // Button can extend up to constrainedButtonExtension on each side of the character
      const double maxAllowedButtonWidth = width + 2.0 * constrainedButtonExtension;
      
      // Start with the ideal uniform button width
      const double idealUniformButtonWidth = zeroWidth * 1.5;
      const double inset = std::min(3.0, idealUniformButtonWidth * 0.2);
      const double idealButtonWidth = std::max(4.0, idealUniformButtonWidth - 2.0 * inset);
      
      // Constrain to what we can actually fit
      const double buttonWidth = std::min(idealButtonWidth, maxAllowedButtonWidth);
      
      const double buttonX = startX + (width - buttonWidth) / 2.0;
      const double buttonHeightAdjusted = std::max(0.0, buttonHeight - 2.0);
      slot.upButton = QRectF(buttonX, bounds.top() + 1.0,
          buttonWidth, buttonHeightAdjusted);
      slot.downButton = QRectF(buttonX,
          bounds.bottom() - buttonHeight + 1.0, buttonWidth, buttonHeightAdjusted);
    }

    layout.columns.push_back(slot);
    startX += width;
  }

  // Apply button visibility logic similar to medm's set_button_visibility
  // Hide buttons that would cause values to exceed limits
  const double value = displayedValue();
  const double lowLimit = effectiveLowLimit();
  const double highLimit = effectiveHighLimit();
  
  // Calculate roundoff as 0.1 times the smallest increment (rightmost digit)
  double roundoff = 0.0;
  for (const auto &slot : layout.columns) {
    if (slot.hasButtons && std::abs(slot.step) > 0.0) {
      if (roundoff == 0.0 || std::abs(slot.step) < roundoff) {
        roundoff = std::abs(slot.step);
      }
    }
  }
  roundoff *= 0.1;

  // Check each digit from left to right (largest to smallest increment).
  // Check each digit independently.

  for (auto &slot : layout.columns) {
    if (!slot.hasButtons) {
      continue;
    }

    // Check if incrementing THIS specific digit would exceed the high limit
    bool hideUp = (value + slot.step > highLimit + roundoff);
    
    // Check if decrementing THIS specific digit would exceed the low limit
    bool hideDown = (value - slot.step < lowLimit - roundoff);

    // Apply visibility flags
    slot.showUpButton = !hideUp;
    slot.showDownButton = !hideDown;
  }

  return layout;
}

void WheelSwitchElement::updateHoverState(const QPointF &pos)
{
  const QRectF outer = rect().adjusted(qreal(0.5), qreal(0.5), qreal(-0.5), qreal(-0.5));
  const Layout layout = layoutForRect(outer);

  int newIndex = -1;
  RepeatDirection newDirection = RepeatDirection::kNone;

  for (int i = 0; i < static_cast<int>(layout.columns.size()); ++i) {
    const Layout::Slot &column = layout.columns.at(i);
    if (!column.hasButtons) {
      continue;
    }
    if (column.showUpButton && column.upButton.contains(pos)) {
      newIndex = i;
      newDirection = RepeatDirection::kUp;
      break;
    }
    if (column.showDownButton && column.downButton.contains(pos)) {
      newIndex = i;
      newDirection = RepeatDirection::kDown;
      break;
    }
  }

  if (newIndex != hoveredSlotIndex_ || newDirection != hoveredDirection_) {
    hoveredSlotIndex_ = newIndex;
    hoveredDirection_ = newDirection;
    update();
  }
}

void WheelSwitchElement::clearHoverState()
{
  if (hoveredSlotIndex_ == -1 && hoveredDirection_ == RepeatDirection::kNone) {
    return;
  }
  hoveredSlotIndex_ = -1;
  hoveredDirection_ = RepeatDirection::kNone;
  update();
}

QColor WheelSwitchElement::buttonFillColor(bool isUp, bool pressed, bool enabled) const
{
  QColor base = effectiveBackground();
  if (!base.isValid()) {
    base = QColor(220, 220, 220);
  }
  if (!enabled) {
    return base;
  }
  if (pressed) {
    return isUp ? blendedColor(base, 108) : blendedColor(base, 92);
  }
  return base;
}

void WheelSwitchElement::paintButton(QPainter &painter, const QRectF &rect,
    bool isUp, bool pressed, bool enabled, bool hovered) const
{
  if (!rect.isValid() || rect.width() < 4.0 || rect.height() < 4.0) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(buttonFillColor(isUp, pressed, enabled));
  painter.drawRoundedRect(rect, 3.0, 3.0);

  if (hovered) {
    painter.setPen(QPen(QColor(0, 0, 0, 100)));
    painter.setBrush(Qt::NoBrush);
    if (rect.width() >= 20) {
      painter.drawRoundedRect(rect.adjusted(2.0, 1.0, -3.0, -1.0), 3.0, 3.0);
    } else {
      painter.drawRoundedRect(rect, 3.0, 3.0);
    }
  }

  const QPointF center = rect.center();
  const qreal halfWidth = rect.width() * 0.22;
  const qreal halfHeight = rect.height() * 0.28;

  QPainterPath arrow;
  if (isUp) {
    arrow.moveTo(center.x(), rect.top() + rect.height() * 0.32);
    arrow.lineTo(center.x() - halfWidth, center.y() + halfHeight);
    arrow.lineTo(center.x() + halfWidth, center.y() + halfHeight);
  } else {
    arrow.moveTo(center.x(), rect.bottom() - rect.height() * 0.32);
    arrow.lineTo(center.x() - halfWidth, center.y() - halfHeight);
    arrow.lineTo(center.x() + halfWidth, center.y() - halfHeight);
  }
  arrow.closeSubpath();

  painter.setPen(Qt::NoPen);
  painter.setBrush(valueForeground());
  painter.drawPath(arrow);
  painter.restore();
}

void WheelSwitchElement::paintValueDisplay(QPainter &painter,
    const Layout &layout) const
{
  if (!layout.valueRect.isValid() || layout.valueRect.width() <= 0.0
      || layout.valueRect.height() <= 0.0) {
    return;
  }

  painter.save();
  painter.setClipRect(layout.valueRect);
  painter.setPen(valueForeground());
  painter.setFont(layout.font);

  for (const Layout::Slot &slot : layout.columns) {
    if (!slot.charRect.isValid() || slot.charRect.width() <= 0.0
        || slot.charRect.height() <= 0.0) {
      continue;
    }
    painter.drawText(slot.charRect, Qt::AlignCenter | Qt::AlignVCenter,
        QString(slot.character));
  }

  painter.restore();
}

void WheelSwitchElement::paintSelectionOverlay(QPainter &painter) const
{
  painter.save();
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
  painter.restore();
}

QString WheelSwitchElement::displayText() const
{
  if (keyboardEntryActive_) {
    return keyboardEntryDisplayText();
  }

  if (executeMode_) {
    if (!runtimeConnected_ || !hasRuntimeValue_) {
      return QString();
    }
  }

  const double value = displayedValue();
  const WheelSwitchFormatInfo info = wheelSwitchFormatInfo(format_,
      effectiveLowLimit(), effectiveHighLimit(), effectivePrecision());
  return formattedWheelSwitchValue(value, effectiveLowLimit(),
      effectiveHighLimit(), info);
}

int WheelSwitchElement::formatDecimals() const
{
  const QString trimmed = format_.trimmed();
  if (trimmed.isEmpty()) {
    return -1;
  }
  const WheelSwitchFormatInfo info = wheelSwitchFormatInfo(trimmed,
      effectiveLowLimit(), effectiveHighLimit(), effectivePrecision());
  return std::clamp(info.precision, 0, 17);
}

double WheelSwitchElement::displayedValue() const
{
  if (executeMode_ && hasRuntimeValue_) {
    return runtimeValue_;
  }
  return sampleValue();
}

double WheelSwitchElement::effectiveLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double WheelSwitchElement::effectiveHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

int WheelSwitchElement::effectivePrecision() const
{
  // Always use precision from PV Limits, not the separate precision_ field
  if (limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
  }
  // Use precisionDefault from limits regardless of source
  return std::clamp(limits_.precisionDefault, 0, 17);
}

double WheelSwitchElement::sampleValue() const
{
  return defaultSampleValue();
}

double WheelSwitchElement::defaultSampleValue() const
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
  return low + span * 0.5;
}

double WheelSwitchElement::clampToLimits(double value) const
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

double WheelSwitchElement::valueStep(Qt::KeyboardModifiers mods) const
{
  int decimals = formatDecimals();
  if (decimals < 0) {
    decimals = effectivePrecision();
  }
  double base = std::pow(10.0, -std::max(decimals, 0));
  if (!std::isfinite(base) || base <= 0.0) {
    base = 1.0;
  }
  if (mods & Qt::ControlModifier) {
    base *= 100.0;
  } else if (mods & Qt::ShiftModifier) {
    base *= 10.0;
  }
  double epsilon = valueEpsilon();
  if (base < epsilon) {
    base = epsilon;
  }
  return base;
}

double WheelSwitchElement::applyModifiersToStep(double step,
    Qt::KeyboardModifiers mods) const
{
  double adjusted = std::abs(step);
  if (!std::isfinite(adjusted) || adjusted <= 0.0) {
    return adjusted;
  }
  if (mods & Qt::ControlModifier) {
    adjusted *= 100.0;
  } else if (mods & Qt::ShiftModifier) {
    adjusted *= 10.0;
  }
  return adjusted;
}

int WheelSwitchElement::slotIndexForStep(const Layout &layout, double step) const
{
  if (!std::isfinite(step) || step <= 0.0) {
    return -1;
  }

  const double target = std::abs(step);
  const double tolerance = target * 1e-4 + 1e-9;
  int bestIndex = -1;
  double bestDiff = std::numeric_limits<double>::max();

  for (int i = 0; i < static_cast<int>(layout.columns.size()); ++i) {
    const Layout::Slot &slot = layout.columns.at(i);
    if (!slot.hasButtons || !std::isfinite(slot.step) || slot.step <= 0.0) {
      continue;
    }
    const double diff = std::abs(slot.step - target);
    if (diff <= tolerance) {
      return i;
    }
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIndex = i;
    }
  }

  return bestIndex;
}

int WheelSwitchElement::defaultSlotIndex(const Layout &layout) const
{
  int result = -1;
  double smallestStep = std::numeric_limits<double>::max();
  for (int i = 0; i < static_cast<int>(layout.columns.size()); ++i) {
    const Layout::Slot &slot = layout.columns.at(i);
    if (!slot.hasButtons || !std::isfinite(slot.step) || slot.step <= 0.0) {
      continue;
    }
    if (slot.step < smallestStep) {
      smallestStep = slot.step;
      result = i;
    }
  }
  return result;
}

void WheelSwitchElement::startRepeating(RepeatDirection direction, double step,
    int slotIndex)
{
  if (!isInteractive() || direction == RepeatDirection::kNone) {
    return;
  }

  cancelKeyboardEntry();
  repeatDirection_ = direction;
  repeatStep_ = std::abs(step);
  if (!std::isfinite(repeatStep_) || repeatStep_ <= 0.0) {
    repeatStep_ = 1.0;
  }
  selectedSlotIndex_ = slotIndex;
  pressedSlotIndex_ = slotIndex;
  pressedDirection_ = direction;

  performStep(direction, repeatStep_, true);
  repeatTimer_->setInterval(kRepeatInitialDelayMs);
  repeatTimer_->setSingleShot(true);
  repeatTimer_->start();
  update();
}

void WheelSwitchElement::stopRepeating()
{
  repeatTimer_->stop();
  repeatTimer_->setSingleShot(true);
  repeatDirection_ = RepeatDirection::kNone;
  repeatStep_ = 0.0;
  pressedSlotIndex_ = -1;
  pressedDirection_ = RepeatDirection::kNone;
  update();
}

void WheelSwitchElement::performStep(RepeatDirection direction, double step,
    bool forceSend)
{
  if (!isInteractive()) {
    return;
  }
  if (direction == RepeatDirection::kNone) {
    return;
  }
  double current = displayedValue();
  double target = current;
  if (direction == RepeatDirection::kUp) {
    target = current + step;
  } else if (direction == RepeatDirection::kDown) {
    target = current - step;
  }
  activateValue(target, forceSend);
}

void WheelSwitchElement::activateValue(double value, bool forceSend)
{
  double clamped = clampToLimits(value);
  if (!std::isfinite(clamped)) {
    return;
  }
  bool changed = !hasLastSentValue_ || std::abs(clamped - lastSentValue_) > valueEpsilon();
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  update();
  if (!activationCallback_) {
    return;
  }
  if (forceSend || changed) {
    activationCallback_(clamped);
    lastSentValue_ = clamped;
    hasLastSentValue_ = true;
  }
}

void WheelSwitchElement::beginKeyboardEntry()
{
  if (!isInteractive()) {
    return;
  }
  stopRepeating();
  keyboardEntryActive_ = true;
  keyboardEntryText_.clear();
  update();
}

void WheelSwitchElement::cancelKeyboardEntry()
{
  if (!keyboardEntryActive_ && keyboardEntryText_.isEmpty()) {
    return;
  }
  keyboardEntryActive_ = false;
  keyboardEntryText_.clear();
  update();
}

void WheelSwitchElement::commitKeyboardEntry()
{
  if (!keyboardEntryActive_) {
    return;
  }

  bool ok = false;
  const double value = keyboardEntryValue(&ok);
  if (!ok) {
    QApplication::beep();
    return;
  }

  keyboardEntryActive_ = false;
  keyboardEntryText_.clear();
  activateValue(value, true);
}

bool WheelSwitchElement::handleKeyboardEntryKey(QKeyEvent *event)
{
  if (!keyboardEntryActive_ || !event) {
    return false;
  }

  switch (event->key()) {
  case Qt::Key_Return:
  case Qt::Key_Enter:
    commitKeyboardEntry();
    event->accept();
    return true;
  case Qt::Key_Escape:
    cancelKeyboardEntry();
    event->accept();
    return true;
  case Qt::Key_Backspace:
  case Qt::Key_Delete: {
    QString updated = keyboardEntryText_;
    if (!updated.isEmpty()) {
      updated.chop(1);
    }
    keyboardEntryText_ = updated;
    update();
    event->accept();
    return true;
  }
  default:
    break;
  }

  if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier
          | Qt::MetaModifier)) {
    return false;
  }

  const QString text = event->text();
  if (text.isEmpty()) {
    return false;
  }

  const QChar ch = text.at(0);
  QString updated = keyboardEntryText_;
  if (ch.isDigit()) {
    updated += ch;
  } else if (ch == QLatin1Char('.')) {
    if (updated.contains(QLatin1Char('.'))) {
      QApplication::beep();
      event->accept();
      return true;
    }
    if (updated.isEmpty()) {
      updated = QStringLiteral("0.");
    } else if (updated == QStringLiteral("-")) {
      updated = QStringLiteral("-0.");
    } else {
      updated += ch;
    }
  } else if (ch == QLatin1Char('-')) {
    if (updated.startsWith(QLatin1Char('-'))) {
      updated.remove(0, 1);
    } else {
      updated.prepend(QLatin1Char('-'));
    }
  } else if (ch == QLatin1Char('+')) {
    if (updated.startsWith(QLatin1Char('-'))) {
      updated.remove(0, 1);
    }
  } else {
    return false;
  }

  if (!updateKeyboardEntryText(updated)) {
    QApplication::beep();
  }
  event->accept();
  return true;
}

bool WheelSwitchElement::updateKeyboardEntryText(const QString &text)
{
  const QString candidate = text;
  if (candidate.isEmpty() || candidate == QStringLiteral("-")
      || candidate == QStringLiteral("+") || candidate == QStringLiteral(".")
      || candidate == QStringLiteral("-.")
      || candidate == QStringLiteral("+.")) {
    keyboardEntryText_ = candidate;
    update();
    return true;
  }

  bool ok = false;
  const double value = candidate.toDouble(&ok);
  if (!ok || !std::isfinite(value)) {
    return false;
  }
  if (std::abs(clampToLimits(value) - value) > valueEpsilon()) {
    return false;
  }

  const WheelSwitchFormatInfo info = wheelSwitchFormatInfo(format_,
      effectiveLowLimit(), effectiveHighLimit(), effectivePrecision());
  bool overflow = false;
  formattedWheelSwitchValue(value, effectiveLowLimit(), effectiveHighLimit(),
      info, &overflow);
  if (overflow) {
    return false;
  }

  keyboardEntryText_ = candidate;
  update();
  return true;
}

double WheelSwitchElement::keyboardEntryValue(bool *ok) const
{
  if (ok) {
    *ok = false;
  }

  const QString trimmed = keyboardEntryText_.trimmed();
  if (trimmed.isEmpty() || trimmed == QStringLiteral("-")
      || trimmed == QStringLiteral("+") || trimmed == QStringLiteral(".")
      || trimmed == QStringLiteral("-.")
      || trimmed == QStringLiteral("+.")) {
    return 0.0;
  }

  bool parsed = false;
  const double value = trimmed.toDouble(&parsed);
  if (!parsed || !std::isfinite(value)) {
    return 0.0;
  }

  if (std::abs(clampToLimits(value) - value) > valueEpsilon()) {
    return 0.0;
  }

  const WheelSwitchFormatInfo info = wheelSwitchFormatInfo(format_,
      effectiveLowLimit(), effectiveHighLimit(), effectivePrecision());
  bool overflow = false;
  formattedWheelSwitchValue(value, effectiveLowLimit(), effectiveHighLimit(),
      info, &overflow);
  if (overflow) {
    return 0.0;
  }

  if (ok) {
    *ok = true;
  }
  return value;
}

QString WheelSwitchElement::keyboardEntryDisplayText() const
{
  return keyboardEntryText_;
}

bool WheelSwitchElement::selectedSlotStep(const Layout &layout,
    Qt::KeyboardModifiers mods, int *slotIndex, double *step) const
{
  int index = selectedSlotIndex_;
  if (index < 0 || index >= static_cast<int>(layout.columns.size())
      || !layout.columns.at(index).hasButtons) {
    index = defaultSlotIndex(layout);
  }
  if (index < 0 || index >= static_cast<int>(layout.columns.size())) {
    return false;
  }

  double selectedStep = layout.columns.at(index).step;
  selectedStep = applyModifiersToStep(selectedStep, mods);
  if (!std::isfinite(selectedStep) || selectedStep <= 0.0) {
    return false;
  }

  if (slotIndex) {
    *slotIndex = index;
  }
  if (step) {
    *step = selectedStep;
  }
  return true;
}

int WheelSwitchElement::moveSelectedSlot(const Layout &layout, int direction)
{
  if (direction == 0) {
    return selectedSlotIndex_;
  }

  int current = selectedSlotIndex_;
  if (current < 0 || current >= static_cast<int>(layout.columns.size())
      || !layout.columns.at(current).hasButtons) {
    current = defaultSlotIndex(layout);
  }
  if (current < 0) {
    return -1;
  }

  int next = current;
  while (true) {
    next += direction;
    if (next < 0 || next >= static_cast<int>(layout.columns.size())) {
      break;
    }
    if (layout.columns.at(next).hasButtons) {
      selectedSlotIndex_ = next;
      update();
      return next;
    }
  }

  selectedSlotIndex_ = current;
  update();
  return current;
}

void WheelSwitchElement::handleRepeatTimeout()
{
  if (repeatDirection_ == RepeatDirection::kNone) {
    return;
  }
  performStep(repeatDirection_, repeatStep_, false);
  repeatTimer_->setInterval(kRepeatIntervalMs);
  repeatTimer_->setSingleShot(false);
  repeatTimer_->start();
}

void WheelSwitchElement::updateCursor()
{
  if (!executeMode_) {
    unsetCursor();
    return;
  }
  if (isInteractive()) {
    setCursor(CursorUtils::arrowCursor());
  } else {
    setCursor(CursorUtils::forbiddenCursor());
  }
}

bool WheelSwitchElement::isInteractive() const
{
  return executeMode_ && runtimeConnected_ && runtimeWriteAccess_
      && static_cast<bool>(activationCallback_);
}

double WheelSwitchElement::valueEpsilon() const
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

bool WheelSwitchElement::forwardMouseEventToParent(QMouseEvent *event) const
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
