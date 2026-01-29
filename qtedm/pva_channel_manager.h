#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>

#include "pv_protocol.h"
#include "shared_channel_manager.h"
#include "pvaSDDS.h"

class PvaChannelManager : public QObject, public SubscriptionOwner
{
public:
  static PvaChannelManager &instance();

  struct PvaInfoSnapshot
  {
    QString pvName;
    bool connected = false;
    bool canRead = false;
    bool canWrite = false;
    int fieldType = -1;
    unsigned long elementCount = 0;
    QString host;
    QString value;
    bool hasValue = false;
    short severity = 0;
    double hopr = 0.0;
    double lopr = 0.0;
    bool hasLimits = false;
    int precision = -1;
    bool hasPrecision = false;
    QString units;
    bool hasUnits = false;
    QStringList states;
    bool hasStates = false;
  };

  SubscriptionHandle subscribe(
      const QString &pvName,
      chtype requestedType,
      long elementCount,
      ChannelValueCallback valueCallback,
      ChannelConnectionCallback connectionCallback = nullptr,
      ChannelAccessRightsCallback accessRightsCallback = nullptr);

  bool getInfoSnapshot(const QString &pvName, PvaInfoSnapshot &snapshot);

  bool putValue(const QString &pvName, double value);
  bool putValue(const QString &pvName, const QString &value);
  bool putValue(const QString &pvName, dbr_enum_t value);
  bool putArrayValue(const QString &pvName, const QVector<double> &values);

  int uniqueChannelCount() const;
  int totalSubscriptionCount() const;
  int connectedChannelCount() const;
  QList<ChannelSummary> channelSummaries() const;
  void resetUpdateCounters();
  double elapsedSecondsSinceReset() const;

  void unsubscribe(quint64 subscriptionId) override;

private:
  PvaChannelManager();
  ~PvaChannelManager() override;

  struct Subscriber
  {
    quint64 id = 0;
    ChannelValueCallback valueCallback;
    ChannelConnectionCallback connectionCallback;
    ChannelAccessRightsCallback accessRightsCallback;
  };

  struct PvaChannel
  {
    SharedChannelKey key;
    QString rawName;
    QString pvName;
    PVA_OVERALL *pva = nullptr;
    bool connected = false;
    bool canRead = false;
    bool canWrite = false;
    SharedChannelData cachedData;
    QList<Subscriber> subscribers;
    int updateCount = 0;
    qint64 lastNotifyTimeMs = 0;
  };

  PvaChannel *findOrCreateChannel(const SharedChannelKey &key,
      const QString &rawName, const QString &pvName);
  void destroyChannelIfUnused(PvaChannel *channel);
  void updateCachedData(PvaChannel *channel);
  void notifySubscribers(PvaChannel *channel);
  void updateAccessRights(PvaChannel *channel);
  void pollChannels();

  QHash<SharedChannelKey, PvaChannel *> channels_;
  QHash<quint64, PvaChannel *> subscriptionToChannel_;
  quint64 nextSubscriptionId_ = 1;
  QElapsedTimer statsTimer_;
  QTimer pollTimer_;
};
