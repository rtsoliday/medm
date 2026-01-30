#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include "display_properties.h"
#include "pv_channel_manager.h"

class HeatmapElement;

class HeatmapRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit HeatmapRuntime(HeatmapElement *element);
  ~HeatmapRuntime() override;

  void start();
  void stop();

private:
  struct ChannelState
  {
    QString name;
    SubscriptionHandle subscription;
    bool connected = false;
    short fieldType = -1;
    long elementCount = 0;
  };

  void resetRuntimeState();
  void subscribeDataChannel();
  void subscribeDimensionChannel(ChannelState &state, const QString &name);
  void handleDataConnection(bool connected, const SharedChannelData &data);
  void handleDataValue(const SharedChannelData &data);
  void handleDimensionConnection(ChannelState &state, bool connected);
  void handleDimensionValue(ChannelState &state, int value);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<HeatmapElement> element_;
  ChannelState dataChannel_;
  ChannelState xDimensionChannel_;
  ChannelState yDimensionChannel_;
  bool started_ = false;
  short lastSeverity_ = -1;
  int runtimeXDimension_ = 0;
  int runtimeYDimension_ = 0;
};

template <typename Func>
inline void HeatmapRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<HeatmapElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
