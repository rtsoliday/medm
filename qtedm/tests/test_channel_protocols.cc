#include <QtTest/QtTest>

#include "pv_protocol.h"

Q_DECLARE_METATYPE(PvProtocol)

class TestChannelProtocols : public QObject
{
  Q_OBJECT

private slots:
  void parsesProviderPrefixes_data();
  void parsesProviderPrefixes();
  void stripsOnlyRecognizedProviderPrefix();
};

void TestChannelProtocols::parsesProviderPrefixes_data()
{
  QTest::addColumn<QString>("input");
  QTest::addColumn<PvProtocol>("protocol");
  QTest::addColumn<QString>("pvName");

  QTest::newRow("ca-default")
      << QStringLiteral("device:value") << PvProtocol::kCa
      << QStringLiteral("device:value");
  QTest::newRow("pva")
      << QStringLiteral("pva://device:value") << PvProtocol::kPva
      << QStringLiteral("device:value");
  QTest::newRow("pva-case-insensitive")
      << QStringLiteral("PVA://device:value") << PvProtocol::kPva
      << QStringLiteral("device:value");
  QTest::newRow("trimmed-ca")
      << QStringLiteral("  device:value  ") << PvProtocol::kCa
      << QStringLiteral("device:value");
  QTest::newRow("trimmed-pva")
      << QStringLiteral("  pva://device:value  ") << PvProtocol::kPva
      << QStringLiteral("device:value");
  QTest::newRow("empty")
      << QStringLiteral("   ") << PvProtocol::kCa << QString();
  QTest::newRow("unknown-scheme-remains-ca")
      << QStringLiteral("plugin://device:value") << PvProtocol::kCa
      << QStringLiteral("plugin://device:value");
}

void TestChannelProtocols::parsesProviderPrefixes()
{
  QFETCH(QString, input);
  QFETCH(PvProtocol, protocol);
  QFETCH(QString, pvName);

  const ParsedPvName parsed = parsePvName(input);
  QCOMPARE(parsed.rawName, input);
  QCOMPARE(parsed.protocol, protocol);
  QCOMPARE(parsed.pvName, pvName);
}

void TestChannelProtocols::stripsOnlyRecognizedProviderPrefix()
{
  QCOMPARE(stripPvProtocol(QStringLiteral("pva://A:B")),
      QStringLiteral("A:B"));
  QCOMPARE(stripPvProtocol(QStringLiteral("ca://A:B")),
      QStringLiteral("ca://A:B"));
  QCOMPARE(stripPvProtocol(QStringLiteral("plugin://A:B")),
      QStringLiteral("plugin://A:B"));
}

QTEST_APPLESS_MAIN(TestChannelProtocols)

#include "test_channel_protocols.moc"
