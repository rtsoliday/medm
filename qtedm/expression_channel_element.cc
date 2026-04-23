#include "expression_channel_element.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QUuid>

#include "pv_name_utils.h"

namespace {

constexpr int kHorizontalMargin = 6;
constexpr int kVerticalMargin = 4;

} // namespace

ExpressionChannelElement::ExpressionChannelElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setForegroundColor(palette().color(QPalette::WindowText));
  setBackgroundColor(palette().color(QPalette::Window));
  resize(kMinimumExpressionChannelWidth, kMinimumExpressionChannelHeight);
  updateToolTip();
}

void ExpressionChannelElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ExpressionChannelElement::isSelected() const
{
  return selected_;
}

QString ExpressionChannelElement::variable() const
{
  return variable_;
}

void ExpressionChannelElement::setVariable(const QString &variable)
{
  const QString normalized = variable.trimmed();
  if (variable_ == normalized) {
    return;
  }
  variable_ = normalized;
  if (!variable_.isEmpty()) {
    resolvedVariableName_.clear();
  }
  updateToolTip();
  update();
}

QString ExpressionChannelElement::resolvedVariableName()
{
  if (!variable_.isEmpty()) {
    return variable_;
  }
  if (resolvedVariableName_.isEmpty()) {
    resolvedVariableName_ = QStringLiteral("__expr_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  }
  return resolvedVariableName_;
}

QString ExpressionChannelElement::calc() const
{
  return calc_;
}

void ExpressionChannelElement::setCalc(const QString &calc)
{
  const QString normalized = calc.trimmed();
  if (calc_ == normalized) {
    return;
  }
  calc_ = normalized;
  updateToolTip();
  update();
}

QString ExpressionChannelElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[static_cast<std::size_t>(index)];
}

void ExpressionChannelElement::setChannel(int index, const QString &channel)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  const QString normalized = PvNameUtils::normalizePvName(channel);
  QString &stored = channels_[static_cast<std::size_t>(index)];
  if (stored == normalized) {
    return;
  }
  stored = normalized;
  updateToolTip();
}

double ExpressionChannelElement::initialValue() const
{
  return initialValue_;
}

void ExpressionChannelElement::setInitialValue(double value)
{
  if (qFuzzyCompare(initialValue_ + 1.0, value + 1.0)) {
    return;
  }
  initialValue_ = value;
  updateToolTip();
}

ExpressionChannelEventSignalMode ExpressionChannelElement::eventSignalMode() const
{
  return eventSignalMode_;
}

void ExpressionChannelElement::setEventSignalMode(
    ExpressionChannelEventSignalMode mode)
{
  if (eventSignalMode_ == mode) {
    return;
  }
  eventSignalMode_ = mode;
  updateToolTip();
}

QColor ExpressionChannelElement::foregroundColor() const
{
  return foregroundColor_;
}

void ExpressionChannelElement::setForegroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  update();
}

QColor ExpressionChannelElement::backgroundColor() const
{
  return backgroundColor_;
}

void ExpressionChannelElement::setBackgroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  update();
}

int ExpressionChannelElement::precision() const
{
  return precision_;
}

void ExpressionChannelElement::setPrecision(int precision)
{
  const int clamped = std::max(0, precision);
  if (precision_ == clamped) {
    return;
  }
  precision_ = clamped;
  updateToolTip();
}

void ExpressionChannelElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QWidget::isVisible();
  }

  executeMode_ = execute;
  if (executeMode_) {
    QWidget::setVisible(false);
  } else {
    QWidget::setVisible(designModeVisible_);
  }
}

bool ExpressionChannelElement::isExecuteMode() const
{
  return executeMode_;
}

void ExpressionChannelElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QWidget::setVisible(executeMode_ ? false : visible);
}

void ExpressionChannelElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.fillRect(rect(), backgroundColor_);

  QRect borderRect = rect().adjusted(0, 0, -1, -1);
  QPen borderPen(foregroundColor_.darker(110));
  borderPen.setWidth(1);
  painter.setPen(borderPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(borderRect);

  const int contentWidth = std::max(1, width() - (2 * kHorizontalMargin));
  QRect headerRect(kHorizontalMargin, kVerticalMargin, contentWidth,
      std::max(12, height() / 2 - kVerticalMargin));
  QRect bodyRect(kHorizontalMargin, headerRect.bottom() + 2, contentWidth,
      std::max(10, height() - headerRect.height() - (2 * kVerticalMargin)));

  QFont headerFont = font();
  headerFont.setBold(true);
  painter.setFont(headerFont);
  painter.setPen(foregroundColor_);

  const QString variableLabel = variable_.isEmpty()
      ? QStringLiteral("(auto)")
      : variable_;
  const QFontMetrics headerMetrics(headerFont);
  painter.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter,
      headerMetrics.elidedText(QStringLiteral("f(x) %1").arg(variableLabel),
          Qt::ElideRight, headerRect.width()));

  QFont bodyFont = font();
  if (bodyFont.pointSize() > 0) {
    bodyFont.setPointSize(std::max(6, bodyFont.pointSize() - 1));
  }
  painter.setFont(bodyFont);
  const QFontMetrics bodyMetrics(bodyFont);
  const QString calcLabel = calc_.isEmpty() ? QStringLiteral("A+B") : calc_;
  painter.drawText(bodyRect, Qt::AlignLeft | Qt::AlignVCenter,
      bodyMetrics.elidedText(calcLabel, Qt::ElideRight, bodyRect.width()));

  if (selected_) {
    QPen selectionPen(Qt::black);
    selectionPen.setStyle(Qt::DashLine);
    selectionPen.setWidth(1);
    painter.setPen(selectionPen);
    painter.drawRect(borderRect);
  }
}

QColor ExpressionChannelElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  return palette().color(QPalette::WindowText);
}

QColor ExpressionChannelElement::defaultBackgroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  return palette().color(QPalette::Window);
}

void ExpressionChannelElement::updateToolTip()
{
  setToolTip(QString());
}
