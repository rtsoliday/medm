#pragma once

#include <array>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <cadef.h>

class CompositeElement;

class DisplayWindow;

class CompositeRuntime : public QObject
{
  friend class DisplayWindow;
public:
  explicit CompositeRuntime(CompositeElement *element);
  ~CompositeRuntime() override;

  void start();
  void stop();

private:
  struct ChannelRuntime
  {
    CompositeRuntime *owner = nullptr;
    int index = 0;
    QString name;
    chid channelId = nullptr;
    evid subscriptionId = nullptr;
    chtype subscriptionType = DBR_TIME_DOUBLE;
    short fieldType = -1;
    long elementCount = 1;
    bool connected = false;
    bool hasValue = false;
    double value = 0.0;
    short severity = 0;
  };

  void resetState();
  void initializeChannels();
  void cleanupChannels();
  void subscribeChannel(ChannelRuntime &channel);
  void unsubscribeChannel(ChannelRuntime &channel);
  void handleChannelConnection(ChannelRuntime &channel,
      const connection_handler_args &args);
  void handleChannelValue(ChannelRuntime &channel,
      const event_handler_args &args);
  void evaluateVisibility();
  bool evaluateCalcExpression(double &result) const;

  static void channelConnectionCallback(struct connection_handler_args args);
  static void valueEventCallback(struct event_handler_args args);

  CompositeElement *element_ = nullptr;
  std::array<ChannelRuntime, 5> channels_{};
  QByteArray calcPostfix_;
  bool calcValid_ = false;
  bool started_ = false;
};
