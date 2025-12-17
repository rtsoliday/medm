#include "single_channel_monitor_runtime_base.h"

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "bar_monitor_element.h"
#include "meter_element.h"
#include "scale_monitor_element.h"
#include "channel_access_context.h"
#include "runtime_utils.h"
#include "statistics_tracker.h"

namespace {
using RuntimeUtils::kInvalidSeverity;

} // namespace

template <typename ElementType>
SingleChannelMonitorRuntimeBase<ElementType>::SingleChannelMonitorRuntimeBase(ElementType *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

template <typename ElementType>
SingleChannelMonitorRuntimeBase<ElementType>::~SingleChannelMonitorRuntimeBase()
{
  stop();
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::start()
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
  StatisticsTracker::instance().registerDisplayObjectStarted();

  channelName_ = element_->channel().trimmed();
  if (channelName_.isEmpty()) {
    return;
  }

  /* Use SharedChannelManager for connection sharing.
   * These monitors all use DBR_TIME_DOUBLE with element count 1. */
  auto &mgr = SharedChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_DOUBLE,
      1,  /* Single element */
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &) {
        handleChannelConnection(connected);
      });
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();

  /* SubscriptionHandle automatically unsubscribes on reset */
  subscription_.reset();

  resetRuntimeState();
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::resetRuntimeState()
{
  connected_ = false;
  lastValue_ = 0.0;
  hasLastValue_ = false;
  lastSeverity_ = kInvalidSeverity;
  hasControlInfo_ = false;
  initialUpdateTracked_ = false;

  invokeOnElement([](ElementType *element) {
    element->clearRuntimeState();
    element->setRuntimeConnected(false);
    element->setRuntimeSeverity(kInvalidSeverity);
  });
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::handleChannelConnection(bool connected)
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
    lastSeverity_ = kInvalidSeverity;

    invokeOnElement([](ElementType *element) {
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
    hasControlInfo_ = false;
    invokeOnElement([](ElementType *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::handleChannelData(const SharedChannelData &data)
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
    const double low = data.lopr;
    const double high = data.hopr;
    const int precision = data.precision;

    /* Only apply if we got reasonable limits */
    if (low != high || low != 0.0) {
      invokeOnElement([low, high, precision](ElementType *element) {
        element->setRuntimeLimits(low, high);
        element->setRuntimePrecision(precision);
      });
    }
  }

  if (severity != lastSeverity_) {
    lastSeverity_ = severity;
    invokeOnElement([severity](ElementType *element) {
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
    invokeOnElement([numericValue](ElementType *element) {
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

// Explicit template instantiations
template class SingleChannelMonitorRuntimeBase<BarMonitorElement>;
template class SingleChannelMonitorRuntimeBase<MeterElement>;
template class SingleChannelMonitorRuntimeBase<ScaleMonitorElement>;
