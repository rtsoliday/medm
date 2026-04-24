#include "soft_pv_registry.h"

#include <utility>

#include <QThread>

#include <db_access.h>
#include <epicsTime.h>

namespace {

QString normalizedSoftPvName(const QString &value)
{
  return value.trimmed();
}

} // namespace

SoftPvRegistry &SoftPvRegistry::instance()
{
  static SoftPvRegistry registry;
  return registry;
}

SoftPvRegistry::SoftPvRegistry()
  : QObject(nullptr)
{
}

SoftPvRegistry::~SoftPvRegistry()
{
  qDeleteAll(entries_);
  entries_.clear();
  subscriptionToEntry_.clear();
}

void SoftPvRegistry::prepareName(const QString &name)
{
  assertMainThread();

  const QString normalized = normalizedSoftPvName(name);
  if (normalized.isEmpty()) {
    return;
  }

  Entry *entry = findEntry(normalized);
  if (!entry) {
    entry = new Entry;
    entry->name = normalized;
    entries_.insert(normalized, entry);
  }
  ++entry->preparedCount;
}

void SoftPvRegistry::releasePreparedName(const QString &name)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->preparedCount <= 0) {
    return;
  }

  --entry->preparedCount;
  cleanupEntryIfUnused(entry);
}

void SoftPvRegistry::registerName(const QString &name, bool writable)
{
  assertMainThread();

  const QString normalized = normalizedSoftPvName(name);
  if (normalized.isEmpty()) {
    return;
  }

  Entry *entry = findOrCreateEntry(normalized);
  const bool wasWritable = entry->writableProducerCount > 0;
  ++entry->producerCount;
  if (writable) {
    ++entry->writableProducerCount;
  }
  if (wasWritable != (entry->writableProducerCount > 0)) {
    dispatchAccessRightsCallbacks(entry);
  }
}

void SoftPvRegistry::unregisterName(const QString &name, bool writable)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->producerCount <= 0) {
    return;
  }

  const bool wasWritable = entry->writableProducerCount > 0;
  --entry->producerCount;
  if (writable && entry->writableProducerCount > 0) {
    --entry->writableProducerCount;
  }
  if (entry->writableProducerCount > entry->producerCount) {
    entry->writableProducerCount = entry->producerCount;
  }
  if (entry->connectedProducerCount > entry->producerCount) {
    entry->connectedProducerCount = entry->producerCount;
  }
  if (wasWritable != (entry->writableProducerCount > 0)) {
    dispatchAccessRightsCallbacks(entry);
  }

  cleanupEntryIfUnused(entry);
}

void SoftPvRegistry::publishValue(const QString &name, double value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->valueKind = ValueKind::kNumeric;
  entry->value = value;
  entry->hasValue = true;
  entry->stringValue.clear();
  entry->charArrayValue.clear();
  entry->arrayValues.clear();
  stampValue(entry);
  dispatchValueCallbacks(entry);
}

void SoftPvRegistry::publishStringValue(const QString &name,
    const QString &value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->valueKind = ValueKind::kString;
  entry->stringValue = value;
  entry->value = value.toDouble();
  entry->hasValue = true;
  entry->charArrayValue.clear();
  entry->arrayValues.clear();
  stampValue(entry);
  dispatchValueCallbacks(entry);
}

void SoftPvRegistry::publishEnumValue(const QString &name,
    dbr_enum_t value, const QStringList &labels)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->valueKind = ValueKind::kEnum;
  entry->enumValue = value;
  entry->enumStrings = labels;
  entry->value = static_cast<double>(value);
  entry->hasValue = true;
  entry->stringValue.clear();
  if (value < static_cast<dbr_enum_t>(labels.size())) {
    entry->stringValue = labels.at(static_cast<int>(value));
  }
  entry->charArrayValue.clear();
  entry->arrayValues.clear();
  stampValue(entry);
  dispatchValueCallbacks(entry);
}

void SoftPvRegistry::publishCharArrayValue(const QString &name,
    const QByteArray &value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->valueKind = ValueKind::kCharArray;
  entry->charArrayValue = value;
  entry->stringValue = QString::fromLatin1(value.constData(),
      value.indexOf('\0') >= 0 ? value.indexOf('\0') : value.size());
  entry->value = value.isEmpty()
      ? 0.0
      : static_cast<double>(static_cast<unsigned char>(value.at(0)));
  entry->hasValue = true;
  entry->arrayValues.clear();
  stampValue(entry);
  dispatchValueCallbacks(entry);
}

