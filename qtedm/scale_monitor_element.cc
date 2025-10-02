#include "scale_monitor_element.h"

#include <algorithm>

#include <QApplication>
#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPolygonF>

namespace {

constexpr int kTickCount = 10;
constexpr double kPointerSampleValue = 0.65;

qreal clampSize(qreal value)
{
  return std::max<qreal>(value, 0.0);
}

} // namespace

ScaleMonitorElement::ScaleMonitorElement(QWidget *parent)
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

void ScaleMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ScaleMonitorElement::isSelected() const
{
  return selected_;
}

QColor ScaleMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void ScaleMonitorElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor ScaleMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void ScaleMonitorElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

TextColorMode ScaleMonitorElement::colorMode() const
{
  return colorMode_;
}

void ScaleMonitorElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  update();
}

MeterLabel ScaleMonitorElement::label() const
{
  return label_;
}

void ScaleMonitorElement::setLabel(MeterLabel label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

BarDirection ScaleMonitorElement::direction() const
{
  return direction_;
}

void ScaleMonitorElement::setDirection(BarDirection direction)
{
  if (direction_ == direction) {
    return;
  }
  direction_ = direction;
  update();
}

const PvLimits &ScaleMonitorElement::limits() const
{
  return limits_;
}

void ScaleMonitorElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  update();
}

QString ScaleMonitorElement::channel() const
{
  return channel_;
}

void ScaleMonitorElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  update();
}

void ScaleMonitorElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  painter.fillRect(rect(), effectiveBackground());

  QRectF labelRect;
  QRectF scaleRect = scaleRectForPainting(
      rect().adjusted(2.0, 2.0, -2.0, -2.0), labelRect);
  if (!scaleRect.isValid() || scaleRect.isEmpty()) {
    if (selected_) {
      paintSelectionOverlay(painter);
    }
    return;
  }

  paintScale(painter, scaleRect);
  paintTicks(painter, scaleRect);
  paintPointer(painter, scaleRect);
  paintLabels(painter, scaleRect, labelRect);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QRectF ScaleMonitorElement::scaleRectForPainting(
    QRectF contentRect, QRectF &labelRect) const
{
  labelRect = QRectF();
  const bool vertical = isVertical();

  if (label_ == MeterLabel::kChannel) {
    const qreal labelExtent = std::min<qreal>(28.0,
        vertical ? contentRect.height() * 0.35 : contentRect.height() * 0.4);
    if (labelExtent > 6.0) {
      labelRect = QRectF(contentRect.left(),
          contentRect.bottom() - labelExtent, contentRect.width(), labelExtent);
      contentRect.setBottom(labelRect.top() - 4.0);
    }
  } else if (label_ == MeterLabel::kLimits) {
    if (vertical) {
      contentRect.adjust(0.0, 14.0, 0.0, -14.0);
    } else {
      contentRect.adjust(14.0, 0.0, -14.0, 0.0);
    }
  }

  if (vertical) {
    contentRect = contentRect.adjusted(10.0, 6.0, -10.0, -6.0);
  } else {
    contentRect = contentRect.adjusted(6.0, 10.0, -6.0, -10.0);
  }

  if (contentRect.width() < 4.0 || contentRect.height() < 4.0) {
    return QRectF();
  }

  return contentRect;
}

void ScaleMonitorElement::paintScale(
    QPainter &painter, const QRectF &scaleRect) const
{
  QColor frameColor = effectiveForeground().darker(140);
  QColor fillColor = effectiveBackground().lighter(108);

  QPen framePen(frameColor);
  framePen.setWidth(1);
  painter.setPen(framePen);
  painter.setBrush(fillColor);
  painter.drawRect(scaleRect);
}

void ScaleMonitorElement::paintTicks(
    QPainter &painter, const QRectF &scaleRect) const
{
  const QColor tickColor = effectiveForeground().darker(150);
  QPen tickPen(tickColor);
  tickPen.setWidth(1);
  painter.setPen(tickPen);

  const bool vertical = isVertical();
  const qreal majorLength = vertical ? scaleRect.width() * 0.45
                                     : scaleRect.height() * 0.45;
  const qreal tickLength = std::min<qreal>(majorLength, 10.0);

  for (int i = 0; i <= kTickCount; ++i) {
    const double ratio = static_cast<double>(i) / kTickCount;
    if (vertical) {
      const qreal y = scaleRect.bottom() - ratio * scaleRect.height();
      painter.drawLine(QPointF(scaleRect.left(), y),
          QPointF(scaleRect.left() + tickLength, y));
      painter.drawLine(QPointF(scaleRect.right(), y),
          QPointF(scaleRect.right() - tickLength, y));
    } else {
      const qreal x = scaleRect.left() + ratio * scaleRect.width();
      painter.drawLine(QPointF(x, scaleRect.top()),
          QPointF(x, scaleRect.top() + tickLength));
      painter.drawLine(QPointF(x, scaleRect.bottom()),
          QPointF(x, scaleRect.bottom() - tickLength));
    }
  }
}

