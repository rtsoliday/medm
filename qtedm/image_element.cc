#include "image_element.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPalette>
#include <QPen>

ImageElement::ImageElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  loadPixmap();
  update();
}

void ImageElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ImageElement::isSelected() const
{
  return selected_;
}

ImageType ImageElement::imageType() const
{
  return imageType_;
}

void ImageElement::setImageType(ImageType type)
{
  if (imageType_ == type) {
    return;
  }
  imageType_ = type;
  loadPixmap();
  update();
}

QString ImageElement::imageName() const
{
  return imageName_;
}

void ImageElement::setImageName(const QString &name)
{
  if (imageName_ == name) {
    return;
  }
  imageName_ = name;
  if (imageName_.isEmpty()) {
    setToolTip(QString());
  } else {
    setToolTip(imageName_);
  }
  loadPixmap();
  update();
}

QString ImageElement::baseDirectory() const
{
  return baseDirectory_;
}

void ImageElement::setBaseDirectory(const QString &directory)
{
  QString normalized = directory.trimmed();
  if (!normalized.isEmpty()) {
    normalized = QDir(normalized).absolutePath();
  }
  if (baseDirectory_ == normalized) {
    return;
  }
  baseDirectory_ = normalized;
  loadPixmap();
  update();
}

QString ImageElement::calc() const
{
  return calc_;
}

void ImageElement::setCalc(const QString &calc)
{
  if (calc_ == calc) {
    return;
  }
  calc_ = calc;
}

TextColorMode ImageElement::colorMode() const
{
  return colorMode_;
}

void ImageElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode ImageElement::visibilityMode() const
{
  return visibilityMode_;
}

void ImageElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString ImageElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void ImageElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString ImageElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void ImageElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void ImageElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  const QRect drawRect = rect().adjusted(0, 0, -1, -1);

  if (!pixmap_.isNull()) {
    painter.drawPixmap(drawRect, pixmap_);
  } else {
    painter.fillRect(drawRect, backgroundColor());
    painter.setPen(QPen(foregroundColor(), 1, Qt::DashLine));
    painter.drawRect(drawRect);
    painter.setPen(QPen(foregroundColor(), 1, Qt::SolidLine));
    painter.drawLine(drawRect.topLeft(), drawRect.bottomRight());
    painter.drawLine(drawRect.topRight(), drawRect.bottomLeft());
  }

  if (selected_) {
    QPen pen(Qt::black);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(drawRect);
  }
}

void ImageElement::loadPixmap()
{
  pixmap_ = QPixmap();
  if (imageType_ == ImageType::kNone) {
    return;
  }

  const QString trimmedName = imageName_.trimmed();
  if (trimmedName.isEmpty()) {
    return;
  }

  const QFileInfo directInfo(trimmedName);
  auto tryLoad = [this](const QString &path) {
    if (path.isEmpty()) {
      return false;
    }
    QPixmap pixmap(path);
    if (pixmap.isNull()) {
      return false;
    }
    pixmap_ = pixmap;
    return true;
  };

  if (directInfo.isAbsolute()) {
    if (tryLoad(directInfo.filePath())) {
      return;
    }
  } else {
    if (!baseDirectory_.isEmpty()) {
      if (tryLoad(QDir(baseDirectory_).absoluteFilePath(trimmedName))) {
        return;
      }
    }
    if (tryLoad(trimmedName)) {
      return;
    }
  }
}

QColor ImageElement::foregroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return Qt::black;
}

QColor ImageElement::backgroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return Qt::white;
}
