#include <QtTest/QtTest>

#include "runtime_utils.h"
#include "medm_calc.h"

#include <array>
#include <cmath>

class TestRuntimeUtils : public QObject
{
  Q_OBJECT

private slots:
  void appendsNullTerminatorOnce();
  void normalizesCalcExpressions();
  void calculationCompilerBoundsBuffersAndStacks();
  void calculationArithmetic_data();
  void calculationArithmetic();
  void detectsNumericFieldTypes();
  void sanitizesSddsColumnNames();
  void invokesImmediatelyOnObjectThread();
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

void TestRuntimeUtils::sanitizesSddsColumnNames()
{
  QCOMPARE(RuntimeUtils::sanitizeSddsColumnName(
      QStringLiteral("__qtedm_demo:wave")), QStringLiteral("qtedm_demo_wave"));
  QCOMPARE(RuntimeUtils::sanitizeSddsColumnName(
      QStringLiteral("_PV")), QStringLiteral("PV"));
  QCOMPARE(RuntimeUtils::sanitizeSddsColumnName(
      QStringLiteral("prefix:signal.VAL")), QStringLiteral("prefix_signal_VAL"));
  QCOMPARE(RuntimeUtils::sanitizeSddsColumnName(
      QStringLiteral("___"), QStringLiteral("_Pen0")), QStringLiteral("Pen0"));
  QCOMPARE(RuntimeUtils::sanitizeSddsColumnName(
      QStringLiteral("___")), QStringLiteral("Column"));
}

void TestRuntimeUtils::invokesImmediatelyOnObjectThread()
{
  QObject target;
  bool invoked = false;
  RuntimeUtils::invokeOnObject(QPointer<QObject>(&target),
      [&](QObject *object) {
        QCOMPARE(object, &target);
        invoked = true;
      });
  QVERIFY(invoked);
}

void TestRuntimeUtils::calculationCompilerBoundsBuffersAndStacks()
{
  std::array<char, QTEDM_CALC_POSTFIX_CAPACITY> post{};
  std::array<double, 12> args{};
  short error = 0;
  double result = 0;
  /* This 61-byte expression used to overflow the 300-byte runtime buffer. */
  QByteArray infix = QByteArray("1+").repeated(30) + "1";
  QCOMPARE(qtedmPostfix(infix.data(), post.data(), post.size(), &error), 0L);
  QCOMPARE(calcPerform(args.data(), &result, post.data()), 0L);
  QCOMPARE(result, 31.0);

  infix = QByteArray("1+").repeated(39) + "1";
  QCOMPARE(infix.size(), QTEDM_CALC_MAX_INFIX);
  QCOMPARE(qtedmPostfix(infix.data(), post.data(), post.size(), &error), 0L);
  QCOMPARE(calcPerform(args.data(), &result, post.data()), 0L);
  QCOMPARE(result, 40.0);

  infix = QByteArray(80, '(') + "1" + QByteArray(80, ')');
  QVERIFY(qtedmPostfix(infix.data(), post.data(), post.size(), &error) != 0);
  QVERIFY(calcPerform(args.data(), &result, post.data()) != 0);

  infix = QByteArray(10000, '1');
  QVERIFY(qtedmPostfix(infix.data(), post.data(), post.size(), &error) != 0);
  std::array<char, 16> small;
  small.fill('!');
  infix = "1+2";
  QVERIFY(qtedmPostfix(infix.data(), small.data(), 1, &error) != 0);
  for (size_t index = 1; index < small.size(); ++index) {
    QCOMPARE(small[index], '!');
  }
  small[0] = '!';
  QVERIFY(qtedmPostfix(infix.data(), small.data(), 0, &error) != 0);
  QCOMPARE(small[0], '!');
}

void TestRuntimeUtils::calculationArithmetic_data()
{
  QTest::addColumn<QByteArray>("expression");
  QTest::addColumn<double>("expected");
  QTest::addColumn<long>("status");
  QTest::newRow("wide-remainder") << QByteArray("40000%50000")
      << 40000.0 << 0L;
  QTest::newRow("negative-remainder") << QByteArray("(-40000)%50000")
      << -40000.0 << 0L;
  QTest::newRow("negative-divisor") << QByteArray("40000%(-50000)")
      << 40000.0 << 0L;
  QTest::newRow("fractional-modulo") << QByteArray("5.9%2.1")
      << 1.0 << 0L;
  QTest::newRow("minimum-integer-modulo")
      << QByteArray("(-2147483648)%(-1)") << 0.0 << 0L;
  QTest::newRow("modulo-zero") << QByteArray("1%0") << 0.0 << -1L;
  QTest::newRow("modulo-out-of-range") << QByteArray("1E100%3")
      << 0.0 << -1L;
  QTest::newRow("negative-odd-power") << QByteArray("(-2)^(-3)")
      << -0.125 << 0L;
  QTest::newRow("negative-even-power") << QByteArray("(-2)^(-2)")
      << 0.25 << 0L;
  QTest::newRow("positive-odd-power") << QByteArray("(-2)^3")
      << -8.0 << 0L;
  QTest::newRow("wide-exponent") << QByteArray("(-1)^32769")
      << -1.0 << 0L;
  QTest::newRow("fractional-negative-base") << QByteArray("(-2)^0.5")
      << 0.0 << -1L;
}

void TestRuntimeUtils::calculationArithmetic()
{
  QFETCH(QByteArray, expression);
  QFETCH(double, expected);
  QFETCH(long, status);
  std::array<char, QTEDM_CALC_POSTFIX_CAPACITY> post{};
  std::array<double, 12> args{};
  short error = 0;
  double result = 0.0;
  QCOMPARE(qtedmPostfix(expression.data(), post.data(), post.size(), &error),
      0L);
  QCOMPARE(calcPerform(args.data(), &result, post.data()), status);
  if (status == 0) {
    QVERIFY(std::abs(result - expected) <= 1e-12);
  }
}

QTEST_APPLESS_MAIN(TestRuntimeUtils)

#include "test_runtime_utils.moc"
