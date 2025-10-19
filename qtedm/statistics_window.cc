#include "statistics_window.h"

#include <QFont>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <algorithm>

namespace {
constexpr int kUpdateIntervalMs = 5000;

QString formatIntervalText(const StatisticsSnapshot &snapshot)
{
  return QString::asprintf(
      "Time Interval (sec)       = %8.2f\n"
      "CA Channels               = %8d\n"
      "CA Channels Connected     = %8d\n"
      "CA Incoming Events        = %8d\n"
      "MEDM Objects Updating     = %8d\n"
      "MEDM Objects Updated      = %8d\n"
      "Update Requests           = %8d\n"
      "Update Requests Discarded = %8d\n"
      "Update Requests Queued    = %8d\n",
      snapshot.intervalSeconds,
      snapshot.channelCount,
      snapshot.channelConnected,
      snapshot.caEventCount,
      snapshot.objectCount,
      snapshot.updateExecuted,
      snapshot.updateRequestCount,
      snapshot.updateDiscardCount,
      snapshot.updateRequestQueued);
}

QString formatAverageText(double elapsedSeconds,
    double caEvents, double updatesExecuted,
    double updatesRequested, double updatesDiscarded)
{
  const double safeElapsed = std::max(elapsedSeconds, 0.0);
  const double caRate = (safeElapsed > 0.0)
      ? (caEvents / safeElapsed) : 0.0;
  const double executedRate = (safeElapsed > 0.0)
      ? (updatesExecuted / safeElapsed) : 0.0;
  const double requestedRate = (safeElapsed > 0.0)
      ? (updatesRequested / safeElapsed) : 0.0;
  const double discardedRate = (safeElapsed > 0.0)
      ? (updatesDiscarded / safeElapsed) : 0.0;

  return QString::asprintf(
      "AVERAGES\n\n"
      "CA Incoming Events        = %8.1f\n"
      "MEDM Objects Updated      = %8.1f\n"
      "Update Requests           = %8.1f\n"
      "Update Requests Discarded = %8.1f\n\n"
      "Total Time Elapsed        = %8.1f\n",
      caRate,
      executedRate,
      requestedRate,
      discardedRate,
      safeElapsed);
}
}

StatisticsWindow::StatisticsWindow(const QPalette &palette,
    const QFont &titleFont, const QFont &contentFont,
    QWidget *parent)
  : QDialog(parent)
  , contentFont_(contentFont)
{
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowTitle(QStringLiteral("MEDM Statistics Window"));
  setPalette(palette);
  setAutoFillBackground(true);
  setFont(titleFont);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  label_ = new QLabel(this);
  label_->setObjectName(QStringLiteral("statisticsLabel"));
  label_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  label_->setWordWrap(true);
  label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label_->setFont(contentFont_);
  label_->setMinimumWidth(320);
  layout->addWidget(label_);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->setSpacing(12);

  auto *closeButton = new QPushButton(QStringLiteral("Close"), this);
  closeButton->setFont(titleFont);
  QObject::connect(closeButton, &QPushButton::clicked,
      this, &StatisticsWindow::close);

  auto *resetButton = new QPushButton(QStringLiteral("Reset"), this);
  resetButton->setFont(titleFont);
  QObject::connect(resetButton, &QPushButton::clicked,
      this, &StatisticsWindow::resetAverages);

  modeButton_ = new QPushButton(QStringLiteral("Mode"), this);
  modeButton_->setFont(titleFont);
  QObject::connect(modeButton_, &QPushButton::clicked,
      this, &StatisticsWindow::toggleMode);

  buttonRow->addStretch();
  buttonRow->addWidget(closeButton);
  buttonRow->addWidget(resetButton);
  buttonRow->addWidget(modeButton_);
  buttonRow->addStretch();

  layout->addLayout(buttonRow);

  timer_ = new QTimer(this);
  timer_->setInterval(kUpdateIntervalMs);
  QObject::connect(timer_, &QTimer::timeout,
      this, &StatisticsWindow::updateStatistics);

  updateIntervalDisplay(lastSnapshot_);
}

void StatisticsWindow::showAndRaise()
{
  show();
  raise();
  activateWindow();
}

void StatisticsWindow::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  restartTracking();
  updateStatistics();
  if (timer_) {
    timer_->start();
  }
}

void StatisticsWindow::hideEvent(QHideEvent *event)
{
  if (timer_) {
    timer_->stop();
  }
  QDialog::hideEvent(event);
}

void StatisticsWindow::restartTracking()
{
  StatisticsTracker::instance().reset();
  totalElapsedSeconds_ = 0.0;
  totalCaEvents_ = 0.0;
  totalUpdateRequested_ = 0.0;
  totalUpdateDiscarded_ = 0.0;
  totalUpdateExecuted_ = 0.0;
  lastSnapshot_ = StatisticsSnapshot{};
}

void StatisticsWindow::updateStatistics()
{
  lastSnapshot_ = StatisticsTracker::instance().snapshotAndReset();
  if (lastSnapshot_.intervalSeconds < 0.0) {
    lastSnapshot_.intervalSeconds = 0.0;
  }
  totalElapsedSeconds_ += lastSnapshot_.intervalSeconds;
  totalCaEvents_ += lastSnapshot_.caEventCount;
  totalUpdateRequested_ += lastSnapshot_.updateRequestCount;
  totalUpdateDiscarded_ += lastSnapshot_.updateDiscardCount;
  totalUpdateExecuted_ += lastSnapshot_.updateExecuted;

  if (averageMode_) {
    updateAverageDisplay();
  } else {
    updateIntervalDisplay(lastSnapshot_);
  }
}

void StatisticsWindow::updateIntervalDisplay(
    const StatisticsSnapshot &snapshot)
{
  if (!label_) {
    return;
  }
  label_->setText(formatIntervalText(snapshot));
}

void StatisticsWindow::updateAverageDisplay()
{
  if (!label_) {
    return;
  }
  label_->setText(formatAverageText(totalElapsedSeconds_,
      totalCaEvents_, totalUpdateExecuted_,
      totalUpdateRequested_, totalUpdateDiscarded_));
}

void StatisticsWindow::toggleMode()
{
  averageMode_ = !averageMode_;
  if (averageMode_) {
    updateAverageDisplay();
  } else {
    updateIntervalDisplay(lastSnapshot_);
  }
}

void StatisticsWindow::resetAverages()
{
  restartTracking();
  updateStatistics();
}
