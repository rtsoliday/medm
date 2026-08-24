#pragma once

#include <QHash>
#include <QString>
#include <Qt>

#include "related_display_element.h"

struct RelatedDisplayLaunchSpec
{
  QString fileName;
  QHash<QString, QString> macros;
  bool replace = false;
};

bool prepareRelatedDisplayLaunch(const RelatedDisplayEntry &entry,
    Qt::KeyboardModifiers modifiers,
    const QHash<QString, QString> &inheritedMacros,
    RelatedDisplayLaunchSpec *result, QString *errorMessage = nullptr);
