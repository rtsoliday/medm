#pragma once

#include <array>

#include <QColor>
#include <QVector>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;
class QPainter;

class CartesianPlotElement : public QWidget
{
public:
  explicit CartesianPlotElement(QWidget *parent = nullptr);

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

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct Trace
  {
    QString xChannel;
    QString yChannel;
    QColor color;
    CartesianPlotYAxis yAxis = CartesianPlotYAxis::kY1;
    bool usesRightAxis = false;
  };

  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor effectiveTraceColor(int index) const;
  QRectF chartRect() const;
  void paintFrame(QPainter &painter) const;
  void paintGrid(QPainter &painter, const QRectF &rect) const;
  void paintAxes(QPainter &painter, const QRectF &rect) const;
  void paintLabels(QPainter &painter, const QRectF &rect) const;
  void paintTraces(QPainter &painter, const QRectF &rect) const;
  void paintSelectionOverlay(QPainter &painter) const;
  QVector<QPointF> syntheticTracePoints(const QRectF &rect,
      int traceIndex, int sampleCount) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString title_;
  QString xLabel_;
  std::array<QString, 4> yLabels_{};
  CartesianPlotStyle style_ = CartesianPlotStyle::kLine;
  bool eraseOldest_ = false;
  int count_ = 1;
  CartesianPlotEraseMode eraseMode_ = CartesianPlotEraseMode::kIfNotZero;
  QString triggerChannel_;
  QString eraseChannel_;
  QString countChannel_;
  std::array<Trace, kCartesianPlotTraceCount> traces_{};
};

