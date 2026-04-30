#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <cadef.h>
#include <db_access.h>

class PvaChannelManager;
class SharedChannelManager;
class SoftPvRegistry;

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
  QStringList stringArrayValue;
  dbr_enum_t enumValue = 0;
  QVector<double> arrayValues;
  QByteArray charArrayValue;
  std::shared_ptr<const double> sharedArrayData;
  size_t sharedArraySize = 0;

  /* Alarm information */
  short severity = 0;
  short status = 0;
  epicsTimeStamp timestamp{};
  bool hasTimestamp = false;

  /* Control information (from DBR_CTRL_* requests) */
  double hopr = 0.0;
  double lopr = 0.0;
  short precision = -1;
  QString units;
  QStringList enumStrings;
  bool hasControlInfo = false;
  bool hasUnits = false;
  bool hasPrecision = false;

  /* Flags indicating what data is valid */
  bool hasValue = false;
  bool isNumeric = false;
  bool isString = false;
  bool isEnum = false;
  bool isCharArray = false;
  bool isArray = false;
};

/* Protocol-agnostic interface for subscription ownership. */
class SubscriptionOwner
{
public:
  virtual ~SubscriptionOwner() = default;
  virtual void unsubscribe(quint64 subscriptionId) = 0;
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
  friend class PvaChannelManager;
  friend class SharedChannelManager;
  friend class SoftPvRegistry;
  friend class SubscriptionOwner;
  explicit SubscriptionHandle(quint64 id, SubscriptionOwner *owner);

  quint64 id_ = 0;
  SubscriptionOwner *owner_ = nullptr;
};

/* Callback types for channel events.
 * Note: Connection callback receives SharedChannelData reference to provide
 * native type info immediately upon connection, before any value arrives. */
using ChannelValueCallback = std::function<void(const SharedChannelData &)>;
using ChannelConnectionCallback =
    std::function<void(bool connected, const SharedChannelData &data)>;
using ChannelAccessRightsCallback =
    std::function<void(bool canRead, bool canWrite)>;
