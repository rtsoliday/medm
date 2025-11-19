#pragma once

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include <QColor>
#include <QVector>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;
class QPainter;

class CartesianPlotElement : public QWidget
{
  Q_OBJECT

public:
  explicit CartesianPlotElement(QWidget *parent = nullptr);

signals:
  void axisDialogRequested();

public:

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

  QString yLabel(int index) const;
  void setYLabel(int index, const QString &label);

  CartesianPlotStyle style() const;
  void setStyle(CartesianPlotStyle style);

  bool eraseOldest() const;
  void setEraseOldest(bool eraseOldest);

  int count() const;
  void setCount(int count);

  CartesianPlotEraseMode eraseMode() const;
  void setEraseMode(CartesianPlotEraseMode mode);

  QString triggerChannel() const;
  void setTriggerChannel(const QString &channel);

  QString eraseChannel() const;
  void setEraseChannel(const QString &channel);

  QString countChannel() const;
  void setCountChannel(const QString &channel);

  int traceCount() const;

  QString traceXChannel(int index) const;
  void setTraceXChannel(int index, const QString &channel);

  QString traceYChannel(int index) const;
  void setTraceYChannel(int index, const QString &channel);

  QColor traceColor(int index) const;
  void setTraceColor(int index, const QColor &color);

  CartesianPlotYAxis traceYAxis(int index) const;
  void setTraceYAxis(int index, CartesianPlotYAxis axis);

  bool traceUsesRightAxis(int index) const;
  void setTraceUsesRightAxis(int index, bool usesRightAxis);

  CartesianPlotAxisStyle axisStyle(int axisIndex) const;
  void setAxisStyle(int axisIndex, CartesianPlotAxisStyle style);

  CartesianPlotRangeStyle axisRangeStyle(int axisIndex) const;
  void setAxisRangeStyle(int axisIndex, CartesianPlotRangeStyle style);

  double axisMinimum(int axisIndex) const;
  void setAxisMinimum(int axisIndex, double value);

  double axisMaximum(int axisIndex) const;
  void setAxisMaximum(int axisIndex, double value);

  CartesianPlotTimeFormat axisTimeFormat(int axisIndex) const;
  void setAxisTimeFormat(int axisIndex, CartesianPlotTimeFormat format);
  QString axisLabel(int axisIndex) const;
  bool isAxisDrawnOnLeft(int axisIndex) const;

  bool drawMajorGrid() const;
  void setDrawMajorGrid(bool draw);

  bool drawMinorGrid() const;
  void setDrawMinorGrid(bool draw);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;
  void setTraceRuntimeMode(int index, CartesianPlotTraceMode mode);
  void setTraceRuntimeConnected(int index, bool connected);
  void updateTraceRuntimeData(int index, QVector<QPointF> points);
  void clearTraceRuntimeData(int index);
  void clearRuntimeState();
  void setRuntimeCount(int count);
  int effectiveSampleCapacity() const;
  void setAxisRuntimeLimits(int axisIndex, double minimum, double maximum,
      bool valid);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  bool event(QEvent *event) override;

private:
  struct Trace
  {
    QString xChannel;
    QString yChannel;
    QColor color;
    CartesianPlotYAxis yAxis = CartesianPlotYAxis::kY1;
    bool usesRightAxis = false;
    CartesianPlotTraceMode runtimeMode = CartesianPlotTraceMode::kNone;
    bool runtimeConnected = false;
    QVector<QPointF> runtimePoints;
  };

  struct AxisRange
  {
    double minimum = 0.0;
    double maximum = 1.0;
    bool valid = false;
    CartesianPlotAxisStyle style = CartesianPlotAxisStyle::kLinear;
  };

  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor effectiveTraceColor(int index) const;
  QRectF chartRect() const;
  void paintFrame(QPainter &painter) const;
  void paintGrid(QPainter &painter, const QRectF &rect) const;
  void paintAxes(QPainter &painter, const QRectF &rect) const;
    void paintYAxis(QPainter &painter, const QRectF &rect, int yAxisIndex,
      qreal axisX, bool onLeft, const AxisRange *precomputedRange) const;
  struct YAxisPositions {
    std::vector<std::pair<int, qreal>> leftAxes;  // (axisIndex, xPosition)
    std::vector<std::pair<int, qreal>> rightAxes; // (axisIndex, xPosition)
  };
  YAxisPositions calculateYAxisPositions(const QRectF &widgetBounds) const;
  void paintLabels(QPainter &painter, const QRectF &rect) const;
  void paintTraces(QPainter &painter, const QRectF &rect) const;
  void paintSelectionOverlay(QPainter &painter) const;
  QVector<QPointF> syntheticTracePoints(const QRectF &rect,
      int traceIndex, int sampleCount) const;
  void paintTracesExecute(QPainter &painter, const QRectF &rect) const;
  AxisRange computeAxisRange(int axisIndex,
      const std::array<bool, kCartesianAxisCount> &hasData,
      const std::array<double, kCartesianAxisCount> &autoMinimums,
      const std::array<double, kCartesianAxisCount> &autoMaximums) const;
  bool mapPointToChart(const QPointF &value, const AxisRange &xRange,
      const AxisRange &yRange, const QRectF &rect, QPointF *mapped) const;
  int axisIndexForTrace(int traceIndex) const;
  bool isYAxisOnRight(int yAxisIndex) const;
  bool isYAxisVisible(int yAxisIndex) const;
  bool shouldPaintYAxisCue(int yAxisIndex) const;
    std::optional<QColor> axisCueColor(int yAxisIndex) const;
    void paintAxisColorCue(QPainter &painter, const QRectF &labelBounds,
      const QColor &color) const;
  void ensureRuntimeArraySizes();
  
  struct NiceAxisRange
  {
    double drawMin;
    double drawMax;
    double majorInc;
    int numMajor;
    int numMinor;
  };
  static NiceAxisRange computeNiceAxisRange(double min, double max, bool isLog);

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString title_;
  QString xLabel_;
  std::array<QString, 4> yLabels_{};
  CartesianPlotStyle style_ = CartesianPlotStyle::kLine;
  bool eraseOldest_ = false;
  int count_ = 0;
  CartesianPlotEraseMode eraseMode_ = CartesianPlotEraseMode::kIfNotZero;
  QString triggerChannel_;
  QString eraseChannel_;
  QString countChannel_;
  std::array<Trace, kCartesianPlotTraceCount> traces_{};
  std::array<CartesianPlotAxisStyle, kCartesianAxisCount> axisStyles_{};
  std::array<CartesianPlotRangeStyle, kCartesianAxisCount> axisRangeStyles_{};
  std::array<double, kCartesianAxisCount> axisMinimums_{};
  std::array<double, kCartesianAxisCount> axisMaximums_{};
  std::array<CartesianPlotTimeFormat, kCartesianAxisCount> axisTimeFormats_{};
  bool drawMajorGrid_ = true;
  bool drawMinorGrid_ = false;
  bool executeMode_ = false;
  int runtimeCount_ = 0;
  bool runtimeCountValid_ = false;
  std::array<bool, kCartesianAxisCount> axisRuntimeValid_{};
  std::array<double, kCartesianAxisCount> axisRuntimeMinimums_{};
  std::array<double, kCartesianAxisCount> axisRuntimeMaximums_{};
  mutable std::array<AxisRange, kCartesianAxisCount> cachedAxisRanges_{};
  mutable bool cachedAxisRangesValid_ = false;
};
