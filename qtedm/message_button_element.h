#pragma once

#include <functional>

#include <QColor>
#include <QString>
#include <QWidget>

#include "display_properties.h"

class QPushButton;
class QPaintEvent;
class QResizeEvent;
class QEvent;

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

  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  void setRuntimeConnected(bool connected);
  void setRuntimeSeverity(short severity);
  void setRuntimeWriteAccess(bool writeAccess);

  void setPressCallback(const std::function<void()> &callback);
  void setReleaseCallback(const std::function<void()> &callback);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;

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

  bool selected_ = false;
  QPushButton *button_ = nullptr;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  QString label_;
  QString pressMessage_;
  QString releaseMessage_;
  QString channel_;
  bool executeMode_ = false;
  bool runtimeConnected_ = false;
  bool runtimeWriteAccess_ = false;
  short runtimeSeverity_ = 0;
  std::function<void()> pressCallback_;
  std::function<void()> releaseCallback_;
};
