#include <memory>
#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>

#include "display_properties.h"
#include "graphic_shape_element.h"

class HeatmapElement : public GraphicShapeElement
{
public:
  explicit HeatmapElement(QWidget *parent = nullptr);

  QString dataChannel() const;
  void setDataChannel(const QString &channel);

  QString title() const;
  void setTitle(const QString &title);

  HeatmapDimensionSource xDimensionSource() const;
  void setXDimensionSource(HeatmapDimensionSource source);

  HeatmapDimensionSource yDimensionSource() const;
  void setYDimensionSource(HeatmapDimensionSource source);

  int xDimension() const;
  void setXDimension(int value);

  int yDimension() const;
  void setYDimension(int value);

  QString xDimensionChannel() const;
  void setXDimensionChannel(const QString &channel);

  QString yDimensionChannel() const;
  void setYDimensionChannel(const QString &channel);

  HeatmapOrder order() const;
  void setOrder(HeatmapOrder order);

  HeatmapColorMap colorMap() const;
  void setColorMap(HeatmapColorMap colorMap);

  HeatmapProfileMode profileMode() const;
  void setProfileMode(HeatmapProfileMode mode);

  bool showTopProfile() const;
  void setShowTopProfile(bool show);

  bool showRightProfile() const;
  void setShowRightProfile(bool show);

  bool preserveAspectRatio() const;
  void setPreserveAspectRatio(bool preserve);

  bool flipHorizontal() const;
  void setFlipHorizontal(bool flip);

  bool flipVertical() const;
  void setFlipVertical(bool flip);

  HeatmapRotation rotation() const;
  void setRotation(HeatmapRotation rotation);

  bool invertGreyscale() const;
  void setInvertGreyscale(bool invert);

  void setRuntimeData(const QVector<double> &values);
  void setRuntimeSharedData(std::shared_ptr<const double> sharedData, size_t size);
  void setRuntimeDimensions(int xDim, int yDim);
  void clearRuntimeState();

  // Zoom/pan support (execute mode only)
  bool isZoomed() const;
  void resetZoom();


  // Coordinate mappers
  void mapVisualToDataFraction(double& x, double& y) const;
  void mapVisualToDataDelta(double& dx, double& dy) const;
  void mapVisualToDataZoomFlags(bool& zoomX, bool& zoomY) const;

protected:

  void paintEvent(QPaintEvent *event) override;
  void onRuntimeStateReset() override;
  void onRuntimeConnectedChanged() override;
  void onRuntimeSeverityChanged() override;
  void onExecuteStateApplied() override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  bool event(QEvent *event) override;

private:
  void requestVisualUpdate(bool immediate = false);
  void invalidateCache(bool immediate = false);
  void invalidatePaletteCache();
  void rebuildImage(const QSize &targetSize);
  QImage maxPoolDownsample(const QImage &source,
      const QSize &targetSize) const;
  QSize effectiveDimensions() const;
  QSize visibleDataSize() const;
  QSize renderTargetSize(const QSize &availableSize) const;
  QColor backgroundColor() const;
  QColor borderColor() const;
  const QVector<QRgb> &cachedPalette();
  const QImage &legendBarImage(const QSize &size);

  QString dataChannel_;
  QString title_;
  HeatmapDimensionSource xDimensionSource_ = HeatmapDimensionSource::kStatic;
  HeatmapDimensionSource yDimensionSource_ = HeatmapDimensionSource::kStatic;
  int xDimension_ = 10;
  int yDimension_ = 10;
  QString xDimensionChannel_;
  QString yDimensionChannel_;
  HeatmapOrder order_ = HeatmapOrder::kRowMajor;
  HeatmapColorMap colorMap_ = HeatmapColorMap::kGrayscale;
  bool invertGreyscale_ = true;
  bool preserveAspectRatio_ = false;
  bool flipHorizontal_ = false;
  bool flipVertical_ = false;
  HeatmapRotation rotation_ = HeatmapRotation::kNone;
  HeatmapProfileMode profileMode_ = HeatmapProfileMode::kAbsolute;
  QVector<double> topProfileData_;
  double topProfileMin_ = 0.0;
  double topProfileMax_ = 0.0;
  QVector<double> rightProfileData_;
  double rightProfileMin_ = 0.0;
  double rightProfileMax_ = 0.0;
  bool showTopProfile_ = false;
  bool showRightProfile_ = false;

  QVector<double> runtimeValues_;
  std::shared_ptr<const double> runtimeSharedValues_;
  size_t runtimeSharedSize_ = 0;
  int runtimeXDimension_ = 0;
  int runtimeYDimension_ = 0;
  bool runtimeDimensionsValid_ = false;
  bool runtimeDataValid_ = false;
  bool runtimeRangeValid_ = false;
  double runtimeMinValue_ = 0.0;
  double runtimeMaxValue_ = 0.0;

  QImage cachedImage_;
  bool cacheValid_ = false;
  QSize cachedRenderSize_;
  QVector<QRgb> cachedPalette_;
  bool paletteCacheValid_ = false;
  QImage cachedLegendBarImage_;
  QSize cachedLegendBarSize_;
  QImage downsampledCachedImage_;
  QSize downsampledTargetSize_;

  bool forwardMouseEventToParent(QMouseEvent *event) const;

  // Zoom/pan state (execute mode only)
  bool zoomed_ = false;
  double zoomXMin_ = 0.0;
  double zoomXMax_ = 1.0;
  double zoomYMin_ = 0.0;
  double zoomYMax_ = 1.0;

  bool panning_ = false;
  QPointF panStartPos_;
  double panStartXMin_ = 0.0;
  double panStartXMax_ = 1.0;
  double panStartYMin_ = 0.0;
  double panStartYMax_ = 1.0;

  QRectF lastHeatmapRect_;
  QTimer interactionTimer_;
};
