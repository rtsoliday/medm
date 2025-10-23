#include "image_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "image_element.h"

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {
constexpr short kInvalidSeverity = 3;
constexpr double kVisibilityEpsilon = 1e-12;
constexpr int kCalcInputCount = 12;

void appendNullTerminator(QByteArray &bytes)
{
  if (bytes.isEmpty() || bytes.back() != '\0') {
    bytes.append('\0');
  }
}

} // namespace

ImageRuntime::ImageRuntime(ImageElement *element)
  : QObject(element)
  , element_(element)
{
  for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
    channels_[i].owner = this;
    channels_[i].index = i;
  }
}

ImageRuntime::~ImageRuntime()
{
  stop();
}

void ImageRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  ChannelAccessContext &context = ChannelAccessContext::instance();
  context.ensureInitialized();
  if (!context.isInitialized()) {
    qWarning() << "Channel Access context not available";
    return;
  }

  resetState();
  started_ = true;

  if (element_->visibilityMode() == TextVisibilityMode::kCalc) {
    const QString calcExpr = element_->visibilityCalc().trimmed();
    if (!calcExpr.isEmpty()) {
      QByteArray infix = calcExpr.toLatin1();
      visibilityCalcPostfix_.resize(512);
      visibilityCalcPostfix_.fill('\0');
      short error = 0;
      long status = postfix(infix.data(), visibilityCalcPostfix_.data(), &error);
      if (status == 0) {
        visibilityCalcValid_ = true;
      } else {
        visibilityCalcValid_ = false;
        qWarning() << "Invalid visibility calc expression for image element:"
                   << calcExpr << "(error" << error << ')';
      }
    }
  }

  const QString imageCalcExpr = element_->calc().trimmed();
  hasImageCalcExpression_ = !imageCalcExpr.isEmpty();
  animate_ = imageCalcExpr.isEmpty() && element_->frameCount() > 1;
  if (hasImageCalcExpression_) {
    QByteArray infix = imageCalcExpr.toLatin1();
    imageCalcPostfix_.resize(512);
    imageCalcPostfix_.fill('\0');
    short error = 0;
    long status = postfix(infix.data(), imageCalcPostfix_.data(), &error);
    if (status == 0) {
      imageCalcValid_ = true;
    } else {
      imageCalcValid_ = false;
      qWarning() << "Invalid image calc expression:" << imageCalcExpr
                 << "(error" << error << ')';
    }
  }

  /* Check if any channel is specified (mimic MEDM behavior) */
  bool hasChannel = false;
  for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
    if (!element_->channel(i).trimmed().isEmpty()) {
      hasChannel = true;
      break;
    }
  }

  /* Channels are needed only if a channel is specified AND
   * (color mode is dynamic OR visibility mode is dynamic OR has image calc) */
  channelsNeeded_ = hasChannel
      && ((element_->colorMode() != TextColorMode::kStatic)
          || (element_->visibilityMode() != TextVisibilityMode::kStatic)
          || hasImageCalcExpression_);

  initializeChannels();
  evaluateState();
}

void ImageRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  cleanupChannels();
  resetState();
}

void ImageRuntime::resetState()
{
  visibilityCalcPostfix_.clear();
  visibilityCalcValid_ = false;
  imageCalcPostfix_.clear();
  imageCalcValid_ = false;
  hasImageCalcExpression_ = false;
  animate_ = false;
  channelsNeeded_ = true;

  for (auto &channel : channels_) {
    channel.name.clear();
    channel.channelId = nullptr;
    channel.subscriptionId = nullptr;
    channel.subscriptionType = DBR_TIME_DOUBLE;
    channel.fieldType = -1;
    channel.elementCount = 1;
    channel.connected = false;
    channel.hasValue = false;
    channel.controlInfoRequested = false;
    channel.hasControlInfo = false;
    channel.value = 0.0;
    channel.severity = 0;
    channel.status = 0;
    channel.hopr = 0.0;
    channel.lopr = 0.0;
    channel.precision = -1;
  }

  if (element_) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeVisible(true);
    element_->setRuntimeAnimate(false);
    element_->setRuntimeFrameValid(element_->frameCount() > 0);
    element_->setRuntimeFrameIndex(0);
  }
}

