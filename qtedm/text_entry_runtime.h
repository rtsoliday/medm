#pragma once

#include <QObject>
#include <QPointer>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include "startup_timing.h"

#include <utility>

#include "pv_channel_manager.h"

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
  void resubscribe(int requestedType, long elementCount);
  void handleChannelConnection(bool connected, const SharedChannelData &data);
  void handleChannelData(const SharedChannelData &data);
  void handleAccessRights(bool canRead, bool canWrite);
  void handleActivation(const QString &text);
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

  QPointer<TextEntryElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
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
  bool initialUpdateTracked_ = false;
  int requestedType_ = 0;
  long requestedCount_ = 0;
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
