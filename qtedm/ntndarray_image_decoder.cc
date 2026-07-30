#include "ntndarray_image_decoder.h"

#include <QColor>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool safeMultiply(quint64 left, quint64 right, quint64 *result)
{
  if (!result || (right != 0
          && left > std::numeric_limits<quint64>::max() / right)) {
    return false;
  }
  *result = left * right;
  return true;
}

int scalarSize(NtNdArrayScalarType type)
{
  switch (type) {
  case NtNdArrayScalarType::kInt8:
  case NtNdArrayScalarType::kUInt8:
    return 1;
  case NtNdArrayScalarType::kInt16:
  case NtNdArrayScalarType::kUInt16:
    return 2;
  case NtNdArrayScalarType::kInt32:
  case NtNdArrayScalarType::kUInt32:
  case NtNdArrayScalarType::kFloat32:
    return 4;
  case NtNdArrayScalarType::kInt64:
  case NtNdArrayScalarType::kUInt64:
  case NtNdArrayScalarType::kFloat64:
    return 8;
  }
  return 0;
}

double valueAt(const NtNdArrayFrame &frame, std::size_t index)
{
  if (!frame.data || index >= frame.elementCount) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const void *data = frame.data.get();
  switch (frame.scalarType) {
  case NtNdArrayScalarType::kInt8:
    return static_cast<const qint8 *>(data)[index];
  case NtNdArrayScalarType::kUInt8:
    return static_cast<const quint8 *>(data)[index];
  case NtNdArrayScalarType::kInt16:
    return static_cast<const qint16 *>(data)[index];
  case NtNdArrayScalarType::kUInt16:
    return static_cast<const quint16 *>(data)[index];
  case NtNdArrayScalarType::kInt32:
    return static_cast<const qint32 *>(data)[index];
  case NtNdArrayScalarType::kUInt32:
    return static_cast<const quint32 *>(data)[index];
  case NtNdArrayScalarType::kInt64:
    return static_cast<double>(static_cast<const qint64 *>(data)[index]);
  case NtNdArrayScalarType::kUInt64:
    return static_cast<double>(static_cast<const quint64 *>(data)[index]);
  case NtNdArrayScalarType::kFloat32:
    return static_cast<const float *>(data)[index];
  case NtNdArrayScalarType::kFloat64:
    return static_cast<const double *>(data)[index];
  }
  return std::numeric_limits<double>::quiet_NaN();
}

QRgb paletteColor(HeatmapColorMap map, int index)
{
  const double t = std::clamp(index, 0, 255) / 255.0;
  int red = 0;
  int green = 0;
  int blue = 0;
  switch (map) {
  case HeatmapColorMap::kGrayscale:
    red = green = blue = index;
    break;
  case HeatmapColorMap::kJet:
    red = std::clamp(static_cast<int>(
        255.0 * std::min(4.0 * t - 1.5, -4.0 * t + 4.5)), 0, 255);
    green = std::clamp(static_cast<int>(
        255.0 * std::min(4.0 * t - 0.5, -4.0 * t + 3.5)), 0, 255);
    blue = std::clamp(static_cast<int>(
        255.0 * std::min(4.0 * t + 0.5, -4.0 * t + 2.5)), 0, 255);
    break;
  case HeatmapColorMap::kHot:
    red = std::clamp(static_cast<int>(255.0 * 3.0 * t), 0, 255);
    green = std::clamp(static_cast<int>(
        255.0 * (3.0 * t - 1.0)), 0, 255);
    blue = std::clamp(static_cast<int>(
        255.0 * (3.0 * t - 2.0)), 0, 255);
    break;
  case HeatmapColorMap::kCool:
    red = std::clamp(static_cast<int>(255.0 * t), 0, 255);
    green = std::clamp(static_cast<int>(255.0 * (1.0 - t)), 0, 255);
    blue = 255;
    break;
  case HeatmapColorMap::kRainbow:
  case HeatmapColorMap::kTurbo: {
    const double hue = (1.0 - t) * 0.75;
    const QColor color = QColor::fromHsvF(hue, 1.0, 1.0);
    return color.rgb();
  }
  }
  return qRgb(red, green, blue);
}

struct Layout
{
  int width = 0;
  int height = 0;
  int components = 1;
};

bool frameLayout(const NtNdArrayFrame &frame, Layout *layout, QString *error)
{
  if (!layout) {
    return false;
  }
  const QVector<NtNdArrayDimension> &dims = frame.dimensions;
  if (frame.colorMode == NtNdArrayColorMode::kMono) {
    if (dims.size() != 2) {
      if (error) {
        *error = QStringLiteral("Mono NTNDArray must have exactly 2 dimensions.");
      }
      return false;
    }
    layout->width = dims.at(0).size;
    layout->height = dims.at(1).size;
    layout->components = 1;
    return true;
  }
  if (dims.size() != 3) {
    if (error) {
      *error = QStringLiteral("RGB NTNDArray must have exactly 3 dimensions.");
    }
    return false;
  }
  layout->components = 3;
  switch (frame.colorMode) {
  case NtNdArrayColorMode::kRgb1:
    if (dims.at(0).size != 3) {
      break;
    }
    layout->width = dims.at(1).size;
    layout->height = dims.at(2).size;
    return true;
  case NtNdArrayColorMode::kRgb2:
    if (dims.at(1).size != 3) {
      break;
    }
    layout->width = dims.at(0).size;
    layout->height = dims.at(2).size;
    return true;
  case NtNdArrayColorMode::kRgb3:
    if (dims.at(2).size != 3) {
      break;
    }
    layout->width = dims.at(0).size;
    layout->height = dims.at(1).size;
    return true;
  default:
    break;
  }
  if (error) {
    *error = QStringLiteral(
        "NTNDArray RGB dimensions do not match the ColorMode attribute.");
  }
  return false;
}

