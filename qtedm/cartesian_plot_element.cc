#include "cartesian_plot_element.h"

#include <algorithm>
#include <cmath>

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>

#include "medm_colors.h"

namespace {

constexpr int kOuterMargin = 4;
constexpr int kInnerMargin = 8;
constexpr int kGridLines = 5;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kMinimumSampleCount = 8;
constexpr int kMaximumSampleCount = 256;

constexpr int kDefaultTraceColorIndex = 14;

QColor defaultTraceColor(int index)
{
  Q_UNUSED(index);
  const auto &palette = MedmColors::palette();
  if (palette.size() > kDefaultTraceColorIndex) {
    return palette[kDefaultTraceColorIndex];
  }
  if (!palette.empty()) {
    return palette.back();
  }
  return QColor(Qt::black);
}

} // namespace

CartesianPlotElement::CartesianPlotElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  title_ = QStringLiteral("Cartesian Plot");
  xLabel_ = QStringLiteral("X");
  yLabels_[0] = QStringLiteral("Y1");
  yLabels_[1] = QStringLiteral("Y2");
  yLabels_[2] = QStringLiteral("Y3");
  yLabels_[3] = QStringLiteral("Y4");
  for (int i = 0; i < traceCount(); ++i) {
    traces_[i].color = defaultTraceColor(i);
  }
  for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
    axisStyles_[axis] = CartesianPlotAxisStyle::kLinear;
    axisRangeStyles_[axis] = CartesianPlotRangeStyle::kChannel;
    axisMinimums_[axis] = 0.0;
    axisMaximums_[axis] = 1.0;
    axisTimeFormats_[axis] = CartesianPlotTimeFormat::kHhMmSs;
  }
}

void CartesianPlotElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool CartesianPlotElement::isSelected() const
{
  return selected_;
}

QColor CartesianPlotElement::foregroundColor() const
{
  return foregroundColor_;
}

void CartesianPlotElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor CartesianPlotElement::backgroundColor() const
{
  return backgroundColor_;
}

void CartesianPlotElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

QString CartesianPlotElement::title() const
{
  return title_;
}

void CartesianPlotElement::setTitle(const QString &title)
{
  if (title_ == title) {
    return;
  }
  title_ = title;
  update();
}

QString CartesianPlotElement::xLabel() const
{
  return xLabel_;
}

void CartesianPlotElement::setXLabel(const QString &label)
{
  if (xLabel_ == label) {
    return;
  }
  xLabel_ = label;
  update();
}

QString CartesianPlotElement::yLabel(int index) const
{
  if (index < 0 || index >= static_cast<int>(yLabels_.size())) {
    return QString();
  }
  return yLabels_[index];
}

void CartesianPlotElement::setYLabel(int index, const QString &label)
{
  if (index < 0 || index >= static_cast<int>(yLabels_.size())) {
    return;
  }
  if (yLabels_[index] == label) {
    return;
  }
  yLabels_[index] = label;
  update();
}

CartesianPlotStyle CartesianPlotElement::style() const
{
  return style_;
}

void CartesianPlotElement::setStyle(CartesianPlotStyle style)
{
  if (style_ == style) {
    return;
  }
  style_ = style;
  update();
}

bool CartesianPlotElement::eraseOldest() const
{
  return eraseOldest_;
}

void CartesianPlotElement::setEraseOldest(bool eraseOldest)
{
  if (eraseOldest_ == eraseOldest) {
    return;
  }
  eraseOldest_ = eraseOldest;
  update();
}

int CartesianPlotElement::count() const
{
  return count_;
}

void CartesianPlotElement::setCount(int count)
{
  const int clamped = std::clamp(count, 1, kMaximumSampleCount);
  if (count_ == clamped) {
    return;
  }
  count_ = clamped;
  update();
}

CartesianPlotEraseMode CartesianPlotElement::eraseMode() const
{
  return eraseMode_;
}

void CartesianPlotElement::setEraseMode(CartesianPlotEraseMode mode)
{
  if (eraseMode_ == mode) {
    return;
  }
  eraseMode_ = mode;
  update();
}

QString CartesianPlotElement::triggerChannel() const
{
  return triggerChannel_;
}

void CartesianPlotElement::setTriggerChannel(const QString &channel)
{
  if (triggerChannel_ == channel) {
    return;
  }
  triggerChannel_ = channel;
  update();
}

QString CartesianPlotElement::eraseChannel() const
{
  return eraseChannel_;
}

