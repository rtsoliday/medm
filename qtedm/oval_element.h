#pragma once

#include <QColor>
#include <QPaintEvent>
#include <QString>
#include <QWidget>

#include "graphic_properties.h"
#include "graphic_shape_element.h"

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
  int lineWidth_ = 1;
};
