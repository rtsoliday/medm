#pragma once

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "heatmap_element.h"
#include "ntndarray_image_decoder.h"

class NtNdArrayImageElement : public HeatmapElement
{
public:
  explicit NtNdArrayImageElement(QWidget *parent = nullptr);

  bool showPixelProbe() const;
  void setShowPixelProbe(bool show);

  quint64 maximumInputBytes() const;
  void setMaximumInputBytes(quint64 bytes);

  quint64 maximumOutputBytes() const;
  void setMaximumOutputBytes(quint64 bytes);

  int maximumDimension() const;
  void setMaximumDimension(int dimension);

  NtNdArrayDecodeOptions decodeOptions() const;

  void setDecodedFrame(const NtNdArrayDecodedFrame &frame);
  void setStreamStatus(bool connected, quint64 droppedFrames,
      const QString &error);
  void clearNtNdArrayState();

  const NtNdArrayDecodedFrame &decodedFrame() const;
  quint64 droppedFrames() const;
  QString lastError() const;
  bool streamConnected() const;

  bool isImageZoomed() const;
  void resetImageView();

protected:
  void paintEvent(QPaintEvent *event) override;
  void onExecuteStateApplied() override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QRect imageDrawRect() const;
  QRectF sourceViewRect() const;
  bool mapWidgetToImage(const QPointF &position, QPoint *pixel) const;
  void updateProbe(const QPointF &position);
  void panBy(const QPointF &delta);

  bool showPixelProbe_ = true;
  quint64 maximumInputBytes_ = 512ULL * 1024ULL * 1024ULL;
  quint64 maximumOutputBytes_ = 256ULL * 1024ULL * 1024ULL;
  int maximumDimension_ = 16384;

  NtNdArrayDecodedFrame frame_;
  bool connected_ = false;
  quint64 droppedFrames_ = 0;
  QString lastError_;

  double zoom_ = 1.0;
  QPointF viewCenter_ = QPointF(0.5, 0.5);
  bool panning_ = false;
  QPointF panStart_;
  QPointF panCenterStart_;
  QPoint probePixel_ = QPoint(-1, -1);
  QString probeText_;
  mutable QRect lastDrawRect_;
};
