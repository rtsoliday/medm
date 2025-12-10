#include "shared_channel_manager.h"

#include <algorithm>
#include <cstring>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QTimer>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "startup_timing.h"
#include "statistics_tracker.h"

namespace {
/* Minimum interval between subscriber notifications per channel (100ms = 10Hz max).
 * This rate limits high-frequency PV updates to reduce CPU load. */
constexpr qint64 kMinNotifyIntervalMs = 100;
} // namespace

/* SubscriptionHandle implementation */

SubscriptionHandle::SubscriptionHandle(quint64 id, SharedChannelManager *manager)
  : id_(id)
  , manager_(manager)
{
}

SubscriptionHandle::~SubscriptionHandle()
{
  reset();
}

SubscriptionHandle::SubscriptionHandle(SubscriptionHandle &&other) noexcept
  : id_(other.id_)
  , manager_(other.manager_)
{
  other.id_ = 0;
  other.manager_ = nullptr;
}

SubscriptionHandle &SubscriptionHandle::operator=(SubscriptionHandle &&other) noexcept
{
  if (this != &other) {
    reset();
    id_ = other.id_;
    manager_ = other.manager_;
    other.id_ = 0;
    other.manager_ = nullptr;
  }
  return *this;
}

void SubscriptionHandle::reset()
{
  if (id_ != 0 && manager_) {
    manager_->unsubscribe(id_);
  }
  id_ = 0;
  manager_ = nullptr;
}

/* SharedChannelManager implementation */

SharedChannelManager &SharedChannelManager::instance()
{
  static SharedChannelManager *manager = []() {
    auto *mgr = new SharedChannelManager;
    return mgr;
  }();
  return *manager;
}

SharedChannelManager::SharedChannelManager()
  : QObject(QCoreApplication::instance())
{
  /* Register types for queued connections from CA thread */
  qRegisterMetaType<QByteArray>("QByteArray");
}

SharedChannelManager::~SharedChannelManager()
{
  /* Clean up all channels */
  for (auto *channel : channels_) {
    if (channel->subscriptionId) {
      ca_clear_subscription(channel->subscriptionId);
    }
    if (channel->channelId) {
      ca_clear_channel(channel->channelId);
    }
    delete channel;
  }
  channels_.clear();
  subscriptionToChannel_.clear();
}

SubscriptionHandle SharedChannelManager::subscribe(
    const QString &pvName,
    chtype requestedType,
    long elementCount,
    ChannelValueCallback valueCallback,
    ChannelConnectionCallback connectionCallback,
    ChannelAccessRightsCallback accessRightsCallback)
{
  if (pvName.trimmed().isEmpty() || !valueCallback) {
    return SubscriptionHandle();
  }

  ChannelAccessContext::instance().ensureInitialized();
  if (!ChannelAccessContext::instance().isInitialized()) {
    qWarning() << "SharedChannelManager: CA context not available";
    return SubscriptionHandle();
  }

  SharedChannelKey key;
  key.pvName = pvName.trimmed();
  key.requestedType = requestedType;
  key.elementCount = elementCount;

  SharedChannel *channel = findOrCreateChannel(key);
  if (!channel) {
    return SubscriptionHandle();
  }

  /* Create subscriber entry */
  quint64 subId = nextSubscriptionId_++;
  Subscriber sub;
  sub.id = subId;
  sub.valueCallback = std::move(valueCallback);
  sub.connectionCallback = std::move(connectionCallback);
  sub.accessRightsCallback = std::move(accessRightsCallback);

  channel->subscribers.append(sub);
  subscriptionToChannel_.insert(subId, channel);

  /* If already connected, deliver cached data immediately */
  if (channel->connected) {
    if (sub.connectionCallback) {
      sub.connectionCallback(true, channel->cachedData);
    }
    if (sub.accessRightsCallback) {
      sub.accessRightsCallback(channel->canRead, channel->canWrite);
    }
    if (channel->cachedData.hasValue) {
      sub.valueCallback(channel->cachedData);
    }
  }

  return SubscriptionHandle(subId, this);
}

void SharedChannelManager::unsubscribe(quint64 subscriptionId)
{
  auto it = subscriptionToChannel_.find(subscriptionId);
  if (it == subscriptionToChannel_.end()) {
    return;
  }

  SharedChannel *channel = it.value();
  subscriptionToChannel_.erase(it);

  /* Remove subscriber from channel */
  auto &subs = channel->subscribers;
  for (int i = 0; i < subs.size(); ++i) {
    if (subs[i].id == subscriptionId) {
      subs.removeAt(i);
      break;
    }
  }

  /* Destroy channel if no more subscribers */
  destroyChannelIfUnused(channel);
}

