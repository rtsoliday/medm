#include "line_element.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"

LineElement::LineElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setForegroundColor(defaultForegroundColor());
  setLineStyle(RectangleLineStyle::kSolid);
  setLineWidth(1);
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  designModeVisible_ = QWidget::isVisible();
  update();
}

void LineElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool LineElement::isSelected() const
{
  return selected_;
}

QColor LineElement::color() const
{
  return color_;
}

void LineElement::setForegroundColor(const QColor &color)
{
  QColor effective = color;
  if (!effective.isValid()) {
    effective = defaultForegroundColor();
  }
  if (color_ == effective) {
    return;
  }
  color_ = effective;
  update();
}

RectangleLineStyle LineElement::lineStyle() const
{
  return lineStyle_;
}

void LineElement::setLineStyle(RectangleLineStyle style)
{
  if (lineStyle_ == style) {
    return;
  }
  lineStyle_ = style;
  update();
}

int LineElement::lineWidth() const
{
  return lineWidth_;
}

void LineElement::setLineWidth(int width)
{
  const int clamped = std::max(1, width);
  if (lineWidth_ == clamped) {
    return;
  }
  lineWidth_ = clamped;
  update();
}

TextColorMode LineElement::colorMode() const
{
  return colorMode_;
}

void LineElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode LineElement::visibilityMode() const
{
  return visibilityMode_;
}

void LineElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString LineElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void LineElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString LineElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void LineElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void LineElement::setLocalEndpoints(const QPoint &start, const QPoint &end)
{
  const QSize currentSize = size();
  if (currentSize.isEmpty()) {
    startRatio_ = QPointF(0.0, 0.0);
    endRatio_ = QPointF(1.0, 1.0);
    update();
    return;
  }

  const QPoint clampedStart = clampToSize(start, currentSize);
  const QPoint clampedEnd = clampToSize(end, currentSize);

  startRatio_ = ratioForPoint(clampedStart, currentSize);
  endRatio_ = ratioForPoint(clampedEnd, currentSize);
  update();
}

QVector<QPoint> LineElement::absolutePoints() const
{
  QVector<QPoint> points;
  points.reserve(2);
  const QPoint topLeft = geometry().topLeft();
  points.append(topLeft + pointFromRatio(startRatio_));
  points.append(topLeft + pointFromRatio(endRatio_));
  return points;
}

void LineElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QWidget::isVisible();
  }

  executeMode_ = execute;
  runtimeConnected_ = false;
  runtimeVisible_ = true;
  runtimeSeverity_ = 0;
  updateExecuteState();
}

bool LineElement::isExecuteMode() const
{
  return executeMode_;
}

void LineElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    updateExecuteState();
  }
}

void LineElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyRuntimeVisibility();
  }
}

void LineElement::setRuntimeSeverity(short severity)
{
  if (severity < 0) {
    severity = 0;
  }
  severity = std::min<short>(severity, 3);
  if (runtimeSeverity_ == severity) {
    return;
  }
  runtimeSeverity_ = severity;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    update();
  }
}

void LineElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(visible);
}

void LineElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QColor effectiveColor = effectiveForegroundColor();
  QPen pen(effectiveColor);
  pen.setWidth(lineWidth_);
  pen.setStyle(lineStyle_ == RectangleLineStyle::kDash ? Qt::DashLine
                                                       : Qt::SolidLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  const QPoint startPoint = pointFromRatio(startRatio_);
  const QPoint endPoint = pointFromRatio(endRatio_);
  painter.drawLine(startPoint, endPoint);

  if (selected_) {
    QPen selectionPen(Qt::black);
    selectionPen.setStyle(Qt::DashLine);
    selectionPen.setWidth(1);
    painter.setPen(selectionPen);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

QColor LineElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor LineElement::effectiveForegroundColor() const
{
  const QColor baseColor = color_.isValid() ? color_ : defaultForegroundColor();
  if (!executeMode_) {
    return baseColor;
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    if (!runtimeConnected_) {
      return QColor(255, 255, 255);
    }
    return MedmColors::alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void LineElement::applyRuntimeVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_;
    QWidget::setVisible(visible);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

void LineElement::updateExecuteState()
{
  applyRuntimeVisibility();
  update();
}

QPoint LineElement::clampToSize(const QPoint &point, const QSize &size) const
{
  const int x = std::clamp(point.x(), 0, std::max(0, size.width() - 1));
  const int y = std::clamp(point.y(), 0, std::max(0, size.height() - 1));
  return QPoint(x, y);
}

QPointF LineElement::ratioForPoint(const QPoint &point, const QSize &size) const
{
  const double denomX = size.width() <= 1 ? 1.0 : static_cast<double>(size.width() - 1);
  const double denomY = size.height() <= 1 ? 1.0 : static_cast<double>(size.height() - 1);
  const double rx = denomX == 0.0 ? 0.0 : point.x() / denomX;
  const double ry = denomY == 0.0 ? 0.0 : point.y() / denomY;
  return QPointF(std::clamp(rx, 0.0, 1.0), std::clamp(ry, 0.0, 1.0));
}

QPoint LineElement::pointFromRatio(const QPointF &ratio) const
{
  const double denomX = width() <= 1 ? 1.0 : static_cast<double>(width() - 1);
  const double denomY = height() <= 1 ? 1.0 : static_cast<double>(height() - 1);
  const int x = static_cast<int>(std::round(std::clamp(ratio.x(), 0.0, 1.0) * denomX));
  const int y = static_cast<int>(std::round(std::clamp(ratio.y(), 0.0, 1.0) * denomY));
  return QPoint(x, y);
}

