#pragma once

#include <array>
#include <deque>

#include <QColor>
#include <QFont>
#include <QPixmap>
#include <QWidget>

#include "display_properties.h"

class QTimer;

class QMouseEvent;
class QPaintEvent;
class QPainter;
class QFontMetrics;
class QResizeEvent;

class StripChartElement : public QWidget
{
public:
  explicit StripChartElement(QWidget *parent = nullptr);

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

  double period() const;
  void setPeriod(double period);

  TimeUnits units() const;
  void setUnits(TimeUnits units);

  int penCount() const;

  QString channel(int index) const;
  void setChannel(int index, const QString &channel);

  QColor penColor(int index) const;
  void setPenColor(int index, const QColor &color);

  PvLimits penLimits(int index) const;
  void setPenLimits(int index, const PvLimits &limits);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;
  void setRuntimeConnected(int index, bool connected);
  void setRuntimeLimits(int index, double low, double high);
  void addRuntimeSample(int index, double value, qint64 timestampMs);
  void clearRuntimeState();
  void clearPenRuntimeState(int index);

  // Data export methods
  int sampleCount() const;
  double sampleValue(int penIndex, int sampleIndex) const;
  double sampleIntervalSeconds() const;
  bool penHasData(int index) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  bool forwardMouseEventToParent(QMouseEvent *event) const;

private:
  struct Layout
  {
    QRect innerRect;
    QRect chartRect;
    QRect titleRect;
    QRect xLabelRect;
    QRect yLabelRect;
    QString titleText;
    QString xLabelText;
    QString yLabelText;
    int yAxisLabelOffset = 0;  // Extra offset for Y-axis labels when avoiding overlap
  };

  struct Pen
  {
    QColor color;
    QString channel;
    PvLimits limits;
    bool runtimeConnected = false;
    bool runtimeLimitsValid = false;
    double runtimeLow = 0.0;
    double runtimeHigh = 0.0;
    std::deque<double> samples;
    double runtimeValue = 0.0;
    bool hasRuntimeValue = false;
  };

  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor effectivePenColor(int index) const;
  QFont labelFont() const;
  QFont titleFont() const;
  QRect chartRect() const;
  Layout calculateLayout(const QFontMetrics &metrics) const;
  int calculateYAxisLabelWidth(const QFontMetrics &metrics) const;
  int calculateYAxisLabelHeightExtension(const QFontMetrics &metrics) const;
  int calculateXAxisTickCount(int chartWidth, const QFontMetrics &metrics) const;
  int calculateYAxisTickCount(int chartHeight, const QFontMetrics &metrics) const;
  void paintFrame(QPainter &painter) const;
  void paintGrid(QPainter &painter, const QRect &content) const;
  void paintTickMarks(QPainter &painter, const QRect &chartRect) const;
  void paintAxisScales(QPainter &painter, const QRect &chartRect,
      const QFontMetrics &metrics, int yAxisLabelOffset = 0) const;
  void paintPens(QPainter &painter, const QRect &content) const;
  void paintDesignPens(QPainter &painter, const QRect &content) const;
  void paintRuntimePens(QPainter &painter, const QRect &content) const;
  void paintLabels(QPainter &painter, const Layout &layout,
      const QFontMetrics &metrics) const;
  void paintSelectionOverlay(QPainter &painter) const;
  double periodMilliseconds() const;
  double effectivePenLow(int index) const;
  double effectivePenHigh(int index) const;
  void ensureRefreshTimer();
  void updateRefreshTimer();
  void handleRefreshTimer();
  void updateSamplingGeometry(int chartWidth);
  void enforceSampleCapacity(int capacity);
  void maybeAppendSamples(qint64 nowMs);
  void appendSampleColumn();
  bool anyPenConnected() const;
  bool anyPenReady() const;
  void invalidateStaticCache();
  void ensureStaticCache(const QFont &labelsFont, const QFontMetrics &metrics);
  void paintStaticContent(QPainter &painter, const Layout &layout,
      const QFontMetrics &metrics) const;
  void invalidatePenCache();
  void ensurePenCache(const QRect &plotArea);

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString title_;
  QString xLabel_;
  QString yLabel_;
  double period_ = kDefaultStripChartPeriod;
  TimeUnits units_ = TimeUnits::kSeconds;
  std::array<Pen, kStripChartPenCount> pens_{};
  bool executeMode_ = false;
  QTimer *refreshTimer_ = nullptr;
  double sampleIntervalMs_ = 1000.0;
  qint64 lastSampleMs_ = 0;
  qint64 nextAdvanceTimeMs_ = 0;  // MEDM-style: time for next pixel advance
  int cachedChartWidth_ = 0;
  int sampleHistoryLength_ = 0;
  int newSampleColumns_ = 0;  // Columns added since last paint
  mutable QPixmap staticCache_;
  mutable bool staticCacheDirty_ = true;
  mutable Layout cachedLayout_;
  mutable QPixmap penCache_;
  mutable bool penCacheDirty_ = true;
  mutable QRect penCachePlotArea_;
  mutable int circularWriteSlot_ = 0;  // MEDM-style circular buffer write position
  
  // Adaptive refresh rate for slow network connections
  int currentRefreshIntervalMs_ = 100;
  int lateRefreshCount_ = 0;
  qint64 expectedRefreshTimeMs_ = 0;
};