SharedChannelManager::SharedChannel *SharedChannelManager::findOrCreateChannel(
    const SharedChannelKey &key)
{
  auto it = channels_.find(key);
  if (it != channels_.end()) {
    return it.value();
  }

  /* Create new channel */
  auto *channel = new SharedChannel;
  channel->key = key;
  channel->cachedData.connected = false;

  QByteArray pvBytes = key.pvName.toLatin1();
  int status = ca_create_channel(
      pvBytes.constData(),
      &SharedChannelManager::connectionCallback,
      channel,
      CA_PRIORITY_DEFAULT,
      &channel->channelId);

  if (status != ECA_NORMAL) {
    qWarning() << "SharedChannelManager: ca_create_channel failed for"
               << key.pvName << ":" << ca_message(status);
    delete channel;
    return nullptr;
  }

  /* Set user pointer for access rights callback (ca_puser retrieval) */
  ca_set_puser(channel->channelId, channel);

  /* Register access rights callback */
  ca_replace_access_rights_event(channel->channelId,
      &SharedChannelManager::accessRightsCallback);

  StatisticsTracker::instance().registerChannelCreated();
  channels_.insert(key, channel);
  
  /* With preemptive callbacks, CA processes events on its own thread.
   * We just need to flush periodically to ensure requests are sent. */
  static int channelsCreatedSinceFlush = 0;
  channelsCreatedSinceFlush++;
  if (channelsCreatedSinceFlush >= 100) {
    channelsCreatedSinceFlush = 0;
    ca_flush_io();
  } else {
    scheduleDeferredFlush();
  }

  return channel;
}

void SharedChannelManager::destroyChannelIfUnused(SharedChannel *channel)
{
  if (!channel || !channel->subscribers.isEmpty()) {
    return;
  }

  /* Remove from hash */
  channels_.remove(channel->key);

  /* Clean up CA resources */
  if (channel->subscriptionId) {
    ca_clear_subscription(channel->subscriptionId);
    channel->subscriptionId = nullptr;
  }
  if (channel->channelId) {
    /* Remove access rights callback before clearing channel */
    ca_replace_access_rights_event(channel->channelId, nullptr);
    if (channel->connected) {
      StatisticsTracker::instance().registerChannelDisconnected();
    }
    ca_clear_channel(channel->channelId);
    StatisticsTracker::instance().registerChannelDestroyed();
    channel->channelId = nullptr;
  }

  ca_flush_io();
  delete channel;
}

void SharedChannelManager::subscribeToChannel(SharedChannel *channel)
{
  if (!channel || channel->subscribed || !channel->channelId) {
    return;
  }

  long count = channel->key.elementCount;
  if (count == 0) {
    count = std::max<long>(ca_element_count(channel->channelId), 1);
  }

  int status = ca_create_subscription(
      channel->key.requestedType,
      count,
      channel->channelId,
      DBE_VALUE | DBE_ALARM,
      &SharedChannelManager::valueCallback,
      channel,
      &channel->subscriptionId);

  if (status != ECA_NORMAL) {
    qWarning() << "SharedChannelManager: ca_create_subscription failed for"
               << channel->key.pvName << ":" << ca_message(status);
    return;
  }

  channel->subscribed = true;
  scheduleDeferredFlush();
}

void SharedChannelManager::requestControlInfo(SharedChannel *channel)
{
  if (!channel || !channel->channelId || channel->controlInfoRequested) {
    return;
  }

  /* Use cached field type from connection callback */
  short fieldType = channel->cachedData.nativeFieldType;
  chtype controlType = 0;

  switch (fieldType) {
  case DBR_ENUM:
    controlType = DBR_CTRL_ENUM;
    break;
  case DBR_CHAR:
  case DBR_SHORT:
  case DBR_LONG:
  case DBR_FLOAT:
  case DBR_DOUBLE:
    controlType = DBR_CTRL_DOUBLE;
    break;
  default:
    return;
  }

  channel->controlInfoRequested = true;

  int status = ca_array_get_callback(
      controlType,
      1,
      channel->channelId,
      &SharedChannelManager::controlInfoCallback,
      channel);

  if (status == ECA_NORMAL) {
    scheduleDeferredFlush();
  }
}

