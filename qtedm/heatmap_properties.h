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

constexpr int kMinimumHeatmapWidth = 40;
constexpr int kMinimumHeatmapHeight = 40;
