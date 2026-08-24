#include <QtTest/QtTest>

#include <cmath>

#include "soft_pv_registry.h"
#include "text_entry_element.h"
#include "text_entry_runtime.h"
#include "text_format_utils.h"

class TestTextEntryRuntime : public QObject
{
  Q_OBJECT

private slots:
  void parsesNumericFormats_data();
  void parsesNumericFormats();
  void rejectsInvalidNumericAndSexagesimalInput();
  void wrapsSexagesimalValuesWithCircularControlLimits();
  void parsesEnumLabelsAndNumericRepresentations();
  void sizesAndTruncatesCharArrayInput();
  void rejectsWritesUntilConnectedAndWritable();
};

void TestTextEntryRuntime::parsesNumericFormats_data()
{
  QTest::addColumn<int>("format");
  QTest::addColumn<QString>("text");
  QTest::addColumn<double>("expected");

  QTest::newRow("decimal")
      << static_cast<int>(TextMonitorFormat::kDecimal)
      << QStringLiteral("-12.5") << -12.5;
  QTest::newRow("exponential")
      << static_cast<int>(TextMonitorFormat::kExponential)
      << QStringLiteral("2.5e2") << 250.0;
  QTest::newRow("decimal-hex-prefix")
      << static_cast<int>(TextMonitorFormat::kDecimal)
      << QStringLiteral("0x2a") << 42.0;
  QTest::newRow("hexadecimal")
      << static_cast<int>(TextMonitorFormat::kHexadecimal)
      << QStringLiteral("ff") << 255.0;
  QTest::newRow("octal")
      << static_cast<int>(TextMonitorFormat::kOctal)
      << QStringLiteral("17") << 15.0;
  QTest::newRow("sexagesimal")
      << static_cast<int>(TextMonitorFormat::kSexagesimal)
      << QStringLiteral("12:30:00") << 12.5;
  QTest::newRow("sexagesimal-negative")
      << static_cast<int>(TextMonitorFormat::kSexagesimal)
      << QStringLiteral("-1:30") << -1.5;
  QTest::newRow("hours-minutes-seconds")
      << static_cast<int>(TextMonitorFormat::kSexagesimalHms)
      << QStringLiteral("6:00:00") << (TextFormatUtils::kPi / 2.0);
  QTest::newRow("degrees-minutes-seconds")
      << static_cast<int>(TextMonitorFormat::kSexagesimalDms)
      << QStringLiteral("180:00:00") << TextFormatUtils::kPi;
}

void TestTextEntryRuntime::parsesNumericFormats()
{
  QFETCH(int, format);
  QFETCH(QString, text);
  QFETCH(double, expected);

  TextEntryElement element;
  element.setFormat(static_cast<TextMonitorFormat>(format));
  TextEntryRuntime runtime(&element);
  double actual = 0.0;
  QVERIFY(runtime.parseNumericInput(text, actual));
  QVERIFY(std::abs(actual - expected) < 1e-12);
}

void TestTextEntryRuntime::rejectsInvalidNumericAndSexagesimalInput()
{
  TextEntryElement element;
  TextEntryRuntime runtime(&element);
  double value = 0.0;

  element.setFormat(TextMonitorFormat::kDecimal);
  for (const QString &input : {QString(), QStringLiteral("nan"),
       QStringLiteral("inf"), QStringLiteral("1e9999"),
       QStringLiteral("12 trailing"), QStringLiteral("0xbroken")}) {
    QVERIFY2(!runtime.parseNumericInput(input, value), qPrintable(input));
  }

  element.setFormat(TextMonitorFormat::kHexadecimal);
  QVERIFY(!runtime.parseNumericInput(QStringLiteral("xyz"), value));
  element.setFormat(TextMonitorFormat::kOctal);
  QVERIFY(!runtime.parseNumericInput(QStringLiteral("18"), value));

  element.setFormat(TextMonitorFormat::kSexagesimal);
  for (const QString &input : {QStringLiteral("1:60:00"),
       QStringLiteral("1:00:60"), QStringLiteral("1:-2:00"),
       QStringLiteral("--1:00"), QStringLiteral("1:00 extra")}) {
    QVERIFY2(!runtime.parseNumericInput(input, value), qPrintable(input));
  }
}

