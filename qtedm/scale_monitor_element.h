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

  QString channel() const;
  void setChannel(const QString &channel);

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
  bool isVertical() const;
  bool isDirectionInverted() const;
  void paintSelectionOverlay(QPainter &painter) const;
  double normalizedSampleValue() const;
  double sampleValue() const;
  QString formattedSampleValue() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
  BarDirection direction_ = BarDirection::kRight;
  PvLimits limits_{};
  QString channel_;
};
