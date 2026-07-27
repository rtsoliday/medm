#include "plugin_element.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaProperty>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>

#include "adl_writer.h"
#include "plugin_manager.h"

namespace {

void setNodeProperty(AdlNode &node, const QString &key, const QString &value)
{
  for (AdlProperty &property : node.properties) {
    if (property.key.compare(key, Qt::CaseInsensitive) == 0) {
      property.key = key;
      property.value = value;
      return;
    }
  }
  node.properties.append({key, value});
}

AdlNode &ensureChild(AdlNode &node, const QString &name)
{
  for (AdlNode &child : node.children) {
    if (normalizedAdlName(child.name) == normalizedAdlName(name)) {
      return child;
    }
  }
  node.children.append(AdlNode{name, {}, {}});
  return node.children.last();
}

QString variantTypeName(const QVariant &value)
{
  const int type = value.userType();
  if (type == QMetaType::Bool) {
    return QStringLiteral("boolean");
  }
  if (type == QMetaType::Int || type == QMetaType::UInt
      || type == QMetaType::LongLong || type == QMetaType::ULongLong) {
    return QStringLiteral("integer");
  }
  if (type == QMetaType::Double || type == QMetaType::Float) {
    return QStringLiteral("double");
  }
  if (type == QMetaType::QColor) {
    return QStringLiteral("color");
  }
  if (type == QMetaType::QStringList) {
    return QStringLiteral("string_list");
  }
  return QStringLiteral("string");
}

QString variantValue(const QVariant &value)
{
  const QString type = variantTypeName(value);
  if (type == QStringLiteral("boolean")) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (type == QStringLiteral("integer")) {
    return QString::number(value.toLongLong());
  }
  if (type == QStringLiteral("double")) {
    return QString::number(value.toDouble(), 'g', 15);
  }
  if (type == QStringLiteral("color")) {
    return value.value<QColor>().name(QColor::HexArgb);
  }
  if (type == QStringLiteral("string_list")) {
    QJsonArray array;
    for (const QString &entry : value.toStringList()) {
      array.append(entry);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
  }
  return value.toString();
}

QVariant parseVariant(const QString &typeName, const QString &value,
    bool *ok)
{
  const QString type = typeName.trimmed().toLower();
  bool valid = true;
  QVariant result;
  if (type == QStringLiteral("boolean") || type == QStringLiteral("bool")) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1")) {
      result = true;
    } else if (normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0")) {
      result = false;
    } else {
      valid = false;
    }
  } else if (type == QStringLiteral("integer")
      || type == QStringLiteral("int")) {
    bool converted = false;
    const qlonglong integer = value.toLongLong(&converted);
    valid = converted;
    result = integer;
  } else if (type == QStringLiteral("double")
      || type == QStringLiteral("number")) {
    bool converted = false;
    const double number = value.toDouble(&converted);
    valid = converted && std::isfinite(number);
    result = number;
  } else if (type == QStringLiteral("color")) {
    const QColor color(value);
    valid = color.isValid();
    result = color;
  } else if (type == QStringLiteral("string_list")) {
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(value.toUtf8(), &parseError);
    valid = parseError.error == QJsonParseError::NoError
        && document.isArray();
    QStringList entries;
    if (valid) {
      for (const QJsonValue &entry : document.array()) {
        if (!entry.isString()) {
          valid = false;
          break;
        }
        entries.append(entry.toString());
      }
    }
    result = entries;
  } else if (type.isEmpty() || type == QStringLiteral("string")) {
    result = value;
  } else {
    valid = false;
  }
  if (ok) {
    *ok = valid;
  }
  return valid ? result : QVariant();
}

QString nodeNameForOutput(const QString &name)
{
  if (name.contains(QRegularExpression(QStringLiteral("\\s")))) {
    return QStringLiteral("\"%1\"").arg(AdlWriter::escapeAdlString(name));
  }
  return name;
}

} // namespace

PluginElement::PluginElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(true);
  resize(160, 80);
}

