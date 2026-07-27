#pragma once

#include <functional>

#include <QColor>
#include <QString>
#include <QWidget>

#include "text_properties.h"

class QPushButton;
class QPaintEvent;
class QResizeEvent;
class QEvent;
class QMouseEvent;

class MessageButtonElement : public QWidget
{
public:
  explicit MessageButtonElement(QWidget *parent = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  QString label() const;
  void setLabel(const QString &label);

  QString pressMessage() const;
  void setPressMessage(const QString &message);

  QString releaseMessage() const;
  void setReleaseMessage(const QString &message);

  QString channel() const;
  void setChannel(const QString &channel);

  bool isQtedmToggle() const;
  void setQtedmToggle(bool enabled);
  QString offValue() const;
  void setOffValue(const QString &value);
  QString onValue() const;
  void setOnValue(const QString &value);
  QString offLabel() const;
  void setOffLabel(const QString &label);
  QString onLabel() const;
  void setOnLabel(const QString &label);
  bool confirmationRequired() const;
  void setConfirmationRequired(bool required);

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeSeverity(short severity);
  void setRuntimeReadAccess(bool readAccess);
  void setRuntimeWriteAccess(bool writeAccess);
  void setRuntimeToggleState(bool on, bool known);

  void setPressCallback(const std::function<void()> &callback);
  void setReleaseCallback(const std::function<void()> &callback);
  bool handleChildMouseEvent(QMouseEvent *event) const;

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  void applyPaletteColors();
  void updateSelectionVisual();
  void updateButtonFont();
  void updateButtonState();
  void handleButtonPressed();
  void handleButtonReleased();
  QString effectiveLabel() const;
  QColor effectiveForeground() const;
  QColor effectiveBackground() const;
  bool forwardMouseEventToParent(QMouseEvent *event) const;

  bool selected_ = false;
  QPushButton *button_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  QString label_;
  QString pressMessage_;
  QString releaseMessage_;
  QString channel_;
  bool qtedmToggle_ = false;
  QString offValue_ = QStringLiteral("0");
  QString onValue_ = QStringLiteral("1");
  QString offLabel_ = QStringLiteral("Off");
  QString onLabel_ = QStringLiteral("On");
  bool confirmationRequired_ = false;
  bool runtimeToggleOn_ = false;
  bool runtimeToggleKnown_ = false;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeReadAccess_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  std::function<void()> pressCallback_;
  std::function<void()> releaseCallback_;
};
