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
