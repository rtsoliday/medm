#include "rectangle_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "rectangle_element.h"

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

/* Normalize calc expression to MEDM calc engine syntax.
 * MEDM calc uses single '=' for equality (not '==') and '#' for inequality (not '!=').
 * This function converts modern C-style operators to MEDM syntax. */
QString normalizeCalcExpression(const QString &expr)
{
  QString result = expr;
  /* Replace != with # (must do this before replacing ==) */
  result.replace("!=", "#");
  /* Replace == with = */
  result.replace("==", "=");
  return result;
}

} // namespace

RectangleRuntime::RectangleRuntime(RectangleElement *element)
  : QObject(element)
  , element_(element)
{
  for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
    channels_[i].owner = this;
    channels_[i].index = i;
  }
}

RectangleRuntime::~RectangleRuntime()
{
  stop();
}

void RectangleRuntime::start()
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

  /* Check if any channel is specified (mimic MEDM behavior) */
  bool hasChannel = false;
  for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
    if (!element_->channel(i).trimmed().isEmpty()) {
      hasChannel = true;
      break;
    }
  }

  /* Channels are needed only if a channel is specified AND
   * (color mode is dynamic OR visibility mode is dynamic) */
  channelsNeeded_ = hasChannel
      && ((element_->colorMode() != TextColorMode::kStatic)
          || (element_->visibilityMode() != TextVisibilityMode::kStatic));

  if (element_->visibilityMode() == TextVisibilityMode::kCalc) {
    const QString calcExpr = element_->visibilityCalc().trimmed();
    if (!calcExpr.isEmpty()) {
      /* Normalize expression: convert == to = and != to # for MEDM calc engine */
      QString normalized = normalizeCalcExpression(calcExpr);
      QByteArray infix = normalized.toLatin1();
      calcPostfix_.resize(512);
      calcPostfix_.fill('\0');
      short error = 0;
      long status = postfix(infix.data(), calcPostfix_.data(), &error);
      if (status == 0) {
        calcValid_ = true;
      } else {
        calcValid_ = false;
        qWarning() << "Invalid visibility calc expression for rectangle element:"
                   << calcExpr << "(error" << error << ')';
      }
    }
  }

  initializeChannels();
  evaluateState();
}

void RectangleRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  cleanupChannels();
  resetState();
}

void RectangleRuntime::resetState()
{
  calcPostfix_.clear();
  calcValid_ = false;
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
    element_->setRuntimeSeverity(0);
    element_->setRuntimeVisible(true);
  }
}

void RectangleRuntime::initializeChannels()
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
        &RectangleRuntime::channelConnectionCallback, nullptr,
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

void RectangleRuntime::cleanupChannels()
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

void RectangleRuntime::subscribeChannel(ChannelRuntime &channel)
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
      &RectangleRuntime::valueEventCallback, &channel,
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

void RectangleRuntime::unsubscribeChannel(ChannelRuntime &channel)
{
  if (channel.subscriptionId) {
    ca_clear_subscription(channel.subscriptionId);
    channel.subscriptionId = nullptr;
  }
  channel.hasValue = false;
}

void RectangleRuntime::requestControlInfo(ChannelRuntime &channel)
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
      &RectangleRuntime::controlInfoCallback, &channel);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to request control info for" << channel.name
               << ':' << ca_message(status);
    return;
  }

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void RectangleRuntime::handleChannelConnection(ChannelRuntime &channel,
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

void RectangleRuntime::handleChannelValue(ChannelRuntime &channel,
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

void RectangleRuntime::handleChannelControlInfo(ChannelRuntime &channel,
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

void RectangleRuntime::evaluateState()
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
    return;
  }

  if (!allConnected) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(kInvalidSeverity);
    element_->setRuntimeVisible(true);
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
    if (calcValid_ && evaluateCalcExpression(result)) {
      visible = std::fabs(result) > kVisibilityEpsilon;
    } else {
      visible = false;
    }
    break;
  }
  }

  element_->setRuntimeVisible(visible);
}

bool RectangleRuntime::evaluateCalcExpression(double &result) const
{
  if (!calcValid_ || calcPostfix_.isEmpty()) {
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
      const_cast<char *>(calcPostfix_.constData()));
  return status == 0;
}

void RectangleRuntime::channelConnectionCallback(connection_handler_args args)
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

void RectangleRuntime::valueEventCallback(event_handler_args args)
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

void RectangleRuntime::controlInfoCallback(event_handler_args args)
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
