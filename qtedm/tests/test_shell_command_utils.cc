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
  void quotesDynamicValuesAndRejectsUnsafeMacros();
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
#ifdef Q_OS_WIN
  QCOMPARE(output, QStringLiteral(
      "tool /displays/main.adl main.adl 12345 pv:a pv:b &Q trailing&"));
#else
  QCOMPARE(output, QStringLiteral(
      "tool '/displays/main.adl' 'main.adl' 12345 'pv:a' 'pv:b' &Q trailing&"));
#endif
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
#ifdef Q_OS_WIN
  context.displayTitle = QStringLiteral("Operator's screen");
#else
  context.displayTitle = QStringLiteral("Operator's screen &A");
#endif
  context.windowId = 42;
  context.pvNames = QStringList{QStringLiteral("TEST:VALUE")};
  QString output;
  QString error;
  QVERIFY(expandShellCommandTokens(
      QStringLiteral("viewer \"&A\" \"&T\" &X &P"), context,
      &output, &error));
#ifdef Q_OS_WIN
  QCOMPARE(output, QStringLiteral(
      "viewer \"/displays/Operator's screens/main.adl\" "
      "\"Operator's screen\" 42 TEST:VALUE"));
#else
  QCOMPARE(output, QStringLiteral(
      "viewer \"/displays/Operator's screens/main.adl\" "
      "\"Operator's screen &A\" 42 'TEST:VALUE'"));
#endif
  QVERIFY(error.isEmpty());
}

void TestShellCommandUtils::quotesDynamicValuesAndRejectsUnsafeMacros()
{
  const auto usedIn = [](const QString &value) {
    return QStringList{QStringLiteral("tool ") + value};
  };
  QVERIFY(shellCommandMacrosAreSafe({
      {QStringLiteral("P"), QStringLiteral("../safe path:1")}},
      usedIn(QStringLiteral("../safe path:1"))));
  QVERIFY(!shellCommandMacrosAreSafe({
      {QStringLiteral("P"), QStringLiteral("safe; unexpected")}},
      usedIn(QStringLiteral("safe; unexpected"))));
  QVERIFY(!shellCommandMacrosAreSafe({
      {QStringLiteral("P"), QStringLiteral("safe\nunexpected")}},
      usedIn(QStringLiteral("safe\nunexpected"))));
  QVERIFY(!shellCommandMacrosAreSafe({
      {QStringLiteral("T"), QStringLiteral("x -e touch /tmp/x")}},
      usedIn(QStringLiteral("x -e touch /tmp/x"))));
  QVERIFY(!shellCommandMacrosAreSafe({
      {QStringLiteral("T"), QStringLiteral("--output=/tmp/x")}},
      usedIn(QStringLiteral("--output=/tmp/x"))));
  QVERIFY(shellCommandMacrosAreSafe({
      {QStringLiteral("OFFSET"), QStringLiteral("-5 -0.25 1-2 a-b")}},
      usedIn(QStringLiteral("-5 -0.25 1-2 a-b"))));

  // Macros the command does not contain never block it.
  const QHash<QString, QString> mixed{
      {QStringLiteral("DESC"), QStringLiteral("Pump (upstream) #1")},
      {QStringLiteral("BPM"), QStringLiteral("SR:C01-BI{BPM:1}")},
      {QStringLiteral("P"), QStringLiteral("S35:")}};
  QString unsafeMacro;
  QVERIFY(shellCommandMacrosAreSafe(mixed,
      {QStringLiteral("probe S35:current &")}, &unsafeMacro));
  QVERIFY(unsafeMacro.isEmpty());
  QVERIFY(!shellCommandMacrosAreSafe(mixed,
      {QStringLiteral("probe S35:current &"),
       QStringLiteral("probe SR:C01-BI{BPM:1} &")}, &unsafeMacro));
  QCOMPARE(unsafeMacro, QStringLiteral("BPM"));

  ShellCommandExpansionContext controls;
  controls.displayPath = QStringLiteral("x\necho injected #");
  controls.displayTitle = QStringLiteral("title\r");
  controls.pvNames = QStringList{QStringLiteral("pv\tname")};
  QString controlOutput;
  QString controlError;
  QVERIFY(!expandShellCommandTokens(QStringLiteral("echo hi # &A"),
      controls, &controlOutput, &controlError));
  QVERIFY(controlError.contains(QStringLiteral("control characters")));
  controlError.clear();
  QVERIFY(!expandShellCommandTokens(QStringLiteral("echo &T"),
      controls, &controlOutput, &controlError));
  QVERIFY(controlError.contains(QStringLiteral("control characters")));
  controlError.clear();
  QVERIFY(!expandShellCommandTokens(QStringLiteral("echo &P"),
      controls, &controlOutput, &controlError));
  QVERIFY(controlError.contains(QStringLiteral("control characters")));
#ifndef Q_OS_WIN
  ShellCommandExpansionContext context;
  context.displayPath = QStringLiteral("a';touch x");
  context.displayTitle = QStringLiteral("a\";touch x");
  context.pvNames = QStringList{QStringLiteral("pv;touch x")};
  QString output;
  QString error;
  QVERIFY(expandShellCommandTokens(QStringLiteral("open &A \"&T\" &P"),
      context, &output));
  QCOMPARE(output, QStringLiteral(
      "open 'a'\\'';touch' 'x' \"a\\\";touch x\" 'pv;touch' 'x'"));
  // Unquoted values split into words as in MEDM, but each word is literal.
  context.displayTitle = QStringLiteral("Beam  Current (mA)");
  QVERIFY(expandShellCommandTokens(QStringLiteral("xmessage &T '&T'"),
      context, &output));
  QCOMPARE(output, QStringLiteral(
      "xmessage 'Beam' 'Current' '(mA)' 'Beam  Current (mA)'"));
  // Nested-syntax characters that the outer shell treats literally do not
  // block later tokens.
  context.displayPath = QStringLiteral("/displays/main.adl");
  QVERIFY(expandShellCommandTokens(
      QStringLiteral("awk '{print $(NF) \"`\"}' \"<<\" \\` &A"), context,
      &output, &error));
  QCOMPARE(output, QStringLiteral(
      "awk '{print $(NF) \"`\"}' \"<<\" \\` '/displays/main.adl'"));
  context.displayTitle = QStringLiteral("x$(id)`whoami`");
  QVERIFY(expandShellCommandTokens(QStringLiteral("open \"&T\""),
      context, &output));
  QCOMPARE(output, QStringLiteral("open \"x\\$(id)\\`whoami\\`\""));
  QVERIFY(expandShellCommandTokens(
      QStringLiteral("open &A \"$(date)\""), context, &output, &error));
  error.clear();
  QVERIFY(!expandShellCommandTokens(
      QStringLiteral("echo \"$(basename &A)\""), context,
      &output, &error));
  QVERIFY(error.contains(QStringLiteral("nested shell syntax")));
  error.clear();
  QVERIFY(!expandShellCommandTokens(
      QStringLiteral("cat <<EOF\n&A\nEOF"), context,
      &output, &error));
  QVERIFY(error.contains(QStringLiteral("nested shell syntax")));
  error.clear();
  QVERIFY(!expandShellCommandTokens(
      QStringLiteral("echo `basename &A`"), context,
      &output, &error));
  QVERIFY(error.contains(QStringLiteral("nested shell syntax")));
#endif
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
