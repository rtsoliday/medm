#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <cadef.h>

#include "display_properties.h"
#include "shared_channel_manager.h"

class TextMonitorElement;

class DisplayWindow;

/* Runtime component for TextMonitorElement that handles EPICS Channel Access.
 *
 * Now uses SharedChannelManager for connection sharing. Because text monitors
 * need specific DBR types (STRING, ENUM, CHAR, DOUBLE) depending on the
 * native field type, different monitors of the same PV may or may not share
 * a channel depending on field type. */
class TextMonitorRuntime : public QObject
{
  friend class DisplayWindow;
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
  void handleChannelConnection(bool connected, const SharedChannelData &data);
  void handleChannelData(const SharedChannelData &data);
  void updateElementDisplay();
  int resolvedPrecision() const;
  QString formatNumeric(double value, int precision) const;
  QString formatEnumValue(short value) const;
  QString formatCharArray(const QByteArray &bytes) const;
  chtype determineSubscriptionType(short nativeFieldType) const;

  TextMonitorElement *element_ = nullptr;
  QString channelName_;
  SubscriptionHandle subscription_;
  short nativeFieldType_ = -1;
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
