#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QMetaObject>

#include <functional>
#include <utility>

#include "pv_channel_manager.h"

class MessageButtonElement;

class DisplayWindow;

class MessageButtonRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit MessageButtonRuntime(MessageButtonElement *element);
  ~MessageButtonRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void handleChannelConnection(bool connected, const SharedChannelData &data);
  void handleChannelData(const SharedChannelData &data);
  void handleAccessRights(bool canRead, bool canWrite);
  void handlePress();
  void handleRelease();

  bool sendValue(const QString &value);
  bool sendStringValue(const QString &value);
  bool sendCharArrayValue(const QString &value);
  bool sendEnumValue(const QString &value);
  bool sendNumericValue(const QString &value);

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<MessageButtonElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  short fieldType_ = -1;
  long elementCount_ = 1;
  bool lastWriteAccess_ = false;
  short lastSeverity_ = 0;
  QStringList enumStrings_;
};

template <typename Func>
inline void MessageButtonRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<MessageButtonElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