void SoftPvRegistry::publishArrayValue(const QString &name,
    const QVector<double> &values)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->valueKind = ValueKind::kArray;
  entry->arrayValues = values;
  entry->value = values.isEmpty() ? 0.0 : values.first();
  entry->hasValue = true;
  entry->stringValue.clear();
  entry->charArrayValue.clear();
  stampValue(entry);
  dispatchValueCallbacks(entry);
}

void SoftPvRegistry::setControlInfo(const QString &name, double low,
    double high, short precision, const QString &units)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->low = low;
  entry->high = high;
  if (entry->high < entry->low) {
    std::swap(entry->low, entry->high);
  }
  if (entry->high == entry->low) {
    entry->high = entry->low + 1.0;
  }
  entry->precision = precision;
  entry->units = units;
  entry->hasControlInfo = true;
  if (entry->hasValue) {
    dispatchValueCallbacks(entry);
  }
}

void SoftPvRegistry::setExpressionChannelInfo(const QString &name,
    const QString &calc, const QStringList &channels)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->producedByExpressionChannel = true;
  entry->expressionCalc = calc.trimmed();
  entry->expressionChannels.clear();
  for (const QString &channel : channels) {
    entry->expressionChannels.append(channel.trimmed());
  }
}

void SoftPvRegistry::clearExpressionChannelInfo(const QString &name)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  entry->producedByExpressionChannel = false;
  entry->expressionCalc.clear();
  entry->expressionChannels.clear();
}

void SoftPvRegistry::setConnected(const QString &name, bool connected)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return;
  }

  const bool wasConnected = entry->connectedProducerCount > 0;
  if (connected) {
    ++entry->connectedProducerCount;
  } else if (entry->connectedProducerCount > 0) {
    --entry->connectedProducerCount;
  } else {
    return;
  }

  const bool isConnected = entry->connectedProducerCount > 0;
  if (wasConnected == isConnected) {
    return;
  }

  if (!isConnected) {
    clearValue(entry);
  }

  dispatchConnectionCallbacks(entry);
  cleanupEntryIfUnused(entry);
}

bool SoftPvRegistry::putValue(const QString &name, double value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->writableProducerCount <= 0) {
    return false;
  }

  publishValue(name, value);
  return true;
}

bool SoftPvRegistry::putValue(const QString &name, const QString &value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->writableProducerCount <= 0) {
    return false;
  }

  if (entry->valueKind == ValueKind::kNumeric
      || entry->valueKind == ValueKind::kArray) {
    bool ok = false;
    const double numeric = value.trimmed().toDouble(&ok);
    if (!ok) {
      return false;
    }
    publishValue(name, numeric);
    return true;
  }

  publishStringValue(name, value);
  return true;
}

bool SoftPvRegistry::putValue(const QString &name, dbr_enum_t value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->writableProducerCount <= 0) {
    return false;
  }

  publishEnumValue(name, value, entry->enumStrings);
  return true;
}

bool SoftPvRegistry::putCharArrayValue(const QString &name,
    const QByteArray &value)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->writableProducerCount <= 0) {
    return false;
  }

  publishCharArrayValue(name, value);
  return true;
}

bool SoftPvRegistry::putArrayValue(const QString &name,
    const QVector<double> &values)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->writableProducerCount <= 0) {
    return false;
  }

  publishArrayValue(name, values);
  return true;
}

bool SoftPvRegistry::isRegistered(const QString &name) const
{
  const Entry *entry = findEntry(name);
  return entry != nullptr
      && (entry->preparedCount > 0 || entry->producerCount > 0);
}

bool SoftPvRegistry::infoSnapshot(const QString &name,
    SoftPvInfoSnapshot &snapshot) const
{
  assertMainThread();

  snapshot = SoftPvInfoSnapshot{};

  const Entry *entry = findEntry(name);
  if (!entry) {
    return false;
  }

  snapshot.name = entry->name;
  snapshot.registered = (entry->preparedCount > 0 || entry->producerCount > 0);
  snapshot.connected = entry->connectedProducerCount > 0;
  snapshot.hasValue = entry->hasValue;
  snapshot.value = entry->value;
  snapshot.timestamp = entry->timestamp;
  snapshot.hasTimestamp = entry->hasTimestamp;
  snapshot.preparedCount = entry->preparedCount;
  snapshot.producerCount = entry->producerCount;
  snapshot.connectedProducerCount = entry->connectedProducerCount;
  snapshot.subscriberCount = entry->subscribers.size();
  snapshot.producedByExpressionChannel =
      entry->producedByExpressionChannel;
  snapshot.expressionCalc = entry->expressionCalc;
  snapshot.expressionChannels = entry->expressionChannels;
  return true;
}

