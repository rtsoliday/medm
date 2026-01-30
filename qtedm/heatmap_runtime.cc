#include "heatmap_runtime.h"

#include <QDebug>

#include <algorithm>

#include <db_access.h>

#include "heatmap_element.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"
#include "statistics_tracker.h"

namespace {
using RuntimeUtils::isNumericFieldType;
using RuntimeUtils::kInvalidSeverity;
}

HeatmapRuntime::HeatmapRuntime(HeatmapElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    dataChannel_.name = element_->dataChannel().trimmed();
  }
}

HeatmapRuntime::~HeatmapRuntime()
{
  stop();
}

void HeatmapRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  const QString initialChannel = element_->dataChannel().trimmed();
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

  subscribeDataChannel();

  if (element_->xDimensionSource() == HeatmapDimensionSource::kChannel) {
    subscribeDimensionChannel(xDimensionChannel_,
        element_->xDimensionChannel());
  }
  if (element_->yDimensionSource() == HeatmapDimensionSource::kChannel) {
    subscribeDimensionChannel(yDimensionChannel_,
        element_->yDimensionChannel());
  }
}

void HeatmapRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  dataChannel_.subscription.reset();
  xDimensionChannel_.subscription.reset();
  yDimensionChannel_.subscription.reset();
  resetRuntimeState();
}

void HeatmapRuntime::resetRuntimeState()
{
  dataChannel_.connected = false;
  dataChannel_.fieldType = -1;
  dataChannel_.elementCount = 0;
  xDimensionChannel_.connected = false;
  yDimensionChannel_.connected = false;
  lastSeverity_ = kInvalidSeverity;
  runtimeXDimension_ = 0;
  runtimeYDimension_ = 0;

  invokeOnElement([](HeatmapElement *element) {
    element->clearRuntimeState();
    element->setRuntimeConnected(false);
    element->setRuntimeSeverity(kInvalidSeverity);
  });
}

void HeatmapRuntime::subscribeDataChannel()
{
  if (!element_) {
    return;
  }

  dataChannel_.name = element_->dataChannel().trimmed();
  if (dataChannel_.name.isEmpty()) {
    return;
  }

  auto &mgr = PvChannelManager::instance();
  dataChannel_.subscription = mgr.subscribe(
      dataChannel_.name,
      DBR_TIME_DOUBLE,
      0,
      [this](const SharedChannelData &data) { handleDataValue(data); },
      [this](bool connected, const SharedChannelData &data) {
        handleDataConnection(connected, data);
      });
}

void HeatmapRuntime::subscribeDimensionChannel(ChannelState &state,
    const QString &name)
{
  state.name = name.trimmed();
  if (state.name.isEmpty()) {
    return;
  }

  auto &mgr = PvChannelManager::instance();
  state.subscription = mgr.subscribe(
      state.name,
      DBR_TIME_LONG,
      1,
      [this, &state](const SharedChannelData &data) {
        if (!data.isNumeric) {
          return;
        }
        const int value = static_cast<int>(data.numericValue);
        handleDimensionValue(state, value);
      },
      [this, &state](bool connected, const SharedChannelData &) {
        handleDimensionConnection(state, connected);
      });
}

void HeatmapRuntime::handleDataConnection(bool connected,
    const SharedChannelData &data)
{
  auto &stats = StatisticsTracker::instance();
  if (connected) {
    const bool wasConnected = dataChannel_.connected;
    dataChannel_.connected = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    dataChannel_.fieldType = data.nativeFieldType;
    dataChannel_.elementCount = data.nativeElementCount;

    if (!isNumericFieldType(dataChannel_.fieldType)) {
      qWarning() << "Heatmap channel" << dataChannel_.name
                 << "is not numeric";
      invokeOnElement([](HeatmapElement *element) {
        element->setRuntimeConnected(false);
        element->setRuntimeSeverity(kInvalidSeverity);
        element->clearRuntimeState();
      });
      return;
    }

    invokeOnElement([](HeatmapElement *element) {
      element->setRuntimeConnected(true);
      element->setRuntimeSeverity(0);
    });
  } else {
    const bool wasConnected = dataChannel_.connected;
    dataChannel_.connected = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    invokeOnElement([](HeatmapElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
      element->clearRuntimeState();
    });
  }
}

void HeatmapRuntime::handleDataValue(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }
  if (!data.isNumeric) {
    return;
  }

  {
    auto &stats = StatisticsTracker::instance();
    stats.registerCaEvent();
    stats.registerUpdateRequest(true);
    stats.registerUpdateExecuted();
  }

  if (data.severity != lastSeverity_) {
    lastSeverity_ = data.severity;
    invokeOnElement([severity = data.severity](HeatmapElement *element) {
      element->setRuntimeSeverity(severity);
    });
  }

  QVector<double> values;
  if (data.isArray && !data.arrayValues.isEmpty()) {
    values = data.arrayValues;
  } else {
    values.append(data.numericValue);
  }
  invokeOnElement([values](HeatmapElement *element) {
    element->setRuntimeData(values);
  });
}

void HeatmapRuntime::handleDimensionConnection(ChannelState &state,
    bool connected)
{
  state.connected = connected;
}

void HeatmapRuntime::handleDimensionValue(ChannelState &state, int value)
{
  if (!started_) {
    return;
  }
  if (value <= 0) {
    return;
  }

  if (&state == &xDimensionChannel_) {
    runtimeXDimension_ = value;
  } else if (&state == &yDimensionChannel_) {
    runtimeYDimension_ = value;
  }

  const int xDim = runtimeXDimension_ > 0 ? runtimeXDimension_ : element_->xDimension();
  const int yDim = runtimeYDimension_ > 0 ? runtimeYDimension_ : element_->yDimension();

  invokeOnElement([xDim, yDim](HeatmapElement *element) {
    element->setRuntimeDimensions(xDim, yDim);
  });
}
