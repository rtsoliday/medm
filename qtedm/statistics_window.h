#pragma once

#include <QDialog>

#include "statistics_tracker.h"

class QLabel;
class QPushButton;
class QTimer;
class QPalette;
class QFont;

class StatisticsWindow : public QDialog
{
public:
  StatisticsWindow(const QPalette &palette,
      const QFont &titleFont, const QFont &contentFont,
      QWidget *parent = nullptr);

  void showAndRaise();

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private:
  void restartTracking();
  void updateStatistics();
  void updateIntervalDisplay(const StatisticsSnapshot &snapshot);
  void updateAverageDisplay();
  void toggleMode();
  void resetAverages();

  QLabel *label_ = nullptr;
  QPushButton *modeButton_ = nullptr;
  QTimer *timer_ = nullptr;
  bool averageMode_ = false;
  StatisticsSnapshot lastSnapshot_;
  double totalElapsedSeconds_ = 0.0;
  double totalCaEvents_ = 0.0;
  double totalUpdateRequested_ = 0.0;
  double totalUpdateDiscarded_ = 0.0;
  double totalUpdateExecuted_ = 0.0;
  QFont contentFont_;
};
