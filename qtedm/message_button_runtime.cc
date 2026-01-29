#include "message_button_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QApplication>
#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "message_button_element.h"
#include "pv_channel_manager.h"
#include "statistics_tracker.h"

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

  const QString initialChannel = element_->channel().trimmed();
  const bool needsCa = parsePvName(initialChannel).protocol == PvProtocol::kCa;
  if (needsCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available";
      return;
    }
  }

  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();
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

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_DOUBLE,
      0,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &data) {
        handleChannelConnection(connected, data);
      },
      [this](bool canRead, bool canWrite) { handleAccessRights(canRead, canWrite); });
}

void MessageButtonRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
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

void MessageButtonRuntime::handleChannelConnection(bool connected,
    const SharedChannelData &data)
{
  auto &stats = StatisticsTracker::instance();

  if (connected) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    fieldType_ = data.nativeFieldType;
    elementCount_ = std::max<long>(data.nativeElementCount, 1);
    enumStrings_ = data.enumStrings;
    invokeOnElement([](MessageButtonElement *element) {
      element->setRuntimeConnected(true);
      element->setRuntimeSeverity(0);
    });
  } else {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastWriteAccess_ = false;
    enumStrings_.clear();
    invokeOnElement([](MessageButtonElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void MessageButtonRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }
  const short severity = data.severity;

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    invokeOnElement([severity](MessageButtonElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }

  if (!data.enumStrings.isEmpty() && enumStrings_ != data.enumStrings) {
    enumStrings_ = data.enumStrings;
  }
}
void MessageButtonRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }
  if (canWrite == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = canWrite;
  invokeOnElement([canWrite](MessageButtonElement *element) {
    element->setRuntimeWriteAccess(canWrite);
  });
}

void MessageButtonRuntime::handlePress()
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
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
  if (!started_ || !connected_ || !lastWriteAccess_) {
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
  if (!PvChannelManager::instance().putValue(channelName_, value)) {
    qWarning() << "Failed to write string" << value << "to"
               << channelName_;
    return false;
  }
  AuditLogger::instance().logPut(channelName_, value,
      QStringLiteral("MessageButton"));
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

  if (!PvChannelManager::instance().putCharArrayValue(channelName_, data)) {
    qWarning() << "Failed to write char array" << value << "to"
               << channelName_;
    return false;
  }
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

  if (!PvChannelManager::instance().putValue(channelName_, toSend)) {
    qWarning() << "Failed to write enum" << value << "to"
               << channelName_;
    return false;
  }
  AuditLogger::instance().logPut(channelName_, static_cast<int>(toSend),
      QStringLiteral("MessageButton"));
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

  if (!PvChannelManager::instance().putValue(channelName_, numeric)) {
    qWarning() << "Failed to write numeric" << value << "to"
               << channelName_;
    return false;
  }
  AuditLogger::instance().logPut(channelName_, numeric,
      QStringLiteral("MessageButton"));
  return true;
}
