#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QStringList>

#include <utility>

#include "pv_channel_manager.h"

class ChoiceButtonElement;

class DisplayWindow;

class ChoiceButtonRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit ChoiceButtonRuntime(ChoiceButtonElement *element);
  ~ChoiceButtonRuntime() override;

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

  QPointer<ChoiceButtonElement> element_;
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
inline void ChoiceButtonRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<ChoiceButtonElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
