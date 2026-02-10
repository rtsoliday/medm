#include "slider_runtime.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"
#include "slider_element.h"
#include "statistics_tracker.h"

namespace {
using RuntimeUtils::kInvalidSeverity;

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

  /* Use PvChannelManager for connection sharing */
  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_DOUBLE,
      1,  /* Single element */
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &) {
        handleChannelConnection(connected);
      },
      [this](bool canRead, bool canWrite) { handleAccessRights(canRead, canWrite); });
}

void SliderRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();

  /* SubscriptionHandle automatically unsubscribes on reset */
  subscription_.reset();

  if (element_) {
    element_->setActivationCallback(std::function<void(double)>());
  }
  resetRuntimeState();
}

void SliderRuntime::resetRuntimeState()
{
  connected_ = false;
  lastValue_ = 0.0;
  hasLastValue_ = false;
  lastSeverity_ = 0;
  hasControlInfo_ = false;
  lastWriteAccess_ = false;
  initialUpdateTracked_ = false;

  if (element_) {
    invokeOnElement([](SliderElement *element) {
      element->clearRuntimeState();
    });
  }
}

void SliderRuntime::handleChannelConnection(bool connected)
{
  if (!started_) {
    return;
  }

  auto &stats = StatisticsTracker::instance();

  if (connected) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    hasLastValue_ = false;
    lastValue_ = 0.0;
    lastSeverity_ = 0;

    invokeOnElement([](SliderElement *element) {
      element->setRuntimeConnected(true);
    });
  } else {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastWriteAccess_ = false;
    hasControlInfo_ = false;
    invokeOnElement([](SliderElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void SliderRuntime::handleChannelData(const SharedChannelData &data)
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

  /* Apply limits from control info if available and not yet done */
  if (!hasControlInfo_ && (data.hasControlInfo || data.lopr != 0.0 || data.hopr != 0.0)) {
    hasControlInfo_ = true;
    /* MEDM path converts CA limits through float before use. Mirror that
     * behavior for compatibility, including possible +/-inf from overflow. */
    double low = static_cast<double>(static_cast<float>(data.lopr));
    double high = static_cast<double>(static_cast<float>(data.hopr));
    const int precision = data.precision;

    /* MEDM compatibility: avoid degenerate 0..0 range from channel info. */
    if (high == 0.0 && low == 0.0) {
      high += 1.0;
    }

    if (low != high || low != 0.0) {
      invokeOnElement([low, high, precision](SliderElement *element) {
        element->setRuntimeLimits(low, high);
        element->setRuntimePrecision(precision);
      });
    }
  }

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
    if (!initialUpdateTracked_) {
      auto &tracker = StartupUiSettlingTracker::instance();
      if (tracker.enabled()) {
        tracker.recordInitialUpdateQueued();
      }
    }
    invokeOnElement([numericValue](SliderElement *element) {
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

void SliderRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }

  if (canWrite == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = canWrite;
  invokeOnElement([canWrite](SliderElement *element) {
    element->setRuntimeWriteAccess(canWrite);
  });
}

void SliderRuntime::handleActivation(double value)
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
    return;
  }
  if (!std::isfinite(value)) {
    return;
  }

  /* Use SharedChannelManager for the put operation */
  if (!PvChannelManager::instance().putValue(channelName_, value)) {
    qWarning() << "Failed to write slider value" << value << "to"
               << channelName_;
  }
}
