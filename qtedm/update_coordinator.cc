#include "update_coordinator.h"

#include <QWidget>

UpdateCoordinator &UpdateCoordinator::instance()
{
  static UpdateCoordinator instance;
  return instance;
}

UpdateCoordinator::UpdateCoordinator()
{
  timer_ = new QTimer();
  timer_->setTimerType(Qt::CoarseTimer);  // Don't need precision for batched updates
  timer_->setInterval(currentIntervalMs_);
  QObject::connect(timer_, &QTimer::timeout, [this]() {
    processPendingUpdates();
  });
  elapsedTimer_.start();
  timer_->start();
}

UpdateCoordinator::~UpdateCoordinator()
{
  if (timer_) {
    timer_->stop();
    delete timer_;
  }
}

void UpdateCoordinator::requestUpdate(QWidget *widget)
{
  if (!widget) {
    return;
  }
  // Check if widget is already in the pending list
  for (const QPointer<QWidget> &ptr : pendingWidgets_) {
    if (ptr.data() == widget) {
      return;  // Already pending
    }
  }
  pendingWidgets_.append(QPointer<QWidget>(widget));
}

void UpdateCoordinator::setUpdateInterval(int intervalMs)
{
  if (intervalMs < kMinIntervalMs) {
    intervalMs = kMinIntervalMs;
  }
  baseIntervalMs_ = intervalMs;
  // Reset current interval to base when user changes setting
  currentIntervalMs_ = intervalMs;
  lateTickCount_ = 0;
  if (timer_) {
    timer_->setInterval(currentIntervalMs_);
  }
}

int UpdateCoordinator::updateInterval() const
{
  return currentIntervalMs_;
}

bool UpdateCoordinator::isThrottled() const
{
  return currentIntervalMs_ > baseIntervalMs_;
}

void UpdateCoordinator::resetThrottling()
{
  currentIntervalMs_ = baseIntervalMs_;
  lateTickCount_ = 0;
  onTimeTickCount_ = 0;
  if (timer_) {
    timer_->setInterval(currentIntervalMs_);
  }
}

void UpdateCoordinator::processPendingUpdates()
{
  const qint64 nowMs = elapsedTimer_.elapsed();

  // Track late ticks for adaptive throttling
  // This helps smooth performance over slow/variable network connections
  if (expectedTickTimeMs_ > 0) {
    const qint64 deltaMs = nowMs - expectedTickTimeMs_;
    if (deltaMs > kLateThresholdMs) {
      ++lateTickCount_;
      onTimeTickCount_ = 0;  // Reset on-time counter when late
      if (lateTickCount_ > kLateCountThreshold) {
        // Increase interval to reduce load
        currentIntervalMs_ += kIntervalIncrementMs;
        if (currentIntervalMs_ > kMaxIntervalMs) {
          // Reset to base if we've slowed down too much
          currentIntervalMs_ = baseIntervalMs_;
        }
        lateTickCount_ = 0;
        if (timer_) {
          timer_->setInterval(currentIntervalMs_);
        }
      }
    } else {
      // Tick was on time - track consecutive on-time ticks
      lateTickCount_ = 0;
      if (currentIntervalMs_ > baseIntervalMs_) {
        ++onTimeTickCount_;
        if (onTimeTickCount_ >= kOnTimeCountThreshold) {
          // Network has been stable for a while, try speeding up
          currentIntervalMs_ -= kIntervalIncrementMs / 2;
          if (currentIntervalMs_ < baseIntervalMs_) {
            currentIntervalMs_ = baseIntervalMs_;
          }
          onTimeTickCount_ = 0;
          if (timer_) {
            timer_->setInterval(currentIntervalMs_);
          }
        }
      } else {
        onTimeTickCount_ = 0;
      }
    }
  }
  expectedTickTimeMs_ = nowMs + currentIntervalMs_;

  if (pendingWidgets_.isEmpty()) {
    return;
  }

  // Copy and clear the list to avoid issues if widgets request updates
  // during their paint events
  QVector<QPointer<QWidget>> widgetsToUpdate;
  widgetsToUpdate.swap(pendingWidgets_);

  for (const QPointer<QWidget> &widgetPtr : widgetsToUpdate) {
    if (QWidget *widget = widgetPtr.data()) {
      widget->update();
    }
  }
}
