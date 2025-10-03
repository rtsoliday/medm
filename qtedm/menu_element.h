#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class QComboBox;
class QPaintEvent;
class QResizeEvent;

class MenuElement : public QWidget
{
public:
  explicit MenuElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  QString channel() const;
  void setChannel(const QString &channel);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void applyPaletteColors();
  void updateSelectionVisual();

  bool selected_ = false;
  QComboBox *comboBox_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  QString channel_;
};

