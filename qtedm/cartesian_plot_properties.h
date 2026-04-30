#pragma once

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

constexpr int kCartesianPlotTraceCount = 8;
constexpr int kCartesianAxisCount = 5;
constexpr int kCartesianPlotMaximumSampleCount = 256;
constexpr int kMinimumCartesianPlotWidth = 160;
constexpr int kMinimumCartesianPlotHeight = 93;
