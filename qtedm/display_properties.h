#pragma once

// Shared enums and constants used across QtEDM widgets and dialogs.

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

enum class RectangleFill
{
  kOutline,
  kSolid
};

enum class RectangleLineStyle
{
  kSolid,
  kDash
};

enum class ImageType
{
  kNone,
  kGif,
  kTiff
};

constexpr int kDefaultDisplayWidth = 400;
constexpr int kDefaultDisplayHeight = 400;
constexpr int kDefaultGridSpacing = 5;
constexpr int kMinimumGridSpacing = 2;
constexpr bool kDefaultGridOn = false;
constexpr bool kDefaultSnapToGrid = false;
constexpr int kMinimumTextWidth = 40;
constexpr int kMinimumTextHeight = 20;
constexpr int kMinimumRectangleSize = 6;
constexpr int kMainWindowRightMargin = 5;
constexpr int kMainWindowTopMargin = 5;

