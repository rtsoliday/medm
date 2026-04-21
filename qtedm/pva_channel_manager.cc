#include "pva_channel_manager.h"

#include "pva_bridge.h"
#include "heatmap_runtime.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QStringList>
#include <QVector>

namespace {

constexpr qint64 kMinNotifyIntervalMs = 100;
constexpr int kPollIntervalMs = 100;

static SharedChannelData toSharedChannelData(const PvaBridgeData &bridgeData)
{
  SharedChannelData data;
  data.connected = bridgeData.connected;
  data.nativeFieldType = static_cast<short>(bridgeData.nativeFieldType);
  data.nativeElementCount = bridgeData.nativeElementCount;
  data.numericValue = bridgeData.numericValue;
  data.stringValue = QString::fromUtf8(bridgeData.stringValue.c_str());
  data.enumValue = static_cast<dbr_enum_t>(bridgeData.enumValue);
  data.arrayValues = QVector<double>(bridgeData.arrayValues.begin(),
      bridgeData.arrayValues.end());
  data.sharedArrayData = bridgeData.sharedArrayData;
  data.sharedArraySize = bridgeData.sharedArraySize;
  data.severity = bridgeData.severity;
  data.status = bridgeData.status;
  data.timestamp = epicsTimeStamp{};
  data.hasTimestamp = false;
  data.hopr = bridgeData.hopr;
  data.lopr = bridgeData.lopr;
  data.precision = bridgeData.precision;
  data.units = QString::fromUtf8(bridgeData.units.c_str());
  for (const std::string &entry : bridgeData.enumStrings) {
    data.enumStrings.append(QString::fromUtf8(entry.c_str()));
  }
  data.hasControlInfo = bridgeData.hasControlInfo;
  data.hasUnits = bridgeData.hasUnits;
  data.hasPrecision = bridgeData.hasPrecision;
  data.hasValue = bridgeData.hasValue;
  data.isNumeric = bridgeData.isNumeric;
  data.isString = bridgeData.isString;
  data.isEnum = bridgeData.isEnum;
  data.isCharArray = bridgeData.isCharArray;
  data.isArray = bridgeData.isArray;
  return data;
}

} // namespace

PvaChannelManager &PvaChannelManager::instance()
{
  static PvaChannelManager manager;
  return manager;
}

PvaChannelManager::PvaChannelManager()
  : QObject(nullptr)
{
  statsTimer_.start();
  pollTimer_.setInterval(kPollIntervalMs);
  pollTimer_.setTimerType(Qt::CoarseTimer);
  connect(&pollTimer_, &QTimer::timeout, this, [this]() { pollChannels(); });
  if (QCoreApplication *core = QCoreApplication::instance()) {
    connect(core, &QCoreApplication::aboutToQuit, this,
        &PvaChannelManager::shutdown);
  }
}

PvaChannelManager::~PvaChannelManager()
{
  shutdown();
}

SubscriptionHandle PvaChannelManager::subscribe(
    const QString &pvName,
    chtype requestedType,
    long elementCount,
    ChannelValueCallback valueCallback,
    ChannelConnectionCallback connectionCallback,
    ChannelAccessRightsCallback accessRightsCallback)
{
  Q_UNUSED(requestedType);
  Q_UNUSED(elementCount);

  if (shutdownComplete_ || pvName.trimmed().isEmpty() || !valueCallback) {
    return SubscriptionHandle();
  }

  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return SubscriptionHandle();
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = requestedType;
  key.elementCount = elementCount;

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel) {
    return SubscriptionHandle();
  }

  quint64 subId = nextSubscriptionId_++;
  Subscriber sub;
  sub.id = subId;
  sub.valueCallback = std::move(valueCallback);
  sub.connectionCallback = std::move(connectionCallback);
  sub.accessRightsCallback = std::move(accessRightsCallback);

  channel->subscribers.append(sub);
  subscriptionToChannel_.insert(subId, channel);

  if (channel->connected) {
    scheduleInitialDelivery(subId);
  }

  if (!pollTimer_.isActive()) {
    pollTimer_.start();
  }

  return SubscriptionHandle(subId, this);
}

