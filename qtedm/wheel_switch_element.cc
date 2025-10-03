#include "wheel_switch_element.h"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>

namespace {

constexpr double kMinimumCenterHeight = 24.0;
constexpr double kMinimumButtonHeight = 14.0;

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
  limits_.lowSource = PvLimitSource::kDefault;
  limits_.highSource = PvLimitSource::kDefault;
  limits_.precisionSource = PvLimitSource::kDefault;
  limits_.lowDefault = 0.0;
  limits_.highDefault = 100.0;
  limits_.precisionDefault = 1;
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
  update();
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

  qreal buttonHeight = std::max(kMinimumButtonHeight, outer.height() * 0.22);
  const qreal maxButtonHeight = std::max(kMinimumButtonHeight,
      (outer.height() - kMinimumCenterHeight) / 2.0);
  if (buttonHeight > maxButtonHeight) {
    buttonHeight = maxButtonHeight;
  }
  if (outer.height() - 2.0 * buttonHeight < kMinimumCenterHeight) {
    buttonHeight = std::max(kMinimumButtonHeight,
        (outer.height() - kMinimumCenterHeight) / 2.0);
  }
  buttonHeight = std::clamp(buttonHeight, kMinimumButtonHeight, outer.height() / 2.0);

  QRectF topRect(outer.left() + 1.0, outer.top() + 1.0, outer.width() - 2.0,
      std::max(0.0, buttonHeight - 2.0));
  QRectF bottomRect(outer.left() + 1.0, outer.bottom() - buttonHeight + 1.0,
      outer.width() - 2.0, std::max(0.0, buttonHeight - 2.0));

  paintButton(painter, topRect, true);
  paintButton(painter, bottomRect, false);

  QRectF valueRect(outer.left() + 4.0, topRect.bottom() + 4.0,
      outer.width() - 8.0, bottomRect.top() - topRect.bottom() - 8.0);
  if (valueRect.height() > 6.0 && valueRect.width() > 6.0) {
    paintValueDisplay(painter, valueRect);
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
  return palette().color(QPalette::WindowText);
}

QColor WheelSwitchElement::effectiveBackground() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return palette().color(QPalette::Window);
}

void WheelSwitchElement::paintButton(QPainter &painter, const QRectF &rect,
    bool isUp) const
{
  if (!rect.isValid() || rect.width() < 4.0 || rect.height() < 4.0) {
    return;
  }

  painter.save();
  QColor base = effectiveBackground();
  QColor fill = isUp ? blendedColor(base, 120) : blendedColor(base, 90);
  if (!fill.isValid()) {
    fill = isUp ? QColor(220, 220, 220) : QColor(200, 200, 200);
  }
  painter.setPen(Qt::NoPen);
  painter.setBrush(fill);
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
  painter.setBrush(effectiveForeground());
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
  painter.setPen(effectiveForeground());
  QFont valueFont = font();
  if (rect.height() > 0.0) {
    int pixelSize = static_cast<int>(rect.height() * 0.6);
    pixelSize = std::clamp(pixelSize, 8, static_cast<int>(rect.height() - 4.0));
    valueFont.setPixelSize(pixelSize);
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
  const double value = sampleValue();
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

  int digits = std::clamp(static_cast<int>(std::round(precision_)), 0, 17);
  return QString::number(value, 'f', digits);
}

double WheelSwitchElement::sampleValue() const
{
  double low = (limits_.lowSource == PvLimitSource::kChannel) ? 0.0
                                                              : limits_.lowDefault;
  double high = (limits_.highSource == PvLimitSource::kChannel) ? 100.0
                                                                : limits_.highDefault;
  if (high < low) {
    std::swap(low, high);
  }
  return (low + high) * 0.5;
}