void CartesianPlotElement::setEraseChannel(const QString &channel)
{
  if (eraseChannel_ == channel) {
    return;
  }
  eraseChannel_ = channel;
  update();
}

QString CartesianPlotElement::countChannel() const
{
  return countChannel_;
}

void CartesianPlotElement::setCountChannel(const QString &channel)
{
  if (countChannel_ == channel) {
    return;
  }
  countChannel_ = channel;
  update();
}

int CartesianPlotElement::traceCount() const
{
  return static_cast<int>(traces_.size());
}

QString CartesianPlotElement::traceXChannel(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return QString();
  }
  return traces_[index].xChannel;
}

void CartesianPlotElement::setTraceXChannel(int index, const QString &channel)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].xChannel == channel) {
    return;
  }
  traces_[index].xChannel = channel;
  update();
}

QString CartesianPlotElement::traceYChannel(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return QString();
  }
  return traces_[index].yChannel;
}

void CartesianPlotElement::setTraceYChannel(int index, const QString &channel)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].yChannel == channel) {
    return;
  }
  traces_[index].yChannel = channel;
  update();
}

QColor CartesianPlotElement::traceColor(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return QColor();
  }
  return traces_[index].color;
}

void CartesianPlotElement::setTraceColor(int index, const QColor &color)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].color == color) {
    return;
  }
  traces_[index].color = color;
  update();
}

CartesianPlotYAxis CartesianPlotElement::traceYAxis(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return CartesianPlotYAxis::kY1;
  }
  return traces_[index].yAxis;
}

void CartesianPlotElement::setTraceYAxis(int index, CartesianPlotYAxis axis)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].yAxis == axis) {
    return;
  }
  traces_[index].yAxis = axis;
  update();
}

bool CartesianPlotElement::traceUsesRightAxis(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return false;
  }
  return traces_[index].usesRightAxis;
}

void CartesianPlotElement::setTraceUsesRightAxis(int index, bool usesRightAxis)
{
  if (index < 0 || index >= traceCount()) {
    return;
  }
  if (traces_[index].usesRightAxis == usesRightAxis) {
    return;
  }
  traces_[index].usesRightAxis = usesRightAxis;
  update();
}

CartesianPlotAxisStyle CartesianPlotElement::axisStyle(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return CartesianPlotAxisStyle::kLinear;
  }
  return axisStyles_[axisIndex];
}

void CartesianPlotElement::setAxisStyle(int axisIndex,
    CartesianPlotAxisStyle style)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisStyles_[axisIndex] == style) {
    return;
  }
  axisStyles_[axisIndex] = style;
  update();
}

CartesianPlotRangeStyle CartesianPlotElement::axisRangeStyle(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return CartesianPlotRangeStyle::kChannel;
  }
  return axisRangeStyles_[axisIndex];
}

void CartesianPlotElement::setAxisRangeStyle(int axisIndex,
    CartesianPlotRangeStyle style)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisRangeStyles_[axisIndex] == style) {
    return;
  }
  axisRangeStyles_[axisIndex] = style;
  update();
}

double CartesianPlotElement::axisMinimum(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return 0.0;
  }
  return axisMinimums_[axisIndex];
}

void CartesianPlotElement::setAxisMinimum(int axisIndex, double value)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisMinimums_[axisIndex] == value) {
    return;
  }
  axisMinimums_[axisIndex] = value;
  update();
}

double CartesianPlotElement::axisMaximum(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return 1.0;
  }
  return axisMaximums_[axisIndex];
}

void CartesianPlotElement::setAxisMaximum(int axisIndex, double value)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisMaximums_[axisIndex] == value) {
    return;
  }
  axisMaximums_[axisIndex] = value;
  update();
}

CartesianPlotTimeFormat CartesianPlotElement::axisTimeFormat(int axisIndex) const
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return CartesianPlotTimeFormat::kHhMmSs;
  }
  return axisTimeFormats_[axisIndex];
}

void CartesianPlotElement::setAxisTimeFormat(int axisIndex,
    CartesianPlotTimeFormat format)
{
  if (axisIndex < 0 || axisIndex >= kCartesianAxisCount) {
    return;
  }
  if (axisTimeFormats_[axisIndex] == format) {
    return;
  }
  axisTimeFormats_[axisIndex] = format;
  update();
}

void CartesianPlotElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  painter.fillRect(rect(), effectiveBackground());

  const QRectF chart = chartRect();
  paintFrame(painter);
  paintGrid(painter, chart);
  paintTraces(painter, chart);
  paintAxes(painter, chart);
  paintLabels(painter, chart);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor CartesianPlotElement::effectiveForeground() const
{
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return palette().color(QPalette::WindowText);
}

