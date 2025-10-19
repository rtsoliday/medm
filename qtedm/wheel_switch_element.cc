#include "wheel_switch_element.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QApplication>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QTimer>

#include "legacy_fonts.h"

namespace {

constexpr double kMinimumCenterHeight = 24.0;
constexpr double kMinimumButtonHeight = 14.0;
constexpr int kRepeatInitialDelayMs = 350;
constexpr int kRepeatIntervalMs = 90;
constexpr double kValueEpsilonFactor = 1e-9;
constexpr short kInvalidSeverity = 3;

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

WheelSwitchElement::WheelSwitchElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  setFocusPolicy(Qt::StrongFocus);

  limits_.lowSource = PvLimitSource::kDefault;
  limits_.highSource = PvLimitSource::kDefault;
  limits_.precisionSource = PvLimitSource::kDefault;
  limits_.lowDefault = 0.0;

  limits_.highDefault = 100.0;
  limits_.precisionDefault = 1;
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

QString WheelSwitchElement::channel() const
{
  return channel_;
}

void WheelSwitchElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  setToolTip(channel_.trimmed());
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
    update();
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
  topPressed_ = false;
  bottomPressed_ = false;
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
  if (event->button() != Qt::LeftButton || !isInteractive()) {
    QWidget::mousePressEvent(event);
    return;
  }

  setFocus(Qt::MouseFocusReason);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif

  const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  const Layout layout = layoutForRect(outer);
  bool handled = false;

