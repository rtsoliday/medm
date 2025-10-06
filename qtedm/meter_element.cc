#include "meter_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>

namespace {

constexpr double kStartAngleDegrees = 225.0;
constexpr double kSpanAngleDegrees = 270.0;
constexpr int kTickCount = 10;
constexpr double kInnerTickRatio = 0.78;
constexpr double kNeedleRatio = 0.72;

inline double degreesToRadians(double degrees)
{
  return degrees * M_PI / 180.0;
}

} // namespace

MeterElement::MeterElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
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
  update();
}

void MeterElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.fillRect(rect(), effectiveBackground());

  const QRectF bounds = rect().adjusted(4.0, 4.0, -4.0, -4.0);
  const double diameter = std::max(0.0, std::min(bounds.width(), bounds.height()));
  QRectF dialRect(bounds.center().x() - diameter / 2.0,
      bounds.center().y() - diameter / 2.0, diameter, diameter);
  dialRect.adjust(2.0, 2.0, -2.0, -2.0);

  paintDial(painter, dialRect);
  paintTicks(painter, dialRect);
  paintNeedle(painter, dialRect);
  paintLabels(painter, dialRect);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor MeterElement::effectiveForeground() const
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

QColor MeterElement::effectiveBackground() const
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
  QColor rimColor = effectiveForeground().darker(140);
  QColor faceColor = effectiveBackground().lighter(110);

  QPen rimPen(rimColor);
  rimPen.setWidth(2);
  painter.setPen(rimPen);
  painter.setBrush(faceColor);
  painter.drawEllipse(dialRect);

  QRectF inner = dialRect.adjusted(dialRect.width() * 0.12, dialRect.height() * 0.12,
      -dialRect.width() * 0.12, -dialRect.height() * 0.12);
  QPen innerPen(faceColor.darker(125));
  innerPen.setWidth(1);
  painter.setPen(innerPen);
  painter.setBrush(faceColor.lighter(105));
  painter.drawEllipse(inner);
}

void MeterElement::paintTicks(QPainter &painter, const QRectF &dialRect) const
{
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
  const QPointF center = dialRect.center();
  const double radius = dialRect.width() / 2.0;

  const double normalizedValue = 0.65;
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
  painter.drawEllipse(center, radius * 0.07, radius * 0.07);
}

void MeterElement::paintLabels(QPainter &painter, const QRectF &dialRect) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  painter.setPen(effectiveForeground());
  QFont labelFont = painter.font();
  labelFont.setPointSizeF(std::max(8.0, dialRect.height() / 7.0));
  painter.setFont(labelFont);
  if (label_ == MeterLabel::kChannel) {
    const QString text = channel_.trimmed();
    if (!text.isEmpty()) {
      painter.drawText(rect().adjusted(6, 0, -6, -6),
          Qt::AlignHCenter | Qt::AlignBottom, text);
    }
    return;
  }

  if (label_ == MeterLabel::kOutline) {
    QPen outlinePen(effectiveForeground().darker(150));
    outlinePen.setStyle(Qt::DotLine);
    outlinePen.setWidth(1);
    painter.setPen(outlinePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(dialRect.adjusted(6.0, 6.0, -6.0, -6.0));
    return;
  }

  if (label_ == MeterLabel::kLimits) {
    const QString lowText = QString::number(limits_.lowDefault, 'g', 5);
    const QString highText = QString::number(limits_.highDefault, 'g', 5);

    const QRectF bounds = rect().adjusted(6, 0, -6, -6);
    painter.drawText(bounds, Qt::AlignLeft | Qt::AlignBottom, lowText);
    painter.drawText(bounds, Qt::AlignRight | Qt::AlignBottom, highText);
  }
}
