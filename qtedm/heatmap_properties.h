#pragma once

enum class HeatmapDimensionSource
{
  kStatic,
  kChannel,
};

enum class HeatmapOrder
{
  kRowMajor,
  kColumnMajor,
};

enum class HeatmapColorMap
{
  kGrayscale,
  kJet,
  kHot,
  kCool,
  kRainbow,
  kTurbo,
};

enum class HeatmapRotation
{
  kNone,
  k90,
  k180,
  k270,
};

enum class HeatmapProfileMode
{
  kAbsolute,
  kAveraged,
};

enum class HeatmapRangeMode
{
  kAuto,
  kManual,
};

constexpr int kMinimumHeatmapWidth = 40;
constexpr int kMinimumHeatmapHeight = 40;
