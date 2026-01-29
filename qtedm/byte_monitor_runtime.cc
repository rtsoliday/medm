#include "byte_monitor_runtime.h"

#include <algorithm>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "byte_monitor_element.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"
#include "statistics_tracker.h"

namespace {
using RuntimeUtils::isNumericFieldType;
using RuntimeUtils::kInvalidSeverity;

bool isSupportedFieldType(chtype fieldType)
{
  return isNumericFieldType(fieldType) || fieldType == DBR_ENUM;
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

  resetRuntimeState();
  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();

  channelName_ = element_->channel().trimmed();
  if (channelName_.isEmpty()) {
    return;
  }

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_LONG,
      1,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &data) {
        handleChannelConnection(connected, data);
      });
}

void ByteMonitorRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
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

void ByteMonitorRuntime::handleChannelConnection(bool connected,
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
    hasLastValue_ = false;
    lastValue_ = 0u;
    lastSeverity_ = kInvalidSeverity;

    if (!isSupportedFieldType(fieldType_)) {
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
  } else {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    hasLastValue_ = false;
    invokeOnElement([](ByteMonitorElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void ByteMonitorRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  if (!data.isNumeric) {
    return;
  }

  const quint32 numericValue = static_cast<quint32>(data.numericValue);
  const short severity = data.severity;

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

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

