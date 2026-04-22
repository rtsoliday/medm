#include "soft_pv_registry.h"

#include <utility>

#include <QThread>

#include <db_access.h>

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

void SoftPvRegistry::registerName(const QString &name)
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
  ++entry->producerCount;
}

void SoftPvRegistry::unregisterName(const QString &name)
{
  assertMainThread();

  Entry *entry = findEntry(name);
  if (!entry || entry->producerCount <= 0) {
    return;
  }

  --entry->producerCount;
  if (entry->connectedProducerCount > entry->producerCount) {
    entry->connectedProducerCount = entry->producerCount;
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

  entry->value = value;
  entry->hasValue = true;
  dispatchValueCallbacks(entry);
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
    entry->hasValue = false;
    entry->value = 0.0;
  }

  dispatchConnectionCallbacks(entry);
  cleanupEntryIfUnused(entry);
}

bool SoftPvRegistry::isRegistered(const QString &name) const
{
  const Entry *entry = findEntry(name);
  return entry != nullptr
      && (entry->preparedCount > 0 || entry->producerCount > 0);
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
    subscriber.accessRightsCallback(true, false);
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

SharedChannelData SoftPvRegistry::buildData(const Entry &entry) const
{
  SharedChannelData data;
  data.connected = entry.connectedProducerCount > 0;
  data.nativeFieldType = DBF_DOUBLE;
  data.nativeElementCount = 1;
  data.severity = 0;
  data.status = 0;

  if (entry.hasValue) {
    data.numericValue = entry.value;
    data.hasValue = true;
    data.isNumeric = true;
  }

  return data;
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
