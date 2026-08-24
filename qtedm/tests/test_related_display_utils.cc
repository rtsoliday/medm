#include <QtTest>

#include "related_display_utils.h"

class TestRelatedDisplayUtils : public QObject
{
  Q_OBJECT

private slots:
  void expandsMacrosAndSelectsLaunchMode();
  void rejectsMissingTargetsAndMalformedMacros();
};

void TestRelatedDisplayUtils::expandsMacrosAndSelectsLaunchMode()
{
  RelatedDisplayEntry entry;
  entry.name = QStringLiteral(" child.adl ");
  entry.args = QStringLiteral("P=$(PREFIX),DESC=$(LABEL),NESTED=$(NESTED)");
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
  QCOMPARE(result.macros.value(QStringLiteral("P")),
      QStringLiteral("ioc:"));
  QCOMPARE(result.macros.value(QStringLiteral("DESC")),
      QStringLiteral("Beam line"));
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

QTEST_APPLESS_MAIN(TestRelatedDisplayUtils)

#include "test_related_display_utils.moc"
