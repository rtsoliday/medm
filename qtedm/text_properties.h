#pragma once

enum class TextColorMode
{
  kStatic,
  kAlarm,
  kDiscrete
};

enum class TextVisibilityMode
{
  kStatic,
  kIfNotZero,
  kIfZero,
  kCalc
};

enum class TextMonitorFormat
{
  kDecimal,
  kExponential,
  kEngineering,
  kCompact,
  kTruncated,
  kHexadecimal,
  kOctal,
  kString,
  kSexagesimal,
  kSexagesimalHms,
  kSexagesimalDms,
};

enum class TextAreaWrapMode
{
  kNoWrap,
  kWidgetWidth,
  kFixedColumnWidth,
};

enum class TextAreaCommitMode
{
  kCtrlEnter,
  kEnter,
  kOnFocusLost,
  kExplicit,
};

constexpr int kMinimumTextWidth = 5;
constexpr int kMinimumTextElementHeight = 6;
constexpr int kMinimumTextHeight = 5;
