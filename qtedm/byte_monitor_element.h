#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class ByteMonitorElement : public QWidget
{
public:
  explicit ByteMonitorElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  BarDirection direction() const;
  void setDirection(BarDirection direction);

  int startBit() const;
  void setStartBit(int bit);

  int endBit() const;
  void setEndBit(int bit);

  QString channel() const;
  void setChannel(const QString &channel);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void paintSelectionOverlay(QPainter &painter) const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  BarDirection direction_ = BarDirection::kRight;
  int startBit_ = 15;
  int endBit_ = 0;
  QString channel_;
};

