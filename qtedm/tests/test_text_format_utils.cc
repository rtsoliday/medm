#include <QtTest/QtTest>

#include <limits>

#include "text_format_utils.h"

class TestTextFormatUtils : public QObject
{
  Q_OBJECT

private slots:
  void clampsPrecision();
  void formatsEngineeringNotation();
  void formatsEngineeringNotationBoundaries();
  void formatsSexagesimalAndIntegers();
  void formatsMinimumSignedInteger();
  void readsNegativeHexadecimalBackForSignedChannels();
  void saturatesDoubleToLong();
};

void TestTextFormatUtils::clampsPrecision()
{
  QCOMPARE(TextFormatUtils::clampPrecision(-3), 0);
  QCOMPARE(TextFormatUtils::clampPrecision(4), 4);
  QCOMPARE(TextFormatUtils::clampPrecision(100),
      TextFormatUtils::kMaxPrecision);
}

void TestTextFormatUtils::formatsEngineeringNotation()
{
  char buffer[TextFormatUtils::kMaxTextField];

  TextFormatUtils::localCvtDoubleToExpNotationString(12345.0, buffer, 2);
  QCOMPARE(QString::fromLatin1(buffer), QStringLiteral("12.35e+03"));

  TextFormatUtils::localCvtDoubleToExpNotationString(0.0123, buffer, 1);
  QCOMPARE(QString::fromLatin1(buffer), QStringLiteral("12.3e-03"));
}

void TestTextFormatUtils::formatsEngineeringNotationBoundaries()
{
  char buffer[TextFormatUtils::kMaxTextField];

  TextFormatUtils::localCvtDoubleToExpNotationString(
      std::numeric_limits<double>::infinity(), buffer, 2);
  QCOMPARE(QString::fromLatin1(buffer), QStringLiteral("+Inf"));

  TextFormatUtils::localCvtDoubleToExpNotationString(
      -std::numeric_limits<double>::infinity(), buffer, 2);
  QCOMPARE(QString::fromLatin1(buffer), QStringLiteral("-Inf"));

  TextFormatUtils::localCvtDoubleToExpNotationString(
      std::numeric_limits<double>::quiet_NaN(), buffer, 2);
  QCOMPARE(QString::fromLatin1(buffer), QStringLiteral("NaN"));

  TextFormatUtils::localCvtDoubleToExpNotationString(
      std::numeric_limits<double>::max(), buffer, 2);
  QVERIFY(QString::fromLatin1(buffer).endsWith(QStringLiteral("e+306")));
}

void TestTextFormatUtils::formatsSexagesimalAndIntegers()
{
  QCOMPARE(TextFormatUtils::makeSexagesimal(12.5, 2),
      QStringLiteral("12:30"));
  QCOMPARE(TextFormatUtils::formatHex(255), QStringLiteral("0xff"));
  QCOMPARE(TextFormatUtils::formatHex(-2), QStringLiteral("0xfffffffe"));
  QCOMPARE(TextFormatUtils::formatHex(-255), QStringLiteral("0xffffff01"));
  QCOMPARE(TextFormatUtils::formatOctal(493), QStringLiteral("755"));
}

void TestTextFormatUtils::formatsMinimumSignedInteger()
{
  const long minimum = std::numeric_limits<long>::min();
  unsigned long magnitude = static_cast<unsigned long>(minimum);
  magnitude = 0UL - magnitude;

  QCOMPARE(TextFormatUtils::formatHex(minimum),
      QStringLiteral("0x")
          + QString::number(static_cast<qulonglong>(
              static_cast<unsigned long>(minimum) & 0xffffffffUL), 16));
  QCOMPARE(TextFormatUtils::formatOctal(minimum),
      QStringLiteral("-")
          + QString::number(static_cast<qulonglong>(magnitude), 8));
}

void TestTextFormatUtils::readsNegativeHexadecimalBackForSignedChannels()
{
  /* formatHex(-4) shows 0xfffffffc; a signed channel reads it as -4. */
  bool ok = false;
  const qlonglong shown = TextFormatUtils::formatHex(-4).toLongLong(&ok, 16);
  QVERIFY(ok);
  QCOMPARE(TextFormatUtils::integerInputValue(shown, true), -4.0);
  QCOMPARE(TextFormatUtils::integerInputValue(shown, false), 4294967292.0);
  QCOMPARE(TextFormatUtils::integerInputValue(0x7fffffffLL, true),
      2147483647.0);
  QCOMPARE(TextFormatUtils::integerInputValue(0x80000000LL, true),
      -2147483648.0);
  QCOMPARE(TextFormatUtils::integerInputValue(0x100000000LL, true),
      4294967296.0);
  QCOMPARE(TextFormatUtils::integerInputValue(-2, true), -2.0);
}

void TestTextFormatUtils::saturatesDoubleToLong()
{
  QCOMPARE(TextFormatUtils::saturatedLongFromDouble(1.9), 1L);
  QCOMPARE(TextFormatUtils::saturatedLongFromDouble(-1.9), -1L);
  QCOMPARE(TextFormatUtils::saturatedLongFromDouble(1.5, true), 2L);
  QCOMPARE(TextFormatUtils::saturatedLongFromDouble(
      std::numeric_limits<double>::max()), std::numeric_limits<long>::max());
  QCOMPARE(TextFormatUtils::saturatedLongFromDouble(
      -std::numeric_limits<double>::max()), std::numeric_limits<long>::min());
}

QTEST_APPLESS_MAIN(TestTextFormatUtils)

#include "test_text_format_utils.moc"