void SharedChannelManager::connectionCallback(connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *channel = static_cast<SharedChannel *>(ca_puser(args.chid));
  if (!channel) {
    return;
  }
  
  bool connected = (args.op == CA_OP_CONN_UP);
  
  /* Capture native type info while still on CA thread */
  short nativeType = connected ? ca_field_type(args.chid) : -1;
  long nativeCount = connected ? ca_element_count(args.chid) : 0;
  
  /* With preemptive callbacks, we're on the CA thread.
   * Queue the event to the main thread for processing. */
  QMetaObject::invokeMethod(&SharedChannelManager::instance(),
      "onConnectionChanged",
      Qt::QueuedConnection,
      Q_ARG(void*, channel),
      Q_ARG(bool, connected),
      Q_ARG(short, nativeType),
      Q_ARG(long, nativeCount));
}

void SharedChannelManager::valueCallback(event_handler_args args)
{
  auto *channel = static_cast<SharedChannel *>(args.usr);
  if (!channel) {
    return;
  }
  
  /* Copy the event data so it can be passed to the main thread.
   * The args.dbr pointer is only valid during this callback. */
  QByteArray eventData;
  if (args.dbr && args.status == ECA_NORMAL) {
    size_t dataSize = dbr_size_n(args.type, args.count);
    eventData = QByteArray(static_cast<const char*>(args.dbr), 
                           static_cast<int>(dataSize));
  }
  
  QMetaObject::invokeMethod(&SharedChannelManager::instance(),
      "onValueReceived",
      Qt::QueuedConnection,
      Q_ARG(void*, channel),
      Q_ARG(QByteArray, eventData),
      Q_ARG(int, args.status),
      Q_ARG(long, args.type),
      Q_ARG(long, args.count));
}

void SharedChannelManager::controlInfoCallback(event_handler_args args)
{
  auto *channel = static_cast<SharedChannel *>(args.usr);
  if (!channel) {
    return;
  }
  
  /* Copy the event data for main thread processing */
  QByteArray eventData;
  if (args.dbr && args.status == ECA_NORMAL) {
    size_t dataSize = dbr_size_n(args.type, args.count);
    eventData = QByteArray(static_cast<const char*>(args.dbr), 
                           static_cast<int>(dataSize));
  }
  
  QMetaObject::invokeMethod(&SharedChannelManager::instance(),
      "onControlInfoReceived",
      Qt::QueuedConnection,
      Q_ARG(void*, channel),
      Q_ARG(QByteArray, eventData),
      Q_ARG(int, args.status),
      Q_ARG(long, args.type));
}

void SharedChannelManager::accessRightsCallback(access_rights_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *channel = static_cast<SharedChannel *>(ca_puser(args.chid));
  if (!channel) {
    return;
  }
  bool canRead = ca_read_access(args.chid) != 0;
  bool canWrite = ca_write_access(args.chid) != 0;
  
  QMetaObject::invokeMethod(&SharedChannelManager::instance(),
      "onAccessRightsChanged",
      Qt::QueuedConnection,
      Q_ARG(void*, channel),
      Q_ARG(bool, canRead),
      Q_ARG(bool, canWrite));
}

/* Slot implementations - these run on the main Qt thread */

void SharedChannelManager::onConnectionChanged(void *channelPtr, bool connected,
                                                short nativeType, long nativeCount)
{
  auto *channel = static_cast<SharedChannel *>(channelPtr);
  
  /* Validate the channel pointer is still in our map */
  std::lock_guard<std::mutex> lock(channelMutex_);
  bool found = false;
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    if (it.value() == channel) {
      found = true;
      break;
    }
  }
  if (!found) {
    /* Channel was destroyed before we processed this event */
    return;
  }
  
  /* Store the native type info (captured on CA thread) */
  channel->cachedData.nativeFieldType = nativeType;
  channel->cachedData.nativeElementCount = nativeCount;
  
  handleConnection(channel, connected);
}

void SharedChannelManager::onValueReceived(void *channelPtr, QByteArray eventData,
                                            int status, long type, long count)
{
  auto *channel = static_cast<SharedChannel *>(channelPtr);
  
  /* Validate the channel pointer */
  std::lock_guard<std::mutex> lock(channelMutex_);
  bool found = false;
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    if (it.value() == channel) {
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }
  
  /* Reconstruct event_handler_args from the copied data */
  event_handler_args args;
  args.usr = channel;
  args.chid = channel->channelId;
  args.type = type;
  args.count = count;
  args.status = status;
  args.dbr = eventData.isEmpty() ? nullptr : eventData.constData();
  
  handleValue(channel, args);
}

void SharedChannelManager::onControlInfoReceived(void *channelPtr, QByteArray eventData,
                                                  int status, long type)
{
  auto *channel = static_cast<SharedChannel *>(channelPtr);
  
  /* Validate the channel pointer */
  std::lock_guard<std::mutex> lock(channelMutex_);
  bool found = false;
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    if (it.value() == channel) {
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }
  
  /* Reconstruct event_handler_args from the copied data */
  event_handler_args args;
  args.usr = channel;
  args.chid = channel->channelId;
  args.type = type;
  args.count = 1;
  args.status = status;
  args.dbr = eventData.isEmpty() ? nullptr : eventData.constData();
  
  handleControlInfo(channel, args);
}

