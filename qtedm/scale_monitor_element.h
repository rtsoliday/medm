#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QRectF>

#include "medm_minimum_size.h"
#include "monitor_properties.h"
#include "text_properties.h"

class QFontMetrics;
class QPainter;
class QResizeEvent;

class ScaleMonitorElement : public QWidget
{
  friend class DisplayWindow;

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
  double displayLowLimit() const;
  double displayHighLimit() const;
  bool hasExplicitLimitsBlock() const;
  void setHasExplicitLimitsBlock(bool hasBlock);
  bool hasExplicitLimitsData() const;
  void setHasExplicitLimitsData(bool hasData);
  bool hasExplicitLowLimitData() const;
  void setHasExplicitLowLimitData(bool hasData);
  bool hasExplicitHighLimitData() const;
  void setHasExplicitHighLimitData(bool hasData);
  bool hasExplicitPrecisionData() const;
  void setHasExplicitPrecisionData(bool hasData);

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
  void resizeEvent(QResizeEvent *event) override;

private:
  void paintXcIndicator(QPainter &painter, const QFontMetrics &metrics) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor defaultForeground() const;
  QColor defaultBackground() const;
  bool isVertical() const;
  void paintSelectionOverlay(QPainter &painter) const;
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
  bool hasExplicitLowLimitData_ = false;
  bool hasExplicitHighLimitData_ = false;
  bool hasExplicitPrecisionData_ = false;
  QString channel_;
  bool executeMode_ = false;
  MedmMinimumSize medmMinimumSize_;
  bool runtimeConnected_ = false;
  bool runtimeLimitsValid_ = false;
  bool hasRuntimeValue_ = false;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 0.0;
  int runtimePrecision_ = -1;
  double runtimeValue_ = 0.0;
  short runtimeSeverity_ = 3;
};
