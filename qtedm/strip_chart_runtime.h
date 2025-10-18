#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <utility>

#include <cadef.h>

#include "display_properties.h"

class StripChartElement;

class StripChartRuntime : public QObject
{
public:
  explicit StripChartRuntime(StripChartElement *element);
  ~StripChartRuntime() override;

  void start();
  void stop();

private:
  struct PenState
  {
    QString channelName;
    chid channelId = nullptr;
    evid subscriptionId = nullptr;
    bool connected = false;
    short fieldType = -1;
  };

  struct PenContext
  {
    StripChartRuntime *runtime = nullptr;
    int index = -1;
  };

  void subscribePen(int index);
  void requestControlInfo(int index);
  void handleConnectionEvent(int index, const connection_handler_args &args);
  void handleValueEvent(int index, const event_handler_args &args);
  void handleControlInfo(int index, const event_handler_args &args);
  void resetPen(int index);
  void unsubscribePen(int index);

  template <typename Func>
  void invokeOnElement(Func &&func);

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);

  QPointer<StripChartElement> element_;
  std::array<PenState, kStripChartPenCount> pens_{};
  std::array<PenContext, kStripChartPenCount> contexts_{};
  bool started_ = false;
};

template <typename Func>
inline void StripChartRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<StripChartElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
