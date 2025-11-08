#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QRectF>

#include "display_properties.h"

class QFontMetricsF;

class BarMonitorElement : public QWidget
{
public:
  explicit BarMonitorElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  MeterLabel label() const;
  void setLabel(MeterLabel label);

  BarDirection direction() const;
  void setDirection(BarDirection direction);

  BarFill fillMode() const;
  void setFillMode(BarFill mode);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);

  QString channel() const;
  void setChannel(const QString &channel);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;
  void setRuntimeConnected(bool connected);
  void setRuntimeSeverity(short severity);
  void setRuntimeValue(double value);
  void setRuntimeLimits(double low, double high);
  void setRuntimePrecision(int precision);
  void clearRuntimeState();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct Layout;
  Layout calculateLayout(const QRectF &contentRect,
      const QFontMetricsF &metrics) const;
  void paintTrack(QPainter &painter, const QRectF &trackRect) const;
  void paintFill(QPainter &painter, const QRectF &trackRect) const;
  void paintAxis(QPainter &painter, const Layout &layout) const;
  void paintLabels(QPainter &painter, const Layout &layout) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor barTrackColor() const;
  QColor barFillColor() const;
  void paintSelectionOverlay(QPainter &painter) const;
  double normalizedSampleValue() const;
  double sampleValue() const;
  QString formattedSampleValue() const;
  double effectiveLowLimit() const;
  double effectiveHighLimit() const;
  int effectivePrecision() const;
  double currentValue() const;
  double defaultSampleValue() const;
  double clampToLimits(double value) const;
  QString formatValue(double value, char format = 'f', int precision = -1) const;
  QString axisLabelText(double value) const;
  double valueEpsilon() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kNone;
  BarDirection direction_ = BarDirection::kRight;
  BarFill fillMode_ = BarFill::kFromEdge;
  PvLimits limits_{};
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeLimitsValid_ = false;
  bool hasRuntimeValue_ = false;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 1.0;
  int runtimePrecision_ = -1;
  double runtimeValue_ = 0.0;
  short runtimeSeverity_ = 0;
};
