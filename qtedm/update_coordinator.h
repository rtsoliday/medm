#pragma once

#include <QVector>
#include <QTimer>
#include <QPointer>

class QWidget;

/**
 * @brief Coordinates widget updates to reduce event loop congestion.
 *
 * Instead of each widget calling update() immediately when data changes,
 * widgets register their pending updates with this coordinator. The
 * coordinator batches all pending updates and triggers them at a fixed
 * rate (default 5Hz = 200ms), reducing event loop congestion from
 * many individual paint events.
 *
 * Widgets that need high-frequency updates (like StripChart and
 * CartesianPlot) should NOT use this coordinator and instead call
 * update() directly.
 */
class UpdateCoordinator
{
public:
  static UpdateCoordinator &instance();

  /**
   * @brief Request an update for a widget.
   *
   * The widget will be added to the pending list and updated on the
   * next coordinator tick. Multiple requests for the same widget
   * are automatically coalesced.
   */
  void requestUpdate(QWidget *widget);

  /**
   * @brief Set the update interval in milliseconds.
   * Default is 200ms (5Hz).
   */
  void setUpdateInterval(int intervalMs);

  /**
   * @brief Get the current update interval in milliseconds.
   */
  int updateInterval() const;

private:
  UpdateCoordinator();
  ~UpdateCoordinator();

  UpdateCoordinator(const UpdateCoordinator &) = delete;
  UpdateCoordinator &operator=(const UpdateCoordinator &) = delete;

  void processPendingUpdates();

private:
  QTimer *timer_ = nullptr;
  QVector<QPointer<QWidget>> pendingWidgets_;
  int updateIntervalMs_ = 200;  // 5Hz default
};
