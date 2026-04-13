#include <QtTest/QtTest>

#include "runtime_utils.h"

class TestRuntimeUtils : public QObject
{
  Q_OBJECT

private slots:
  void appendsNullTerminatorOnce();
  void normalizesCalcExpressions();
  void detectsNumericFieldTypes();
};

void TestRuntimeUtils::appendsNullTerminatorOnce()
{
  QByteArray bytes("calc");

  RuntimeUtils::appendNullTerminator(bytes);
  QCOMPARE(bytes, QByteArray("calc\0", 5));

  RuntimeUtils::appendNullTerminator(bytes);
  QCOMPARE(bytes, QByteArray("calc\0", 5));
}

void TestRuntimeUtils::normalizesCalcExpressions()
{
  QCOMPARE(RuntimeUtils::normalizeCalcExpression(
      QStringLiteral("A!=B && C==D")), QStringLiteral("A#B && C=D"));
}

void TestRuntimeUtils::detectsNumericFieldTypes()
{
  QVERIFY(RuntimeUtils::isNumericFieldType(DBR_DOUBLE));
  QVERIFY(RuntimeUtils::isNumericFieldType(DBR_LONG));
  QVERIFY(!RuntimeUtils::isNumericFieldType(DBR_STRING));
}

QTEST_APPLESS_MAIN(TestRuntimeUtils)

#include "test_runtime_utils.moc"
