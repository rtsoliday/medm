#pragma once

#include "pv_protocol.h"
#include "pva_channel_manager.h"
#include "shared_channel_manager.h"

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
      ChannelAccessRightsCallback accessRightsCallback = nullptr);

  bool putValue(const QString &pvName, double value);
  bool putValue(const QString &pvName, const QString &value);
  bool putValue(const QString &pvName, dbr_enum_t value);
  bool putCharArrayValue(const QString &pvName, const QByteArray &value);
  bool putArrayValue(const QString &pvName, const QVector<double> &values);

  int uniqueChannelCount() const;
  int totalSubscriptionCount() const;
  int connectedChannelCount() const;
  QList<ChannelSummary> channelSummaries() const;
  void resetUpdateCounters();
  double elapsedSecondsSinceReset() const;

private:
  PvChannelManager() = default;
};