void ImageRuntime::initializeChannels()
{
  if (!element_) {
    return;
  }
  if (!channelsNeeded_) {
    for (auto &channel : channels_) {
      channel.name.clear();
    }
    return;
  }

  for (auto &channel : channels_) {
    channel.name = element_->channel(channel.index).trimmed();
    if (channel.name.isEmpty()) {
      continue;
    }

    QByteArray channelBytes = channel.name.toLatin1();
    int status = ca_create_channel(channelBytes.constData(),
        &ImageRuntime::channelConnectionCallback, nullptr,
        CA_PRIORITY_DEFAULT, &channel.channelId);
    if (status != ECA_NORMAL) {
      qWarning() << "Failed to create Channel Access channel for"
                 << channel.name << ':' << ca_message(status);
      channel.channelId = nullptr;
      continue;
    }
    ca_set_puser(channel.channelId, &channel);
  }

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void ImageRuntime::cleanupChannels()
{
  for (auto &channel : channels_) {
    if (channel.subscriptionId) {
      ca_clear_subscription(channel.subscriptionId);
      channel.subscriptionId = nullptr;
    }
    if (channel.channelId) {
      ca_set_puser(channel.channelId, nullptr);
      ca_clear_channel(channel.channelId);
      channel.channelId = nullptr;
    }
  }

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void ImageRuntime::subscribeChannel(ChannelRuntime &channel)
{
  if (channel.subscriptionId || !channel.channelId) {
    return;
  }

  switch (channel.fieldType) {
  case DBR_STRING:
    channel.subscriptionType = DBR_TIME_STRING;
    break;
  case DBR_ENUM:
    channel.subscriptionType = DBR_TIME_ENUM;
    break;
  case DBR_CHAR:
    channel.subscriptionType = DBR_TIME_CHAR;
    break;
  case DBR_SHORT:
    channel.subscriptionType = DBR_TIME_SHORT;
    break;
  case DBR_LONG:
    channel.subscriptionType = DBR_TIME_LONG;
    break;
  case DBR_FLOAT:
    channel.subscriptionType = DBR_TIME_FLOAT;
    break;
  case DBR_DOUBLE:
  default:
    channel.subscriptionType = DBR_TIME_DOUBLE;
    break;
  }

  long count = std::max<long>(channel.elementCount, 1);
  int status = ca_create_subscription(channel.subscriptionType, count,
      channel.channelId, DBE_VALUE | DBE_ALARM,
      &ImageRuntime::valueEventCallback, &channel,
      &channel.subscriptionId);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channel.name << ':'
               << ca_message(status);
    channel.subscriptionId = nullptr;
    return;
  }
  channel.hasValue = false;

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void ImageRuntime::unsubscribeChannel(ChannelRuntime &channel)
{
  if (channel.subscriptionId) {
    ca_clear_subscription(channel.subscriptionId);
    channel.subscriptionId = nullptr;
  }
  channel.hasValue = false;
}

void ImageRuntime::requestControlInfo(ChannelRuntime &channel)
{
  if (channel.index != 0 || !channel.channelId) {
    return;
  }
  if (channel.controlInfoRequested) {
    return;
  }

  chtype controlType = 0;
  switch (channel.fieldType) {
  case DBR_CHAR:
  case DBR_SHORT:
  case DBR_LONG:
  case DBR_FLOAT:
  case DBR_DOUBLE:
    controlType = DBR_CTRL_DOUBLE;
    break;
  default:
    break;
  }

  channel.controlInfoRequested = true;
  if (controlType == 0) {
    channel.hasControlInfo = false;
    return;
  }

  int status = ca_array_get_callback(controlType, 1, channel.channelId,
      &ImageRuntime::controlInfoCallback, &channel);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to request control info for" << channel.name
               << ':' << ca_message(status);
    return;
  }

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void ImageRuntime::handleChannelConnection(ChannelRuntime &channel,
    const connection_handler_args &args)
{
  if (!started_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    channel.connected = true;
    channel.fieldType = ca_field_type(args.chid);
    channel.elementCount = std::max<long>(ca_element_count(args.chid), 1);
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
    channel.status = 0;
    subscribeChannel(channel);
    requestControlInfo(channel);
  } else if (args.op == CA_OP_CONN_DOWN) {
    channel.connected = false;
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
    channel.status = 0;
    unsubscribeChannel(channel);
  }

  evaluateState();
}

void ImageRuntime::handleChannelValue(ChannelRuntime &channel,
    const event_handler_args &args)
{
  if (!started_) {
    return;
  }
  if (!args.dbr) {
    return;
  }

  double newValue = channel.value;
  short newSeverity = channel.severity;
  short newStatus = channel.status;
  bool processed = false;

  switch (args.type) {
  case DBR_TIME_DOUBLE: {
    const auto *value = static_cast<const dbr_time_double *>(args.dbr);
    newValue = value->value;
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  case DBR_TIME_FLOAT: {
    const auto *value = static_cast<const dbr_time_float *>(args.dbr);
    newValue = value->value;
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  case DBR_TIME_LONG: {
    const auto *value = static_cast<const dbr_time_long *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  case DBR_TIME_SHORT: {
    const auto *value = static_cast<const dbr_time_short *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  case DBR_TIME_CHAR: {
    const auto *value = static_cast<const dbr_time_char *>(args.dbr);
    if (channel.elementCount > 1) {
      QByteArray bytes(reinterpret_cast<const char *>(value->value),
          channel.elementCount);
      appendNullTerminator(bytes);
      char *end = nullptr;
      double parsed = std::strtod(bytes.constData(), &end);
      if (end && *end == '\0') {
        newValue = parsed;
      } else {
        newValue = 0.0;
      }
    } else {
      newValue = static_cast<double>(value->value);
    }
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  case DBR_TIME_ENUM: {
    const auto *value = static_cast<const dbr_time_enum *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  case DBR_TIME_STRING: {
    const auto *value = static_cast<const dbr_time_string *>(args.dbr);
    QByteArray bytes(value->value, static_cast<int>(sizeof(value->value)));
    appendNullTerminator(bytes);
    char *end = nullptr;
    double parsed = std::strtod(bytes.constData(), &end);
    if (end && *end == '\0') {
      newValue = parsed;
    } else {
      newValue = 0.0;
    }
    newSeverity = value->severity;
    newStatus = value->status;
    processed = true;
    break;
  }
  default:
    break;
  }

  if (!processed) {
    return;
  }

  channel.value = newValue;
  channel.severity = newSeverity;
  channel.status = newStatus;
  channel.hasValue = true;

  if (channel.index == 0 && element_) {
    element_->setRuntimeSeverity(newSeverity);
  }

  evaluateState();
}

void ImageRuntime::handleChannelControlInfo(ChannelRuntime &channel,
    const event_handler_args &args)
{
  if (!started_) {
    return;
  }
  if (!args.dbr) {
    return;
  }

  if (args.type == DBR_CTRL_DOUBLE) {
    const auto *info = static_cast<const dbr_ctrl_double *>(args.dbr);
    channel.hopr = info->upper_ctrl_limit;
    channel.lopr = info->lower_ctrl_limit;
    channel.precision = info->precision;
    channel.hasControlInfo = true;
  }

  evaluateState();
}

void ImageRuntime::evaluateState()
{
  if (!element_) {
    return;
  }

  bool anyChannels = false;
  bool allConnected = true;
  for (const auto &channel : channels_) {
    if (channel.name.isEmpty()) {
      continue;
    }
    anyChannels = true;
    if (!channel.connected) {
      allConnected = false;
      break;
    }
  }

  if (!anyChannels) {
    element_->setRuntimeConnected(true);
    element_->setRuntimeSeverity(0);
    element_->setRuntimeVisible(true);
    if (animate_ && element_->frameCount() > 1) {
      element_->setRuntimeAnimate(true);
      element_->setRuntimeFrameValid(element_->frameCount() > 0);
    } else {
      element_->setRuntimeAnimate(false);
      evaluateFrameSelection();
    }
    return;
  }

  if (!allConnected) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(kInvalidSeverity);
    element_->setRuntimeVisible(true);
    element_->setRuntimeAnimate(false);
    element_->setRuntimeFrameValid(false);
    return;
  }

  ChannelRuntime &primary = channels_.front();
  element_->setRuntimeConnected(true);
  element_->setRuntimeSeverity(primary.severity);

  bool visible = true;
  switch (element_->visibilityMode()) {
  case TextVisibilityMode::kStatic:
    visible = true;
    break;
  case TextVisibilityMode::kIfNotZero:
    visible = std::fabs(primary.value) > kVisibilityEpsilon;
    break;
  case TextVisibilityMode::kIfZero:
    visible = std::fabs(primary.value) <= kVisibilityEpsilon;
    break;
  case TextVisibilityMode::kCalc: {
    double result = 0.0;
    if (evaluateVisibilityCalc(result)) {
      visible = std::fabs(result) > kVisibilityEpsilon;
    } else {
      visible = false;
    }
    break;
  }
  }

  element_->setRuntimeVisible(visible);

  if (animate_ && element_->frameCount() > 1) {
    element_->setRuntimeAnimate(true);
    element_->setRuntimeFrameValid(true);
    return;
  }

  element_->setRuntimeAnimate(false);
  evaluateFrameSelection();
}

void ImageRuntime::evaluateFrameSelection()
{
  if (!element_) {
    return;
  }

  const int count = element_->frameCount();
  if (count <= 0) {
    element_->setRuntimeFrameValid(false);
    return;
  }

  if (!hasImageCalcExpression_) {
    element_->setRuntimeFrameIndex(0);
    element_->setRuntimeFrameValid(true);
    return;
  }

  if (!imageCalcValid_) {
    element_->setRuntimeFrameValid(false);
    return;
  }

  double result = 0.0;
  if (!evaluateImageCalc(result)) {
    element_->setRuntimeFrameValid(false);
    return;
  }

  double clamped = std::max(0.0, std::min(result,
      static_cast<double>(count - 1)));
  int frameIndex = static_cast<int>(std::floor(clamped + 0.5));
  frameIndex = std::max(0, std::min(frameIndex, count - 1));

  element_->setRuntimeFrameIndex(frameIndex);
  element_->setRuntimeFrameValid(true);
}

bool ImageRuntime::evaluateVisibilityCalc(double &result) const
{
  if (!visibilityCalcValid_ || visibilityCalcPostfix_.isEmpty()) {
    return false;
  }

  double args[kCalcInputCount] = {0.0};
  args[0] = channels_[0].value;
  args[1] = channels_[1].value;
  args[2] = channels_[2].value;
  args[3] = channels_[3].value;
  args[4] = 0.0;
  args[5] = 0.0;

  const ChannelRuntime &primary = channels_[0];
  args[6] = static_cast<double>(std::max<long>(primary.elementCount, 1));
  args[7] = primary.hopr;
  args[8] = static_cast<double>(primary.status);
  args[9] = static_cast<double>(primary.severity);
  args[10] = static_cast<double>(primary.precision >= 0 ? primary.precision : 0);
  args[11] = primary.lopr;

  long status = calcPerform(args, &result,
      const_cast<char *>(visibilityCalcPostfix_.constData()));
  return status == 0;
}

bool ImageRuntime::evaluateImageCalc(double &result) const
{
  if (!imageCalcValid_ || imageCalcPostfix_.isEmpty()) {
    return false;
  }

  double args[kCalcInputCount] = {0.0};
  args[0] = channels_[0].value;
  args[1] = channels_[1].value;
  args[2] = channels_[2].value;
  args[3] = channels_[3].value;
  args[4] = 0.0;
  args[5] = 0.0;

  const ChannelRuntime &primary = channels_[0];
  args[6] = static_cast<double>(std::max<long>(primary.elementCount, 1));
  args[7] = primary.hopr;
  args[8] = static_cast<double>(primary.status);
  args[9] = static_cast<double>(primary.severity);
  args[10] = static_cast<double>(primary.precision >= 0 ? primary.precision : 0);
  args[11] = primary.lopr;

  long status = calcPerform(args, &result,
      const_cast<char *>(imageCalcPostfix_.constData()));
  return status == 0;
}

void ImageRuntime::channelConnectionCallback(connection_handler_args args)
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

void ImageRuntime::valueEventCallback(event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *channel = static_cast<ChannelRuntime *>(args.usr);
  if (!channel || !channel->owner) {
    return;
  }
  channel->owner->handleChannelValue(*channel, args);
}

void ImageRuntime::controlInfoCallback(event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *channel = static_cast<ChannelRuntime *>(args.usr);
  if (!channel || !channel->owner) {
    return;
  }
  channel->owner->handleChannelControlInfo(*channel, args);
}
