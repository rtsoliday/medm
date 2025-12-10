#pragma once

#include <QVector>
#include <QTimer>
#include <QPointer>
#include <QElapsedTimer>

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
 * The coordinator also implements adaptive rate throttling similar to
 * the StripChart widget. When timer callbacks are consistently late
 * (indicating network or system load), the update interval is increased
 * to reduce load. This helps maintain smooth performance over slow or
 * variable network connections.
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
   * Default is 200ms (5Hz). Minimum is 100ms (10Hz).
   */
  void setUpdateInterval(int intervalMs);

  /**
   * @brief Get the current update interval in milliseconds.
   */
  int updateInterval() const;

  /**
   * @brief Check if adaptive throttling has increased the interval.
   * @return true if the current interval is higher than the base interval.
   */
  bool isThrottled() const;

  /**
   * @brief Reset adaptive throttling to the base interval.
   */
  void resetThrottling();

private:
  UpdateCoordinator();
  ~UpdateCoordinator();

  UpdateCoordinator(const UpdateCoordinator &) = delete;
  UpdateCoordinator &operator=(const UpdateCoordinator &) = delete;

  void processPendingUpdates();

private:
  QTimer *timer_ = nullptr;
  QVector<QPointer<QWidget>> pendingWidgets_;
  int baseIntervalMs_ = 200;     // Base interval (5Hz default)
  int currentIntervalMs_ = 200;  // Current interval (may be increased by throttling)

  // Adaptive throttling state (similar to StripChartElement)
  QElapsedTimer elapsedTimer_;
  qint64 expectedTickTimeMs_ = 0;  // When we expect the next tick
  int lateTickCount_ = 0;          // Consecutive late ticks
  int onTimeTickCount_ = 0;        // Consecutive on-time ticks (for recovery)

  // Throttling constants
  static constexpr int kMinIntervalMs = 100;         // Minimum 100ms (10Hz max)
  static constexpr int kMaxIntervalMs = 1000;        // Maximum 1000ms (1Hz min)
  static constexpr int kLateThresholdMs = 50;        // Tick is "late" if >50ms past expected
  static constexpr int kLateCountThreshold = 5;      // Increase interval after 5 late ticks
  static constexpr int kOnTimeCountThreshold = 100;  // Decrease interval after 100 on-time ticks
  static constexpr int kIntervalIncrementMs = 50;    // Increase by 50ms each time
};
