#include <QtTest/QtTest>

#include "soft_pv_registry.h"

class TestSoftPvRegistry : public QObject
{
  Q_OBJECT

private slots:
  void expressionChannelInfoRoundTrips();
  void arrayInfoSnapshotsIncludePayloads();
  void deliveryModesCoalesceAndRetainNewestValue();
  void callbackMayRemoveLastProducerAndSubscription();
  void accessCallbackMayRemoveLastProducerAndSubscription();
  void connectionCallbackMayRemoveLastProducerAndSubscription();
};

void TestSoftPvRegistry::expressionChannelInfoRoundTrips()
{
  auto &registry = SoftPvRegistry::instance();
  const QString name = QStringLiteral("__test:expr_meta");
  QStringList channels;
  channels << QStringLiteral("src:A")
           << QString()
           << QStringLiteral("src:C")
           << QStringLiteral("src:D");

  registry.registerName(name);
  registry.setConnected(name, true);
  registry.setExpressionChannelInfo(name, QStringLiteral("A+C-D"),
      channels);
  registry.publishValue(name, 12.5);

  SoftPvInfoSnapshot snapshot;
  QVERIFY(registry.infoSnapshot(name, snapshot));
  QVERIFY(snapshot.producedByExpressionChannel);
  QCOMPARE(snapshot.expressionCalc, QStringLiteral("A+C-D"));
  QCOMPARE(snapshot.expressionChannels, channels);
  QVERIFY(snapshot.hasValue);
  QCOMPARE(snapshot.value, 12.5);

  registry.clearExpressionChannelInfo(name);
  QVERIFY(registry.infoSnapshot(name, snapshot));
  QVERIFY(!snapshot.producedByExpressionChannel);
  QVERIFY(snapshot.expressionCalc.isEmpty());
  QVERIFY(snapshot.expressionChannels.isEmpty());

  registry.setConnected(name, false);
  registry.unregisterName(name);
  QVERIFY(!registry.infoSnapshot(name, snapshot));
}

void TestSoftPvRegistry::arrayInfoSnapshotsIncludePayloads()
{
  auto &registry = SoftPvRegistry::instance();
  const QString arrayName = QStringLiteral("__test:array_meta");
  const QString charName = QStringLiteral("__test:char_array_meta");

  registry.registerName(arrayName);
  registry.setConnected(arrayName, true);
  QVector<double> values;
  values << 1.0 << 2.5 << 4.0;
  registry.publishArrayValue(arrayName, values);

  SoftPvInfoSnapshot snapshot;
  QVERIFY(registry.infoSnapshot(arrayName, snapshot));
  QVERIFY(snapshot.hasValue);
  QVERIFY(snapshot.isArray);
  QCOMPARE(snapshot.fieldType, static_cast<short>(DBF_DOUBLE));
  QCOMPARE(snapshot.elementCount, static_cast<unsigned long>(values.size()));
  QCOMPARE(snapshot.arrayValues, values);

  registry.registerName(charName);
  registry.setConnected(charName, true);
  const QByteArray bytes("abc\0", 4);
  registry.publishCharArrayValue(charName, bytes);

  QVERIFY(registry.infoSnapshot(charName, snapshot));
  QVERIFY(snapshot.hasValue);
  QVERIFY(snapshot.isCharArray);
  QCOMPARE(snapshot.fieldType, static_cast<short>(DBF_CHAR));
  QCOMPARE(snapshot.elementCount, static_cast<unsigned long>(bytes.size()));
  QCOMPARE(snapshot.charArrayValue, bytes);

  registry.setConnected(charName, false);
  registry.unregisterName(charName);
  registry.setConnected(arrayName, false);
  registry.unregisterName(arrayName);
}

