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
    bool connected = false;
  };

  void resetState();
  void initializeChannels();
  void cleanupChannels();
  void subscribeChannel(ChannelRuntime &channel);
  void unsubscribeChannel(ChannelRuntime &channel);
  void handleChannelConnection(ChannelRuntime &channel,
      const connection_handler_args &args);
  void updateConnectionState();

  static void channelConnectionCallback(struct connection_handler_args args);

  CompositeElement *element_ = nullptr;
  std::array<ChannelRuntime, 5> channels_{};
  bool started_ = false;
};
