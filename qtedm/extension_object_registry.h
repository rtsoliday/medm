#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "display_state.h"
#include "qtedm_plugin_api.h"

struct ExtensionObjectDescriptor
{
  QString typeId;
  QString displayName;
  QString category;
  CreateTool createTool = CreateTool::kNone;
  QString pluginId;
  int schemaVersion = 0;
  QSize defaultSize;
  QVector<QtedmPluginPropertySchema> propertySchema;

  bool isPluginObject() const
  {
    return !pluginId.isEmpty();
  }
};

/*
 * Registry for built-in QtEDM extensions and version-1 plugin display types.
 * Plugin registrations are removed before their libraries are unloaded, while
 * built-in type IDs remain stable for the lifetime of the application.
 */
class ExtensionObjectRegistry
{
public:
  static ExtensionObjectRegistry &instance();

  const ExtensionObjectDescriptor *descriptor(const QString &typeId) const;
  const ExtensionObjectDescriptor *descriptor(CreateTool tool) const;
  QVector<ExtensionObjectDescriptor> descriptors() const;

  bool registerPluginObject(const QString &pluginId,
      const QtedmDisplayObjectType &descriptor);
  void unregisterPluginObjects();

private:
  ExtensionObjectRegistry();
  bool registerObject(const ExtensionObjectDescriptor &descriptor);

  QHash<QString, ExtensionObjectDescriptor> byTypeId_;
  QHash<int, QString> typeIdByTool_;
};
