#include "slider_element.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QMouseEvent>
#include <QCursor>
#include <QPointF>
#include <QFontMetricsF>
#include <QStringList>

#include "cursor_utils.h"
#include "text_font_utils.h"

namespace {

constexpr double kSampleValue = 0.6;
constexpr int kTickCount = 11;
constexpr short kInvalidSeverity = 3;
constexpr double kValueEpsilonFactor = 1e-6;

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
  limits_.precisionDefault = 1;
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

double SliderElement::precision() const
{
  return precision_;
}

void SliderElement::setPrecision(double precision)
{
  if (std::abs(precision_ - precision) < 1e-9) {
    return;
  }
  precision_ = precision;
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
  if (!isInteractive() || event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
#else
  const QPointF pos = event->localPos();
#endif
  beginDrag(valueFromPosition(pos));
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
  endDrag(valueFromPosition(pos), true);
  event->accept();
}

void SliderElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.fillRect(rect(), effectiveBackground());

  QRectF limitRect;
  QRectF channelRect;
  QRectF trackRect = trackRectForPainting(rect().adjusted(2.0, 2.0, -2.0, -2.0),
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
      const qreal maxLabelHeight = std::min<qreal>(24.0,
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
      const qreal maxLabelHeight = std::min<qreal>(24.0,
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
    const qreal trackWidth = std::max<qreal>(8.0,
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
    const qreal thumbHeight = std::max(workingRect.height() * 0.10, 8.0);
    const qreal reducedHeight = std::max(0.0, workingRect.height() - thumbHeight);
  return QRectF(trackLeft,
    workingRect.top() + thumbHeight / 2.0, clampedTrackWidth, reducedHeight);
  }

  const qreal trackHeight = std::max<qreal>(8.0,
      contentRect.height() / heightDivisor);
  /* Ensure track doesn't extend beyond workingRect to avoid overlapping labels */
  const qreal clampedTrackHeight = std::min(trackHeight, workingRect.height());
  const qreal centerY = workingRect.center().y();
  /* Reduce track width to prevent thumb from extending beyond edges */
  const qreal thumbWidth = std::max(workingRect.width() * 0.10, 8.0);
  const qreal reducedWidth = std::max(0.0, workingRect.width() - thumbWidth);
  return QRectF(workingRect.left() + thumbWidth / 2.0,
      centerY - clampedTrackHeight / 2.0, reducedWidth, clampedTrackHeight);
}

void SliderElement::paintTrack(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  painter.setPen(Qt::NoPen);
  
  const QColor baseColor = effectiveBackground();
  
  /* Draw main track background */
  painter.setBrush(baseColor.darker(120));
  painter.drawRoundedRect(trackRect, 3.0, 3.0);
  
  /* Draw lowered bevel (2 pixels) */
  /* Dark shadow on top/left */
  QPen bevelPen(baseColor.darker(150), 2.0);
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
  bevelPen.setColor(baseColor.lighter(130));
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

  painter.save();
  QPen debugPen(Qt::red);
  debugPen.setWidthF(1.0);
  painter.setPen(debugPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(trackRect.adjusted(0.5, 0.5, -0.5, -0.5));
  painter.restore();
}

void SliderElement::paintThumb(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  
  const QColor thumbColor = effectiveForeground();
  const QColor bgColor = effectiveBackground();
  
  /* Calculate thumb position, ensuring it stays within track bounds (minus bevel) */
  QRectF thumbRect = trackRect;
  const qreal bevelSize = 2.0;
  
  if (isVertical()) {
    /* Reduce thumb height to 10% of track (was 12%), min 8 pixels */
    const qreal thumbHeight = std::max(trackRect.height() * 0.10, 8.0);
    const qreal center = isDirectionInverted()
        ? trackRect.top() + normalizedValue() * trackRect.height()
        : trackRect.bottom() - normalizedValue() * trackRect.height();
    thumbRect.setTop(center - thumbHeight / 2.0);
    thumbRect.setBottom(center + thumbHeight / 2.0);
    /* Inset by bevel size so thumb doesn't extend over track bevel */
    thumbRect.setLeft(trackRect.left() + bevelSize);
    thumbRect.setRight(trackRect.right() - bevelSize);
  } else {
    /* Reduce thumb width to 10% of track (was 12%), min 8 pixels */
    const qreal thumbWidth = std::max(trackRect.width() * 0.10, 8.0);
    const qreal center = isDirectionInverted()
        ? trackRect.right() - normalizedValue() * trackRect.width()
        : trackRect.left() + normalizedValue() * trackRect.width();
    thumbRect.setLeft(center - thumbWidth / 2.0);
    thumbRect.setRight(center + thumbWidth / 2.0);
    /* Inset by bevel size so thumb doesn't extend over track bevel */
    thumbRect.setTop(trackRect.top() + bevelSize);
    thumbRect.setBottom(trackRect.bottom() - bevelSize);
  }
  
  /* Draw main thumb body */
  painter.setPen(Qt::NoPen);
  painter.setBrush(thumbColor);
  painter.drawRoundedRect(thumbRect, 2.0, 2.0);
  
  /* Draw raised bevel (2 pixels) */
  QPen bevelPen(thumbColor.lighter(140), 2.0);
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
  bevelPen.setColor(thumbColor.darker(160));
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
  
  /* Draw center line in background color (1 pixel) */
  QPen centerPen(bgColor, 1.0);
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

  const auto drawDebugRect = [&painter](const QRectF &rect) {
    if (!rect.isValid() || rect.isEmpty()) {
      return;
    }
    painter.save();
    QPen pen(Qt::red);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.restore();
  };

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
      if (isVertical()) {
        const QFont fitted = shrinkFontToFit(painter.font(), QStringList{text},
            channelBounds.size());
        painter.setFont(fitted);
      }
      painter.drawText(channelBounds, channelAlignment, text);
      painter.restore();
      drawDebugRect(channelRect);
    }
  }

  if (shouldShowLimitLabels() && limitRect.isValid() && !limitRect.isEmpty()) {
    const double lowValue = effectiveLowLimit();
    const double highValue = effectiveHighLimit();
    const QString lowText = formatLimit(lowValue);
    const QString highText = formatLimit(highValue);
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
      painter.drawText(bounds, Qt::AlignLeft | Qt::AlignVCenter, lowText);
      if (showValue) {
        painter.save();
        painter.setPen(effectiveForegroundForValueText());
        painter.drawText(bounds, Qt::AlignHCenter | Qt::AlignVCenter, valueText);
        painter.restore();
      }
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignVCenter, highText);
    }
    drawDebugRect(limitRect);
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
    if (colorMode_ == TextColorMode::kAlarm) {
      if (!runtimeConnected_) {
        return QColor(204, 204, 204);
      }
      return alarmColorForSeverity(runtimeSeverity_);
    }
  }
  return effectiveForeground();
}

QColor SliderElement::effectiveBackground() const
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
  if (limits_.precisionSource == PvLimitSource::kChannel) {
    if (runtimePrecision_ >= 0) {
      return std::clamp(runtimePrecision_, 0, 17);
    }
    return std::clamp(limits_.precisionDefault, 0, 17);
  }
  return std::clamp(static_cast<int>(std::round(precision_)), 0, 17);
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

void SliderElement::beginDrag(double value)
{
  dragging_ = true;
  grabMouse();
  hasLastSentValue_ = false;
  updateDrag(value, true);
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
  if (!force && hasLastSentValue_
      && std::abs(value - lastSentValue_) <= sliderEpsilon()) {
    return;
  }
  activationCallback_(value);
  lastSentValue_ = value;
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