void PvaChannelManager::unsubscribe(quint64 subscriptionId)
{
  auto it = subscriptionToChannel_.find(subscriptionId);
  if (it == subscriptionToChannel_.end()) {
    return;
  }

  PvaChannel *channel = it.value();
  subscriptionToChannel_.erase(it);

  auto &subs = channel->subscribers;
  for (int i = 0; i < subs.size(); ++i) {
    if (subs[i].id == subscriptionId) {
      subs.removeAt(i);
      break;
    }
  }

  destroyChannelIfUnused(channel);
}

void PvaChannelManager::scheduleInitialDelivery(quint64 subscriptionId)
{
  QTimer::singleShot(0, this, [this, subscriptionId]() {
    deliverInitialState(subscriptionId);
  });
}

void PvaChannelManager::deliverInitialState(quint64 subscriptionId)
{
  auto it = subscriptionToChannel_.find(subscriptionId);
  if (it == subscriptionToChannel_.end()) {
    return;
  }

  PvaChannel *channel = it.value();
  if (!channel) {
    return;
  }

  ++channel->dispatchDepth;

  if (channel->connected) {
    if (Subscriber *sub = findSubscriber(channel, subscriptionId)) {
      if (sub->connectionCallback) {
        const SharedChannelData data = channel->cachedData;
        auto callback = sub->connectionCallback;
        callback(true, data);
      }
    }

    if (Subscriber *sub = findSubscriber(channel, subscriptionId)) {
      if (sub->accessRightsCallback) {
        auto callback = sub->accessRightsCallback;
        callback(channel->canRead, channel->canWrite);
      }
    }

    if (channel->cachedData.hasValue) {
      if (Subscriber *sub = findSubscriber(channel, subscriptionId)) {
        if (sub->valueCallback) {
          const SharedChannelData data = channel->cachedData;
          auto callback = sub->valueCallback;
          callback(data);
        }
      }
    }
  }

  --channel->dispatchDepth;
  if (channel->dispatchDepth == 0 && channel->destroyPending) {
    destroyChannelIfUnused(channel);
  }
}

PvaChannelManager::Subscriber *PvaChannelManager::findSubscriber(
    PvaChannel *channel, quint64 subscriptionId)
{
  if (!channel) {
    return nullptr;
  }

  for (Subscriber &sub : channel->subscribers) {
    if (sub.id == subscriptionId) {
      return &sub;
    }
  }
  return nullptr;
}

void PvaChannelManager::dispatchConnectionCallbacks(PvaChannel *channel)
{
  if (!channel) {
    return;
  }

  QList<quint64> subscriberIds;
  subscriberIds.reserve(channel->subscribers.size());
  for (const Subscriber &sub : channel->subscribers) {
    subscriberIds.append(sub.id);
  }

  const bool connected = channel->connected;
  const SharedChannelData data = channel->cachedData;
  ++channel->dispatchDepth;
  for (quint64 subscriberId : subscriberIds) {
    Subscriber *sub = findSubscriber(channel, subscriberId);
    if (!sub || !sub->connectionCallback) {
      continue;
    }
    auto callback = sub->connectionCallback;
    callback(connected, data);
  }
  --channel->dispatchDepth;

  if (channel->dispatchDepth == 0 && channel->destroyPending) {
    destroyChannelIfUnused(channel);
  }
}

void PvaChannelManager::dispatchAccessRightsCallbacks(PvaChannel *channel)
{
  if (!channel) {
    return;
  }

  QList<quint64> subscriberIds;
  subscriberIds.reserve(channel->subscribers.size());
  for (const Subscriber &sub : channel->subscribers) {
    subscriberIds.append(sub.id);
  }

  const bool canRead = channel->canRead;
  const bool canWrite = channel->canWrite;
  ++channel->dispatchDepth;
  for (quint64 subscriberId : subscriberIds) {
    Subscriber *sub = findSubscriber(channel, subscriberId);
    if (!sub || !sub->accessRightsCallback) {
      continue;
    }
    auto callback = sub->accessRightsCallback;
    callback(canRead, canWrite);
  }
  --channel->dispatchDepth;

  if (channel->dispatchDepth == 0 && channel->destroyPending) {
    destroyChannelIfUnused(channel);
  }
}