bool PluginElement::loadFromAdlNode(const AdlNode &node, QString *error)
{
  if (normalizedAdlName(node.name) != QStringLiteral("qtedm_plugin")) {
    if (error) {
      *error = QStringLiteral("Expected a qtedm_plugin node.");
    }
    return false;
  }
  rawNode_ = node;
  hasRawNode_ = true;
  pluginId_ = propertyValue(node, QStringLiteral("pluginId")).trimmed();
  typeId_ = propertyValue(node, QStringLiteral("typeId")).trimmed().toLower();
  bool schemaOk = false;
  schemaVersion_ = propertyValue(node, QStringLiteral("schemaVersion"),
      QStringLiteral("1")).toInt(&schemaOk);
  if (pluginId_.isEmpty() || typeId_.isEmpty()
      || !schemaOk || schemaVersion_ < 1) {
    diagnostic_ = QStringLiteral(
        "Plugin node requires pluginId, typeId, and schemaVersion >= 1.");
    if (error) {
      *error = diagnostic_;
    }
    return false;
  }

  properties_.clear();
  for (const AdlNode &child : node.children) {
    if (normalizedAdlName(child.name) != QStringLiteral("property")) {
      continue;
    }
    const QString name =
        propertyValue(child, QStringLiteral("name")).trimmed();
    bool valueOk = false;
    const QVariant value = parseVariant(
        propertyValue(child, QStringLiteral("type")),
        propertyValue(child, QStringLiteral("value")), &valueOk);
    if (name.isEmpty() || !valueOk) {
      diagnostic_ = QStringLiteral("Invalid typed property in plugin node.");
      if (error) {
        *error = diagnostic_;
      }
      return false;
    }
    properties_.insert(name, value);
  }

  const QtedmLoadedDisplayObject *registration =
      QtedmPluginManager::instance().displayObject(pluginId_, typeId_);
  if (registration) {
    if (schemaVersion_ > registration->descriptor.schemaVersion) {
      diagnostic_ = QStringLiteral(
          "Plugin schema %1 is newer than loaded schema %2.")
          .arg(schemaVersion_)
          .arg(registration->descriptor.schemaVersion);
      if (error) {
        *error = diagnostic_;
      }
      update();
      return true;
    }
    for (const QtedmPluginPropertySchema &schema :
         registration->descriptor.properties) {
      if (!properties_.contains(schema.name)
          && schema.defaultValue.isValid()) {
        properties_.insert(schema.name, schema.defaultValue);
      }
    }
  }
  return constructPluginWidget(error) || !diagnostic_.isEmpty();
}

bool PluginElement::constructPluginWidget(QString *error)
{
  if (pluginWidget_) {
    pluginWidget_->deleteLater();
    pluginWidget_.clear();
  }
  diagnostic_.clear();
  QWidget *created = QtedmPluginManager::instance().createDisplayWidget(
      pluginId_, typeId_, this, &diagnostic_);
  if (!created) {
    if (diagnostic_.isEmpty()) {
      diagnostic_ = QStringLiteral("Plugin is missing or construction failed.");
    }
    if (error) {
      *error = diagnostic_;
    }
    update();
    return false;
  }
  pluginWidget_ = created;
  if (created->parentWidget() != this) {
    created->setParent(this);
  }
  created->setAttribute(Qt::WA_TransparentForMouseEvents, !executeMode_);
  if (!QtedmPluginManager::instance().applyDisplayProperties(
          pluginId_, typeId_, created, properties_, &diagnostic_)) {
    created->deleteLater();
    pluginWidget_.clear();
    if (diagnostic_.isEmpty()) {
      diagnostic_ = QStringLiteral("Plugin rejected its saved properties.");
    }
    if (error) {
      *error = diagnostic_;
    }
    update();
    return false;
  }
  updatePluginGeometry();
  created->show();
  update();
  return true;
}

