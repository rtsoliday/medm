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
  timer_->setInterval(updateIntervalMs_);
  QObject::connect(timer_, &QTimer::timeout, [this]() {
    processPendingUpdates();
  });
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
  if (intervalMs < 10) {
    intervalMs = 10;  // Minimum 10ms (100Hz max)
  }
  updateIntervalMs_ = intervalMs;
  if (timer_) {
    timer_->setInterval(updateIntervalMs_);
  }
}

int UpdateCoordinator::updateInterval() const
{
  return updateIntervalMs_;
}

void UpdateCoordinator::processPendingUpdates()
{
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