std::size_t sourceIndex(const NtNdArrayFrame &frame, int x, int y, int color)
{
  int coordinates[3] = {0, 0, 0};
  switch (frame.colorMode) {
  case NtNdArrayColorMode::kMono:
    coordinates[0] = x;
    coordinates[1] = y;
    break;
  case NtNdArrayColorMode::kRgb1:
    coordinates[0] = color;
    coordinates[1] = x;
    coordinates[2] = y;
    break;
  case NtNdArrayColorMode::kRgb2:
    coordinates[0] = x;
    coordinates[1] = color;
    coordinates[2] = y;
    break;
  case NtNdArrayColorMode::kRgb3:
    coordinates[0] = x;
    coordinates[1] = y;
    coordinates[2] = color;
    break;
  default:
    return frame.elementCount;
  }

  std::size_t index = 0;
  std::size_t stride = 1;
  for (int dimension = 0; dimension < frame.dimensions.size(); ++dimension) {
    const NtNdArrayDimension &dim = frame.dimensions.at(dimension);
    int coordinate = coordinates[dimension];
    if (dim.reverse) {
      coordinate = dim.size - 1 - coordinate;
    }
    if (coordinate < 0 || coordinate >= dim.size) {
      return frame.elementCount;
    }
    index += static_cast<std::size_t>(coordinate) * stride;
    stride *= static_cast<std::size_t>(dim.size);
  }
  return index;
}

int scaled(double value, double minimum, double maximum)
{
  if (!std::isfinite(value) || !(maximum > minimum)) {
    return 0;
  }
  const double normalized = (value - minimum) / (maximum - minimum);
  return std::clamp(static_cast<int>(std::lround(normalized * 255.0)),
      0, 255);
}

void transformedToSource(const NtNdArrayDecodedFrame &frame,
    int transformedX, int transformedY, int *sourceX, int *sourceY)
{
  int x = transformedX;
  int y = transformedY;
  const int width = frame.sourceWidth;
  const int height = frame.sourceHeight;
  switch (frame.options.rotation) {
  case HeatmapRotation::k90: {
    const int originalX = y;
    const int originalY = height - 1 - x;
    x = originalX;
    y = originalY;
    break;
  }
  case HeatmapRotation::k180:
    x = width - 1 - x;
    y = height - 1 - y;
    break;
  case HeatmapRotation::k270: {
    const int originalX = width - 1 - y;
    const int originalY = x;
    x = originalX;
    y = originalY;
    break;
  }
  case HeatmapRotation::kNone:
    break;
  }
  if (frame.options.flipHorizontal) {
    x = width - 1 - x;
  }
  if (frame.options.flipVertical) {
    y = height - 1 - y;
  }
  if (sourceX) {
    *sourceX = x;
  }
  if (sourceY) {
    *sourceY = y;
  }
}

} // namespace

