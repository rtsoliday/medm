#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <cadef.h>

struct dbr_ctrl_enum;
class QByteArray;

#include "display_properties.h"

class TextMonitorElement;

class TextMonitorRuntime : public QObject
{
public:
  explicit TextMonitorRuntime(TextMonitorElement *element);
  ~TextMonitorRuntime() override;

  void start();
  void stop();

private:
  enum class ValueKind {
    kNone,
    kNumeric,
    kString,
    kEnum,
    kCharArray
  };

  void resetRuntimeState();
  void subscribe();
  void unsubscribe();
  void requestControlInfo();
  void handleConnectionEvent(const connection_handler_args &args);
  void handleValueEvent(const event_handler_args &args);
  void handleControlInfo(const event_handler_args &args);
  void updateElementDisplay();
  int resolvedPrecision() const;
  QString formatNumeric(double value, int precision) const;
  QString formatEnumValue(short value) const;
  QString formatCharArray(const QByteArray &bytes) const;

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);

  TextMonitorElement *element_ = nullptr;
  QString channelName_;
  chid channelId_ = nullptr;
  evid subscriptionId_ = nullptr;
  chtype subscriptionType_ = DBR_TIME_DOUBLE;
  short fieldType_ = -1;
  long elementCount_ = 1;
  bool connected_ = false;
  bool started_ = false;
  ValueKind valueKind_ = ValueKind::kNone;
  double lastNumericValue_ = 0.0;
  bool hasNumericValue_ = false;
  QString lastStringValue_;
  bool hasStringValue_ = false;
  short lastEnumValue_ = 0;
  short lastSeverity_ = 0;
  short channelPrecision_ = -1;
  QStringList enumStrings_;
};
