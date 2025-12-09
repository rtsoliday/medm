#include "slider_element.h"

#include "update_coordinator.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QCoreApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QMouseEvent>
#include <QCursor>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPointF>
#include <QStringList>

#include "cursor_utils.h"
#include "text_font_utils.h"
#include "window_utils.h"

namespace {

constexpr double kSampleValue = 0.6;
constexpr int kTickCount = 11;
constexpr short kInvalidSeverity = 3;
constexpr double kValueEpsilonFactor = 1e-6;
constexpr double kHorizontalLabelSpacing = 4.0;

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

QFont shrinkFontToFit(const QFont &baseFont, const QStringList &texts,
    const QSizeF &targetSize)
{
  if (texts.isEmpty() || targetSize.width() <= 0.0 || targetSize.height() <= 0.0) {
    return baseFont;
  }

  QFont font(baseFont);
  int pixelSize = font.pixelSize();
  double size = pixelSize > 0 ? static_cast<double>(pixelSize)
      : font.pointSizeF();
  if (size <= 0.0) {
    const int pointSize = font.pointSize();
    size = pointSize > 0 ? static_cast<double>(pointSize) : 12.0;
    font.setPointSizeF(size);
    pixelSize = font.pixelSize();
  }

  auto applySize = [&](double newSize) {
    const double clamped = std::max(1.0, newSize);
    if (pixelSize > 0) {
      font.setPixelSize(std::max(1, static_cast<int>(std::round(clamped))));
    } else {
      font.setPointSizeF(clamped);
    }
  };

  auto fits = [&]() {
    const QFontMetricsF metrics(font);
    double lineHeight = metrics.height();
    if (lineHeight <= 0.0) {
      lineHeight = metrics.ascent() + metrics.descent();
    }
    if (lineHeight <= 0.0) {
      return true;
    }
    const double totalHeight = lineHeight * texts.size();
    if (totalHeight > targetSize.height() + 0.1) {
      return false;
    }
    const double availableWidth = targetSize.width();
    for (const QString &text : texts) {
      if (text.isEmpty()) {
        continue;
      }
      if (metrics.horizontalAdvance(text) > availableWidth + 0.1) {
        return false;
      }
    }
    return true;
  };

  constexpr double kMinSize = 6.0;
  applySize(size);
  int iterations = 0;
  while (!fits() && size > kMinSize && iterations < 64) {
    size -= 1.0;
    if (size < kMinSize) {
      size = kMinSize;
    }
    applySize(size);
    ++iterations;
  }

  return font;
}

QFont shrinkFontToFitHorizontal(const QFont &baseFont, const QString &leftText,
    const QString &centerText, const QString &rightText, const QSizeF &targetSize,
    bool showCenter)
{
  if (targetSize.width() <= 0.0 || targetSize.height() <= 0.0) {
    return baseFont;
  }

  QFont font(baseFont);
  int pixelSize = font.pixelSize();
  double size = pixelSize > 0 ? static_cast<double>(pixelSize)
      : font.pointSizeF();
  if (size <= 0.0) {
    const int pointSize = font.pointSize();
    size = pointSize > 0 ? static_cast<double>(pointSize) : 12.0;
    font.setPointSizeF(size);
    pixelSize = font.pixelSize();
  }

  auto applySize = [&](double newSize) {
    const double clamped = std::max(1.0, newSize);
    if (pixelSize > 0) {
      font.setPixelSize(std::max(1, static_cast<int>(std::round(clamped))));
    } else {
      font.setPointSizeF(clamped);
    }
  };

  auto fits = [&]() {
    const QFontMetricsF metrics(font);
    double lineHeight = metrics.height();
    if (lineHeight <= 0.0) {
      lineHeight = metrics.ascent() + metrics.descent();
    }
    if (lineHeight > targetSize.height() + 0.1) {
      return false;
    }
    const double availableWidth = targetSize.width();
    const double leftWidth = leftText.isEmpty()
        ? 0.0 : metrics.horizontalAdvance(leftText);
    const double rightWidth = rightText.isEmpty()
        ? 0.0 : metrics.horizontalAdvance(rightText);
    const double centerWidth = showCenter && !centerText.isEmpty()
        ? metrics.horizontalAdvance(centerText) : 0.0;
    if (leftWidth > availableWidth + 0.1
        || rightWidth > availableWidth + 0.1
        || centerWidth > availableWidth + 0.1) {
      return false;
    }
    const double spacing = kHorizontalLabelSpacing;
    if (leftWidth + rightWidth > availableWidth - (showCenter ? spacing : 0.0)) {
      return false;
    }
    if (showCenter) {
      const double remaining = availableWidth - leftWidth - rightWidth;
      if (remaining < centerWidth + spacing) {
        return false;
      }
    }
    return true;
  };

  constexpr double kMinSize = 6.0;
  applySize(size);
  int iterations = 0;
  while (!fits() && size > kMinSize && iterations < 64) {
    size -= 1.0;
    if (size < kMinSize) {
      size = kMinSize;
    }
    applySize(size);
    ++iterations;
  }

  return font;
}

} // namespace

