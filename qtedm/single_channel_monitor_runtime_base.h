#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>

#include <type_traits>
#include <utility>

#include <cadef.h>

#include "element_runtime_traits.h"
#include "shared_channel_manager.h"
#include "startup_timing.h"

class DisplayWindow;

/* Base class template for single-channel monitor runtimes (Meter, Bar, Scale).
 *
 * Now uses SharedChannelManager for connection sharing. These monitors
 * all use DBR_TIME_DOUBLE with element count 1, so monitors of the same
 * PV will share a single CA channel. */
template <typename ElementType>
class SingleChannelMonitorRuntimeBase : public QObject
{
  friend class DisplayWindow;

public:
  explicit SingleChannelMonitorRuntimeBase(ElementType *element);
  virtual ~SingleChannelMonitorRuntimeBase();

  void start();
  void stop();

protected:
  void resetRuntimeState();
  void handleChannelConnection(bool connected);
  void handleChannelData(const SharedChannelData &data);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<ElementType> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  double lastValue_ = 0.0;
  bool hasLastValue_ = false;
  short lastSeverity_ = 3;
  bool hasControlInfo_ = false;
  bool initialUpdateTracked_ = false;
};

template <typename ElementType>
template <typename Func>
inline void SingleChannelMonitorRuntimeBase<ElementType>::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<ElementType> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
