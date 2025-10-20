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

class RelatedDisplayElement : public QWidget
{
public:
  explicit RelatedDisplayElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  QString label() const;
  void setLabel(const QString &label);

  RelatedDisplayVisual visual() const;
  void setVisual(RelatedDisplayVisual visual);

  int entryCount() const;
  RelatedDisplayEntry entry(int index) const;
  void setEntry(int index, const RelatedDisplayEntry &entry);

  QString entryLabel(int index) const;
  void setEntryLabel(int index, const QString &label);

  QString entryName(int index) const;
  void setEntryName(int index, const QString &name);

  QString entryArgs(int index) const;
  void setEntryArgs(int index, const QString &args);

  RelatedDisplayMode entryMode(int index) const;
  void setEntryMode(int index, RelatedDisplayMode mode);

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
  void paintMenuVisual(QPainter &painter, const QRect &content) const;
  void paintButtonVisual(QPainter &painter, const QRect &content,
      bool vertical) const;
  void paintHiddenVisual(QPainter &painter, const QRect &content) const;
  void paintSelectionOverlay(QPainter &painter) const;
  bool entryHasTarget(int index) const;
  int firstUsableEntryIndex() const;
  int buttonEntryIndexAt(const QPoint &pos) const;
  void showMenu(Qt::KeyboardModifiers modifiers);

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString label_;
  RelatedDisplayVisual visual_ = RelatedDisplayVisual::kMenu;
  std::array<RelatedDisplayEntry, kRelatedDisplayEntryCount> entries_{};
  bool executeMode_ = false;
  std::function<void(int, Qt::KeyboardModifiers)> activationCallback_;
  int pressedEntryIndex_ = -1;
};
