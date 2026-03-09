#pragma once

#include <QColor>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <array>

#include "display_properties.h"

class QFontMetricsF;
class QPainter;

class ThermometerElement : public QWidget
{
public:
  explicit ThermometerElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  QColor textColor() const;
  void setTextColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);
  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);
  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  MeterLabel label() const;
  void setLabel(MeterLabel label);

  BarDirection direction() const;
  void setDirection(BarDirection direction);

  TextMonitorFormat format() const;
  void setFormat(TextMonitorFormat format);

  bool showValue() const;
  void setShowValue(bool showValue);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);
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
  QString channel(int index) const;
  void setChannel(int index, const QString &channel);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;
  void setRuntimeConnected(bool connected);
  void setRuntimeSeverity(short severity);
  void setRuntimeVisible(bool visible);
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
  void paintTrack(QPainter &painter, const Layout &layout) const;
  void paintFill(QPainter &painter, const Layout &layout) const;
  void paintAxis(QPainter &painter, const Layout &layout) const;
  void paintLabels(QPainter &painter, const Layout &layout) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor trackColor() const;
  QColor fillColor() const;
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
  QString formatValue(double value) const;
  QString axisLabelText(double value) const;
  double valueEpsilon() const;
  void applyRuntimeVisibility();

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QColor textColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  MeterLabel label_ = MeterLabel::kNone;
  BarDirection direction_ = BarDirection::kUp;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  bool showValue_ = false;
  PvLimits limits_{};
  bool hasExplicitLimitsBlock_ = false;
  bool hasExplicitLimitsData_ = false;
  bool hasExplicitLowLimitData_ = false;
  bool hasExplicitHighLimitData_ = false;
  bool hasExplicitPrecisionData_ = false;
  QString channel_;
  std::array<QString, 5> visibilityChannels_{};
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeVisible_ = true;
  bool runtimeLimitsValid_ = false;
  bool hasRuntimeValue_ = false;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 1.0;
  int runtimePrecision_ = -1;
  double runtimeValue_ = 0.0;
  short runtimeSeverity_ = 0;
};
