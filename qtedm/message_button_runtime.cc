#include "message_button_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QApplication>
#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "message_button_element.h"

namespace {
constexpr short kInvalidSeverity = 3;
}

MessageButtonRuntime::MessageButtonRuntime(MessageButtonElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

MessageButtonRuntime::~MessageButtonRuntime()
{
  stop();
}

void MessageButtonRuntime::start()
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

  started_ = true;
  resetRuntimeState();

  element_->setPressCallback([this]() {
    handlePress();
  });
  element_->setReleaseCallback([this]() {
    handleRelease();
  });

  channelName_ = element_->channel().trimmed();
  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &MessageButtonRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ':' << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  ca_set_puser(channelId_, this);
  ca_replace_access_rights_event(channelId_,
      &MessageButtonRuntime::accessRightsCallback);

  ca_flush_io();
}

void MessageButtonRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  unsubscribe();
  if (element_) {
    element_->setPressCallback(std::function<void()>());
    element_->setReleaseCallback(std::function<void()>());
  }
  resetRuntimeState();
}

void MessageButtonRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  lastWriteAccess_ = false;
  lastSeverity_ = 0;
  enumStrings_.clear();

  invokeOnElement([](MessageButtonElement *element) {
    element->setRuntimeConnected(false);
    element->setRuntimeWriteAccess(false);
    element->setRuntimeSeverity(0);
  });
}

void MessageButtonRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  chtype subscriptionType = 0;
  long count = 1;
  switch (fieldType_) {
  case DBR_STRING:
    subscriptionType = DBR_TIME_STRING;
    count = 1;
    break;
  case DBR_ENUM:
    subscriptionType = DBR_TIME_ENUM;
    count = 1;
    break;
  case DBR_CHAR:
    subscriptionType = DBR_TIME_CHAR;
    count = std::max<long>(elementCount_, 1);
    break;
  case DBR_SHORT:
    subscriptionType = DBR_TIME_SHORT;
    count = 1;
    break;
  case DBR_LONG:
    subscriptionType = DBR_TIME_LONG;
    count = 1;
    break;
  case DBR_FLOAT:
    subscriptionType = DBR_TIME_FLOAT;
    count = 1;
    break;
  case DBR_DOUBLE:
  default:
    subscriptionType = DBR_TIME_DOUBLE;
    count = 1;
    break;
  }

  int status = ca_create_subscription(subscriptionType, count, channelId_,
      DBE_VALUE | DBE_ALARM, &MessageButtonRuntime::valueEventCallback,
      this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ':'
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }

  ca_flush_io();
}

void MessageButtonRuntime::unsubscribe()
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

void MessageButtonRuntime::requestControlInfo()
{
  if (!channelId_ || fieldType_ != DBR_ENUM) {
    return;
  }

  int status = ca_array_get_callback(DBR_CTRL_ENUM, 1, channelId_,
      &MessageButtonRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  } else {
    qWarning() << "Failed to request control info for" << channelName_
               << ':' << ca_message(status);
  }
}

