#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

extern const char kQtEdmVersionString[];

struct GeometrySpec
{
  bool hasWidth = false;
  bool hasHeight = false;
  bool hasX = false;
  bool hasY = false;
  int width = 0;
  int height = 0;
  int x = 0;
  int y = 0;
  bool xFromRight = false;
  bool yFromBottom = false;
};

enum class RemoteMode {
  kLocal,
  kAttach,
  kCleanup,
};

struct CommandLineOptions {
  bool startInExecuteMode = false;
  bool showHelp = false;
  bool showVersion = false;
  bool raiseMessageWindow = true;
  bool usePrivateColormap = false;
  bool useBigMousePointer = false;
  bool testSave = false;
  int testExitAfterMs = -1;
  bool crashTest = false;
  bool enableAuditLog = true;
  QString invalidOption;
  QStringList displayFiles;
  QString displayGeometry;
  QString macroString;
  RemoteMode remoteMode = RemoteMode::kLocal;
  QStringList resolvedDisplayFiles;
  QString displayFont = QStringLiteral("alias");
  QString testSaveOutputPath = QStringLiteral("/tmp/qtedmTest.adl");
  QString testDumpStatePath;
  QString testCaptureScreenshotPath;
  QString testReadyFilePath;
};

using MacroMap = QHash<QString, QString>;

QString programName(const QStringList &args);
CommandLineOptions parseCommandLine(const QStringList &args);
QStringList displaySearchPaths();
QString resolveDisplayFile(const QString &fileArgument);
QStringList resolveDisplayArguments(const QStringList &files);
MacroMap parseMacroDefinitionString(const QString &macroString);
std::optional<GeometrySpec> geometrySpecFromString(const QString &geometry);
