#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

enum class DisplayConversionDisposition {
  kMapped,
  kApproximated,
  kOmitted,
  kUnsupported,
};

struct DisplayConversionOptions
{
  QString inputPath;
  QString outputPath;
  QString reportPath;
  QString sourceCopyPath;
};

struct DisplayConversionResult
{
  bool success = false;
  bool hasWarnings = false;
  QString error;
  QString outputPath;
  QString reportPath;
  QString sourceCopyPath;
  QStringList generatedDisplayPaths;
  QJsonObject report;

  int exitCode() const
  {
    if (!success) {
      return 1;
    }
    return hasWarnings ? 2 : 0;
  }
};

/*
 * Reusable one-way display conversion entry point.  Version 1 deliberately
 * accepts only caQtDM/Qt Designer .ui files.  The source is never modified.
 */
class DisplayConverter
{
public:
  static DisplayConversionResult convert(
      const DisplayConversionOptions &options);

  static QString dispositionName(DisplayConversionDisposition disposition);
};
