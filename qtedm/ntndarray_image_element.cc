#include "ntndarray_image_element.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kMaximumZoom = 128.0;
constexpr int kOverlayMargin = 5;

QString frameTimestamp(const NtNdArrayFrame &frame)
{
  if (frame.secondsPastEpoch <= 0) {
    return QString();
  }
  const qint64 milliseconds = frame.secondsPastEpoch * 1000
      + frame.nanoseconds / 1000000;
  return QDateTime::fromMSecsSinceEpoch(milliseconds).toUTC()
      .toString(Qt::ISODateWithMs);
}

} // namespace

NtNdArrayImageElement::NtNdArrayImageElement(QWidget *parent)
  : HeatmapElement(parent)
{
  setMouseTracking(true);
  setPreserveAspectRatio(true);
  setRangeMaximum(255.0);
  setTitle(QStringLiteral("PVA NTNDArray"));
}

bool NtNdArrayImageElement::showPixelProbe() const
{
  return showPixelProbe_;
}

void NtNdArrayImageElement::setShowPixelProbe(bool show)
{
  if (showPixelProbe_ == show) {
    return;
  }
  showPixelProbe_ = show;
  if (!show) {
    probePixel_ = QPoint(-1, -1);
    probeText_.clear();
  }
  update();
}

quint64 NtNdArrayImageElement::maximumInputBytes() const
{
  return maximumInputBytes_;
}

void NtNdArrayImageElement::setMaximumInputBytes(quint64 bytes)
{
  if (bytes > 0) {
    maximumInputBytes_ = bytes;
  }
}

quint64 NtNdArrayImageElement::maximumOutputBytes() const
{
  return maximumOutputBytes_;
}

void NtNdArrayImageElement::setMaximumOutputBytes(quint64 bytes)
{
  if (bytes > 0) {
    maximumOutputBytes_ = bytes;
  }
}

int NtNdArrayImageElement::maximumDimension() const
{
  return maximumDimension_;
}

void NtNdArrayImageElement::setMaximumDimension(int dimension)
{
  if (dimension > 0) {
    maximumDimension_ = dimension;
  }
}

NtNdArrayDecodeOptions NtNdArrayImageElement::decodeOptions() const
{
  NtNdArrayDecodeOptions options;
  options.colorMap = colorMap();
  options.rangeMode = rangeMode();
  options.rangeMinimum = rangeMinimum();
  options.rangeMaximum = rangeMaximum();
  options.flipHorizontal = flipHorizontal();
  options.flipVertical = flipVertical();
  options.rotation = rotation();
  options.maximumDimension = maximumDimension_;
  options.maximumInputBytes = maximumInputBytes_;
  options.maximumOutputBytes = maximumOutputBytes_;
  return options;
}

void NtNdArrayImageElement::setDecodedFrame(
    const NtNdArrayDecodedFrame &frame)
{
  frame_ = frame;
  lastError_ = frame.error;
  if (frame_.valid) {
    setRuntimeConnected(true);
  }
  update();
}

void NtNdArrayImageElement::setStreamStatus(bool connected,
    quint64 droppedFrames, const QString &error)
{
  connected_ = connected;
  droppedFrames_ = droppedFrames;
  if (!error.trimmed().isEmpty()) {
    lastError_ = error;
  } else if (connected && frame_.valid) {
    lastError_.clear();
  }
  setRuntimeConnected(connected);
  setRuntimeSeverity(connected ? 0 : 3);
  update();
}

void NtNdArrayImageElement::clearNtNdArrayState()
{
  frame_ = NtNdArrayDecodedFrame();
  connected_ = false;
  droppedFrames_ = 0;
  lastError_.clear();
  probePixel_ = QPoint(-1, -1);
  probeText_.clear();
  setRuntimeConnected(false);
  setRuntimeSeverity(3);
  resetImageView();
}

const NtNdArrayDecodedFrame &NtNdArrayImageElement::decodedFrame() const
{
  return frame_;
}

quint64 NtNdArrayImageElement::droppedFrames() const
{
  return droppedFrames_;
}

QString NtNdArrayImageElement::lastError() const
{
  return lastError_;
}

bool NtNdArrayImageElement::streamConnected() const
{
  return connected_;
}

bool NtNdArrayImageElement::isImageZoomed() const
{
  return zoom_ > 1.0001;
}

void NtNdArrayImageElement::resetImageView()
{
  zoom_ = 1.0;
  viewCenter_ = QPointF(0.5, 0.5);
  update();
}

QRect NtNdArrayImageElement::imageDrawRect() const
{
  QRect available = rect().adjusted(2, 2, -2, -2);
  if (available.isEmpty() || frame_.image.isNull()
      || !preserveAspectRatio()) {
    return available;
  }
  QSize scaled = frame_.image.size();
  scaled.scale(available.size(), Qt::KeepAspectRatio);
  return QRect(
      available.left() + (available.width() - scaled.width()) / 2,
      available.top() + (available.height() - scaled.height()) / 2,
      scaled.width(), scaled.height());
}

