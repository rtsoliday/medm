#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class MeterElement : public QWidget
{
public:
  explicit MeterElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  MeterLabel label() const;
  void setLabel(MeterLabel label);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);

  QString channel() const;
  void setChannel(const QString &channel);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  void paintSelectionOverlay(QPainter &painter);
  void paintDial(QPainter &painter, const QRectF &dialRect) const;
  void paintTicks(QPainter &painter, const QRectF &dialRect) const;
  void paintNeedle(QPainter &painter, const QRectF &dialRect) const;
  void paintLabels(QPainter &painter, const QRectF &dialRect) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  MeterLabel label_ = MeterLabel::kOutline;
  PvLimits limits_{};
  QString channel_;
};
