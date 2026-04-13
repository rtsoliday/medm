#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "command_line_options.h"

namespace {

class ScopedEnvVar
{
public:
  explicit ScopedEnvVar(const char *name)
    : name_(name)
    , hadValue_(qEnvironmentVariableIsSet(name))
    , value_(qgetenv(name))
  {
  }

  ~ScopedEnvVar()
  {
    if (hadValue_) {
      qputenv(name_, value_);
    } else {
      qunsetenv(name_);
    }
  }

private:
  const char *name_;
  bool hadValue_ = false;
  QByteArray value_;
};

}  // namespace

class TestCommandLine : public QObject
{
  Q_OBJECT

private slots:
  void parsesCommonOptions();
  void usesEnvironmentOverrides();
  void resolvesDisplayFilesFromSearchPath();
  void parsesMacrosAndGeometry();
  void parsesTestAutomationOptions();
};

void TestCommandLine::parsesCommonOptions()
{
  const CommandLineOptions options = parseCommandLine(QStringList{
      QStringLiteral("/tmp/qtedm"),
      QStringLiteral("-x"),
      QStringLiteral("-displayFont"),
      QStringLiteral("scalable"),
      QStringLiteral("-macro"),
      QStringLiteral("A=1,B=two"),
      QStringLiteral("demo.adl"),
  });

  QVERIFY(options.startInExecuteMode);
  QCOMPARE(programName(QStringList{QStringLiteral("/tmp/qtedm")}),
      QStringLiteral("qtedm"));
  QCOMPARE(options.displayFont, QStringLiteral("scalable"));
  QCOMPARE(options.displayFiles, QStringList{QStringLiteral("demo.adl")});
  QCOMPARE(options.macroString, QStringLiteral("A=1,B=two"));
}

void TestCommandLine::usesEnvironmentOverrides()
{
  ScopedEnvVar noLogEnv("QTEDM_NOLOG");
  ScopedEnvVar savePathEnv("QTEDM_TEST_SAVE_PATH");

  qputenv("QTEDM_NOLOG", "1");
  qputenv("QTEDM_TEST_SAVE_PATH", "/tmp/from-env.adl");

  const CommandLineOptions options = parseCommandLine(
      QStringList{QStringLiteral("qtedm")});

  QVERIFY(!options.enableAuditLog);
  QCOMPARE(options.testSaveOutputPath, QStringLiteral("/tmp/from-env.adl"));
}

void TestCommandLine::resolvesDisplayFilesFromSearchPath()
{
  ScopedEnvVar displayPathEnv("EPICS_DISPLAY_PATH");
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QFile file(dir.filePath(QStringLiteral("screen.adl")));
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  QVERIFY(file.write("file {\n}\n") > 0);
  file.close();

  qputenv("EPICS_DISPLAY_PATH", dir.path().toLocal8Bit());

  const QStringList resolved = resolveDisplayArguments(
      QStringList{QStringLiteral("screen.adl")});

  QCOMPARE(resolved.size(), 1);
  QCOMPARE(resolved.first(), QFileInfo(file.fileName()).absoluteFilePath());
}

void TestCommandLine::parsesMacrosAndGeometry()
{
  const MacroMap macros = parseMacroDefinitionString(
      QStringLiteral("A=1, broken, B = two , =oops, C=3"));

  QCOMPARE(macros.value(QStringLiteral("A")), QStringLiteral("1"));
  QCOMPARE(macros.value(QStringLiteral("B")), QStringLiteral("two"));
  QCOMPARE(macros.value(QStringLiteral("C")), QStringLiteral("3"));
  QCOMPARE(macros.size(), 3);

  const std::optional<GeometrySpec> spec =
      geometrySpecFromString(QStringLiteral("640x480-15+25"));
  QVERIFY(spec.has_value());
  QVERIFY(spec->hasWidth);
  QVERIFY(spec->hasHeight);
  QVERIFY(spec->hasX);
  QVERIFY(spec->hasY);
  QCOMPARE(spec->width, 640);
  QCOMPARE(spec->height, 480);
  QCOMPARE(spec->x, 15);
  QCOMPARE(spec->y, 25);
  QVERIFY(spec->xFromRight);
  QVERIFY(!spec->yFromBottom);
  QVERIFY(!geometrySpecFromString(QStringLiteral("bogus")).has_value());
}

void TestCommandLine::parsesTestAutomationOptions()
{
  const CommandLineOptions options = parseCommandLine(QStringList{
      QStringLiteral("qtedm"),
      QStringLiteral("-testSave"),
      QStringLiteral("-testSaveOutput"),
      QStringLiteral("/tmp/output.adl"),
      QStringLiteral("-testDumpState"),
      QStringLiteral("/tmp/state.json"),
      QStringLiteral("-testCaptureScreenshot"),
      QStringLiteral("/tmp/screenshot.png"),
      QStringLiteral("-testReadyFile"),
      QStringLiteral("/tmp/ready.flag"),
      QStringLiteral("-testExitAfterMs"),
      QStringLiteral("250"),
      QStringLiteral("screen.adl"),
  });

  QVERIFY(options.testSave);
  QCOMPARE(options.testSaveOutputPath, QStringLiteral("/tmp/output.adl"));
  QCOMPARE(options.testDumpStatePath, QStringLiteral("/tmp/state.json"));
  QCOMPARE(options.testCaptureScreenshotPath,
      QStringLiteral("/tmp/screenshot.png"));
  QCOMPARE(options.testReadyFilePath, QStringLiteral("/tmp/ready.flag"));
  QCOMPARE(options.testExitAfterMs, 250);
  QCOMPARE(options.displayFiles, QStringList{QStringLiteral("screen.adl")});
}

QTEST_APPLESS_MAIN(TestCommandLine)

#include "test_command_line.moc"
