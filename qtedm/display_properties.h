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

enum class ChoiceButtonStacking
{
  kColumn,
  kRow,
  kRowColumn,
};

enum class MeterLabel
{
  kNone,
  kNoDecorations,
  kOutline,
  kLimits,
  kChannel,
};

enum class BarDirection
{
  kUp,
  kRight,
  kDown,
  kLeft,
};

enum class BarFill
{
  kFromEdge,
  kFromCenter,
};

enum class PvLimitSource
{
  kChannel,
  kDefault,
  kUser
};

struct PvLimits
{
  PvLimitSource lowSource = PvLimitSource::kChannel;
  double lowDefault = 0.0;
  PvLimitSource highSource = PvLimitSource::kChannel;
  double highDefault = 1.0;
  PvLimitSource precisionSource = PvLimitSource::kChannel;
  int precisionDefault = 0;
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

enum class TimeUnits
{
  kMilliseconds,
  kSeconds,
  kMinutes,
};

enum class CartesianPlotStyle
{
  kPoint,
  kLine,
  kStep,
  kFillUnder,
};

enum class CartesianPlotEraseMode
{
  kIfNotZero,
  kIfZero,
};

enum class CartesianPlotYAxis
{
  kY1,
  kY2,
  kY3,
  kY4,
};

constexpr int kStripChartPenCount = 8;
constexpr int kCartesianPlotTraceCount = 8;
constexpr int kDefaultDisplayWidth = 400;
constexpr int kDefaultDisplayHeight = 400;
constexpr int kDefaultGridSpacing = 5;
constexpr int kMinimumGridSpacing = 2;
constexpr bool kDefaultGridOn = false;
constexpr bool kDefaultSnapToGrid = false;
constexpr int kMinimumTextWidth = 40;
constexpr int kMinimumTextHeight = 20;
constexpr int kMinimumRectangleSize = 6;
constexpr int kMinimumMeterSize = 60;
constexpr int kMinimumBarSize = 30;
constexpr int kMinimumByteSize = 30;
constexpr int kMinimumScaleSize = 40;
constexpr int kMinimumStripChartWidth = 120;
constexpr int kMinimumStripChartHeight = 80;
constexpr int kMinimumCartesianPlotWidth = 160;
constexpr int kMinimumCartesianPlotHeight = 120;
constexpr double kDefaultStripChartPeriod = 60.0;
constexpr int kMainWindowRightMargin = 5;
constexpr int kMainWindowTopMargin = 5;

