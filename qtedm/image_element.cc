#include "image_element.h"

#include <algorithm>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QStringList>
#include <QPalette>
#include <QPen>

namespace {
constexpr int kDefaultFrameIndex = 0;
}

ImageElement::ImageElement(QWidget *parent)
  : GraphicShapeElement(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground, true);
  reloadImage();
  update();
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
  reloadImage();
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
  reloadImage();
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
  reloadImage();
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

void ImageElement::setRuntimeAnimate(bool animate)
{
	const bool shouldAnimate = animate && frameCount() > 1;
	if (runtimeAnimate_ == shouldAnimate) {
		return;
	}
	runtimeAnimate_ = shouldAnimate;
	if (!movie_) {
		runtimeAnimate_ = false;
		return;
	}

	if (runtimeAnimate_) {
		if (movie_->state() != QMovie::Running) {
			movie_->start();
		}
		movie_->setPaused(false);
	} else {
		movie_->setPaused(true);
		movie_->jumpToFrame(runtimeFrameIndex_);
		updateCurrentPixmap();
	}
	update();
}

void ImageElement::setRuntimeFrameIndex(int index)
{
	const int count = frameCount();
	if (count <= 0) {
		runtimeFrameIndex_ = kDefaultFrameIndex;
		return;
	}

	const int clamped = std::clamp(index, 0, count - 1);
	if (runtimeFrameIndex_ == clamped && (!movie_ || !runtimeAnimate_)) {
		return;
	}

	runtimeFrameIndex_ = clamped;
	if (movie_) {
		if (!runtimeAnimate_) {
			movie_->setPaused(true);
			movie_->jumpToFrame(runtimeFrameIndex_);
			updateCurrentPixmap();
		}
	}
	update();
}

void ImageElement::setRuntimeFrameValid(bool valid)
{
	if (runtimeFrameValid_ == valid) {
		return;
	}
	runtimeFrameValid_ = valid;
	if (!valid) {
		setRuntimeAnimate(false);
	}
	update();
}

void ImageElement::onRuntimeStateReset()
{
  runtimeAnimate_ = false;
  runtimeFrameValid_ = !pixmap_.isNull();
  runtimeFrameIndex_ = kDefaultFrameIndex;
  if (movie_) {
    movie_->setPaused(true);
    movie_->jumpToFrame(runtimeFrameIndex_);
    updateCurrentPixmap();
  }
}

void ImageElement::onRuntimeConnectedChanged()
{
  if (!runtimeConnected_) {
    setRuntimeAnimate(false);
  }
}

void ImageElement::onRuntimeSeverityChanged()
{
  if (isExecuteMode()) {
    onExecuteStateApplied();
  }
}

short ImageElement::normalizeRuntimeSeverity(short severity) const
{
  if (severity < 0) {
    return 0;
  }
  return severity;
}

int ImageElement::frameCount() const
{
	if (movie_) {
		int count = movie_->frameCount();
		if (count <= 0) {
			count = cachedFrameCount_;
		}
		if (count <= 0) {
			count = 1;
		}
		return count;
	}
	if (!pixmap_.isNull()) {
		return 1;
	}
	return 0;
}

void ImageElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  const QRect drawRect = rect().adjusted(0, 0, -1, -1);

  const bool showImage = !pixmap_.isNull()
      && (!executeMode_ || (runtimeConnected_ && runtimeFrameValid_));

  if (showImage) {
    painter.drawPixmap(drawRect, pixmap_);
  } else {
    const QColor bgColor = (executeMode_ && !runtimeConnected_)
        ? QColor(255, 255, 255)
        : backgroundColor();
    painter.fillRect(drawRect, bgColor);
    painter.setPen(QPen(foregroundColor(), 1, Qt::DashLine));
    painter.drawRect(drawRect);
    painter.setPen(QPen(foregroundColor(), 1, Qt::SolidLine));
    painter.drawLine(drawRect.topLeft(), drawRect.bottomRight());
    painter.drawLine(drawRect.topRight(), drawRect.bottomLeft());
  }

  if (isSelected()) {
    drawSelectionOutline(painter, drawRect);
  }
}

