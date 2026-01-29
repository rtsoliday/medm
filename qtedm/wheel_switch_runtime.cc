#include "wheel_switch_runtime.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
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
  element_->setActivationCallback([this](double value) {
    handleActivation(value);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_DOUBLE,
      1,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &) {
        handleChannelConnection(connected);
      },
      [this](bool canRead, bool canWrite) { handleAccessRights(canRead, canWrite); });
}

void WheelSwitchRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
  if (element_) {
    element_->setActivationCallback(std::function<void(double)>());
  }
  resetRuntimeState();
}

void WheelSwitchRuntime::resetRuntimeState()
{
  connected_ = false;
  lastValue_ = 0.0;
  hasLastValue_ = false;
  lastSeverity_ = kInvalidSeverity;
  lastWriteAccess_ = false;
  initialUpdateTracked_ = false;

  if (element_) {
    invokeOnElement([](WheelSwitchElement *element) {
      element->clearRuntimeState();
    });
  }
}

void WheelSwitchRuntime::handleChannelConnection(bool connected)
{
  auto &stats = StatisticsTracker::instance();

  if (connected) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    lastSeverity_ = kInvalidSeverity;
    invokeOnElement([](WheelSwitchElement *element) {
      element->setRuntimeConnected(true);
    });
  } else {
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

void WheelSwitchRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  const double numericValue = data.numericValue;
  const short severity = data.severity;

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

  if (!data.isNumeric) {
    return;
  }

  if (data.hasControlInfo) {
    const double low = data.lopr;
    const double high = data.hopr;
    const int precision = data.precision;
    invokeOnElement([low, high, precision](WheelSwitchElement *element) {
      element->setRuntimeLimits(low, high);
      element->setRuntimePrecision(precision);
    });
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
    if (!initialUpdateTracked_) {
      auto &tracker = StartupUiSettlingTracker::instance();
      if (tracker.enabled()) {
        tracker.recordInitialUpdateQueued();
      }
    }
    invokeOnElement([numericValue](WheelSwitchElement *element) {
      element->setRuntimeValue(numericValue);
    });
    if (!initialUpdateTracked_) {
      auto &tracker = StartupUiSettlingTracker::instance();
      if (tracker.enabled()) {
        tracker.recordInitialUpdateApplied();
      }
      initialUpdateTracked_ = true;
    }
  }
}

void WheelSwitchRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }

  if (canWrite == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = canWrite;
  invokeOnElement([canWrite](WheelSwitchElement *element) {
    element->setRuntimeWriteAccess(canWrite);
  });
}

void WheelSwitchRuntime::handleActivation(double value)
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }

  if (!PvChannelManager::instance().putValue(channelName_, value)) {
    qWarning() << "Failed to write wheel switch value" << value << "to"
               << channelName_;
    return;
  }
  AuditLogger::instance().logPut(channelName_, value,
      QStringLiteral("WheelSwitch"));
}
