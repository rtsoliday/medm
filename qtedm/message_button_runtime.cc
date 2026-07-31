#include "message_button_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QMessageBox>

#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "message_button_element.h"
#include "pv_channel_manager.h"
#include "soft_pv_registry.h"
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
  const ParsedPvName parsed = parsePvName(initialChannel);
  const bool needsCa = parsed.protocol == PvProtocol::kCa
      && !SoftPvRegistry::instance().isRegistered(parsed.pvName);
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
  lastReadAccess_ = false;
  lastWriteAccess_ = false;
  lastSeverity_ = 0;
  enumStrings_.clear();
  toggleStateKnown_ = false;
  toggleStateOn_ = false;

  invokeOnElement([](MessageButtonElement *element) {
    element->setRuntimeConnected(false);
    element->setRuntimeReadAccess(false);
    element->setRuntimeWriteAccess(false);
    element->setRuntimeSeverity(0);
    element->setRuntimeToggleState(false, false);
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
      element->setRuntimeReadAccess(false);
      element->setRuntimeSeverity(0);
    });
  } else {
    const bool wasConnected = connected_;
    connected_ = false;
    lastReadAccess_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastWriteAccess_ = false;
    enumStrings_.clear();
    invokeOnElement([](MessageButtonElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeReadAccess(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
      element->setRuntimeToggleState(false, false);
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
  if (element_ && element_->isQtedmToggle() && data.hasValue) {
    const bool matchesOn = dataMatchesValue(data, element_->onValue());
    const bool matchesOff = dataMatchesValue(data, element_->offValue());
    toggleStateKnown_ = matchesOn || matchesOff;
    toggleStateOn_ = matchesOn;
    invokeOnElement([on = toggleStateOn_, known = toggleStateKnown_](
                        MessageButtonElement *element) {
      element->setRuntimeToggleState(on, known);
    });
  }
}
void MessageButtonRuntime::handleAccessRights(bool canRead, bool canWrite)
{
  if (!started_) {
    return;
  }
  if (canRead == lastReadAccess_ && canWrite == lastWriteAccess_) {
    return;
  }
  lastReadAccess_ = canRead;
  lastWriteAccess_ = canWrite;
  invokeOnElement([canRead, canWrite](MessageButtonElement *element) {
    element->setRuntimeReadAccess(canRead);
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
  if (element_->isQtedmToggle()) {
    const bool nextOn = !toggleStateKnown_ || !toggleStateOn_;
    const QString value = nextOn ? element_->onValue() : element_->offValue();
    if (element_->confirmationRequired()) {
      const QString label = nextOn ? element_->onLabel() : element_->offLabel();
      const auto answer = QMessageBox::question(element_,
          QStringLiteral("Confirm PV Write"),
          QStringLiteral("Set %1 to %2 (%3)?")
              .arg(channelName_, label, value),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (answer != QMessageBox::Yes) {
        return;
      }
    }
    if (!sendValue(value)) {
      QApplication::beep();
    }
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
      auditWidgetType());
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
  AuditLogger::instance().logPut(channelName_, value, auditWidgetType());
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
    if (!ok || !std::isfinite(numeric)) {
      qWarning() << "Failed to map message" << value << "to enumeration for"
                 << channelName_;
      return false;
    }
    const double maximum = static_cast<double>(
        std::numeric_limits<dbr_enum_t>::max());
    if (numeric <= 0.0) {
      toSend = 0;
    } else if (numeric >= maximum) {
      toSend = std::numeric_limits<dbr_enum_t>::max();
    } else {
      toSend = static_cast<dbr_enum_t>(std::llround(numeric));
    }
  }

  if (!PvChannelManager::instance().putValue(channelName_, toSend)) {
    qWarning() << "Failed to write enum" << value << "to"
               << channelName_;
    return false;
  }
  AuditLogger::instance().logPut(channelName_, static_cast<int>(toSend),
      auditWidgetType());
  return true;
}

bool MessageButtonRuntime::sendNumericValue(const QString &value)
{
  bool ok = false;
  double numeric = value.toDouble(&ok);
  if (!ok || !std::isfinite(numeric)) {
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
      auditWidgetType());
  return true;
}

bool MessageButtonRuntime::dataMatchesValue(const SharedChannelData &data,
    const QString &configuredValue) const
{
  const QString trimmed = configuredValue.trimmed();
  if (data.isEnum) {
    if (data.enumValue < static_cast<dbr_enum_t>(enumStrings_.size())
        && enumStrings_.at(data.enumValue) == trimmed) {
      return true;
    }
    bool ok = false;
    const double target = trimmed.toDouble(&ok);
    if (!ok || !std::isfinite(target)) {
      return false;
    }
    const double maximum = static_cast<double>(
        std::numeric_limits<dbr_enum_t>::max());
    const dbr_enum_t normalized = target <= 0.0 ? 0
        : target >= maximum ? std::numeric_limits<dbr_enum_t>::max()
                            : static_cast<dbr_enum_t>(std::llround(target));
    return data.enumValue == normalized;
  }
  if (data.isNumeric) {
    bool ok = false;
    const double target = trimmed.toDouble(&ok);
    return ok && std::isfinite(target)
        && std::abs(data.numericValue - target) <= 1e-12;
  }
  if (data.isCharArray) {
    const int nul = data.charArrayValue.indexOf('\0');
    const QByteArray bytes = nul >= 0 ? data.charArrayValue.left(nul)
                                     : data.charArrayValue;
    return QString::fromLatin1(bytes) == trimmed;
  }
  if (data.isString) {
    return data.stringValue == trimmed;
  }
  return false;
}

QString MessageButtonRuntime::auditWidgetType() const
{
  return element_ && element_->isQtedmToggle()
      ? QStringLiteral("QtEDMToggle") : QStringLiteral("MessageButton");
}
