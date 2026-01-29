#include "strip_chart_runtime.h"

#include <QByteArray>
#include <QDebug>
#include <QDateTime>

#include <db_access.h>
#include <epicsTime.h>

#include "strip_chart_element.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"

namespace {
constexpr qint64 kUnixEpicsEpochOffsetSeconds = 631152000LL;

using RuntimeUtils::isNumericFieldType;

qint64 epicsTimestampToMs(const epicsTimeStamp &stamp)
{
  const qint64 seconds = static_cast<qint64>(stamp.secPastEpoch)
      + kUnixEpicsEpochOffsetSeconds;
  const qint64 millis = seconds * 1000LL + stamp.nsec / 1000000LL;
  return millis;
}

} // namespace

StripChartRuntime::StripChartRuntime(StripChartElement *element)
  : QObject(element)
  , element_(element)
{
}

StripChartRuntime::~StripChartRuntime()
{
  stop();
}

void StripChartRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  bool needsCa = false;
  for (int i = 0; i < kStripChartPenCount; ++i) {
    const QString name = element_->channel(i).trimmed();
    if (!name.isEmpty() && parsePvName(name).protocol == PvProtocol::kCa) {
      needsCa = true;
      break;
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

  started_ = true;
  invokeOnElement([](StripChartElement *element) {
    element->clearRuntimeState();
  });

  for (int i = 0; i < kStripChartPenCount; ++i) {
    PenState &pen = pens_[i];
    pen.channelName = element_->channel(i).trimmed();
    pen.connected = false;
    pen.fieldType = -1;
    pen.elementCount = 1;

    if (pen.channelName.isEmpty()) {
      continue;
    }

    subscribePen(i);
  }
}

void StripChartRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  for (int i = 0; i < kStripChartPenCount; ++i) {
    unsubscribePen(i);
    resetPen(i);
  }

  invokeOnElement([](StripChartElement *element) {
    element->clearRuntimeState();
  });
}

void StripChartRuntime::subscribePen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  pen.subscription.reset();

  auto &mgr = PvChannelManager::instance();
  pen.subscription = mgr.subscribe(
      pen.channelName,
      DBR_TIME_DOUBLE,
      1,
      [this, index](const SharedChannelData &data) {
        handleValueEvent(index, data);
      },
      [this, index](bool connected, const SharedChannelData &data) {
        handleConnectionEvent(index, connected, data);
      });
}

void StripChartRuntime::handleConnectionEvent(int index,
    bool connected, const SharedChannelData &data)
{
  if (!started_ || index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];

  if (connected) {
    pen.connected = true;
    pen.fieldType = data.nativeFieldType;
    pen.elementCount = std::max<long>(data.nativeElementCount, 1);
    if (!isNumericFieldType(pen.fieldType) || pen.elementCount < 1) {
      qWarning() << "Strip chart channel" << pen.channelName
                 << "is not a numeric scalar";
      invokeOnElement([index](StripChartElement *element) {
        element->setRuntimeConnected(index, false);
      });
      return;
    }

    invokeOnElement([index](StripChartElement *element) {
      element->setRuntimeConnected(index, true);
    });

    if (data.hasControlInfo) {
      const double low = data.lopr;
      const double high = data.hopr;
      invokeOnElement([index, low, high](StripChartElement *element) {
        element->setRuntimeLimits(index, low, high);
      });
    }
  } else {
    pen.connected = false;
    invokeOnElement([index](StripChartElement *element) {
      element->setRuntimeConnected(index, false);
      element->clearPenRuntimeState(index);
    });
  }
}

void StripChartRuntime::handleValueEvent(int index,
    const SharedChannelData &data)
{
  if (!started_ || index < 0 || index >= kStripChartPenCount) {
    return;
  }
  if (data.hasControlInfo) {
    const double low = data.lopr;
    const double high = data.hopr;
    invokeOnElement([index, low, high](StripChartElement *element) {
      element->setRuntimeLimits(index, low, high);
    });
  }
  if (!data.isNumeric) {
    return;
  }

  const double numericValue = data.numericValue;
  qint64 timestampMs = 0;
  if (data.hasTimestamp) {
    timestampMs = epicsTimestampToMs(data.timestamp);
  } else {
    timestampMs = QDateTime::currentMSecsSinceEpoch();
  }

  invokeOnElement([index, numericValue, timestampMs](StripChartElement *element) {
    element->addRuntimeSample(index, numericValue, timestampMs);
  });
}

void StripChartRuntime::resetPen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  pen.subscription.reset();
  pen.connected = false;
  pen.fieldType = -1;
  pen.elementCount = 1;
}

void StripChartRuntime::unsubscribePen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  pen.subscription.reset();
}