AdlNode PluginElement::toAdlNode(const QRect &serializedGeometry) const
{
  AdlNode node = hasRawNode_ ? rawNode_
                             : AdlNode{QStringLiteral("qtedm_plugin"), {}, {}};
  node.name = QStringLiteral("qtedm_plugin");
  setNodeProperty(node, QStringLiteral("pluginId"), pluginId_);
  setNodeProperty(node, QStringLiteral("typeId"), typeId_);
  setNodeProperty(node, QStringLiteral("schemaVersion"),
      QString::number(schemaVersion_));

  AdlNode &object = ensureChild(node, QStringLiteral("object"));
  setNodeProperty(object, QStringLiteral("x"),
      QString::number(serializedGeometry.x()));
  setNodeProperty(object, QStringLiteral("y"),
      QString::number(serializedGeometry.y()));
  setNodeProperty(object, QStringLiteral("width"),
      QString::number(std::max(1, serializedGeometry.width())));
  setNodeProperty(object, QStringLiteral("height"),
      QString::number(std::max(1, serializedGeometry.height())));

  if (!pluginWidget_) {
    return node;
  }

  QVariantMap currentProperties =
      QtedmPluginManager::instance().serializeDisplayProperties(
          pluginId_, typeId_, pluginWidget_);
  if (currentProperties.isEmpty()) {
    currentProperties = properties_;
  }
  QSet<QString> serializedPropertyNames;
  for (auto it = currentProperties.constBegin();
       it != currentProperties.constEnd(); ++it) {
    serializedPropertyNames.insert(it.key().trimmed().toLower());
  }
  for (auto it = node.children.begin(); it != node.children.end();) {
    const QString savedName = propertyValue(
        *it, QStringLiteral("name")).trimmed().toLower();
    if (normalizedAdlName(it->name) == QStringLiteral("property")
        && serializedPropertyNames.contains(savedName)) {
      it = node.children.erase(it);
    } else {
      ++it;
    }
  }
  QStringList names = currentProperties.keys();
  std::sort(names.begin(), names.end(), [](const QString &left,
      const QString &right) {
    return left.compare(right, Qt::CaseInsensitive) < 0;
  });
  for (const QString &name : names) {
    const QVariant value = currentProperties.value(name);
    AdlNode propertyNode;
    propertyNode.name = QStringLiteral("property");
    propertyNode.properties.append(
        {QStringLiteral("name"), name});
    propertyNode.properties.append(
        {QStringLiteral("type"), variantTypeName(value)});
    propertyNode.properties.append(
        {QStringLiteral("value"), variantValue(value)});
    node.children.append(propertyNode);
  }
  return node;
}

void PluginElement::writeAdlNode(QTextStream &stream, const AdlNode &node,
    int level)
{
  AdlWriter::writeIndentedLine(stream, level,
      QStringLiteral("%1 {").arg(nodeNameForOutput(node.name)));
  for (const AdlProperty &property : node.properties) {
    AdlWriter::writeIndentedLine(stream, level + 1,
        QStringLiteral("%1=\"%2\"").arg(
            nodeNameForOutput(property.key),
            AdlWriter::escapeAdlString(property.value)));
  }
  for (const AdlNode &child : node.children) {
    writeAdlNode(stream, child, level + 1);
  }
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
}

QString PluginElement::pluginId() const
{
  return pluginId_;
}

QString PluginElement::typeId() const
{
  return typeId_;
}

int PluginElement::schemaVersion() const
{
  return schemaVersion_;
}

QVariantMap PluginElement::properties() const
{
  if (pluginWidget_) {
    const QVariantMap serialized =
        QtedmPluginManager::instance().serializeDisplayProperties(
            pluginId_, typeId_, pluginWidget_);
    if (!serialized.isEmpty()) {
      return serialized;
    }
  }
  return properties_;
}

void PluginElement::setProperties(const QVariantMap &properties)
{
  if (properties_ == properties) {
    return;
  }
  properties_ = properties;
  QString error;
  if (pluginWidget_) {
    if (!QtedmPluginManager::instance().applyDisplayProperties(
            pluginId_, typeId_, pluginWidget_, properties_, &error)) {
      diagnostic_ = error;
    } else {
      diagnostic_.clear();
    }
  } else {
    constructPluginWidget(&error);
  }
  notifyChanged();
  update();
}

