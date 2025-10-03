#include "strip_chart_element.h"

#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include "medm_colors.h"

namespace {

constexpr int kOuterMargin = 3;
constexpr int kInnerMargin = 6;
constexpr int kGridLines = 5;
constexpr double kPenSampleCount = 24.0;

constexpr int kDefaultPenColorIndex = 14;

QColor defaultPenColor(int index)
{
  Q_UNUSED(index);
  const auto &palette = MedmColors::palette();
  if (palette.size() > kDefaultPenColorIndex) {
    return palette[kDefaultPenColorIndex];
  }
  if (!palette.empty()) {
    return palette.back();
  }
  return QColor(Qt::black);
}

} // namespace

StripChartElement::StripChartElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  title_ = QStringLiteral("Strip Chart");
  xLabel_ = QStringLiteral("Time");
  yLabel_ = QStringLiteral("Value");
  for (int i = 0; i < static_cast<int>(pens_.size()); ++i) {
    pens_[i].limits.lowSource = PvLimitSource::kDefault;
    pens_[i].limits.highSource = PvLimitSource::kDefault;
    pens_[i].limits.precisionSource = PvLimitSource::kDefault;
    pens_[i].limits.lowDefault = 0.0;
    pens_[i].limits.highDefault = 100.0;
    pens_[i].limits.precisionDefault = 1;
    pens_[i].color = defaultPenColor(i);
  }
}

void StripChartElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool StripChartElement::isSelected() const
{
  return selected_;
}

QColor StripChartElement::foregroundColor() const
{
  return foregroundColor_;
}

void StripChartElement::setForegroundColor(const QColor &color)
{
  if (foregroundColor_ == color) {
    return;
  }
  foregroundColor_ = color;
  update();
}

QColor StripChartElement::backgroundColor() const
{
  return backgroundColor_;
}

void StripChartElement::setBackgroundColor(const QColor &color)
{
  if (backgroundColor_ == color) {
    return;
  }
  backgroundColor_ = color;
  update();
}

QString StripChartElement::title() const
{
  return title_;
}

void StripChartElement::setTitle(const QString &title)
{
  if (title_ == title) {
    return;
  }
  title_ = title;
  update();
}

QString StripChartElement::xLabel() const
{
  return xLabel_;
}

void StripChartElement::setXLabel(const QString &label)
{
  if (xLabel_ == label) {
    return;
  }
  xLabel_ = label;
  update();
}

QString StripChartElement::yLabel() const
{
  return yLabel_;
}

void StripChartElement::setYLabel(const QString &label)
{
  if (yLabel_ == label) {
    return;
  }
  yLabel_ = label;
  update();
}

double StripChartElement::period() const
{
  return period_;
}

void StripChartElement::setPeriod(double period)
{
  const double clamped = period > 0.0 ? period : kDefaultStripChartPeriod;
  if (std::abs(period_ - clamped) < 1e-6) {
    return;
  }
  period_ = clamped;
  update();
}

TimeUnits StripChartElement::units() const
{
  return units_;
}

void StripChartElement::setUnits(TimeUnits units)
{
  if (units_ == units) {
    return;
  }
  units_ = units;
  update();
}

int StripChartElement::penCount() const
{
  return static_cast<int>(pens_.size());
}

QString StripChartElement::channel(int index) const
{
  if (index < 0 || index >= penCount()) {
    return QString();
  }
  return pens_[index].channel;
}

void StripChartElement::setChannel(int index, const QString &channel)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  if (pens_[index].channel == channel) {
    return;
  }
  pens_[index].channel = channel;
  update();
}

QColor StripChartElement::penColor(int index) const
{
  if (index < 0 || index >= penCount()) {
    return QColor();
  }
  return pens_[index].color;
}

void StripChartElement::setPenColor(int index, const QColor &color)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  if (pens_[index].color == color) {
    return;
  }
  pens_[index].color = color;
  update();
}

PvLimits StripChartElement::penLimits(int index) const
{
  if (index < 0 || index >= penCount()) {
    return PvLimits{};
  }
  return pens_[index].limits;
}

void StripChartElement::setPenLimits(int index, const PvLimits &limits)
{
  if (index < 0 || index >= penCount()) {
    return;
  }
  pens_[index].limits = limits;
  pens_[index].limits.precisionDefault =
      std::clamp(pens_[index].limits.precisionDefault, 0, 17);
  update();
}

void StripChartElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  paintFrame(painter);

  const QRect content = chartRect();
  painter.fillRect(content, effectiveBackground());

  paintGrid(painter, content.adjusted(1, 1, -1, -1));
  paintPens(painter, content.adjusted(1, 1, -1, -1));
  paintLabels(painter, content);

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor StripChartElement::effectiveForeground() const
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

QColor StripChartElement::effectiveBackground() const
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

QColor StripChartElement::effectivePenColor(int index) const
{
  if (index < 0 || index >= penCount()) {
    return QColor();
  }
  if (pens_[index].color.isValid()) {
    return pens_[index].color;
  }
  return defaultPenColor(index);
}

QRect StripChartElement::chartRect() const
{
  return rect().adjusted(kOuterMargin, kOuterMargin, -kOuterMargin,
      -kOuterMargin);
}

void StripChartElement::paintFrame(QPainter &painter) const
{
  const QRect frameRect = rect().adjusted(0, 0, -1, -1);
  QPen pen(effectiveForeground());
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(frameRect);
}

void StripChartElement::paintGrid(QPainter &painter, const QRect &content) const
{
  if (content.width() <= 0 || content.height() <= 0) {
    return;
  }
  QColor gridColor = effectiveForeground();
  gridColor.setAlpha(80);
  QPen pen(gridColor);
  pen.setStyle(Qt::DotLine);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  const int verticalLines = kGridLines;
  const int horizontalLines = kGridLines;
  for (int i = 1; i < verticalLines; ++i) {
    const int x = content.left() + i * content.width() / verticalLines;
    painter.drawLine(x, content.top(), x, content.bottom());
  }
  for (int j = 1; j < horizontalLines; ++j) {
    const int y = content.top() + j * content.height() / horizontalLines;
    painter.drawLine(content.left(), y, content.right(), y);
  }
}

void StripChartElement::paintPens(QPainter &painter, const QRect &content) const
{
  if (content.width() <= 0 || content.height() <= 0) {
    return;
  }

  for (int i = 0; i < penCount(); ++i) {
    const QString channelName = pens_[i].channel.trimmed();
    if (channelName.isEmpty() && i > 0) {
      continue;
    }
    QPen pen(effectivePenColor(i));
    pen.setWidth(1);
    painter.setPen(pen);

    QPainterPath path;
    const int samples = static_cast<int>(kPenSampleCount);
    for (int s = 0; s <= samples; ++s) {
      const double t = static_cast<double>(s) / samples;
      const double phase = (static_cast<double>(i) * 0.6);
      const double value = 0.5 + 0.4 * std::sin((t * 6.28318) + phase);
      const double yValue = content.bottom() - value * content.height();
      const double xValue = content.left() + t * content.width();
      if (s == 0) {
        path.moveTo(xValue, yValue);
      } else {
        path.lineTo(xValue, yValue);
      }
    }
    painter.drawPath(path);
  }
}

void StripChartElement::paintLabels(QPainter &painter, const QRect &content) const
{
  painter.save();
  painter.setPen(effectiveForeground());
  QFontMetrics metrics(font());

  const QRect titleRect = QRect(content.left(), rect().top() + kInnerMargin,
      content.width(), metrics.height());
  if (!title_.trimmed().isEmpty()) {
    painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop,
        title_.trimmed());
  }

  const QRect xLabelRect = QRect(content.left(), rect().bottom() - kInnerMargin
          - metrics.height(),
      content.width(), metrics.height());
  if (!xLabel_.trimmed().isEmpty()) {
    painter.drawText(xLabelRect, Qt::AlignHCenter | Qt::AlignBottom,
        xLabel_.trimmed());
  }

  if (!yLabel_.trimmed().isEmpty()) {
    painter.save();
    painter.translate(rect().left() + kInnerMargin,
        content.center().y());
    painter.rotate(-90.0);
    painter.drawText(QRect(-content.height() / 2, -metrics.height() / 2,
                         content.height(), metrics.height()),
        Qt::AlignCenter, yLabel_.trimmed());
    painter.restore();
  }

  painter.restore();
}

void StripChartElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

