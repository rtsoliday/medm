#pragma once

#include <atomic>

#include "channel_subscription.h"
#include "pv_protocol.h"

class PvChannelManager
{
public:
  static PvChannelManager &instance();

  SubscriptionHandle subscribe(
      const QString &pvName,
      chtype requestedType,
      long elementCount,
      ChannelValueCallback valueCallback,
      ChannelConnectionCallback connectionCallback = nullptr,
      ChannelAccessRightsCallback accessRightsCallback = nullptr,
      ChannelDeliveryMode deliveryMode = ChannelDeliveryMode::kPassive);

  /* Callers that append an attributed success record pass false to avoid
   * a second provider record. Observe-only blocked records are unconditional. */
  bool putValue(const QString &pvName, double value,
      bool logProviderPut = true);
  bool putValue(const QString &pvName, const QString &value,
      bool logProviderPut = true);
  bool putValue(const QString &pvName, dbr_enum_t value,
      bool logProviderPut = true);
  bool putCharArrayValue(const QString &pvName, const QByteArray &value,
      bool logProviderPut = true);
  bool putArrayValue(const QString &pvName, const QVector<double> &values,
      bool logProviderPut = true);

  void setObserveOnly(bool observeOnly);
  bool isObserveOnly() const;

  int uniqueChannelCount() const;
  int totalSubscriptionCount() const;
  int connectedChannelCount() const;
  QList<ChannelSummary> channelSummaries() const;
  void resetUpdateCounters();
  double elapsedSecondsSinceReset() const;

private:
  PvChannelManager() = default;

  bool rejectWrite(const QString &pvName, const QString &valueDescription);

  std::atomic_bool observeOnly_{false};
};
