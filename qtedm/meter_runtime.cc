#include "meter_runtime.h"

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "meter_element.h"

namespace {
constexpr short kInvalidSeverity = 3;

bool isNumericFieldType(chtype fieldType)
{
  switch (fieldType) {
  case DBR_CHAR:
  case DBR_SHORT:
  case DBR_LONG:
  case DBR_FLOAT:
  case DBR_DOUBLE:
    return true;
  default:
    return false;
  }
}

} // namespace

MeterRuntime::MeterRuntime(MeterElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

MeterRuntime::~MeterRuntime()
{
  stop();
}

void MeterRuntime::start()
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

  channelName_ = element_->channel().trimmed();
  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &MeterRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ':' << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  ca_set_puser(channelId_, this);
  ca_flush_io();
}

void MeterRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  unsubscribe();
  resetRuntimeState();
}

void MeterRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  lastValue_ = 0.0;
  hasLastValue_ = false;
  lastSeverity_ = kInvalidSeverity;

  invokeOnElement([](MeterElement *element) {
    element->clearRuntimeState();
    element->setRuntimeConnected(false);
    element->setRuntimeSeverity(kInvalidSeverity);
  });
}

void MeterRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  int status = ca_create_subscription(DBR_TIME_DOUBLE, 1, channelId_,
      DBE_VALUE | DBE_ALARM,
      &MeterRuntime::valueEventCallback, this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ':'
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }

  ca_flush_io();
}

void MeterRuntime::unsubscribe()
{
  if (subscriptionId_) {
    ca_clear_subscription(subscriptionId_);
    subscriptionId_ = nullptr;
  }
  if (channelId_) {
    ca_clear_channel(channelId_);
    channelId_ = nullptr;
  }
  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void MeterRuntime::requestControlInfo()
{
  if (!channelId_ || !isNumericFieldType(fieldType_)) {
    return;
  }

  int status = ca_array_get_callback(DBR_CTRL_DOUBLE, 1, channelId_,
      &MeterRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  } else {
    qWarning() << "Failed to request control info for" << channelName_
               << ':' << ca_message(status);
  }
}

void MeterRuntime::handleConnectionEvent(const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    connected_ = true;
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    hasLastValue_ = false;
    lastValue_ = 0.0;
    lastSeverity_ = kInvalidSeverity;

    if (!isNumericFieldType(fieldType_)) {
      qWarning() << "Meter channel" << channelName_ << "is not numeric";
      invokeOnElement([](MeterElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeSeverity(kInvalidSeverity);
      });
      return;
    }

    invokeOnElement([](MeterElement *element) {
      element->setRuntimeConnected(true);
      element->setRuntimeSeverity(0);
    });
    subscribe();
    requestControlInfo();
  } else if (args.op == CA_OP_CONN_DOWN) {
    connected_ = false;
    hasLastValue_ = false;
    invokeOnElement([](MeterElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void MeterRuntime::handleValueEvent(const event_handler_args &args)
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

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    invokeOnElement([severity](MeterElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }

  if (!std::isfinite(numericValue)) {
    return;
  }

  if (!hasLastValue_ || std::abs(numericValue - lastValue_) > 1e-12) {
    lastValue_ = numericValue;
    hasLastValue_ = true;
    invokeOnElement([numericValue](MeterElement *element) {
      element->setRuntimeValue(numericValue);
    });
  }
}

void MeterRuntime::handleControlInfo(const event_handler_args &args)
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

  invokeOnElement([low, high, precision](MeterElement *element) {
    element->setRuntimeLimits(low, high);
    element->setRuntimePrecision(precision);
  });
}

void MeterRuntime::channelConnectionCallback(struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<MeterRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void MeterRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<MeterRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}

void MeterRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<MeterRuntime *>(args.usr);
  if (self) {
    self->handleControlInfo(args);
  }
}