NtNdArrayDecodedFrame NtNdArrayImageDecoder::decode(
    const NtNdArrayFrame &frame, const NtNdArrayDecodeOptions &options)
{
  NtNdArrayDecodedFrame result;
  result.source = frame;
  result.options = options;

  const QString codec = frame.codec.trimmed().toLower();
  if (!codec.isEmpty() && codec != QLatin1String("none")
      && codec != QLatin1String("raw")) {
    result.error = QStringLiteral(
        "Compressed NTNDArray codec “%1” is not supported.").arg(frame.codec);
    return result;
  }
  if (!frame.data || frame.elementCount == 0 || frame.byteCount == 0) {
    result.error = QStringLiteral("NTNDArray frame has no raw pixel data.");
    return result;
  }
  if (frame.byteCount > options.maximumInputBytes) {
    result.error = QStringLiteral(
        "NTNDArray input exceeds the configured memory limit.");
    return result;
  }
  const int bytesPerElement = scalarSize(frame.scalarType);
  if (bytesPerElement <= 0
      || frame.elementCount
          > std::numeric_limits<std::size_t>::max()
              / static_cast<std::size_t>(bytesPerElement)
      || frame.byteCount != frame.elementCount
          * static_cast<std::size_t>(bytesPerElement)) {
    result.error = QStringLiteral("NTNDArray byte count does not match its type.");
    return result;
  }
  if (frame.compressedSize > 0
      && static_cast<quint64>(frame.compressedSize) != frame.byteCount) {
    result.error = QStringLiteral(
        "Raw NTNDArray compressedSize does not match its value array.");
    return result;
  }
  if (frame.uncompressedSize > 0
      && static_cast<quint64>(frame.uncompressedSize) != frame.byteCount) {
    result.error = QStringLiteral(
        "Raw NTNDArray uncompressedSize does not match its value array.");
    return result;
  }

  Layout layout;
  if (!frameLayout(frame, &layout, &result.error)) {
    return result;
  }
  if (layout.width <= 0 || layout.height <= 0
      || layout.width > options.maximumDimension
      || layout.height > options.maximumDimension) {
    result.error = QStringLiteral(
        "NTNDArray dimensions exceed the configured size limit.");
    return result;
  }
  quint64 pixels = 0;
  quint64 outputBytes = 0;
  quint64 expectedElements = 0;
  if (!safeMultiply(static_cast<quint64>(layout.width),
          static_cast<quint64>(layout.height), &pixels)
      || !safeMultiply(pixels, 4, &outputBytes)
      || !safeMultiply(pixels, static_cast<quint64>(layout.components),
          &expectedElements)
      || outputBytes > options.maximumOutputBytes
      || expectedElements != frame.elementCount) {
    result.error = QStringLiteral(
        "NTNDArray dimensions, element count, or output memory are invalid.");
    return result;
  }

  double minimum = std::numeric_limits<double>::infinity();
  double maximum = -std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < frame.elementCount; ++index) {
    const double value = valueAt(frame, index);
    if (std::isfinite(value)) {
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
    }
  }
  if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
    result.error = QStringLiteral("NTNDArray frame has no finite pixel values.");
    return result;
  }
  if (options.rangeMode == HeatmapRangeMode::kManual) {
    minimum = options.rangeMinimum;
    maximum = options.rangeMaximum;
    if (!std::isfinite(minimum) || !std::isfinite(maximum)
        || !(maximum > minimum)) {
      result.error = QStringLiteral(
          "Manual intensity maximum must be greater than minimum.");
      return result;
    }
  } else if (!(maximum > minimum)) {
    maximum = minimum + 1.0;
  }

  QImage image(layout.width, layout.height, QImage::Format_RGB32);
  if (image.isNull()) {
    result.error = QStringLiteral("Could not allocate NTNDArray image.");
    return result;
  }
  for (int y = 0; y < layout.height; ++y) {
    QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
    for (int x = 0; x < layout.width; ++x) {
      if (layout.components == 1) {
        const double value = valueAt(frame, sourceIndex(frame, x, y, 0));
        line[x] = paletteColor(options.colorMap,
            scaled(value, minimum, maximum));
      } else {
        const int red = scaled(
            valueAt(frame, sourceIndex(frame, x, y, 0)), minimum, maximum);
        const int green = scaled(
            valueAt(frame, sourceIndex(frame, x, y, 1)), minimum, maximum);
        const int blue = scaled(
            valueAt(frame, sourceIndex(frame, x, y, 2)), minimum, maximum);
        line[x] = qRgb(red, green, blue);
      }
    }
  }

  if (options.flipHorizontal || options.flipVertical) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    Qt::Orientations orientations;
    if (options.flipHorizontal) {
      orientations |= Qt::Horizontal;
    }
    if (options.flipVertical) {
      orientations |= Qt::Vertical;
    }
    image = image.flipped(orientations);
#else
    image = image.mirrored(
        options.flipHorizontal, options.flipVertical);
#endif
  }
  if (options.rotation != HeatmapRotation::kNone) {
    QTransform transform;
    switch (options.rotation) {
    case HeatmapRotation::k90:
      transform.rotate(90.0);
      break;
    case HeatmapRotation::k180:
      transform.rotate(180.0);
      break;
    case HeatmapRotation::k270:
      transform.rotate(270.0);
      break;
    case HeatmapRotation::kNone:
      break;
    }
    image = image.transformed(transform, Qt::FastTransformation);
  }

  result.image = image;
  result.sourceWidth = layout.width;
  result.sourceHeight = layout.height;
  result.minimum = minimum;
  result.maximum = maximum;
  result.valid = true;
  return result;
}

QVector<double> NtNdArrayImageDecoder::pixelValues(
    const NtNdArrayDecodedFrame &frame, int transformedX, int transformedY)
{
  QVector<double> values;
  if (!frame.valid || transformedX < 0 || transformedY < 0
      || transformedX >= frame.image.width()
      || transformedY >= frame.image.height()) {
    return values;
  }
  int sourceX = 0;
  int sourceY = 0;
  transformedToSource(frame, transformedX, transformedY, &sourceX, &sourceY);
  if (sourceX < 0 || sourceX >= frame.sourceWidth
      || sourceY < 0 || sourceY >= frame.sourceHeight) {
    return values;
  }
  const int components =
      frame.source.colorMode == NtNdArrayColorMode::kMono ? 1 : 3;
  values.reserve(components);
  for (int component = 0; component < components; ++component) {
    values.append(valueAt(frame.source,
        sourceIndex(frame.source, sourceX, sourceY, component)));
  }
  return values;
}
