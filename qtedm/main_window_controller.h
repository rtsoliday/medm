#pragma once

#include <QObject>
#include <QPointer>

#include <memory>

#include "display_state.h"

class QMainWindow;

class MainWindowController : public QObject
{
public:
  MainWindowController(QMainWindow *mainWindow,
      std::weak_ptr<DisplayState> state);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void closeAllDisplays();

  QPointer<QMainWindow> mainWindow_;
  std::weak_ptr<DisplayState> state_;
  bool closing_ = false;
};

