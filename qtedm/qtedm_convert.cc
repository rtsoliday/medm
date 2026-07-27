#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>

#include <cstdio>

#include "display_converter.h"

namespace {

void printUsage(const QString &program)
{
  fprintf(stdout,
      "Usage: %s [--output display.adl] [--report report.json]\n"
      "       [--source-copy preserved.source.ui] input.ui\n"
      "\n"
      "Converts a caQtDM/Qt Designer .ui display to QtEDM ADL.\n"
      "Exit status: 0 complete, 2 converted with warnings, 1 fatal.\n",
      program.toLocal8Bit().constData());
}

} // namespace

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  const QStringList args = app.arguments();
  DisplayConversionOptions options;

  for (int index = 1; index < args.size(); ++index) {
    const QString arg = args.at(index);
    if (arg == QLatin1String("-h") || arg == QLatin1String("--help")) {
      printUsage(QFileInfo(args.value(0)).fileName());
      return 0;
    }
    auto takeValue = [&](QString *target) -> bool {
      if ((index + 1) >= args.size()) {
        return false;
      }
      *target = args.at(++index);
      return true;
    };
    if (arg == QLatin1String("-o") || arg == QLatin1String("--output")) {
      if (!takeValue(&options.outputPath)) {
        fprintf(stderr, "qtedm-convert: --output requires a path\n");
        return 1;
      }
    } else if (arg == QLatin1String("--report")) {
      if (!takeValue(&options.reportPath)) {
        fprintf(stderr, "qtedm-convert: --report requires a path\n");
        return 1;
      }
    } else if (arg == QLatin1String("--source-copy")) {
      if (!takeValue(&options.sourceCopyPath)) {
        fprintf(stderr, "qtedm-convert: --source-copy requires a path\n");
        return 1;
      }
    } else if (arg.startsWith(QLatin1Char('-'))) {
      fprintf(stderr, "qtedm-convert: unknown option: %s\n",
          arg.toLocal8Bit().constData());
      return 1;
    } else if (options.inputPath.isEmpty()) {
      options.inputPath = arg;
    } else {
      fprintf(stderr, "qtedm-convert: only one input file is supported\n");
      return 1;
    }
  }

  if (options.inputPath.isEmpty()) {
    printUsage(QFileInfo(args.value(0)).fileName());
    return 1;
  }

  const DisplayConversionResult result = DisplayConverter::convert(options);
  if (!result.success) {
    fprintf(stderr, "qtedm-convert: %s\n",
        result.error.toLocal8Bit().constData());
    return 1;
  }

  fprintf(stdout, "output: %s\nreport: %s\nsource-copy: %s\n",
      result.outputPath.toLocal8Bit().constData(),
      result.reportPath.toLocal8Bit().constData(),
      result.sourceCopyPath.toLocal8Bit().constData());
  if (result.hasWarnings) {
    fprintf(stdout, "conversion completed with warnings\n");
  } else {
    fprintf(stdout, "conversion completed\n");
  }
  return result.exitCode();
}
