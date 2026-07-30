#include "display_converter.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QRect>
#include <QSaveFile>
#include <QSet>
#include <QVariant>
#include <QVariantList>
#include <QXmlStreamReader>

#include <algorithm>

namespace {

constexpr int kReportSchemaVersion = 1;
constexpr int kDefaultDisplayWidth = 800;
constexpr int kDefaultDisplayHeight = 600;
constexpr qint64 kMaximumUiBytes = 64LL * 1024LL * 1024LL;

struct UiObject
{
  QString className;
  QString objectName;
  QMap<QString, QVariant> properties;
  QList<UiObject> children;
};

struct ConversionContext
{
  QString sourcePath;
  QString outputPath;
  QString outputDirectory;
  QString outputBaseName;
  QList<QJsonObject> records;
  QStringList generatedPaths;
  QSet<QString> usedChildNames;
  bool warnings = false;
  int sourceObjectCount = 0;
  int targetObjectCount = 0;
};

QString quoted(const QString &value)
{
  QString escaped = value;
  escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
  escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
  escaped.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
  return QStringLiteral("\"%1\"").arg(escaped);
}

QString normalizedClass(const QString &className)
{
  return className.trimmed().toLower();
}

QString slug(const QString &value, const QString &fallback)
{
  QString result;
  for (const QChar ch : value.trimmed().toLower()) {
    if (ch.isLetterOrNumber()) {
      result.append(ch);
    } else if (!result.isEmpty() && !result.endsWith(QLatin1Char('_'))) {
      result.append(QLatin1Char('_'));
    }
  }
  while (result.endsWith(QLatin1Char('_'))) {
    result.chop(1);
  }
  return result.isEmpty() ? fallback : result;
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
  const QFileInfo info(path);
  if (!QDir().mkpath(info.absolutePath())) {
    if (error) {
      *error = QStringLiteral("Could not create output directory: %1")
          .arg(info.absolutePath());
    }
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = QStringLiteral("Could not open %1: %2")
          .arg(path, file.errorString());
    }
    return false;
  }
  if (file.write(data) != data.size() || !file.commit()) {
    if (error) {
      *error = QStringLiteral("Could not write %1: %2")
          .arg(path, file.errorString());
    }
    return false;
  }
  return true;
}

QVariant readUiValue(QXmlStreamReader &reader)
{
  const QString type = reader.name().toString();
  if (type == QLatin1String("rect")) {
    QMap<QString, int> values;
    while (reader.readNextStartElement()) {
      bool ok = false;
      const int value = reader.readElementText().trimmed().toInt(&ok);
      if (ok) {
        values.insert(reader.name().toString(), value);
      }
    }
    return QRect(values.value(QStringLiteral("x")),
        values.value(QStringLiteral("y")),
        values.value(QStringLiteral("width")),
        values.value(QStringLiteral("height")));
  }
  if (type == QLatin1String("size")) {
    QVariantList values;
    while (reader.readNextStartElement()) {
      values.append(reader.readElementText().trimmed().toInt());
    }
    return values;
  }
  if (type == QLatin1String("color")) {
    QMap<QString, int> values;
    while (reader.readNextStartElement()) {
      bool ok = false;
      const int value = reader.readElementText().trimmed().toInt(&ok);
      if (ok) {
        values.insert(reader.name().toString(), value);
      }
    }
    return QStringLiteral("#%1%2%3")
        .arg(values.value(QStringLiteral("red")), 2, 16, QLatin1Char('0'))
        .arg(values.value(QStringLiteral("green")), 2, 16, QLatin1Char('0'))
        .arg(values.value(QStringLiteral("blue")), 2, 16, QLatin1Char('0'));
  }
  if (type == QLatin1String("stringlist")) {
    QStringList entries;
    while (reader.readNextStartElement()) {
      if (reader.name() == QLatin1String("string")) {
        entries.append(reader.readElementText());
      } else {
        reader.skipCurrentElement();
      }
    }
    return entries;
  }
  if (type == QLatin1String("bool")) {
    const QString text = reader.readElementText().trimmed().toLower();
    return text == QLatin1String("true") || text == QLatin1String("1");
  }
  if (type == QLatin1String("number")
      || type == QLatin1String("int")
      || type == QLatin1String("longlong")
      || type == QLatin1String("uint")
      || type == QLatin1String("ulonglong")) {
    bool ok = false;
    const qlonglong value = reader.readElementText().trimmed().toLongLong(&ok);
    return ok ? QVariant(value) : QVariant();
  }
  if (type == QLatin1String("double")) {
    bool ok = false;
    const double value = reader.readElementText().trimmed().toDouble(&ok);
    return ok ? QVariant(value) : QVariant();
  }
  if (type == QLatin1String("font")
      || type == QLatin1String("palette")
      || type == QLatin1String("brush")) {
    reader.skipCurrentElement();
    return QVariant();
  }
  if (type == QLatin1String("iconset")) {
    QString icon;
    while (reader.readNextStartElement()) {
      const QString candidate = reader.readElementText().trimmed();
      if (icon.isEmpty() && !candidate.isEmpty()) {
        icon = candidate;
      }
    }
    return icon;
  }
  return reader.readElementText();
}

