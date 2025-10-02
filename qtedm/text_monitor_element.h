#pragma once

#include <array>

#include <QColor>
#include <QLabel>
#include <QString>

#include "display_properties.h"

class QResizeEvent;
class QPaintEvent;

class TextMonitorElement : public QLabel
{
public:
  explicit TextMonitorElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  Qt::Alignment textAlignment() const;
  void setTextAlignment(Qt::Alignment alignment);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextMonitorFormat format() const;
  void setFormat(TextMonitorFormat format);

  int precision() const;
  void setPrecision(int precision);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void updateSelectionVisual();
  void applyPaletteColors();
  void updateFontForGeometry();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  Qt::Alignment alignment_ = Qt::AlignLeft | Qt::AlignVCenter;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  int precision_ = -1;
  std::array<QString, 4> channels_{};
};

