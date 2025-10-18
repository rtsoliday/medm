#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class MeterElement : public QWidget
{
public:
  explicit MeterElement(QWidget *parent = nullptr);

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
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  double effectiveLowLimit() const;
  double effectiveHighLimit() const;
  int effectivePrecision() const;
  double currentValue() const;
  double defaultSampleValue() const;
  double clampToLimits(double value) const;
  double meterEpsilon() const;
  QString formatValue(double value) const;
  void paintSelectionOverlay(QPainter &painter);
  void paintDial(QPainter &painter, const QRectF &dialRect) const;
  void paintTicks(QPainter &painter, const QRectF &dialRect) const;
  void paintNeedle(QPainter &painter, const QRectF &dialRect) const;
  void paintLabels(QPainter &painter, const QRectF &dialRect,
      const QRectF &limitsRect, const QRectF &valueRect,
      const QRectF &channelRect) const;
  double normalizedSampleValue() const;
  QString formattedSampleValue() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
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
