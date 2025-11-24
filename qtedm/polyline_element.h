#pragma once

#include <QColor>
#include <QPaintEvent>
#include <QPoint>
#include <QPointF>
#include <QPolygon>
#include <QResizeEvent>
#include <QString>
#include <QVector>
#include <QWidget>

#include "display_properties.h"
#include "graphic_shape_element.h"

class PolylineElement : public GraphicShapeElement
{
public:
  explicit PolylineElement(QWidget *parent = nullptr);

  RectangleLineStyle lineStyle() const;
  void setLineStyle(RectangleLineStyle style);

  int lineWidth() const;
  void setLineWidth(int width);

  void setAbsolutePoints(const QVector<QPoint> &points);
  QVector<QPoint> absolutePoints() const;
  bool containsGlobalPoint(const QPoint &point) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void recalcLocalPolyline();

  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  QVector<QPointF> normalizedPoints_;
  QPolygon localPolyline_;
};
