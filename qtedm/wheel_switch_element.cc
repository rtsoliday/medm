#include "wheel_switch_element.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

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
#include "medm_colors.h"
#include "cursor_utils.h"

namespace {

constexpr double kMinimumCenterHeight = 12.0;
constexpr double kMinimumButtonHeight = 7.0;
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
  pressedSlotIndex_ = -1;
  pressedDirection_ = RepeatDirection::kNone;
  repeatDirection_ = RepeatDirection::kNone;
  repeatStep_ = 0.0;
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

  if (event->button() != Qt::LeftButton || !isInteractive()) {
    QWidget::mousePressEvent(event);
    return;
  }

  setFocus(Qt::MouseFocusReason);

  const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
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
    if (column.upButton.contains(pos)) {
      const double step = applyModifiersToStep(baseStep, event->modifiers());
      startRepeating(RepeatDirection::kUp, step, i);
      handled = true;
      break;
    }
    if (column.downButton.contains(pos)) {
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

  const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  const Layout layout = layoutForRect(outer);

  switch (event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Right: {
    double step = valueStep(event->modifiers());
    if (!std::isfinite(step) || step <= 0.0) {
      step = 1.0;
    }
    int slotIndex = slotIndexForStep(layout, step);
    if (slotIndex < 0) {
      slotIndex = defaultSlotIndex(layout);
    }
    if (slotIndex >= 0) {
      startRepeating(RepeatDirection::kUp, step, slotIndex);
      update();
      event->accept();
      return;
    }
    break;
  }
  case Qt::Key_Down:
  case Qt::Key_Left: {
    double step = valueStep(event->modifiers());
    if (!std::isfinite(step) || step <= 0.0) {
      step = 1.0;
    }
    int slotIndex = slotIndexForStep(layout, step);
    if (slotIndex < 0) {
      slotIndex = defaultSlotIndex(layout);
    }
    if (slotIndex >= 0) {
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

  const bool upHovered = (hoveredSlotIndex_ == i
    && hoveredDirection_ == RepeatDirection::kUp);
  const bool downHovered = (hoveredSlotIndex_ == i
    && hoveredDirection_ == RepeatDirection::kDown);

  paintButton(painter, column.upButton, true, upPressed, enabled, upHovered);
  paintButton(painter, column.downButton, false, downPressed, enabled,
    downHovered);
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

  layout.text = displayText();

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

  const double maxAvailableWidth = std::max(0.0, bounds.width() - 4.0);
  const double desiredValueWidth = std::min(maxAvailableWidth,
      std::max(layout.valueRect.width(), totalWidth + 8.0));
  const double valueCenterX = bounds.center().x();
  double valueLeft = valueCenterX - desiredValueWidth / 2.0;
  const double minValueLeft = bounds.left() + 2.0;
  if (valueLeft < minValueLeft) {
    valueLeft = minValueLeft;
  }
  double valueRight = valueLeft + desiredValueWidth;
  const double maxValueRight = bounds.right() - 2.0;
  if (valueRight > maxValueRight) {
    const double shift = valueRight - maxValueRight;
    valueLeft = std::max(minValueLeft, valueLeft - shift);
    valueRight = valueLeft + desiredValueWidth;
  }
  layout.valueRect.setLeft(valueLeft);
  layout.valueRect.setRight(valueRight);

  double startX = layout.valueRect.left();
  if (totalWidth < layout.valueRect.width()) {
    startX += (layout.valueRect.width() - totalWidth) / 2.0;
  }

  int decimalIndex = layout.text.indexOf(QLatin1Char('.'));
  if (decimalIndex < 0) {
    decimalIndex = layout.text.size();
  }

  int digitsLeft = 0;
  for (int i = 0; i < decimalIndex; ++i) {
    if (layout.text.at(i).isDigit()) {
      ++digitsLeft;
    }
  }
  int leftCounter = digitsLeft;
  int rightCounter = 0;

  for (int i = 0; i < layout.text.size(); ++i) {
    const QChar ch = layout.text.at(i);
    const double width = charWidths.at(i);
    Layout::Slot slot;
    slot.character = ch;
    slot.charRect = QRectF(startX, layout.valueRect.top(), width,
        layout.valueRect.height());

    if (ch.isDigit()) {
      slot.hasButtons = true;
      int exponent = 0;
      if (i < decimalIndex) {
        --leftCounter;
        exponent = leftCounter;
      } else if (i > decimalIndex) {
        ++rightCounter;
        exponent = -rightCounter;
      } else {
        exponent = 0;
      }
      slot.exponent = exponent;
      slot.step = std::pow(10.0, exponent);

      const double inset = std::min(3.0, width * 0.2);
      const double buttonWidth = std::max(4.0, width - 2.0 * inset);
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

  return layout;
}

void WheelSwitchElement::updateHoverState(const QPointF &pos)
{
  const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  const Layout layout = layoutForRect(outer);

  int newIndex = -1;
  RepeatDirection newDirection = RepeatDirection::kNone;

  for (int i = 0; i < static_cast<int>(layout.columns.size()); ++i) {
    const Layout::Slot &column = layout.columns.at(i);
    if (!column.hasButtons) {
      continue;
    }
    if (column.upButton.contains(pos)) {
      newIndex = i;
      newDirection = RepeatDirection::kUp;
      break;
    }
    if (column.downButton.contains(pos)) {
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
    painter.drawRoundedRect(rect, 3.0, 3.0);
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
  if (executeMode_) {
    if (!runtimeConnected_ || !hasRuntimeValue_) {
      return QString();
    }
  }

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

  repeatDirection_ = direction;
  repeatStep_ = std::abs(step);
  if (!std::isfinite(repeatStep_) || repeatStep_ <= 0.0) {
    repeatStep_ = 1.0;
  }
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
