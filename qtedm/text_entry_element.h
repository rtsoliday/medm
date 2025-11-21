#pragma once

#include <functional>

#include <QColor>
#include <QWidget>

#include "display_properties.h"

class QLineEdit;
class QEvent;
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
  bool hasExplicitLimitsBlock() const;
  void setHasExplicitLimitsBlock(bool hasBlock);
  bool hasExplicitLimitsData() const;
  void setHasExplicitLimitsData(bool hasData);
  bool hasExplicitLowLimitData() const;
  void setHasExplicitLowLimitData(bool hasData);
  bool hasExplicitHighLimitData() const;
  void setHasExplicitHighLimitData(bool hasData);
  bool hasExplicitPrecisionData() const;
  void setHasExplicitPrecisionData(bool hasData);

  QString channel() const;
  void setChannel(const QString &value);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeWriteAccess(bool writeAccess);
  void setRuntimeSeverity(short severity);
  void setRuntimeText(const QString &text);
  void setRuntimeLimits(double low, double high);
  void setRuntimePrecision(int precision);
  void clearRuntimeState();

  void setActivationCallback(
      const std::function<void(const QString &)> &callback);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void applyPaletteColors();
  void updateSelectionVisual();
  void updateFontForGeometry();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  QColor effectiveForegroundColor() const;
  QColor effectiveBackgroundColor() const;
  void updateLineEditState();
  void applyRuntimeTextToLineEdit();

  bool selected_ = false;
  QLineEdit *lineEdit_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  PvLimits limits_{};
  bool hasExplicitLimitsBlock_ = false;
  bool hasExplicitLimitsData_ = false;
  bool hasExplicitLowLimitData_ = false;
  bool hasExplicitHighLimitData_ = false;
  bool hasExplicitPrecisionData_ = false;
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  QString runtimeText_;
  QString designModeText_;
  bool updateAllowed_ = true;
  bool hasPendingRuntimeText_ = false;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 0.0;
  bool runtimeLimitsValid_ = false;
  int runtimePrecision_ = -1;
  std::function<void(const QString &)> activationCallback_;
};
