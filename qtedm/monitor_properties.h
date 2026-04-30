#pragma once

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

enum class LedShape
{
  kCircle,
  kSquare,
  kRoundedSquare,
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

constexpr int kLedStateCount = 16;
constexpr int kMinimumSliderWidth = 20;
constexpr int kMinimumSliderHeight = 5;
constexpr int kMinimumWheelSwitchWidth = 70;
constexpr int kMinimumWheelSwitchHeight = 30;
constexpr int kMinimumMeterSize = 60;
constexpr int kMinimumBarSize = 5;
constexpr int kMinimumThermometerSize = 5;
constexpr int kMinimumByteSize = 5;
constexpr int kMinimumLedSize = 5;
constexpr int kDefaultLedSize = 20;
constexpr int kMinimumScaleSize = 20;
