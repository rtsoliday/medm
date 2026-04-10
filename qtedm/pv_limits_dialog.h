#pragma once

#include <functional>

#include <QDialog>
#include <QFont>
#include <QString>

#include "display_properties.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class PvLimitsDialog : public QDialog
{
public:
  PvLimitsDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr);

  void clearTargets();
  void setTextMonitorCallbacks(const QString &channelName,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<void()> changeNotifier = {},
      std::function<PvLimits()> limitsGetter = {},
      std::function<void(const PvLimits &)> limitsSetter = {},
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {},
      bool allowUserSources = false);
  void showForTextMonitor();
  void setTextEntryCallbacks(const QString &channelName,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<void()> changeNotifier = {},
      std::function<PvLimits()> limitsGetter = {},
      std::function<void(const PvLimits &)> limitsSetter = {},
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForTextEntry();
  void setMeterCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForMeter();
  void setStripChartCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForStripChart();
  void setSliderCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForSlider();
  void setWheelSwitchCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForWheelSwitch();
  void setBarCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForBarMonitor();
  void setThermometerCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForThermometer();
  void setScaleCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier,
      std::function<double()> lowValueGetter = {},
      std::function<double()> highValueGetter = {});
  void showForScaleMonitor();

private:
  enum class Mode {
    kNone,
    kTextMonitor,
    kTextEntry,
    kMeter,
    kStripChart,
    kSlider,
    kWheelSwitch,
    kBarMonitor,
    kThermometer,
    kScaleMonitor
  };

  bool userSourcesAllowed() const;
  bool modeSupportsPrecision() const;
  bool modeSupportsLimits() const;
  void updatePrecisionControls();
  void updateMeterControls();
  void handlePrecisionSourceChanged(int index);
  void commitPrecisionValue();
  void handleLowSourceChanged(int index);
  void handleHighSourceChanged(int index);
  void commitLowValue();
  void commitHighValue();
  void notifyChanged();
  void setRowEnabled(QLabel *label, QComboBox *combo, QLineEdit *edit,
      bool enabled);
  void setLimitsRowsVisible(bool visible);
  void setPrecisionRowVisible(bool visible);

  Mode mode_ = Mode::kNone;
  QFont labelFont_;
  QFont valueFont_;
  QLabel *titleLabel_ = nullptr;
  QLabel *loprLabel_ = nullptr;
  QLabel *hoprLabel_ = nullptr;
  QLabel *precisionLabel_ = nullptr;
  QComboBox *loprSourceCombo_ = nullptr;
  QComboBox *hoprSourceCombo_ = nullptr;
  QComboBox *precisionSourceCombo_ = nullptr;
  QLineEdit *loprEdit_ = nullptr;
  QLineEdit *hoprEdit_ = nullptr;
  QLineEdit *precisionEdit_ = nullptr;
  QPushButton *closeButton_ = nullptr;
  QPushButton *helpButton_ = nullptr;
  std::function<PvLimitSource()> precisionSourceGetter_;
  std::function<void(PvLimitSource)> precisionSourceSetter_;
  std::function<int()> precisionDefaultGetter_;
  std::function<void(int)> precisionDefaultSetter_;
  std::function<PvLimits()> meterLimitsGetter_;
  std::function<void(const PvLimits &)> meterLimitsSetter_;
  std::function<double()> lowValueGetter_;
  std::function<double()> highValueGetter_;
  std::function<void()> onChangedCallback_;
  bool allowUserSources_ = false;
  bool updating_ = false;
  QString channelLabel_;
};
