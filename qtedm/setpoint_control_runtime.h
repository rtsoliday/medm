#pragma once

#include <QObject>
#include <QMetaObject>
#include <QPointer>
#include <QString>

#include <utility>

#include "pv_channel_manager.h"

class SetpointControlElement;

class SetpointControlRuntime : public QObject
{
public:
  explicit SetpointControlRuntime(SetpointControlElement *element);
  ~SetpointControlRuntime() override;

  void start();
  void stop();

private:
  void resetRuntimeState();
  void handleSetpointConnection(bool connected, const SharedChannelData &data);
  void handleSetpointData(const SharedChannelData &data);
  void handleReadbackConnection(bool connected, const SharedChannelData &data);
  void handleReadbackData(const SharedChannelData &data);
  void handleAccessRights(bool canRead, bool canWrite);
  void handleActivation(const QString &text);
  void updateMetadataFromSetpoint(const SharedChannelData &data);
  int resolvedPrecision() const;
  QString formatNumeric(double value, int precision) const;
  bool parseNumericInput(const QString &text, double &value) const;
  bool validateAgainstLimits(double value, QString &message) const;
  bool isSupportedNumeric(const SharedChannelData &data) const;

  template <typename Func>
  void invokeOnElement(Func &&func);

  QPointer<SetpointControlElement> element_;
  QString setpointChannel_;
  QString readbackChannel_;
  SubscriptionHandle setpointSubscription_;
  SubscriptionHandle readbackSubscription_;
  bool started_ = false;
  bool setpointConnected_ = false;
  bool readbackConnected_ = false;
  bool setpointNumeric_ = false;
  bool readbackNumeric_ = false;
  bool writeAccess_ = false;
  int channelPrecision_ = -1;
  double controlLow_ = 0.0;
  double controlHigh_ = 1.0;
  bool hasControlLimits_ = false;
};

template <typename Func>
inline void SetpointControlRuntime::invokeOnElement(Func &&func)
{
  if (!element_) {
    return;
  }
  QPointer<SetpointControlElement> target = element_;
  QPointer<SetpointControlRuntime> self(this);
  QMetaObject::invokeMethod(element_.data(),
      [target, self, func = std::forward<Func>(func)]() mutable {
        if (!target || !self) {
          return;
        }
        func(target.data());
      },
      Qt::QueuedConnection);
}
