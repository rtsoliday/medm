#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "display_state.h"

struct ExtensionObjectDescriptor
{
  QString typeId;
  QString displayName;
  QString category;
  CreateTool createTool = CreateTool::kNone;
};

/*
 * Internal registry for QtEDM extension objects.  Phase 1 deliberately keeps
 * this registry private to the application; the stable plugin API planned for
 * Phase 5 can promote the same type IDs without changing saved displays.
 */
class ExtensionObjectRegistry
{
public:
  static ExtensionObjectRegistry &instance();

  const ExtensionObjectDescriptor *descriptor(const QString &typeId) const;
  const ExtensionObjectDescriptor *descriptor(CreateTool tool) const;
  QVector<ExtensionObjectDescriptor> descriptors() const;

private:
  ExtensionObjectRegistry();
  void registerObject(const ExtensionObjectDescriptor &descriptor);

  QHash<QString, ExtensionObjectDescriptor> byTypeId_;
  QHash<int, QString> typeIdByTool_;
};
