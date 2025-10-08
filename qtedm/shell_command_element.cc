#include "shell_command_element.h"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>

namespace {

QString sanitizedLabel(const QString &value, bool &showIcon)
{
  showIcon = true;
  QString trimmed = value;
  if (trimmed.startsWith(QLatin1Char('-'))) {
    showIcon = false;
    trimmed.remove(0, 1);
  }
  return trimmed.trimmed();
}

QString entryDisplayLabel(const ShellCommandEntry &entry)
{
  QString label = entry.label.trimmed();
  if (!label.isEmpty()) {
    return label;
  }
  QString command = entry.command.trimmed();
  if (!command.isEmpty()) {
    return command;
  }
  return QString();
}

int messageButtonPixelLimit(int height)
{
  if (height <= 0) {
    return 1;
  }
  const double scaled = 0.90 * static_cast<double>(height);
  int limit = static_cast<int>(scaled) - 4;
  return std::max(1, limit);
}

QFont scaledFontForHeight(const QFont &base, int pixelLimit)
{
  if (pixelLimit <= 0) {
    return base;
  }

  QFont adjusted(base);
  if (adjusted.pixelSize() > 0) {
    adjusted.setPixelSize(pixelLimit);
    return adjusted;
  }

  qreal pointSize = adjusted.pointSizeF();
  if (pointSize <= 0.0) {
    pointSize = adjusted.pointSize();
  }
  if (pointSize <= 0.0) {
    QFontInfo info(adjusted);
    pointSize = info.pointSizeF();
  }
  if (pointSize <= 0.0) {
    pointSize = 12.0;
  }

  QFontMetricsF metrics(adjusted);
  qreal textHeight = metrics.ascent() + metrics.descent();
  if (textHeight <= 0.0) {
    textHeight = static_cast<qreal>(pixelLimit);
  }

  qreal scaledPoint = pointSize * static_cast<qreal>(pixelLimit) / textHeight;
  if (scaledPoint < 1.0) {
    scaledPoint = 1.0;
  }
  adjusted.setPointSizeF(scaledPoint);

  QFontMetricsF scaledMetrics(adjusted);
  qreal scaledHeight = scaledMetrics.ascent() + scaledMetrics.descent();
  int iterations = 0;
  while (scaledHeight > pixelLimit && scaledPoint > 1.0 && iterations < 16) {
    scaledPoint = std::max<qreal>(1.0, scaledPoint - 0.5);
    adjusted.setPointSizeF(scaledPoint);
    scaledMetrics = QFontMetricsF(adjusted);
    scaledHeight = scaledMetrics.ascent() + scaledMetrics.descent();
    ++iterations;
  }

  return adjusted;
}

} // namespace

ShellCommandElement::ShellCommandElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
}

void ShellCommandElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ShellCommandElement::isSelected() const
{
  return selected_;
}

QColor ShellCommandElement::foregroundColor() const
{
  return foregroundColor_;
}

void ShellCommandElement::setForegroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : palette().color(QPalette::WindowText);
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  update();
}

QColor ShellCommandElement::backgroundColor() const
{
  return backgroundColor_;
}

void ShellCommandElement::setBackgroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : palette().color(QPalette::Window);
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  update();
}

QString ShellCommandElement::label() const
{
  return label_;
}

void ShellCommandElement::setLabel(const QString &label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

int ShellCommandElement::entryCount() const
{
  return static_cast<int>(entries_.size());
}

ShellCommandEntry ShellCommandElement::entry(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return ShellCommandEntry{};
  }
  return entries_[index];
}

void ShellCommandElement::setEntry(int index, const ShellCommandEntry &entry)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  entries_[index] = entry;
  update();
}

QString ShellCommandElement::entryLabel(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return QString();
  }
  return entries_[index].label;
}

void ShellCommandElement::setEntryLabel(int index, const QString &label)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].label == label) {
    return;
  }
  entries_[index].label = label;
  update();
}

QString ShellCommandElement::entryCommand(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return QString();
  }
  return entries_[index].command;
}

void ShellCommandElement::setEntryCommand(int index, const QString &command)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].command == command) {
    return;
  }
  entries_[index].command = command;
  update();
}

QString ShellCommandElement::entryArgs(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return QString();
  }
  return entries_[index].args;
}

void ShellCommandElement::setEntryArgs(int index, const QString &args)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].args == args) {
    return;
  }
  entries_[index].args = args;
  update();
}

void ShellCommandElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QRect canvas = rect();
  painter.fillRect(canvas, effectiveBackground());

  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();

  QRect frame = canvas.adjusted(0, 0, -1, -1);
  painter.setPen(QPen(bg.lighter(130), 1));
  painter.drawLine(frame.topLeft(), frame.topRight());
  painter.drawLine(frame.topLeft(), frame.bottomLeft());
  painter.setPen(QPen(bg.darker(130), 1));
  painter.drawLine(frame.bottomLeft(), frame.bottomRight());
  painter.drawLine(frame.topRight(), frame.bottomRight());

  QRect content = canvas.adjusted(3, 3, -3, -3);
  bool showIcon = true;
  QString text = displayLabel(showIcon);

  const int activeCount = activeEntryCount();
  const bool showArrow = activeCount > 1;
  const bool singleEntry = activeCount == 1;

  QRect arrowRect = content;
  if (showArrow) {
    arrowRect.setLeft(content.right() - 10);
  }

  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = scaledFontForHeight(painter.font(), fontLimit);
  painter.setFont(labelFont);
  QFontMetricsF labelMetrics(labelFont);
  const int textWidth = std::max(0, static_cast<int>(std::ceil(labelMetrics.horizontalAdvance(text))));

  const int iconSize = showIcon ? std::min(content.height(), 24) : 0;
  const int spacing = (showIcon && !text.isEmpty()) ? 6 : 0;

  QRect textRect = content;
  if (showArrow) {
    textRect.setRight(arrowRect.left() - 4);
  }

  QRect iconRect;

  if (singleEntry && !showArrow) {
    int totalWidth = textWidth;
    if (showIcon && iconSize > 0) {
      totalWidth += iconSize + spacing;
    }
    int layoutLeft = content.left();
    int extra = content.width() - totalWidth;
    if (extra > 0) {
      layoutLeft += extra / 2;
    }
    if (showIcon && iconSize > 0) {
      iconRect = QRect(layoutLeft, content.top(), iconSize, iconSize);
      iconRect.moveTop(content.top() + (content.height() - iconSize) / 2);
      layoutLeft = iconRect.right() + spacing;
    }
    textRect.setLeft(layoutLeft);
    textRect.setRight(layoutLeft + textWidth);
    if (textRect.width() < 0) {
      textRect.setRight(textRect.left());
    }
    textRect.setTop(content.top());
    textRect.setBottom(content.bottom());
  } else {
    if (showIcon && iconSize > 0) {
      iconRect = QRect(content.left(), content.top(), iconSize, iconSize);
      iconRect.moveTop(content.top() + (content.height() - iconSize) / 2);
      textRect.setLeft(iconRect.right() + spacing);
    } else {
      textRect.setLeft(content.left() + 4);
    }
    textRect = textRect.adjusted(0, 0, -2, 0);
  }

  if (showIcon && iconSize > 0 && iconRect.width() > 0 && iconRect.height() > 0) {
    paintIcon(painter, iconRect);
  }

  painter.setPen(fg);
  painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

  if (showArrow) {
    painter.setBrush(fg);
    painter.setPen(Qt::NoPen);
    const int arrowWidth = 7;
    const int arrowHeight = 5;
    const int arrowX = arrowRect.right() - arrowWidth - 1;
    const int arrowY = arrowRect.center().y() - arrowHeight / 2;
    QPoint points[3] = {
        QPoint(arrowX, arrowY),
        QPoint(arrowX + arrowWidth, arrowY),
        QPoint(arrowX + arrowWidth / 2, arrowY + arrowHeight)};
    painter.drawPolygon(points, 3);
  }

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor ShellCommandElement::effectiveForeground() const
{
  return foregroundColor_.isValid() ? foregroundColor_
                                    : palette().color(QPalette::WindowText);
}

QColor ShellCommandElement::effectiveBackground() const
{
  return backgroundColor_.isValid() ? backgroundColor_
                                    : palette().color(QPalette::Window);
}

QString ShellCommandElement::displayLabel(bool &showIcon) const
{
  QString base = sanitizedLabel(label_, showIcon);
  if (!base.isEmpty()) {
    return base;
  }
  for (const auto &entry : entries_) {
    QString candidate = entryDisplayLabel(entry);
    if (!candidate.isEmpty()) {
      return candidate;
    }
  }
  showIcon = true;
  return QStringLiteral("Shell Command");
}

int ShellCommandElement::activeEntryCount() const
{
  int count = 0;
  for (const auto &entry : entries_) {
    if (!entry.label.trimmed().isEmpty() || !entry.command.trimmed().isEmpty()
        || !entry.args.trimmed().isEmpty()) {
      ++count;
    }
  }
  return count;
}

void ShellCommandElement::paintIcon(QPainter &painter, const QRect &rect) const
{
  if (rect.width() <= 0 || rect.height() <= 0) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);

  const int size = std::min(rect.width(), rect.height());
  QRect square(rect.left(), rect.top(), size, size);
  square.moveTop(rect.top() + (rect.height() - size) / 2);

  painter.setPen(Qt::NoPen);
  painter.setBrush(effectiveForeground());

  const auto scaled = [size](double value) {
    return static_cast<int>(std::round(value * size));
  };

  const int rectX = square.left() + scaled(12.0 / 25.0);
  const int rectY = square.top() + scaled(4.0 / 25.0);
  const int rectW = std::max(1, scaled(3.0 / 25.0));
  const int rectH = std::max(1, scaled(14.0 / 25.0));
  painter.drawRect(rectX, rectY, rectW, rectH);

  const int dotX = square.left() + scaled(12.0 / 25.0);
  const int dotY = square.top() + scaled(20.0 / 25.0);
  const int dotW = std::max(1, scaled(3.0 / 25.0));
  const int dotH = std::max(1, scaled(3.0 / 25.0));
  painter.drawEllipse(QRect(dotX, dotY, dotW, dotH));

  painter.restore();
}

void ShellCommandElement::paintSelectionOverlay(QPainter &painter) const
{
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
  painter.restore();
}
