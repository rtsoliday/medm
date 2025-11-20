#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QRectF>

#include "display_properties.h"

class QFontMetricsF;

class ScaleMonitorElement : public QWidget
{
public:
  explicit ScaleMonitorElement(QWidget *parent = nullptr);

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

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);
  bool hasExplicitLimitsBlock() const;
  void setHasExplicitLimitsBlock(bool hasBlock);
  bool hasExplicitLimitsData() const;
  void setHasExplicitLimitsData(bool hasData);

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

  Layout calculateLayout(
      const QRectF &bounds, const QFontMetricsF &metrics) const;
  void paintScale(QPainter &painter, const QRectF &chartRect) const;
  void paintAxis(QPainter &painter, const Layout &layout) const;
  void paintInternalTicks(QPainter &painter, const QRectF &chartRect) const;
  void paintPointer(QPainter &painter, const Layout &layout) const;
  void paintLabels(QPainter &painter, const Layout &layout) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor defaultForeground() const;
  QColor defaultBackground() const;
  bool isVertical() const;
  bool isDirectionInverted() const;
  void paintSelectionOverlay(QPainter &painter) const;
  double normalizedSampleValue() const;
  double sampleValue() const;
  QString formattedSampleValue() const;
  double effectiveLowLimit() const;
  double effectiveHighLimit() const;
  int effectivePrecision() const;
  double currentValue() const;
  double defaultSampleValue() const;
  QString formatValue(double value, char format = 'f', int precision = -1) const;
  QString axisLabelText(double value) const;
  double clampToLimits(double value) const;
  double valueEpsilon() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
  BarDirection direction_ = BarDirection::kRight;
  PvLimits limits_{};
  bool hasExplicitLimitsBlock_ = false;
  bool hasExplicitLimitsData_ = false;
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeLimitsValid_ = false;
  bool hasRuntimeValue_ = false;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 0.0;
  int runtimePrecision_ = -1;
  double runtimeValue_ = 0.0;
  short runtimeSeverity_ = 3;
};
