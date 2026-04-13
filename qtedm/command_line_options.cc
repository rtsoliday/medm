#include "command_line_options.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <cstdio>

const char kQtEdmVersionString[] =
    "QtEDM Version 1.0.0  (EPICS 7.0.9.1-DEV)";

QString programName(const QStringList &args)
{
  if (args.isEmpty()) {
    return QStringLiteral("qtedm");
  }
  return QFileInfo(args.first()).fileName();
}

CommandLineOptions parseCommandLine(const QStringList &args)
{
  CommandLineOptions options;

  const QByteArray testSavePathEnv = qgetenv("QTEDM_TEST_SAVE_PATH");
  if (!testSavePathEnv.isEmpty()) {
    options.testSaveOutputPath = QString::fromLocal8Bit(testSavePathEnv);
  }

  for (int i = 1; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    if (arg == QLatin1String("-x")) {
      options.startInExecuteMode = true;
    } else if (arg == QLatin1String("-local")) {
      options.remoteMode = RemoteMode::kLocal;
    } else if (arg == QLatin1String("-attach")) {
      options.remoteMode = RemoteMode::kAttach;
    } else if (arg == QLatin1String("-cleanup")) {
      options.remoteMode = RemoteMode::kCleanup;
    } else if (arg == QLatin1String("-help") ||
               arg == QLatin1String("-h") ||
               arg == QLatin1String("-?")) {
      options.showHelp = true;
    } else if (arg == QLatin1String("-version")) {
      options.showVersion = true;
    } else if (arg == QLatin1String("-noMsg")) {
      options.raiseMessageWindow = false;
    } else if (arg == QLatin1String("-nolog")) {
      options.enableAuditLog = false;
    } else if (arg == QLatin1String("-bigMousePointer")) {
      options.useBigMousePointer = true;
    } else if (arg == QLatin1String("-cmap")) {
      options.usePrivateColormap = true;
    } else if (arg == QLatin1String("-testSave")) {
      options.testSave = true;
    } else if (arg == QLatin1String("-testSaveOutput") ||
               arg == QLatin1String("--test-save-output")) {
      if ((i + 1) < args.size()) {
        options.testSaveOutputPath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("-testExitAfterMs") ||
               arg == QLatin1String("--test-exit-after-ms")) {
      if ((i + 1) < args.size()) {
        bool ok = false;
        const int value = args.at(++i).toInt(&ok);
        if (!ok || value < 0) {
          options.invalidOption = arg;
        } else {
          options.testExitAfterMs = value;
        }
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("-testDumpState") ||
               arg == QLatin1String("--test-dump-state")) {
      if ((i + 1) < args.size()) {
        options.testDumpStatePath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("-testCaptureScreenshot") ||
               arg == QLatin1String("--test-capture-screenshot")) {
      if ((i + 1) < args.size()) {
        options.testCaptureScreenshotPath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("-testReadyFile") ||
               arg == QLatin1String("--test-ready-file")) {
      if ((i + 1) < args.size()) {
        options.testReadyFilePath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("-crashTest") ||
               arg == QLatin1String("--crash-test")) {
      options.crashTest = true;
    } else if (arg == QLatin1String("-macro")) {
      if ((i + 1) < args.size()) {
        QString tmp = args.at(++i);
        if (!tmp.isEmpty() && tmp.front() == QLatin1Char('"')) {
          tmp.remove(0, 1);
        }
        if (!tmp.isEmpty() && tmp.back() == QLatin1Char('"')) {
          tmp.chop(1);
        }
        options.macroString = tmp.trimmed();
      }
    } else if (arg == QLatin1String("-displayGeometry") ||
               arg == QLatin1String("-dg")) {
      if ((i + 1) < args.size()) {
        options.displayGeometry = args.at(++i);
      }
    } else if (arg == QLatin1String("-displayFont")) {
      if ((i + 1) < args.size()) {
        options.displayFont = args.at(++i).trimmed();
      } else {
        options.invalidOption = arg;
      }
    } else if (arg.startsWith(QLatin1Char('-'))) {
      options.invalidOption = arg;
    } else {
      options.displayFiles.push_back(arg);
    }
  }

  const QByteArray noLogEnv = qgetenv("QTEDM_NOLOG");
  if (!noLogEnv.isEmpty() && noLogEnv != "0") {
    options.enableAuditLog = false;
  }

  if (!options.invalidOption.isEmpty()) {
    options.showHelp = true;
  }
  if (options.showHelp) {
    options.showVersion = true;
  }
  return options;
}

QStringList displaySearchPaths()
{
  QStringList searchPaths;
  const QByteArray env = qgetenv("EPICS_DISPLAY_PATH");
  if (!env.isEmpty()) {
    const QStringList parts = QString::fromLocal8Bit(env).split(
        QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
      const QString trimmed = part.trimmed();
      if (!trimmed.isEmpty()) {
        searchPaths.push_back(trimmed);
      }
    }
  }
  return searchPaths;
}

QString resolveDisplayFile(const QString &fileArgument)
{
  QFileInfo directInfo(fileArgument);
  if (directInfo.exists() && directInfo.isFile()) {
    return directInfo.absoluteFilePath();
  }
  const QStringList searchPaths = displaySearchPaths();
  for (const QString &basePath : searchPaths) {
    QFileInfo candidate(QDir(basePath), fileArgument);
    if (candidate.exists() && candidate.isFile()) {
      return candidate.absoluteFilePath();
    }
  }
  return QString();
}

QStringList resolveDisplayArguments(const QStringList &files)
{
  QStringList resolved;
  for (const QString &file : files) {
    if (!file.endsWith(QStringLiteral(".adl"), Qt::CaseInsensitive)) {
      fprintf(stderr, "\nFile has wrong suffix: %s\n",
          file.toLocal8Bit().constData());
      fflush(stderr);
      continue;
    }
    const QString resolvedPath = resolveDisplayFile(file);
    if (resolvedPath.isEmpty()) {
      fprintf(stderr, "\nCannot access file: %s\n",
          file.toLocal8Bit().constData());
      fflush(stderr);
      continue;
    }
    resolved.push_back(resolvedPath);
  }
  return resolved;
}

MacroMap parseMacroDefinitionString(const QString &macroString)
{
  MacroMap macros;
  if (macroString.isEmpty()) {
    return macros;
  }
  const QStringList entries = macroString.split(QLatin1Char(','),
      Qt::KeepEmptyParts);
  for (const QString &entry : entries) {
    const QString trimmedEntry = entry.trimmed();
    if (trimmedEntry.isEmpty()) {
      continue;
    }
    const int equalsIndex = trimmedEntry.indexOf(QLatin1Char('='));
    if (equalsIndex <= 0) {
      fprintf(stderr, "\nInvalid macro definition: %s\n",
          trimmedEntry.toLocal8Bit().constData());
      continue;
    }
    const QString name = trimmedEntry.left(equalsIndex).trimmed();
    const QString value = trimmedEntry.mid(equalsIndex + 1).trimmed();
    if (name.isEmpty()) {
      fprintf(stderr, "\nInvalid macro definition: %s\n",
          trimmedEntry.toLocal8Bit().constData());
      continue;
    }
    macros.insert(name, value);
  }
  return macros;
}

std::optional<GeometrySpec> geometrySpecFromString(const QString &geometry)
{
  const QString trimmed = geometry.trimmed();
  if (trimmed.isEmpty()) {
    return std::nullopt;
  }
  QString effective = trimmed;
  if (effective.startsWith(QLatin1Char('='))) {
    effective.remove(0, 1);
  }
  static const QRegularExpression kGeometryPattern(
      QStringLiteral(R"(^\s*(?:(\d+))?(?:x(\d+))?([+-]\d+)?([+-]\d+)?\s*$)"));
  const QRegularExpressionMatch match = kGeometryPattern.match(effective);
  if (!match.hasMatch()) {
    return std::nullopt;
  }

  GeometrySpec spec;
  if (!match.captured(1).isEmpty()) {
    spec.hasWidth = true;
    spec.width = match.captured(1).toInt();
  }
  if (!match.captured(2).isEmpty()) {
    spec.hasHeight = true;
    spec.height = match.captured(2).toInt();
  }
  if (!match.captured(3).isEmpty()) {
    spec.hasX = true;
    const QString xStr = match.captured(3);
    spec.xFromRight = xStr.startsWith(QLatin1Char('-'));
    spec.x = xStr.mid(1).toInt();
  }
  if (!match.captured(4).isEmpty()) {
    spec.hasY = true;
    const QString yStr = match.captured(4);
    spec.yFromBottom = yStr.startsWith(QLatin1Char('-'));
    spec.y = yStr.mid(1).toInt();
  }
  return spec;
}
