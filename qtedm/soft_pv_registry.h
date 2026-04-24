#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "shared_channel_manager.h"

struct SoftPvInfoSnapshot
{
  QString name;
  bool registered = false;
  bool connected = false;
  bool hasValue = false;
  double value = 0.0;
  epicsTimeStamp timestamp{};
  bool hasTimestamp = false;
  int preparedCount = 0;
  int producerCount = 0;
  int connectedProducerCount = 0;
  int subscriberCount = 0;
  bool producedByExpressionChannel = false;
  QString expressionCalc;
  QStringList expressionChannels;
};

class SoftPvRegistry : public QObject, public SubscriptionOwner
{
public:
  static SoftPvRegistry &instance();

  void prepareName(const QString &name);
  void releasePreparedName(const QString &name);
  void registerName(const QString &name, bool writable = false);
  void unregisterName(const QString &name, bool writable = false);

  void publishValue(const QString &name, double value);
  void publishStringValue(const QString &name, const QString &value);
  void publishEnumValue(const QString &name, dbr_enum_t value,
      const QStringList &labels);
  void publishCharArrayValue(const QString &name, const QByteArray &value);
  void publishArrayValue(const QString &name, const QVector<double> &values);
  void setControlInfo(const QString &name, double low, double high,
      short precision, const QString &units = QString());
  void setExpressionChannelInfo(const QString &name, const QString &calc,
      const QStringList &channels);
  void clearExpressionChannelInfo(const QString &name);
  void setConnected(const QString &name, bool connected);

  bool putValue(const QString &name, double value);
  bool putValue(const QString &name, const QString &value);
  bool putValue(const QString &name, dbr_enum_t value);
  bool putCharArrayValue(const QString &name, const QByteArray &value);
  bool putArrayValue(const QString &name, const QVector<double> &values);

  bool isRegistered(const QString &name) const;
  bool infoSnapshot(const QString &name, SoftPvInfoSnapshot &snapshot) const;

  SubscriptionHandle subscribe(
      const QString &name,
      ChannelValueCallback valueCallback,
      ChannelConnectionCallback connectionCallback = nullptr,
      ChannelAccessRightsCallback accessRightsCallback = nullptr);

private:
  enum class ValueKind
  {
    kNone,
    kNumeric,
    kString,
    kEnum,
    kCharArray,
    kArray,
  };

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
    int writableProducerCount = 0;
    int connectedProducerCount = 0;
    ValueKind valueKind = ValueKind::kNone;
    bool hasValue = false;
    double value = 0.0;
    QString stringValue;
    dbr_enum_t enumValue = 0;
    QStringList enumStrings;
    QByteArray charArrayValue;
    QVector<double> arrayValues;
    epicsTimeStamp timestamp{};
    bool hasTimestamp = false;
    bool hasControlInfo = false;
    double low = 0.0;
    double high = 0.0;
    short precision = -1;
    QString units;
    bool producedByExpressionChannel = false;
    QString expressionCalc;
    QStringList expressionChannels;
    QVector<Subscriber> subscribers;
    int dispatchDepth = 0;
    bool cleanupPending = false;
  };

  SoftPvRegistry();
  ~SoftPvRegistry() override;

  void unsubscribe(quint64 subscriptionId) override;

  Entry *findEntry(const QString &name) const;
  Entry *findOrCreateEntry(const QString &name);
  Subscriber *findSubscriber(Entry *entry, quint64 id);
  void stampValue(Entry *entry);
  void clearValue(Entry *entry);
  SharedChannelData buildData(const Entry &entry) const;
  void dispatchConnectionCallbacks(Entry *entry);
  void dispatchValueCallbacks(Entry *entry);
  void dispatchAccessRightsCallbacks(Entry *entry);
  void cleanupEntryIfUnused(Entry *entry);
  void assertMainThread() const;

  QHash<QString, Entry *> entries_;
  QHash<quint64, Entry *> subscriptionToEntry_;
  quint64 nextSubscriptionId_ = 1;
};