void readContainer(QXmlStreamReader &reader, QList<UiObject> &children);

UiObject readWidget(QXmlStreamReader &reader)
{
  UiObject object;
  object.className = reader.attributes().value(QStringLiteral("class")).toString();
  object.objectName = reader.attributes().value(QStringLiteral("name")).toString();

  while (reader.readNextStartElement()) {
    const QString name = reader.name().toString();
    if (name == QLatin1String("property")
        || name == QLatin1String("attribute")) {
      const QString propertyName =
          reader.attributes().value(QStringLiteral("name")).toString();
      QVariant value;
      if (reader.readNextStartElement()) {
        value = readUiValue(reader);
      }
      while (!reader.atEnd()
          && !(reader.isEndElement()
              && (reader.name() == QLatin1String("property")
                  || reader.name() == QLatin1String("attribute")))) {
        reader.readNext();
      }
      if (name == QLatin1String("attribute")) {
        object.properties.insert(
            QStringLiteral("attribute:%1").arg(propertyName), value);
      } else {
        object.properties.insert(propertyName, value);
      }
    } else if (name == QLatin1String("widget")) {
      object.children.append(readWidget(reader));
    } else if (name == QLatin1String("layout")
        || name == QLatin1String("item")) {
      readContainer(reader, object.children);
    } else {
      reader.skipCurrentElement();
    }
  }
  return object;
}

void readContainer(QXmlStreamReader &reader, QList<UiObject> &children)
{
  while (reader.readNextStartElement()) {
    const QString name = reader.name().toString();
    if (name == QLatin1String("widget")) {
      children.append(readWidget(reader));
    } else if (name == QLatin1String("layout")
        || name == QLatin1String("item")) {
      readContainer(reader, children);
    } else {
      reader.skipCurrentElement();
    }
  }
}

bool parseUi(const QByteArray &bytes, UiObject *root, QString *error)
{
  if (!root) {
    return false;
  }
  QXmlStreamReader reader(bytes);
  bool foundUi = false;
  bool foundRoot = false;
  if (reader.readNextStartElement()
      && reader.name() == QLatin1String("ui")) {
    foundUi = true;
    while (reader.readNextStartElement()) {
      if (reader.name() == QLatin1String("widget") && !foundRoot) {
        *root = readWidget(reader);
        foundRoot = true;
      } else {
        reader.skipCurrentElement();
      }
    }
  }
  if (reader.hasError()) {
    if (error) {
      *error = QStringLiteral("Malformed Qt Designer XML at line %1: %2")
          .arg(reader.lineNumber()).arg(reader.errorString());
    }
    return false;
  }
  if (!foundUi || !foundRoot) {
    if (error) {
      *error = QStringLiteral(
          "The input is not a Qt Designer .ui file with a root widget.");
    }
    return false;
  }
  return true;
}

QRect geometryOf(const UiObject &object)
{
  const QVariant geometry = object.properties.value(QStringLiteral("geometry"));
  if (geometry.canConvert<QRect>()) {
    const QRect rect = geometry.toRect();
    if (rect.width() > 0 && rect.height() > 0) {
      return rect;
    }
  }
  return QRect(0, 0, 120, 30);
}

QRect contentBounds(const QList<UiObject> &objects,
    const QPoint &offset = QPoint())
{
  QRect bounds;
  for (const UiObject &object : objects) {
    QRect geometry = geometryOf(object);
    geometry.translate(offset);
    bounds = bounds.isNull() ? geometry : bounds.united(geometry);
    const QRect nested = contentBounds(
        object.children, geometry.topLeft());
    if (!nested.isNull()) {
      bounds = bounds.isNull() ? nested : bounds.united(nested);
    }
  }
  return bounds;
}

