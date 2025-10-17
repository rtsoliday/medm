#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QMetaObject>

#include <cadef.h>

#include <functional>
#include <utility>

class MessageButtonElement;

class MessageButtonRuntime : public QObject
{
public:
  explicit MessageButtonRuntime(MessageButtonElement *element);
  ~MessageButtonRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void subscribe();
  void unsubscribe();
  void requestControlInfo();
  void updateWriteAccess();

  void handleConnectionEvent(const connection_handler_args &args);
  void handleValueEvent(const event_handler_args &args);
  void handleControlInfo(const event_handler_args &args);
  void handleAccessRightsEvent(const access_rights_handler_args &args);
  void handlePress();
  void handleRelease();

  bool sendValue(const QString &value);
  bool sendStringValue(const QString &value);
  bool sendCharArrayValue(const QString &value);
  bool sendEnumValue(const QString &value);
  bool sendNumericValue(const QString &value);

  template <typename Func>
  void invokeOnElement(Func &&func);

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);
  static void accessRightsCallback(struct access_rights_handler_args args);

  QPointer<MessageButtonElement> element_;
  QString channelName_;
  chid channelId_ = nullptr;
  evid subscriptionId_ = nullptr;
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
