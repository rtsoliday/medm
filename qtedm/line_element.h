#pragma once

#include <QColor>
#include <QPaintEvent>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

#include "display_properties.h"
#include "graphic_shape_element.h"

class LineElement : public GraphicShapeElement
{
public:
  explicit LineElement(QWidget *parent = nullptr);

  RectangleLineStyle lineStyle() const;
  void setLineStyle(RectangleLineStyle style);

  int lineWidth() const;
  void setLineWidth(int width);

  void setLocalEndpoints(const QPoint &start, const QPoint &end);
  QVector<QPoint> absolutePoints() const;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QPoint clampToSize(const QPoint &point, const QSize &size) const;
  QPointF ratioForPoint(const QPoint &point, const QSize &size) const;
  QPoint pointFromRatio(const QPointF &ratio) const;

  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  QPointF startRatio_{0.0, 0.0};
  QPointF endRatio_{1.0, 1.0};
};
