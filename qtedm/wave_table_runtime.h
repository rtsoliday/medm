#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cadef.h>

#include "pv_channel_manager.h"

class WaveTableElement;

class WaveTableRuntime : public QObject
{
  friend class DisplayWindow;

public:
  explicit WaveTableRuntime(WaveTableElement *element);
  ~WaveTableRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void resubscribe(chtype requestedType, long elementCount);
  void handleChannelConnection(bool connected, const SharedChannelData &data);
  void handleChannelData(const SharedChannelData &data);
  chtype subscriptionTypeForNativeType(short nativeFieldType) const;
  long subscriptionCountForNativeType(short nativeFieldType,
      long nativeElementCount) const;
  QVector<QString> formatNumericValues(const QVector<double> &values) const;
  QVector<QString> formatEnumValues(const QVector<double> &values) const;
  QVector<QString> formatStringValues(const QStringList &values) const;
  QVector<QString> formatCharValues(const QByteArray &bytes) const;
  QString formatNumeric(double value) const;
  QString formatEnum(double value) const;
  QString formatCharByte(unsigned char value) const;
  QString formatCharString(const QByteArray &bytes) const;
  int resolvedPrecision() const;
  int displayLimit(int receivedCount) const;
  void applyElementState(const QVector<QString> &values, long receivedCount);

  QPointer<WaveTableElement> element_;
  QString channelName_;
  SubscriptionHandle subscription_;
  bool started_ = false;
  bool connected_ = false;
  bool initialUpdateTracked_ = false;
  short nativeFieldType_ = -1;
  long nativeElementCount_ = 0;
  short lastSeverity_ = 3;
  int channelPrecision_ = -1;
  QString units_;
  QStringList enumStrings_;
  chtype requestedType_ = DBR_TIME_DOUBLE;
  long requestedCount_ = 0;
};
