#pragma once

#include <QString>

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

enum class CartesianPlotAxisStyle
{
  kLinear,
  kLog10,
  kTime,
};

enum class CartesianPlotRangeStyle
{
  kChannel,
  kUserSpecified,
  kAutoScale,
};

enum class CartesianPlotTimeFormat
{
  kHhMmSs,
  kHhMm,
  kHh00,
  kMonthDayYear,
  kMonthDay,
  kMonthDayHour00,
  kWeekdayHour00,
};

enum class CartesianPlotTraceMode
{
  kNone,
  kXYScalar,
  kXScalar,
  kYScalar,
  kXVector,
  kYVector,
  kXVectorYScalar,
  kYVectorXScalar,
  kXYVector,
};

constexpr int kStripChartPenCount = 8;
constexpr int kCartesianPlotTraceCount = 8;
constexpr int kCartesianAxisCount = 5;
constexpr int kCartesianPlotMaximumSampleCount = 256;
constexpr int kMinimumDisplayWidth = 1;
constexpr int kMinimumDisplayHeight = 1;
constexpr int kDefaultDisplayWidth = 400;
constexpr int kDefaultDisplayHeight = 400;
constexpr int kDefaultGridSpacing = 5;
constexpr int kMinimumGridSpacing = 2;
constexpr bool kDefaultGridOn = false;
constexpr bool kDefaultSnapToGrid = false;
constexpr int kMinimumTextWidth = 5;
constexpr int kMinimumTextElementHeight = 8;
constexpr int kMinimumTextHeight = 5;
constexpr int kMinimumSliderWidth = 80;
constexpr int kMinimumSliderHeight = 30;
constexpr int kMinimumWheelSwitchWidth = 70;
constexpr int kMinimumWheelSwitchHeight = 30;
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
constexpr int kRelatedDisplayEntryCount = 16;
constexpr int kShellCommandEntryCount = 16;
constexpr int kMainWindowRightMargin = 5;
constexpr int kMainWindowTopMargin = 5;