QStringList PluginElement::channels() const
{
  return QtedmPluginManager::instance().displayChannels(
      pluginId_, typeId_, properties());
}

QWidget *PluginElement::pluginWidget() const
{
  return pluginWidget_;
}

bool PluginElement::pluginAvailable() const
{
  return !pluginWidget_.isNull();
}

QString PluginElement::diagnostic() const
{
  return diagnostic_;
}

void PluginElement::setRuntimeDiagnostic(const QString &diagnostic)
{
  if (diagnostic_ == diagnostic) {
    return;
  }
  diagnostic_ = diagnostic;
  update();
}

QtedmPluginRuntime *PluginElement::createRuntime(QString *error)
{
  if (!pluginWidget_) {
    if (error) {
      *error = diagnostic_.isEmpty()
          ? QStringLiteral("Plugin widget is unavailable.") : diagnostic_;
    }
    return nullptr;
  }
  return QtedmPluginManager::instance().createDisplayRuntime(
      pluginId_, typeId_, pluginWidget_, error);
}

void PluginElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool PluginElement::isSelected() const
{
  return selected_;
}

void PluginElement::setExecuteMode(bool execute)
{
  executeMode_ = execute;
  if (pluginWidget_) {
    pluginWidget_->setAttribute(Qt::WA_TransparentForMouseEvents, !execute);
  }
  if (execute) {
    setSelected(false);
  }
  update();
}

bool PluginElement::isExecuteMode() const
{
  return executeMode_;
}

QString PluginElement::ruleText() const
{
  if (pluginWidget_) {
    const QMetaObject *meta = pluginWidget_->metaObject();
    const int index = meta ? meta->indexOfProperty("text") : -1;
    if (index >= 0) {
      return meta->property(index).read(pluginWidget_).toString();
    }
  }
  return properties_.value(QStringLiteral("text"),
      properties_.value(QStringLiteral("label"))).toString();
}

bool PluginElement::setRuleText(const QString &text)
{
  if (pluginWidget_) {
    const QMetaObject *meta = pluginWidget_->metaObject();
    const int index = meta ? meta->indexOfProperty("text") : -1;
    if (index >= 0 && meta->property(index).isWritable()
        && meta->property(index).write(pluginWidget_, text)) {
      return true;
    }
  }
  QString propertyName;
  if (properties_.contains(QStringLiteral("text"))) {
    propertyName = QStringLiteral("text");
  } else if (properties_.contains(QStringLiteral("label"))) {
    propertyName = QStringLiteral("label");
  }
  if (propertyName.isEmpty()) {
    return false;
  }
  QVariantMap updated = properties_;
  updated.insert(propertyName, text);
  properties_ = updated;
  if (pluginWidget_) {
    QString error;
    return QtedmPluginManager::instance().applyDisplayProperties(
        pluginId_, typeId_, pluginWidget_, properties_, &error);
  }
  return true;
}

void PluginElement::setChangedCallback(std::function<void()> callback)
{
  changedCallback_ = std::move(callback);
}

void PluginElement::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  if (!pluginWidget_) {
    painter.fillRect(rect(), QColor(245, 225, 225));
    painter.setPen(QPen(QColor(150, 0, 0), 2, Qt::DashLine));
    painter.drawRect(rect().adjusted(1, 1, -2, -2));
    painter.setPen(QColor(110, 0, 0));
    const QString heading = QStringLiteral("Missing QtEDM plugin\n%1 / %2")
        .arg(pluginId_, typeId_);
    painter.drawText(rect().adjusted(6, 6, -6, -6),
        Qt::AlignCenter | Qt::TextWordWrap,
        diagnostic_.isEmpty() ? heading
                              : heading + QLatin1Char('\n') + diagnostic_);
  }
  if (selected_ && !executeMode_) {
    painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

void PluginElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  updatePluginGeometry();
}

void PluginElement::updatePluginGeometry()
{
  if (pluginWidget_) {
    pluginWidget_->setGeometry(rect());
  }
}

void PluginElement::notifyChanged()
{
  if (changedCallback_) {
    changedCallback_();
  }
}