void SharedChannelManager::onAccessRightsChanged(void *channelPtr, bool canRead, 
                                                  bool canWrite)
{
  auto *channel = static_cast<SharedChannel *>(channelPtr);
  
  /* Validate the channel pointer */
  std::lock_guard<std::mutex> lock(channelMutex_);
  bool found = false;
  for (auto it = channels_.begin(); it != channels_.end(); ++it) {
    if (it.value() == channel) {
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }
  
  handleAccessRights(channel, canRead, canWrite);
}

void SharedChannelManager::handleConnection(SharedChannel *channel, bool connected)
{
  if (!channel) {
    return;
  }

  bool wasConnected = channel->connected;
  channel->connected = connected;
  channel->cachedData.connected = connected;

  if (connected) {
    if (!wasConnected) {
      StatisticsTracker::instance().registerChannelConnected();
      ++totalConnectionsMade_;
      lastConnectedPvName_ = channel->key.pvName;
      /* Report first connection for timing diagnostics */
      if (!firstConnectionReported_) {
        firstConnectionReported_ = true;
        expectedChannelCount_ = channels_.size();
        QTEDM_TIMING_MARK_COUNT("PV channels created", expectedChannelCount_);
        QTEDM_TIMING_MARK_DETAIL("First PV connection", channel->key.pvName);
      }
      /* Report connection milestones */
      if (expectedChannelCount_ > 0) {
        int pct = (totalConnectionsMade_ * 100) / expectedChannelCount_;
        if (!connection10Reported_ && pct >= 10) {
          connection10Reported_ = true;
          QTEDM_TIMING_MARK_COUNT("PV connections: 10% complete", totalConnectionsMade_);
        }
        if (!connection25Reported_ && pct >= 25) {
          connection25Reported_ = true;
          QTEDM_TIMING_MARK_COUNT("PV connections: 25% complete", totalConnectionsMade_);
        }
        if (!connection50Reported_ && pct >= 50) {
          connection50Reported_ = true;
          QTEDM_TIMING_MARK_COUNT("PV connections: 50% complete", totalConnectionsMade_);
        }
        if (!connection75Reported_ && pct >= 75) {
          connection75Reported_ = true;
          QTEDM_TIMING_MARK_COUNT("PV connections: 75% complete", totalConnectionsMade_);
        }
        if (!connection90Reported_ && pct >= 90) {
          connection90Reported_ = true;
          QTEDM_TIMING_MARK_COUNT("PV connections: 90% complete", totalConnectionsMade_);
        }
      }
      /* Check if all channels are now connected */
      if (!lastConnectionReported_ && totalConnectionsMade_ == channels_.size()) {
        lastConnectionReported_ = true;
        QTEDM_TIMING_MARK_COUNT("All PVs connected, total", totalConnectionsMade_);
        QTEDM_TIMING_MARK_DETAIL("Last PV connection", channel->key.pvName);
      }
    }

    /* Native type info is already set by onConnectionChanged() which 
     * captured it on the CA thread before queuing to main thread. */

    /* Subscribe and request control info */
    subscribeToChannel(channel);
    requestControlInfo(channel);
  } else {
    if (wasConnected) {
      StatisticsTracker::instance().registerChannelDisconnected();
    }

    /* Clear cached value state on disconnect */
    channel->cachedData.hasValue = false;
    channel->cachedData.hasControlInfo = false;
    channel->subscribed = false;
    channel->controlInfoRequested = false;
    if (channel->subscriptionId) {
      ca_clear_subscription(channel->subscriptionId);
      channel->subscriptionId = nullptr;
    }
  }

  /* Notify all subscribers of connection state change */
  for (const auto &sub : channel->subscribers) {
    if (sub.connectionCallback) {
      sub.connectionCallback(connected, channel->cachedData);
    }
  }
}

void SharedChannelManager::handleValue(SharedChannel *channel,
    const event_handler_args &args)
{
  if (!channel || !args.dbr || args.status != ECA_NORMAL) {
    return;
  }

  StatisticsTracker::instance().registerCaEvent();

  /* Track values for timing diagnostics */
  bool isFirstValueForChannel = !channel->cachedData.hasValue;
  if (isFirstValueForChannel) {
    ++totalValuesReceived_;
    lastValuePvName_ = channel->key.pvName;
  }
  /* Report first value for timing diagnostics */
  if (!firstValueReported_) {
    firstValueReported_ = true;
    QTEDM_TIMING_MARK_DETAIL("First PV value received", channel->key.pvName);
  }
  /* Report value milestones */
  int connectedCount = connectedChannelCount();
  if (connectedCount > 0 && isFirstValueForChannel) {
    int pct = (totalValuesReceived_ * 100) / connectedCount;
    if (!value10Reported_ && pct >= 10) {
      value10Reported_ = true;
      QTEDM_TIMING_MARK_COUNT("PV values: 10% complete", totalValuesReceived_);
    }
    if (!value25Reported_ && pct >= 25) {
      value25Reported_ = true;
      QTEDM_TIMING_MARK_COUNT("PV values: 25% complete", totalValuesReceived_);
    }
    if (!value50Reported_ && pct >= 50) {
      value50Reported_ = true;
      QTEDM_TIMING_MARK_COUNT("PV values: 50% complete", totalValuesReceived_);
    }
    if (!value75Reported_ && pct >= 75) {
      value75Reported_ = true;
      QTEDM_TIMING_MARK_COUNT("PV values: 75% complete", totalValuesReceived_);
    }
    if (!value90Reported_ && pct >= 90) {
      value90Reported_ = true;
      QTEDM_TIMING_MARK_COUNT("PV values: 90% complete", totalValuesReceived_);
    }
  }
  /* Check if all channels have received at least one value */
  if (!lastValueReported_ && isFirstValueForChannel &&
      totalValuesReceived_ == connectedCount) {
    lastValueReported_ = true;
    QTEDM_TIMING_MARK_COUNT("All PVs have values, total", totalValuesReceived_);
    QTEDM_TIMING_MARK_DETAIL("Last PV value received", channel->key.pvName);
  }

  SharedChannelData &data = channel->cachedData;

  /* Reset value type flags */
  data.isNumeric = false;
  data.isString = false;
  data.isEnum = false;
  data.isCharArray = false;
  data.isArray = false;
  data.arrayValues.clear();
  data.charArrayValue.clear();

  switch (args.type) {
  case DBR_TIME_DOUBLE: {
    const auto *val = static_cast<const dbr_time_double *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    if (args.count > 1) {
      data.isArray = true;
      data.arrayValues.resize(args.count);
      const double *src = &val->value;
      for (long i = 0; i < args.count; ++i) {
        data.arrayValues[i] = src[i];
      }
      data.numericValue = val->value;
    } else {
      data.numericValue = val->value;
    }
    data.isNumeric = true;
    break;
  }
  case DBR_TIME_FLOAT: {
    const auto *val = static_cast<const dbr_time_float *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    if (args.count > 1) {
      data.isArray = true;
      data.arrayValues.resize(args.count);
      const float *src = &val->value;
      for (long i = 0; i < args.count; ++i) {
        data.arrayValues[i] = static_cast<double>(src[i]);
      }
      data.numericValue = static_cast<double>(val->value);
    } else {
      data.numericValue = static_cast<double>(val->value);
    }
    data.isNumeric = true;
    break;
  }
  case DBR_TIME_LONG: {
    const auto *val = static_cast<const dbr_time_long *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    if (args.count > 1) {
      data.isArray = true;
      data.arrayValues.resize(args.count);
      const dbr_long_t *src = &val->value;
      for (long i = 0; i < args.count; ++i) {
        data.arrayValues[i] = static_cast<double>(src[i]);
      }
      data.numericValue = static_cast<double>(val->value);
    } else {
      data.numericValue = static_cast<double>(val->value);
    }
    data.isNumeric = true;
    break;
  }
  case DBR_TIME_SHORT: {
    const auto *val = static_cast<const dbr_time_short *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    if (args.count > 1) {
      data.isArray = true;
      data.arrayValues.resize(args.count);
      const dbr_short_t *src = &val->value;
      for (long i = 0; i < args.count; ++i) {
        data.arrayValues[i] = static_cast<double>(src[i]);
      }
      data.numericValue = static_cast<double>(val->value);
    } else {
      data.numericValue = static_cast<double>(val->value);
    }
    data.isNumeric = true;
    break;
  }
  case DBR_TIME_CHAR: {
    const auto *val = static_cast<const dbr_time_char *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    if (args.count > 1) {
      /* Treat as char array / string */
      data.isCharArray = true;
      const char *src = reinterpret_cast<const char *>(&val->value);
      data.charArrayValue = QByteArray(src, args.count);
      /* Also provide as string (null-terminated) */
      int len = 0;
      for (int i = 0; i < args.count; ++i) {
        if (src[i] == '\0') {
          break;
        }
        ++len;
      }
      data.stringValue = QString::fromLatin1(src, len);
      data.isString = true;
    } else {
      data.numericValue = static_cast<double>(
          static_cast<unsigned char>(val->value));
      data.isNumeric = true;
    }
    break;
  }
  case DBR_TIME_ENUM: {
    const auto *val = static_cast<const dbr_time_enum *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    data.enumValue = val->value;
    data.numericValue = static_cast<double>(val->value);
    data.isEnum = true;
    data.isNumeric = true;
    /* String value from enum strings if available */
    if (data.hasControlInfo && val->value < data.enumStrings.size()) {
      data.stringValue = data.enumStrings.at(val->value);
      data.isString = true;
    }
    break;
  }
  case DBR_TIME_STRING: {
    const auto *val = static_cast<const dbr_time_string *>(args.dbr);
    data.severity = val->severity;
    data.status = val->status;
    data.timestamp = val->stamp;
    data.hasTimestamp = true;
    data.stringValue = QString::fromLatin1(val->value);
    data.isString = true;
    break;
  }
  default:
    return;
  }

  data.hasValue = true;

  /* Check if value or alarm state actually changed.
   * Skip notification if nothing changed to reduce CPU load. */
  bool valueChanged = false;
  if (channel->lastNotifiedSeverity < 0) {
    /* First value - always notify */
    valueChanged = true;
  } else if (data.severity != channel->lastNotifiedSeverity) {
    /* Alarm state changed */
    valueChanged = true;
  } else if (data.isNumeric && data.numericValue != channel->lastNotifiedValue) {
    /* Numeric value changed */
    valueChanged = true;
  } else if (data.isString && data.stringValue != channel->lastNotifiedString) {
    /* String value changed */
    valueChanged = true;
  } else if (data.isEnum && data.enumValue != channel->lastNotifiedEnum) {
    /* Enum value changed */
    valueChanged = true;
  }

  if (!valueChanged) {
    /* No change - skip notification but still update cached data timestamp */
    return;
  }

  /* Rate limit subscriber notifications to reduce CPU load from high-frequency PVs.
   * Always notify on the first value, then enforce minimum interval between updates. */
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (channel->lastNotifyTimeMs > 0) {
    const qint64 elapsedMs = nowMs - channel->lastNotifyTimeMs;
    if (elapsedMs < kMinNotifyIntervalMs) {
      /* Skip this notification - too soon since last one.
       * The cached data is still updated, so the next notification
       * will have the latest value. */
      return;
    }
  }
  channel->lastNotifyTimeMs = nowMs;

  /* Update last notified values for next comparison */
  channel->lastNotifiedValue = data.numericValue;
  channel->lastNotifiedSeverity = data.severity;
  channel->lastNotifiedString = data.stringValue;
  channel->lastNotifiedEnum = data.enumValue;

  /* Count this update for statistics (only updates passed to subscribers) */
  ++channel->updateCount;

  /* Notify all subscribers */
  for (const auto &sub : channel->subscribers) {
    if (sub.valueCallback) {
      sub.valueCallback(data);
    }
  }
}

void SharedChannelManager::handleControlInfo(SharedChannel *channel,
    const event_handler_args &args)
{
  if (!channel || !args.dbr || args.status != ECA_NORMAL) {
    return;
  }

  SharedChannelData &data = channel->cachedData;

  switch (args.type) {
  case DBR_CTRL_DOUBLE: {
    const auto *info = static_cast<const dbr_ctrl_double *>(args.dbr);
    data.hopr = info->upper_ctrl_limit;
    data.lopr = info->lower_ctrl_limit;
    data.precision = info->precision;
    data.hasControlInfo = true;
    break;
  }
  case DBR_CTRL_ENUM: {
    const auto *info = static_cast<const dbr_ctrl_enum *>(args.dbr);
    data.enumStrings.clear();
    for (int i = 0; i < info->no_str; ++i) {
      data.enumStrings.append(QString::fromLatin1(info->strs[i]));
    }
    data.hasControlInfo = true;
    /* Update string value if we have an enum value */
    if (data.isEnum && data.enumValue < data.enumStrings.size()) {
      data.stringValue = data.enumStrings.at(data.enumValue);
      data.isString = true;
    }
    break;
  }
  default:
    break;
  }

  /* Re-notify subscribers so they get the control info */
  if (data.hasValue) {
    for (const auto &sub : channel->subscribers) {
      if (sub.valueCallback) {
        sub.valueCallback(data);
      }
    }
  }
}

void SharedChannelManager::handleAccessRights(SharedChannel *channel,
    bool canRead, bool canWrite)
{
  if (!channel) {
    return;
  }

  bool changed = (channel->canRead != canRead) || (channel->canWrite != canWrite);
  channel->canRead = canRead;
  channel->canWrite = canWrite;

  if (changed) {
    /* Notify all subscribers of access rights change */
    for (const auto &sub : channel->subscribers) {
      if (sub.accessRightsCallback) {
        sub.accessRightsCallback(canRead, canWrite);
      }
    }
  }
}

bool SharedChannelManager::putValue(const QString &pvName, double value)
{
  ChannelAccessContext::instance().ensureInitialized();
  if (!ChannelAccessContext::instance().isInitialized()) {
    return false;
  }

  QString trimmed = pvName.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  /* Look for any existing channel with this PV name */
  chid putChannel = nullptr;
  for (auto *channel : channels_) {
    if (channel->key.pvName == trimmed && channel->connected) {
      putChannel = channel->channelId;
      break;
    }
  }

  if (!putChannel) {
    /* Create temporary channel for put - this is rare */
    QByteArray pvBytes = trimmed.toLatin1();
    int status = ca_create_channel(
        pvBytes.constData(), nullptr, nullptr, CA_PRIORITY_DEFAULT, &putChannel);
    if (status != ECA_NORMAL) {
      return false;
    }
    ca_pend_io(1.0);
    if (ca_state(putChannel) != cs_conn) {
      ca_clear_channel(putChannel);
      return false;
    }
    int putStatus = ca_put(DBR_DOUBLE, putChannel, &value);
    if (putStatus == ECA_NORMAL) {
      AuditLogger::instance().logPut(trimmed, value,
          QStringLiteral("Slider"));
    }
    ca_flush_io();
    ca_clear_channel(putChannel);
    return putStatus == ECA_NORMAL;
  }

  int status = ca_put(DBR_DOUBLE, putChannel, &value);
  if (status == ECA_NORMAL) {
    AuditLogger::instance().logPut(trimmed, value,
        QStringLiteral("Slider"));
  }
  ca_flush_io();
  return status == ECA_NORMAL;
}

bool SharedChannelManager::putValue(const QString &pvName, const QString &value)
{
  ChannelAccessContext::instance().ensureInitialized();
  if (!ChannelAccessContext::instance().isInitialized()) {
    return false;
  }

  QString trimmed = pvName.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  /* Look for any existing channel with this PV name */
  chid putChannel = nullptr;
  for (auto *channel : channels_) {
    if (channel->key.pvName == trimmed && channel->connected) {
      putChannel = channel->channelId;
      break;
    }
  }

  QByteArray valueBytes = value.toLatin1();
  char strValue[MAX_STRING_SIZE];
  std::memset(strValue, 0, sizeof(strValue));
  std::strncpy(strValue, valueBytes.constData(),
      std::min<size_t>(valueBytes.size(), MAX_STRING_SIZE - 1));

  if (!putChannel) {
    QByteArray pvBytes = trimmed.toLatin1();
    int status = ca_create_channel(
        pvBytes.constData(), nullptr, nullptr, CA_PRIORITY_DEFAULT, &putChannel);
    if (status != ECA_NORMAL) {
      return false;
    }
    ca_pend_io(1.0);
    if (ca_state(putChannel) != cs_conn) {
      ca_clear_channel(putChannel);
      return false;
    }
    int putStatus = ca_put(DBR_STRING, putChannel, strValue);
    ca_flush_io();
    ca_clear_channel(putChannel);
    return putStatus == ECA_NORMAL;
  }

  int status = ca_put(DBR_STRING, putChannel, strValue);
  ca_flush_io();
  return status == ECA_NORMAL;
}

bool SharedChannelManager::putValue(const QString &pvName, dbr_enum_t value)
{
  ChannelAccessContext::instance().ensureInitialized();
  if (!ChannelAccessContext::instance().isInitialized()) {
    return false;
  }

  QString trimmed = pvName.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  chid putChannel = nullptr;
  for (auto *channel : channels_) {
    if (channel->key.pvName == trimmed && channel->connected) {
      putChannel = channel->channelId;
      break;
    }
  }

  if (!putChannel) {
    QByteArray pvBytes = trimmed.toLatin1();
    int status = ca_create_channel(
        pvBytes.constData(), nullptr, nullptr, CA_PRIORITY_DEFAULT, &putChannel);
    if (status != ECA_NORMAL) {
      return false;
    }
    ca_pend_io(1.0);
    if (ca_state(putChannel) != cs_conn) {
      ca_clear_channel(putChannel);
      return false;
    }
    int putStatus = ca_put(DBR_ENUM, putChannel, &value);
    ca_flush_io();
    ca_clear_channel(putChannel);
    return putStatus == ECA_NORMAL;
  }

  int status = ca_put(DBR_ENUM, putChannel, &value);
  ca_flush_io();
  return status == ECA_NORMAL;
}

bool SharedChannelManager::putArrayValue(const QString &pvName,
    const QVector<double> &values)
{
  ChannelAccessContext::instance().ensureInitialized();
  if (!ChannelAccessContext::instance().isInitialized()) {
    return false;
  }

  QString trimmed = pvName.trimmed();
  if (trimmed.isEmpty() || values.isEmpty()) {
    return false;
  }

  chid putChannel = nullptr;
  for (auto *channel : channels_) {
    if (channel->key.pvName == trimmed && channel->connected) {
      putChannel = channel->channelId;
      break;
    }
  }

  if (!putChannel) {
    QByteArray pvBytes = trimmed.toLatin1();
    int status = ca_create_channel(
        pvBytes.constData(), nullptr, nullptr, CA_PRIORITY_DEFAULT, &putChannel);
    if (status != ECA_NORMAL) {
      return false;
    }
    ca_pend_io(1.0);
    if (ca_state(putChannel) != cs_conn) {
      ca_clear_channel(putChannel);
      return false;
    }
    int putStatus = ca_array_put(DBR_DOUBLE, values.size(), putChannel,
        values.constData());
    ca_flush_io();
    ca_clear_channel(putChannel);
    return putStatus == ECA_NORMAL;
  }

  int status = ca_array_put(DBR_DOUBLE, values.size(), putChannel,
      values.constData());
  ca_flush_io();
  return status == ECA_NORMAL;
}

int SharedChannelManager::uniqueChannelCount() const
{
  return channels_.size();
}

int SharedChannelManager::totalSubscriptionCount() const
{
  int total = 0;
  for (const auto *channel : channels_) {
    total += channel->subscribers.size();
  }
  return total;
}

int SharedChannelManager::connectedChannelCount() const
{
  int count = 0;
  for (const auto *channel : channels_) {
    if (channel->connected) {
      ++count;
    }
  }
  return count;
}

QList<ChannelSummary> SharedChannelManager::channelSummaries() const
{
  QList<ChannelSummary> summaries;
  summaries.reserve(channels_.size());

  double elapsed = elapsedSecondsSinceReset();

  for (const auto *channel : channels_) {
    ChannelSummary summary;
    summary.pvName = channel->key.pvName;
    summary.connected = channel->connected;
    summary.writable = channel->canWrite;
    summary.subscriberCount = channel->subscribers.size();
    summary.updateCount = channel->updateCount;
    summary.updateRate = (elapsed > 0.0)
        ? (static_cast<double>(channel->updateCount) / elapsed)
        : 0.0;
    summary.severity = channel->cachedData.severity;
    summaries.append(summary);
  }

  /* Sort by PV name for consistent display */
  std::sort(summaries.begin(), summaries.end(),
      [](const ChannelSummary &a, const ChannelSummary &b) {
        return a.pvName.compare(b.pvName, Qt::CaseInsensitive) < 0;
      });

  return summaries;
}

void SharedChannelManager::resetUpdateCounters()
{
  for (auto *channel : channels_) {
    channel->updateCount = 0;
  }
  updateRateTimer_.start();
  updateRateTimerStarted_ = true;
}

double SharedChannelManager::elapsedSecondsSinceReset() const
{
  if (!updateRateTimerStarted_) {
    return 0.0;
  }
  return static_cast<double>(updateRateTimer_.elapsed()) / 1000.0;
}

void SharedChannelManager::scheduleDeferredFlush()
{
  if (flushScheduled_) {
    return;
  }
  flushScheduled_ = true;
  /* Use a zero-timeout timer to defer the flush to the next event loop
   * iteration. This allows multiple CA operations to be batched together
   * before flushing, preventing event loop starvation during rapid
   * connection sequences. */
  QTimer::singleShot(0, this, &SharedChannelManager::performDeferredFlush);
}

void SharedChannelManager::performDeferredFlush()
{
  flushScheduled_ = false;
  if (StartupTiming::instance().isEnabled()) {
    qint64 before = StartupTiming::instance().elapsedMs();
    fprintf(stderr, "[TIMING] %8lld ms : performDeferredFlush starting\n", before);
    fflush(stderr);
    ca_flush_io();
    qint64 after = StartupTiming::instance().elapsedMs();
    fprintf(stderr, "[TIMING] %8lld ms : performDeferredFlush complete (took %lld ms)\n",
        after, after - before);
    fflush(stderr);
  } else {
    ca_flush_io();
  }
}

/* Include moc-generated code */
#include "moc_shared_channel_manager.cpp"