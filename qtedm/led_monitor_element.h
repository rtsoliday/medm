#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QtGlobal>

#include <array>

#include "monitor_properties.h"
#include "text_properties.h"

class LedMonitorElement : public QWidget
{
  friend class DisplayWindow;

public:
  explicit LedMonitorElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  LedShape shape() const;
  void setShape(LedShape shape);

  bool bezel() const;
  void setBezel(bool bezel);

  QColor onColor() const;
  void setOnColor(const QColor &color);

  QColor offColor() const;
  void setOffColor(const QColor &color);

  QColor undefinedColor() const;
  void setUndefinedColor(const QColor &color);

  QColor stateColor(int index) const;
  void setStateColor(int index, const QColor &color);

  int stateCount() const;
  void setStateCount(int count);

  QString channel() const;
  void setChannel(const QString &channel);
  QString channel(int index) const;
  void setChannel(int index, const QString &channel);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);
  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

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
  struct FillState
  {
    QColor color;
    bool hatched = false;
  };

  void applyRuntimeVisibility();
  void paintSelectionOverlay(QPainter &painter) const;
  FillState effectiveFillState() const;
  QColor effectiveBackground() const;
  QColor defaultForeground() const;
  QColor defaultBackground() const;
  QColor defaultUndefined() const;
  QColor currentColorForState(int index) const;
  void syncBinaryColorsFromStates();
  void syncStateColorsFromBinary();

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kAlarm;
  LedShape shape_ = LedShape::kCircle;
  bool bezel_ = false;
  QColor onColor_;
  QColor offColor_;
  QColor undefinedColor_;
  std::array<QColor, kLedStateCount> stateColors_{};
  int stateCount_ = 2;
  QString channel_;
  std::array<QString, 5> visibilityChannels_{};
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeVisible_ = true;
  bool hasRuntimeValue_ = false;
  qint32 runtimeValue_ = 0;
  short runtimeSeverity_ = 3;
};
