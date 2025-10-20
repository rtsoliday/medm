#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include <cadef.h>

#include <utility>

class TextEntryElement;

class DisplayWindow;

class TextEntryRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit TextEntryRuntime(TextEntryElement *element);
  ~TextEntryRuntime() override;

  void start();
  void stop();

private:
  enum class ValueKind
  {
    kNone,
    kString,
    kEnum,
    kCharArray,
    kNumeric,
  };

  void resetRuntimeState();
  void subscribe();
  void unsubscribe();
  void requestControlInfo();
  void handleConnectionEvent(const connection_handler_args &args);
  void handleValueEvent(const event_handler_args &args);
  void handleControlInfo(const event_handler_args &args);
  void handleAccessRightsEvent(const access_rights_handler_args &args);
  void handleActivation(const QString &text);
  void updateWriteAccess();
  void updateElementDisplay();
  int resolvedPrecision() const;
  QString formatNumeric(double value, int precision) const;
  QString formatCharArray(const QByteArray &bytes) const;
  bool parseNumericInput(const QString &text, double &value) const;
  bool parseSexagesimal(const QString &text, double &value) const;
  bool parseEnumInput(const QString &text, short &value) const;
  bool parseCharArrayInput(const QString &text, QByteArray &bytes) const;

  template <typename Func>
  void invokeOnElement(Func &&func);

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);
  static void accessRightsCallback(struct access_rights_handler_args args);

  QPointer<TextEntryElement> element_;
  QString channelName_;
  chid channelId_ = nullptr;
  evid subscriptionId_ = nullptr;
  bool started_ = false;
  bool connected_ = false;
  short fieldType_ = -1;
  long elementCount_ = 1;
  ValueKind valueKind_ = ValueKind::kNone;
  double lastNumericValue_ = 0.0;
  bool hasNumericValue_ = false;
  QString lastStringValue_;
  bool hasStringValue_ = false;
  short lastEnumValue_ = 0;
  short lastSeverity_ = 0;
  QStringList enumStrings_;
  int channelPrecision_ = -1;
  double controlLow_ = 0.0;
  double controlHigh_ = 0.0;
  bool hasControlLimits_ = false;
  bool lastWriteAccess_ = false;
};

template <typename Func>
inline void TextEntryRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<TextEntryElement> target = element_;
  QMetaObject::invokeMethod(element_.data(),
      [target, func = std::forward<Func>(func)]() mutable {
        if (!target) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
