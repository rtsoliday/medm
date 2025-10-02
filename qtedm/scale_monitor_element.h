#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QRectF>

#include "display_properties.h"

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

  QString channel() const;
  void setChannel(const QString &channel);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QRectF scaleRectForPainting(QRectF contentRect, QRectF &labelRect) const;
  void paintScale(QPainter &painter, const QRectF &scaleRect) const;
  void paintTicks(QPainter &painter, const QRectF &scaleRect) const;
  void paintPointer(QPainter &painter, const QRectF &scaleRect) const;
  void paintLabels(
      QPainter &painter, const QRectF &scaleRect, const QRectF &labelRect) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  bool isVertical() const;
  bool isDirectionInverted() const;
  void paintSelectionOverlay(QPainter &painter) const;
  double normalizedSampleValue() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
  BarDirection direction_ = BarDirection::kRight;
  PvLimits limits_{};
  QString channel_;
};

