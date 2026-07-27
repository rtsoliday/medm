#pragma once

#include <QImage>
#include <QString>
#include <QVector>

#include "heatmap_properties.h"
#include "pva_ntndarray_source.h"

struct NtNdArrayDecodeOptions
{
  HeatmapColorMap colorMap = HeatmapColorMap::kGrayscale;
  HeatmapRangeMode rangeMode = HeatmapRangeMode::kAuto;
  double rangeMinimum = 0.0;
  double rangeMaximum = 255.0;
  bool flipHorizontal = false;
  bool flipVertical = false;
  HeatmapRotation rotation = HeatmapRotation::kNone;
  int maximumDimension = 16384;
  quint64 maximumInputBytes = 512ULL * 1024ULL * 1024ULL;
  quint64 maximumOutputBytes = 256ULL * 1024ULL * 1024ULL;
};

struct NtNdArrayDecodedFrame
{
  bool valid = false;
  QString error;
  QImage image;
  NtNdArrayFrame source;
  NtNdArrayDecodeOptions options;
  int sourceWidth = 0;
  int sourceHeight = 0;
  double minimum = 0.0;
  double maximum = 0.0;
};

class NtNdArrayImageDecoder
{
public:
  static NtNdArrayDecodedFrame decode(const NtNdArrayFrame &frame,
      const NtNdArrayDecodeOptions &options);

  static QVector<double> pixelValues(
      const NtNdArrayDecodedFrame &frame, int transformedX, int transformedY);
};

