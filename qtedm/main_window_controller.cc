#include "main_window_controller.h"

#include <QCoreApplication>
#include <QEvent>
#include <QMainWindow>

#include "display_window.h"

MainWindowController::MainWindowController(QMainWindow *mainWindow,
    std::weak_ptr<DisplayState> state)
  : QObject(mainWindow)
  , mainWindow_(mainWindow)
  , state_(std::move(state))
{
  if (QCoreApplication *core = QCoreApplication::instance()) {
    QObject::connect(core, &QCoreApplication::aboutToQuit, this,
        [this]() { closeAllDisplays(); });
  }
}

bool MainWindowController::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == mainWindow_ && event->type() == QEvent::Close) {
    closeAllDisplays();
  }
  return QObject::eventFilter(watched, event);
}

void MainWindowController::closeAllDisplays()
{
  if (closing_) {
    return;
  }
  closing_ = true;
  if (auto state = state_.lock()) {
    const auto displays = state->displays;
    for (const auto &display : displays) {
      if (!display.isNull()) {
        display->close();
      }
    }
    state->createTool = CreateTool::kNone;
  }
  closing_ = false;
}

