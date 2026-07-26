#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QStringList>

#include <utility>

#include "channel_subscription.h"
#include "runtime_utils.h"

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
  bool lastReadAccessKnown_ = false;
  bool lastReadAccess_ = false;
  bool lastWriteAccess_ = false;
  bool lastValueOutOfRange_ = false;
  QStringList enumStrings_;
};

template <typename Func>
inline void ChoiceButtonRuntime::invokeOnElement(Func &&func)
{
  RuntimeUtils::invokeOnObject(element_, std::forward<Func>(func));
}
