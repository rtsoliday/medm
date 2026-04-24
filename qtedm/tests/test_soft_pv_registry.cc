#include <QtTest/QtTest>

#include "soft_pv_registry.h"

class TestSoftPvRegistry : public QObject
{
  Q_OBJECT

private slots:
  void expressionChannelInfoRoundTrips();
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

QTEST_APPLESS_MAIN(TestSoftPvRegistry)

#include "test_soft_pv_registry.moc"
