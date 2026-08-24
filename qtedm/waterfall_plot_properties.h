#pragma once

enum class WaterfallScrollDirection
{
  kTopToBottom,
  kBottomToTop,
  kLeftToRight,
  kRightToLeft,
};

enum class WaterfallIntensityScale
{
  kAuto,
  kManual,
  kLog,
};

enum class WaterfallEraseMode
{
  kIfNotZero,
  kIfZero,
};

constexpr int kMinimumWaterfallPlotWidth = 160;
constexpr int kMinimumWaterfallPlotHeight = 93;
constexpr int kWaterfallDefaultHistory = 200;
constexpr int kWaterfallMaxHistory = 4096;
constexpr int kWaterfallMaxColumns = 65536;
constexpr int kWaterfallMaxBufferedValues = 8 * 1024 * 1024;

constexpr int waterfallMaximumColumnsForHistory(int historyCount)
{
  const int safeHistoryCount = historyCount > 0 ? historyCount : 1;
  const int columnsForBuffer =
      kWaterfallMaxBufferedValues / safeHistoryCount;
  const int positiveColumns = columnsForBuffer > 0 ? columnsForBuffer : 1;
  return positiveColumns < kWaterfallMaxColumns
      ? positiveColumns
      : kWaterfallMaxColumns;
}
