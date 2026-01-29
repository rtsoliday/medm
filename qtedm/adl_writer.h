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
constexpr int kMedmPvaVersionNumber = 40000;

QString indentString(int level);
void writeIndentedLine(QTextStream &stream, int level, const QString &text);
QString escapeAdlString(const QString &value);
QString colorModeString(TextColorMode mode);
QString visibilityModeString(TextVisibilityMode mode);
QString lineStyleString(RectangleLineStyle style);
QString fillString(RectangleFill fill);
QString alignmentString(Qt::Alignment alignment);
QString imageTypeString(ImageType type);
QString meterLabelString(MeterLabel label);
QString barDirectionString(BarDirection direction);
QString barFillModeString(BarFill fill);
QString timeUnitsString(TimeUnits units);
QString channelFieldName(int index);
QString textMonitorFormatString(TextMonitorFormat format);
QString choiceButtonStackingString(ChoiceButtonStacking stacking);
QString relatedDisplayVisualString(RelatedDisplayVisual visual);
QString relatedDisplayModeString(RelatedDisplayMode mode);
int medmLineWidthValue(int width);
int medmColorIndex(const QColor &color);
void writeObjectSection(QTextStream &stream, int level, const QRect &rect);
void writeBasicAttributeSection(QTextStream &stream, int level, int colorIndex,
    RectangleLineStyle lineStyle, RectangleFill fill, int lineWidth,
    bool writeWidthForSingleLine = false,
    bool suppressWidthLine = false);
void writeDynamicAttributeSection(QTextStream &stream, int level,
    TextColorMode colorMode, TextVisibilityMode visibilityMode,
    const QString &calc, const std::array<QString, 5> &channels);
void writeMonitorSection(QTextStream &stream, int level, const QString &channel,
    int colorIndex, int backgroundIndex);
void writeControlSection(QTextStream &stream, int level, const QString &channel,
    int colorIndex, int backgroundIndex);
QString cartesianPlotStyleString(CartesianPlotStyle style);
QString cartesianEraseOldestString(bool eraseOldest);
QString cartesianEraseModeString(CartesianPlotEraseMode mode);
QString cartesianAxisStyleString(CartesianPlotAxisStyle style);
QString cartesianRangeStyleString(CartesianPlotRangeStyle style);
QString cartesianTimeFormatString(CartesianPlotTimeFormat format);
void writePlotcom(QTextStream &stream, int level, const QString &title,
    const QString &xLabel, const std::array<QString, 4> &yLabels,
    int colorIndex, int backgroundIndex);
void writeLimitsSection(QTextStream &stream, int level, const PvLimits &limits,
    bool includeChannelDefaults = false,
    bool forceEmptyBlock = false,
    bool includePrecisionDefaults = false,
    bool includeLowChannelDefault = false,
    bool includeHighChannelDefault = false);
void writeStripChartPenSection(QTextStream &stream, int level, int index,
    const QString &channel, int colorIndex, const PvLimits &limits);
void writePointsSection(
    QTextStream &stream, int level, const QVector<QPoint> &points);
void writeCartesianTraceSection(QTextStream &stream, int level, int index,
    const QString &xChannel, const QString &yChannel, int colorIndex,
    int axisIndex, bool usesRightAxis);
void writeCartesianAxisSection(QTextStream &stream, int level, int axisIndex,
    CartesianPlotAxisStyle axisStyle, CartesianPlotRangeStyle rangeStyle,
    double minRange, double maxRange, CartesianPlotTimeFormat timeFormat,
    bool includeTimeFormat);
void writeRelatedDisplayEntry(QTextStream &stream, int level, int index,
    const RelatedDisplayEntry &entry);

std::array<QString, 5> channelsForMedmFourValues(
    const std::array<QString, 5> &rawChannels);

template <typename Element>
std::array<QString, 5> collectChannels(const Element *element)
{
    std::array<QString, 5> channels{};
  if (!element) {
    return channels;
  }
  for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
    channels[i] = element->channel(i);
  }
  return channels;
}

} // namespace AdlWriter