SliderElement::SliderElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  setFocusPolicy(Qt::NoFocus);
  limits_.lowSource = PvLimitSource::kDefault;
  limits_.highSource = PvLimitSource::kDefault;
  limits_.precisionSource = PvLimitSource::kDefault;
  limits_.lowDefault = 0.0;
  limits_.highDefault = 100.0;
  limits_.precisionDefault = 0;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
}

void SliderElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool SliderElement::isSelected() const
{
  return selected_;
}

QColor SliderElement::foregroundColor() const
{
  return foregroundColor_;
}

void SliderElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor SliderElement::backgroundColor() const
{
  return backgroundColor_;
}

void SliderElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode SliderElement::colorMode() const
{
  return colorMode_;
}

void SliderElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

MeterLabel SliderElement::label() const
{
  return label_;
}

void SliderElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

BarDirection SliderElement::direction() const
{
  return direction_;
}

void SliderElement::setDirection(BarDirection direction)
{
  if (direction_ == direction) {
    return;
  }
  direction_ = direction;
  update();
}

double SliderElement::increment() const
{
  return increment_;
}

void SliderElement::setIncrement(double increment)
{
  double sanitized = std::isfinite(increment) ? increment : 0.0;
  if (sanitized < 0.0) {
    sanitized = -sanitized;
  }
  if (std::abs(increment_ - sanitized) < 1e-9) {
    return;
  }
  increment_ = sanitized;
  update();
}

const PvLimits &SliderElement::limits() const
{
  return limits_;
}

void SliderElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  runtimeLimitsValid_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  update();
}

bool SliderElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void SliderElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool SliderElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void SliderElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool SliderElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void SliderElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool SliderElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void SliderElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool SliderElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void SliderElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString SliderElement::channel() const
{
  return channel_;
}

void SliderElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  setToolTip(channel_);
  update();
}

void SliderElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  if (dragging_) {
    releaseMouse();
    dragging_ = false;
  }
  executeMode_ = execute;
  setFocusPolicy(executeMode_ ? Qt::StrongFocus : Qt::NoFocus);
  if (!executeMode_ && hasFocus()) {
    clearFocus();
  }
  clearRuntimeState();
  updateCursor();
}

bool SliderElement::isExecuteMode() const
{
  return executeMode_;
}

void SliderElement::setRuntimeConnected(bool connected)
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

void SliderElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  updateCursor();
}

void SliderElement::setRuntimeSeverity(short severity)
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

void SliderElement::setRuntimeLimits(double low, double high)
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

void SliderElement::setRuntimePrecision(int precision)
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

void SliderElement::setRuntimeValue(double value)
{
  if (!executeMode_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }
  bool firstValue = !hasRuntimeValue_;
  bool changed = firstValue || std::abs(value - runtimeValue_) > sliderEpsilon();
  runtimeValue_ = value;
  hasRuntimeValue_ = true;
  if (!dragging_ && changed) {
    update();
  }
}

void SliderElement::clearRuntimeState()
{
  if (dragging_) {
    releaseMouse();
  }
  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = 0;
  runtimeLimitsValid_ = false;
  runtimePrecision_ = -1;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  hasRuntimeValue_ = false;
  runtimeValue_ = defaultSampleValue();
  dragging_ = false;
  dragValue_ = runtimeValue_;
  hasLastSentValue_ = false;
  lastSentValue_ = runtimeValue_;
  updateCursor();
  update();
}

void SliderElement::setActivationCallback(const std::function<void(double)> &callback)
{
  activationCallback_ = callback;
  hasLastSentValue_ = false;
  updateCursor();
}

void SliderElement::mousePressEvent(QMouseEvent *event)
{
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

  if (!isInteractive() || event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif
  setFocus(Qt::MouseFocusReason);

  QRectF limitRect;
  QRectF channelRect;
  const QRectF trackRect = trackRectForPainting(rect().adjusted(2.0, 2.0,
      -2.0, -2.0), limitRect, channelRect);
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    QWidget::mousePressEvent(event);
    return;
  }

  const QRectF thumbRect = thumbRectForTrack(trackRect).adjusted(-1.0, -1.0,
      1.0, 1.0);

  if (thumbRect.contains(pos)) {
    beginDrag(currentDisplayedValue(), false);
    event->accept();
    return;
  }

  if (!trackRect.contains(pos)) {
    QWidget::mousePressEvent(event);
    return;
  }

  double step = keyboardStep(event->modifiers());
  if (!std::isfinite(step) || step <= 0.0) {
    step = 1.0;
  }

  const double currentValue = currentDisplayedValue();
  const double requestedValue = valueFromPosition(pos);
  const double epsilon = sliderEpsilon();

  int direction = 0;
  if (std::isfinite(currentValue) && std::isfinite(requestedValue)) {
    if (requestedValue > currentValue + epsilon) {
      direction = 1;
    } else if (requestedValue < currentValue - epsilon) {
      direction = -1;
    }
  }

  if (direction == 0) {
    const QPointF thumbCenter = thumbRect.center();
    if (isVertical()) {
      const double delta = pos.y() - thumbCenter.y();
      if (std::abs(delta) > 0.5) {
        direction = delta > 0.0 ? (isDirectionInverted() ? 1 : -1)
                                : (isDirectionInverted() ? -1 : 1);
      }
    } else {
      const double delta = pos.x() - thumbCenter.x();
      if (std::abs(delta) > 0.5) {
        direction = delta > 0.0 ? (isDirectionInverted() ? -1 : 1)
                                : (isDirectionInverted() ? 1 : -1);
      }
    }
  }

  if (direction != 0) {
    applyKeyboardDelta(direction * step);
  }

  event->accept();
}

