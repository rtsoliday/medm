#pragma once

#include <functional>
#include <memory>

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cadef.h>
#include <db_access.h>

/* Forward declarations */
class SharedChannelManager;

/* Unique key identifying a specific channel configuration.
 * Different DBR types or element counts for the same PV name
 * result in different channel instances per the user's requirements. */
struct SharedChannelKey
{
  QString pvName;
  chtype requestedType;  /* DBR type requested (e.g., DBR_TIME_DOUBLE) */
  long elementCount;     /* Number of array elements (0 = native count) */

  bool operator==(const SharedChannelKey &other) const
  {
    return pvName == other.pvName
        && requestedType == other.requestedType
        && elementCount == other.elementCount;
  }
};

inline uint qHash(const SharedChannelKey &key, uint seed = 0)
{
  return qHash(key.pvName, seed) ^ qHash(key.requestedType, seed)
      ^ qHash(key.elementCount, seed);
}

/* Summary information about a channel for display in statistics views */
struct ChannelSummary
{
  QString pvName;
  bool connected = false;
  bool writable = false;
  int subscriberCount = 0;
  int updateCount = 0;      /* Updates since last reset */
  double updateRate = 0.0;  /* Updates per second */
  short severity = 0;
};

/* Data structure holding cached channel values and metadata.
 * This is delivered to subscribers on value updates. */
struct SharedChannelData
{
  /* Connection state */
  bool connected = false;

  /* Native field type from the IOC */
  short nativeFieldType = -1;
  long nativeElementCount = 0;

  /* Last received value - stored in multiple formats for flexibility */
  double numericValue = 0.0;
  QString stringValue;
  dbr_enum_t enumValue = 0;
  QVector<double> arrayValues;
  QByteArray charArrayValue;

  /* Alarm information */
  short severity = 0;
  short status = 0;
  epicsTimeStamp timestamp{};
  bool hasTimestamp = false;

  /* Control information (from DBR_CTRL_* requests) */
  double hopr = 0.0;
  double lopr = 0.0;
  short precision = -1;
  QStringList enumStrings;
  bool hasControlInfo = false;

  /* Flags indicating what data is valid */
  bool hasValue = false;
  bool isNumeric = false;
  bool isString = false;
  bool isEnum = false;
  bool isCharArray = false;
  bool isArray = false;
};

/* Handle returned when subscribing to a channel.
 * Used to unsubscribe later. Automatically cleans up if destroyed. */
class SubscriptionHandle
{
public:
  SubscriptionHandle() = default;
  ~SubscriptionHandle();

  SubscriptionHandle(const SubscriptionHandle &) = delete;
  SubscriptionHandle &operator=(const SubscriptionHandle &) = delete;

  SubscriptionHandle(SubscriptionHandle &&other) noexcept;
  SubscriptionHandle &operator=(SubscriptionHandle &&other) noexcept;

  bool isValid() const { return id_ != 0; }
  quint64 id() const { return id_; }

  /* Explicitly release the subscription */
  void reset();

private:
  friend class SharedChannelManager;
  explicit SubscriptionHandle(quint64 id, SharedChannelManager *manager);

  quint64 id_ = 0;
  SharedChannelManager *manager_ = nullptr;
};

/* Callback types for channel events.
 * Note: Connection callback receives SharedChannelData reference to provide
 * native type info immediately upon connection, before any value arrives. */
using ChannelValueCallback = std::function<void(const SharedChannelData &)>;
using ChannelConnectionCallback = std::function<void(bool connected, const SharedChannelData &data)>;
using ChannelAccessRightsCallback = std::function<void(bool canRead, bool canWrite)>;

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
class SharedChannelManager : public QObject
{

public:
  static SharedChannelManager &instance();

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

  void unsubscribe(quint64 subscriptionId);

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

  /* Channel management */
  SharedChannel *findOrCreateChannel(const SharedChannelKey &key);
  void destroyChannelIfUnused(SharedChannel *channel);
  void subscribeToChannel(SharedChannel *channel);
  void requestControlInfo(SharedChannel *channel);

  /* Data */
  QHash<SharedChannelKey, SharedChannel *> channels_;
  QHash<quint64, SharedChannel *> subscriptionToChannel_;
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
};

