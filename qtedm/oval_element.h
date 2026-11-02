#pragma once

#include <QColor>
#include <QPaintEvent>
#include <QString>
#include <QWidget>

#include "graphic_properties.h"
#include "graphic_shape_element.h"

class QPainter;

/* Fill an ellipse with the spans the X server's miFillEllipseI
 * (mi/mifillarc.c) generates for MEDM's XFillArc of a full circle. Qt
 * rasterizes an unoutlined ellipse one column wider on the left, which
 * hides that side of an outline drawn around it. */
void fillX11Ellipse(QPainter &painter, const QRect &box, const QColor &color);

class OvalElement : public GraphicShapeElement
{
public:
  explicit OvalElement(QWidget *parent = nullptr);

  RectangleFill fill() const;
  void setFill(RectangleFill fill);

  RectangleLineStyle lineStyle() const;
  void setLineStyle(RectangleLineStyle style);

  int lineWidth() const;
  void setLineWidth(int width);
  bool containsGlobalPoint(const QPoint &point) const;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  RectangleFill fill_ = RectangleFill::kOutline;
  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 0;
};
