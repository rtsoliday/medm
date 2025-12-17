#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>

#include <utility>

#include "shared_channel_manager.h"
#include "startup_timing.h"

class SliderElement;

class DisplayWindow;

/* Runtime for slider control widget.
 * Uses SharedChannelManager for connection sharing with other widgets
 * monitoring the same PV. Handles reading values and writing user input. */
class SliderRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit SliderRuntime(SliderElement *element);
  ~SliderRuntime() override;

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

  QPointer<SliderElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  double lastValue_ = 0.0;
  bool hasLastValue_ = false;
  short lastSeverity_ = 0;
  bool hasControlInfo_ = false;
  bool lastWriteAccess_ = false;
  bool initialUpdateTracked_ = false;
};

template <typename Func>
inline void SliderRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<SliderElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
