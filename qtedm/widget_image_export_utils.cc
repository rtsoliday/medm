#include "widget_image_export_utils.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSvgGenerator>
#include <QWidget>

namespace {

void setError(QString *errorMessage, const QString &message)
{
  if (errorMessage) {
    *errorMessage = message;
  }
}

} // namespace

bool renderWidgetImageToPath(QWidget *widget, const QString &filePath,
    const QString &title, const QString &description, QString *errorMessage)
{
  if (!widget) {
    setError(errorMessage, QStringLiteral("No widget was provided to render."));
    return false;
  }
  if (filePath.trimmed().isEmpty()) {
    setError(errorMessage, QStringLiteral("The image output path is empty."));
    return false;
  }
  if (widget->width() <= 0 || widget->height() <= 0) {
    setError(errorMessage, QStringLiteral("The widget has no renderable area."));
    return false;
  }

  const QString normalized = QFileInfo(filePath).absoluteFilePath();
  const QFileInfo outputInfo(normalized);
  const QString suffix = outputInfo.suffix().toLower();
  if (suffix != QLatin1String("png") && suffix != QLatin1String("jpg")
      && suffix != QLatin1String("jpeg") && suffix != QLatin1String("bmp")
      && suffix != QLatin1String("svg")) {
    setError(errorMessage,
        QStringLiteral("Unsupported image extension for %1.").arg(normalized));
    return false;
  }

  const QDir parent = outputInfo.absoluteDir();
  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    setError(errorMessage,
        QStringLiteral("Failed to create image directory %1.")
            .arg(parent.absolutePath()));
    return false;
  }

  if (suffix == QLatin1String("svg")) {
    QSvgGenerator generator;
    generator.setFileName(normalized);
    generator.setSize(widget->size());
    generator.setViewBox(QRect(QPoint(0, 0), widget->size()));
    generator.setTitle(title);
    generator.setDescription(description);

    QPainter painter;
    if (!painter.begin(&generator)) {
      setError(errorMessage,
          QStringLiteral("Failed to open %1 for SVG rendering.")
              .arg(normalized));
      return false;
    }
    widget->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();
    return true;
  }

  QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    setError(errorMessage,
        QStringLiteral("Failed to allocate a %1 by %2 image.")
            .arg(widget->width()).arg(widget->height()));
    return false;
  }
  image.fill(Qt::transparent);
  QPainter painter(&image);
  widget->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
  painter.end();

  const char *format = "PNG";
  if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
    format = "JPEG";
  } else if (suffix == QLatin1String("bmp")) {
    format = "BMP";
  }
  if (!image.save(normalized, format)) {
    setError(errorMessage,
        QStringLiteral("Failed to write rendered image to %1.")
            .arg(normalized));
    return false;
  }
  return true;
}
