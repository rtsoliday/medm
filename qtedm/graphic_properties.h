#pragma once

#include <QRect>

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

/* MEDM outlines inset the path by (line width + 1)/2. A cosmetic outline
 * has a widget padded by one pixel for X11's inclusive far endpoints. */
inline QRect medmOutlineRect(const QRect &rect, int lineWidth)
{
  if (lineWidth == 0) {
    return rect.adjusted(0, 0, -1, -1);
  }
  const int inset = (lineWidth + 1) / 2;
  return rect.adjusted(inset, inset, -inset, -inset);
}

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
