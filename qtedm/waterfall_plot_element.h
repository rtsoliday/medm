#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

#include "heatmap_properties.h"
#include "time_units.h"
#include "waterfall_plot_properties.h"

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QWheelEvent;

class WaterfallPlotElement : public QWidget
{
public:
  explicit WaterfallPlotElement(QWidget *parent = nullptr);

  QSize sizeHint() const override;

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  QString title() const;
  void setTitle(const QString &title);

  QString xLabel() const;
  void setXLabel(const QString &label);

  QString yLabel() const;
  void setYLabel(const QString &label);

  QString dataChannel() const;
  void setDataChannel(const QString &channel);

  QString countChannel() const;
  void setCountChannel(const QString &channel);

  QString triggerChannel() const;
  void setTriggerChannel(const QString &channel);

  QString eraseChannel() const;
  void setEraseChannel(const QString &channel);

  WaterfallEraseMode eraseMode() const;
  void setEraseMode(WaterfallEraseMode mode);

  int historyCount() const;
  void setHistoryCount(int count);

  WaterfallScrollDirection scrollDirection() const;
  void setScrollDirection(WaterfallScrollDirection direction);

  HeatmapColorMap colorMap() const;
  void setColorMap(HeatmapColorMap map);

  bool invertGreyscale() const;
  void setInvertGreyscale(bool invert);

  WaterfallIntensityScale intensityScale() const;
  void setIntensityScale(WaterfallIntensityScale scale);

  double intensityMin() const;
  void setIntensityMin(double value);

  double intensityMax() const;
  void setIntensityMax(double value);

  bool showLegend() const;
  void setShowLegend(bool show);

  bool showGrid() const;
  void setShowGrid(bool show);

  double samplePeriod() const;
  void setSamplePeriod(double period);

  TimeUnits units() const;
  void setUnits(TimeUnits units);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  bool isRuntimeConnected() const;

  void setRuntimeWaveformLength(int length);
  int runtimeWaveformLength() const;

  void pushWaveform(const double *values, int count, qint64 timestampMs = 0,
      bool requestUpdate = true);
  void clearBuffer(bool requestUpdate = true);

  bool isZoomed() const;
  void resetZoom();

  int bufferedSampleCount() const;
  int waveformLength() const;
  int sampleLength(int sampleIndex) const;
  qint64 sampleTimestampMs(int sampleIndex) const;
  double sampleValue(int sampleIndex, int pointIndex) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  bool event(QEvent *event) override;

private:
  struct IntensityRange
  {
    bool valid = false;
    double minimum = 0.0;
    double maximum = 1.0;
  };

  struct PlotLayout
  {
    QRect titleRect;
    QRect plotRect;
    QRect xLabelRect;
    QRect yLabelRect;
    QRect legendRect;
  };

  void invalidateImageCache(bool requestUpdate = true);
  void invalidatePaletteCache();
  void ensureBufferCapacity(int columnCount);
  int bufferSlotForSample(int sampleIndex) const;
  int leadingBlankCount() const;
  int timeIndexForChronologicalSample(int sampleIndex) const;
  int chronologicalSampleForTimeIndex(int timeIndex) const;
  int visibleTimeStartIndex() const;
  int visibleTimeEndIndex() const;
  IntensityRange computeIntensityRange() const;
  double normalizedIntensity(double value, const IntensityRange &range) const;
  QImage buildPlotImage() const;
  const QVector<QRgb> &cachedPalette() const;
  PlotLayout computeLayout(const QRect &bounds, int titleHeight,
      int axisLabelHeight, int tickLabelHeight, int leftTickWidth,
      int legendTextWidth) const;
  QString physicalXAxisLabel() const;
  QString physicalYAxisLabel() const;
  double configuredSamplePeriodSeconds() const;
  double estimatedSamplePeriodSeconds() const;
  double ageSecondsForTimeIndex(int timeIndex) const;
  double ageSecondsForSample(int sampleIndex) const;
  double convertSecondsForUnits(double seconds) const;
  QString timeAxisValueText(int timeIndex) const;
  QString intensityValueText(double value) const;
  QString tooltipTextForPosition(const QPoint &position) const;
  bool forwardMouseEventToParent(QMouseEvent *event) const;
  void drawSelectionOutline(QPainter &painter, const QRect &rect) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString title_;
  QString xLabel_;
  QString yLabel_;
  QString dataChannel_;
  QString countChannel_;
  QString triggerChannel_;
  QString eraseChannel_;
  WaterfallEraseMode eraseMode_ = WaterfallEraseMode::kIfNotZero;
  int historyCount_ = kWaterfallDefaultHistory;
  WaterfallScrollDirection scrollDirection_ =
      WaterfallScrollDirection::kTopToBottom;
  HeatmapColorMap colorMap_ = HeatmapColorMap::kGrayscale;
  bool invertGreyscale_ = true;
  WaterfallIntensityScale intensityScale_ = WaterfallIntensityScale::kAuto;
  double intensityMin_ = 0.0;
  double intensityMax_ = 1.0;
  bool showLegend_ = true;
  bool showGrid_ = false;
  double samplePeriod_ = 0.0;
  TimeUnits units_ = TimeUnits::kSeconds;

  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  int runtimeWaveformLength_ = 0;
  int bufferColumnCount_ = 0;
  QVector<double> bufferValues_;
  QVector<int> sampleLengths_;
  QVector<qint64> sampleTimestampsMs_;
  int writeCursor_ = 0;
  int bufferedSamples_ = 0;

  mutable bool paletteCacheValid_ = false;
  mutable QVector<QRgb> cachedPalette_;
  mutable bool imageCacheValid_ = false;
  mutable QImage cachedImage_;
  mutable IntensityRange cachedIntensityRange_;

  QRect lastPlotRect_;
  QRect lastImageRect_;
  int lastVisibleTimeStart_ = 0;
  int lastVisibleTimeEnd_ = 0;

  double timeViewStart_ = 0.0;
  double timeViewEnd_ = 1.0;
  bool panning_ = false;
  QPoint panStartPos_;
  double panStartTimeViewStart_ = 0.0;
  double panStartTimeViewEnd_ = 1.0;
};
