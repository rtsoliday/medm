#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class WheelSwitchElement : public QWidget
{
public:
  explicit WheelSwitchElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  double precision() const;
  void setPrecision(double precision);

  QString format() const;
  void setFormat(const QString &format);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);

  QString channel() const;
  void setChannel(const QString &channel);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  void paintButton(QPainter &painter, const QRectF &rect, bool isUp) const;
  void paintValueDisplay(QPainter &painter, const QRectF &rect) const;
  void paintSelectionOverlay(QPainter &painter) const;
  QString formattedSampleValue() const;
  double sampleValue() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  double precision_ = 1.0;
  QString format_;
  PvLimits limits_{};
  QString channel_;
};
