#include "composite_runtime.h"
#include "composite_element.h"
#include "display_properties.h"

#include <QDebug>

CompositeRuntime::CompositeRuntime(CompositeElement *element)
  : QObject(nullptr)
  , element_(element)
{
  for (std::size_t i = 0; i < channels_.size(); ++i) {
    channels_[i].owner = this;
    channels_[i].index = static_cast<int>(i);
  }
}

CompositeRuntime::~CompositeRuntime()
{
  stop();
}

void CompositeRuntime::start()
{
  if (started_ || !element_) {
    return;
  }
  started_ = true;
  resetState();
  initializeChannels();
}

void CompositeRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  cleanupChannels();
  resetState();
}

void CompositeRuntime::resetState()
{
  /* Reset connection state */
  for (auto &channel : channels_) {
    channel.connected = false;
  }
}

void CompositeRuntime::initializeChannels()
{
  if (!element_) {
    return;
  }

  /* Only monitor channels if visibility mode is not static */
  const TextVisibilityMode visibilityMode = element_->visibilityMode();
  if (visibilityMode == TextVisibilityMode::kStatic) {
    return;
  }

  for (std::size_t i = 0; i < channels_.size(); ++i) {
    const QString channelName = element_->channel(static_cast<int>(i));
    if (channelName.isEmpty()) {
      continue;
    }
    channels_[i].name = channelName;
    subscribeChannel(channels_[i]);
  }
}

void CompositeRuntime::cleanupChannels()
{
  for (auto &channel : channels_) {
    unsubscribeChannel(channel);
  }
}

void CompositeRuntime::subscribeChannel(ChannelRuntime &channel)
{
  if (channel.name.isEmpty() || channel.channelId) {
    return;
  }

  const QByteArray nameBytes = channel.name.toUtf8();
  const int result = ca_create_channel(
      nameBytes.constData(),
      channelConnectionCallback,
      &channel,
      CA_PRIORITY_DEFAULT,
      &channel.channelId);

  if (result != ECA_NORMAL) {
    qWarning() << "CompositeRuntime: ca_create_channel failed for"
               << channel.name << ":" << ca_message(result);
    channel.channelId = nullptr;
  }
}

void CompositeRuntime::unsubscribeChannel(ChannelRuntime &channel)
{
  if (channel.channelId) {
    ca_clear_channel(channel.channelId);
    channel.channelId = nullptr;
  }
  channel.subscriptionId = nullptr;
  channel.connected = false;
  channel.name.clear();
}

void CompositeRuntime::handleChannelConnection(ChannelRuntime &channel,
    const connection_handler_args &args)
{
  const bool nowConnected = (args.op == CA_OP_CONN_UP);
  
  if (channel.connected == nowConnected) {
    return;
  }

  channel.connected = nowConnected;
  updateConnectionState();
}

void CompositeRuntime::updateConnectionState()
{
  if (!element_) {
    return;
  }

  /* Check if visibility mode is non-static */
  const TextVisibilityMode visibilityMode = element_->visibilityMode();
  if (visibilityMode == TextVisibilityMode::kStatic) {
    element_->setChannelConnected(true);
    return;
  }

  /* Check if any channel with a name is disconnected */
  bool allConnected = true;
  for (const auto &channel : channels_) {
    if (!channel.name.isEmpty() && !channel.connected) {
      allConnected = false;
      break;
    }
  }

  element_->setChannelConnected(allConnected);
}

void CompositeRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *channel = static_cast<ChannelRuntime *>(ca_puser(args.chid));
  if (!channel || !channel->owner) {
    return;
  }

  channel->owner->handleChannelConnection(*channel, args);
}
