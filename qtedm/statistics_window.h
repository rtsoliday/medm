#pragma once

#include <QDialog>

#include "statistics_tracker.h"

class QLabel;
class QPushButton;
class QTimer;
class QPalette;
class QFont;
class QScrollArea;
class QTableWidget;

enum class StatisticsMode {
  kInterval,
  kAverage,
  kPvDetails
};

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
  void updatePvDetailsDisplay();
  void toggleMode();
  void resetAverages();
  void adjustSizeForMode();

  QLabel *label_ = nullptr;
  QTableWidget *pvTable_ = nullptr;
  QScrollArea *scrollArea_ = nullptr;
  QPushButton *modeButton_ = nullptr;
  QTimer *timer_ = nullptr;
  StatisticsMode mode_ = StatisticsMode::kInterval;
  StatisticsSnapshot lastSnapshot_;
  double totalElapsedSeconds_ = 0.0;
  double totalCaEvents_ = 0.0;
  double totalUpdateRequested_ = 0.0;
  double totalUpdateDiscarded_ = 0.0;
  double totalUpdateExecuted_ = 0.0;
  QFont contentFont_;
  QPalette basePalette_;
};