void SliderElement::mouseMoveEvent(QMouseEvent *event)
{
  if (!dragging_) {
    QWidget::mouseMoveEvent(event);
    return;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif
  updateDrag(valueFromPosition(pos), false);
  event->accept();
}

void SliderElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (!dragging_ || event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif
  QRectF limitRect;
  QRectF channelRect;
  const QRectF trackRect = trackRectForPainting(rect().adjusted(2.0, 2.0,
      -2.0, -2.0), limitRect, channelRect);

  bool releaseOnThumb = false;
  if (trackRect.isValid() && !trackRect.isEmpty()) {
    QRectF thumbRect = thumbRectForTrack(trackRect).adjusted(-1.0, -1.0,
        1.0, 1.0);
    releaseOnThumb = thumbRect.contains(pos);
  }

  if (releaseOnThumb) {
    endDrag(currentDisplayedValue(), false);
  } else {
    endDrag(valueFromPosition(pos), true);
  }
  event->accept();
}

void SliderElement::keyPressEvent(QKeyEvent *event)
{
  if (!isInteractive()) {
    QWidget::keyPressEvent(event);
    return;
  }

  const double step = keyboardStep(event->modifiers());
  double delta = 0.0;
  bool handled = false;
  switch (event->key()) {
  case Qt::Key_Right:
    delta = isDirectionInverted() ? -step : step;
    handled = true;
    break;
  case Qt::Key_Left:
    delta = isDirectionInverted() ? step : -step;
    handled = true;
    break;
  case Qt::Key_Up:
    if (isVertical()) {
      delta = isDirectionInverted() ? -step : step;
      handled = true;
    }
    break;
  case Qt::Key_Down:
    if (isVertical()) {
      delta = isDirectionInverted() ? step : -step;
      handled = true;
    }
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }

  if (!handled || !std::isfinite(step) || step <= 0.0
      || delta == 0.0 || !std::isfinite(delta)) {
    QWidget::keyPressEvent(event);
    return;
  }

  applyKeyboardDelta(delta);
  event->accept();
}

void SliderElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.fillRect(rect(), effectiveBackground());

  // In execute mode, don't draw slider elements if disconnected or no channel
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  QRectF limitRect;
  QRectF channelRect;
  QRectF trackRect = trackRectForPainting(rect().adjusted(0.0, 0.0, 0.0, 0.0),
      limitRect, channelRect);
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  paintTrack(painter, trackRect);
  /* Ticks removed per user request */
  /* paintTicks(painter, trackRect); */
  paintThumb(painter, trackRect);
  paintLabels(painter, trackRect, limitRect, channelRect);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QRectF SliderElement::trackRectForPainting(QRectF contentRect,
    QRectF &limitRect, QRectF &channelRect) const
{
  limitRect = QRectF();
  channelRect = QRectF();

  QRectF workingRect = contentRect;
  const bool vertical = isVertical();
  const bool showChannel = label_ == MeterLabel::kChannel;
  const bool showLimits = shouldShowLimitLabels();

  const qreal widgetLeft = 0.0;
  const qreal widgetRight = static_cast<qreal>(width());

  if (vertical) {
    if (showChannel) {
      const qreal maxLabelHeight = std::min<qreal>(24.0,
          workingRect.height() * 0.35);
      if (maxLabelHeight > 6.0) {
        channelRect = QRectF(widgetLeft, workingRect.top(),
            widgetRight - widgetLeft, maxLabelHeight);
        workingRect.setTop(channelRect.bottom() + 4.0);
      }
    }    
    if (showLimits) {
      const qreal maxLabelWidth = std::min<qreal>(24.0,
          workingRect.width() * 0.35);
      if (maxLabelWidth > 6.0) {
        limitRect = QRectF(workingRect.left(), workingRect.top(), maxLabelWidth,
            workingRect.height());
        workingRect.setLeft(limitRect.right() + 4.0);
        if (limitRect.isValid()) {
          const qreal trackBoundary = workingRect.left();
          const qreal expandedRight = trackBoundary + 7.0;
          limitRect.setRight(std::min(expandedRight, contentRect.right()));
        }
      }
    }
  } else {
    if (showChannel) {
      const qreal maxLabelHeight = std::min<qreal>(30.0,
          workingRect.height() * 0.35);
      if (maxLabelHeight > 6.0) {
        channelRect = QRectF(widgetLeft, workingRect.top(),
            widgetRight - widgetLeft, maxLabelHeight);
        const qreal desiredFinalGap = 2.0;
        const qreal preAdjustGap = std::max<qreal>(0.0, desiredFinalGap - 1.0);
        const qreal availablePreGap = std::max<qreal>(0.0,
            workingRect.bottom() - channelRect.bottom());
        const qreal clampedPreGap = std::min(preAdjustGap, availablePreGap);
        workingRect.setTop(channelRect.bottom() + clampedPreGap);
        if (workingRect.top() > workingRect.bottom()) {
          workingRect.setTop(workingRect.bottom());
        }
      }
    }
    if (showLimits) {
      const qreal maxLabelHeight = std::min<qreal>(30.0,
          workingRect.height() * 0.35);
      if (maxLabelHeight > 6.0) {
        limitRect = QRectF(workingRect.left(),
            workingRect.bottom() - maxLabelHeight, workingRect.width(),
            maxLabelHeight);
        workingRect.setBottom(limitRect.top() - 4.0);
        if (limitRect.isValid()) {
          limitRect.setTop(workingRect.bottom());
          const qreal expandedBottom = std::min(limitRect.bottom() + 2.0,
              contentRect.bottom());
          limitRect.setBottom(expandedBottom);
        }
      }
    }
  }

  //workingRect = workingRect.adjusted(4.0, 4.0, -4.0, -4.0);
  workingRect = workingRect.adjusted(1.0, 1.0, -2.0, -2.0);
  if (workingRect.width() < 2.0 || workingRect.height() < 2.0) {
    return QRectF();
  }

  /* Track size matches MEDM's heightDivisor logic:
   * - LABEL_NONE/NO_DECORATIONS: divisor = 1 (100% of dimension)
   * - OUTLINE/LIMITS: divisor = 2 (50% of dimension)
   * - CHANNEL: divisor = 3 (33.3% of dimension)
   * Track thickness is calculated from the FULL widget dimension (contentRect),
   * not from workingRect which has already had label space removed.
   */
  int heightDivisor = 1;
  if (label_ == MeterLabel::kOutline || label_ == MeterLabel::kLimits) {
    heightDivisor = 2;
  } else if (label_ == MeterLabel::kChannel) {
    heightDivisor = 3;
  }

  if (vertical) {
    const qreal trackWidth = std::max<qreal>(9.0,
        contentRect.width() / heightDivisor);
  const qreal trackRight = contentRect.right() + 1.0;
  const qreal availableWidth = std::max<qreal>(0.0,
    trackRight - workingRect.left());
  if (availableWidth <= 0.0) {
    return QRectF();
  }
  const qreal clampedTrackWidth = std::min(trackWidth, availableWidth);
  const qreal trackLeft = trackRight - clampedTrackWidth;
  if (showLimits && limitRect.isValid()) {
    const qreal adjustedRight = trackLeft - 1.0;
    limitRect.setRight(adjustedRight);
    if (limitRect.right() < limitRect.left()) {
      limitRect.setRight(limitRect.left());
    }
  }
    /* Reduce track height to prevent thumb from extending beyond edges */
    const qreal thumbHeight = std::max(workingRect.height() * 0.10, 30.0);
    const qreal reducedHeight = std::max(0.0, workingRect.height() - thumbHeight);
  return QRectF(trackLeft,
    workingRect.top() + thumbHeight / 2.0, clampedTrackWidth, reducedHeight);
  }

  const qreal trackHeight = std::max<qreal>(9.0,
      contentRect.height() / heightDivisor);
  /* Ensure track doesn't extend beyond workingRect to avoid overlapping labels */
  const qreal clampedTrackHeight = std::max(9.0, std::min(trackHeight, workingRect.height()));
  const qreal centerY = workingRect.center().y();
  /* Reduce track width to prevent thumb from extending beyond edges */
  const qreal thumbWidth = std::max(workingRect.width() * 0.10, 30.0);
  const qreal reducedWidth = std::max(0.0, workingRect.width() - thumbWidth);
  return QRectF(workingRect.left() + thumbWidth / 2.0,
      centerY - clampedTrackHeight / 2.0, reducedWidth, clampedTrackHeight);
}

void SliderElement::paintTrack(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  painter.setPen(Qt::NoPen);
  
  const QColor baseColor = effectiveBackground();
  
  /* Check if background is very dark using perceived luminance */
  const int r = baseColor.red();
  const int g = baseColor.green();
  const int b = baseColor.blue();
  const double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
  const bool isVeryDark = luminance < 40.0;
  
  /* Draw main track background */
  QColor trackBg;
  if (isVeryDark) {
    /* For very dark backgrounds, brighten by 20% instead of darkening */
    const int brightenAmount = 52; /* 20% of 255 ≈ 26 */
    trackBg = QColor(std::min(255, r + brightenAmount), 
                     std::min(255, g + brightenAmount), 
                     std::min(255, b + brightenAmount));
  } else {
    trackBg = baseColor.darker(120);
  }
  painter.setBrush(trackBg);
  painter.drawRoundedRect(trackRect, 3.0, 3.0);
  
  /* Draw lowered bevel (2 pixels) */
  /* Dark shadow on top/left */
  QColor shadowColor;
  if (isVeryDark) {
    /* For very dark backgrounds, ensure shadow is darker but visible */
    shadowColor = QColor(std::max(0, r - 30), std::max(0, g - 30), std::max(0, b - 30));
  } else {
    shadowColor = baseColor.darker(150);
  }
  QPen bevelPen(shadowColor, 2.0);
  painter.setPen(bevelPen);
  painter.setBrush(Qt::NoBrush);
  QRectF bevelRect = trackRect.adjusted(1.0, 1.0, -1.0, -1.0);
  
  if (isVertical()) {
    /* Left edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.left(), bevelRect.bottom()));
    /* Top edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.top()));
  } else {
    /* Top edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.top()));
    /* Left edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.left(), bevelRect.bottom()));
  }
  
  /* Light highlight on bottom/right */
  QColor highlightColor;
  if (isVeryDark) {
    /* For very dark backgrounds, add absolute lightening to ensure visibility */
    highlightColor = QColor(std::min(255, r + 40), std::min(255, g + 40), std::min(255, b + 40));
  } else {
    highlightColor = baseColor.lighter(130);
  }
  bevelPen.setColor(highlightColor);
  painter.setPen(bevelPen);
  
  if (isVertical()) {
    /* Right edge - light highlight */
    painter.drawLine(QPointF(bevelRect.right(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
    /* Bottom edge - light highlight */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.bottom()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
  } else {
    /* Bottom edge - light highlight */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.bottom()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
    /* Right edge - light highlight */
    painter.drawLine(QPointF(bevelRect.right(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
  }
  
  painter.restore();

  //painter.save();
  //QPen debugPen(Qt::red);
  //debugPen.setWidthF(1.0);
  //painter.setPen(debugPen);
  //painter.setBrush(Qt::NoBrush);
  //painter.drawRect(trackRect.adjusted(0.5, 0.5, -0.5, -0.5));
  //painter.restore();
}

void SliderElement::paintThumb(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  
  const QColor bgColor = effectiveBackground();
  const QColor thumbColor = bgColor;
  
  /* Check if background is very dark using perceived luminance */
  const int r = bgColor.red();
  const int g = bgColor.green();
  const int b = bgColor.blue();
  const double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
  const bool isVeryDark = luminance < 40.0;
  
  /* Calculate thumb position, ensuring it stays within track bounds (minus bevel) */
  QRectF thumbRect = thumbRectForTrack(trackRect);
  
  /* Draw main thumb body */
  painter.setPen(Qt::NoPen);
  painter.setBrush(thumbColor);
  painter.drawRoundedRect(thumbRect, 2.0, 2.0);
  
  /* Draw raised bevel (2 pixels) */
  QColor highlightColor;
  if (isVeryDark) {
    /* For very dark backgrounds, add absolute lightening to ensure visibility */
    highlightColor = QColor(std::min(255, r + 50), std::min(255, g + 50), std::min(255, b + 50));
  } else {
    highlightColor = thumbColor.lighter(140);
  }
  QPen bevelPen(highlightColor, 2.0);
  painter.setPen(bevelPen);
  painter.setBrush(Qt::NoBrush);
  QRectF bevelRect = thumbRect.adjusted(1.0, 1.0, -1.0, -1.0);
  
  /* Light highlight on top/left */
  if (isVertical()) {
    /* Left edge - light highlight */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.left(), bevelRect.bottom()));
    /* Top edge - light highlight */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.top()));
  } else {
    /* Top edge - light highlight */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.top()));
    /* Left edge - light highlight */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.top()),
                     QPointF(bevelRect.left(), bevelRect.bottom()));
  }
  
  /* Dark shadow on bottom/right */
  QColor shadowColor;
  if (isVeryDark) {
    /* For very dark backgrounds, use subtle darkening */
    shadowColor = QColor(std::max(0, r - 15), std::max(0, g - 15), std::max(0, b - 15));
  } else {
    shadowColor = thumbColor.darker(160);
  }
  bevelPen.setColor(shadowColor);
  painter.setPen(bevelPen);
  
  if (isVertical()) {
    /* Right edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.right(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
    /* Bottom edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.bottom()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
  } else {
    /* Bottom edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.left(), bevelRect.bottom()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
    /* Right edge - dark shadow */
    painter.drawLine(QPointF(bevelRect.right(), bevelRect.top()),
                     QPointF(bevelRect.right(), bevelRect.bottom()));
  }
  
  /* Draw center line: black for light backgrounds, white for dark backgrounds */
  const bool isLightBackground = luminance > 127.5;
  const QColor centerLineColor = isLightBackground ? Qt::black : Qt::white;
  QPen centerPen(centerLineColor, 1.0);
  painter.setPen(centerPen);
  
  if (isVertical()) {
    /* Horizontal center line */
    const qreal centerY = thumbRect.center().y();
    painter.drawLine(QPointF(thumbRect.left() + 2.0, centerY),
                     QPointF(thumbRect.right() - 2.0, centerY));
  } else {
    /* Vertical center line */
    const qreal centerX = thumbRect.center().x();
    painter.drawLine(QPointF(centerX, thumbRect.top() + 2.0),
                     QPointF(centerX, thumbRect.bottom() - 2.0));
  }
  
  painter.restore();
}

