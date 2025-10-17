#include "slider_runtime.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "slider_element.h"

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

SliderRuntime::SliderRuntime(SliderElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

SliderRuntime::~SliderRuntime()
{
  stop();
}

void SliderRuntime::start()
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
  element_->setActivationCallback([this](double value) {
    handleActivation(value);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &SliderRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ':' << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  ca_set_puser(channelId_, this);
  ca_replace_access_rights_event(channelId_,
      &SliderRuntime::accessRightsCallback);

  ca_flush_io();
}

void SliderRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  unsubscribe();
  if (element_) {
    element_->setActivationCallback(std::function<void(double)>());
  }
  resetRuntimeState();
}

void SliderRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  lastValue_ = 0.0;
  hasLastValue_ = false;
  lastSeverity_ = 0;
  lastWriteAccess_ = false;

  if (element_) {
    invokeOnElement([](SliderElement *element) {
      element->clearRuntimeState();
    });
  }
}

void SliderRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  int status = ca_create_subscription(DBR_TIME_DOUBLE, 1, channelId_,
      DBE_VALUE | DBE_ALARM,
      &SliderRuntime::valueEventCallback, this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ':'
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }

  ca_flush_io();
}

void SliderRuntime::unsubscribe()
{
  if (subscriptionId_) {
    ca_clear_subscription(subscriptionId_);
    subscriptionId_ = nullptr;
  }
  if (channelId_) {
    ca_replace_access_rights_event(channelId_, nullptr);
    ca_clear_channel(channelId_);
    channelId_ = nullptr;
  }
  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void SliderRuntime::requestControlInfo()
{
  if (!channelId_ || !isNumericFieldType(fieldType_)) {
    return;
  }

  int status = ca_array_get_callback(DBR_CTRL_DOUBLE, 1, channelId_,
      &SliderRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  } else {
    qWarning() << "Failed to request control info for" << channelName_
               << ':' << ca_message(status);
  }
}

void SliderRuntime::handleConnectionEvent(const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    connected_ = true;
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    if (!isNumericFieldType(fieldType_)) {
      qWarning() << "Slider channel" << channelName_
                 << "is not a numeric type";
      invokeOnElement([](SliderElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeWriteAccess(false);
        element->setRuntimeSeverity(kInvalidSeverity);
      });
      return;
    }
    updateWriteAccess();
    invokeOnElement([](SliderElement *element) {
      element->setRuntimeConnected(true);
    });
    subscribe();
    requestControlInfo();
  } else if (args.op == CA_OP_CONN_DOWN) {
    connected_ = false;
    lastWriteAccess_ = false;
    invokeOnElement([](SliderElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void SliderRuntime::handleValueEvent(const event_handler_args &args)
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
    invokeOnElement([severity](SliderElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }

  if (!std::isfinite(numericValue)) {
    return;
  }

  if (!hasLastValue_ || std::abs(numericValue - lastValue_) > 1e-12) {
    lastValue_ = numericValue;
    hasLastValue_ = true;
    invokeOnElement([numericValue](SliderElement *element) {
      element->setRuntimeValue(numericValue);
    });
  }
}

void SliderRuntime::handleControlInfo(const event_handler_args &args)
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

  invokeOnElement([low, high, precision](SliderElement *element) {
    element->setRuntimeLimits(low, high);
    element->setRuntimePrecision(precision);
  });
}

void SliderRuntime::handleAccessRightsEvent(
    const access_rights_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }
  updateWriteAccess();
}

void SliderRuntime::handleActivation(double value)
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
    qWarning() << "Failed to write slider value" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return;
  }
  ca_flush_io();
}

void SliderRuntime::updateWriteAccess()
{
  if (!channelId_) {
    return;
  }
  const bool writeAccess = ca_write_access(channelId_) != 0;
  if (writeAccess == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = writeAccess;
  invokeOnElement([writeAccess](SliderElement *element) {
    element->setRuntimeWriteAccess(writeAccess);
  });
}

void SliderRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<SliderRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void SliderRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<SliderRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}

void SliderRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<SliderRuntime *>(args.usr);
  if (self) {
    self->handleControlInfo(args);
  }
}

void SliderRuntime::accessRightsCallback(
    struct access_rights_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<SliderRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleAccessRightsEvent(args);
  }
}
