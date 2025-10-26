#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>

#include <type_traits>
#include <utility>

#include <cadef.h>

#include "element_runtime_traits.h"

class DisplayWindow;

template <typename ElementType>
class SingleChannelMonitorRuntimeBase : public QObject
{
  friend class DisplayWindow;

  // Compile-time interface validation
  // TODO: Re-enable after debugging trait detection
  /*
  static_assert(ElementTraits::HasSingleChannelInterface<ElementType>::value,
                "ElementType must provide RuntimeChannelInterface methods (single-channel variant)");
  static_assert(ElementTraits::HasValueInterface<ElementType>::value,
                "ElementType must provide RuntimeValueInterface methods");
  static_assert(ElementTraits::HasLimitsInterface<ElementType>::value,
                "ElementType must provide RuntimeLimitsInterface methods");
  */

public:
  explicit SingleChannelMonitorRuntimeBase(ElementType *element);
  virtual ~SingleChannelMonitorRuntimeBase();

  void start();
  void stop();

protected:
  void resetRuntimeState();
  void subscribe();
  void unsubscribe();
  void requestControlInfo();
  void handleConnectionEvent(const connection_handler_args &args);
  void handleValueEvent(const event_handler_args &args);
  void handleControlInfo(const event_handler_args &args);

  template <typename Func>
  void invokeOnElement(Func &&func);

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);

  QPointer<ElementType> element_;
  QString channelName_;
  chid channelId_ = nullptr;
  evid subscriptionId_ = nullptr;
  bool started_ = false;
  bool connected_ = false;
  short fieldType_ = -1;
  long elementCount_ = 1;
  double lastValue_ = 0.0;
  bool hasLastValue_ = false;
  short lastSeverity_ = 3;
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
