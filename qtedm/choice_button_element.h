#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <QRect>

#include <functional>

#include "display_properties.h"

class QAbstractButton;
class QButtonGroup;

class ChoiceButtonElement : public QWidget
{
public:
  explicit ChoiceButtonElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  ChoiceButtonStacking stacking() const;
  void setStacking(ChoiceButtonStacking stacking);

  QString channel() const;
  void setChannel(const QString &channel);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeSeverity(short severity);
  void setRuntimeWriteAccess(bool writeAccess);
  void setRuntimeLabels(const QStringList &labels);
  void setRuntimeValue(int value);

  void setActivationCallback(const std::function<void(int)> &callback);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  void paintSelectionOverlay(QPainter &painter) const;
  void ensureButtonGroup();
  void clearButtons();
  void rebuildButtons();
  void layoutButtons();
  void updateButtonPalettes();
  void updateButtonEnabledState();
  void applyButtonFont(QAbstractButton *button, const QRect &bounds) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  ChoiceButtonStacking stacking_ = ChoiceButtonStacking::kRow;
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  int runtimeValue_ = -1;
  QStringList runtimeLabels_;
  QButtonGroup *buttonGroup_ = nullptr;
  QVector<QAbstractButton *> buttons_;
  std::function<void(int)> activationCallback_;
};
