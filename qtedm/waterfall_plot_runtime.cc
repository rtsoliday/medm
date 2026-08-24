#include "waterfall_plot_runtime.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QDateTime>
#include <QDebug>

#include <db_access.h>
#include <epicsTime.h>

#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "pv_protocol.h"
#include "runtime_utils.h"
#include "soft_pv_registry.h"
#include "statistics_tracker.h"
#include "waterfall_plot_element.h"

namespace {

constexpr qint64 kUnixEpicsEpochOffsetSeconds = 631152000LL;
constexpr int kRepaintIntervalMs = 33;

qint64 epicsTimestampToMs(const epicsTimeStamp &stamp)
{
  const qint64 seconds = static_cast<qint64>(stamp.secPastEpoch)
      + kUnixEpicsEpochOffsetSeconds;
  return seconds * 1000LL + stamp.nsec / 1000000LL;
}

bool shouldClearOnTransition(double previous, double current,
    WaterfallEraseMode mode)
{
  const bool previousZero = std::abs(previous) <= RuntimeUtils::kVisibilityEpsilon;
  const bool currentZero = std::abs(current) <= RuntimeUtils::kVisibilityEpsilon;
  if (mode == WaterfallEraseMode::kIfZero) {
    return !previousZero && currentZero;
  }
  return previousZero && !currentZero;
}

int boundedNativeElementCount(long elementCount)
{
  return static_cast<int>(std::clamp<long>(elementCount, 1,
      static_cast<long>(std::numeric_limits<int>::max())));
}

} // namespace

WaterfallPlotRuntime::WaterfallPlotRuntime(WaterfallPlotElement *element)
  : QObject(element)
  , element_(element)
{
  repaintTimer_.setSingleShot(true);
  repaintTimer_.setInterval(kRepaintIntervalMs);
  connect(&repaintTimer_, &QTimer::timeout, this,
      &WaterfallPlotRuntime::flushElementUpdate);
}

WaterfallPlotRuntime::~WaterfallPlotRuntime()
{
  stop();
}

void WaterfallPlotRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  bool needsCa = false;
  const QStringList channelNames = {
      element_->dataChannel().trimmed(),
      element_->countChannel().trimmed(),
      element_->triggerChannel().trimmed(),
      element_->eraseChannel().trimmed()
  };
  for (const QString &name : channelNames) {
    const ParsedPvName parsed = parsePvName(name);
    if (!name.isEmpty() && parsed.protocol == PvProtocol::kCa
        && !SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
      needsCa = true;
      break;
    }
  }

  if (needsCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available for Waterfall Plot";
      return;
    }
  }

  resetState();
  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();

  dataChannel_.name = element_->dataChannel().trimmed();
  countChannel_.name = element_->countChannel().trimmed();
  triggerChannel_.name = element_->triggerChannel().trimmed();
  eraseChannel_.name = element_->eraseChannel().trimmed();
  eraseMode_ = element_->eraseMode();

  invokeOnElement([](WaterfallPlotElement *element) {
    element->setRuntimeConnected(false);
  });

  subscribeChannel(dataChannel_, ChannelKind::kData, DBR_TIME_DOUBLE, 0);
  subscribeChannel(countChannel_, ChannelKind::kCount, DBR_TIME_LONG, 1);
  subscribeChannel(triggerChannel_, ChannelKind::kTrigger, DBR_TIME_DOUBLE, 1);
  subscribeChannel(eraseChannel_, ChannelKind::kErase, DBR_TIME_DOUBLE, 1);
}

void WaterfallPlotRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  repaintTimer_.stop();
  dataChannel_.subscription.reset();
  countChannel_.subscription.reset();
  triggerChannel_.subscription.reset();
  eraseChannel_.subscription.reset();
  resetState();
  invokeOnElement([](WaterfallPlotElement *element) {
    element->setRuntimeConnected(false);
  });
}

void WaterfallPlotRuntime::resetState()
{
  dataChannel_ = ChannelState{};
  countChannel_ = ChannelState{};
  triggerChannel_ = ChannelState{};
  eraseChannel_ = ChannelState{};
  latestWaveform_.clear();
  latestWaveformTimestampMs_ = 0;
  countFromChannel_ = 0;
  eraseValueKnown_ = false;
  lastEraseValue_ = 0.0;
  eraseMode_ = WaterfallEraseMode::kIfNotZero;
  repaintPending_ = false;
}

void WaterfallPlotRuntime::subscribeChannel(ChannelState &state,
    ChannelKind kind, chtype type, long elementCount)
{
  if (state.name.isEmpty()) {
    return;
  }

  auto &mgr = PvChannelManager::instance();
  state.subscription = mgr.subscribe(
      state.name,
      type,
      elementCount,
      [this, kind](const SharedChannelData &data) {
        handleValue(kind, data);
      },
      [this, &state, kind](bool connected, const SharedChannelData &data) {
        handleConnection(state, kind, connected, data);
      },
      nullptr,
      ChannelDeliveryMode::kRealtime);
}

