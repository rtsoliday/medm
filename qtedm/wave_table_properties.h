#pragma once

enum class WaveTableLayout
{
  kRow,
  kColumn,
  kGrid,
};

enum class WaveTableValueFormat
{
  kDefault,
  kFixed,
  kScientific,
  kHex,
  kEngineering,
};

enum class WaveTableCharMode
{
  kString,
  kBytes,
  kAscii,
  kNumeric,
};
