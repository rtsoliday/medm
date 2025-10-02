#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QRectF>

#include "display_properties.h"

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

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QRectF trackRectForPainting(QRectF contentRect, QRectF &labelRect) const;
  void paintTrack(QPainter &painter, const QRectF &trackRect) const;
  void paintFill(QPainter &painter, const QRectF &trackRect) const;
  void paintLabels(
      QPainter &painter, const QRectF &trackRect, const QRectF &labelRect) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor barTrackColor() const;
  QColor barFillColor() const;
  void paintSelectionOverlay(QPainter &painter) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
  BarDirection direction_ = BarDirection::kRight;
  BarFill fillMode_ = BarFill::kFromEdge;
  PvLimits limits_{};
  QString channel_;
};
