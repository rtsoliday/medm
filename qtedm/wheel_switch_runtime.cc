#include "wheel_switch_runtime.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "runtime_utils.h"
#include "wheel_switch_element.h"
#include "statistics_tracker.h"

namespace {
using RuntimeUtils::isNumericFieldType;
using RuntimeUtils::kInvalidSeverity;

} // namespace

WheelSwitchRuntime::WheelSwitchRuntime(WheelSwitchElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

WheelSwitchRuntime::~WheelSwitchRuntime()
{
  stop();
}

void WheelSwitchRuntime::start()
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

  resetRuntimeState();
  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();

  channelName_ = element_->channel().trimmed();
  element_->setActivationCallback([this](double value) {
    handleActivation(value);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &WheelSwitchRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ':' << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  StatisticsTracker::instance().registerChannelCreated();

  ca_set_puser(channelId_, this);
  ca_replace_access_rights_event(channelId_,
      &WheelSwitchRuntime::accessRightsCallback);

  ca_flush_io();
}

void WheelSwitchRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  unsubscribe();
  if (element_) {
    element_->setActivationCallback(std::function<void(double)>());
  }
  resetRuntimeState();
}

void WheelSwitchRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  lastValue_ = 0.0;
  hasLastValue_ = false;
  lastSeverity_ = kInvalidSeverity;
  lastWriteAccess_ = false;

  if (element_) {
    invokeOnElement([](WheelSwitchElement *element) {
      element->clearRuntimeState();
    });
  }
}

void WheelSwitchRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  int status = ca_create_subscription(DBR_TIME_DOUBLE, 1, channelId_,
      DBE_VALUE | DBE_ALARM,
      &WheelSwitchRuntime::valueEventCallback, this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ':'
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }

  ca_flush_io();
}

void WheelSwitchRuntime::unsubscribe()
{
  auto &stats = StatisticsTracker::instance();
  if (subscriptionId_) {
    ca_clear_subscription(subscriptionId_);
    subscriptionId_ = nullptr;
  }
  if (channelId_) {
    if (connected_) {
      stats.registerChannelDisconnected();
      connected_ = false;
    }
    ca_replace_access_rights_event(channelId_, nullptr);
    ca_clear_channel(channelId_);
    stats.registerChannelDestroyed();
    channelId_ = nullptr;
  }
  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void WheelSwitchRuntime::requestControlInfo()
{
  if (!channelId_ || !isNumericFieldType(fieldType_)) {
    return;
  }

  int status = ca_array_get_callback(DBR_CTRL_DOUBLE, 1, channelId_,
      &WheelSwitchRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  } else {
    qWarning() << "Failed to request control info for" << channelName_
               << ':' << ca_message(status);
  }
}

void WheelSwitchRuntime::handleConnectionEvent(const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  auto &stats = StatisticsTracker::instance();

  if (args.op == CA_OP_CONN_UP) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    lastSeverity_ = kInvalidSeverity;
    if (!isNumericFieldType(fieldType_)) {
      qWarning() << "Wheel switch channel" << channelName_
                 << "is not a numeric type";
      invokeOnElement([](WheelSwitchElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeWriteAccess(false);
        element->setRuntimeSeverity(kInvalidSeverity);
      });
      return;
    }
    updateWriteAccess();
    invokeOnElement([](WheelSwitchElement *element) {
      element->setRuntimeConnected(true);
    });
    subscribe();
    requestControlInfo();
  } else if (args.op == CA_OP_CONN_DOWN) {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastWriteAccess_ = false;
    invokeOnElement([](WheelSwitchElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void WheelSwitchRuntime::handleValueEvent(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.type != DBR_TIME_DOUBLE) {
    return;
  }

  const auto *value = static_cast<const dbr_time_double *>(args.dbr);
  const double numericValue = value->value;
  const short severity = value->severity;

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    invokeOnElement([severity](WheelSwitchElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }

  if (!std::isfinite(numericValue)) {
    return;
  }

  if (!hasLastValue_ || std::abs(numericValue - lastValue_) > 1e-12) {
    lastValue_ = numericValue;
    hasLastValue_ = true;
    invokeOnElement([numericValue](WheelSwitchElement *element) {
      element->setRuntimeValue(numericValue);
    });
  }
}

void WheelSwitchRuntime::handleControlInfo(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.type != DBR_CTRL_DOUBLE) {
    return;
  }

  const auto *info = static_cast<const dbr_ctrl_double *>(args.dbr);
  const double low = info->lower_disp_limit;
  const double high = info->upper_disp_limit;
  const int precision = info->precision;

  invokeOnElement([low, high, precision](WheelSwitchElement *element) {
    element->setRuntimeLimits(low, high);
    element->setRuntimePrecision(precision);
  });
}

void WheelSwitchRuntime::handleAccessRightsEvent(
    const access_rights_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }
  updateWriteAccess();
}

void WheelSwitchRuntime::handleActivation(double value)
{
  if (!started_ || !channelId_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }

  dbr_double_t toSend = static_cast<dbr_double_t>(value);
  int status = ca_put(DBR_DOUBLE, channelId_, &toSend);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write wheel switch value" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return;
  }
  AuditLogger::instance().logPut(channelName_, value,
      QStringLiteral("WheelSwitch"));
  ca_flush_io();
}

void WheelSwitchRuntime::updateWriteAccess()
{
  if (!channelId_) {
    return;
  }
  const bool writeAccess = ca_write_access(channelId_) != 0;
  if (writeAccess == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = writeAccess;
  invokeOnElement([writeAccess](WheelSwitchElement *element) {
    element->setRuntimeWriteAccess(writeAccess);
  });
}

void WheelSwitchRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<WheelSwitchRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void WheelSwitchRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<WheelSwitchRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}

void WheelSwitchRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<WheelSwitchRuntime *>(args.usr);
  if (self) {
    self->handleControlInfo(args);
  }
}

void WheelSwitchRuntime::accessRightsCallback(
    struct access_rights_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<WheelSwitchRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleAccessRightsEvent(args);
  }
}