void SliderElement::paintTicks(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  QPen pen(effectiveForeground().darker(140));
  pen.setWidth(1);
  painter.setPen(pen);

  for (int i = 0; i < kTickCount; ++i) {
    const double ratio = static_cast<double>(i) / (kTickCount - 1);
    if (isVertical()) {
      const qreal y = trackRect.top() + ratio * trackRect.height();
      const qreal x1 = trackRect.left() - 6.0;
      const qreal x2 = trackRect.right() + 6.0;
      painter.drawLine(QPointF(x1, y), QPointF(x2, y));
    } else {
      const qreal x = trackRect.left() + ratio * trackRect.width();
      const qreal y1 = trackRect.top() - 6.0;
      const qreal y2 = trackRect.bottom() + 6.0;
      painter.drawLine(QPointF(x, y1), QPointF(x, y2));
    }
  }

  painter.restore();
}

void SliderElement::paintLabels(QPainter &painter, const QRectF &trackRect,
    const QRectF &limitRect, const QRectF &channelRect) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  painter.save();
  const QColor penColor = effectiveForeground();
  painter.setPen(penColor);
  painter.setBrush(Qt::NoBrush);

  if (label_ == MeterLabel::kOutline) {
    QPen pen(penColor.darker(150));
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);
    painter.drawRect(trackRect.adjusted(3.0, 3.0, -3.0, -3.0));
    painter.setPen(penColor);
  }

  QFont labelFont = medmSliderLabelFont(label_, direction_, size());
  if (labelFont.family().isEmpty()) {
    labelFont = painter.font();
  }
  painter.setFont(labelFont);

  if (label_ == MeterLabel::kChannel) {
    const QString text = channel_.trimmed();
    if (!text.isEmpty() && channelRect.isValid() && !channelRect.isEmpty()) {
      const QRectF channelBounds = channelRect.adjusted(2.0, 0.0, -2.0, -2.0);
      const Qt::Alignment channelAlignment = isVertical()
          ? (Qt::AlignHCenter | Qt::AlignBottom)
          : (Qt::AlignHCenter | Qt::AlignVCenter);
      painter.save();
      const QFont fitted = shrinkFontToFit(painter.font(), QStringList{text},
          channelBounds.size());
      painter.setFont(fitted);
      painter.drawText(channelBounds, channelAlignment, text);
      painter.restore();
      //drawDebugRect(channelRect);
    }
  }

  if (shouldShowLimitLabels() && limitRect.isValid() && !limitRect.isEmpty()) {
    const double lowValue = effectiveLowLimit();
    const double highValue = effectiveHighLimit();
    QString lowText = formatLimit(lowValue);
    QString highText = formatLimit(highValue);
    if (isDirectionInverted()) {
      std::swap(lowText, highText);
    }
    const bool showValue = executeMode_
        && (label_ == MeterLabel::kChannel || label_ == MeterLabel::kLimits);
    QString valueText;
    if (showValue) {
      if (runtimeConnected_ && (hasRuntimeValue_ || dragging_)) {
        valueText = formatLimit(currentDisplayedValue());
      } else {
        valueText = QStringLiteral("--");
      }
    }
    QRectF bounds = limitRect.adjusted(2.0,
        isVertical() ? 2.0 : -2.0, -2.0, -2.0);
    if (isVertical()) {
      bounds.setRight(std::min(bounds.right(), trackRect.left() - 1.0));
      if (bounds.right() < bounds.left()) {
        bounds.setRight(bounds.left());
      }
    } else {
      const qreal availableShift = std::max<qreal>(0.0,
          limitRect.bottom() - bounds.bottom());
      const qreal shift = std::min<qreal>(2.0, availableShift);
      bounds.translate(0.0, shift);
    }
    //painter.save();
    //QPen highlightPen(Qt::red);
    //highlightPen.setWidth(1);
    //painter.setPen(highlightPen);
    //painter.drawRect(limitRect.adjusted(1.0, 1.0, -1.0, -1.0));
    //painter.restore();
    if (isVertical()) {
      QStringList limitSamples;
      limitSamples << highText << lowText;
      if (showValue) {
        limitSamples << valueText;
      }
      painter.save();
      const QFont fitted = shrinkFontToFit(painter.font(), limitSamples,
          bounds.size());
      painter.setFont(fitted);
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignBottom, lowText);
      if (showValue) {
        painter.save();
        painter.setPen(effectiveForegroundForValueText());
        painter.drawText(bounds, Qt::AlignRight | Qt::AlignVCenter, valueText);
        painter.restore();
      }
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignTop, highText);
      painter.restore();
    } else {
      painter.save();
      const QFont fitted = shrinkFontToFitHorizontal(painter.font(), lowText,
          valueText, highText, bounds.size(), showValue);
      painter.setFont(fitted);

      const QFontMetricsF metrics(painter.font());
      qreal leftWidth = lowText.isEmpty()
          ? 0.0 : metrics.horizontalAdvance(lowText);
      qreal rightWidth = highText.isEmpty()
          ? 0.0 : metrics.horizontalAdvance(highText);
      const qreal availableWidth = bounds.width();
      leftWidth = std::min(leftWidth, availableWidth);
      rightWidth = std::min(rightWidth, availableWidth);

      qreal leftEnd = bounds.left() + leftWidth;
      qreal rightStart = bounds.right() - rightWidth;
      if (rightStart < leftEnd) {
        const qreal midpoint = 0.5 * (leftEnd + rightStart);
        leftEnd = midpoint;
        rightStart = midpoint;
      }

      const QRectF leftBounds(bounds.left(), bounds.top(),
          std::max<qreal>(0.0, leftEnd - bounds.left()), bounds.height());
      if (!lowText.isEmpty()) {
        painter.drawText(leftBounds, Qt::AlignLeft | Qt::AlignVCenter, lowText);
      }

      const qreal rightRectLeft = std::max<qreal>(bounds.left(), rightStart);
      const QRectF rightBounds(rightRectLeft, bounds.top(),
          std::max<qreal>(0.0, bounds.right() - rightRectLeft),
          bounds.height());
      if (!highText.isEmpty()) {
        painter.drawText(rightBounds, Qt::AlignRight | Qt::AlignVCenter,
            highText);
      }

      if (showValue) {
        const qreal spacing = kHorizontalLabelSpacing;
        qreal centerLeft = leftEnd + spacing * 0.5;
        qreal centerRight = rightStart - spacing * 0.5;
        if (centerRight < centerLeft) {
          centerLeft = leftEnd;
          centerRight = rightStart;
        }
        if (centerRight > centerLeft) {
          painter.save();
          painter.setPen(effectiveForegroundForValueText());
          const QRectF centerBounds(centerLeft, bounds.top(),
              centerRight - centerLeft, bounds.height());
          painter.drawText(centerBounds, Qt::AlignHCenter | Qt::AlignVCenter,
              valueText);
          painter.restore();
        }
      }

      painter.restore();
    }
    //drawDebugRect(limitRect);
  }

  painter.restore();
}

