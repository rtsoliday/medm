#include "adl_writer.h"

#include <QTextStream>

#include <algorithm>
#include <limits>

#include "medm_colors.h"

namespace AdlWriter {

QString indentString(int level)
{
  return QString(level, QLatin1Char('\t'));
}

void writeIndentedLine(QTextStream &stream, int level, const QString &text)
{
  stream << '\n' << indentString(level) << text;
}

QString escapeAdlString(const QString &value)
{
  QString result;
  result.reserve(value.size());
  for (QChar ch : value) {
    switch (ch.unicode()) {
    case '\\':
      result.append(QStringLiteral("\\\\"));
      break;
    case '\"':
      result.append(QStringLiteral("\\\""));
      break;
    case '\n':
      result.append(QStringLiteral("\\n"));
      break;
    case '\r':
      result.append(QStringLiteral("\\r"));
      break;
    case '\t':
      result.append(QStringLiteral("\\t"));
      break;
    default:
      if (ch.isPrint()) {
        result.append(ch);
      } else {
        result.append(QStringLiteral("\\x%1")
            .arg(ch.unicode(), 2, 16, QLatin1Char('0')));
      }
      break;
    }
  }
  return result;
}

QString colorModeString(TextColorMode mode)
{
  switch (mode) {
  case TextColorMode::kAlarm:
    return QStringLiteral("alarm");
  case TextColorMode::kDiscrete:
    return QStringLiteral("discrete");
  case TextColorMode::kStatic:
  default:
    return QStringLiteral("static");
  }
}

QString visibilityModeString(TextVisibilityMode mode)
{
  switch (mode) {
  case TextVisibilityMode::kIfNotZero:
    return QStringLiteral("if not zero");
  case TextVisibilityMode::kIfZero:
    return QStringLiteral("if zero");
  case TextVisibilityMode::kCalc:
    return QStringLiteral("calc");
  case TextVisibilityMode::kStatic:
  default:
    return QStringLiteral("static");
  }
}

QString lineStyleString(RectangleLineStyle style)
{
  return style == RectangleLineStyle::kDash ? QStringLiteral("dash")
                                            : QStringLiteral("solid");
}

QString fillString(RectangleFill fill)
{
  return fill == RectangleFill::kSolid ? QStringLiteral("solid")
                                       : QStringLiteral("outline");
}

QString alignmentString(Qt::Alignment alignment)
{
  if (alignment & Qt::AlignHCenter) {
    return QStringLiteral("horiz. centered");
  }
  if (alignment & Qt::AlignRight) {
    return QStringLiteral("horiz. right");
  }
  return QStringLiteral("horiz. left");
}

QString imageTypeString(ImageType type)
{
  switch (type) {
  case ImageType::kGif:
    return QStringLiteral("gif");
  case ImageType::kTiff:
    return QStringLiteral("tiff");
  case ImageType::kNone:
  default:
    return QStringLiteral("no image");
  }
}

QString timeUnitsString(TimeUnits units)
{
  switch (units) {
  case TimeUnits::kMilliseconds:
    return QStringLiteral("milli-second");
  case TimeUnits::kMinutes:
    return QStringLiteral("minute");
  case TimeUnits::kSeconds:
  default:
    return QStringLiteral("second");
  }
}

QString channelFieldName(int index)
{
  if (index <= 0) {
    return QStringLiteral("chan");
  }
  return QStringLiteral("chan%1").arg(QChar(QLatin1Char('A' + index)));
}

QString textMonitorFormatString(TextMonitorFormat format)
{
  switch (format) {
  case TextMonitorFormat::kExponential:
    return QStringLiteral("exponential");
  case TextMonitorFormat::kEngineering:
    return QStringLiteral("engr. notation");
  case TextMonitorFormat::kCompact:
    return QStringLiteral("compact");
  case TextMonitorFormat::kTruncated:
    return QStringLiteral("truncated");
  case TextMonitorFormat::kHexadecimal:
    return QStringLiteral("hexadecimal");
  case TextMonitorFormat::kOctal:
    return QStringLiteral("octal");
  case TextMonitorFormat::kString:
    return QStringLiteral("string");
  case TextMonitorFormat::kSexagesimal:
    return QStringLiteral("sexagesimal");
  case TextMonitorFormat::kSexagesimalHms:
    return QStringLiteral("sexagesimal-hms");
  case TextMonitorFormat::kSexagesimalDms:
    return QStringLiteral("sexagesimal-dms");
  case TextMonitorFormat::kDecimal:
  default:
    return QStringLiteral("decimal");
  }
}

int medmLineWidthValue(int width)
{
  return width <= 1 ? 0 : width;
}

int medmColorIndex(const QColor &color)
{
  if (!color.isValid()) {
    return 14;
  }
  const int exact = MedmColors::indexForColor(color);
  if (exact >= 0) {
    return exact;
  }

  const auto &palette = MedmColors::palette();
  const QColor target = color.toRgb();
  int bestIndex = 0;
  int bestDistance = std::numeric_limits<int>::max();
  for (int i = 0; i < static_cast<int>(palette.size()); ++i) {
    const QColor candidate = palette[i].toRgb();
    const int dr = candidate.red() - target.red();
    const int dg = candidate.green() - target.green();
    const int db = candidate.blue() - target.blue();
    const int distance = dr * dr + dg * dg + db * db;
    if (distance < bestDistance) {
      bestDistance = distance;
      bestIndex = i;
      if (distance == 0) {
        break;
      }
    }
  }
  return bestIndex;
}

void writeObjectSection(QTextStream &stream, int level, const QRect &rect)
{
  const QRect bounded(rect.x(), rect.y(), std::max(rect.width(), 0),
      std::max(rect.height(), 0));
  writeIndentedLine(stream, level, QStringLiteral("object {"));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("x=%1").arg(bounded.x()));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("y=%1").arg(bounded.y()));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("width=%1").arg(std::max(bounded.width(), 0)));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("height=%1").arg(std::max(bounded.height(), 0)));
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