void MessageButtonRuntime::updateWriteAccess()
{
  if (!channelId_) {
    return;
  }
  bool writeAccess = ca_write_access(channelId_) != 0;
  if (writeAccess == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = writeAccess;
  invokeOnElement([writeAccess](MessageButtonElement *element) {
    element->setRuntimeWriteAccess(writeAccess);
  });
}

void MessageButtonRuntime::handleConnectionEvent(
    const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    connected_ = true;
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    enumStrings_.clear();
    updateWriteAccess();
    invokeOnElement([](MessageButtonElement *element) {
      element->setRuntimeConnected(true);
      element->setRuntimeSeverity(0);
    });
    subscribe();
    requestControlInfo();
  } else if (args.op == CA_OP_CONN_DOWN) {
    connected_ = false;
    lastWriteAccess_ = false;
    enumStrings_.clear();
    invokeOnElement([](MessageButtonElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void MessageButtonRuntime::handleValueEvent(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }

  short severity = 0;
  switch (args.type) {
  case DBR_TIME_STRING:
    severity = static_cast<const dbr_time_string *>(args.dbr)->severity;
    break;
  case DBR_TIME_ENUM:
    severity = static_cast<const dbr_time_enum *>(args.dbr)->severity;
    break;
  case DBR_TIME_CHAR:
    severity = static_cast<const dbr_time_char *>(args.dbr)->severity;
    break;
  case DBR_TIME_SHORT:
    severity = static_cast<const dbr_time_short *>(args.dbr)->severity;
    break;
  case DBR_TIME_LONG:
    severity = static_cast<const dbr_time_long *>(args.dbr)->severity;
    break;
  case DBR_TIME_FLOAT:
    severity = static_cast<const dbr_time_float *>(args.dbr)->severity;
    break;
  case DBR_TIME_DOUBLE:
    severity = static_cast<const dbr_time_double *>(args.dbr)->severity;
    break;
  default:
    return;
  }

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    invokeOnElement([severity](MessageButtonElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }
}

void MessageButtonRuntime::handleControlInfo(
    const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.type != DBR_CTRL_ENUM) {
    return;
  }

  const auto *info = static_cast<const dbr_ctrl_enum *>(args.dbr);
  int count = std::clamp<int>(info->no_str, 0, MAX_ENUM_STATES);

  QStringList strings;
  strings.reserve(count);
  for (int i = 0; i < count; ++i) {
    strings.append(QString::fromLatin1(info->strs[i]));
  }
  enumStrings_ = strings;
}

void MessageButtonRuntime::handleAccessRightsEvent(
    const access_rights_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }
  updateWriteAccess();
}

void MessageButtonRuntime::handlePress()
{
  if (!started_ || !channelId_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (!element_) {
    return;
  }
  const QString message = element_->pressMessage();
  if (message.trimmed().isEmpty()) {
    return;
  }
  if (!sendValue(message)) {
    invokeOnElement([](MessageButtonElement *) {
      QApplication::beep();
    });
  }
}

void MessageButtonRuntime::handleRelease()
{
  if (!started_ || !channelId_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (!element_) {
    return;
  }
  const QString message = element_->releaseMessage();
  if (message.trimmed().isEmpty()) {
    return;
  }
  if (!sendValue(message)) {
    invokeOnElement([](MessageButtonElement *) {
      QApplication::beep();
    });
  }
}

bool MessageButtonRuntime::sendValue(const QString &value)
{
  if (!channelId_) {
    return false;
  }
  const QString trimmed = value.trimmed();
  if (trimmed.isEmpty()) {
    return true;
  }

  bool success = false;
  switch (fieldType_) {
  case DBR_STRING:
    success = sendStringValue(trimmed);
    break;
  case DBR_ENUM:
    success = sendEnumValue(trimmed);
    break;
  case DBR_CHAR:
    if (elementCount_ > 1) {
      success = sendCharArrayValue(trimmed);
    } else {
      success = sendNumericValue(trimmed);
    }
    break;
  case DBR_SHORT:
  case DBR_LONG:
  case DBR_FLOAT:
  case DBR_DOUBLE:
  default:
    success = sendNumericValue(trimmed);
    break;
  }

  return success;
}

bool MessageButtonRuntime::sendStringValue(const QString &value)
{
  dbr_string_t buffer;
  std::memset(buffer, 0, sizeof(buffer));

  QByteArray bytes = value.toLatin1();
  std::strncpy(buffer, bytes.constData(), sizeof(buffer) - 1);

  int status = ca_put(DBR_STRING, channelId_, buffer);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write string" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return false;
  }
  ca_flush_io();
  return true;
}

bool MessageButtonRuntime::sendCharArrayValue(const QString &value)
{
  const long count = std::max<long>(elementCount_, 1);
  if (count <= 0) {
    return false;
  }

  const long clamped = std::min<long>(count,
      static_cast<long>(std::numeric_limits<int>::max()));
  QByteArray data(static_cast<int>(clamped), 0);
  QByteArray bytes = value.toLatin1();
  const int copyCount = std::min<int>(bytes.size(), data.size());
  if (copyCount > 0) {
    std::memcpy(data.data(), bytes.constData(), copyCount);
  }

  int status = ca_array_put(DBR_CHAR, static_cast<unsigned long>(clamped),
      channelId_, data.constData());
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write char array" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return false;
  }
  ca_flush_io();
  return true;
}

bool MessageButtonRuntime::sendEnumValue(const QString &value)
{
  dbr_enum_t toSend = 0;
  bool matched = false;
  for (int i = 0; i < enumStrings_.size(); ++i) {
    if (enumStrings_.at(i) == value) {
      toSend = static_cast<dbr_enum_t>(i);
      matched = true;
      break;
    }
  }

  if (!matched) {
    bool ok = false;
    const double numeric = value.toDouble(&ok);
    if (!ok) {
      qWarning() << "Failed to map message" << value << "to enumeration for"
                 << channelName_;
      return false;
    }
    long rounded = static_cast<long>(std::llround(numeric));
    if (rounded < 0) {
      rounded = 0;
    }
    if (rounded > std::numeric_limits<dbr_enum_t>::max()) {
      rounded = std::numeric_limits<dbr_enum_t>::max();
    }
    toSend = static_cast<dbr_enum_t>(rounded);
  }

  int status = ca_put(DBR_ENUM, channelId_, &toSend);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write enum" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return false;
  }
  ca_flush_io();
  return true;
}

bool MessageButtonRuntime::sendNumericValue(const QString &value)
{
  bool ok = false;
  double numeric = value.toDouble(&ok);
  if (!ok) {
    qWarning() << "Failed to convert message" << value << "to numeric for"
               << channelName_;
    return false;
  }

  int status = ca_put(DBR_DOUBLE, channelId_, &numeric);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write numeric" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return false;
  }
  ca_flush_io();
  return true;
}

void MessageButtonRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<MessageButtonRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void MessageButtonRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<MessageButtonRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}

void MessageButtonRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<MessageButtonRuntime *>(args.usr);
  if (self) {
    self->handleControlInfo(args);
  }
}

void MessageButtonRuntime::accessRightsCallback(
    struct access_rights_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<MessageButtonRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleAccessRightsEvent(args);
  }
}
