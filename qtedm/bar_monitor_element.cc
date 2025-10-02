#include "bar_monitor_element.h"

#include <algorithm>

#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

namespace {

constexpr double kSampleFillFraction = 0.65;

} // namespace

BarMonitorElement::BarMonitorElement(QWidget *parent)
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

void BarMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool BarMonitorElement::isSelected() const
{
  return selected_;
}

QColor BarMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void BarMonitorElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor BarMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void BarMonitorElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode BarMonitorElement::colorMode() const
{
  return colorMode_;
}

void BarMonitorElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

MeterLabel BarMonitorElement::label() const
{
  return label_;
}

void BarMonitorElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

BarDirection BarMonitorElement::direction() const
{
  return direction_;
}

void BarMonitorElement::setDirection(BarDirection direction)
{
  if (direction_ == direction) {
    return;
  }
  direction_ = direction;
  update();
}

BarFill BarMonitorElement::fillMode() const
{
  return fillMode_;
}

void BarMonitorElement::setFillMode(BarFill mode)
{
  if (fillMode_ == mode) {
    return;
  }
  fillMode_ = mode;
  update();
}

const PvLimits &BarMonitorElement::limits() const
{
  return limits_;
}

void BarMonitorElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  update();
}

QString BarMonitorElement::channel() const
{
  return channel_;
}

void BarMonitorElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  update();
}

void BarMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  painter.fillRect(rect(), effectiveBackground());

  QRectF labelRect;
  QRectF trackRect = trackRectForPainting(rect().adjusted(2.0, 2.0, -2.0, -2.0),
      labelRect);
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  paintTrack(painter, trackRect);
  paintFill(painter, trackRect);
  paintLabels(painter, trackRect, labelRect);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QRectF BarMonitorElement::trackRectForPainting(
    QRectF contentRect, QRectF &labelRect) const
{
  labelRect = QRectF();
  if (label_ == MeterLabel::kChannel || label_ == MeterLabel::kLimits) {
    const qreal maxLabelHeight = std::min<qreal>(24.0, contentRect.height() * 0.35);
    if (maxLabelHeight > 6.0) {
      labelRect = QRectF(contentRect.left(),
          contentRect.bottom() - maxLabelHeight, contentRect.width(),
          maxLabelHeight);
      contentRect.setBottom(labelRect.top() - 4.0);
    }
  }
  contentRect = contentRect.adjusted(4.0, 4.0, -4.0, -4.0);
  if (contentRect.width() < 1.0 || contentRect.height() < 1.0) {
    return QRectF();
  }
  return contentRect;
}

void BarMonitorElement::paintTrack(QPainter &painter,
    const QRectF &trackRect) const
{
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(barTrackColor());
  painter.drawRect(trackRect);

  if (label_ != MeterLabel::kNoDecorations) {
    QPen framePen(effectiveForeground().darker(160));
    framePen.setWidth(1);
    painter.setPen(framePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(trackRect.adjusted(0.5, 0.5, -0.5, -0.5));
  }
  painter.restore();
}

void BarMonitorElement::paintFill(QPainter &painter,
    const QRectF &trackRect) const
{
  if (!trackRect.isValid() || trackRect.isEmpty()) {
    return;
  }

  QRectF fillRect = trackRect;
  if (direction_ == BarDirection::kUp || direction_ == BarDirection::kDown) {
    const double totalHeight = trackRect.height();
    if (totalHeight <= 0.0) {
      return;
    }
    const double fillHeight = std::clamp(totalHeight * kSampleFillFraction, 0.0,
        totalHeight);
    if (fillMode_ == BarFill::kFromCenter) {
      const double half = fillHeight / 2.0;
      const double center = trackRect.center().y();
      fillRect.setTop(center - half);
      fillRect.setBottom(center + half);
    } else if (direction_ == BarDirection::kUp) {
      fillRect.setTop(trackRect.bottom() - fillHeight);
    } else { // Down
      fillRect.setBottom(trackRect.top() + fillHeight);
    }
  } else {
    const double totalWidth = trackRect.width();
    if (totalWidth <= 0.0) {
      return;
    }
    const double fillWidth = std::clamp(totalWidth * kSampleFillFraction, 0.0,
        totalWidth);
    if (fillMode_ == BarFill::kFromCenter) {
      const double half = fillWidth / 2.0;
      const double center = trackRect.center().x();
      fillRect.setLeft(center - half);
      fillRect.setRight(center + half);
    } else if (direction_ == BarDirection::kLeft) {
      fillRect.setLeft(trackRect.right() - fillWidth);
    } else { // Right
      fillRect.setRight(trackRect.left() + fillWidth);
    }
  }

  fillRect = fillRect.intersected(trackRect);
  if (!fillRect.isValid() || fillRect.isEmpty()) {
    return;
  }

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(barFillColor());
  painter.drawRect(fillRect);

  QPen edgePen(barFillColor().darker(160));
  edgePen.setWidth(1);
  painter.setPen(edgePen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(fillRect.adjusted(0.5, 0.5, -0.5, -0.5));
  painter.restore();
}

void BarMonitorElement::paintLabels(QPainter &painter,
    const QRectF &trackRect, const QRectF &labelRect) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  painter.save();
  painter.setPen(effectiveForeground());
  painter.setBrush(Qt::NoBrush);

  if (label_ == MeterLabel::kOutline) {
    QPen pen(effectiveForeground().darker(150));
    pen.setStyle(Qt::DotLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(trackRect.adjusted(3.0, 3.0, -3.0, -3.0));
    painter.restore();
    return;
  }

  QFont labelFont = painter.font();
  labelFont.setPointSizeF(std::max(8.0, std::min(trackRect.height(),
      trackRect.width()) / 6.0));
  painter.setFont(labelFont);

  if (label_ == MeterLabel::kChannel) {
    const QString text = channel_.trimmed();
    if (!text.isEmpty() && labelRect.isValid() && !labelRect.isEmpty()) {
      painter.drawText(labelRect.adjusted(2.0, 0.0, -2.0, -2.0),
          Qt::AlignHCenter | Qt::AlignBottom, text);
    }
    painter.restore();
    return;
  }

  if (label_ == MeterLabel::kLimits) {
    if (labelRect.isValid() && !labelRect.isEmpty()) {
      const QString lowText = QString::number(limits_.lowDefault, 'g', 5);
      const QString highText = QString::number(limits_.highDefault, 'g', 5);
      const QRectF bounds = labelRect.adjusted(2.0, 0.0, -2.0, -2.0);
      painter.drawText(bounds, Qt::AlignLeft | Qt::AlignBottom, lowText);
      painter.drawText(bounds, Qt::AlignRight | Qt::AlignBottom, highText);
    }
    painter.restore();
    return;
  }

  painter.restore();
}

QColor BarMonitorElement::effectiveForeground() const
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

QColor BarMonitorElement::effectiveBackground() const
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

QColor BarMonitorElement::barTrackColor() const
{
  QColor color = effectiveBackground();
  if (!color.isValid()) {
    color = QColor(Qt::white);
  }
  return color.darker(110);
}

QColor BarMonitorElement::barFillColor() const
{
  QColor color = effectiveForeground();
  if (!color.isValid()) {
    color = QColor(Qt::black);
  }
  return color;
}

void BarMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}