bool SliderElement::shouldShowLimitLabels() const
{
  return label_ == MeterLabel::kOutline || label_ == MeterLabel::kLimits
      || label_ == MeterLabel::kChannel;
}

QColor SliderElement::effectiveForeground() const
{
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
    return QColor(204, 204, 204);
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

QColor SliderElement::effectiveForegroundForValueText() const
{
  if (executeMode_) {
    if (!runtimeConnected_ || channel_.trimmed().isEmpty()) {
      return QColor(204, 204, 204);
    }
    if (colorMode_ == TextColorMode::kAlarm) {
      return alarmColorForSeverity(runtimeSeverity_);
    }
  }
  return effectiveForeground();
}

QColor SliderElement::effectiveBackground() const
{
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
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

void SliderElement::paintSelectionOverlay(QPainter &painter) const
{
  painter.save();
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
  painter.restore();
}

bool SliderElement::isVertical() const
{
  return direction_ == BarDirection::kUp || direction_ == BarDirection::kDown;
}

bool SliderElement::isDirectionInverted() const
{
  return direction_ == BarDirection::kLeft || direction_ == BarDirection::kDown;
}

double SliderElement::normalizedValue() const
{
  const double low = effectiveLowLimit();
  const double high = effectiveHighLimit();
  const double value = currentDisplayedValue();
  if (!std::isfinite(low) || !std::isfinite(high) || !std::isfinite(value)) {
    return std::clamp(kSampleValue, 0.0, 1.0);
  }
  const double span = high - low;
  if (std::abs(span) < 1e-12) {
    return 0.0;
  }
  double normalized = (value - low) / span;
  return std::clamp(normalized, 0.0, 1.0);
}

double SliderElement::currentDisplayedValue() const
{
  if (dragging_) {
    return dragValue_;
  }
  if (executeMode_ && hasRuntimeValue_) {
    return runtimeValue_;
  }
  return defaultSampleValue();
}

double SliderElement::effectiveLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double SliderElement::effectiveHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

int SliderElement::effectivePrecision() const
{
  const int defaultPrecision = std::clamp(limits_.precisionDefault, 0, 17);
  if (limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
    return defaultPrecision;
  }
  return defaultPrecision;
}

double SliderElement::clampToLimits(double value) const
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

double SliderElement::valueFromPosition(const QPointF &pos) const
{
  QRectF limitRect;
  QRectF channelRect;
  QRectF trackRect = trackRectForPainting(rect().adjusted(2.0, 2.0, -2.0, -2.0),
      limitRect, channelRect);
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    return currentDisplayedValue();
  }

  double normalized = 0.0;
  if (isVertical()) {
    const double y = std::clamp(pos.y(), trackRect.top(), trackRect.bottom());
    if (isDirectionInverted()) {
      normalized = (y - trackRect.top()) / trackRect.height();
    } else {
      normalized = (trackRect.bottom() - y) / trackRect.height();
    }
  } else {
    const double x = std::clamp(pos.x(), trackRect.left(), trackRect.right());
    if (isDirectionInverted()) {
      normalized = (trackRect.right() - x) / trackRect.width();
    } else {
      normalized = (x - trackRect.left()) / trackRect.width();
    }
  }
  normalized = std::clamp(normalized, 0.0, 1.0);

  const double low = effectiveLowLimit();
  const double high = effectiveHighLimit();
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return normalized;
  }
  const double span = high - low;
  if (!std::isfinite(span) || std::abs(span) < 1e-12) {
    return low;
  }
  return low + normalized * span;
}

