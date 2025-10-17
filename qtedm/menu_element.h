#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

#include "display_properties.h"

class QComboBox;
class QPaintEvent;
class QResizeEvent;

class MenuElement : public QWidget
{
public:
  explicit MenuElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

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
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QColor effectiveForegroundColor() const;
  QColor effectiveBackgroundColor() const;
  void applyPaletteColors();
  void updateSelectionVisual();
  void populateSampleItems();
  void updateComboBoxEnabledState();
  void updateComboBoxCursor();
  void updateComboBoxFont();

  bool selected_ = false;
  QComboBox *comboBox_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  int runtimeValue_ = -1;
  QStringList runtimeLabels_;
  std::function<void(int)> activationCallback_;
};

