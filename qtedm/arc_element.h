#pragma once

#include <QColor>
#include <QPaintEvent>
#include <QString>
#include <QWidget>

#include "display_properties.h"
#include "graphic_shape_element.h"

class ArcElement : public GraphicShapeElement
{
public:
  explicit ArcElement(QWidget *parent = nullptr);

  RectangleFill fill() const;
  void setFill(RectangleFill fill);

  RectangleLineStyle lineStyle() const;
  void setLineStyle(RectangleLineStyle style);

  int lineWidth() const;
  void setLineWidth(int width);

  int beginAngle() const;
  void setBeginAngle(int angle64);

  int pathAngle() const;
  void setPathAngle(int angle64);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  int toQtAngle(int angle64) const;

  RectangleFill fill_ = RectangleFill::kOutline;
  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  int beginAngle_ = 0;
  int pathAngle_ = 90 * 64;
};
