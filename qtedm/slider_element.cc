#include "slider_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>

namespace {

constexpr double kSampleValue = 0.6;
constexpr int kTickCount = 11;
constexpr double kTrackThicknessRatio = 0.25;

} // namespace

SliderElement::SliderElement(QWidget *parent)
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
  update();
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
  paintTicks(painter, trackRect);
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

  if (vertical) {
    if (showChannel) {
      const qreal maxLabelHeight = std::min<qreal>(24.0,
          workingRect.height() * 0.35);
      if (maxLabelHeight > 6.0) {
        channelRect = QRectF(workingRect.left(), workingRect.top(),
            workingRect.width(), maxLabelHeight);
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
      }
    }
  } else {
    if (showChannel) {
      const qreal maxLabelHeight = std::min<qreal>(24.0,
          workingRect.height() * 0.35);
      if (maxLabelHeight > 6.0) {
        channelRect = QRectF(workingRect.left(), workingRect.top(),
            workingRect.width(), maxLabelHeight);
        workingRect.setTop(channelRect.bottom() + 4.0);
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
      }
    }
  }

  workingRect = workingRect.adjusted(4.0, 4.0, -4.0, -4.0);
  if (workingRect.width() < 2.0 || workingRect.height() < 2.0) {
    return QRectF();
  }

  if (vertical) {
    const qreal trackWidth = std::max<qreal>(8.0,
        workingRect.width() * kTrackThicknessRatio);
    const qreal centerX = workingRect.center().x();
    return QRectF(centerX - trackWidth / 2.0, workingRect.top(), trackWidth,
        workingRect.height());
  }

  const qreal trackHeight = std::max<qreal>(8.0,
      workingRect.height() * kTrackThicknessRatio);
  const qreal centerY = workingRect.center().y();
  return QRectF(workingRect.left(), centerY - trackHeight / 2.0,
      workingRect.width(), trackHeight);
}

void SliderElement::paintTrack(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveBackground().darker(110));
  painter.drawRoundedRect(trackRect.adjusted(-1.0, -1.0, 1.0, 1.0), 4.0, 4.0);
  painter.setBrush(effectiveBackground().lighter(110));
  painter.drawRoundedRect(trackRect, 3.0, 3.0);
  painter.restore();
}

void SliderElement::paintThumb(QPainter &painter, const QRectF &trackRect) const
{
  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveForeground());

  QRectF thumbRect = trackRect;
  if (isVertical()) {
    const qreal thumbHeight = std::max(trackRect.height() * 0.12, 10.0);
    const qreal center = isDirectionInverted()
        ? trackRect.top() + normalizedSampleValue() * trackRect.height()
        : trackRect.bottom() - normalizedSampleValue() * trackRect.height();
    thumbRect.setTop(center - thumbHeight / 2.0);
    thumbRect.setBottom(center + thumbHeight / 2.0);
  } else {
    const qreal thumbWidth = std::max(trackRect.width() * 0.12, 10.0);
    const qreal center = isDirectionInverted()
        ? trackRect.right() - normalizedSampleValue() * trackRect.width()
        : trackRect.left() + normalizedSampleValue() * trackRect.width();
    thumbRect.setLeft(center - thumbWidth / 2.0);
    thumbRect.setRight(center + thumbWidth / 2.0);
  }
  thumbRect = thumbRect.adjusted(-2.0, -2.0, 2.0, 2.0);
  painter.drawRoundedRect(thumbRect, 4.0, 4.0);
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

  QFont labelFont = painter.font();
  labelFont.setPointSizeF(std::max(8.0, std::min(trackRect.width(),
      trackRect.height()) / 6.0));
  painter.setFont(labelFont);

  if (label_ == MeterLabel::kChannel) {
    const QString text = channel_.trimmed();
    if (!text.isEmpty() && channelRect.isValid() && !channelRect.isEmpty()) {
      painter.drawText(channelRect.adjusted(2.0, 0.0, -2.0, -2.0),
          Qt::AlignHCenter | Qt::AlignBottom, text);
    }
  }

  if (shouldShowLimitLabels() && limitRect.isValid() && !limitRect.isEmpty()) {
    const QString lowText = formatLimit(limits_.lowDefault);
    const QString highText = formatLimit(limits_.highDefault);
    const QRectF bounds = limitRect.adjusted(2.0, 2.0, -2.0, -2.0);
    if (isVertical()) {
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignBottom, lowText);
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignTop, highText);
    } else {
      painter.drawText(bounds, Qt::AlignLeft | Qt::AlignBottom, lowText);
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignBottom, highText);
    }
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

QColor SliderElement::effectiveBackground() const
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

double SliderElement::normalizedSampleValue() const
{
  return kSampleValue;
}

QString SliderElement::formatLimit(double value) const
{
  const int digits = std::clamp(static_cast<int>(std::round(precision_)), 0, 17);
  return QString::number(value, 'f', digits);
}
