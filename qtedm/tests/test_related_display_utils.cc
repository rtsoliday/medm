#include <QtTest>

#include "related_display_utils.h"

class TestRelatedDisplayUtils : public QObject
{
  Q_OBJECT

private slots:
  void expandsMacrosAndSelectsLaunchMode();
  void rejectsMissingTargetsAndMalformedMacros();
  void refusesArgumentsThatOutgrowADisplayFile();
};

void TestRelatedDisplayUtils::expandsMacrosAndSelectsLaunchMode()
{
  RelatedDisplayEntry entry;
  entry.name = QStringLiteral(" child.adl ");
  entry.args = QStringLiteral(
      "P=$(PREFIX),DESC=$(LABEL),NESTED=$(NESTED),P=other:");
  entry.mode = RelatedDisplayMode::kAdd;
  const QHash<QString, QString> inherited{
      {QStringLiteral("PREFIX"), QStringLiteral("ioc:")},
      {QStringLiteral("LABEL"), QStringLiteral("Beam line")},
      {QStringLiteral("NESTED"), QStringLiteral("$(PREFIX)value")}};

  RelatedDisplayLaunchSpec result;
  QString error;
  QVERIFY2(prepareRelatedDisplayLaunch(entry, Qt::NoModifier,
      inherited, &result, &error), qPrintable(error));
  QCOMPARE(result.fileName, QStringLiteral("child.adl"));
  QVERIFY(!result.replace);
  /* As in MEDM, the first definition of a name wins. */
  QCOMPARE(result.macros.value(QStringLiteral("P")),
      QStringLiteral("ioc:"));
  /* MEDM substitutes the parent's macros into the arguments and then drops
   * every whitespace character from each value. */
  QCOMPARE(result.macros.value(QStringLiteral("DESC")),
      QStringLiteral("Beamline"));
  QCOMPARE(result.macros.value(QStringLiteral("NESTED")),
      QStringLiteral("ioc:value"));

  QVERIFY(prepareRelatedDisplayLaunch(entry, Qt::ControlModifier,
      inherited, &result, &error));
  QVERIFY(result.replace);
  entry.mode = RelatedDisplayMode::kReplace;
  QVERIFY(prepareRelatedDisplayLaunch(entry, Qt::NoModifier,
      inherited, &result, &error));
  QVERIFY(result.replace);
}

void TestRelatedDisplayUtils::rejectsMissingTargetsAndMalformedMacros()
{
  RelatedDisplayEntry entry;
  RelatedDisplayLaunchSpec result;
  QString error;
  QVERIFY(!prepareRelatedDisplayLaunch(entry, Qt::NoModifier, {},
      &result, &error));
  QVERIFY(error.contains(QStringLiteral("no target")));

  entry.name = QStringLiteral("child.adl");
  entry.args = QStringLiteral("GOOD=1,broken");
  error.clear();
  QVERIFY(!prepareRelatedDisplayLaunch(entry, Qt::NoModifier, {},
      &result, &error));
  QVERIFY(error.contains(QStringLiteral("broken")));
}

/* A value that refers to its own macro 64 times grows 64-fold at each pass:
 * 256 characters, then 16 Ki, 1 Mi, and 64 Mi, beyond any display file. */
void TestRelatedDisplayUtils::refusesArgumentsThatOutgrowADisplayFile()
{
  RelatedDisplayEntry entry;
  entry.name = QStringLiteral("child.adl");
  entry.args = QStringLiteral("A=$(A)");
  const QHash<QString, QString> inherited{
      {QStringLiteral("A"), QStringLiteral("$(A)").repeated(64)}};
  RelatedDisplayLaunchSpec result;
  QString error;
  QVERIFY(!prepareRelatedDisplayLaunch(entry, Qt::NoModifier, inherited,
      &result, &error));
  QVERIFY2(error.contains(QStringLiteral("expand to more than")),
      qPrintable(error));
}

QTEST_APPLESS_MAIN(TestRelatedDisplayUtils)

#include "test_related_display_utils.moc"
