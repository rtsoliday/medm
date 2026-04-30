#include "single_channel_monitor_runtime_base.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QByteArray>
#include <QDebug>

#include <db_access.h>

#include "bar_monitor_element.h"
#include "led_monitor_element.h"
#include "meter_element.h"
#include "scale_monitor_element.h"
#include "thermometer_element.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"
#include "soft_pv_registry.h"
#include "startup_timing.h"
#include "statistics_tracker.h"

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {
using RuntimeUtils::kInvalidSeverity;
using RuntimeUtils::kVisibilityEpsilon;
using RuntimeUtils::kCalcInputCount;

} // namespace

template <typename ElementType>
SingleChannelMonitorRuntimeBase<ElementType>::SingleChannelMonitorRuntimeBase(ElementType *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
  for (int i = 0; i < static_cast<int>(visibilityChannels_.size()); ++i) {
    visibilityChannels_[static_cast<std::size_t>(i)].index = i;
  }
}

template <typename ElementType>
SingleChannelMonitorRuntimeBase<ElementType>::~SingleChannelMonitorRuntimeBase()
{
  stop();
}

template <typename ElementType>
bool SingleChannelMonitorRuntimeBase<ElementType>::hasConfiguredVisibilityChannels() const
{
  if constexpr (!ElementTraits::HasVisibilityInterface<ElementType>::value) {
    return false;
  }

  for (const auto &channel : visibilityChannels_) {
    if (!channel.name.isEmpty()) {
      return true;
    }
  }
  return false;
}

template <typename ElementType>
bool SingleChannelMonitorRuntimeBase<ElementType>::allVisibilityChannelsConnected() const
{
  if constexpr (!ElementTraits::HasVisibilityInterface<ElementType>::value) {
    return true;
  }

  for (const auto &channel : visibilityChannels_) {
    if (!channel.name.isEmpty() && !channel.connected) {
      return false;
    }
  }
  return true;
}