QRectF NtNdArrayImageElement::sourceViewRect() const
{
  if (frame_.image.isNull()) {
    return QRectF();
  }
  const double viewWidth = frame_.image.width() / zoom_;
  const double viewHeight = frame_.image.height() / zoom_;
  const double centerX = viewCenter_.x() * frame_.image.width();
  const double centerY = viewCenter_.y() * frame_.image.height();
  double left = centerX - viewWidth / 2.0;
  double top = centerY - viewHeight / 2.0;
  left = std::clamp(left, 0.0,
      std::max(0.0, frame_.image.width() - viewWidth));
  top = std::clamp(top, 0.0,
      std::max(0.0, frame_.image.height() - viewHeight));
  return QRectF(left, top, viewWidth, viewHeight);
}

void NtNdArrayImageElement::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);
  painter.fillRect(rect(), QColor(28, 28, 28));
  lastDrawRect_ = imageDrawRect();

  if (frame_.valid && !frame_.image.isNull() && !lastDrawRect_.isEmpty()) {
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(lastDrawRect_, frame_.image, sourceViewRect());
  } else {
    painter.setPen(QPen(connected_ ? QColor(255, 190, 50)
                                  : QColor(255, 85, 85), 1, Qt::DashLine));
    painter.drawRect(rect().adjusted(2, 2, -3, -3));
    painter.drawLine(rect().topLeft() + QPoint(3, 3),
        rect().bottomRight() - QPoint(3, 3));
    painter.drawLine(rect().topRight() + QPoint(-3, 3),
        rect().bottomLeft() + QPoint(3, -3));
  }

  QStringList status;
  if (frame_.valid) {
    status << QStringLiteral("%1x%2")
                  .arg(frame_.image.width()).arg(frame_.image.height())
           << QStringLiteral("ID %1").arg(frame_.source.uniqueId);
    const QString timestamp = frameTimestamp(frame_.source);
    if (!timestamp.isEmpty()) {
      status << timestamp;
    }
  }
  if (droppedFrames_ > 0) {
    status << QStringLiteral("dropped %1").arg(droppedFrames_);
  }
  if (!connected_) {
    status << QStringLiteral("DISCONNECTED");
  }
  if (!lastError_.isEmpty()) {
    status << lastError_;
  }

  const QFontMetrics metrics(font());
  const QString statusText = status.join(QStringLiteral("  "));
  if (!statusText.isEmpty()) {
    const QRect textRect = metrics.boundingRect(statusText)
        .adjusted(-kOverlayMargin, -2, kOverlayMargin, 2)
        .translated(5, 5);
    painter.fillRect(textRect, QColor(0, 0, 0, 170));
    painter.setPen(Qt::white);
    painter.drawText(textRect.adjusted(kOverlayMargin, 0,
        -kOverlayMargin, 0), Qt::AlignVCenter, statusText);
  }

  if (showPixelProbe_ && !probeText_.isEmpty()) {
    QRect probeRect = metrics.boundingRect(probeText_)
        .adjusted(-kOverlayMargin, -2, kOverlayMargin, 2);
    probeRect.moveBottomRight(
        rect().bottomRight() - QPoint(kOverlayMargin, kOverlayMargin));
    painter.fillRect(probeRect, QColor(0, 0, 0, 190));
    painter.setPen(Qt::white);
    painter.drawText(probeRect.adjusted(kOverlayMargin, 0,
        -kOverlayMargin, 0), Qt::AlignVCenter, probeText_);
  }
  drawSelectionOutline(painter, rect().adjusted(0, 0, -1, -1));
}

void NtNdArrayImageElement::onExecuteStateApplied()
{
  HeatmapElement::onExecuteStateApplied();
  setMouseTracking(isExecuteMode());
  if (!isExecuteMode()) {
    panning_ = false;
    probePixel_ = QPoint(-1, -1);
    probeText_.clear();
  }
}

bool NtNdArrayImageElement::mapWidgetToImage(
    const QPointF &position, QPoint *pixel) const
{
  if (!pixel || !frame_.valid || frame_.image.isNull()
      || !lastDrawRect_.contains(position.toPoint())) {
    return false;
  }
  const QRectF source = sourceViewRect();
  const double nx = (position.x() - lastDrawRect_.left())
      / std::max(1, lastDrawRect_.width());
  const double ny = (position.y() - lastDrawRect_.top())
      / std::max(1, lastDrawRect_.height());
  const int x = std::clamp(static_cast<int>(
      source.left() + nx * source.width()), 0, frame_.image.width() - 1);
  const int y = std::clamp(static_cast<int>(
      source.top() + ny * source.height()), 0, frame_.image.height() - 1);
  *pixel = QPoint(x, y);
  return true;
}

