#pragma once

#include <array>
#include <functional>

#include <QDialog>
#include <QFont>
#include <QPalette>

#include "display_properties.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class CartesianAxisDialog : public QDialog
{
public:
  CartesianAxisDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr);

  void clearCallbacks();
  void setCartesianCallbacks(
      std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> styleGetters,
      std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> styleSetters,
      std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> rangeGetters,
      std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> rangeSetters,
      std::array<std::function<double()>, kCartesianAxisCount> minimumGetters,
      std::array<std::function<void(double)>, kCartesianAxisCount> minimumSetters,
      std::array<std::function<double()>, kCartesianAxisCount> maximumGetters,
      std::array<std::function<void(double)>, kCartesianAxisCount> maximumSetters,
      std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> timeFormatGetters,
      std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> timeFormatSetters,
      std::function<void()> changeNotifier);

  void showDialog();

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  void handleAxisChanged(int index);
  void handleAxisStyleChanged(int index);
  void handleRangeStyleChanged(int index);
  void handleTimeFormatChanged(int index);
  void commitMinimum();
  void commitMaximum();
  void refreshForAxis(int axisIndex);
  void updateControlStates();

  QFont labelFont_;
  QFont valueFont_;
  QComboBox *axisCombo_ = nullptr;
  QComboBox *axisStyleCombo_ = nullptr;
  QComboBox *rangeStyleCombo_ = nullptr;
  QLineEdit *minEdit_ = nullptr;
  QLineEdit *maxEdit_ = nullptr;
  QComboBox *timeFormatCombo_ = nullptr;
  QLabel *timeFormatLabel_ = nullptr;
  QPushButton *closeButton_ = nullptr;

  std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> styleGetters_{};
  std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> styleSetters_{};
  std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> rangeGetters_{};
  std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> rangeSetters_{};
  std::array<std::function<double()>, kCartesianAxisCount> minimumGetters_{};
  std::array<std::function<void(double)>, kCartesianAxisCount> minimumSetters_{};
  std::array<std::function<double()>, kCartesianAxisCount> maximumGetters_{};
  std::array<std::function<void(double)>, kCartesianAxisCount> maximumSetters_{};
  std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> timeFormatGetters_{};
  std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> timeFormatSetters_{};
  std::function<void()> changeNotifier_;
  int currentAxisIndex_ = 0;
  bool updating_ = false;
};