void TestTextEntryRuntime::wrapsSexagesimalValuesWithCircularControlLimits()
{
  TextEntryElement element;
  element.setFormat(TextMonitorFormat::kSexagesimal);
  TextEntryRuntime runtime(&element);
  runtime.hasControlLimits_ = true;
  runtime.controlLow_ = 0.0;
  runtime.controlHigh_ = 24.0;

  double value = 0.0;
  QVERIFY(runtime.parseNumericInput(QStringLiteral("25:30"), value));
  QCOMPARE(value, 1.5);
  QVERIFY(runtime.parseNumericInput(QStringLiteral("-1:00"), value));
  QCOMPARE(value, 23.0);
}

void TestTextEntryRuntime::parsesEnumLabelsAndNumericRepresentations()
{
  TextEntryElement element;
  TextEntryRuntime runtime(&element);
  runtime.enumStrings_ = QStringList{
      QStringLiteral("Idle"), QStringLiteral("Running"),
      QStringLiteral("Complete")};

  short value = -1;
  QVERIFY(runtime.parseEnumInput(QStringLiteral("Running"), value));
  QCOMPARE(value, static_cast<short>(1));
  QVERIFY(runtime.parseEnumInput(QStringLiteral("0x2"), value));
  QCOMPARE(value, static_cast<short>(2));
  QVERIFY(!runtime.parseEnumInput(QStringLiteral("running"), value));
  QVERIFY(!runtime.parseEnumInput(QStringLiteral("3"), value));
  QVERIFY(!runtime.parseEnumInput(QStringLiteral("-1"), value));

  runtime.enumStrings_.clear();
  element.setFormat(TextMonitorFormat::kHexadecimal);
  QVERIFY(runtime.parseEnumInput(QStringLiteral("a"), value));
  QCOMPARE(value, static_cast<short>(10));
  element.setFormat(TextMonitorFormat::kOctal);
  QVERIFY(runtime.parseEnumInput(QStringLiteral("17"), value));
  QCOMPARE(value, static_cast<short>(15));
  QVERIFY(!runtime.parseEnumInput(QStringLiteral("18"), value));
}

void TestTextEntryRuntime::sizesAndTruncatesCharArrayInput()
{
  TextEntryElement element;
  TextEntryRuntime runtime(&element);
  QByteArray bytes;

  runtime.elementCount_ = 5;
  QVERIFY(runtime.parseCharArrayInput(QStringLiteral("abc"), bytes));
  QCOMPARE(bytes, QByteArray("abc\0\0", 5));
  QVERIFY(runtime.parseCharArrayInput(QStringLiteral("abcdef"), bytes));
  QCOMPARE(bytes, QByteArray("abcde", 5));
  QVERIFY(runtime.parseCharArrayInput(QString(), bytes));
  QCOMPARE(bytes, QByteArray(5, '\0'));

  runtime.elementCount_ = 0;
  QVERIFY(!runtime.parseCharArrayInput(QStringLiteral("x"), bytes));
}

void TestTextEntryRuntime::rejectsWritesUntilConnectedAndWritable()
{
  const QString channel = QStringLiteral("__test:text_entry_write_gate");
  auto &soft = SoftPvRegistry::instance();
  soft.registerName(channel, true);
  soft.setConnected(channel, true);
  soft.publishValue(channel, 1.0);

  TextEntryElement element;
  element.setChannel(channel);
  element.setFormat(TextMonitorFormat::kDecimal);
  TextEntryRuntime runtime(&element);
  runtime.channelName_ = channel;
  runtime.valueKind_ = TextEntryRuntime::ValueKind::kNumeric;
  runtime.started_ = true;

  SoftPvInfoSnapshot snapshot;
  runtime.connected_ = false;
  runtime.lastWriteAccess_ = true;
  runtime.handleActivation(QStringLiteral("2"));
  QVERIFY(soft.infoSnapshot(channel, snapshot));
  QCOMPARE(snapshot.value, 1.0);

  runtime.connected_ = true;
  runtime.lastWriteAccess_ = false;
  runtime.handleActivation(QStringLiteral("2"));
  QVERIFY(soft.infoSnapshot(channel, snapshot));
  QCOMPARE(snapshot.value, 1.0);

  runtime.lastWriteAccess_ = true;
  runtime.handleActivation(QStringLiteral("invalid"));
  QVERIFY(soft.infoSnapshot(channel, snapshot));
  QCOMPARE(snapshot.value, 1.0);
  runtime.handleActivation(QStringLiteral("2.5"));
  QVERIFY(soft.infoSnapshot(channel, snapshot));
  QCOMPARE(snapshot.value, 2.5);

  runtime.started_ = false;
  soft.setConnected(channel, false);
  soft.unregisterName(channel, true);
}

QTEST_MAIN(TestTextEntryRuntime)
#include "test_text_entry_runtime.moc"
