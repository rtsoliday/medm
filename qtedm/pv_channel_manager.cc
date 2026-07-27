#include "pv_channel_manager.h"

#include <algorithm>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "pva_channel_manager.h"
#include "plugin_manager.h"
#include "shared_channel_manager.h"
#include "soft_pv_registry.h"

PvChannelManager &PvChannelManager::instance()
{
  static PvChannelManager manager;
  return manager;
}

SubscriptionHandle PvChannelManager::subscribe(
    const QString &pvName,
    chtype requestedType,
    long elementCount,
    ChannelValueCallback valueCallback,
    ChannelConnectionCallback connectionCallback,
    ChannelAccessRightsCallback accessRightsCallback,
    ChannelDeliveryMode deliveryMode)
{
  if (accessRightsCallback) {
    ChannelAccessRightsCallback requestedCallback =
        std::move(accessRightsCallback);
    accessRightsCallback =
        [this, callback = std::move(requestedCallback)](
            bool canRead, bool canWrite) {
          callback(canRead, canWrite && !isObserveOnly());
        };
  }

  if (QtedmPluginManager::instance().supportsDataProvider(pvName)) {
    return QtedmPluginManager::instance().subscribeDataProvider(pvName,
        std::move(valueCallback), std::move(connectionCallback),
        std::move(accessRightsCallback), deliveryMode);
  }

  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kCa
      && SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
    return SoftPvRegistry::instance().subscribe(parsed.pvName,
        std::move(valueCallback), std::move(connectionCallback),
        std::move(accessRightsCallback), deliveryMode);
  }

  ChannelAccessContext::instance().ensureInitializedForProtocol(parsed.protocol);

  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().subscribe(pvName, requestedType,
        elementCount, std::move(valueCallback), std::move(connectionCallback),
        std::move(accessRightsCallback), deliveryMode);
  }

  return SharedChannelManager::instance().subscribe(pvName, requestedType,
      elementCount, std::move(valueCallback), std::move(connectionCallback),
      std::move(accessRightsCallback), deliveryMode);
}

bool PvChannelManager::putValue(const QString &pvName, double value)
{
  if (rejectWrite(pvName, QString::number(value, 'g', 15))) {
    return false;
  }
  if (QtedmPluginManager::instance().supportsDataProvider(pvName)) {
    QString error;
    const bool success = QtedmPluginManager::instance().putDataProvider(
        pvName, value, &error);
    if (success) {
      AuditLogger::instance().logPut(pvName, value,
          QStringLiteral("PluginDataProvider"));
    }
    return success;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kCa
      && SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
    return SoftPvRegistry::instance().putValue(parsed.pvName, value);
  }
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName, value);
  }
  return SharedChannelManager::instance().putValue(pvName, value);
}

bool PvChannelManager::putValue(const QString &pvName, const QString &value)
{
  if (rejectWrite(pvName, value)) {
    return false;
  }
  if (QtedmPluginManager::instance().supportsDataProvider(pvName)) {
    QString error;
    const bool success = QtedmPluginManager::instance().putDataProvider(
        pvName, value, &error);
    if (success) {
      AuditLogger::instance().logPut(pvName, value,
          QStringLiteral("PluginDataProvider"));
    }
    return success;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kCa
      && SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
    return SoftPvRegistry::instance().putValue(parsed.pvName, value);
  }
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName, value);
  }
  return SharedChannelManager::instance().putValue(pvName, value);
}

bool PvChannelManager::putValue(const QString &pvName, dbr_enum_t value)
{
  if (rejectWrite(pvName, QString::number(value))) {
    return false;
  }
  if (QtedmPluginManager::instance().supportsDataProvider(pvName)) {
    QString error;
    const bool success = QtedmPluginManager::instance().putDataProvider(
        pvName, QVariant::fromValue(static_cast<unsigned int>(value)), &error);
    if (success) {
      AuditLogger::instance().logPut(pvName, static_cast<int>(value),
          QStringLiteral("PluginDataProvider"));
    }
    return success;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kCa
      && SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
    return SoftPvRegistry::instance().putValue(parsed.pvName, value);
  }
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName, value);
  }
  return SharedChannelManager::instance().putValue(pvName, value);
}

