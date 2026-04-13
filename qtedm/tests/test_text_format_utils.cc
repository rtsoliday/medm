#include <QtTest/QtTest>

#include "text_format_utils.h"

class TestTextFormatUtils : public QObject
{
  Q_OBJECT

private slots:
  void clampsPrecision();
  void formatsEngineeringNotation();
  void formatsSexagesimalAndIntegers();
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

void TestTextFormatUtils::formatsSexagesimalAndIntegers()
{
  QCOMPARE(TextFormatUtils::makeSexagesimal(12.5, 2),
      QStringLiteral("12:30"));
  QCOMPARE(TextFormatUtils::formatHex(-255), QStringLiteral("-0xff"));
  QCOMPARE(TextFormatUtils::formatOctal(493), QStringLiteral("755"));
}

QTEST_APPLESS_MAIN(TestTextFormatUtils)

#include "test_text_format_utils.moc"
