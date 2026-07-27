#pragma once

#include <QString>
#include <QVector>

#include <cstddef>
#include <memory>

enum class NtNdArrayScalarType {
  kInt8,
  kUInt8,
  kInt16,
  kUInt16,
  kInt32,
  kUInt32,
  kInt64,
  kUInt64,
  kFloat32,
  kFloat64,
};

enum class NtNdArrayColorMode {
  kMono = 0,
  kRgb1 = 2,
  kRgb2 = 3,
  kRgb3 = 4,
  kUnsupported = -1,
};

struct NtNdArrayDimension
{
  int size = 0;
  int offset = 0;
  int fullSize = 0;
  int binning = 1;
  bool reverse = false;
};

struct NtNdArrayFrame
{
  std::shared_ptr<const void> data;
  std::size_t elementCount = 0;
  std::size_t byteCount = 0;
  NtNdArrayScalarType scalarType = NtNdArrayScalarType::kUInt8;
  QVector<NtNdArrayDimension> dimensions;
  NtNdArrayColorMode colorMode = NtNdArrayColorMode::kUnsupported;
  QString codec;
  qint64 compressedSize = 0;
  qint64 uncompressedSize = 0;
  qint64 secondsPastEpoch = 0;
  int nanoseconds = 0;
  int uniqueId = 0;
};

struct PvaNtNdArrayPollResult
{
  bool connected = false;
  bool connectionChanged = false;
  bool hasFrame = false;
  int receivedFrames = 0;
  int droppedFrames = 0;
  NtNdArrayFrame frame;
  QString error;
};

struct PvaNtNdArraySource;

PvaNtNdArraySource *pvaNtNdArrayCreateSource(
    const QString &rawName, const QString &pvName, QString *error);
void pvaNtNdArrayDestroySource(PvaNtNdArraySource *source);
PvaNtNdArrayPollResult pvaNtNdArrayPoll(PvaNtNdArraySource *source);

