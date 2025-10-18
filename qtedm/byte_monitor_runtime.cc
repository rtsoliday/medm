#include "byte_monitor_runtime.h"

#include <algorithm>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "byte_monitor_element.h"
#include "channel_access_context.h"

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
  case DBR_ENUM:
    return true;
  default:
    return false;
  }
}

} // namespace

ByteMonitorRuntime::ByteMonitorRuntime(ByteMonitorElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

ByteMonitorRuntime::~ByteMonitorRuntime()
{
  stop();
}

void ByteMonitorRuntime::start()
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
      &ByteMonitorRuntime::channelConnectionCallback, this,
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

void ByteMonitorRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  unsubscribe();
  resetRuntimeState();
}

void ByteMonitorRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  lastValue_ = 0u;
  hasLastValue_ = false;
  lastSeverity_ = kInvalidSeverity;

  invokeOnElement([](ByteMonitorElement *element) {
    element->clearRuntimeState();
    element->setRuntimeConnected(false);
    element->setRuntimeSeverity(kInvalidSeverity);
  });
}

void ByteMonitorRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  int status = ca_create_subscription(DBR_TIME_LONG, 1, channelId_,
      DBE_VALUE | DBE_ALARM,
      &ByteMonitorRuntime::valueEventCallback, this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ':'
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }

  ca_flush_io();
}

void ByteMonitorRuntime::unsubscribe()
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

void ByteMonitorRuntime::handleConnectionEvent(const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    connected_ = true;
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    hasLastValue_ = false;
    lastValue_ = 0u;
    lastSeverity_ = kInvalidSeverity;

    if (!isNumericFieldType(fieldType_)) {
      qWarning() << "Byte channel" << channelName_ << "is not numeric";
      invokeOnElement([](ByteMonitorElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeSeverity(kInvalidSeverity);
      });
      return;
    }

    if (elementCount_ > 1) {
      qWarning() << "Byte channel" << channelName_
                 << "has" << elementCount_ << "elements; only the first will be used";
    }

    invokeOnElement([](ByteMonitorElement *element) {
      element->setRuntimeConnected(true);
      element->setRuntimeSeverity(0);
    });
    subscribe();
  } else if (args.op == CA_OP_CONN_DOWN) {
    connected_ = false;
    hasLastValue_ = false;
    invokeOnElement([](ByteMonitorElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void ByteMonitorRuntime::handleValueEvent(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.type != DBR_TIME_LONG) {
    return;
  }

  const auto *value = static_cast<const dbr_time_long *>(args.dbr);
  const quint32 numericValue = static_cast<quint32>(value->value);
  const short severity = value->severity;

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    invokeOnElement([severity](ByteMonitorElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }

  if (!hasLastValue_ || numericValue != lastValue_) {
    lastValue_ = numericValue;
    hasLastValue_ = true;
    invokeOnElement([numericValue](ByteMonitorElement *element) {
      element->setRuntimeValue(numericValue);
    });
  }
}

void ByteMonitorRuntime::channelConnectionCallback(struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<ByteMonitorRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void ByteMonitorRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<ByteMonitorRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}