void PvaChannelManager::dispatchValueCallbacks(PvaChannel *channel)
{
  if (!channel) {
    return;
  }

  QList<quint64> subscriberIds;
  subscriberIds.reserve(channel->subscribers.size());
  for (const Subscriber &sub : channel->subscribers) {
    subscriberIds.append(sub.id);
  }

  const SharedChannelData data = channel->cachedData;
  ++channel->dispatchDepth;
  for (quint64 subscriberId : subscriberIds) {
    Subscriber *sub = findSubscriber(channel, subscriberId);
    if (!sub || !sub->valueCallback) {
      continue;
    }
    auto callback = sub->valueCallback;
    callback(data);
  }
  --channel->dispatchDepth;

  if (channel->dispatchDepth == 0 && channel->destroyPending) {
    destroyChannelIfUnused(channel);
  }
}

void PvaChannelManager::scheduleDeferredValueNotify(const SharedChannelKey &key,
    int delayMs)
{
  QTimer::singleShot(delayMs, this, [this, key]() {
    dispatchDeferredValueNotify(key);
  });
}

void PvaChannelManager::dispatchDeferredValueNotify(const SharedChannelKey &key)
{
  PvaChannel *channel = channels_.value(key, nullptr);
  if (!channel || !channel->notifyPending) {
    return;
  }

  notifySubscribers(channel);
}

PvaChannelManager::PvaChannel *PvaChannelManager::findOrCreateChannel(
    const SharedChannelKey &key,
    const QString &rawName,
    const QString &pvName)
{
  if (shutdownComplete_) {
    return nullptr;
  }
  auto it = channels_.find(key);
  if (it != channels_.end()) {
    return it.value();
  }

  auto *channel = new PvaChannel;
  channel->key = key;
  channel->rawName = rawName.trimmed();
  channel->pvName = pvName.trimmed();
  channel->bridge = pvaBridgeCreateChannel(channel->rawName.toStdString(),
      channel->pvName.toStdString(), key.requestedType, key.elementCount);
  if (!channel->bridge) {
    delete channel;
    return nullptr;
  }

  updateCachedData(channel);

  channels_.insert(key, channel);
  return channel;
}

void PvaChannelManager::destroyChannelIfUnused(PvaChannel *channel)
{
  if (!channel) {
    return;
  }
  if (!channel->subscribers.isEmpty()) {
    channel->destroyPending = false;
    return;
  }
  if (channel->dispatchDepth > 0) {
    channel->destroyPending = true;
    return;
  }
  channel->destroyPending = false;

  channels_.remove(channel->key);
  pvaBridgeDestroyChannel(channel->bridge);
  channel->bridge = nullptr;
  delete channel;
  if (channels_.isEmpty()) {
    pollTimer_.stop();
  }
}

void PvaChannelManager::updateAccessRights(PvaChannel *channel)
{
  updateCachedData(channel);
}

void PvaChannelManager::updateCachedData(PvaChannel *channel)
{
  if (!channel || !channel->bridge) {
    return;
  }

  if (!pvaBridgeRefresh(channel->bridge,
          HeatmapRuntime::isGlobalUpdatesPaused())) {
    return;
  }

  PvaBridgeData bridgeData;
  if (!pvaBridgeGetData(channel->bridge, &bridgeData)) {
    return;
  }

  channel->connected = bridgeData.connected;
  channel->canRead = bridgeData.canRead;
  channel->canWrite = bridgeData.canWrite;
  channel->cachedData = toSharedChannelData(bridgeData);
}

