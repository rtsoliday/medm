#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QStringList>

#include <utility>

#include "pv_channel_manager.h"

class MenuElement;

class DisplayWindow;

class MenuRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit MenuRuntime(MenuElement *element);
  ~MenuRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void handleChannelConnection(bool connected);
  void handleChannelData(const SharedChannelData &data);
  void handleAccessRights(bool canRead, bool canWrite);
  void handleActivation(int value);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<MenuElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  short lastSeverity_ = 0;
  short lastValue_ = -1;
  bool lastWriteAccess_ = false;
  QStringList enumStrings_;
};

template <typename Func>
inline void MenuRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<MenuElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