bool PvChannelManager::putCharArrayValue(const QString &pvName,
    const QByteArray &value)
{
  if (rejectWrite(pvName, QStringLiteral("<char-array:%1 bytes>")
          .arg(value.size()))) {
    return false;
  }
  if (QtedmPluginManager::instance().supportsDataProvider(pvName)) {
    QString error;
    const bool success = QtedmPluginManager::instance().putDataProvider(
        pvName, value, &error);
    if (success) {
      AuditLogger::instance().logPut(pvName,
          QStringLiteral("<char-array:%1 bytes>").arg(value.size()),
          QStringLiteral("PluginDataProvider"));
    }
    return success;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kCa
      && SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
    return SoftPvRegistry::instance().putCharArrayValue(parsed.pvName, value);
  }
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName,
        QString::fromLatin1(value));
  }
  return SharedChannelManager::instance().putCharArrayValue(pvName, value);
}

bool PvChannelManager::putArrayValue(const QString &pvName,
    const QVector<double> &values)
{
  if (rejectWrite(pvName, QStringLiteral("<numeric-array:%1 elements>")
          .arg(values.size()))) {
    return false;
  }
  if (QtedmPluginManager::instance().supportsDataProvider(pvName)) {
    QVariantList list;
    list.reserve(values.size());
    for (double value : values) {
      list.append(value);
    }
    QString error;
    const bool success = QtedmPluginManager::instance().putDataProvider(
        pvName, list, &error);
    if (success) {
      AuditLogger::instance().logPut(pvName,
          QStringLiteral("<numeric-array:%1 elements>").arg(values.size()),
          QStringLiteral("PluginDataProvider"));
    }
    return success;
  }
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kCa
      && SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
    return SoftPvRegistry::instance().putArrayValue(parsed.pvName, values);
  }
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putArrayValue(pvName, values);
  }
  return SharedChannelManager::instance().putArrayValue(pvName, values);
}

int PvChannelManager::uniqueChannelCount() const
{
  return SharedChannelManager::instance().uniqueChannelCount()
      + PvaChannelManager::instance().uniqueChannelCount();
}

int PvChannelManager::totalSubscriptionCount() const
{
  return SharedChannelManager::instance().totalSubscriptionCount()
      + PvaChannelManager::instance().totalSubscriptionCount();
}

int PvChannelManager::connectedChannelCount() const
{
  return SharedChannelManager::instance().connectedChannelCount()
      + PvaChannelManager::instance().connectedChannelCount();
}

QList<ChannelSummary> PvChannelManager::channelSummaries() const
{
  QList<ChannelSummary> summaries = SharedChannelManager::instance().channelSummaries();
  summaries.append(PvaChannelManager::instance().channelSummaries());
  std::sort(summaries.begin(), summaries.end(),
      [](const ChannelSummary &a, const ChannelSummary &b) {
        return a.pvName < b.pvName;
      });
  return summaries;
}

void PvChannelManager::resetUpdateCounters()
{
  SharedChannelManager::instance().resetUpdateCounters();
  PvaChannelManager::instance().resetUpdateCounters();
}

double PvChannelManager::elapsedSecondsSinceReset() const
{
  return std::max(SharedChannelManager::instance().elapsedSecondsSinceReset(),
      PvaChannelManager::instance().elapsedSecondsSinceReset());
}

void PvChannelManager::setObserveOnly(bool observeOnly)
{
  observeOnly_.store(observeOnly, std::memory_order_release);
}

bool PvChannelManager::isObserveOnly() const
{
  return observeOnly_.load(std::memory_order_acquire);
}

bool PvChannelManager::rejectWrite(const QString &pvName,
    const QString &valueDescription)
{
  if (!isObserveOnly()) {
    return false;
  }
  AuditLogger::instance().logBlockedPut(pvName, valueDescription,
      QStringLiteral("ObserveOnly"));
  return true;
}
