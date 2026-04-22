#pragma once

#include <array>

#include <QColor>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;

class ExpressionChannelElement : public QWidget
{
public:
  explicit ExpressionChannelElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QString variable() const;
  void setVariable(const QString &variable);
  QString resolvedVariableName();

  QString calc() const;
  void setCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &channel);

  double initialValue() const;
  void setInitialValue(double value);

  ExpressionChannelEventSignalMode eventSignalMode() const;
  void setEventSignalMode(ExpressionChannelEventSignalMode mode);

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  int precision() const;
  void setPrecision(int precision);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setVisible(bool visible) override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  void updateToolTip();

  bool selected_ = false;
  QString variable_;
  QString resolvedVariableName_;
  QString calc_;
  std::array<QString, 4> channels_{};
  double initialValue_ = 0.0;
  ExpressionChannelEventSignalMode eventSignalMode_ =
      ExpressionChannelEventSignalMode::kOnAnyChange;
  QColor foregroundColor_;
  QColor backgroundColor_;
  int precision_ = 0;
  bool executeMode_ = false;
  bool designModeVisible_ = true;
};