QString propertyText(const UiObject &object,
    std::initializer_list<const char *> names)
{
  for (const char *name : names) {
    const QVariant value =
        object.properties.value(QString::fromLatin1(name));
    if (!value.isValid()) {
      continue;
    }
    if (value.userType() == QMetaType::QStringList) {
      const QStringList list = value.toStringList();
      if (!list.isEmpty()) {
        return list.first().trimmed();
      }
    }
    const QString text = value.toString().trimmed();
    if (!text.isEmpty()) {
      return text;
    }
  }
  return QString();
}

QStringList channelList(const UiObject &object)
{
  QStringList channels;
  for (auto it = object.properties.cbegin();
       it != object.properties.cend(); ++it) {
    const QString key = it.key().toLower();
    if (!key.startsWith(QLatin1String("channel"))
        && key != QLatin1String("pv")
        && key != QLatin1String("pvname")) {
      continue;
    }
    QStringList candidates;
    if (it.value().userType() == QMetaType::QStringList) {
      candidates = it.value().toStringList();
    } else {
      candidates = it.value().toString().split(
          QLatin1Char(';'), Qt::SkipEmptyParts);
    }
    for (const QString &candidate : candidates) {
      const QString trimmed = candidate.trimmed();
      if (!trimmed.isEmpty() && !channels.contains(trimmed)) {
        channels.append(trimmed);
      }
    }
  }
  return channels;
}

void appendObjectGeometry(QString &adl, const QRect &rect, int level = 1)
{
  const QString indent(level * 2, QLatin1Char(' '));
  adl += indent + QStringLiteral("object {\n");
  adl += indent + QStringLiteral("  x=%1\n").arg(rect.x());
  adl += indent + QStringLiteral("  y=%1\n").arg(rect.y());
  adl += indent + QStringLiteral("  width=%1\n").arg(std::max(1, rect.width()));
  adl += indent + QStringLiteral("  height=%1\n").arg(std::max(1, rect.height()));
  adl += indent + QStringLiteral("}\n");
}

QString standardHeader(const QString &name, const QSize &size)
{
  return QStringLiteral(
      "file {\n"
      "  name=%1\n"
      "  version=030122\n"
      "}\n"
      "display {\n"
      "  object {\n"
      "    x=0\n"
      "    y=0\n"
      "    width=%2\n"
      "    height=%3\n"
      "  }\n"
      "  clr=14\n"
      "  bclr=4\n"
      "  cmap=\"\"\n"
      "  gridSpacing=5\n"
      "  gridOn=0\n"
      "  snapToGrid=0\n"
      "}\n"
      "\"color map\" {\n"
      "  ncolors=16\n"
      "  colors {\n"
      "    ffffff, ececec, dadada, c8c8c8,\n"
      "    bbbbbb, aeaeae, 9e9e9e, 919191,\n"
      "    858585, 787878, 696969, 5a5a5a,\n"
      "    464646, 2d2d2d, 000000, 00d800,\n"
      "  }\n"
      "}\n")
      .arg(quoted(name))
      .arg(std::max(1, size.width()))
      .arg(std::max(1, size.height()));
}

void appendRecord(ConversionContext &context, const UiObject &object,
    DisplayConversionDisposition disposition, const QString &targetType,
    const QString &message, const QString &pv = QString(),
    const QString &outputFile = QString())
{
  ++context.sourceObjectCount;
  if (disposition == DisplayConversionDisposition::kApproximated
      || disposition == DisplayConversionDisposition::kOmitted
      || disposition == DisplayConversionDisposition::kUnsupported) {
    context.warnings = true;
  }
  QJsonObject record;
  record[QStringLiteral("source_index")] = context.sourceObjectCount - 1;
  record[QStringLiteral("source_class")] = object.className;
  record[QStringLiteral("source_name")] = object.objectName;
  record[QStringLiteral("classification")] =
      DisplayConverter::dispositionName(disposition);
  record[QStringLiteral("target_type")] = targetType;
  record[QStringLiteral("message")] = message;
  if (!pv.isEmpty()) {
    record[QStringLiteral("pv")] = pv;
  }
  if (!outputFile.isEmpty()) {
    record[QStringLiteral("output_file")] = outputFile;
  }
  context.records.append(record);
}

void appendText(QString &adl, const QRect &rect, const QString &text,
    int color = 14)
{
  adl += QStringLiteral("text {\n");
  appendObjectGeometry(adl, rect);
  adl += QStringLiteral("  \"basic attribute\" {\n");
  adl += QStringLiteral("    clr=%1\n").arg(color);
  adl += QStringLiteral("  }\n");
  adl += QStringLiteral("  textix=%1\n").arg(quoted(text));
  adl += QStringLiteral("  align=\"horiz. centered\"\n");
  adl += QStringLiteral("}\n");
}

