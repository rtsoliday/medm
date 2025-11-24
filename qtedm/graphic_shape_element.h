#pragma once

#include <array>

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class QPainter;

class GraphicShapeElement : public QWidget
{
public:
  explicit GraphicShapeElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor color() const;
  void setForegroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);

  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeVisible(bool visible);
  void setRuntimeSeverity(short severity);

  void setVisible(bool visible) override;

protected:
  QColor defaultForegroundColor() const;
  QColor effectiveForegroundColor() const;

  void applyRuntimeVisibility();
  void updateExecuteState();
  void drawSelectionOutline(QPainter &painter, const QRect &rect) const;

  virtual void onRuntimeStateReset() {}
  virtual void onExecuteStateApplied();
  virtual void onRuntimeConnectedChanged() {}
  virtual void onRuntimeVisibilityChanged() {}
  virtual void onRuntimeSeverityChanged();
  virtual short normalizeRuntimeSeverity(short severity) const;

  bool selected_ = false;
  QColor color_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  bool executeMode_ = false;
  bool designModeVisible_ = true;
  bool runtimeConnected_ = false;
  bool runtimeVisible_ = true;
  short runtimeSeverity_ = 0;
};