void TestSoftPvRegistry::deliveryModesCoalesceAndRetainNewestValue()
{
  auto &registry = SoftPvRegistry::instance();
  const QString name = QStringLiteral("__test:delivery_modes");
  registry.registerName(name);
  registry.setConnected(name, true);

  int passiveValues = 0;
  int realtimeValues = 0;
  int connectionChanges = 0;
  double passiveLatest = 0.0;
  double realtimeLatest = 0.0;

  SubscriptionHandle passive = registry.subscribe(name,
      [&](const SharedChannelData &data) {
        ++passiveValues;
        passiveLatest = data.numericValue;
      },
      [&](bool, const SharedChannelData &) { ++connectionChanges; });
  SubscriptionHandle realtime = registry.subscribe(name,
      [&](const SharedChannelData &data) {
        ++realtimeValues;
        realtimeLatest = data.numericValue;
      },
      nullptr, nullptr, ChannelDeliveryMode::kRealtime);

  QCOMPARE(connectionChanges, 1);
  registry.publishValue(name, 1.0);
  QCOMPARE(passiveValues, 1);
  QCOMPARE(realtimeValues, 1);

  QTest::qWait(20);
  registry.publishValue(name, 2.0);
  registry.publishValue(name, 3.0);
  QCOMPARE(passiveValues, 1);
  QCOMPARE(realtimeValues, 1);

  QTest::qWait(100);
  QCOMPARE(realtimeValues, 2);
  QCOMPARE(realtimeLatest, 3.0);
  QCOMPARE(passiveValues, 1);

  QTest::qWait(110);
  QCOMPARE(passiveValues, 2);
  QCOMPARE(passiveLatest, 3.0);

  registry.setConnected(name, false);
  QCOMPARE(connectionChanges, 2);
  passive.reset();
  realtime.reset();
  registry.unregisterName(name);
}

void TestSoftPvRegistry::callbackMayRemoveLastProducerAndSubscription()
{
  auto &registry = SoftPvRegistry::instance();
  const QString name = QStringLiteral("__test:self_removing_callback");
  registry.registerName(name);
  registry.setConnected(name, true);

  int callbacks = 0;
  SubscriptionHandle subscription;
  subscription = registry.subscribe(name,
      [&](const SharedChannelData &) {
        ++callbacks;
        registry.unregisterName(name);
        subscription.reset();
      });

  registry.publishValue(name, 1.0);
  QCOMPARE(callbacks, 1);

  SoftPvInfoSnapshot snapshot;
  QVERIFY(!registry.infoSnapshot(name, snapshot));
}

void TestSoftPvRegistry::accessCallbackMayRemoveLastProducerAndSubscription()
{
  auto &registry = SoftPvRegistry::instance();
  const QString name = QStringLiteral("__test:self_removing_access");
  registry.registerName(name, true);

  int callbacks = 0;
  SubscriptionHandle subscription;
  subscription = registry.subscribe(name, nullptr, nullptr,
      [&](bool readAccess, bool writeAccess) {
        QVERIFY(readAccess);
        ++callbacks;
        if (!writeAccess) {
          subscription.reset();
        }
      });

  QCOMPARE(callbacks, 1);
  registry.unregisterName(name, true);
  QCOMPARE(callbacks, 2);
  QVERIFY(!subscription.isValid());

  SoftPvInfoSnapshot snapshot;
  QVERIFY(!registry.infoSnapshot(name, snapshot));
}

void TestSoftPvRegistry::
    connectionCallbackMayRemoveLastProducerAndSubscription()
{
  auto &registry = SoftPvRegistry::instance();
  const QString name = QStringLiteral("__test:self_removing_connection");
  registry.registerName(name);
  registry.setConnected(name, true);

  int callbacks = 0;
  SubscriptionHandle subscription;
  subscription = registry.subscribe(name, nullptr,
      [&](bool connected, const SharedChannelData &) {
        ++callbacks;
        if (!connected) {
          registry.unregisterName(name);
          subscription.reset();
        }
      });

  QCOMPARE(callbacks, 1);
  registry.setConnected(name, false);
  QCOMPARE(callbacks, 2);
  QVERIFY(!subscription.isValid());

  SoftPvInfoSnapshot snapshot;
  QVERIFY(!registry.infoSnapshot(name, snapshot));
}

QTEST_GUILESS_MAIN(TestSoftPvRegistry)

#include "test_soft_pv_registry.moc"
