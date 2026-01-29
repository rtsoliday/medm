#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QtGlobal>

#include <utility>

#include "pv_channel_manager.h"

class ByteMonitorElement;

class DisplayWindow;

class ByteMonitorRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit ByteMonitorRuntime(ByteMonitorElement *element);
  ~ByteMonitorRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void handleChannelConnection(bool connected, const SharedChannelData &data);
  void handleChannelData(const SharedChannelData &data);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<ByteMonitorElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  short fieldType_ = -1;
  long elementCount_ = 1;
  quint32 lastValue_ = 0u;
  bool hasLastValue_ = false;
  short lastSeverity_ = 3;
};

template <typename Func>
inline void ByteMonitorRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<ByteMonitorElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
