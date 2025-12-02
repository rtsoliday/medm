#include "shell_command_element.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QStyle>
#include <QStyleOptionButton>

#include "medm_colors.h"
#include "text_font_utils.h"
#include "window_utils.h"

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

void ShellCommandElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  pressedEntryIndex_ = -1;
}

bool ShellCommandElement::isExecuteMode() const
{
  return executeMode_;
}

void ShellCommandElement::setActivationCallback(
    const std::function<void(int, Qt::KeyboardModifiers)> &callback)
{
  activationCallback_ = callback;
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

  /* Compute Motif-style shadow colors for proper bevel visibility
   * even with very dark backgrounds like black */
  QColor topShadow, bottomShadow;
  MedmColors::computeShadowColors(bg, topShadow, bottomShadow);

  QRect bevelOuter = canvas.adjusted(0, 0, -1, -1);
  painter.setPen(QPen(topShadow, 1));
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.topRight());
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.bottomLeft());
  painter.setPen(QPen(bottomShadow, 1));
  painter.drawLine(bevelOuter.bottomLeft(), bevelOuter.bottomRight());
  painter.drawLine(bevelOuter.topRight(), bevelOuter.bottomRight());

  QRect bevelInner = bevelOuter.adjusted(1, 1, -1, -1);
  painter.setPen(QPen(topShadow.lighter(110), 1));
  painter.drawLine(bevelInner.topLeft(), bevelInner.topRight());
  painter.drawLine(bevelInner.topLeft(), bevelInner.bottomLeft());
  painter.setPen(QPen(bottomShadow.darker(115), 1));
  painter.drawLine(bevelInner.bottomLeft(), bevelInner.bottomRight());
  painter.drawLine(bevelInner.topRight(), bevelInner.bottomRight());

  QRect content = bevelInner.adjusted(2, 2, -1, -1);
  bool showIcon = true;
  QString text = displayLabel(showIcon);
  
  // Prepend "! " when icon would be shown
  if (showIcon) {
    text = QStringLiteral("!") + text;
  }

  const int activeCount = activeEntryCount();
  const bool singleEntry = activeCount == 1;

  // Calculate constraint using (0.90 * height) - 4, matching MEDM's messageButtonFontListIndex
  // Search from largest to smallest font, return first that fits
  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = medmMessageButtonFont(fontLimit);
  painter.setFont(labelFont);
  QFontMetricsF labelMetrics(labelFont);
  const int textWidth = std::max(0, static_cast<int>(std::ceil(labelMetrics.horizontalAdvance(text))));

  QRect textRect = content;

  if (singleEntry) {
    int layoutLeft = content.left();
    int extra = content.width() - textWidth;
    if (extra > 0) {
      layoutLeft += extra / 2;
    }
    textRect.setLeft(layoutLeft);
    textRect.setRight(layoutLeft + textWidth);
    if (textRect.width() < 0) {
      textRect.setRight(textRect.left());
    }
    textRect.setTop(content.top());
    textRect.setBottom(content.bottom());
  } else {
    textRect.setLeft(content.left() + 4);
    textRect = textRect.adjusted(0, 0, -2, 0);
  }

  painter.setPen(fg);
  painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

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
  return base;
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

int ShellCommandElement::activatableEntryCount() const
{
  int count = 0;
  const int total = entryCount();
  for (int i = 0; i < total; ++i) {
    if (entryHasCommand(i)) {
      ++count;
    }
  }
  return count;
}

bool ShellCommandElement::entryHasCommand(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return false;
  }
  return !entries_[index].command.trimmed().isEmpty();
}

int ShellCommandElement::firstActivatableEntry() const
{
  const int total = entryCount();
  for (int i = 0; i < total; ++i) {
    if (entryHasCommand(i)) {
      return i;
    }
  }
  return -1;
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

void ShellCommandElement::mousePressEvent(QMouseEvent *event)
{
  if (!event) {
    return;
  }

  // Forward left clicks to parent when PV Info picking mode is active
  if (executeMode_ && event->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
    QWidget *target = window();
    if (target) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const QPointF globalPosF = event->globalPosition();
      const QPoint globalPoint = globalPosF.toPoint();
      const QPointF localPos = target->mapFromGlobal(globalPoint);
      QMouseEvent forwarded(event->type(), localPos, localPos, globalPosF,
          event->button(), event->buttons(), event->modifiers());
#else
      const QPoint globalPoint = event->globalPos();
      const QPointF localPos = target->mapFromGlobal(globalPoint);
      QMouseEvent forwarded(event->type(), localPos, localPos,
          QPointF(globalPoint), event->button(), event->buttons(),
          event->modifiers());
#endif
      QCoreApplication::sendEvent(target, &forwarded);
      return;
    }
  }

  if (!executeMode_ || event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  pressedEntryIndex_ = -1;
  const int count = activatableEntryCount();
  if (count <= 0) {
    event->accept();
    return;
  }

  if (count == 1) {
    pressedEntryIndex_ = firstActivatableEntry();
    event->accept();
    return;
  }

  showMenu(event->modifiers());
  event->accept();
}

void ShellCommandElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (!event) {
    return;
  }

  if (!executeMode_ || event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  int index = pressedEntryIndex_;
  pressedEntryIndex_ = -1;
  if (index >= 0 && entryHasCommand(index) && rect().contains(event->pos())) {
    if (activationCallback_) {
      activationCallback_(index, event->modifiers());
    }
  }

  event->accept();
}

void ShellCommandElement::showMenu(Qt::KeyboardModifiers modifiers)
{
  QMenu menu(this);
  int labelIndex = 0;
  for (int i = 0; i < entryCount(); ++i) {
    if (!entryHasCommand(i)) {
      continue;
    }
    QString label = entryDisplayLabel(entries_[i]);
    if (label.isEmpty()) {
      label = QStringLiteral("Command %1").arg(labelIndex + 1);
    }
    QAction *action = menu.addAction(label);
    action->setData(i);
    ++labelIndex;
  }

  if (menu.actions().isEmpty()) {
    return;
  }

  const QPoint globalPos = mapToGlobal(QPoint(width() / 2, height()));
  QAction *selected = menu.exec(globalPos);
  if (!selected) {
    return;
  }

  bool ok = false;
  int index = selected->data().toInt(&ok);
  if (!ok || !entryHasCommand(index)) {
    return;
  }

  if (activationCallback_) {
    activationCallback_(index, modifiers);
  }
}
