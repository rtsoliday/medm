#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class ChoiceButtonElement : public QWidget
{
public:
  explicit ChoiceButtonElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  ChoiceButtonStacking stacking() const;
  void setStacking(ChoiceButtonStacking stacking);

  QString channel() const;
  void setChannel(const QString &channel);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  void paintSelectionOverlay(QPainter &painter) const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  ChoiceButtonStacking stacking_ = ChoiceButtonStacking::kRow;
  QString channel_;
};
