#include "statistics_window.h"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <algorithm>

#include "pv_channel_manager.h"

namespace {
constexpr int kUpdateIntervalMs = 5000;
constexpr int kMaxPvTableRows = 500;  /* Limit to prevent excessive UI */
constexpr int kPvDetailsMinWidth = 600;
constexpr int kPvDetailsMinHeight = 400;
constexpr int kPvDetailsMaxHeightFraction = 80;  /* % of screen height */

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
  , basePalette_(palette)
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

  /* Create scroll area for PV details table (hidden initially) */
  scrollArea_ = new QScrollArea(this);
  scrollArea_->setWidgetResizable(true);
  scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea_->setVisible(false);

  pvTable_ = new QTableWidget(this);
  pvTable_->setColumnCount(5);
  pvTable_->setHorizontalHeaderLabels({
      QStringLiteral("PV Name"),
      QStringLiteral("Connected"),
      QStringLiteral("Writable"),
      QStringLiteral("Update Rate"),
      QStringLiteral("Severity")
  });
  pvTable_->setFont(contentFont_);
  pvTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  pvTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  pvTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  pvTable_->setAlternatingRowColors(true);
  pvTable_->horizontalHeader()->setStretchLastSection(true);
  pvTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  pvTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  pvTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  pvTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  pvTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  pvTable_->verticalHeader()->setVisible(false);
  pvTable_->setSortingEnabled(true);

  scrollArea_->setWidget(pvTable_);
  layout->addWidget(scrollArea_, 1);

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
  PvChannelManager::instance().resetUpdateCounters();
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

