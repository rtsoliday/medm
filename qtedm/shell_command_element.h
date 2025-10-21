#pragma once

#include <array>
#include <functional>

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;
class QPainter;
class QMouseEvent;

class ShellCommandElement : public QWidget
{
public:
  explicit ShellCommandElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  QString label() const;
  void setLabel(const QString &label);

  int entryCount() const;

  ShellCommandEntry entry(int index) const;
  void setEntry(int index, const ShellCommandEntry &entry);

  QString entryLabel(int index) const;
  void setEntryLabel(int index, const QString &label);

  QString entryCommand(int index) const;
  void setEntryCommand(int index, const QString &command);

  QString entryArgs(int index) const;
  void setEntryArgs(int index, const QString &args);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setActivationCallback(
      const std::function<void(int, Qt::KeyboardModifiers)> &callback);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QString displayLabel(bool &showIcon) const;
  int activeEntryCount() const;
  int activatableEntryCount() const;
  bool entryHasCommand(int index) const;
  int firstActivatableEntry() const;
  void showMenu(Qt::KeyboardModifiers modifiers);
  void paintIcon(QPainter &painter, const QRect &rect) const;
  void paintSelectionOverlay(QPainter &painter) const;

  bool selected_ = false;
  bool executeMode_ = false;
  int pressedEntryIndex_ = -1;
  std::function<void(int, Qt::KeyboardModifiers)> activationCallback_;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString label_;
  std::array<ShellCommandEntry, kShellCommandEntryCount> entries_{};
};
