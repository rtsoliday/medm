#include "composite_runtime.h"
#include "composite_element.h"
#include "display_properties.h"
#include "channel_access_context.h"
#include "statistics_tracker.h"

#include <QDebug>

#include <cmath>

#include <db_access.h>

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {

constexpr int kCalcInputCount = 12;
constexpr double kVisibilityEpsilon = 1e-9;

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

}

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

  /* Parse calc expression if visibility mode is calc */
  if (element_->visibilityMode() == TextVisibilityMode::kCalc) {
    const QString calcExpr = element_->visibilityCalc().trimmed();
    if (!calcExpr.isEmpty()) {
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
        qWarning() << "CompositeRuntime: Invalid calc expression:"
                   << calcExpr;
      }
    }
  }

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
  calcPostfix_.clear();
  calcValid_ = false;
}

void CompositeRuntime::resetState()
{
  /* Reset connection state */
  for (auto &channel : channels_) {
    channel.connected = false;
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
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
    /* Static mode - always visible and connected */
    element_->setChannelConnected(true);
    return;
  }

  /* Create channels for all non-empty channel names */
  for (std::size_t i = 0; i < channels_.size(); ++i) {
    const QString channelName = element_->channel(static_cast<int>(i));
    if (channelName.isEmpty()) {
      continue;
    }
    channels_[i].name = channelName;
    
    /* Create channel */
    const QByteArray nameBytes = channelName.toUtf8();
    const int result = ca_create_channel(
        nameBytes.constData(),
        channelConnectionCallback,
        nullptr,
        CA_PRIORITY_DEFAULT,
        &channels_[i].channelId);

    if (result != ECA_NORMAL) {
      qWarning() << "CompositeRuntime: ca_create_channel failed for"
                 << channelName << ":" << ca_message(result);
      channels_[i].channelId = nullptr;
    } else {
      ca_set_puser(channels_[i].channelId, &channels_[i]);
    }
  }

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
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
  if (!channel.channelId || channel.subscriptionId) {
    return;
  }

  /* Determine subscription type based on field type */
  switch (channel.fieldType) {
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
      &CompositeRuntime::valueEventCallback, &channel,
      &channel.subscriptionId);
  if (status != ECA_NORMAL) {
    qWarning() << "CompositeRuntime: Failed to subscribe to" << channel.name
               << ':' << ca_message(status);
    channel.subscriptionId = nullptr;
    return;
  }
  channel.hasValue = false;

  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void CompositeRuntime::unsubscribeChannel(ChannelRuntime &channel)
{
  if (channel.subscriptionId) {
    ca_clear_subscription(channel.subscriptionId);
    channel.subscriptionId = nullptr;
  }
  if (channel.channelId) {
    ca_clear_channel(channel.channelId);
    channel.channelId = nullptr;
  }
  channel.connected = false;
  channel.hasValue = false;
  channel.name.clear();
}

void CompositeRuntime::handleChannelConnection(ChannelRuntime &channel,
    const connection_handler_args &args)
{
  if (!started_) {
    return;
  }

  auto &stats = StatisticsTracker::instance();

  if (args.op == CA_OP_CONN_UP) {
    const bool wasConnected = channel.connected;
    channel.connected = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    channel.fieldType = ca_field_type(args.chid);
    channel.elementCount = std::max<long>(ca_element_count(args.chid), 1);
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
    subscribeChannel(channel);
  } else if (args.op == CA_OP_CONN_DOWN) {
    const bool wasConnected = channel.connected;
    channel.connected = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
    if (channel.subscriptionId) {
      ca_clear_subscription(channel.subscriptionId);
      channel.subscriptionId = nullptr;
    }
  }

  evaluateVisibility();
}

void CompositeRuntime::handleChannelValue(ChannelRuntime &channel,
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
  bool processed = false;

  switch (args.type) {
  case DBR_TIME_DOUBLE: {
    const auto *value = static_cast<const dbr_time_double *>(args.dbr);
    newValue = value->value;
    newSeverity = value->severity;
    processed = true;
    break;
  }
  case DBR_TIME_FLOAT: {
    const auto *value = static_cast<const dbr_time_float *>(args.dbr);
    newValue = value->value;
    newSeverity = value->severity;
    processed = true;
    break;
  }
  case DBR_TIME_LONG: {
    const auto *value = static_cast<const dbr_time_long *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    processed = true;
    break;
  }
  case DBR_TIME_SHORT: {
    const auto *value = static_cast<const dbr_time_short *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    processed = true;
    break;
  }
  case DBR_TIME_CHAR: {
    const auto *value = static_cast<const dbr_time_char *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    processed = true;
    break;
  }
  case DBR_TIME_ENUM: {
    const auto *value = static_cast<const dbr_time_enum *>(args.dbr);
    newValue = static_cast<double>(value->value);
    newSeverity = value->severity;
    processed = true;
    break;
  }
  case DBR_TIME_STRING: {
    const auto *value = static_cast<const dbr_time_string *>(args.dbr);
    QByteArray bytes(value->value, static_cast<int>(sizeof(value->value)));
    char *end = nullptr;
    double parsed = std::strtod(bytes.constData(), &end);
    if (end && *end == '\0') {
      newValue = parsed;
    } else {
      newValue = 0.0;
    }
    newSeverity = value->severity;
    processed = true;
    break;
  }
  default:
    break;
  }

  if (!processed) {
    return;
  }

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

  channel.value = newValue;
  channel.severity = newSeverity;
  channel.hasValue = true;

  evaluateVisibility();
}

void CompositeRuntime::evaluateVisibility()
{
  if (!element_) {
    return;
  }

  const TextVisibilityMode visibilityMode = element_->visibilityMode();

  /* Static mode - always visible and connected */
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

  /* If disconnected, hide the composite */
  if (!allConnected) {
    element_->setChannelConnected(false);
    return;
  }

  /* All channels are connected - now evaluate visibility based on values */
  const ChannelRuntime &primary = channels_[0];
  
  bool visible = true;
  switch (visibilityMode) {
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

  /* Update element - show if connected AND visible */
  element_->setChannelConnected(allConnected && visible);
}

bool CompositeRuntime::evaluateCalcExpression(double &result) const
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
  args[6] = 0.0;
  args[7] = 0.0;
  args[8] = 0.0;
  args[9] = 0.0;
  args[10] = 0.0;
  args[11] = 0.0;

  long status = calcPerform(args, &result,
      const_cast<char *>(calcPostfix_.constData()));
  return status == 0;
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

void CompositeRuntime::valueEventCallback(event_handler_args args)
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
