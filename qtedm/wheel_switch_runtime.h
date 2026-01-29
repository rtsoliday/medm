#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>

#include <utility>

#include "pv_channel_manager.h"
#include "startup_timing.h"

class WheelSwitchElement;

class DisplayWindow;

class WheelSwitchRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit WheelSwitchRuntime(WheelSwitchElement *element);
  ~WheelSwitchRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void handleChannelConnection(bool connected);
  void handleChannelData(const SharedChannelData &data);
  void handleAccessRights(bool canRead, bool canWrite);
  void handleActivation(double value);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<WheelSwitchElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  double lastValue_ = 0.0;
  bool hasLastValue_ = false;
  short lastSeverity_ = 0;
  bool lastWriteAccess_ = false;
  bool initialUpdateTracked_ = false;
};

template <typename Func>
inline void WheelSwitchRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<WheelSwitchElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
