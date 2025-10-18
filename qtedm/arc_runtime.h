#pragma once

#include <array>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cadef.h>

class ArcElement;

class ArcRuntime : public QObject
{
public:
  explicit ArcRuntime(ArcElement *element);
  ~ArcRuntime() override;

  void start();
  void stop();

private:
  struct ChannelRuntime
  {
    ArcRuntime *owner = nullptr;
    int index = 0;
    QString name;
    chid channelId = nullptr;
    evid subscriptionId = nullptr;
    chtype subscriptionType = DBR_TIME_DOUBLE;
    short fieldType = -1;
    long elementCount = 1;
    bool connected = false;
    bool hasValue = false;
    bool controlInfoRequested = false;
    bool hasControlInfo = false;
    double value = 0.0;
    short severity = 0;
    short status = 0;
    double hopr = 0.0;
    double lopr = 0.0;
    short precision = -1;
  };

  void resetState();
  void initializeChannels();
  void cleanupChannels();
  void subscribeChannel(ChannelRuntime &channel);
  void unsubscribeChannel(ChannelRuntime &channel);
  void requestControlInfo(ChannelRuntime &channel);
  void handleChannelConnection(ChannelRuntime &channel,
      const connection_handler_args &args);
  void handleChannelValue(ChannelRuntime &channel,
      const event_handler_args &args);
  void handleChannelControlInfo(ChannelRuntime &channel,
      const event_handler_args &args);
  void evaluateState();
  bool evaluateCalcExpression(double &result) const;

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);
  static void controlInfoCallback(struct event_handler_args args);

  ArcElement *element_ = nullptr;
  std::array<ChannelRuntime, 5> channels_{};
  QByteArray calcPostfix_;
  bool calcValid_ = false;
  bool started_ = false;
};
