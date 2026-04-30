#pragma once

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

enum class ExpressionChannelEventSignalMode
{
  kNever,
  kOnFirstChange,
  kOnAnyChange,
  kTriggerZeroToOne,
  kTriggerOneToZero
};

enum class ImageType
{
  kNone,
  kGif,
  kTiff
};

constexpr int kMinimumRectangleSize = 1;
constexpr int kMinimumExpressionChannelWidth = 80;
constexpr int kMinimumExpressionChannelHeight = 40;