void appendMonitor(QString &adl, const QString &type, const QRect &rect,
    const QString &channel, bool control)
{
  adl += quoted(type) + QStringLiteral(" {\n");
  appendObjectGeometry(adl, rect);
  adl += QStringLiteral("  %1 {\n")
      .arg(control ? QStringLiteral("control") : QStringLiteral("monitor"));
  adl += QStringLiteral("    chan=%1\n").arg(quoted(channel));
  adl += QStringLiteral("    clr=14\n");
  adl += QStringLiteral("    bclr=4\n");
  adl += QStringLiteral("  }\n");
  adl += QStringLiteral("  limits {\n");
  adl += QStringLiteral("    loprSrc=\"default\"\n");
  adl += QStringLiteral("    hoprSrc=\"default\"\n");
  adl += QStringLiteral("    precSrc=\"default\"\n");
  adl += QStringLiteral("  }\n");
  adl += QStringLiteral("}\n");
}

void appendPlaceholder(QString &adl, const QRect &rect,
    const UiObject &object)
{
  adl += QStringLiteral("rectangle {\n");
  appendObjectGeometry(adl, rect);
  adl += QStringLiteral("  \"basic attribute\" {\n");
  adl += QStringLiteral("    clr=14\n");
  adl += QStringLiteral("    fill=\"outline\"\n");
  adl += QStringLiteral("    width=2\n");
  adl += QStringLiteral("  }\n");
  adl += QStringLiteral("}\n");
  appendText(adl, rect.adjusted(3, 3, -3, -3),
      QStringLiteral("Unsupported import: %1 (%2)")
          .arg(object.className, object.objectName), 14);
}

QString uniqueChildName(ConversionContext &context, const QString &label,
    int index)
{
  const QString base = QStringLiteral("%1_tab_%2")
      .arg(context.outputBaseName,
          slug(label, QStringLiteral("page_%1").arg(index + 1)));
  QString candidate = base;
  int suffix = 2;
  while (context.usedChildNames.contains(candidate)) {
    candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
  }
  context.usedChildNames.insert(candidate);
  return candidate + QStringLiteral(".adl");
}

void convertChildren(const QList<UiObject> &objects, QString &adl,
    ConversionContext &context, const QPoint &offset);

