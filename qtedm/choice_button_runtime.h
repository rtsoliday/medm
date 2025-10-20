#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QStringList>

#include <cadef.h>

#include <utility>

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
  void subscribe();
  void unsubscribe();
  void requestControlInfo();
  void handleConnectionEvent(const connection_handler_args &args);
  void handleValueEvent(const event_handler_args &args);
  void handleControlInfo(const event_handler_args &args);
  void handleAccessRightsEvent(const access_rights_handler_args &args);
  void handleActivation(int value);
  void updateWriteAccess();

  template <typename Func>
  void invokeOnElement(Func &&func);

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);
  static void accessRightsCallback(struct access_rights_handler_args args);

  QPointer<ChoiceButtonElement> element_;
  QString channelName_;
  chid channelId_ = nullptr;
  evid subscriptionId_ = nullptr;
  bool started_ = false;
  bool connected_ = false;
  short fieldType_ = -1;
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
