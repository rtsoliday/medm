#include "plot_export_utils.h"

#include <algorithm>
#include <cmath>

#include <QBuffer>
#include <QFile>
#include <QSet>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include "cartesian_plot_element.h"
#include "runtime_utils.h"

namespace {

struct TraceColumnInfo
{
  int traceIndex = 0;
  QString xColumn;
  QString yColumn;
  bool includeX = true;
  bool includeY = true;
};

QVector<TraceColumnInfo> collectTraceColumns(
    const CartesianPlotElement &plot, int *maximumPointCount)
{
  QVector<TraceColumnInfo> columns;
  QSet<QString> usedNames;
  int maximum = 0;
  for (int index = 0; index < plot.traceCount(); ++index) {
    if (!plot.traceHasData(index)) {
      continue;
    }
    TraceColumnInfo info;
    info.traceIndex = index;
    QString xChannel = plot.traceXChannel(index);
    if (xChannel.isEmpty()) {
      xChannel = QStringLiteral("X%1").arg(index);
    }
    info.xColumn = RuntimeUtils::sanitizeSddsColumnName(
        xChannel, QStringLiteral("X%1").arg(index));
    QString yChannel = plot.traceYChannel(index);
    if (yChannel.isEmpty()) {
      yChannel = plot.traceXChannel(index);
    }
    if (yChannel.isEmpty()) {
      yChannel = QStringLiteral("Trace%1").arg(index);
    }
    info.yColumn = RuntimeUtils::sanitizeSddsColumnName(
        yChannel, QStringLiteral("Trace%1").arg(index));
    if (info.xColumn == info.yColumn) {
      info.xColumn += QStringLiteral("_X");
      info.yColumn += QStringLiteral("_Y");
    }
    if (usedNames.contains(info.xColumn)) {
      info.includeX = false;
    } else {
      usedNames.insert(info.xColumn);
    }
    if (usedNames.contains(info.yColumn)) {
      info.includeY = false;
    } else {
      usedNames.insert(info.yColumn);
    }
    if (info.includeX || info.includeY) {
      columns.append(info);
      maximum = std::max(maximum, plot.dataPointCount(index));
    }
  }
  if (maximumPointCount) {
    *maximumPointCount = maximum;
  }
  return columns;
}

void writeNumber(QTextStream &stream, double value, bool csv)
{
  if (std::isnan(value)) {
    if (!csv) {
      stream << QStringLiteral("nan");
    }
  } else {
    stream << QString::number(value, 'g', 15);
  }
}

}  // namespace

QByteArray serializeCartesianPlotData(const CartesianPlotElement &plot,
    PlotDataExportFormat format, QString *errorMessage)
{
  int maximumPointCount = 0;
  const QVector<TraceColumnInfo> columns =
      collectTraceColumns(plot, &maximumPointCount);
  if (columns.isEmpty() || maximumPointCount <= 0) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "The Cartesian plot has no connected traces with data.");
    }
    return QByteArray();
  }

  QByteArray data;
  QBuffer buffer(&data);
  /* Newlines are emitted explicitly so exports remain byte-for-byte
   * identical across platforms. */
  if (!buffer.open(QIODevice::WriteOnly)) {
    if (errorMessage) {
      *errorMessage = buffer.errorString();
    }
    return QByteArray();
  }
  QTextStream stream(&buffer);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Utf8);
#else
  stream.setCodec("UTF-8");
#endif
  const bool csv = format == PlotDataExportFormat::kCsv;
  if (csv) {
    stream << QStringLiteral("Index");
    for (const TraceColumnInfo &info : columns) {
      if (info.includeX) {
        stream << QStringLiteral(",\"") << info.xColumn
               << QStringLiteral("\"");
      }
      if (info.includeY) {
        stream << QStringLiteral(",\"") << info.yColumn
               << QStringLiteral("\"");
      }
    }
    stream << QLatin1Char('\n');
  } else {
    stream << QStringLiteral("SDDS1\n");
    stream << QStringLiteral(
        "&description text=\"Cartesian Plot Data Export from QtEDM\", "
        "contents=\"cartesian plot data\" &end\n");
    stream << QStringLiteral(
        "&column name=Index, type=long, description=\"Data point index\" "
        "&end\n");
    for (const TraceColumnInfo &info : columns) {
      if (info.includeX) {
        stream << QStringLiteral(
            "&column name=%1, type=double, description=\"X values for "
            "trace %2\" &end\n").arg(info.xColumn).arg(info.traceIndex);
      }
      if (info.includeY) {
        stream << QStringLiteral(
            "&column name=%1, type=double, description=\"Y values for "
            "trace %2\" &end\n").arg(info.yColumn).arg(info.traceIndex);
      }
    }
    stream << QStringLiteral("&data mode=ascii &end\n");
    stream << maximumPointCount << QLatin1Char('\n');
  }

  for (int pointIndex = 0; pointIndex < maximumPointCount; ++pointIndex) {
    stream << pointIndex;
    for (const TraceColumnInfo &info : columns) {
      const bool hasPoint =
          pointIndex < plot.dataPointCount(info.traceIndex);
      const QPointF point = hasPoint
          ? plot.dataPoint(info.traceIndex, pointIndex) : QPointF();
      if (info.includeX) {
        stream << (csv ? QLatin1Char(',') : QLatin1Char(' '));
        if (hasPoint) {
          writeNumber(stream, point.x(), csv);
        } else if (!csv) {
          stream << QStringLiteral("nan");
        }
      }
      if (info.includeY) {
        stream << (csv ? QLatin1Char(',') : QLatin1Char(' '));
        if (hasPoint) {
          writeNumber(stream, point.y(), csv);
        } else if (!csv) {
          stream << QStringLiteral("nan");
        }
      }
    }
    stream << QLatin1Char('\n');
  }
  stream.flush();
  if (stream.status() != QTextStream::Ok) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed while serializing plot data.");
    }
    return QByteArray();
  }
  return data;
}

bool writeCartesianPlotData(const CartesianPlotElement &plot,
    const QString &path, PlotDataExportFormat format, QString *errorMessage)
{
  QString serializationError;
  const QByteArray data =
      serializeCartesianPlotData(plot, format, &serializationError);
  if (data.isEmpty()) {
    if (errorMessage) {
      *errorMessage = serializationError;
    }
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to open %1: %2")
          .arg(path, file.errorString());
    }
    return false;
  }
  if (file.write(data) != data.size()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to write %1: %2")
          .arg(path, file.errorString());
    }
    return false;
  }
  return true;
}
