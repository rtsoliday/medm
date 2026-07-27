#include "extension_object_registry.h"

#include <algorithm>

#include <QDebug>

ExtensionObjectRegistry &ExtensionObjectRegistry::instance()
{
  static ExtensionObjectRegistry registry;
  return registry;
}

ExtensionObjectRegistry::ExtensionObjectRegistry()
{
  registerObject({QStringLiteral("qtedm_symbol"),
      QStringLiteral("Multi-State Symbol"), QStringLiteral("Monitors"),
      CreateTool::kQtedmSymbol});
  registerObject({QStringLiteral("qtedm_toggle"),
      QStringLiteral("Toggle"), QStringLiteral("Controls"),
      CreateTool::kQtedmToggle});
  registerObject({QStringLiteral("qtedm_spinbox"),
      QStringLiteral("Spin Box"), QStringLiteral("Controls"),
      CreateTool::kQtedmSpinBox});
  registerObject({QStringLiteral("qtedm_tabbed_display"),
      QStringLiteral("Tabbed Display"), QStringLiteral("Containers"),
      CreateTool::kQtedmTabbedDisplay});
  registerObject({QStringLiteral("qtedm_archive_plot"),
      QStringLiteral("Archive Plot"), QStringLiteral("Monitors"),
      CreateTool::kQtedmArchivePlot});
  registerObject({QStringLiteral("qtedm_ndarray_image"),
      QStringLiteral("NTNDArray Image"), QStringLiteral("Monitors"),
      CreateTool::kQtedmNdArrayImage});
}

bool ExtensionObjectRegistry::registerObject(
    const ExtensionObjectDescriptor &descriptor)
{
  const QString typeId = descriptor.typeId.trimmed().toLower();
  const bool hasCreateTool = descriptor.createTool != CreateTool::kNone;
  if (typeId.isEmpty() || byTypeId_.contains(typeId)
      || (!hasCreateTool && descriptor.pluginId.trimmed().isEmpty())
      || (hasCreateTool
          && typeIdByTool_.contains(static_cast<int>(descriptor.createTool)))) {
    qWarning() << "Rejected duplicate or invalid QtEDM extension object"
               << descriptor.typeId;
    return false;
  }
  ExtensionObjectDescriptor normalized = descriptor;
  normalized.typeId = typeId;
  normalized.pluginId = descriptor.pluginId.trimmed();
  byTypeId_.insert(typeId, normalized);
  if (hasCreateTool) {
    typeIdByTool_.insert(static_cast<int>(descriptor.createTool), typeId);
  }
  return true;
}

const ExtensionObjectDescriptor *ExtensionObjectRegistry::descriptor(
    const QString &typeId) const
{
  const auto it = byTypeId_.constFind(typeId.trimmed().toLower());
  return it == byTypeId_.cend() ? nullptr : &it.value();
}

const ExtensionObjectDescriptor *ExtensionObjectRegistry::descriptor(
    CreateTool tool) const
{
  const auto idIt = typeIdByTool_.constFind(static_cast<int>(tool));
  return idIt == typeIdByTool_.cend() ? nullptr : descriptor(idIt.value());
}

QVector<ExtensionObjectDescriptor> ExtensionObjectRegistry::descriptors() const
{
  QVector<ExtensionObjectDescriptor> result;
  result.reserve(byTypeId_.size());
  for (const ExtensionObjectDescriptor &descriptor : byTypeId_) {
    result.append(descriptor);
  }
  std::sort(result.begin(), result.end(),
      [](const ExtensionObjectDescriptor &left,
          const ExtensionObjectDescriptor &right) {
        const int category = left.category.compare(
            right.category, Qt::CaseInsensitive);
        if (category != 0) {
          return category < 0;
        }
        return left.displayName.compare(
            right.displayName, Qt::CaseInsensitive) < 0;
      });
  return result;
}

bool ExtensionObjectRegistry::registerPluginObject(const QString &pluginId,
    const QtedmDisplayObjectType &descriptor)
{
  ExtensionObjectDescriptor extension;
  extension.typeId = descriptor.typeId;
  extension.displayName = descriptor.displayName;
  extension.category = descriptor.category.isEmpty()
      ? QStringLiteral("Plugins") : descriptor.category;
  extension.pluginId = pluginId;
  extension.schemaVersion = descriptor.schemaVersion;
  extension.defaultSize = descriptor.defaultSize;
  extension.propertySchema = descriptor.properties;
  return registerObject(extension);
}

void ExtensionObjectRegistry::unregisterPluginObjects()
{
  for (auto it = byTypeId_.begin(); it != byTypeId_.end();) {
    if (it->isPluginObject()) {
      it = byTypeId_.erase(it);
    } else {
      ++it;
    }
  }
}
