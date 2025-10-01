#pragma once

#include <array>

#include <QColor>
#include <QPaintEvent>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class LineElement : public QWidget
{
public:
  explicit LineElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor color() const;
  void setForegroundColor(const QColor &color);

  RectangleLineStyle lineStyle() const;
  void setLineStyle(RectangleLineStyle style);

  int lineWidth() const;
  void setLineWidth(int width);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);

  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  void setLocalEndpoints(const QPoint &start, const QPoint &end);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor defaultForegroundColor() const;
  QPoint clampToSize(const QPoint &point, const QSize &size) const;
  QPointF ratioForPoint(const QPoint &point, const QSize &size) const;
  QPoint pointFromRatio(const QPointF &ratio) const;

  bool selected_ = false;
  QColor color_;
  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 4> channels_{};
  QPointF startRatio_{0.0, 0.0};
  QPointF endRatio_{1.0, 1.0};
};