void writeBasicAttributeSection(QTextStream &stream, int level, int colorIndex,
    RectangleLineStyle lineStyle, RectangleFill fill, int lineWidth)
{
  writeIndentedLine(stream, level, QStringLiteral("\"basic attribute\" {"));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("clr=%1").arg(colorIndex));
  if (lineStyle != RectangleLineStyle::kSolid) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("style=\"%1\"").arg(lineStyleString(lineStyle)));
  }
  if (fill != RectangleFill::kSolid) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("fill=\"%1\"").arg(fillString(fill)));
  }
  const int medmWidth = medmLineWidthValue(lineWidth);
  if (medmWidth > 0) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("width=%1").arg(medmWidth));
  }
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

void writeDynamicAttributeSection(QTextStream &stream, int level,
    TextColorMode colorMode, TextVisibilityMode visibilityMode,
    const QString &calc, const std::array<QString, 4> &channels)
{
  const bool hasColor = colorMode != TextColorMode::kStatic;
  const bool hasVisibility = visibilityMode != TextVisibilityMode::kStatic;
  const bool hasCalc = !calc.trimmed().isEmpty();
  bool hasChannel = false;
  for (const QString &channel : channels) {
    if (!channel.trimmed().isEmpty()) {
      hasChannel = true;
      break;
    }
  }

  if (!hasColor && !hasVisibility && !hasCalc && !hasChannel) {
    return;
  }

  writeIndentedLine(stream, level,
      QStringLiteral("\"dynamic attribute\" {"));
  if (hasColor) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("clr=\"%1\"").arg(colorModeString(colorMode)));
  }
  if (hasVisibility) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("vis=\"%1\"")
            .arg(visibilityModeString(visibilityMode)));
  }
  if (hasCalc) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("calc=\"%1\"")
            .arg(escapeAdlString(calc)));
  }
  for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
    const QString channel = channels[i].trimmed();
    if (channel.isEmpty()) {
      continue;
    }
    writeIndentedLine(stream, level + 1,
        QStringLiteral("%1=\"%2\"")
            .arg(channelFieldName(i), escapeAdlString(channel)));
  }
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

void writeMonitorSection(QTextStream &stream, int level, const QString &channel,
    int colorIndex, int backgroundIndex)
{
  writeIndentedLine(stream, level, QStringLiteral("\"monitor\" {"));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("%1=\"%2\"")
          .arg(channelFieldName(0), escapeAdlString(channel)));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("clr=%1").arg(colorIndex));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("bclr=%1").arg(backgroundIndex));
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

void writePlotcom(QTextStream &stream, int level, const QString &title,
    const QString &xLabel, const QString &yLabel, int colorIndex,
    int backgroundIndex)
{
  writeIndentedLine(stream, level, QStringLiteral("plotcom {"));
  if (!title.trimmed().isEmpty()) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("title=\"%1\"")
            .arg(escapeAdlString(title.trimmed())));
  }
  if (!xLabel.trimmed().isEmpty()) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("xlabel=\"%1\"")
            .arg(escapeAdlString(xLabel.trimmed())));
  }
  if (!yLabel.trimmed().isEmpty()) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("ylabel=\"%1\"")
            .arg(escapeAdlString(yLabel.trimmed())));
  }
  writeIndentedLine(stream, level + 1,
      QStringLiteral("clr=%1").arg(colorIndex));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("bclr=%1").arg(backgroundIndex));
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

QString pvLimitSourceString(PvLimitSource source)
{
  switch (source) {
  case PvLimitSource::kDefault:
    return QStringLiteral("default");
  case PvLimitSource::kUser:
    return QStringLiteral("user specified");
  case PvLimitSource::kChannel:
  default:
    return QStringLiteral("channel");
  }
}

void writeLimitsSection(QTextStream &stream, int level, const PvLimits &limits)
{
  writeIndentedLine(stream, level, QStringLiteral("\"limits\" {"));
  if (limits.precisionSource != PvLimitSource::kChannel) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("precSrc=\"%1\"").arg(pvLimitSourceString(
            limits.precisionSource)));
  }
  if (limits.precisionSource == PvLimitSource::kDefault
      && limits.precisionDefault != 0) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("precDefault=%1").arg(limits.precisionDefault));
  }
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

void writeStripChartPenSection(QTextStream &stream, int level, int index,
    const QString &channel, int colorIndex, const PvLimits &limits)
{
  const QString trimmedChannel = channel.trimmed();
  if (trimmedChannel.isEmpty()) {
    return;
  }
  writeIndentedLine(stream, level,
      QStringLiteral("pen[%1] {").arg(index));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("chan=\"%1\"")
          .arg(escapeAdlString(trimmedChannel)));
  writeIndentedLine(stream, level + 1,
      QStringLiteral("clr=%1").arg(colorIndex));
  writeLimitsSection(stream, level + 1, limits);
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

void writePointsSection(
    QTextStream &stream, int level, const QVector<QPoint> &points)
{
  if (points.isEmpty()) {
    return;
  }
  writeIndentedLine(stream, level, QStringLiteral("points {"));
  for (const QPoint &point : points) {
    writeIndentedLine(stream, level + 1,
        QStringLiteral("(%1,%2)").arg(point.x()).arg(point.y()));
  }
  writeIndentedLine(stream, level, QStringLiteral("}"));
}

} // namespace AdlWriter

