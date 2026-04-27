#pragma once

#include <functional>

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class QLabel;
class QLineEdit;
class QEvent;
class QPaintEvent;
class QResizeEvent;

class SetpointControlElement : public QWidget
{
public:
  explicit SetpointControlElement(QWidget *parent = nullptr);

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
  double displayLowLimit() const;
  double displayHighLimit() const;

  QString label() const;
  void setLabel(const QString &label);

  QString setpointChannel() const;
  void setSetpointChannel(const QString &channel);
  QString channel() const;
  void setChannel(const QString &channel);

  QString readbackChannel() const;
  void setReadbackChannel(const QString &channel);

  SetpointToleranceMode toleranceMode() const;
  void setToleranceMode(SetpointToleranceMode mode);
  QString toleranceModeString() const;
  void setToleranceModeString(const QString &mode);

  double tolerance() const;
  void setTolerance(double tolerance);

  bool showReadback() const;
  void setShowReadback(bool show);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setSetpointConnected(bool connected);
  void setSetpointWriteAccess(bool writeAccess);
  void setSetpointSeverity(short severity);
  void setSetpointValue(double value, const QString &text);
  void setSetpointMetadata(double low, double high, int precision,
      const QString &units);
  void setReadbackConnected(bool connected);
  void setReadbackSeverity(short severity);
  void setReadbackValue(double value, const QString &text);
  void setRuntimeNotice(const QString &notice);
  void acceptAppliedValue(double value, const QString &text);
  void clearRuntimeState();

  bool runtimeSetpointConnected() const;
  bool runtimeReadbackConnected() const;
  bool runtimeWriteAccess() const;
  short runtimeSetpointSeverity() const;
  short runtimeReadbackSeverity() const;
  bool hasSetpointValue() const;
  bool hasReadbackValue() const;
  double runtimeSetpointValue() const;
  double runtimeReadbackValue() const;
  QString runtimeSetpointText() const;
  QString runtimeReadbackText() const;
  QString runtimeStatusText() const;
  bool isDirty() const;
  bool isPending() const;
  bool isInTolerance() const;

  void setActivationCallback(
      const std::function<void(const QString &)> &callback);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  bool eventFilter(QObject *watched, QEvent *event) override;

  void applyPaletteColors();
  void updateSelectionVisual();
  void updateLayoutState();
  void updateChildInteraction();
  void updateDisplayTexts();
  void updateStatusText();
  void updateFontForGeometry();
  void updateChildGeometry();
  void applyRuntimeSetpointToEditor();
  void applyReadbackStyle();
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  QColor effectiveForegroundColor() const;
  QColor effectiveBackgroundColor() const;
  QColor readbackBorderColor() const;
  bool hasEffectiveReadback() const;
  bool canCommitCurrentText() const;
  double toleranceTargetValue() const;
  QString displayLabelText() const;
  bool forwardMouseEventToParent(QEvent *event) const;

  bool selected_ = false;
  QLabel *labelWidget_ = nullptr;
  QLineEdit *setpointEdit_ = nullptr;
  QLineEdit *readbackEdit_ = nullptr;

  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kAlarm;
  TextMonitorFormat format_ = TextMonitorFormat::kDecimal;
  PvLimits limits_{};
  QString label_;
  QString setpointChannel_;
  QString readbackChannel_;
  SetpointToleranceMode toleranceMode_ = SetpointToleranceMode::kNone;
  double tolerance_ = 0.0;
  bool showReadback_ = true;

  bool executeMode_ = false;
  bool runtimeSetpointConnected_ = false;
  bool runtimeReadbackConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSetpointSeverity_ = 3;
  short runtimeReadbackSeverity_ = 3;
  bool hasSetpointValue_ = false;
  bool hasReadbackValue_ = false;
  double setpointValue_ = 0.0;
  double readbackValue_ = 0.0;
  QString setpointText_;
  QString readbackText_;
  QString units_;
  double runtimeLow_ = 0.0;
  double runtimeHigh_ = 1.0;
  bool runtimeLimitsValid_ = false;
  int runtimePrecision_ = -1;
  bool dirty_ = false;
  bool pending_ = false;
  bool hasAppliedTarget_ = false;
  double appliedTarget_ = 0.0;
  QString runtimeNotice_;
  QString statusText_;
  std::function<void(const QString &)> activationCallback_;
};