SubscriptionHandle SoftPvRegistry::subscribe(const QString &name,
    ChannelValueCallback valueCallback,
    ChannelConnectionCallback connectionCallback,
    ChannelAccessRightsCallback accessRightsCallback)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry) {
    return SubscriptionHandle();
  }

  Subscriber subscriber;
  subscriber.id = nextSubscriptionId_++;
  subscriber.valueCallback = std::move(valueCallback);
  subscriber.connectionCallback = std::move(connectionCallback);
  subscriber.accessRightsCallback = std::move(accessRightsCallback);
  entry->subscribers.append(subscriber);
  subscriptionToEntry_.insert(subscriber.id, entry);

  SharedChannelData data = buildData(*entry);
  if (subscriber.connectionCallback) {
    subscriber.connectionCallback(data.connected, data);
  }
  if (subscriber.accessRightsCallback) {
    subscriber.accessRightsCallback(true, entry->writableProducerCount > 0);
  }
  if (data.connected && data.hasValue && subscriber.valueCallback) {
    subscriber.valueCallback(data);
  }

  return SubscriptionHandle(subscriber.id, this);
}

void SoftPvRegistry::unsubscribe(quint64 subscriptionId)
{
  assertMainThread();

  auto it = subscriptionToEntry_.find(subscriptionId);
  if (it == subscriptionToEntry_.end()) {
    return;
  }

  Entry *entry = it.value();
  subscriptionToEntry_.erase(it);
  if (!entry) {
    return;
  }

  for (auto subIt = entry->subscribers.begin();
       subIt != entry->subscribers.end(); ++subIt) {
    if (subIt->id == subscriptionId) {
      entry->subscribers.erase(subIt);
      break;
    }
  }

  cleanupEntryIfUnused(entry);
}

SoftPvRegistry::Entry *SoftPvRegistry::findEntry(const QString &name) const
{
  const QString normalized = normalizedSoftPvName(name);
  if (normalized.isEmpty()) {
    return nullptr;
  }
  auto it = entries_.find(normalized);
  return it == entries_.end() ? nullptr : it.value();
}

SoftPvRegistry::Entry *SoftPvRegistry::findOrCreateEntry(const QString &name)
{
  const QString normalized = normalizedSoftPvName(name);
  if (normalized.isEmpty()) {
    return nullptr;
  }
  Entry *entry = findEntry(normalized);
  if (entry) {
    return entry;
  }
  entry = new Entry;
  entry->name = normalized;
  entries_.insert(normalized, entry);
  return entry;
}

SoftPvRegistry::Subscriber *SoftPvRegistry::findSubscriber(Entry *entry,
    quint64 id)
{
  if (!entry) {
    return nullptr;
  }

  for (Subscriber &subscriber : entry->subscribers) {
    if (subscriber.id == id) {
      return &subscriber;
    }
  }
  return nullptr;
}

void SoftPvRegistry::stampValue(Entry *entry)
{
  if (!entry) {
    return;
  }
  epicsTimeGetCurrent(&entry->timestamp);
  entry->hasTimestamp = true;
}

void SoftPvRegistry::clearValue(Entry *entry)
{
  if (!entry) {
    return;
  }
  entry->valueKind = ValueKind::kNone;
  entry->hasValue = false;
  entry->value = 0.0;
  entry->stringValue.clear();
  entry->enumValue = 0;
  entry->charArrayValue.clear();
  entry->arrayValues.clear();
  entry->hasTimestamp = false;
  entry->timestamp = epicsTimeStamp{};
}

