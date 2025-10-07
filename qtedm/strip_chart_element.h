#pragma once

#include <array>

#include <QColor>
#include <QFont>
#include <QWidget>

#include "display_properties.h"

class QPaintEvent;
class QPainter;
class QFontMetrics;

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
  struct Layout
  {
    QRect innerRect;
    QRect chartRect;
    QRect titleRect;
    QRect xLabelRect;
    QRect yLabelRect;
    QString titleText;
    QString xLabelText;
    QString yLabelText;
  };

  struct Pen
  {
    QColor color;
    QString channel;
    PvLimits limits;
  };

  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  QColor effectivePenColor(int index) const;
  QFont labelFont() const;
  QRect chartRect() const;
  Layout calculateLayout(const QFontMetrics &metrics) const;
  void paintFrame(QPainter &painter) const;
  void paintGrid(QPainter &painter, const QRect &content) const;
  void paintPens(QPainter &painter, const QRect &content) const;
  void paintLabels(QPainter &painter, const Layout &layout,
      const QFontMetrics &metrics) const;
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