QColor CartesianPlotElement::effectiveBackground() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return palette().color(QPalette::Window);
}

QColor CartesianPlotElement::effectiveTraceColor(int index) const
{
  if (index < 0 || index >= traceCount()) {
    return effectiveForeground();
  }
  const QColor color = traces_[index].color;
  if (color.isValid()) {
    return color;
  }
  return defaultTraceColor(index);
}

QRectF CartesianPlotElement::chartRect() const
{
  QRectF frame = rect().adjusted(kOuterMargin, kOuterMargin,
      -kOuterMargin, -kOuterMargin);
  if (frame.width() <= 0.0 || frame.height() <= 0.0) {
    return frame;
  }

  const QFontMetrics metrics(font());
  qreal topMargin = kInnerMargin;
  if (!title_.trimmed().isEmpty()) {
    topMargin += metrics.height();
  }
  if (!yLabels_[2].trimmed().isEmpty() || !yLabels_[3].trimmed().isEmpty()) {
    topMargin += metrics.height();
  }

  qreal bottomMargin = kInnerMargin;
  if (!xLabel_.trimmed().isEmpty()) {
    bottomMargin += metrics.height();
  }

  qreal leftMargin = kInnerMargin;
  if (!yLabels_[0].trimmed().isEmpty()) {
    leftMargin += metrics.height();
  }

  qreal rightMargin = kInnerMargin;
  if (!yLabels_[1].trimmed().isEmpty()) {
    rightMargin += metrics.height();
  }

  frame.adjust(leftMargin, topMargin, -rightMargin, -bottomMargin);
  return frame;
}

void CartesianPlotElement::paintFrame(QPainter &painter) const
{
  const QRect frameRect = rect().adjusted(0, 0, -1, -1);
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(frameRect);
}

void CartesianPlotElement::paintGrid(QPainter &painter, const QRectF &rect) const
{
  if (rect.width() <= 0.0 || rect.height() <= 0.0) {
    return;
  }
  QColor gridColor = effectiveForeground();
  gridColor.setAlpha(80);
  QPen pen(gridColor);
  pen.setStyle(Qt::DotLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  for (int i = 1; i < kGridLines; ++i) {
    const qreal x = rect.left() + i * rect.width() / kGridLines;
    painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
  }
  for (int j = 1; j < kGridLines; ++j) {
    const qreal y = rect.top() + j * rect.height() / kGridLines;
    painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
  }
}

void CartesianPlotElement::paintAxes(QPainter &painter, const QRectF &rect) const
{
  if (rect.width() <= 0.0 || rect.height() <= 0.0) {
    return;
  }
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  painter.drawLine(QPointF(rect.left(), rect.bottom()),
      QPointF(rect.right(), rect.bottom()));
  painter.drawLine(QPointF(rect.left(), rect.top()),
      QPointF(rect.left(), rect.bottom()));
  painter.drawLine(QPointF(rect.right(), rect.top()),
      QPointF(rect.right(), rect.bottom()));
}

void CartesianPlotElement::paintLabels(QPainter &painter, const QRectF &rect) const
{
  painter.save();
  painter.setPen(effectiveForeground());
  QFontMetrics metrics(font());

  if (!title_.trimmed().isEmpty()) {
    QRectF titleRect(rect.left(), rect.top() - metrics.height() - 2.0,
        rect.width(), metrics.height());
    painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop,
        title_.trimmed());
  }

  if (!xLabel_.trimmed().isEmpty()) {
    QRectF xRect(rect.left(), rect.bottom() + 2.0,
        rect.width(), metrics.height());
    painter.drawText(xRect, Qt::AlignHCenter | Qt::AlignTop,
        xLabel_.trimmed());
  }

  if (!yLabels_[0].trimmed().isEmpty()) {
    painter.save();
    painter.translate(rect.left() - metrics.height() / 2.0 - 2.0,
        rect.center().y());
    painter.rotate(-90.0);
    painter.drawText(QRectF(-rect.height() / 2.0, -metrics.height() / 2.0,
                         rect.height(), metrics.height()),
        Qt::AlignCenter, yLabels_[0].trimmed());
    painter.restore();
  }

  if (!yLabels_[1].trimmed().isEmpty()) {
    painter.save();
    painter.translate(rect.right() + metrics.height() / 2.0 + 2.0,
        rect.center().y());
    painter.rotate(90.0);
    painter.drawText(QRectF(-rect.height() / 2.0, -metrics.height() / 2.0,
                         rect.height(), metrics.height()),
        Qt::AlignCenter, yLabels_[1].trimmed());
    painter.restore();
  }

  const bool hasY3 = !yLabels_[2].trimmed().isEmpty();
  const bool hasY4 = !yLabels_[3].trimmed().isEmpty();
  if (hasY3 || hasY4) {
    const qreal top = rect.top() - metrics.height() - 2.0;
    if (hasY3) {
      QRectF y3Rect(rect.left(), top, rect.width() / 2.0, metrics.height());
      painter.drawText(y3Rect, Qt::AlignLeft | Qt::AlignBottom,
          yLabels_[2].trimmed());
    }
    if (hasY4) {
      QRectF y4Rect(rect.left() + rect.width() / 2.0, top, rect.width() / 2.0,
          metrics.height());
      painter.drawText(y4Rect, Qt::AlignRight | Qt::AlignBottom,
          yLabels_[3].trimmed());
    }
  }

  painter.restore();
}