QRectF SliderElement::thumbRectForTrack(const QRectF &trackRect) const
{
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    return QRectF();
  }

  QRectF thumbRect = trackRect;
  const qreal bevelSize = 2.0;

  if (isVertical()) {
    const qreal thumbHeight = std::max(trackRect.height() * 0.10, 30.0);
    const qreal center = isDirectionInverted()
        ? trackRect.top() + normalizedValue() * trackRect.height()
        : trackRect.bottom() - normalizedValue() * trackRect.height();
    thumbRect.setTop(center - thumbHeight / 2.0);
    thumbRect.setBottom(center + thumbHeight / 2.0);
    thumbRect.setLeft(trackRect.left() + bevelSize);
    thumbRect.setRight(trackRect.right() - bevelSize);
  } else {
    const qreal thumbWidth = std::max(trackRect.width() * 0.10, 30.0);
    const qreal center = isDirectionInverted()
        ? trackRect.right() - normalizedValue() * trackRect.width()
        : trackRect.left() + normalizedValue() * trackRect.width();
    thumbRect.setLeft(center - thumbWidth / 2.0);
    thumbRect.setRight(center + thumbWidth / 2.0);
    thumbRect.setTop(trackRect.top() + bevelSize);
    thumbRect.setBottom(trackRect.bottom() - bevelSize);
  }

  return thumbRect;
}

