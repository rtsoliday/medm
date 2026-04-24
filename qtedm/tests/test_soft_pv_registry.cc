#include <QtTest/QtTest>

#include "soft_pv_registry.h"

class TestSoftPvRegistry : public QObject
{
  Q_OBJECT

private slots:
  void expressionChannelInfoRoundTrips();
  void arrayInfoSnapshotsIncludePayloads();
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

QTEST_APPLESS_MAIN(TestSoftPvRegistry)

#include "test_soft_pv_registry.moc"
