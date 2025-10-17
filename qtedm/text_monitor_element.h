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

  PvLimitSource precisionSource() const;
  void setPrecisionSource(PvLimitSource source);
  int precisionDefault() const;
  void setPrecisionDefault(int precision);
  const PvLimits &limits() const;
  void setLimits(const PvLimits &limits);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeText(const QString &text);
  void setRuntimeConnected(bool connected);
  void setRuntimeSeverity(short severity);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void updateSelectionVisual();
  void applyPaletteColors();
  void updateFontForGeometry();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  QColor effectiveForegroundColor() const;
  QColor effectiveBackgroundColor() const;

  bool selected_ = false;
  QColor foregroundColor_;
  QColor backgroundColor_;
  Qt::Alignment alignment_ = Qt::AlignLeft | Qt::AlignTop;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  PvLimits limits_{};
  std::array<QString, 5> channels_{};
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  short runtimeSeverity_ = 0;
  QString designModeText_;
};