  switch (mode_) {
  case StatisticsMode::kInterval:
    updateIntervalDisplay(lastSnapshot_);
    break;
  case StatisticsMode::kAverage:
    updateAverageDisplay();
    break;
  case StatisticsMode::kPvDetails:
    updatePvDetailsDisplay();
    break;
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

void StatisticsWindow::updatePvDetailsDisplay()
{
  if (!pvTable_) {
    return;
  }

  QList<ChannelSummary> summaries =
      PvChannelManager::instance().channelSummaries();

  /* Limit number of rows to prevent UI issues */
  const int rowCount = std::min(static_cast<int>(summaries.size()), kMaxPvTableRows);

  /* Block signals during update to prevent sorting issues */
  pvTable_->setSortingEnabled(false);
  pvTable_->setRowCount(rowCount);

  for (int i = 0; i < rowCount; ++i) {
    const ChannelSummary &summary = summaries.at(i);

    /* PV Name */
    auto *nameItem = new QTableWidgetItem(summary.pvName);
    nameItem->setFont(contentFont_);
    nameItem->setForeground(Qt::black);
    pvTable_->setItem(i, 0, nameItem);

    /* Connected status */
    QString connectedText = summary.connected
        ? QStringLiteral("Yes")
        : QStringLiteral("No");
    auto *connectedItem = new QTableWidgetItem(connectedText);
    connectedItem->setFont(contentFont_);
    connectedItem->setTextAlignment(Qt::AlignCenter);
    if (!summary.connected) {
      connectedItem->setForeground(Qt::red);
    } else {
      connectedItem->setForeground(Qt::darkGreen);
    }
    pvTable_->setItem(i, 1, connectedItem);

    /* Writable status */
    QString writableText = summary.writable
        ? QStringLiteral("Yes")
        : QStringLiteral("No");
    auto *writableItem = new QTableWidgetItem(writableText);
    writableItem->setFont(contentFont_);
    writableItem->setTextAlignment(Qt::AlignCenter);
    if (summary.writable) {
      writableItem->setForeground(Qt::darkGreen);
    } else {
      writableItem->setForeground(Qt::black);
    }
    pvTable_->setItem(i, 2, writableItem);

    /* Update rate */
    QString rateText = QString::asprintf("%.2f Hz", summary.updateRate);
    auto *rateItem = new QTableWidgetItem(rateText);
    rateItem->setFont(contentFont_);
    rateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rateItem->setForeground(Qt::black);
    /* Store numeric value for proper sorting */
    rateItem->setData(Qt::UserRole, summary.updateRate);
    pvTable_->setItem(i, 3, rateItem);

    /* Severity */
    QString severityText;
    QColor severityColor = Qt::black;
    switch (summary.severity) {
    case 0:  /* NO_ALARM */
      severityText = QStringLiteral("OK");
      severityColor = Qt::darkGreen;
      break;
    case 1:  /* MINOR_ALARM */
      severityText = QStringLiteral("MINOR");
      severityColor = QColor(0xC0, 0xC0, 0x00);  /* Dark yellow */
      break;
    case 2:  /* MAJOR_ALARM */
      severityText = QStringLiteral("MAJOR");
      severityColor = Qt::red;
      break;
    case 3:  /* INVALID_ALARM */
      severityText = QStringLiteral("INVALID");
      severityColor = Qt::magenta;
      break;
    default:
      severityText = QStringLiteral("?");
      break;
    }
    auto *severityItem = new QTableWidgetItem(severityText);
    severityItem->setFont(contentFont_);
    severityItem->setTextAlignment(Qt::AlignCenter);
    severityItem->setForeground(severityColor);
    /* Store numeric value for proper sorting */
    severityItem->setData(Qt::UserRole, summary.severity);
    pvTable_->setItem(i, 4, severityItem);
  }

  pvTable_->setSortingEnabled(true);

  /* Update window title to show count */
  if (summaries.size() > kMaxPvTableRows) {
    setWindowTitle(QStringLiteral("MEDM Statistics - PV Details (showing %1 of %2)")
        .arg(kMaxPvTableRows).arg(summaries.size()));
  } else {
    setWindowTitle(QStringLiteral("MEDM Statistics - PV Details (%1 PVs)")
        .arg(summaries.size()));
  }
}

void StatisticsWindow::toggleMode()
{
  switch (mode_) {
  case StatisticsMode::kInterval:
    mode_ = StatisticsMode::kAverage;
    label_->setVisible(true);
    scrollArea_->setVisible(false);
    setWindowTitle(QStringLiteral("MEDM Statistics Window"));
    updateAverageDisplay();
    break;
  case StatisticsMode::kAverage:
    mode_ = StatisticsMode::kPvDetails;
    label_->setVisible(false);
    scrollArea_->setVisible(true);
    PvChannelManager::instance().resetUpdateCounters();
    updatePvDetailsDisplay();
    break;
  case StatisticsMode::kPvDetails:
    mode_ = StatisticsMode::kInterval;
    label_->setVisible(true);
    scrollArea_->setVisible(false);
    setWindowTitle(QStringLiteral("MEDM Statistics Window"));
    updateIntervalDisplay(lastSnapshot_);
    break;
  }
  adjustSizeForMode();
}

void StatisticsWindow::adjustSizeForMode()
{
  if (mode_ == StatisticsMode::kPvDetails) {
    /* Calculate appropriate size based on content and screen */
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
      screen = QApplication::screens().value(0);
    }
    int maxHeight = 600;
    if (screen) {
      maxHeight = screen->availableGeometry().height()
          * kPvDetailsMaxHeightFraction / 100;
    }

    int rowCount = pvTable_->rowCount();
    int rowHeight = pvTable_->verticalHeader()->defaultSectionSize();
    int headerHeight = pvTable_->horizontalHeader()->height();
    int contentHeight = headerHeight + (rowCount * rowHeight) + 60;

    int newHeight = std::min(contentHeight, maxHeight);
    newHeight = std::max(newHeight, kPvDetailsMinHeight);

    setMinimumSize(kPvDetailsMinWidth, kPvDetailsMinHeight);
    resize(kPvDetailsMinWidth, newHeight);
  } else {
    /* Reset to default size for interval/average modes */
    setMinimumSize(320, 200);
    adjustSize();
  }
}

void StatisticsWindow::resetAverages()
{
  restartTracking();
  updateStatistics();
}
