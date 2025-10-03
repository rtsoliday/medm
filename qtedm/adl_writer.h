#pragma once

#include <array>

#include <QColor>
#include <QPoint>
#include <QRect>
#include <Qt>
#include <QString>
#include <QVector>

#include "display_properties.h"

class QTextStream;

namespace AdlWriter {

constexpr int kMedmVersionNumber = 30122;

QString indentString(int level);
void writeIndentedLine(QTextStream &stream, int level, const QString &text);
QString escapeAdlString(const QString &value);
QString colorModeString(TextColorMode mode);
QString visibilityModeString(TextVisibilityMode mode);
QString lineStyleString(RectangleLineStyle style);
QString fillString(RectangleFill fill);
QString alignmentString(Qt::Alignment alignment);
QString imageTypeString(ImageType type);
QString timeUnitsString(TimeUnits units);
QString channelFieldName(int index);
QString textMonitorFormatString(TextMonitorFormat format);
int medmLineWidthValue(int width);
int medmColorIndex(const QColor &color);
void writeObjectSection(QTextStream &stream, int level, const QRect &rect);
void writeBasicAttributeSection(QTextStream &stream, int level, int colorIndex,
    RectangleLineStyle lineStyle, RectangleFill fill, int lineWidth);
void writeDynamicAttributeSection(QTextStream &stream, int level,
    TextColorMode colorMode, TextVisibilityMode visibilityMode,
    const QString &calc, const std::array<QString, 4> &channels);
void writeMonitorSection(QTextStream &stream, int level, const QString &channel,
    int colorIndex, int backgroundIndex);
void writeControlSection(QTextStream &stream, int level, const QString &channel,
    int colorIndex, int backgroundIndex);
QString cartesianPlotStyleString(CartesianPlotStyle style);
QString cartesianEraseOldestString(bool eraseOldest);
QString cartesianEraseModeString(CartesianPlotEraseMode mode);
void writePlotcom(QTextStream &stream, int level, const QString &title,
    const QString &xLabel, const std::array<QString, 4> &yLabels,
    int colorIndex, int backgroundIndex);
void writeLimitsSection(QTextStream &stream, int level, const PvLimits &limits);
void writeStripChartPenSection(QTextStream &stream, int level, int index,
    const QString &channel, int colorIndex, const PvLimits &limits);
void writePointsSection(
    QTextStream &stream, int level, const QVector<QPoint> &points);
void writeCartesianTraceSection(QTextStream &stream, int level, int index,
    const QString &xChannel, const QString &yChannel, int colorIndex,
    int axisIndex, bool usesRightAxis);

template <typename Element>
std::array<QString, 4> collectChannels(const Element *element)
{
  std::array<QString, 4> channels{};
  if (!element) {
    return channels;
  }
  for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
    channels[i] = element->channel(i);
  }
  return channels;
}

} // namespace AdlWriter
