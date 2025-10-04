#pragma once

#include <array>

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

class PolygonElement : public QWidget
{
public:
  explicit PolygonElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor color() const;
  void setForegroundColor(const QColor &color);

  RectangleFill fill() const;
  void setFill(RectangleFill fill);

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

  void setAbsolutePoints(const QVector<QPoint> &points);
  QVector<QPoint> absolutePoints() const;
  bool containsGlobalPoint(const QPoint &point) const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QColor defaultForegroundColor() const;
  void recalcLocalPolygon();

  bool selected_ = false;
  QColor color_;
  RectangleFill fill_ = RectangleFill::kOutline;
  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  QVector<QPointF> normalizedPoints_;
  QPolygon localPolygon_;
};