bool PvaChannelManager::getInfoSnapshot(const QString &pvName,
    PvaInfoSnapshot &snapshot)
{
  snapshot = PvaInfoSnapshot{};
  if (shutdownComplete_) {
    return false;
  }

  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_TIME_DOUBLE;
  key.elementCount = 0;

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel) {
    return false;
  }
  const auto cleanupChannel = [this](PvaChannel *candidate) {
    if (candidate && candidate->subscribers.isEmpty()) {
      destroyChannelIfUnused(candidate);
    }
  };

  updateCachedData(channel);

  PvaBridgeData bridgeData;
  if (!pvaBridgeGetData(channel->bridge, &bridgeData)) {
    cleanupChannel(channel);
    return false;
  }

  const SharedChannelData &data = channel->cachedData;
  snapshot.pvName = parsed.rawName.trimmed();
  snapshot.connected = channel->connected;
  snapshot.canRead = channel->canRead;
  snapshot.canWrite = channel->canWrite;
  snapshot.fieldType = data.nativeFieldType;
  snapshot.elementCount = static_cast<unsigned long>(data.nativeElementCount);
  snapshot.host = QString::fromUtf8(bridgeData.host.c_str());
  snapshot.units = data.units;
  snapshot.hasUnits = data.hasUnits;
  snapshot.severity = data.severity;
  snapshot.hopr = data.hopr;
  snapshot.lopr = data.lopr;
  snapshot.precision = data.precision;
  snapshot.hasPrecision = data.hasPrecision;
  snapshot.hasLimits = data.hasControlInfo;
  snapshot.states = data.enumStrings;
  snapshot.hasStates = !data.enumStrings.isEmpty();

  if (data.hasValue) {
    snapshot.hasValue = true;
    if (data.isString) {
      snapshot.value = data.stringValue;
    } else if (data.isEnum && !data.enumStrings.isEmpty()) {
      int idx = static_cast<int>(data.enumValue);
      if (idx >= 0 && idx < data.enumStrings.size()) {
        snapshot.value = data.enumStrings.at(idx);
      } else {
        snapshot.value = QString::number(data.numericValue, 'g', 12);
      }
    } else {
      snapshot.value = QString::number(data.numericValue, 'g', 12);
    }
  }

  cleanupChannel(channel);
  return true;
}