void convertObject(const UiObject &object, QString &adl,
    ConversionContext &context, const QPoint &offset)
{
  const QString cls = normalizedClass(object.className);
  QRect rect = geometryOf(object);
  rect.translate(offset);
  const QStringList channels = channelList(object);
  const QString channel = channels.value(0);
  const QString label = propertyText(object,
      {"text", "label", "title", "caption"});

  if (cls == QLatin1String("qwidget")
      || cls == QLatin1String("qscrollarea")
      || cls == QLatin1String("qstackedwidget")) {
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("flattened_container"),
        QStringLiteral("Non-visual Qt container flattened."));
    convertChildren(object.children, adl, context, rect.topLeft());
    return;
  }

  if (cls == QLatin1String("qtabwidget")
      || cls == QLatin1String("cadoubletabwidget")) {
    adl += QStringLiteral("qtedm_tabbed_display {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  mode=\"tabs\"\n");
    QString firstPageId;
    QSet<QString> pageIds;
    int pageIndex = 0;
    for (const UiObject &page : object.children) {
      const QString pageLabel = propertyText(page,
          {"attribute:title", "windowTitle", "title"});
      const QString effectiveLabel = pageLabel.isEmpty()
          ? QStringLiteral("Page %1").arg(pageIndex + 1) : pageLabel;
      const QString childName =
          uniqueChildName(context, effectiveLabel, pageIndex);
      const QString childPath =
          QDir(context.outputDirectory).filePath(childName);
      const QString pageIdBase = slug(page.objectName,
          QStringLiteral("page_%1").arg(pageIndex + 1));
      QString pageId = pageIdBase;
      int pageIdSuffix = 2;
      while (pageIds.contains(pageId)) {
        pageId = QStringLiteral("%1_%2")
            .arg(pageIdBase).arg(pageIdSuffix++);
      }
      pageIds.insert(pageId);
      if (firstPageId.isEmpty()) {
        firstPageId = pageId;
        adl += QStringLiteral("  active_page=%1\n").arg(quoted(pageId));
      }

      const QRect pageRect = geometryOf(page);
      const QRect pageContent = contentBounds(page.children);
      const int pageWidth = std::max({
          1, rect.width(), pageRect.width(), pageContent.right() + 1});
      const int pageHeight = std::max({
          1, rect.height(), pageRect.height(), pageContent.bottom() + 1});
      QString childAdl = standardHeader(childName,
          QSize(pageWidth, pageHeight));
      convertChildren(page.children, childAdl, context, QPoint());
      QString error;
      if (!writeFile(childPath, childAdl.toUtf8(), &error)) {
        appendRecord(context, page, DisplayConversionDisposition::kOmitted,
            QStringLiteral("tab_page"), error);
      } else {
        context.generatedPaths.append(childPath);
        appendRecord(context, page, DisplayConversionDisposition::kMapped,
            QStringLiteral("tab_page"),
            QStringLiteral("Converted to a file-backed child ADL."),
            QString(), childName);
      }

      const QString macros = propertyText(page,
          {"macro", "macros", "macroString"});
      adl += QStringLiteral("  page {\n");
      adl += QStringLiteral("    id=%1\n").arg(quoted(pageId));
      adl += QStringLiteral("    label=%1\n").arg(quoted(effectiveLabel));
      adl += QStringLiteral("    display=%1\n").arg(quoted(childName));
      if (!macros.isEmpty()) {
        adl += QStringLiteral("    macros=%1\n").arg(quoted(macros));
      }
      adl += QStringLiteral("    keepAlive=\"false\"\n");
      adl += QStringLiteral("  }\n");
      ++pageIndex;
    }
    adl += QStringLiteral("}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("qtedm_tabbed_display"),
        QStringLiteral("Tab pages converted to child ADLs."));
    return;
  }

  if (cls == QLatin1String("qlabel") || cls == QLatin1String("calabel")) {
    appendText(adl, rect, label);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("text"), QStringLiteral("Static label converted."));
    return;
  }

  if (cls == QLatin1String("calineedit")
      || cls == QLatin1String("cadisplayer")
      || cls == QLatin1String("catextupdate")) {
    appendMonitor(adl, QStringLiteral("text update"), rect, channel, false);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("text update"),
        QStringLiteral("Scalar monitor converted."), channel);
    return;
  }

  if (cls == QLatin1String("catextentry")
      || cls == QLatin1String("calineeditwrite")) {
    appendMonitor(adl, QStringLiteral("text entry"), rect, channel, true);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("text entry"),
        QStringLiteral("Scalar text control converted."), channel);
    return;
  }

  if (cls == QLatin1String("canumeric")
      || cls == QLatin1String("caapplynumeric")
      || cls == QLatin1String("caspinbox")) {
    adl += QStringLiteral("qtedm_spinbox {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  control {\n");
    adl += QStringLiteral("    chan=%1\n").arg(quoted(channel));
    adl += QStringLiteral("    clr=14\n    bclr=4\n  }\n");
    adl += QStringLiteral("  step=1\n");
    adl += QStringLiteral("  limits {\n"
                          "    loprSrc=\"default\"\n"
                          "    hoprSrc=\"default\"\n"
                          "    precSrc=\"default\"\n"
                          "  }\n}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("qtedm_spinbox"),
        QStringLiteral("Numeric control converted."), channel);
    return;
  }

  if (cls == QLatin1String("caslider")) {
    appendMonitor(adl, QStringLiteral("valuator"), rect, channel, true);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("valuator"), QStringLiteral("Slider converted."),
        channel);
    return;
  }

  if (cls == QLatin1String("cachoice")
      || cls == QLatin1String("caenum")) {
    appendMonitor(adl, QStringLiteral("choice button"), rect, channel, true);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("choice button"),
        QStringLiteral("Enum control converted."), channel);
    return;
  }

  if (cls == QLatin1String("camenu")) {
    appendMonitor(adl, QStringLiteral("menu"), rect, channel, true);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("menu"), QStringLiteral("Enum menu converted."),
        channel);
    return;
  }

  if (cls == QLatin1String("catogglebutton")) {
    adl += QStringLiteral("qtedm_toggle {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  control {\n");
    adl += QStringLiteral("    chan=%1\n").arg(quoted(channel));
    adl += QStringLiteral("    clr=14\n    bclr=4\n  }\n");
    adl += QStringLiteral("  offValue=\"0\"\n  onValue=\"1\"\n");
    adl += QStringLiteral("  offLabel=\"Off\"\n  onLabel=\"On\"\n}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("qtedm_toggle"),
        QStringLiteral("Toggle converted with explicit 0/1 values."),
        channel);
    return;
  }

  if (cls == QLatin1String("cathermo")) {
    appendMonitor(adl, QStringLiteral("indicator"), rect, channel, false);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("indicator"), QStringLiteral("Thermometer converted."),
        channel);
    return;
  }

  if (cls == QLatin1String("cacirculargauge")) {
    appendMonitor(adl, QStringLiteral("meter"), rect, channel, false);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("meter"), QStringLiteral("Circular gauge converted."),
        channel);
    return;
  }

  if (cls == QLatin1String("calineargauge")
      || cls == QLatin1String("caprogressbar")) {
    appendMonitor(adl, QStringLiteral("bar"), rect, channel, false);
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("bar"), QStringLiteral("Linear gauge converted."),
        channel);
    return;
  }

  if (cls == QLatin1String("castripplot")) {
    adl += QStringLiteral("\"strip chart\" {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  plotcom {\n"
                          "    title=%1\n"
                          "    xlabel=\"Seconds\"\n"
                          "    ylabel=\"Value\"\n"
                          "    clr=14\n"
                          "    bclr=4\n"
                          "  }\n").arg(quoted(label));
    int pen = 0;
    for (const QString &pv : channels) {
      adl += QStringLiteral("  pen[%1] {\n").arg(pen++);
      adl += QStringLiteral("    chan=%1\n").arg(quoted(pv));
      adl += QStringLiteral("    clr=15\n"
                            "    limits {\n"
                            "      loprSrc=\"default\"\n"
                            "      hoprSrc=\"default\"\n"
                            "    }\n"
                            "  }\n");
    }
    adl += QStringLiteral("}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("strip chart"),
        QStringLiteral("Strip plot channels converted."),
        channels.join(QLatin1Char(',')));
    return;
  }

  if (cls == QLatin1String("cacartesianplot")) {
    adl += QStringLiteral("\"cartesian plot\" {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  plotcom {\n"
                          "    title=%1\n"
                          "    xlabel=\"X\"\n"
                          "    ylabel=\"Y\"\n"
                          "    clr=14\n"
                          "    bclr=4\n"
                          "  }\n").arg(quoted(label));
    int trace = 0;
    for (const QString &pv : channels) {
      adl += QStringLiteral("  trace[%1] {\n").arg(trace++);
      adl += QStringLiteral("    ydata=%1\n").arg(quoted(pv));
      adl += QStringLiteral("    data_clr=15\n  }\n");
    }
    adl += QStringLiteral("}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kApproximated,
        QStringLiteral("cartesian plot"),
        QStringLiteral(
            "Plot channels were preserved; caQtDM axis styling needs review."),
        channels.join(QLatin1Char(',')));
    return;
  }

  if (cls == QLatin1String("cainclude")) {
    QString file = propertyText(object,
        {"filename", "fileName", "filenameList"});
    if (file.endsWith(QLatin1String(".ui"), Qt::CaseInsensitive)) {
      file.chop(3);
      file += QStringLiteral("adl");
    }
    const QString macros = propertyText(object,
        {"macro", "macros", "macroString"});
    adl += QStringLiteral("composite {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  \"composite name\"=%1\n").arg(quoted(file));
    if (!macros.isEmpty()) {
      adl += QStringLiteral("  \"composite file\" {\n");
      adl += QStringLiteral("    name=%1\n").arg(quoted(file));
      adl += QStringLiteral("    args=%1\n").arg(quoted(macros));
      adl += QStringLiteral("  }\n");
    }
    adl += QStringLiteral("}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kMapped,
        QStringLiteral("composite"),
        QStringLiteral("File-backed include converted."), QString(), file);
    return;
  }

  if (cls == QLatin1String("cacamera")) {
    const QString pvaChannel = channel.startsWith(
        QLatin1String("pva://"), Qt::CaseInsensitive)
        ? channel : QStringLiteral("pva://%1").arg(channel);
    adl += QStringLiteral("qtedm_ndarray_image {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  dataPv=%1\n").arg(quoted(pvaChannel));
    adl += QStringLiteral("  colorMap=\"grayscale\"\n"
                          "  rangeMode=\"auto\"\n"
                          "  preserveAspectRatio=true\n"
                          "  showPixelProbe=true\n"
                          "}\n");
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kApproximated,
        QStringLiteral("qtedm_ndarray_image"),
        QStringLiteral(
            "Camera converted to raw PVA NTNDArray; verify source protocol."),
        pvaChannel);
    return;
  }

  if (cls == QLatin1String("cagraphics")
      || cls == QLatin1String("qframe")
      || cls == QLatin1String("qgroupbox")) {
    const QString form = propertyText(object,
        {"form", "geometryType", "shape"}).toLower();
    QString target = QStringLiteral("rectangle");
    if (form.contains(QLatin1String("ellipse"))
        || form.contains(QLatin1String("circle"))) {
      target = QStringLiteral("oval");
    } else if (form.contains(QLatin1String("arc"))) {
      target = QStringLiteral("arc");
    } else if (form.contains(QLatin1String("line"))) {
      target = QStringLiteral("line");
    }
    adl += target + QStringLiteral(" {\n");
    appendObjectGeometry(adl, rect);
    adl += QStringLiteral("  \"basic attribute\" {\n"
                          "    clr=14\n"
                          "    fill=\"outline\"\n"
                          "    width=1\n"
                          "  }\n"
                          "}\n");
    if (!label.isEmpty()) {
      appendText(adl, QRect(rect.x() + 4, rect.y() + 2,
          std::max(1, rect.width() - 8), std::min(20, rect.height())),
          label);
      ++context.targetObjectCount;
    }
    ++context.targetObjectCount;
    appendRecord(context, object, DisplayConversionDisposition::kApproximated,
        target, QStringLiteral("Qt graphic styling was approximated."));
    convertChildren(object.children, adl, context, rect.topLeft());
    return;
  }

  appendPlaceholder(adl, rect, object);
  context.targetObjectCount += 2;
  appendRecord(context, object, DisplayConversionDisposition::kUnsupported,
      QStringLiteral("qtedm_import_placeholder"),
      QStringLiteral("No safe mapping exists; inserted a visible placeholder."),
      channel);
}

