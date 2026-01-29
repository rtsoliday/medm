#include "pv_channel_manager.h"

#include <algorithm>

#include "channel_access_context.h"

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
    ChannelAccessRightsCallback accessRightsCallback)
{
  ParsedPvName parsed = parsePvName(pvName);
  ChannelAccessContext::instance().ensureInitializedForProtocol(parsed.protocol);

  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().subscribe(pvName, requestedType,
        elementCount, std::move(valueCallback), std::move(connectionCallback),
        std::move(accessRightsCallback));
  }

  return SharedChannelManager::instance().subscribe(pvName, requestedType,
      elementCount, std::move(valueCallback), std::move(connectionCallback),
      std::move(accessRightsCallback));
}

bool PvChannelManager::putValue(const QString &pvName, double value)
{
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName, value);
  }
  return SharedChannelManager::instance().putValue(pvName, value);
}

bool PvChannelManager::putValue(const QString &pvName, const QString &value)
{
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName, value);
  }
  return SharedChannelManager::instance().putValue(pvName, value);
}

bool PvChannelManager::putValue(const QString &pvName, dbr_enum_t value)
{
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName, value);
  }
  return SharedChannelManager::instance().putValue(pvName, value);
}

bool PvChannelManager::putCharArrayValue(const QString &pvName,
    const QByteArray &value)
{
  ParsedPvName parsed = parsePvName(pvName);
  if (parsed.protocol == PvProtocol::kPva) {
    return PvaChannelManager::instance().putValue(pvName,
        QString::fromLatin1(value));
  }
  return SharedChannelManager::instance().putCharArrayValue(pvName, value);
}

bool PvChannelManager::putArrayValue(const QString &pvName,
    const QVector<double> &values)
{
  ParsedPvName parsed = parsePvName(pvName);
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
