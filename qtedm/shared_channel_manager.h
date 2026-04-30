#pragma once

#include <atomic>
#include <mutex>

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include "channel_subscription.h"

class SoftPvRegistry;

/* Singleton manager for shared EPICS Channel Access connections.
 *
 * Multiple widgets monitoring the same PV (with the same DBR type and
 * element count) share a single CA channel. Different DBR types or
 * element counts for the same PV name create separate channels.
 *
 * Usage:
 *   auto &mgr = SharedChannelManager::instance();
 *   auto handle = mgr.subscribe("PV:NAME", DBR_TIME_DOUBLE, 1,
 *       [this](const SharedChannelData &data) { handleValue(data); },
 *       [this](bool conn) { handleConnection(conn); });
 *   // ... later ...
 *   handle.reset();  // or let handle go out of scope
 */
class SharedChannelManager : public QObject, public SubscriptionOwner
{
  Q_OBJECT

public:
  static SharedChannelManager &instance();
  static SharedChannelManager *instanceIfExists();

  /* Subscribe to a channel.
   *
   * @param pvName        PV name to connect to
   * @param requestedType DBR type for subscription (e.g., DBR_TIME_DOUBLE)
   * @param elementCount  Number of array elements (0 = use native count)
   * @param valueCallback Called when a new value arrives
   * @param connectionCallback Called when connection state changes (optional)
   * @param accessRightsCallback Called when access rights change (optional)
   * @return Handle to manage the subscription lifetime
   *
   * If another subscriber already has a channel with the same key
   * (pvName + requestedType + elementCount), they share the same
   * CA channel. If already connected, the callbacks fire immediately
   * with the cached data. */
  SubscriptionHandle subscribe(
      const QString &pvName,
      chtype requestedType,
      long elementCount,
      ChannelValueCallback valueCallback,
      ChannelConnectionCallback connectionCallback = nullptr,
      ChannelAccessRightsCallback accessRightsCallback = nullptr);

  /* Perform a ca_put operation through a shared channel.
   * Creates a temporary channel if none exists. */
  bool putValue(const QString &pvName, double value);
  bool putValue(const QString &pvName, const QString &value);
  bool putValue(const QString &pvName, dbr_enum_t value);
  bool putCharArrayValue(const QString &pvName, const QByteArray &value);
  bool putArrayValue(const QString &pvName, const QVector<double> &values);

  /* Get current statistics */
  int uniqueChannelCount() const;
  int totalSubscriptionCount() const;
  int connectedChannelCount() const;

  /* Get detailed channel information for statistics display.
   * Returns a list of ChannelSummary sorted by PV name. */
  QList<ChannelSummary> channelSummaries() const;

  /* Reset update counters for all channels (for rate calculation) */
  void resetUpdateCounters();

  /* Get elapsed time since last reset (for rate calculation) */
  double elapsedSecondsSinceReset() const;

private:
  friend class SubscriptionHandle;

  SharedChannelManager();
  ~SharedChannelManager() override;

  void unsubscribe(quint64 subscriptionId) override;

  /* Internal structures */
  struct Subscriber
  {
    quint64 id = 0;
    ChannelValueCallback valueCallback;
    ChannelConnectionCallback connectionCallback;
    ChannelAccessRightsCallback accessRightsCallback;
  };

  struct SharedChannel
  {
    SharedChannelKey key;
    quint64 instanceId = 0;
    chid channelId = nullptr;
    evid subscriptionId = nullptr;
    bool connected = false;
    bool subscribed = false;
    bool controlInfoRequested = false;
    bool canRead = false;
    bool canWrite = false;
    SharedChannelData cachedData;
    QList<Subscriber> subscribers;
    int updateCount = 0;  /* Updates since last reset for rate calc */
    qint64 lastNotifyTimeMs = 0;  /* Time of last subscriber notification */
    bool notifyPending = false;  /* Deferred notify scheduled */
    int dispatchDepth = 0;  /* Active callback dispatches using this channel */
    bool destroyPending = false;  /* Delay destruction until dispatch completes */
    /* Last notified values for change detection */
    double lastNotifiedValue = 0.0;
    short lastNotifiedSeverity = -1;  /* -1 = never notified */
    QString lastNotifiedString;
    dbr_enum_t lastNotifiedEnum = 0;
  };