void ScaleMonitorElement::paintPointer(
    QPainter &painter, const QRectF &scaleRect) const
{
  const bool vertical = isVertical();
  double ratio = normalizedSampleValue();
  if (isDirectionInverted()) {
    ratio = 1.0 - ratio;
  }
  ratio = std::clamp(ratio, 0.0, 1.0);

  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveForeground());

  if (vertical) {
    const qreal y = scaleRect.bottom() - ratio * scaleRect.height();
    const qreal arrowWidth = std::min<qreal>(scaleRect.width() * 0.8, 14.0);
    const qreal arrowHeight = std::min<qreal>(scaleRect.height() * 0.16, 16.0);

    QPen linePen(effectiveForeground());
    linePen.setWidth(2);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(scaleRect.left(), y),
        QPointF(scaleRect.right(), y));

    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveForeground());
    QPolygonF arrow;
    arrow << QPointF(scaleRect.left() - arrowWidth, y)
          << QPointF(scaleRect.left(), y - arrowHeight / 2.0)
          << QPointF(scaleRect.left(), y + arrowHeight / 2.0);
    painter.drawPolygon(arrow);
  } else {
    const qreal x = scaleRect.left() + ratio * scaleRect.width();
    const qreal arrowHeight = std::min<qreal>(scaleRect.height() * 0.8, 16.0);
    const qreal arrowWidth = std::min<qreal>(scaleRect.width() * 0.16, 16.0);

    QPen linePen(effectiveForeground());
    linePen.setWidth(2);
    painter.setPen(linePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(x, scaleRect.top()),
        QPointF(x, scaleRect.bottom()));

    painter.setPen(Qt::NoPen);
    painter.setBrush(effectiveForeground());
    QPolygonF arrow;
    arrow << QPointF(x, scaleRect.bottom() + arrowWidth)
          << QPointF(x - arrowHeight / 2.0, scaleRect.bottom())
          << QPointF(x + arrowHeight / 2.0, scaleRect.bottom());
    painter.drawPolygon(arrow);
  }
}

void ScaleMonitorElement::paintLabels(QPainter &painter,
    const QRectF &scaleRect, const QRectF &labelRect) const
{
  if (label_ == MeterLabel::kNone || label_ == MeterLabel::kNoDecorations) {
    return;
  }

  const QColor fg = effectiveForeground();
  painter.setPen(fg);

  if (label_ == MeterLabel::kOutline) {
    QPen outlinePen(fg.darker(160));
    outlinePen.setStyle(Qt::DotLine);
    outlinePen.setWidth(1);
    painter.setPen(outlinePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(scaleRect.adjusted(-4.0, -4.0, 4.0, 4.0));
    return;
  }

  painter.setBrush(Qt::NoBrush);
  painter.setPen(fg);
  QFont labelFont = painter.font();
  labelFont.setPointSizeF(std::max(8.0, scaleRect.height() / 7.0));
  painter.setFont(labelFont);

  if (label_ == MeterLabel::kChannel) {
    const QString text = channel_.trimmed();
    if (!text.isEmpty()) {
      QRectF target = labelRect;
      if (!target.isValid() || target.isEmpty()) {
        target = QRectF(rect().left() + 4.0, scaleRect.bottom() + 4.0,
            rect().width() - 8.0, clampSize(rect().height() - scaleRect.bottom()));
      }
      painter.drawText(target, Qt::AlignCenter | Qt::AlignVCenter, text);
    }
    return;
  }

  if (label_ == MeterLabel::kLimits) {
    const QString lowText = QString::number(limits_.lowDefault, 'g', 5);
    const QString highText = QString::number(limits_.highDefault, 'g', 5);
    if (isVertical()) {
      QRectF highRect(scaleRect.left() - scaleRect.width(),
          scaleRect.top() - 20.0, scaleRect.width() * 3.0, 16.0);
      painter.drawText(highRect, Qt::AlignHCenter | Qt::AlignBottom, highText);
      QRectF lowRect(scaleRect.left() - scaleRect.width(),
          scaleRect.bottom() + 4.0, scaleRect.width() * 3.0, 16.0);
      painter.drawText(lowRect, Qt::AlignHCenter | Qt::AlignTop, lowText);
    } else {
      QRectF lowRect(scaleRect.left() - 48.0, scaleRect.top(), 44.0,
          scaleRect.height());
      painter.drawText(lowRect, Qt::AlignRight | Qt::AlignVCenter, lowText);
      QRectF highRect(scaleRect.right() + 4.0, scaleRect.top(), 44.0,
          scaleRect.height());
      painter.drawText(highRect, Qt::AlignLeft | Qt::AlignVCenter, highText);
    }
  }
}

QColor ScaleMonitorElement::effectiveForeground() const
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

QColor ScaleMonitorElement::effectiveBackground() const
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

bool ScaleMonitorElement::isVertical() const
{
  return direction_ == BarDirection::kUp || direction_ == BarDirection::kDown;
}

bool ScaleMonitorElement::isDirectionInverted() const
{
  return direction_ == BarDirection::kDown || direction_ == BarDirection::kLeft;
}

void ScaleMonitorElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

double ScaleMonitorElement::normalizedSampleValue() const
{
  return kPointerSampleValue;
}

