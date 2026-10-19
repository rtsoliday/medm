#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "shell_command_utils.h"

class TestShellCommandUtils : public QObject
{
  Q_OBJECT

private slots:
  void expandsMedmTokensAndPreservesUnknownTokens();
  void preservesTokenValuesWithoutRecursiveExpansion();
  void rejectsMissingPvSelectionAndEmptyCommands();
  void invokesCheckedInHelperAndReportsFailures();
};

void TestShellCommandUtils::expandsMedmTokensAndPreservesUnknownTokens()
{
  ShellCommandExpansionContext context;
  context.displayPath = QStringLiteral("/displays/main.adl");
  context.displayTitle = QStringLiteral("main.adl");
  context.windowId = 12345;
  context.pvNames = QStringList{
      QStringLiteral("pv:a"), QStringLiteral("pv:b")};
  QString output;
  QString error;
  QVERIFY(expandShellCommandTokens(
      QStringLiteral("tool &A &T &X &P &Q trailing&"), context,
      &output, &error));
  QCOMPARE(output, QStringLiteral(
      "tool /displays/main.adl main.adl 12345 pv:a pv:b &Q trailing&"));
  QVERIFY(error.isEmpty());
}

void TestShellCommandUtils::rejectsMissingPvSelectionAndEmptyCommands()
{
  ShellCommandExpansionContext context;
  QString output;
  QString error;
  QVERIFY(!expandShellCommandTokens(
      QStringLiteral("tool &P"), context, &output, &error));
  QVERIFY(error.contains(QStringLiteral("selected PV")));
  QVERIFY(!expandShellCommandTokens(
      QStringLiteral("tool"), context, nullptr, &error));
  QCOMPARE(executeShellCommand(QString(), &error), -1);
  QVERIFY(error.contains(QStringLiteral("empty")));
}

void TestShellCommandUtils::preservesTokenValuesWithoutRecursiveExpansion()
{
  ShellCommandExpansionContext context;
  context.displayPath = QStringLiteral("/displays/Operator's screens/main.adl");
  context.displayTitle = QStringLiteral("Operator's screen &A");
  context.windowId = 42;
  context.pvNames = QStringList{QStringLiteral("TEST:VALUE")};
  QString output;
  QString error;
  QVERIFY(expandShellCommandTokens(
      QStringLiteral("viewer \"&A\" \"&T\" &X &P"), context,
      &output, &error));
  QCOMPARE(output, QStringLiteral(
      "viewer \"/displays/Operator's screens/main.adl\" "
      "\"Operator's screen &A\" 42 TEST:VALUE"));
  QVERIFY(error.isEmpty());
}

void TestShellCommandUtils::invokesCheckedInHelperAndReportsFailures()
{
  if (qEnvironmentVariableIntValue("QTEDM_TEST_RESTRICTED") == 1) {
    QSKIP("Restricted tests check command strings without executing them.");
  }
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QString helper = QDir(QCoreApplication::applicationDirPath())
      .filePath(QStringLiteral("qtedm_shell_command_helper"));
#ifdef Q_OS_WIN
  helper += QStringLiteral(".exe");
#endif
  QVERIFY2(QFileInfo::exists(helper), qPrintable(helper));
  const QString outputPath =
      directory.filePath(QStringLiteral("helper output.txt"));
  auto quote = [](const QString &value) {
    QString escaped = QDir::toNativeSeparators(value);
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"") + escaped + QStringLiteral("\"");
  };
  QString error;
  const QString command = quote(helper) + QLatin1Char(' ')
      + quote(outputPath) + QStringLiteral(" \"verified payload\"");
  QCOMPARE(executeShellCommand(command, &error), 0);
  QVERIFY(error.isEmpty());
  QFile output(outputPath);
  QVERIFY(output.open(QIODevice::ReadOnly));
  QCOMPARE(output.readAll(), QByteArray("verified payload"));

  const int failure = executeShellCommand(quote(helper), &error);
  QVERIFY(failure != 0);
  QVERIFY(error.contains(QStringLiteral("status")));
}

QTEST_MAIN(TestShellCommandUtils)

#include "test_shell_command_utils.moc"
