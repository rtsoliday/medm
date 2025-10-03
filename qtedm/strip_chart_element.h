#pragma once

#include <array>

#include <QColor>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;
class QPainter;

class StripChartElement : public QWidget
{
public:
  explicit StripChartElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  QString title() const;
  void setTitle(const QString &title);

  QString xLabel() const;
  void setXLabel(const QString &label);

  QString yLabel() const;
  void setYLabel(const QString &label);

  double period() const;
  void setPeriod(double period);

  TimeUnits units() const;
  void setUnits(TimeUnits units);

  int penCount() const;

  QString channel(int index) const;
  void setChannel(int index, const QString &channel);

  QColor penColor(int index) const;
  void setPenColor(int index, const QColor &color);

  PvLimits penLimits(int index) const;
  void setPenLimits(int index, const PvLimits &limits);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct Pen
  {
    QColor color;
    QString channel;
    PvLimits limits;
  };

  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor effectivePenColor(int index) const;
  QRect chartRect() const;
  void paintFrame(QPainter &painter) const;
  void paintGrid(QPainter &painter, const QRect &content) const;
  void paintPens(QPainter &painter, const QRect &content) const;
  void paintLabels(QPainter &painter, const QRect &content) const;
  void paintSelectionOverlay(QPainter &painter) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  QString title_;
  QString xLabel_;
  QString yLabel_;
  double period_ = kDefaultStripChartPeriod;
  TimeUnits units_ = TimeUnits::kSeconds;
  std::array<Pen, kStripChartPenCount> pens_{};
};

