#include "menu_runtime.h"

#include <algorithm>
#include <functional>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "menu_element.h"

namespace {
constexpr short kInvalidSeverity = 3;
}

MenuRuntime::MenuRuntime(MenuElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

MenuRuntime::~MenuRuntime()
{
  stop();
}

void MenuRuntime::start()
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
  element_->setActivationCallback([this](int value) {
    handleActivation(value);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &MenuRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ':' << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  ca_set_puser(channelId_, this);
  ca_replace_access_rights_event(channelId_, &MenuRuntime::accessRightsCallback);

  ca_flush_io();
}

void MenuRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  unsubscribe();
  if (element_) {
    element_->setActivationCallback(std::function<void(int)>());
  }
  resetRuntimeState();
}

void MenuRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  lastSeverity_ = 0;
  lastValue_ = -1;
  lastWriteAccess_ = false;
  enumStrings_.clear();

  if (element_) {
    invokeOnElement([](MenuElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(0);
      element->setRuntimeValue(-1);
      element->setRuntimeLabels(QStringList());
    });
  }
}

void MenuRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_ || fieldType_ != DBR_ENUM) {
    return;
  }

  int status = ca_create_subscription(DBR_TIME_ENUM, 1, channelId_,
      DBE_VALUE | DBE_ALARM, &MenuRuntime::valueEventCallback,
      this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ':'
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }

  ca_flush_io();
}

void MenuRuntime::unsubscribe()
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

void MenuRuntime::requestControlInfo()
{
  if (!channelId_ || fieldType_ != DBR_ENUM) {
    return;
  }

  int status = ca_array_get_callback(DBR_CTRL_ENUM, 1, channelId_,
      &MenuRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  } else {
    qWarning() << "Failed to request control info for" << channelName_
               << ':' << ca_message(status);
  }
}

void MenuRuntime::handleConnectionEvent(const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    connected_ = true;
    fieldType_ = ca_field_type(channelId_);
    updateWriteAccess();
    if (element_) {
      invokeOnElement([](MenuElement *element) {
        element->setRuntimeConnected(true);
      });
    }
    if (fieldType_ != DBR_ENUM) {
      qWarning() << "Menu channel" << channelName_
                 << "is not an ENUM type";
      return;
    }
    subscribe();
    requestControlInfo();
  } else if (args.op == CA_OP_CONN_DOWN) {
    connected_ = false;
    lastWriteAccess_ = false;
    if (element_) {
      invokeOnElement([](MenuElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeWriteAccess(false);
        element->setRuntimeSeverity(kInvalidSeverity);
        element->setRuntimeValue(-1);
      });
    }
  }
}

void MenuRuntime::handleValueEvent(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.type != DBR_TIME_ENUM) {
    return;
  }

  const auto *value = static_cast<const dbr_time_enum *>(args.dbr);
  short severity = value->severity;
  short enumValue = value->value;

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    if (element_) {
      invokeOnElement([severity](MenuElement *element) {
        element->setRuntimeSeverity(severity);
      });
    }
  }

  if (enumValue != lastValue_) {
    lastValue_ = enumValue;
    if (element_) {
      invokeOnElement([enumValue](MenuElement *element) {
        element->setRuntimeValue(enumValue);
      });
    }
  }
}

void MenuRuntime::handleControlInfo(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.type != DBR_CTRL_ENUM) {
    return;
  }

  const auto *info = static_cast<const dbr_ctrl_enum *>(args.dbr);
  int count = std::clamp<int>(info->no_str, 0, MAX_ENUM_STATES);

  QStringList labels;
  labels.reserve(count);
  for (int i = 0; i < count; ++i) {
    labels.append(QString::fromLatin1(info->strs[i]));
  }
  enumStrings_ = labels;
  const QStringList labelsCopy = enumStrings_;
  invokeOnElement([labelsCopy](MenuElement *element) {
    element->setRuntimeLabels(labelsCopy);
  });
}

void MenuRuntime::handleAccessRightsEvent(
    const access_rights_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }
  updateWriteAccess();
}

void MenuRuntime::handleActivation(int value)
{
  if (!started_ || !channelId_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (value < 0) {
    return;
  }

  dbr_enum_t toSend = static_cast<dbr_enum_t>(value);
  int status = ca_put(DBR_ENUM, channelId_, &toSend);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write menu value" << value << "to"
               << channelName_ << ':' << ca_message(status);
    return;
  }
  ca_flush_io();
}

void MenuRuntime::updateWriteAccess()
{
  if (!channelId_) {
    return;
  }
  bool writeAccess = ca_write_access(channelId_) != 0;
  if (writeAccess == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = writeAccess;
  if (element_) {
    invokeOnElement([writeAccess](MenuElement *element) {
      element->setRuntimeWriteAccess(writeAccess);
    });
  }
}

void MenuRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<MenuRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void MenuRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<MenuRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}

void MenuRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<MenuRuntime *>(args.usr);
  if (self) {
    self->handleControlInfo(args);
  }
}

void MenuRuntime::accessRightsCallback(
    struct access_rights_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<MenuRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleAccessRightsEvent(args);
  }
}
