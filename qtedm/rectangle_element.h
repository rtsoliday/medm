#pragma once

#include <array>

#include <QColor>
#include <QPaintEvent>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class RectangleElement : public QWidget
{
public:
  explicit RectangleElement(QWidget *parent = nullptr);

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
  void setLineWidthFromAdl(int width);
  bool shouldSerializeLineWidth() const;

  int adlLineWidth() const;
  void setAdlLineWidth(int width, bool hasProperty);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);

  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeVisible(bool visible);
  void setRuntimeSeverity(short severity);

  void setVisible(bool visible) override;
  void setGeometry(const QRect &rect);
  using QWidget::setGeometry;

  void initializeFromAdlGeometry(const QRect &geometry,
      const QSize &adlSize);
  void setGeometryWithoutTracking(const QRect &geometry);
  QRect geometryForSerialization() const;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor defaultForegroundColor() const;
  QColor effectiveForegroundColor() const;
  void applyRuntimeVisibility();
  void updateExecuteState();

  bool selected_ = false;
  QColor color_;
  RectangleFill fill_ = RectangleFill::kOutline;
  RectangleLineStyle lineStyle_ = RectangleLineStyle::kSolid;
  int lineWidth_ = 1;
  int adlLineWidth_ = 0;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  bool executeMode_ = false;
  bool designModeVisible_ = true;
  bool runtimeConnected_ = false;
  bool runtimeVisible_ = true;
  short runtimeSeverity_ = 0;
  bool suppressGeometryTracking_ = false;
  bool hasOriginalAdlSize_ = false;
  QSize originalAdlSize_;
  bool sizeEdited_ = false;
  bool suppressLineWidthTracking_ = false;
  bool lineWidthEdited_ = false;
  bool hasAdlLineWidthProperty_ = false;
};