template <typename ElementType>
bool SingleChannelMonitorRuntimeBase<ElementType>::evaluateVisibility()
{
  if constexpr (!ElementTraits::HasVisibilityInterface<ElementType>::value) {
    return true;
  } else {
    if (!element_ || !connected_) {
      return true;
    }

    const bool anyVisibilityChannels = hasConfiguredVisibilityChannels();
    if (anyVisibilityChannels && !allVisibilityChannelsConnected()) {
      return true;
    }

    switch (element_->visibilityMode()) {
    case TextVisibilityMode::kStatic:
      return true;
    case TextVisibilityMode::kIfNotZero: {
      double primaryValue = 0.0;
      if (anyVisibilityChannels) {
        const auto &primary = visibilityChannels_.front();
        if (primary.hasValue && std::isfinite(primary.value)) {
          primaryValue = primary.value;
        }
      } else if (hasLastValue_ && std::isfinite(lastValue_)) {
        primaryValue = lastValue_;
      }
      return std::fabs(primaryValue) > kVisibilityEpsilon;
    }
    case TextVisibilityMode::kIfZero: {
      double primaryValue = 0.0;
      if (anyVisibilityChannels) {
        const auto &primary = visibilityChannels_.front();
        if (primary.hasValue && std::isfinite(primary.value)) {
          primaryValue = primary.value;
        }
      } else if (hasLastValue_ && std::isfinite(lastValue_)) {
        primaryValue = lastValue_;
      }
      return std::fabs(primaryValue) <= kVisibilityEpsilon;
    }
    case TextVisibilityMode::kCalc: {
      if (!visibilityCalcValid_ || visibilityCalcPostfix_.isEmpty()) {
        return false;
      }
      std::array<double, kCalcInputCount> args{};
      if (anyVisibilityChannels) {
        for (int i = 0; i < static_cast<int>(visibilityChannels_.size()); ++i) {
          const auto &channel = visibilityChannels_[static_cast<std::size_t>(i)];
          if (channel.hasValue && std::isfinite(channel.value)) {
            args[static_cast<std::size_t>(i)] = channel.value;
          }
        }
      } else if (hasLastValue_ && std::isfinite(lastValue_)) {
        args[0] = lastValue_;
      }
      RuntimeUtils::appendNullTerminator(visibilityCalcPostfix_);
      double result = 0.0;
      const long status = calcPerform(args.data(), &result,
          visibilityCalcPostfix_.data());
      return (status == 0) && std::isfinite(result)
          && std::fabs(result) > kVisibilityEpsilon;
    }
    }
  }

  return true;
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::updateRuntimeVisibility()
{
  if constexpr (!ElementTraits::HasVisibilityInterface<ElementType>::value) {
    return;
  } else {
    const bool visible = evaluateVisibility();
    invokeOnElement([visible](ElementType *element) {
      element->setRuntimeVisible(visible);
    });
  }
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::start()
{
  if (started_ || !element_) {
    return;
  }

  auto needsCaContextFor = [](const QString &channelName) {
    const ParsedPvName parsed = parsePvName(channelName);
    return parsed.protocol == PvProtocol::kCa
        && !SoftPvRegistry::instance().isRegistered(parsed.pvName);
  };

  const QString initialChannel = element_->channel().trimmed();
  bool needsCa = !initialChannel.isEmpty() && needsCaContextFor(initialChannel);
  if constexpr (ElementTraits::HasVisibilityInterface<ElementType>::value) {
    if (!needsCa) {
      for (int i = 0; i < 5; ++i) {
        const QString visibilityChannel = element_->channel(i).trimmed();
        if (!visibilityChannel.isEmpty()
            && needsCaContextFor(visibilityChannel)) {
          needsCa = true;
          break;
        }
      }
    }
  }
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
  if (!channelName_.isEmpty()) {
    /* Use SharedChannelManager for connection sharing.
     * These monitors all use DBR_TIME_DOUBLE with element count 1. */
    auto &mgr = PvChannelManager::instance();
    subscription_ = mgr.subscribe(
        channelName_,
        DBR_TIME_DOUBLE,
        1,  /* Single element */
        [this](const SharedChannelData &data) { handleChannelData(data); },
        [this](bool connected, const SharedChannelData &data) {
          handleChannelConnection(connected, data);
        });
  }

  if constexpr (ElementTraits::HasVisibilityInterface<ElementType>::value) {
    visibilityCalcPostfix_.clear();
    visibilityCalcValid_ = false;

    const TextVisibilityMode mode = element_->visibilityMode();
    if (mode == TextVisibilityMode::kCalc) {
      const QString calcExpr = element_->visibilityCalc().trimmed();
      if (!calcExpr.isEmpty()) {
        const QString normalized = RuntimeUtils::normalizeCalcExpression(calcExpr);
        QByteArray infix = normalized.toLatin1();
        visibilityCalcPostfix_.resize(512);
        visibilityCalcPostfix_.fill('\0');
        short error = 0;
        long status = postfix(infix.data(), visibilityCalcPostfix_.data(), &error);
        if (status == 0) {
          visibilityCalcValid_ = true;
        } else {
          qWarning() << "Invalid visibility calc expression for thermometer:"
                     << calcExpr << "(error" << error << ')';
        }
      }
    }

    auto &mgr = PvChannelManager::instance();
    for (auto &channel : visibilityChannels_) {
      channel.name = element_->channel(channel.index);
      channel.subscription.reset();
      channel.connected = false;
      channel.hasValue = false;
      channel.value = 0.0;
      if (channel.name.isEmpty()) {
        continue;
      }
      const QString subscribeName = channel.name.trimmed();
      if (subscribeName.isEmpty()) {
        continue;
      }
      const int idx = channel.index;
      channel.subscription = mgr.subscribe(
          subscribeName,
          DBR_TIME_DOUBLE,
          1,
          [this, idx](const SharedChannelData &data) {
            auto &channelRef = visibilityChannels_[static_cast<std::size_t>(idx)];
            channelRef.hasValue = data.hasValue;
            channelRef.value = data.numericValue;
            updateRuntimeVisibility();
          },
          [this, idx](bool channelConnected, const SharedChannelData &) {
            auto &channelRef = visibilityChannels_[static_cast<std::size_t>(idx)];
            channelRef.connected = channelConnected;
            channelRef.hasValue = false;
            channelRef.value = 0.0;
            updateRuntimeVisibility();
          });
    }
  }
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
  if constexpr (ElementTraits::HasVisibilityInterface<ElementType>::value) {
    for (auto &channel : visibilityChannels_) {
      channel.subscription.reset();
    }
  }

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
  visibilityCalcPostfix_.clear();
  visibilityCalcValid_ = false;
  if constexpr (ElementTraits::HasVisibilityInterface<ElementType>::value) {
    for (auto &channel : visibilityChannels_) {
      channel.name.clear();
      channel.subscription.reset();
      channel.connected = false;
      channel.hasValue = false;
      channel.value = 0.0;
    }
  }

  invokeOnElement([](ElementType *element) {
    element->clearRuntimeState();
    element->setRuntimeConnected(false);
    element->setRuntimeSeverity(kInvalidSeverity);
    if constexpr (ElementTraits::HasVisibilityInterface<ElementType>::value) {
      element->setRuntimeVisible(true);
    }
  });
}

template <typename ElementType>
void SingleChannelMonitorRuntimeBase<ElementType>::handleChannelConnection(
    bool connected, const SharedChannelData &data)
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
    short initialSeverity = kInvalidSeverity;
    if (data.hasValue) {
      initialSeverity = std::clamp<short>(data.severity, 0, kInvalidSeverity);
    }
    lastSeverity_ = initialSeverity;

    invokeOnElement([initialSeverity](ElementType *element) {
      element->setRuntimeConnected(true);
      element->setRuntimeSeverity(initialSeverity);
    });
    updateRuntimeVisibility();
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
      if constexpr (ElementTraits::HasVisibilityInterface<ElementType>::value) {
        element->setRuntimeVisible(true);
      }
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

  bool controlInfoApplied = false;
  /* Apply limits from control info if available and not yet done */
  if (!hasControlInfo_ && (data.hasControlInfo || data.lopr != 0.0 || data.hopr != 0.0)) {
    hasControlInfo_ = true;
    const double low = std::min(data.lopr, data.hopr);
    double high = std::max(data.lopr, data.hopr);
    const int precision = data.precision;

    if (high == low) {
      high = low + 1.0;
    }

    /* Only apply if we got reasonable limits */
    if (std::isfinite(low) && std::isfinite(high)) {
      invokeOnElement([low, high, precision](ElementType *element) {
        element->setRuntimeLimits(low, high);
        element->setRuntimePrecision(precision);
      });
      controlInfoApplied = true;
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

  if (controlInfoApplied) {
    hasLastValue_ = false;
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

  updateRuntimeVisibility();
}

// Explicit template instantiations
template class SingleChannelMonitorRuntimeBase<BarMonitorElement>;
template class SingleChannelMonitorRuntimeBase<LedMonitorElement>;
template class SingleChannelMonitorRuntimeBase<MeterElement>;
template class SingleChannelMonitorRuntimeBase<ScaleMonitorElement>;
template class SingleChannelMonitorRuntimeBase<ThermometerElement>;
