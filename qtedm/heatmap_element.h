#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

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

  void setRuntimeData(const QVector<double> &values);
  void setRuntimeDimensions(int xDim, int yDim);
  void clearRuntimeState();

protected:
  void paintEvent(QPaintEvent *event) override;
  void onRuntimeStateReset() override;
  void onRuntimeConnectedChanged() override;
  void onRuntimeSeverityChanged() override;

private:
  void invalidateCache();
  void rebuildImage();
  QSize effectiveDimensions() const;
  QColor backgroundColor() const;
  QColor borderColor() const;

  QString dataChannel_;
  QString title_;
  HeatmapDimensionSource xDimensionSource_ = HeatmapDimensionSource::kStatic;
  HeatmapDimensionSource yDimensionSource_ = HeatmapDimensionSource::kStatic;
  int xDimension_ = 10;
  int yDimension_ = 10;
  QString xDimensionChannel_;
  QString yDimensionChannel_;
  HeatmapOrder order_ = HeatmapOrder::kRowMajor;

  QVector<double> runtimeValues_;
  int runtimeXDimension_ = 0;
  int runtimeYDimension_ = 0;
  bool runtimeDimensionsValid_ = false;
  bool runtimeDataValid_ = false;
  bool runtimeRangeValid_ = false;
  double runtimeMinValue_ = 0.0;
  double runtimeMaxValue_ = 0.0;

  QImage cachedImage_;
  bool cacheValid_ = false;
};