void WaterfallPlotRuntime::handleConnection(ChannelState &state,
    ChannelKind kind, bool connected, const SharedChannelData &data)
{
  Q_UNUSED(kind);
  state.connected = connected;
  if (connected) {
    state.fieldType = data.nativeFieldType;
    state.elementCount = data.nativeElementCount;
    if (&state == &dataChannel_) {
      if (!RuntimeUtils::isNumericFieldType(state.fieldType)) {
        qWarning() << "Waterfall plot channel" << state.name
                   << "is not numeric";
        invokeOnElement([](WaterfallPlotElement *element) {
          element->setRuntimeConnected(false);
          element->clearBuffer(false);
        });
        return;
      }
      const int nativeLength = boundedNativeElementCount(state.elementCount);
      invokeOnElement([nativeLength](WaterfallPlotElement *element) {
        element->setRuntimeWaveformLength(nativeLength);
        element->setRuntimeConnected(true);
      });
    }
  } else if (&state == &dataChannel_) {
    invokeOnElement([](WaterfallPlotElement *element) {
      element->setRuntimeConnected(false);
    });
  }
}

void WaterfallPlotRuntime::handleValue(ChannelKind kind,
    const SharedChannelData &data)
{
  if (!started_ || !element_) {
    return;
  }
  switch (kind) {
  case ChannelKind::kData:
    handleDataValue(data);
    break;
  case ChannelKind::kCount:
    handleCountValue(data);
    break;
  case ChannelKind::kTrigger:
    handleTriggerValue(data);
    break;
  case ChannelKind::kErase:
    handleEraseValue(data);
    break;
  }
}

void WaterfallPlotRuntime::handleDataValue(const SharedChannelData &data)
{
  if (!data.isNumeric) {
    return;
  }

  if (data.isArray) {
    if (data.sharedArrayData && data.sharedArraySize > 0) {
      const size_t boundedSize = std::min(data.sharedArraySize,
          static_cast<size_t>(kWaterfallMaxColumns));
      latestWaveform_ = QVector<double>(static_cast<int>(boundedSize));
      const double *source = data.sharedArrayData.get();
      for (int i = 0; i < latestWaveform_.size(); ++i) {
        latestWaveform_[i] = source[i];
      }
    } else {
      latestWaveform_ = data.arrayValues.mid(0, kWaterfallMaxColumns);
    }
  } else {
    latestWaveform_.resize(1);
    latestWaveform_[0] = data.numericValue;
  }
  latestWaveformTimestampMs_ = data.hasTimestamp
      ? epicsTimestampToMs(data.timestamp)
      : QDateTime::currentMSecsSinceEpoch();

  const long observedLength = std::max(data.nativeElementCount,
      static_cast<long>(latestWaveform_.size()));
  const int runtimeLength = boundedNativeElementCount(observedLength);
  invokeOnElement([runtimeLength](WaterfallPlotElement *element) {
    element->setRuntimeWaveformLength(runtimeLength);
    element->setRuntimeConnected(true);
  });

  if (triggerChannel_.name.isEmpty()) {
    pushLatestWaveform();
  }
}

void WaterfallPlotRuntime::handleCountValue(const SharedChannelData &data)
{
  if (!data.isNumeric || !std::isfinite(data.numericValue)) {
    return;
  }
  if (data.numericValue <= 0.0) {
    countFromChannel_ = 0;
  } else if (data.numericValue
      >= static_cast<double>(std::numeric_limits<int>::max())) {
    countFromChannel_ = std::numeric_limits<int>::max();
  } else {
    countFromChannel_ = static_cast<int>(std::llround(data.numericValue));
  }
}

void WaterfallPlotRuntime::handleTriggerValue(const SharedChannelData &data)
{
  if (!data.isNumeric || latestWaveform_.isEmpty()) {
    return;
  }
  pushLatestWaveform();
}

void WaterfallPlotRuntime::handleEraseValue(const SharedChannelData &data)
{
  if (!data.isNumeric) {
    return;
  }
  const double currentValue = data.numericValue;
  if (eraseValueKnown_ && element_
      && shouldClearOnTransition(lastEraseValue_, currentValue, eraseMode_)) {
    invokeOnElement([](WaterfallPlotElement *element) {
      element->clearBuffer(false);
    });
    scheduleElementUpdate();
  }
  lastEraseValue_ = currentValue;
  eraseValueKnown_ = true;
}

void WaterfallPlotRuntime::pushLatestWaveform()
{
  if (!element_ || latestWaveform_.isEmpty()) {
    return;
  }

  int count = latestWaveform_.size();
  if (countFromChannel_ > 0) {
    count = std::min(count, countFromChannel_);
  }
  if (count <= 0) {
    return;
  }

  const QVector<double> waveform = latestWaveform_;
  const qint64 timestampMs = latestWaveformTimestampMs_;
  invokeOnElement([waveform, count, timestampMs](WaterfallPlotElement *element) {
    element->pushWaveform(waveform.constData(), count, timestampMs, false);
  });
  scheduleElementUpdate();
}

void WaterfallPlotRuntime::scheduleElementUpdate()
{
  if (!element_) {
    return;
  }
  repaintPending_ = true;
  if (!repaintTimer_.isActive()) {
    repaintTimer_.start();
  }
}

void WaterfallPlotRuntime::flushElementUpdate()
{
  if (!element_ || !repaintPending_) {
    return;
  }
  repaintPending_ = false;
  invokeOnElement([](WaterfallPlotElement *element) {
    element->update();
  });
}