void convertChildren(const QList<UiObject> &objects, QString &adl,
    ConversionContext &context, const QPoint &offset)
{
  for (const UiObject &object : objects) {
    convertObject(object, adl, context, offset);
  }
}

QJsonObject countsObject(const QList<QJsonObject> &records)
{
  QMap<QString, int> counts;
  for (const QJsonObject &record : records) {
    const QString classification =
        record.value(QStringLiteral("classification")).toString();
    counts[classification] = counts.value(classification) + 1;
  }
  QJsonObject result;
  for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
    result[it.key()] = it.value();
  }
  return result;
}

} // namespace

QString DisplayConverter::dispositionName(
    DisplayConversionDisposition disposition)
{
  switch (disposition) {
  case DisplayConversionDisposition::kMapped:
    return QStringLiteral("mapped");
  case DisplayConversionDisposition::kApproximated:
    return QStringLiteral("approximated");
  case DisplayConversionDisposition::kOmitted:
    return QStringLiteral("omitted");
  case DisplayConversionDisposition::kUnsupported:
    return QStringLiteral("unsupported");
  }
  return QStringLiteral("unsupported");
}

DisplayConversionResult DisplayConverter::convert(
    const DisplayConversionOptions &requested)
{
  DisplayConversionResult result;
  DisplayConversionOptions options = requested;
  const QFileInfo inputInfo(options.inputPath);
  if (!inputInfo.exists() || !inputInfo.isFile()) {
    result.error = QStringLiteral("Input file does not exist: %1")
        .arg(options.inputPath);
    return result;
  }
  if (inputInfo.suffix().compare(
          QStringLiteral("ui"), Qt::CaseInsensitive) != 0) {
    result.error = QStringLiteral(
        "Unsupported input format. Version 1 accepts .ui files only.");
    return result;
  }
  if (inputInfo.size() > kMaximumUiBytes) {
    result.error = QStringLiteral("Input .ui exceeds the 64 MiB safety limit.");
    return result;
  }

  QFile input(options.inputPath);
  if (!input.open(QIODevice::ReadOnly)) {
    result.error = QStringLiteral("Could not read %1: %2")
        .arg(options.inputPath, input.errorString());
    return result;
  }
  const QByteArray sourceBytes = input.read(kMaximumUiBytes + 1);
  if (sourceBytes.size() > kMaximumUiBytes) {
    result.error = QStringLiteral("Input .ui exceeds the 64 MiB safety limit.");
    return result;
  }

  UiObject root;
  if (!parseUi(sourceBytes, &root, &result.error)) {
    return result;
  }

  if (options.outputPath.trimmed().isEmpty()) {
    options.outputPath = inputInfo.absolutePath() + QDir::separator()
        + inputInfo.completeBaseName() + QStringLiteral(".adl");
  }
  const QFileInfo outputInfo(options.outputPath);
  const QString outputDirectory = outputInfo.absolutePath();
  const QString outputBaseName = outputInfo.completeBaseName();
  if (options.reportPath.trimmed().isEmpty()) {
    options.reportPath = QDir(outputDirectory).filePath(
        outputBaseName + QStringLiteral(".qtedm-conversion.json"));
  }
  if (options.sourceCopyPath.trimmed().isEmpty()) {
    options.sourceCopyPath = QDir(outputDirectory).filePath(
        outputBaseName + QStringLiteral(".source.ui"));
  }

  const QString sourceAbsolute = inputInfo.absoluteFilePath();
  const QString outputAbsolute = outputInfo.absoluteFilePath();
  const QString reportAbsolute =
      QFileInfo(options.reportPath).absoluteFilePath();
  const QString sourceCopyAbsolute =
      QFileInfo(options.sourceCopyPath).absoluteFilePath();
  if (outputAbsolute == sourceAbsolute || reportAbsolute == sourceAbsolute
      || outputAbsolute == reportAbsolute
      || outputAbsolute == sourceCopyAbsolute
      || reportAbsolute == sourceCopyAbsolute) {
    result.error = QStringLiteral(
        "Input, ADL output, report, and preserved-source destinations "
        "must not overwrite one another.");
    return result;
  }

  ConversionContext context;
  context.sourcePath = inputInfo.absoluteFilePath();
  context.outputPath = outputInfo.absoluteFilePath();
  context.outputDirectory = outputDirectory;
  context.outputBaseName = outputBaseName;

  const QRect rootGeometry = geometryOf(root);
  const QSize displaySize(
      rootGeometry.width() > 0 ? rootGeometry.width() : kDefaultDisplayWidth,
      rootGeometry.height() > 0 ? rootGeometry.height() : kDefaultDisplayHeight);
  QString adl = standardHeader(outputInfo.fileName(), displaySize);
  appendRecord(context, root, DisplayConversionDisposition::kMapped,
      QStringLiteral("display"),
      QStringLiteral("Root Qt widget converted to the ADL display."));
  convertChildren(root.children, adl, context, QPoint());

  if (!writeFile(options.outputPath, adl.toUtf8(), &result.error)) {
    return result;
  }
  context.generatedPaths.prepend(outputInfo.absoluteFilePath());

  if (sourceAbsolute != sourceCopyAbsolute
      && !writeFile(options.sourceCopyPath, sourceBytes, &result.error)) {
    return result;
  }

  QJsonArray objects;
  for (const QJsonObject &record : context.records) {
    objects.append(record);
  }
  QJsonArray generated;
  for (const QString &path : context.generatedPaths) {
    generated.append(QFileInfo(path).absoluteFilePath());
  }
  QJsonObject report;
  report[QStringLiteral("schema")] =
      QStringLiteral("org.aps.qtedm.display-conversion");
  report[QStringLiteral("schema_version")] = kReportSchemaVersion;
  report[QStringLiteral("source_format")] =
      QStringLiteral("caqtdm-qt-designer-ui");
  report[QStringLiteral("source_path")] = sourceAbsolute;
  report[QStringLiteral("source_copy_path")] = sourceCopyAbsolute;
  report[QStringLiteral("source_sha256")] = QString::fromLatin1(
      QCryptographicHash::hash(sourceBytes, QCryptographicHash::Sha256).toHex());
  report[QStringLiteral("primary_output")] =
      outputInfo.absoluteFilePath();
  report[QStringLiteral("generated_displays")] = generated;
  report[QStringLiteral("source_object_count")] = context.sourceObjectCount;
  report[QStringLiteral("target_object_count")] = context.targetObjectCount;
  report[QStringLiteral("counts")] = countsObject(context.records);
  report[QStringLiteral("objects")] = objects;
  report[QStringLiteral("warnings")] = context.warnings;

  const QByteArray reportBytes =
      QJsonDocument(report).toJson(QJsonDocument::Indented);
  if (!writeFile(options.reportPath, reportBytes, &result.error)) {
    return result;
  }

  result.success = true;
  result.hasWarnings = context.warnings;
  result.outputPath = outputInfo.absoluteFilePath();
  result.reportPath = QFileInfo(options.reportPath).absoluteFilePath();
  result.sourceCopyPath = sourceCopyAbsolute;
  result.generatedDisplayPaths = context.generatedPaths;
  result.report = report;
  return result;
}