void SliderElement::beginDrag(double value, bool sendInitial)
{
  dragging_ = true;
  grabMouse();
  double clamped = clampToLimits(value);
  dragValue_ = clamped;
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  if (sendInitial) {
    hasLastSentValue_ = false;
    sendActivationValue(clamped, true);
  } else {
    lastSentValue_ = clamped;
    hasLastSentValue_ = true;
  }
  update();
}

void SliderElement::updateDrag(double value, bool force)
{
  const double clamped = clampToLimits(value);
  dragValue_ = clamped;
  runtimeValue_ = clamped;
  hasRuntimeValue_ = true;
  sendActivationValue(clamped, force);
  update();
}

void SliderElement::endDrag(double value, bool force)
{
  if (!dragging_) {
    return;
  }
  updateDrag(value, force);
  dragging_ = false;
  releaseMouse();
  updateCursor();
}

void SliderElement::sendActivationValue(double value, bool force)
{
  if (!activationCallback_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }
  const double clamped = clampToLimits(value);
  const double toSend = dragging_ ? clamped : quantizeToIncrement(clamped);
  if (!force && hasLastSentValue_
      && std::abs(toSend - lastSentValue_) <= sliderEpsilon()) {
    return;
  }
  activationCallback_(toSend);
  lastSentValue_ = toSend;
  hasLastSentValue_ = true;
}

