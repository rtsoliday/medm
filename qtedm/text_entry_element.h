#pragma once

#include <QColor>
#include <QWidget>

#include "display_properties.h"

class QLineEdit;
class QPaintEvent;
class QResizeEvent;

class TextEntryElement : public QWidget
{
public:
  explicit TextEntryElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextMonitorFormat format() const;
  void setFormat(TextMonitorFormat format);

  int precision() const;
  void setPrecision(int precision);

  PvLimitSource precisionSource() const;
  void setPrecisionSource(PvLimitSource source);
  int precisionDefault() const;
  void setPrecisionDefault(int precision);

  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);

  QString channel() const;
  void setChannel(const QString &value);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void applyPaletteColors();
  void updateSelectionVisual();
  void updateFontForGeometry();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;

  bool selected_ = false;
  QLineEdit *lineEdit_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  PvLimits limits_{};
  QString channel_;
};

