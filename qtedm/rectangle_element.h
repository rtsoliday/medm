#pragma once

#include <QColor>
#include <QPaintEvent>
#include <QRect>
#include <QSize>
#include <QWidget>

#include "display_properties.h"
#include "graphic_shape_element.h"

class RectangleElement : public GraphicShapeElement
{
public:
  explicit RectangleElement(QWidget *parent = nullptr);

  RectangleFill fill() const;
  void setFill(RectangleFill fill);

  RectangleLineStyle lineStyle() const;
  void setLineStyle(RectangleLineStyle style);

  int lineWidth() const;
  void setLineWidth(int width);
  void setLineWidthFromAdl(int width);
  bool shouldSerializeLineWidth() const;

  int adlLineWidth() const;
  void setAdlLineWidth(int width, bool hasProperty);

  void setGeometry(const QRect &rect);
  using QWidget::setGeometry;

  void initializeFromAdlGeometry(const QRect &geometry,
      const QSize &adlSize);
  void setGeometryWithoutTracking(const QRect &geometry);
  QRect geometryForSerialization() const;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  RectangleFill fill_ = RectangleFill::kOutline;
  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  int adlLineWidth_ = 0;
  bool suppressGeometryTracking_ = false;
  bool hasOriginalAdlSize_ = false;
  QSize originalAdlSize_;
  bool sizeEdited_ = false;
  bool suppressLineWidthTracking_ = false;
  bool lineWidthEdited_ = false;
  bool hasAdlLineWidthProperty_ = false;
};
