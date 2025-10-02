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
      std::function<void()> changeNotifier);
  void showForTextMonitor();
  void setMeterCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier);
  void showForMeter();
  void setBarCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier);
  void showForBarMonitor();
  void setScaleCallbacks(const QString &channelName,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<void()> changeNotifier);
  void showForScaleMonitor();

private:
  enum class Mode {
    kNone,
    kTextMonitor,
    kMeter,
    kBarMonitor,
    kScaleMonitor
  };

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
  std::function<void()> onChangedCallback_;
  bool updating_ = false;
  QString channelLabel_;
};