void NtNdArrayImageElement::updateProbe(const QPointF &position)
{
  if (!showPixelProbe_) {
    return;
  }
  QPoint pixel;
  if (!mapWidgetToImage(position, &pixel)) {
    probePixel_ = QPoint(-1, -1);
    probeText_.clear();
    update();
    return;
  }
  probePixel_ = pixel;
  const QVector<double> values =
      NtNdArrayImageDecoder::pixelValues(frame_, pixel.x(), pixel.y());
  QStringList valueStrings;
  for (double value : values) {
    valueStrings << QString::number(value, 'g', 8);
  }
  probeText_ = QStringLiteral("(%1, %2)  %3")
      .arg(pixel.x()).arg(pixel.y())
      .arg(valueStrings.join(QStringLiteral(", ")));
  update();
}

void NtNdArrayImageElement::mousePressEvent(QMouseEvent *event)
{
  if (!isExecuteMode()) {
    HeatmapElement::mousePressEvent(event);
    return;
  }
  const QPointF position =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position();
#else
      event->localPos();
#endif
  if (event->button() == Qt::LeftButton && isImageZoomed()
      && lastDrawRect_.contains(position.toPoint())) {
    panning_ = true;
    panStart_ = position;
    panCenterStart_ = viewCenter_;
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  HeatmapElement::mousePressEvent(event);
}

void NtNdArrayImageElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (isExecuteMode() && panning_ && event->button() == Qt::LeftButton) {
    panning_ = false;
    unsetCursor();
    event->accept();
    return;
  }
  HeatmapElement::mouseReleaseEvent(event);
}

void NtNdArrayImageElement::panBy(const QPointF &delta)
{
  if (lastDrawRect_.width() <= 0 || lastDrawRect_.height() <= 0) {
    return;
  }
  viewCenter_.setX(std::clamp(panCenterStart_.x()
      - delta.x() / lastDrawRect_.width() / zoom_, 0.0, 1.0));
  viewCenter_.setY(std::clamp(panCenterStart_.y()
      - delta.y() / lastDrawRect_.height() / zoom_, 0.0, 1.0));
  update();
}

void NtNdArrayImageElement::mouseMoveEvent(QMouseEvent *event)
{
  if (!isExecuteMode()) {
    HeatmapElement::mouseMoveEvent(event);
    return;
  }
  const QPointF position =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->position();
#else
      event->localPos();
#endif
  if (panning_) {
    panBy(position - panStart_);
    event->accept();
    return;
  }
  updateProbe(position);
  HeatmapElement::mouseMoveEvent(event);
}

void NtNdArrayImageElement::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (isExecuteMode() && event->button() == Qt::LeftButton) {
    resetImageView();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

void NtNdArrayImageElement::wheelEvent(QWheelEvent *event)
{
  const QPointF position =
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
      event->position();
#else
      event->posF();
#endif
  if (!isExecuteMode() || frame_.image.isNull()
      || !lastDrawRect_.contains(position.toPoint())) {
    HeatmapElement::wheelEvent(event);
    return;
  }
  const QRectF oldSource = sourceViewRect();
  const double previousZoom = zoom_;
  const double factor = std::pow(1.0015, event->angleDelta().y());
  zoom_ = std::clamp(zoom_ * factor, 1.0, kMaximumZoom);
  if (qFuzzyCompare(previousZoom, zoom_)) {
    return;
  }

  const double nx = (position.x() - lastDrawRect_.left())
      / std::max(1, lastDrawRect_.width());
  const double ny = (position.y() - lastDrawRect_.top())
      / std::max(1, lastDrawRect_.height());
  const QPointF anchor(oldSource.left() + nx * oldSource.width(),
      oldSource.top() + ny * oldSource.height());
  const double newWidth = frame_.image.width() / zoom_;
  const double newHeight = frame_.image.height() / zoom_;
  const double left = anchor.x() - nx * newWidth;
  const double top = anchor.y() - ny * newHeight;
  viewCenter_ = QPointF(
      (left + newWidth / 2.0) / frame_.image.width(),
      (top + newHeight / 2.0) / frame_.image.height());
  viewCenter_.setX(std::clamp(viewCenter_.x(), 0.0, 1.0));
  viewCenter_.setY(std::clamp(viewCenter_.y(), 0.0, 1.0));
  updateProbe(position);
  event->accept();
}

void NtNdArrayImageElement::leaveEvent(QEvent *event)
{
  if (isExecuteMode()) {
    probePixel_ = QPoint(-1, -1);
    probeText_.clear();
    update();
  }
  QWidget::leaveEvent(event);
}
