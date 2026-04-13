#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

struct Options {
  QString expectedPath;
  QString actualPath;
  QString diffPath;
  qint64 maxDifferentPixels = 0;
  double maxMeanAbsoluteDelta = 0.0;
  int maxChannelDelta = 0;
  bool showHelp = false;
  QString invalidOption;
};

void printUsage(const QString &programName)
{
  fprintf(stderr,
      "Usage: %s --expected file --actual file [options]\n"
      "Options:\n"
      "  --diff file                     Write a red diff image on mismatch\n"
      "  --max-different-pixels count    Allowed changed pixels (default: 0)\n"
      "  --max-mean-absolute-delta val   Allowed mean channel delta (default: 0)\n"
      "  --max-channel-delta value       Allowed max channel delta (default: 0)\n",
      programName.toLocal8Bit().constData());
}

Options parseArguments(const QStringList &args)
{
  Options options;

  for (int i = 1; i < args.size(); ++i) {
    const QString &arg = args.at(i);
    if (arg == QLatin1String("--expected")) {
      if ((i + 1) < args.size()) {
        options.expectedPath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("--actual")) {
      if ((i + 1) < args.size()) {
        options.actualPath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("--diff")) {
      if ((i + 1) < args.size()) {
        options.diffPath = args.at(++i);
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("--max-different-pixels")) {
      if ((i + 1) < args.size()) {
        bool ok = false;
        const qint64 value = args.at(++i).toLongLong(&ok);
        if (!ok || value < 0) {
          options.invalidOption = arg;
        } else {
          options.maxDifferentPixels = value;
        }
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("--max-mean-absolute-delta")) {
      if ((i + 1) < args.size()) {
        bool ok = false;
        const double value = args.at(++i).toDouble(&ok);
        if (!ok || value < 0.0) {
          options.invalidOption = arg;
        } else {
          options.maxMeanAbsoluteDelta = value;
        }
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("--max-channel-delta")) {
      if ((i + 1) < args.size()) {
        bool ok = false;
        const int value = args.at(++i).toInt(&ok);
        if (!ok || value < 0) {
          options.invalidOption = arg;
        } else {
          options.maxChannelDelta = value;
        }
      } else {
        options.invalidOption = arg;
      }
    } else if (arg == QLatin1String("--help") ||
               arg == QLatin1String("-h")) {
      options.showHelp = true;
    } else {
      options.invalidOption = arg;
    }
  }

  if (!options.invalidOption.isEmpty()) {
    options.showHelp = true;
  }
  return options;
}

QImage loadImage(const QString &path, QString *errorMessage)
{
  QImage image(path);
  if (image.isNull()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to load image: %1").arg(path);
    }
    return QImage();
  }
  return image.convertToFormat(QImage::Format_ARGB32);
}

bool ensureParentDirectoryExists(const QString &path)
{
  const QFileInfo info(path);
  QDir dir = info.absoluteDir();
  return dir.exists() || QDir().mkpath(dir.absolutePath());
}

}  // namespace

int main(int argc, char *argv[])
{
  QCoreApplication app(argc, argv);

  const QStringList args = QCoreApplication::arguments();
  const Options options = parseArguments(args);
  if (options.showHelp) {
    if (!options.invalidOption.isEmpty()) {
      fprintf(stderr, "Invalid option: %s\n",
          options.invalidOption.toLocal8Bit().constData());
    }
    printUsage(QFileInfo(args.value(0, QStringLiteral("qtedm_image_compare"))).fileName());
    return options.invalidOption.isEmpty() ? 0 : 1;
  }

  if (options.expectedPath.isEmpty() || options.actualPath.isEmpty()) {
    printUsage(QFileInfo(args.value(0, QStringLiteral("qtedm_image_compare"))).fileName());
    return 1;
  }

  QString errorMessage;
  const QImage expected = loadImage(options.expectedPath, &errorMessage);
  if (expected.isNull()) {
    fprintf(stderr, "%s\n", errorMessage.toLocal8Bit().constData());
    return 1;
  }

  const QImage actual = loadImage(options.actualPath, &errorMessage);
  if (actual.isNull()) {
    fprintf(stderr, "%s\n", errorMessage.toLocal8Bit().constData());
    return 1;
  }

  if (expected.size() != actual.size()) {
    fprintf(stderr,
        "Image size mismatch: expected=%dx%d actual=%dx%d\n",
        expected.width(), expected.height(),
        actual.width(), actual.height());
    return 1;
  }

  QImage diffImage;
  if (!options.diffPath.isEmpty()) {
    diffImage = QImage(expected.size(), QImage::Format_ARGB32);
    diffImage.fill(Qt::transparent);
  }

  qint64 differentPixels = 0;
  std::uint64_t totalAbsoluteDelta = 0;
  int observedMaxChannelDelta = 0;

  for (int y = 0; y < expected.height(); ++y) {
    const QRgb *expectedLine =
        reinterpret_cast<const QRgb *>(expected.constScanLine(y));
    const QRgb *actualLine =
        reinterpret_cast<const QRgb *>(actual.constScanLine(y));
    QRgb *diffLine = diffImage.isNull()
        ? nullptr
        : reinterpret_cast<QRgb *>(diffImage.scanLine(y));

    for (int x = 0; x < expected.width(); ++x) {
      const int deltaRed = std::abs(qRed(expectedLine[x]) - qRed(actualLine[x]));
      const int deltaGreen =
          std::abs(qGreen(expectedLine[x]) - qGreen(actualLine[x]));
      const int deltaBlue =
          std::abs(qBlue(expectedLine[x]) - qBlue(actualLine[x]));
      const int deltaAlpha =
          std::abs(qAlpha(expectedLine[x]) - qAlpha(actualLine[x]));
      const int pixelMaxDelta = std::max(
          std::max(deltaRed, deltaGreen),
          std::max(deltaBlue, deltaAlpha));

      if (pixelMaxDelta > 0) {
        ++differentPixels;
      }
      observedMaxChannelDelta = std::max(observedMaxChannelDelta,
          pixelMaxDelta);
      totalAbsoluteDelta += static_cast<std::uint64_t>(deltaRed)
          + static_cast<std::uint64_t>(deltaGreen)
          + static_cast<std::uint64_t>(deltaBlue)
          + static_cast<std::uint64_t>(deltaAlpha);

      if (diffLine) {
        diffLine[x] = pixelMaxDelta > 0
            ? qRgba(pixelMaxDelta, 0, 0, 255)
            : qRgba(0, 0, 0, 0);
      }
    }
  }

  const double meanAbsoluteDelta = static_cast<double>(totalAbsoluteDelta)
      / static_cast<double>(expected.width() * expected.height() * 4);

  if (!diffImage.isNull() && differentPixels > 0) {
    if (!ensureParentDirectoryExists(options.diffPath) ||
        !diffImage.save(options.diffPath, "PNG")) {
      fprintf(stderr, "Failed to write diff image: %s\n",
          options.diffPath.toLocal8Bit().constData());
      return 1;
    }
  }

  fprintf(stdout,
      "different_pixels=%lld mean_absolute_delta=%.6f max_channel_delta=%d\n",
      static_cast<long long>(differentPixels), meanAbsoluteDelta,
      observedMaxChannelDelta);

  if (differentPixels > options.maxDifferentPixels ||
      meanAbsoluteDelta > options.maxMeanAbsoluteDelta ||
      observedMaxChannelDelta > options.maxChannelDelta) {
    return 1;
  }

  return 0;
}