SharedChannelData SoftPvRegistry::buildData(const Entry &entry) const
{
  SharedChannelData data;
  data.connected = entry.connectedProducerCount > 0;
  data.nativeFieldType = DBF_DOUBLE;
  data.nativeElementCount = 1;
  data.severity = 0;
  data.status = 0;
  if (entry.hasControlInfo) {
    data.lopr = entry.low;
    data.hopr = entry.high;
    data.precision = entry.precision;
    data.units = entry.units;
    data.hasControlInfo = true;
    data.hasPrecision = entry.precision >= 0;
    data.hasUnits = !entry.units.trimmed().isEmpty();
  }

  if (entry.hasValue) {
    data.hasValue = true;
    data.timestamp = entry.timestamp;
    data.hasTimestamp = entry.hasTimestamp;

    switch (entry.valueKind) {
    case ValueKind::kString:
      data.nativeFieldType = DBF_STRING;
      data.nativeElementCount = 1;
      data.stringValue = entry.stringValue;
      data.isString = true;
      data.numericValue = entry.value;
      break;
    case ValueKind::kEnum:
      data.nativeFieldType = DBF_ENUM;
      data.nativeElementCount = 1;
      data.enumValue = entry.enumValue;
      data.enumStrings = entry.enumStrings;
      data.numericValue = static_cast<double>(entry.enumValue);
      data.isEnum = true;
      data.isNumeric = true;
      data.hasControlInfo = data.hasControlInfo || !entry.enumStrings.isEmpty();
      if (entry.enumValue
          < static_cast<dbr_enum_t>(entry.enumStrings.size())) {
        data.stringValue = entry.enumStrings.at(
            static_cast<int>(entry.enumValue));
        data.isString = true;
      }
      break;
    case ValueKind::kCharArray:
      data.nativeFieldType = DBF_CHAR;
      data.nativeElementCount = entry.charArrayValue.size();
      data.charArrayValue = entry.charArrayValue;
      data.stringValue = entry.stringValue;
      data.numericValue = entry.value;
      data.isCharArray = true;
      data.isString = true;
      data.isNumeric = true;
      break;
    case ValueKind::kArray:
      data.nativeFieldType = DBF_DOUBLE;
      data.nativeElementCount = entry.arrayValues.size();
      data.arrayValues = entry.arrayValues;
      data.numericValue = entry.value;
      data.isArray = true;
      data.isNumeric = true;
      break;
    case ValueKind::kNumeric:
    case ValueKind::kNone:
      data.nativeFieldType = DBF_DOUBLE;
      data.nativeElementCount = 1;
      data.numericValue = entry.value;
      data.isNumeric = true;
      break;
    }
  }

  return data;
}

void SoftPvRegistry::dispatchAccessRightsCallbacks(Entry *entry)
{
  if (!entry) {
    return;
  }

  QVector<quint64> subscriberIds;
  subscriberIds.reserve(entry->subscribers.size());
  for (const Subscriber &subscriber : std::as_const(entry->subscribers)) {
    subscriberIds.append(subscriber.id);
  }

  const bool writable = entry->writableProducerCount > 0;
  ++entry->dispatchDepth;
  for (quint64 subscriberId : subscriberIds) {
    if (Subscriber *subscriber = findSubscriber(entry, subscriberId)) {
      if (subscriber->accessRightsCallback) {
        subscriber->accessRightsCallback(true, writable);
      }
    }
  }
  --entry->dispatchDepth;

  if (entry->dispatchDepth == 0 && entry->cleanupPending) {
    cleanupEntryIfUnused(entry);
  }
}

void SoftPvRegistry::dispatchConnectionCallbacks(Entry *entry)
{
  if (!entry) {
    return;
  }

  QVector<quint64> subscriberIds;
  subscriberIds.reserve(entry->subscribers.size());
  for (const Subscriber &subscriber : std::as_const(entry->subscribers)) {
    subscriberIds.append(subscriber.id);
  }

  const SharedChannelData data = buildData(*entry);
  ++entry->dispatchDepth;
  for (quint64 subscriberId : subscriberIds) {
    if (Subscriber *subscriber = findSubscriber(entry, subscriberId)) {
      if (subscriber->connectionCallback) {
        subscriber->connectionCallback(data.connected, data);
      }
    }
  }
  --entry->dispatchDepth;

  if (entry->dispatchDepth == 0 && entry->cleanupPending) {
    cleanupEntryIfUnused(entry);
  }
}

void SoftPvRegistry::dispatchValueCallbacks(Entry *entry)
{
  if (!entry) {
    return;
  }

  QVector<quint64> subscriberIds;
  subscriberIds.reserve(entry->subscribers.size());
  for (const Subscriber &subscriber : std::as_const(entry->subscribers)) {
    subscriberIds.append(subscriber.id);
  }

  const SharedChannelData data = buildData(*entry);
  ++entry->dispatchDepth;
  for (quint64 subscriberId : subscriberIds) {
    if (Subscriber *subscriber = findSubscriber(entry, subscriberId)) {
      if (subscriber->valueCallback) {
        subscriber->valueCallback(data);
      }
    }
  }
  --entry->dispatchDepth;

  if (entry->dispatchDepth == 0 && entry->cleanupPending) {
    cleanupEntryIfUnused(entry);
  }
}

void SoftPvRegistry::cleanupEntryIfUnused(Entry *entry)
{
  if (!entry) {
    return;
  }
  if (entry->preparedCount > 0 || entry->producerCount > 0
      || !entry->subscribers.isEmpty()) {
    entry->cleanupPending = false;
    return;
  }
  if (entry->dispatchDepth > 0) {
    entry->cleanupPending = true;
    return;
  }

  entry->cleanupPending = false;
  entries_.remove(entry->name);
  delete entry;
}

void SoftPvRegistry::assertMainThread() const
{
  Q_ASSERT(thread() == QThread::currentThread());
}
