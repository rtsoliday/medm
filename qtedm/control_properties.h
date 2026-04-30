#pragma once

enum class SetpointToleranceMode
{
  kNone,
  kAbsolute,
};

enum class ChoiceButtonStacking
{
  kColumn,
  kRow,
  kRowColumn,
};

enum class RelatedDisplayMode
{
  kAdd,
  kReplace
};

enum class RelatedDisplayVisual
{
  kMenu,
  kRowOfButtons,
  kColumnOfButtons,
  kHiddenButton
};

constexpr int kRelatedDisplayEntryCount = 16;
constexpr int kShellCommandEntryCount = 16;