void SliderElement::updateCursor()
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

bool SliderElement::isInteractive() const
{
  return executeMode_ && runtimeConnected_ && runtimeWriteAccess_
      && static_cast<bool>(activationCallback_);
}

double SliderElement::sliderEpsilon() const
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

double SliderElement::quantizeToIncrement(double value) const
{
  double increment = increment_;
  if (!std::isfinite(increment) || increment <= 0.0) {
    return value;
  }
  double low = effectiveLowLimit();
  double high = effectiveHighLimit();
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return value;
  }
  if (low > high) {
    std::swap(low, high);
  }
  if (std::abs(high - low) < 1e-12) {
    return low;
  }
  const double steps = std::round((value - low) / increment);
  double quantized = low + steps * increment;
  if (quantized < low) {
    quantized = low;
  } else if (quantized > high) {
    quantized = high;
  }
  return quantized;
}

double SliderElement::defaultSampleValue() const
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
  const double clamped = std::clamp(kSampleValue, 0.0, 1.0);
  return low + span * clamped;
}

QString SliderElement::formatLimit(double value) const
{
  if (!std::isfinite(value)) {
    return QStringLiteral("--");
  }
  const int digits = effectivePrecision();
  return QString::number(value, 'f', digits);
}

double SliderElement::keyboardStep(Qt::KeyboardModifiers modifiers) const
{
  double step = increment_;
  if (!std::isfinite(step) || step <= 0.0) {
    double span = effectiveHighLimit() - effectiveLowLimit();
    if (!std::isfinite(span)) {
      step = 1.0;
    } else {
      span = std::abs(span);
      step = span > 0.0 ? span / 100.0 : 1.0;
      if (!std::isfinite(step) || step <= 0.0) {
        step = 1.0;
      }
    }
  }
  if (modifiers & Qt::ControlModifier) {
    step *= 10.0;
  }
  return step;
}

bool SliderElement::applyKeyboardDelta(double delta)
{
  if (!std::isfinite(delta) || delta == 0.0) {
    return false;
  }

  double baseValue = hasRuntimeValue_ ? runtimeValue_ : currentDisplayedValue();
  if (!std::isfinite(baseValue)) {
    baseValue = defaultSampleValue();
  }

  double candidate = baseValue + delta;
  candidate = quantizeToIncrement(clampToLimits(candidate));

  if (!std::isfinite(candidate)) {
    return false;
  }

  if (hasRuntimeValue_ && std::abs(candidate - runtimeValue_) <= sliderEpsilon()) {
    return false;
  }

  runtimeValue_ = candidate;
  hasRuntimeValue_ = true;
  dragValue_ = candidate;
  sendActivationValue(candidate, false);
  update();
  return true;
}

bool SliderElement::forwardMouseEventToParent(QMouseEvent *event) const
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
