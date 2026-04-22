#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

#include "shared_channel_manager.h"

class SoftPvRegistry : public QObject, public SubscriptionOwner
{
public:
  static SoftPvRegistry &instance();

  void prepareName(const QString &name);
  void releasePreparedName(const QString &name);
  void registerName(const QString &name);
  void unregisterName(const QString &name);

  void publishValue(const QString &name, double value);
  void setConnected(const QString &name, bool connected);

  bool isRegistered(const QString &name) const;

  SubscriptionHandle subscribe(
      const QString &name,
      ChannelValueCallback valueCallback,
      ChannelConnectionCallback connectionCallback = nullptr,
      ChannelAccessRightsCallback accessRightsCallback = nullptr);

private:
  struct Subscriber
  {
    quint64 id = 0;
    ChannelValueCallback valueCallback;
    ChannelConnectionCallback connectionCallback;
    ChannelAccessRightsCallback accessRightsCallback;
  };

  struct Entry
  {
    QString name;
    int preparedCount = 0;
    int producerCount = 0;
    int connectedProducerCount = 0;
    bool hasValue = false;
    double value = 0.0;
    QVector<Subscriber> subscribers;
    int dispatchDepth = 0;
    bool cleanupPending = false;
  };

  SoftPvRegistry();
  ~SoftPvRegistry() override;

  void unsubscribe(quint64 subscriptionId) override;

  Entry *findEntry(const QString &name) const;
  Subscriber *findSubscriber(Entry *entry, quint64 id);
  SharedChannelData buildData(const Entry &entry) const;
  void dispatchConnectionCallbacks(Entry *entry);
  void dispatchValueCallbacks(Entry *entry);
  void cleanupEntryIfUnused(Entry *entry);
  void assertMainThread() const;

  QHash<QString, Entry *> entries_;
  QHash<quint64, Entry *> subscriptionToEntry_;
  quint64 nextSubscriptionId_ = 1;
};
