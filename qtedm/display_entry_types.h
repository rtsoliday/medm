#pragma once

#include <QList>
#include <QString>

#include "adl_parser.h"
#include "control_properties.h"

struct RelatedDisplayEntry
{
  QString label;
  QString name;
  QString args;
  RelatedDisplayMode mode = RelatedDisplayMode::kAdd;
};

struct ShellCommandEntry
{
  QString label;
  QString command;
  QString args;
  /* Where display macros were substituted into command and args when the
   * display was read from a file. An edited or restored command has no
   * such record, so every macro value is checked against it instead. */
  bool macroUsesKnown = false;
  QList<AdlMacroUse> commandMacroUses;
  QList<AdlMacroUse> argsMacroUses;
};
