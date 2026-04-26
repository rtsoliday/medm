#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include <cadef.h>

#include "pv_channel_manager.h"

class PvTableElement;

class PvTableRuntime : public QObject
{
  friend class DisplayWindow;

public:
  explicit PvTableRuntime(PvTableElement *element);
  ~PvTableRuntime() override;

  void start();
  void stop();

private:
  enum class ValueKind
  {
    kNone,
    kNumeric,
    kString,
    kEnum,
    kCharArray
  };

  struct RowSubscription
  {
    QString channel;
    SubscriptionHandle subscription;
    bool connected = false;
    short nativeFieldType = -1;
    long elementCount = 1;
    ValueKind valueKind = ValueKind::kNone;
    double lastNumericValue = 0.0;
    bool hasNumericValue = false;
    QString lastStringValue;
    short lastEnumValue = 0;
    short lastSeverity = 3;
    short channelPrecision = -1;
    QStringList enumStrings;
    QString units;
  };

  void resetRuntimeState();
  void subscribeRow(int row);
  chtype determineSubscriptionType(short nativeFieldType, long elementCount) const;
  void handleChannelConnection(int row, bool connected,
      const SharedChannelData &data);
  void handleChannelData(int row, const SharedChannelData &data);
  QString formatRowValue(const RowSubscription &row) const;
  QString formatNumeric(double value, int precision) const;
  QString formatCharArray(const QByteArray &bytes) const;
  QString formatEnumValue(const RowSubscription &row) const;
  int resolvedPrecision(const RowSubscription &row) const;
  void applyRowState(int row);

  QPointer<PvTableElement> element_;
  std::vector<std::unique_ptr<RowSubscription>> rows_;
  bool started_ = false;
};
