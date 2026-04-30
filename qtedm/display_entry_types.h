#pragma once

#include <QString>

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
};