void PvaChannelManager::notifySubscribers(PvaChannel *channel)
{
  if (!channel || !channel->cachedData.hasValue) {
    return;
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (channel->lastNotifyTimeMs > 0) {
    const qint64 elapsedMs = now - channel->lastNotifyTimeMs;
    if (elapsedMs < kMinNotifyIntervalMs) {
      if (!channel->notifyPending) {
        channel->notifyPending = true;
        scheduleDeferredValueNotify(channel->key,
            static_cast<int>(kMinNotifyIntervalMs - elapsedMs));
      }
      return;
    }
  }
  channel->lastNotifyTimeMs = now;
  channel->notifyPending = false;
  channel->updateCount++;
  dispatchValueCallbacks(channel);
}

void PvaChannelManager::pollChannels()
{
  if (channels_.isEmpty()) {
    return;
  }

  bool isPaused = HeatmapRuntime::isGlobalUpdatesPaused();
  static bool wasPaused = false;

  if (isPaused && !wasPaused) {
    for (auto *channel : channels_) {
      if (channel->bridge && channel->cachedData.nativeElementCount > 1000) {
        pvaBridgeSetMonitoringPaused(channel->bridge, true);
      }
    }
  } else if (!isPaused && wasPaused) {
    for (auto *channel : channels_) {
      if (channel->bridge && channel->cachedData.nativeElementCount > 1000) {
        pvaBridgeSetMonitoringPaused(channel->bridge, false);
      }
    }
  }
  wasPaused = isPaused;

  const QList<SharedChannelKey> channelKeys = channels_.keys();
  for (const SharedChannelKey &key : channelKeys) {
    PvaChannel *channel = channels_.value(key, nullptr);
    if (!channel || !channel->bridge) {
      continue;
    }

    bool connectionChanged = false;
    int events = pvaBridgePoll(channel->bridge, &connectionChanged, isPaused);

    if (connectionChanged) {
      updateCachedData(channel);
      dispatchConnectionCallbacks(channel);
      if (channels_.value(key, nullptr) != channel) {
        continue;
      }
      dispatchAccessRightsCallbacks(channel);
      if (channels_.value(key, nullptr) != channel) {
        continue;
      }
    }

    if (events > 0) {
      updateCachedData(channel);
      notifySubscribers(channel);
    }
  }
}

bool PvaChannelManager::putValue(const QString &pvName, double value)
{
  if (shutdownComplete_) {
    return false;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_TIME_DOUBLE;
  key.elementCount = 1;

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel) {
    return false;
  }
  const auto cleanupChannel = [this](PvaChannel *candidate) {
    if (candidate && candidate->subscribers.isEmpty()) {
      destroyChannelIfUnused(candidate);
    }
  };
  const bool ok = channel->bridge && channel->connected
      && pvaBridgePutDouble(channel->bridge, value);
  cleanupChannel(channel);
  return ok;
}

bool PvaChannelManager::putValue(const QString &pvName, const QString &value)
{
  if (shutdownComplete_) {
    return false;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_STRING;
  key.elementCount = 1;

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel) {
    return false;
  }
  const auto cleanupChannel = [this](PvaChannel *candidate) {
    if (candidate && candidate->subscribers.isEmpty()) {
      destroyChannelIfUnused(candidate);
    }
  };
  const bool ok = channel->bridge && channel->connected
      && pvaBridgePutString(channel->bridge, value.toStdString());
  cleanupChannel(channel);
  return ok;
}

bool PvaChannelManager::putValue(const QString &pvName, dbr_enum_t value)
{
  return putValue(pvName, static_cast<double>(value));
}

bool PvaChannelManager::putArrayValue(const QString &pvName,
    const QVector<double> &values)
{
  if (shutdownComplete_) {
    return false;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_TIME_DOUBLE;
  key.elementCount = values.size();

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel) {
    return false;
  }
  const auto cleanupChannel = [this](PvaChannel *candidate) {
    if (candidate && candidate->subscribers.isEmpty()) {
      destroyChannelIfUnused(candidate);
    }
  };
  const bool ok = channel->bridge && channel->connected
      && pvaBridgePutDoubleArray(channel->bridge, values.constData(),
          static_cast<size_t>(values.size()));
  cleanupChannel(channel);
  return ok;
}

int PvaChannelManager::uniqueChannelCount() const
{
  return channels_.size();
}

int PvaChannelManager::totalSubscriptionCount() const
{
  return subscriptionToChannel_.size();
}

int PvaChannelManager::connectedChannelCount() const
{
  int count = 0;
  for (auto *channel : channels_) {
    if (channel && channel->connected) {
      ++count;
    }
  }
  return count;
}

QList<ChannelSummary> PvaChannelManager::channelSummaries() const
{
  QList<ChannelSummary> summaries;
  const double elapsed = elapsedSecondsSinceReset();
  for (auto *channel : channels_) {
    if (!channel) {
      continue;
    }
    ChannelSummary summary;
    summary.pvName = channel->rawName;
    summary.connected = channel->connected;
    summary.writable = channel->canWrite;
    summary.subscriberCount = channel->subscribers.size();
    summary.updateCount = channel->updateCount;
    summary.updateRate = (elapsed > 0.0) ? channel->updateCount / elapsed : 0.0;
    summary.severity = channel->cachedData.severity;
    summaries.append(summary);
  }
  std::sort(summaries.begin(), summaries.end(),
      [](const ChannelSummary &a, const ChannelSummary &b) {
        return a.pvName < b.pvName;
      });
  return summaries;
}

void PvaChannelManager::resetUpdateCounters()
{
  for (auto *channel : channels_) {
    if (channel) {
      channel->updateCount = 0;
    }
  }
  statsTimer_.restart();
}

double PvaChannelManager::elapsedSecondsSinceReset() const
{
  return statsTimer_.isValid()
      ? static_cast<double>(statsTimer_.elapsed()) / 1000.0
      : 0.0;
}

void PvaChannelManager::shutdown()
{
  if (shutdownComplete_) {
    return;
  }

  shutdownComplete_ = true;
  pollTimer_.stop();
  for (auto *channel : channels_) {
    pvaBridgeDestroyChannel(channel->bridge);
    channel->bridge = nullptr;
    delete channel;
  }
  channels_.clear();
  subscriptionToChannel_.clear();
}