void CartesianPlotElement::paintTraces(QPainter &painter, const QRectF &rect) const
{
  if (rect.width() <= 0.0 || rect.height() <= 0.0) {
    return;
  }

  const int samples = std::clamp(count_, kMinimumSampleCount,
      kMaximumSampleCount);

  for (int i = 0; i < traceCount(); ++i) {
    const QString yChannel = traces_[i].yChannel.trimmed();
    const QString xChannel = traces_[i].xChannel.trimmed();
    if (yChannel.isEmpty() && xChannel.isEmpty() && i > 0) {
      continue;
    }
    const QVector<QPointF> points = syntheticTracePoints(rect, i, samples);
    if (points.isEmpty()) {
      continue;
    }

    const QColor color = effectiveTraceColor(i);
    QPen pen(color);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (style_) {
    case CartesianPlotStyle::kPoint: {
      painter.save();
      painter.setBrush(color);
      for (const QPointF &point : points) {
        painter.drawEllipse(point, 2.0, 2.0);
      }
      painter.restore();
      break;
    }
    case CartesianPlotStyle::kLine: {
      QPainterPath path(points.front());
      for (int j = 1; j < points.size(); ++j) {
        path.lineTo(points[j]);
      }
      painter.drawPath(path);
      break;
    }
    case CartesianPlotStyle::kStep: {
      QPainterPath path(points.front());
      for (int j = 1; j < points.size(); ++j) {
        const QPointF &prev = points[j - 1];
        const QPointF &curr = points[j];
        path.lineTo(curr.x(), prev.y());
        path.lineTo(curr);
      }
      painter.drawPath(path);
      break;
    }
    case CartesianPlotStyle::kFillUnder: {
      QPainterPath path(points.front());
      for (int j = 1; j < points.size(); ++j) {
        path.lineTo(points[j]);
      }
      path.lineTo(QPointF(points.back().x(), rect.bottom()));
      path.lineTo(QPointF(points.front().x(), rect.bottom()));
      path.closeSubpath();
      QColor fillColor = color;
      fillColor.setAlpha(80);
      painter.save();
      painter.setBrush(fillColor);
      painter.drawPath(path);
      painter.restore();
      break;
    }
    }
  }
}

void CartesianPlotElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

QVector<QPointF> CartesianPlotElement::syntheticTracePoints(const QRectF &rect,
    int traceIndex, int sampleCount) const
{
  QVector<QPointF> points;
  if (sampleCount < 2 || rect.width() <= 0.0 || rect.height() <= 0.0) {
    return points;
  }
  points.reserve(sampleCount);
  const double phaseOffset = static_cast<double>(traceIndex) * 0.7;
  const double amplitude = 0.35 + 0.1 * (traceIndex % 3);
  const double offset = 0.5 + 0.1 * ((traceIndex % 4) - 1.5);
  for (int i = 0; i < sampleCount; ++i) {
    const double t = sampleCount > 1 ? static_cast<double>(i)
            / static_cast<double>(sampleCount - 1)
                                     : 0.0;
    double value = offset + amplitude * std::sin((t * kTwoPi) + phaseOffset);
    value = std::clamp(value, 0.0, 1.0);
    const double x = rect.left() + t * rect.width();
    const double y = rect.bottom() - value * rect.height();
    points.append(QPointF(x, y));
  }
  return points;
}