  /* CA callbacks - static to match CA API */
  static void connectionCallback(struct connection_handler_args args);
  static void valueCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);
  static void accessRightsCallback(struct access_rights_handler_args args);

  /* Internal handlers */
  void handleConnection(SharedChannel *channel, bool connected);
  void handleValue(SharedChannel *channel, const event_handler_args &args);
  void handleControlInfo(SharedChannel *channel, const event_handler_args &args);
  void handleAccessRights(SharedChannel *channel, bool canRead, bool canWrite);
  void notifySubscribersForLocalNumericPut(SharedChannel *channel, double value);
  void scheduleInitialDelivery(quint64 subscriptionId);
  void deliverInitialState(quint64 subscriptionId);
  Subscriber *findSubscriber(SharedChannel *channel, quint64 subscriptionId);
  void dispatchConnectionCallbacks(SharedChannel *channel, bool connected);
  void dispatchValueCallbacks(SharedChannel *channel);
  void dispatchAccessRightsCallbacks(SharedChannel *channel,
      bool canRead, bool canWrite);

  /* Channel management */
  SharedChannel *findChannelByInstanceId(quint64 channelInstanceId) const;
  SharedChannel *findOrCreateChannel(const SharedChannelKey &key);
  void destroyChannelIfUnused(SharedChannel *channel);
  void subscribeToChannel(SharedChannel *channel);
  void requestControlInfo(SharedChannel *channel);
  void shutdown();

  /* Deferred flush mechanism - batches CA operations to avoid blocking */
  void scheduleDeferredFlush();
  void performDeferredFlush();
  void scheduleConnectionCompletionReport();
  void reportConnectionCompletion();

private Q_SLOTS:
  /* Thread-safe slots for processing CA callbacks from the CA thread.
   * These are invoked via QueuedConnection to marshal data to main thread. */
  void onConnectionChanged(quint64 channelInstanceId, bool connected,
                           short nativeType, long nativeCount);
  void onValueReceived(quint64 channelInstanceId, QByteArray eventData, int status,
                       long type, long count);
  void onDeferredValueNotify(quint64 channelInstanceId);
  void onControlInfoReceived(quint64 channelInstanceId, QByteArray eventData,
                             int status, long type);
  void onAccessRightsChanged(quint64 channelInstanceId, bool canRead,
      bool canWrite);

private:
  /* Data */
  mutable std::mutex channelMutex_;  /* Protects channel access from CA thread */
  QHash<SharedChannelKey, SharedChannel *> channels_;
  QHash<quint64, SharedChannel *> instanceIdToChannel_;
  bool flushScheduled_ = false;
  QHash<quint64, SharedChannel *> subscriptionToChannel_;
  quint64 nextChannelInstanceId_ = 1;
  quint64 nextSubscriptionId_ = 1;
  QElapsedTimer updateRateTimer_;
  bool updateRateTimerStarted_ = false;
  bool firstConnectionReported_ = false;
  bool firstValueReported_ = false;
  int totalConnectionsMade_ = 0;
  int totalValuesReceived_ = 0;
  QString lastConnectedPvName_;
  QString lastValuePvName_;
  bool lastConnectionReported_ = false;
  bool lastValueReported_ = false;
  /* Milestone tracking for timing diagnostics */
  int expectedChannelCount_ = 0;
  bool connection10Reported_ = false;
  bool connection25Reported_ = false;
  bool connection50Reported_ = false;
  bool connection75Reported_ = false;
  bool connection90Reported_ = false;
  bool value10Reported_ = false;
  bool value25Reported_ = false;
  bool value50Reported_ = false;
  bool value75Reported_ = false;
  bool value90Reported_ = false;
  QTimer connectionCompletionTimer_;
  int lastConnectionCompletionCount_ = -1;
  int lastConnectionCompletionTotal_ = -1;
  std::atomic<bool> acceptingCallbacks_{false};
  bool shutdownComplete_ = false;
};
