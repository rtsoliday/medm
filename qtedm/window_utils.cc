#include "window_utils.h"

#include <QCursor>
#include <QGuiApplication>
#include <QLayout>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QString>

#include <algorithm>
#include <utility>

#include "display_window.h"

namespace {

#if defined(Q_OS_MAC)
constexpr int kWindowExposureRetryMilliseconds = 10;
constexpr int kWindowExposureRetryCount = 100;

void runWhenWindowExposedImpl(QPointer<QWidget> window,
    std::function<void()> callback, int retriesRemaining)
{
  if (!window || !window->isVisible()) {
    return;
  }

  QWindow *nativeWindow = window->windowHandle();
  if ((nativeWindow && nativeWindow->isExposed()) || retriesRemaining <= 0) {
    callback();
    return;
  }

  QTimer::singleShot(kWindowExposureRetryMilliseconds, window,
      [window, callback = std::move(callback), retriesRemaining]() mutable {
        runWhenWindowExposedImpl(window, std::move(callback),
            retriesRemaining - 1);
      });
}
#endif

} // namespace

void runWhenWindowExposed(QWidget *window, std::function<void()> callback)
{
  if (!window || !callback) {
    return;
  }

#if defined(Q_OS_MAC)
  runWhenWindowExposedImpl(QPointer<QWidget>(window), std::move(callback),
      kWindowExposureRetryCount);
#else
  callback();
#endif
}

void activateWindowWhenExposed(QWidget *window)
{
  runWhenWindowExposed(window, [target = QPointer<QWidget>(window)]() {
    if (!target || !target->isVisible() || target->isMinimized()) {
      return;
    }
    target->raise();
    target->activateWindow();
    if (QWindow *nativeWindow = target->windowHandle()) {
      nativeWindow->requestActivate();
    }
    target->update();
  });
}

void positionWindowTopRight(QWidget *window, int rightMargin, int topMargin)
{
  if (!window) {
    return;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect screenGeometry = screen->availableGeometry();
  QSize frameSize = window->frameGeometry().size();
  if (frameSize.isEmpty()) {
    frameSize = window->size();
  }

  const int xOffset = std::max(0,
      screenGeometry.width() - frameSize.width() - rightMargin);
  const int yOffset = std::max(0, topMargin);
  const int x = screenGeometry.x() + xOffset;
  const int y = screenGeometry.y() + yOffset;

  window->move(x, y);
}

void centerWindowOnScreen(QWidget *window)
{
  if (!window) {
    return;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    if (QWidget *parent = window->parentWidget()) {
      screen = parent->screen();
    }
  }
  if (!screen) {
    screen = QGuiApplication::screenAt(QCursor::pos());
  }
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect screenGeometry = screen->availableGeometry();
  QSize targetSize = window->size();
  if (targetSize.isEmpty()) {
    targetSize = window->sizeHint();
  }

  const int x = screenGeometry.x() + std::max(0,
      (screenGeometry.width() - targetSize.width()) / 2);
  const int y = screenGeometry.y() + std::max(0,
      (screenGeometry.height() - targetSize.height()) / 2);

  window->move(x, y);
}

bool fitWindowToAvailableScreen(QWidget *window,
    const QSize &requestedContentSize)
{
  if (!window) {
    return false;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    screen = QGuiApplication::screenAt(window->frameGeometry().center());
  }
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return false;
  }

  const QSize availableSize = screen->availableGeometry().size();
  const QSize currentWindowSize = window->size();
  const QSize requestedSize = requestedContentSize.isValid()
      ? requestedContentSize : currentWindowSize;
  if (!availableSize.isValid() || !currentWindowSize.isValid()
      || !requestedSize.isValid()
      || (requestedSize.width() <= availableSize.width()
          && requestedSize.height() <= availableSize.height())) {
    return false;
  }

  QSize targetSize = requestedSize;
  targetSize.scale(availableSize, Qt::KeepAspectRatio);
  targetSize = targetSize.expandedTo(window->minimumSize());
  targetSize.setWidth(std::min(targetSize.width(), availableSize.width()));
  targetSize.setHeight(std::min(targetSize.height(), availableSize.height()));
  if (!targetSize.isValid() || targetSize == currentWindowSize) {
    return false;
  }

  window->resize(targetSize);
  if (window->layout()) {
    window->layout()->activate();
  }
  return window->size() != currentWindowSize;
}

bool isParentWindowInPvInfoMode(QWidget *widget)
{
  if (!widget) {
    return false;
  }
  QWidget *target = widget->window();
  if (!target) {
    return false;
  }
  auto *displayWindow = dynamic_cast<DisplayWindow *>(target);
  if (!displayWindow) {
    return false;
  }
  return displayWindow->isPvInfoPickingActive();
}

bool isParentWindowInPvLimitsMode(QWidget *widget)
{
  if (!widget) {
    return false;
  }
  QWidget *target = widget->window();
  if (!target) {
    return false;
  }
  auto *displayWindow = dynamic_cast<DisplayWindow *>(target);
  if (!displayWindow) {
    return false;
  }
  return displayWindow->isPvLimitsPickingActive();
}
