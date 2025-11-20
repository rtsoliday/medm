#pragma once

#include <functional>

#include <QColor>
#include <QString>
#include <QWidget>
#include <QRectF>

#include "display_properties.h"

class QMouseEvent;
class QPointF;
class QPainter;

class QKeyEvent;

class SliderElement : public QWidget
{
public:
  explicit SliderElement(QWidget *parent = nullptr);

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

  double increment() const;
  void setIncrement(double increment);

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
  void setRuntimeWriteAccess(bool writeAccess);
  void setRuntimeSeverity(short severity);
  void setRuntimeLimits(double low, double high);
  void setRuntimePrecision(int precision);
  void setRuntimeValue(double value);
  void clearRuntimeState();

  void setActivationCallback(const std::function<void(double)> &callback);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QRectF trackRectForPainting(QRectF contentRect, QRectF &limitRect,
      QRectF &channelRect) const;
  void paintTrack(QPainter &painter, const QRectF &trackRect) const;
  void paintThumb(QPainter &painter, const QRectF &trackRect) const;
  void paintTicks(QPainter &painter, const QRectF &trackRect) const;
  void paintLabels(QPainter &painter, const QRectF &trackRect,
      const QRectF &limitRect, const QRectF &channelRect) const;
  bool shouldShowLimitLabels() const;
  QColor effectiveForeground() const;
  QColor effectiveForegroundForValueText() const;
  QColor effectiveBackground() const;
  void paintSelectionOverlay(QPainter &painter) const;
  bool isVertical() const;
  bool isDirectionInverted() const;
  double normalizedValue() const;
  double currentDisplayedValue() const;
  double effectiveLowLimit() const;
  double effectiveHighLimit() const;
  int effectivePrecision() const;
  double clampToLimits(double value) const;
  double valueFromPosition(const QPointF &pos) const;
  QRectF thumbRectForTrack(const QRectF &trackRect) const;
  void beginDrag(double value, bool sendInitial);
  void updateDrag(double value, bool force);
  void endDrag(double value, bool force);
  void sendActivationValue(double value, bool force);
  void updateCursor();
  bool isInteractive() const;
  double sliderEpsilon() const;
  double quantizeToIncrement(double value) const;
  double defaultSampleValue() const;
  QString formatLimit(double value) const;
  double keyboardStep(Qt::KeyboardModifiers modifiers) const;
  bool applyKeyboardDelta(double delta);
  bool forwardMouseEventToParent(QMouseEvent *event) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
  BarDirection direction_ = BarDirection::kRight;
  double increment_ = 1.0;
  PvLimits limits_{};
  bool hasExplicitLimitsBlock_ = false;
  bool hasExplicitLimitsData_ = false;
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 0.0;
  bool runtimeLimitsValid_ = false;
  int runtimePrecision_ = -1;
  double runtimeValue_ = 0.0;
  bool hasRuntimeValue_ = false;
  bool dragging_ = false;
  double dragValue_ = 0.0;
  double lastSentValue_ = 0.0;
  bool hasLastSentValue_ = false;
  std::function<void(double)> activationCallback_;
};
