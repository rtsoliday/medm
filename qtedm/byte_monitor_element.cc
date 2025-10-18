#include "byte_monitor_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QtGlobal>

#include "medm_colors.h"

namespace {

constexpr quint32 kSamplePattern = 0x5A5AA5A5u;
constexpr short kInvalidSeverity = 3;
constexpr short kDisconnectedSeverity = kInvalidSeverity + 1;

bool isVertical(BarDirection direction)
{
  return direction == BarDirection::kUp || direction == BarDirection::kDown;
}

} // namespace

ByteMonitorElement::ByteMonitorElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
}

void ByteMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ByteMonitorElement::isSelected() const
{
  return selected_;
}

QColor ByteMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void ByteMonitorElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor ByteMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void ByteMonitorElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode ByteMonitorElement::colorMode() const
{
  return colorMode_;
}

void ByteMonitorElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

BarDirection ByteMonitorElement::direction() const
{
  return direction_;
}

void ByteMonitorElement::setDirection(BarDirection direction)
{
  if (direction_ == direction) {
    return;
  }
  direction_ = direction;
  update();
}

int ByteMonitorElement::startBit() const
{
  return startBit_;
}

void ByteMonitorElement::setStartBit(int bit)
{
  bit = std::clamp(bit, 0, 31);
  if (startBit_ == bit) {
    return;
  }
  startBit_ = bit;
  update();
}

int ByteMonitorElement::endBit() const
{
  return endBit_;
}

void ByteMonitorElement::setEndBit(int bit)
{
  bit = std::clamp(bit, 0, 31);
  if (endBit_ == bit) {
    return;
  }
  endBit_ = bit;
  update();
}

QString ByteMonitorElement::channel() const
{
  return channel_;
}

void ByteMonitorElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  update();
}

void ByteMonitorElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
}

bool ByteMonitorElement::isExecuteMode() const
{
  return executeMode_;
}

void ByteMonitorElement::setRuntimeConnected(bool connected)
{
  if (!executeMode_) {
    return;
  }
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeSeverity_ = kInvalidSeverity;
    hasRuntimeValue_ = false;
    runtimeValue_ = 0u;
  }
  update();
}

void ByteMonitorElement::setRuntimeSeverity(short severity)
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

void ByteMonitorElement::setRuntimeValue(quint32 value)
{
  if (!executeMode_) {
    return;
  }
  if (hasRuntimeValue_ && runtimeValue_ == value) {
    return;
  }
  runtimeValue_ = value;
  hasRuntimeValue_ = true;
  update();
}

void ByteMonitorElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  runtimeSeverity_ = kInvalidSeverity;
  hasRuntimeValue_ = false;
  runtimeValue_ = 0u;
  update();
}

void ByteMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QRect outerRect = rect().adjusted(0, 0, -1, -1);
  const QColor background = effectiveBackground();
  painter.fillRect(rect(), background);

  const int segmentCount = std::max(1, std::abs(endBit_ - startBit_) + 1);
  const QRect contentRect = rect().adjusted(1, 1, -1, -1);

  if (contentRect.width() <= 0 || contentRect.height() <= 0
      || segmentCount <= 0) {
    const QColor fg = effectiveForeground();
    painter.setPen(QPen(fg, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(outerRect);
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  const bool vertical = isVertical(direction_);
  const bool increasing = endBit_ > startBit_;
  const bool drawRuntimeBits = !executeMode_ || runtimeConnected_;
  const quint32 displayValue = currentValue();
  const QColor foreground = effectiveForeground();
  const QColor dividerColor = foreground.darker(160);

  painter.save();
  painter.setPen(Qt::NoPen);

  if (vertical) {
    const double delta = segmentCount > 0
        ? static_cast<double>(contentRect.height()) / segmentCount
        : 0.0;
    int offset = 0;
    for (int i = 0; i < segmentCount; ++i) {
      int nextOffset = static_cast<int>(std::round((i + 1) * delta));
      if (i == segmentCount - 1) {
        nextOffset = contentRect.height();
      }
      nextOffset = std::clamp(nextOffset, offset, contentRect.height());
      const QRect segment(contentRect.left(), contentRect.top() + offset,
          contentRect.width(), nextOffset - offset);
      if (segment.height() > 1 && segment.width() > 1) {
        const QRect fillRect = segment.adjusted(1, 1, -1, -1);
        const int bitIndex = increasing ? (startBit_ + i) : (startBit_ - i);
        const bool bitSet = drawRuntimeBits && bitIndex >= 0 && bitIndex < 32
            && ((displayValue >> bitIndex) & 0x1u);
        if (bitSet) {
          painter.fillRect(fillRect, foreground);
        }
      }
      painter.setPen(QPen(dividerColor, 1));
      if (i < segmentCount - 1 && segment.height() > 0) {
        const int y = contentRect.top() + nextOffset;
        painter.drawLine(contentRect.left(), y,
            contentRect.right(), y);
      }
      painter.setPen(Qt::NoPen);
      offset = nextOffset;
    }
  } else {
    const double delta = segmentCount > 0
        ? static_cast<double>(contentRect.width()) / segmentCount
        : 0.0;
    int offset = 0;
    for (int i = 0; i < segmentCount; ++i) {
      int nextOffset = static_cast<int>(std::round((i + 1) * delta));
      if (i == segmentCount - 1) {
        nextOffset = contentRect.width();
      }
      nextOffset = std::clamp(nextOffset, offset, contentRect.width());
      const QRect segment(contentRect.left() + offset, contentRect.top(),
          nextOffset - offset, contentRect.height());
      if (segment.width() > 1 && segment.height() > 1) {
        const QRect fillRect = segment.adjusted(1, 1, -1, -1);
        const int bitIndex = increasing ? (startBit_ + i) : (startBit_ - i);
        const bool bitSet = drawRuntimeBits && bitIndex >= 0 && bitIndex < 32
            && ((displayValue >> bitIndex) & 0x1u);
        if (bitSet) {
          painter.fillRect(fillRect, foreground);
        }
      }
      painter.setPen(QPen(dividerColor, 1));
      if (i < segmentCount - 1 && segment.width() > 0) {
        const int x = contentRect.left() + nextOffset;
        painter.drawLine(x, contentRect.top(), x, contentRect.bottom());
      }
      painter.setPen(Qt::NoPen);
      offset = nextOffset;
    }
  }

  painter.restore();

  painter.setPen(QPen(dividerColor, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(outerRect);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

void ByteMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  painter.save();
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
  painter.restore();
}

QColor ByteMonitorElement::effectiveForeground() const
{
  if (!executeMode_) {
    if (foregroundColor_.isValid()) {
      return foregroundColor_;
    }
    return defaultForeground();
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    if (!runtimeConnected_) {
      return MedmColors::alarmColorForSeverity(kDisconnectedSeverity);
    }
    return MedmColors::alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    if (foregroundColor_.isValid()) {
      return foregroundColor_;
    }
    return defaultForeground();
  }
}

QColor ByteMonitorElement::effectiveBackground() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackground();
}

QColor ByteMonitorElement::defaultForeground() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor ByteMonitorElement::defaultBackground() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

quint32 ByteMonitorElement::currentValue() const
{
  if (!executeMode_) {
    return kSamplePattern;
  }
  if (hasRuntimeValue_) {
    return runtimeValue_;
  }
  return 0u;
}

