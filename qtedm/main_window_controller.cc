#include "main_window_controller.h"

#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

#include "display_window.h"

MainWindowController::MainWindowController(QMainWindow *mainWindow,
    std::weak_ptr<DisplayState> state)
  : QObject(mainWindow)
  , mainWindow_(mainWindow)
  , state_(std::move(state))
{
  if (mainWindow_) {
    mainWindow_->setAcceptDrops(true);
  }
  if (QCoreApplication *core = QCoreApplication::instance()) {
    QObject::connect(core, &QCoreApplication::aboutToQuit, this,
        [this]() { closeAllDisplays(); });
  }
}

void MainWindowController::setDisplayWindowFactory(DisplayWindowFactory factory)
{
  displayWindowFactory_ = std::move(factory);
}

void MainWindowController::setDisplayWindowRegistrar(DisplayWindowRegistrar registrar)
{
  displayWindowRegistrar_ = std::move(registrar);
}

bool MainWindowController::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == mainWindow_) {
    switch (event->type()) {
    case QEvent::Close:
      closeAllDisplays();
      break;
    case QEvent::DragEnter: {
      auto *dragEvent = static_cast<QDragEnterEvent *>(event);
      if (dragEvent->mimeData()->hasUrls()) {
        const QList<QUrl> urls = dragEvent->mimeData()->urls();
        for (const QUrl &url : urls) {
          if (url.isLocalFile()) {
            const QString path = url.toLocalFile();
            if (path.endsWith(QLatin1String(".adl"), Qt::CaseInsensitive)) {
              dragEvent->acceptProposedAction();
              return true;
            }
          }
        }
      }
      break;
    }
    case QEvent::Drop: {
      auto *dropEvent = static_cast<QDropEvent *>(event);
      if (dropEvent->mimeData()->hasUrls()) {
        QStringList adlFiles;
        const QList<QUrl> urls = dropEvent->mimeData()->urls();
        for (const QUrl &url : urls) {
          if (url.isLocalFile()) {
            const QString path = url.toLocalFile();
            if (path.endsWith(QLatin1String(".adl"), Qt::CaseInsensitive)) {
              adlFiles.append(path);
            }
          }
        }
        if (!adlFiles.isEmpty()) {
          dropEvent->acceptProposedAction();
          handleDroppedFiles(adlFiles);
          return true;
        }
      }
      break;
    }
    default:
      break;
    }
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

void MainWindowController::handleDroppedFiles(const QStringList &filePaths)
{
  if (!displayWindowFactory_ || !displayWindowRegistrar_) {
    return;
  }

  auto state = state_.lock();
  if (!state) {
    return;
  }

  for (const QString &filePath : filePaths) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
      if (mainWindow_) {
        QMessageBox::warning(mainWindow_,
            QStringLiteral("Open Display"),
            QStringLiteral("File not found:\n%1").arg(filePath));
      }
      continue;
    }

    DisplayWindow *displayWin = displayWindowFactory_(
        std::weak_ptr<DisplayState>(state));
    if (!displayWin) {
      continue;
    }

    QString errorMessage;
    if (!displayWin->loadFromFile(filePath, &errorMessage)) {
      const QString message = errorMessage.isEmpty()
          ? QStringLiteral("Failed to open display:\n%1").arg(filePath)
          : errorMessage;
      if (mainWindow_) {
        QMessageBox::critical(mainWindow_,
            QStringLiteral("Open Display"), message);
      }
      delete displayWin;
      continue;
    }

    displayWindowRegistrar_(displayWin);
  }
}

