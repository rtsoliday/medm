#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>

#include <utility>

#include <cadef.h>

class BarMonitorElement;

class DisplayWindow;

class BarMonitorRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit BarMonitorRuntime(BarMonitorElement *element);
  ~BarMonitorRuntime() override;

  void start();
  void stop();

private:
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

  QPointer<BarMonitorElement> element_;
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

template <typename Func>
inline void BarMonitorRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<BarMonitorElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
