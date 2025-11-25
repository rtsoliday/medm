#pragma once

#include <QObject>
#include <QPointer>
#include <QFont>
#include <QPalette>

#include <functional>
#include <memory>

#include "display_state.h"

class QMainWindow;

class MainWindowController : public QObject
{
public:
  MainWindowController(QMainWindow *mainWindow,
      std::weak_ptr<DisplayState> state);

  using DisplayWindowFactory =
      std::function<DisplayWindow *(std::weak_ptr<DisplayState>)>;
  using DisplayWindowRegistrar =
      std::function<void(DisplayWindow *)>;

  void setDisplayWindowFactory(DisplayWindowFactory factory);
  void setDisplayWindowRegistrar(DisplayWindowRegistrar registrar);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void closeAllDisplays();
  void handleDroppedFiles(const QStringList &filePaths);

  QPointer<QMainWindow> mainWindow_;
  std::weak_ptr<DisplayState> state_;
  bool closing_ = false;
  DisplayWindowFactory displayWindowFactory_;
  DisplayWindowRegistrar displayWindowRegistrar_;
};

