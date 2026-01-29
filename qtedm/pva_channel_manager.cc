#include "pva_channel_manager.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QStringList>


namespace {
constexpr qint64 kMinNotifyIntervalMs = 100;
constexpr int kPollIntervalMs = 100;
constexpr double kConnectTimeoutSeconds = 1.0;
}

PvaChannelManager &PvaChannelManager::instance()
{
  static PvaChannelManager *manager = []() {
    auto *mgr = new PvaChannelManager;
    return mgr;
  }();
  return *manager;
}

PvaChannelManager::PvaChannelManager()
  : QObject(QCoreApplication::instance())
{
  statsTimer_.start();
  pollTimer_.setInterval(kPollIntervalMs);
  pollTimer_.setTimerType(Qt::CoarseTimer);
  connect(&pollTimer_, &QTimer::timeout, this, [this]() { pollChannels(); });
}

PvaChannelManager::~PvaChannelManager()
{
  pollTimer_.stop();
  for (auto *channel : channels_) {
    if (channel->pva) {
      freePVA(channel->pva);
      delete channel->pva;
      channel->pva = nullptr;
    }
    delete channel;
  }
  channels_.clear();
  subscriptionToChannel_.clear();
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

  if (pvName.trimmed().isEmpty() || !valueCallback) {
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
  if (channels_.isEmpty()) {
    pollTimer_.stop();
  }
}

PvaChannelManager::PvaChannel *PvaChannelManager::findOrCreateChannel(
    const SharedChannelKey &key,
    const QString &rawName,
    const QString &pvName)
{
  auto it = channels_.find(key);
  if (it != channels_.end()) {
    return it.value();
  }

  auto *channel = new PvaChannel;
  channel->key = key;
  channel->rawName = rawName.trimmed();
  channel->pvName = pvName.trimmed();

  channel->pva = new PVA_OVERALL();
  allocPVA(channel->pva, 1);
  channel->pva->includeAlarmSeverity = true;

  epics::pvData::shared_vector<std::string> names(1);
  names[0] = channel->pvName.toStdString();
  channel->pva->pvaChannelNames = freeze(names);

  epics::pvData::shared_vector<std::string> providers(1);
  providers[0] = "pva";
  channel->pva->pvaProvider = freeze(providers);

  ConnectPVA(channel->pva, kConnectTimeoutSeconds);
  GetPVAValues(channel->pva);
  MonitorPVAValues(channel->pva);

  if (channel->pva->isConnected.size() > 0) {
    channel->connected = channel->pva->isConnected[0];
  }
  updateAccessRights(channel);
  updateCachedData(channel);

  channels_.insert(key, channel);
  return channel;
}

void PvaChannelManager::destroyChannelIfUnused(PvaChannel *channel)
{
  if (!channel || !channel->subscribers.isEmpty()) {
    return;
  }

  channels_.remove(channel->key);
  if (channel->pva) {
    freePVA(channel->pva);
    delete channel->pva;
    channel->pva = nullptr;
  }
  delete channel;
}

void PvaChannelManager::updateAccessRights(PvaChannel *channel)
{
  if (!channel || !channel->pva) {
    return;
  }
  channel->canRead = HaveReadAccess(channel->pva, 0);
  channel->canWrite = HaveWriteAccess(channel->pva, 0);
}

void PvaChannelManager::updateCachedData(PvaChannel *channel)
{
  if (!channel || !channel->pva) {
    return;
  }

  if (channel->connected) {
    ExtractPVAUnits(channel->pva);
    ExtractPVAControlInfo(channel->pva);
  }

  SharedChannelData data = channel->cachedData;
  data.connected = channel->connected;
  data.severity = channel->pva->pvaData[0].alarmSeverity;
  data.status = 0;
  data.hasTimestamp = false;
  data.hasControlInfo = false;
  data.hasUnits = false;
  data.hasPrecision = false;
  data.hasValue = false;
  data.isNumeric = false;
  data.isString = false;
  data.isEnum = false;
  data.isCharArray = false;
  data.isArray = false;
  data.arrayValues.clear();
  data.charArrayValue.clear();

  const PVA_DATA_ALL_READINGS &reading = channel->pva->pvaData[0];
  const PVA_DATA *source = nullptr;
  long elementCount = 0;

  if (reading.numMonitorReadings > 0) {
    source = reading.monitorData;
    elementCount = reading.numMonitorElements;
  } else if (reading.numGetReadings > 0) {
    source = reading.getData;
    elementCount = reading.numGetElements;
  }

  if (source && reading.numeric && source[0].values) {
    data.numericValue = source[0].values[0];
    data.isNumeric = true;
    data.hasValue = true;
    if (elementCount > 1) {
      data.isArray = true;
      data.arrayValues.resize(static_cast<int>(elementCount));
      for (int i = 0; i < static_cast<int>(elementCount); ++i) {
        data.arrayValues[i] = source[0].values[i];
      }
    }
  }

  if (source && reading.nonnumeric && source[0].stringValues) {
    const char *text = source[0].stringValues[0];
    data.stringValue = text ? QString::fromUtf8(text) : QString();
    data.isString = true;
    data.hasValue = true;
  }

  if (IsEnumFieldType(channel->pva, 0)) {
    char **choices = nullptr;
    const uint32_t count = GetEnumChoices(channel->pva, 0, &choices);
    if (count > 0 && choices) {
      QStringList list;
      list.reserve(static_cast<int>(count));
      for (uint32_t i = 0; i < count; ++i) {
        QString choice = QString::fromUtf8(choices[i]);
        if (choice.startsWith('{') && choice.endsWith('}') && choice.size() > 1) {
          choice = choice.mid(1, choice.size() - 2);
        }
        list.append(choice);
        free(choices[i]);
      }
      free(choices);
      data.enumStrings = list;
      data.isEnum = true;
      data.enumValue = static_cast<dbr_enum_t>(data.numericValue);
      data.hasControlInfo = true;
    }
  }

  data.units = QString::fromStdString(GetUnits(channel->pva, 0));
  data.hasUnits = !data.units.trimmed().isEmpty();

  if (reading.hasDisplayLimits) {
    data.lopr = reading.displayLimitLow;
    data.hopr = reading.displayLimitHigh;
    data.hasControlInfo = true;
  } else if (reading.hasControlLimits) {
    data.lopr = reading.controlLimitLow;
    data.hopr = reading.controlLimitHigh;
    data.hasControlInfo = true;
  }

  if (reading.hasPrecision) {
    data.precision = static_cast<short>(reading.displayPrecision);
    data.hasPrecision = true;
    data.hasControlInfo = true;
  }

  if (IsEnumFieldType(channel->pva, 0)) {
    data.nativeFieldType = DBF_ENUM;
  } else if (reading.nonnumeric) {
    data.nativeFieldType = DBF_STRING;
  } else {
    data.nativeFieldType = DBF_DOUBLE;
  }

  if (elementCount <= 0) {
    elementCount = 1;
  }
  data.nativeElementCount = static_cast<long>(GetElementCount(channel->pva, 0));
  if (data.nativeElementCount <= 0) {
    data.nativeElementCount = elementCount;
  }

  channel->cachedData = data;
}

bool PvaChannelManager::getInfoSnapshot(const QString &pvName,
    PvaInfoSnapshot &snapshot)
{
  snapshot = PvaInfoSnapshot{};

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

  updateAccessRights(channel);
  updateCachedData(channel);

  const SharedChannelData &data = channel->cachedData;
  snapshot.pvName = parsed.rawName.trimmed();
  snapshot.connected = channel->connected;
  snapshot.canRead = channel->canRead;
  snapshot.canWrite = channel->canWrite;
  snapshot.fieldType = data.nativeFieldType;
  snapshot.elementCount = static_cast<unsigned long>(data.nativeElementCount);
  snapshot.host = QString::fromStdString(GetRemoteAddress(channel->pva, 0));
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

  return true;
}

void PvaChannelManager::notifySubscribers(PvaChannel *channel)
{
  if (!channel) {
    return;
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (now - channel->lastNotifyTimeMs < kMinNotifyIntervalMs) {
    return;
  }
  channel->lastNotifyTimeMs = now;

  if (!channel->cachedData.hasValue) {
    return;
  }

  for (const Subscriber &sub : channel->subscribers) {
    if (sub.valueCallback) {
      sub.valueCallback(channel->cachedData);
    }
  }
  channel->updateCount++;
}

void PvaChannelManager::pollChannels()
{
  if (channels_.isEmpty()) {
    return;
  }

  for (auto *channel : channels_) {
    if (!channel || !channel->pva) {
      continue;
    }

    const bool wasConnected = channel->connected;
    int events = PollMonitoredPVA(channel->pva);
    if (channel->pva->isConnected.size() > 0) {
      channel->connected = channel->pva->isConnected[0];
    }

    if (channel->connected != wasConnected) {
      updateAccessRights(channel);
      updateCachedData(channel);
      for (const Subscriber &sub : channel->subscribers) {
        if (sub.connectionCallback) {
          sub.connectionCallback(channel->connected, channel->cachedData);
        }
        if (sub.accessRightsCallback) {
          sub.accessRightsCallback(channel->canRead, channel->canWrite);
        }
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
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_TIME_DOUBLE;
  key.elementCount = 1;

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel || !channel->pva || !channel->connected) {
    return false;
  }

  PrepPut(channel->pva, 0, value);
  return PutPVAValues(channel->pva) == 0;
}

bool PvaChannelManager::putValue(const QString &pvName, const QString &value)
{
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_STRING;
  key.elementCount = 1;

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel || !channel->pva || !channel->connected) {
    return false;
  }

  QByteArray bytes = value.toUtf8();
  PrepPut(channel->pva, 0, bytes.data());
  return PutPVAValues(channel->pva) == 0;
}

bool PvaChannelManager::putValue(const QString &pvName, dbr_enum_t value)
{
  return putValue(pvName, static_cast<double>(value));
}

bool PvaChannelManager::putArrayValue(const QString &pvName, const QVector<double> &values)
{
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.pvName.isEmpty()) {
    return false;
  }

  SharedChannelKey key;
  key.pvName = parsed.rawName.trimmed();
  key.requestedType = DBR_TIME_DOUBLE;
  key.elementCount = values.size();

  PvaChannel *channel = findOrCreateChannel(key, parsed.rawName, parsed.pvName);
  if (!channel || !channel->pva || !channel->connected) {
    return false;
  }

  PrepPut(channel->pva, 0, const_cast<double *>(values.data()), values.size());
  return PutPVAValues(channel->pva) == 0;
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
