#pragma once

#include <array>

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;
class QPainter;

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

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QString displayLabel(bool &showIcon) const;
  int activeEntryCount() const;
  void paintIcon(QPainter &painter, const QRect &rect) const;
  void paintSelectionOverlay(QPainter &painter) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString label_;
  std::array<ShellCommandEntry, kShellCommandEntryCount> entries_{};
};