  if (layout.topButton.contains(pos)) {
    topPressed_ = true;
    bottomPressed_ = false;
    startRepeating(RepeatDirection::kUp, event->modifiers());
    handled = true;
  } else if (layout.bottomButton.contains(pos)) {
    bottomPressed_ = true;
    topPressed_ = false;
    startRepeating(RepeatDirection::kDown, event->modifiers());
    handled = true;
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
  if (event->button() != Qt::LeftButton || !isInteractive()) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  if (topPressed_ || bottomPressed_ || repeatDirection_ != RepeatDirection::kNone) {
    stopRepeating();
    event->accept();
  } else {
    QWidget::mouseReleaseEvent(event);
  }
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

  switch (event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Right:
    topPressed_ = true;
    bottomPressed_ = false;
    startRepeating(RepeatDirection::kUp, event->modifiers());
    update();
    event->accept();
    return;
  case Qt::Key_Down:
  case Qt::Key_Left:
    bottomPressed_ = true;
    topPressed_ = false;
    startRepeating(RepeatDirection::kDown, event->modifiers());
    update();
    event->accept();
    return;
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
  case Qt::Key_Right:
  case Qt::Key_Down:
  case Qt::Key_Left:
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

  const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  painter.fillRect(outer, effectiveBackground());

  QPen borderPen(Qt::black);
  borderPen.setWidthF(1.0);
  painter.setPen(borderPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(outer);

  const Layout layout = layoutForRect(outer);
  const bool enabled = isInteractive();

  paintButton(painter, layout.topButton, true, topPressed_, enabled);
  paintButton(painter, layout.bottomButton, false, bottomPressed_, enabled);

  if (layout.valueRect.height() > 6.0 && layout.valueRect.width() > 6.0) {
    paintValueDisplay(painter, layout.valueRect);
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
        return QColor(204, 204, 204);
      }
      return alarmColorForSeverity(runtimeSeverity_);
    }
  }
  return effectiveForeground();
}

WheelSwitchElement::Layout WheelSwitchElement::layoutForRect(const QRectF &bounds) const
{
  Layout layout{};
  layout.outer = bounds;

  qreal buttonHeight = std::max(kMinimumButtonHeight, bounds.height() * 0.22);
  const qreal maxButtonHeight = std::max(kMinimumButtonHeight,
      (bounds.height() - kMinimumCenterHeight) / 2.0);
  if (buttonHeight > maxButtonHeight) {
    buttonHeight = maxButtonHeight;
  }
  if (bounds.height() - 2.0 * buttonHeight < kMinimumCenterHeight) {
    buttonHeight = std::max(kMinimumButtonHeight,
        (bounds.height() - kMinimumCenterHeight) / 2.0);
  }
  buttonHeight = std::clamp(buttonHeight, kMinimumButtonHeight, bounds.height() / 2.0);

  layout.topButton = QRectF(bounds.left() + 1.0, bounds.top() + 1.0,
      bounds.width() - 2.0, std::max(0.0, buttonHeight - 2.0));
  layout.bottomButton = QRectF(bounds.left() + 1.0,
      bounds.bottom() - buttonHeight + 1.0, bounds.width() - 2.0,
      std::max(0.0, buttonHeight - 2.0));

  layout.valueRect = QRectF(bounds.left() + 4.0, layout.topButton.bottom() + 4.0,
      bounds.width() - 8.0,
      layout.bottomButton.top() - layout.topButton.bottom() - 8.0);
  return layout;
}

QColor WheelSwitchElement::buttonFillColor(bool isUp, bool pressed, bool enabled) const
{
  QColor base = effectiveBackground();
  if (!base.isValid()) {
    base = QColor(220, 220, 220);
  }
  if (!enabled) {
    return QColor(210, 210, 210);
  }
  if (pressed) {
    return isUp ? blendedColor(base, 105) : blendedColor(base, 85);
  }
  return isUp ? blendedColor(base, 120) : blendedColor(base, 95);
}

void WheelSwitchElement::paintButton(QPainter &painter, const QRectF &rect,
    bool isUp, bool pressed, bool enabled) const
{
  if (!rect.isValid() || rect.width() < 4.0 || rect.height() < 4.0) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(buttonFillColor(isUp, pressed, enabled));
  painter.drawRoundedRect(rect, 3.0, 3.0);

  painter.setPen(QPen(QColor(0, 0, 0, 100)));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(rect, 3.0, 3.0);

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
    const QRectF &rect) const
{
  painter.save();
  QColor base = effectiveBackground();
  QColor fill = base.isValid() ? blendedColor(base, 115) : QColor(245, 245, 245);
  painter.setPen(Qt::NoPen);
  painter.setBrush(fill);
  painter.drawRoundedRect(rect, 3.0, 3.0);

  painter.setPen(QPen(QColor(0, 0, 0, 90)));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(rect, 3.0, 3.0);

  QString text = formattedSampleValue();
  painter.setPen(valueForeground());
  QFont valueFont = wheelSwitchFontForHeight(height());
  if (valueFont.family().isEmpty()) {
    valueFont = font();
  }
  painter.setFont(valueFont);
  painter.drawText(rect.adjusted(4.0, 0.0, -4.0, 0.0),
      Qt::AlignCenter | Qt::AlignVCenter, text);
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

QString WheelSwitchElement::formattedSampleValue() const
{
  const double value = displayedValue();
  const QString trimmed = format_.trimmed();
  if (!trimmed.isEmpty()) {
    int dotIndex = trimmed.indexOf(QLatin1Char('.'));
    int decimals = 0;
    if (dotIndex >= 0) {
      decimals = trimmed.size() - dotIndex - 1;
    }
    decimals = std::clamp(decimals, 0, 17);
    QString formatted = QString::number(value, 'f', decimals);
    if (trimmed.size() > formatted.size()) {
      formatted = formatted.rightJustified(trimmed.size(), QLatin1Char(' '));
    }
    return formatted;
  }

  int digits = effectivePrecision();
  return QString::number(value, 'f', digits);
}

int WheelSwitchElement::formatDecimals() const
{
  const QString trimmed = format_.trimmed();
  if (trimmed.isEmpty()) {
    return -1;
  }
  const int dotIndex = trimmed.indexOf(QLatin1Char('.'));
  if (dotIndex < 0) {
    return 0;
  }
  const int decimals = trimmed.size() - dotIndex - 1;
  return std::clamp(decimals, 0, 17);
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
  if (limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
    return std::clamp(limits_.precisionDefault, 0, 17);
  }
  int decimals = formatDecimals();
  if (decimals >= 0) {
    return decimals;
  }
  return std::clamp(static_cast<int>(std::round(precision_)), 0, 17);
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

void WheelSwitchElement::startRepeating(RepeatDirection direction,
    Qt::KeyboardModifiers mods)
{
  repeatDirection_ = direction;
  repeatStep_ = valueStep(mods);
  if (!std::isfinite(repeatStep_) || repeatStep_ <= 0.0) {
    repeatStep_ = 1.0;
  }
  performStep(direction, repeatStep_, true);
  repeatTimer_->setInterval(kRepeatInitialDelayMs);
  repeatTimer_->setSingleShot(true);
  repeatTimer_->start();
}

void WheelSwitchElement::stopRepeating()
{
  repeatTimer_->stop();
  repeatTimer_->setSingleShot(true);
  repeatDirection_ = RepeatDirection::kNone;
  repeatStep_ = 0.0;
  topPressed_ = false;
  bottomPressed_ = false;
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
    setCursor(Qt::ArrowCursor);
  } else {
    setCursor(Qt::ForbiddenCursor);
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
