#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <array>
#include <utility>

#include "display_properties.h"
#include "pv_channel_manager.h"

class StripChartElement;

class DisplayWindow;

class StripChartRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit StripChartRuntime(StripChartElement *element);
  ~StripChartRuntime() override;

  void start();
  void stop();

private:
  struct PenState
  {
    QString channelName;
    SubscriptionHandle subscription;
    bool connected = false;
    short fieldType = -1;
    long elementCount = 1;
  };

  void subscribePen(int index);
  void handleConnectionEvent(int index, bool connected,
      const SharedChannelData &data);
  void handleValueEvent(int index, const SharedChannelData &data);
  void resetPen(int index);
  void unsubscribePen(int index);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<StripChartElement> element_;
  std::array<PenState, kStripChartPenCount> pens_{};
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
