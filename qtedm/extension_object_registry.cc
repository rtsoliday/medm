#include "extension_object_registry.h"

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
}

void ExtensionObjectRegistry::registerObject(
    const ExtensionObjectDescriptor &descriptor)
{
  const QString typeId = descriptor.typeId.trimmed().toLower();
  if (typeId.isEmpty() || descriptor.createTool == CreateTool::kNone
      || byTypeId_.contains(typeId)
      || typeIdByTool_.contains(static_cast<int>(descriptor.createTool))) {
    qWarning() << "Rejected duplicate or invalid QtEDM extension object"
               << descriptor.typeId;
    return;
  }
  ExtensionObjectDescriptor normalized = descriptor;
  normalized.typeId = typeId;
  byTypeId_.insert(typeId, normalized);
  typeIdByTool_.insert(static_cast<int>(descriptor.createTool), typeId);
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
  return result;
}
