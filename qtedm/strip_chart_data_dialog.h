#pragma once

#include <array>
#include <functional>

#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "display_properties.h"

class ColorPaletteDialog;
class StripChartElement;

/* Dialog for editing Strip Chart data at runtime, similar to medm's
 * Strip Chart Data dialog. Allows editing pen colors, limits, and
 * the chart period/units. */
class StripChartDataDialog : public QDialog
{
public:
  StripChartDataDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr);

  void setTarget(StripChartElement *element);
  void clearTarget();

private:
  static constexpr int kMaxPens = 8;

  struct PenRow {
    QLabel *channelLabel = nullptr;
    QPushButton *colorButton = nullptr;
    QComboBox *loprSourceCombo = nullptr;
    QDoubleSpinBox *loprValueSpin = nullptr;
    QComboBox *hoprSourceCombo = nullptr;
    QDoubleSpinBox *hoprValueSpin = nullptr;
  };

  void setupUi();
  void populateFromElement();
  void applyChanges();
  void resetToOriginal();

  void openColorPalette(int penIndex);
  void updateColorButton(int penIndex, const QColor &color);
  void updateLoprValueEnabled(int penIndex);
  void updateHoprValueEnabled(int penIndex);

  QFont labelFont_;
  QFont valueFont_;

  QPointer<StripChartElement> element_;
  QPointer<ColorPaletteDialog> colorPaletteDialog_;
  int activeColorPenIndex_ = -1;

  /* UI elements */
  std::array<PenRow, kMaxPens> penRows_;
  QDoubleSpinBox *periodSpin_ = nullptr;
  QComboBox *unitsCombo_ = nullptr;
  QPushButton *applyButton_ = nullptr;
  QPushButton *cancelButton_ = nullptr;
  QPushButton *closeButton_ = nullptr;

  /* Original values for reset */
  struct OriginalPenData {
    QColor color;
    PvLimits limits;
  };
  std::array<OriginalPenData, kMaxPens> originalPenData_;
  double originalPeriod_ = 60.0;
  TimeUnits originalUnits_ = TimeUnits::kSeconds;
};