void ImageElement::reloadImage()
{
	disposeMovie();
	pixmap_ = QPixmap();
	cachedFrameCount_ = 0;
	runtimeFrameIndex_ = kDefaultFrameIndex;
	runtimeFrameValid_ = false;

	if (imageType_ == ImageType::kNone) {
		return;
	}

	const QString trimmedName = imageName_.trimmed();
	if (trimmedName.isEmpty()) {
		return;
	}

  const QFileInfo directInfo(trimmedName);

  auto tryLoadMovie = [this](const QString &path) {
    if (path.isEmpty()) {
      return false;
    }
    auto *movie = new QMovie(path, QByteArray(), this);
    if (!movie->isValid()) {
      delete movie;
      return false;
    }
    movie->setCacheMode(QMovie::CacheAll);
    movie_ = movie;
    movie_->jumpToFrame(kDefaultFrameIndex);
    cachedFrameCount_ = movie_->frameCount();
    if (cachedFrameCount_ <= 0) {
      cachedFrameCount_ = 1;
    }
    QObject::connect(movie_, &QMovie::frameChanged, this,
        [this](int frame) {
          if (frame >= 0) {
            runtimeFrameIndex_ = frame;
          }
          updateCurrentPixmap();
        });
    movie_->setPaused(true);
    updateCurrentPixmap();
    runtimeFrameValid_ = !pixmap_.isNull();
    return true;
  };

  auto tryLoadPixmap = [this](const QString &path) {
    if (path.isEmpty()) {
      return false;
    }
    QPixmap pixmap(path);
    if (pixmap.isNull()) {
      return false;
    }
    pixmap_ = pixmap;
    cachedFrameCount_ = 1;
    runtimeFrameIndex_ = kDefaultFrameIndex;
    runtimeFrameValid_ = true;
    return true;
  };

  QStringList candidatePaths;
  if (directInfo.isAbsolute()) {
    candidatePaths.push_back(directInfo.filePath());
  } else {
    candidatePaths.push_back(trimmedName);
    if (!baseDirectory_.isEmpty()) {
      candidatePaths.push_back(QDir(baseDirectory_).absoluteFilePath(trimmedName));
    }
    const QByteArray env = qgetenv("EPICS_DISPLAY_PATH");
    if (!env.isEmpty()) {
      const QChar separator = QDir::listSeparator();
      const QStringList parts = QString::fromLocal8Bit(env).split(
          separator, Qt::SkipEmptyParts);
      for (const QString &part : parts) {
        const QString trimmedDir = part.trimmed();
        if (!trimmedDir.isEmpty()) {
          candidatePaths.push_back(QDir(trimmedDir).absoluteFilePath(trimmedName));
        }
      }
    }
  }

  bool loaded = false;
  for (const QString &candidate : candidatePaths) {
    if (candidate.isEmpty()) {
      continue;
    }
    if (imageType_ == ImageType::kGif) {
      loaded = tryLoadMovie(candidate);
      if (!loaded) {
        loaded = tryLoadPixmap(candidate);
      }
    } else {
      loaded = tryLoadPixmap(candidate);
    }
    if (loaded) {
      break;
    }
  }

	if (!loaded) {
		disposeMovie();
		pixmap_ = QPixmap();
		runtimeFrameValid_ = false;
		cachedFrameCount_ = 0;
	}
}

void ImageElement::disposeMovie()
{
	if (!movie_) {
		return;
	}
	disconnect(movie_, nullptr, this, nullptr);
	movie_->stop();
	delete movie_;
	movie_ = nullptr;
}

void ImageElement::updateCurrentPixmap()
{
	if (!movie_) {
		return;
	}
	const QPixmap frame = movie_->currentPixmap();
	if (!frame.isNull()) {
		pixmap_ = frame;
		runtimeFrameValid_ = true;
	}
	update();
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
