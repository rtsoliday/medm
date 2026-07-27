#include "strip_chart_runtime.h"

#include <QByteArray>
#include <QDebug>
#include <QDateTime>
#include <QPointer>

#include <db_access.h>
#include <epicsTime.h>

#include "strip_chart_element.h"
#include "channel_access_context.h"
#include "plugin_manager.h"
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
  const QString providerId =
      qEnvironmentVariable("QTEDM_ARCHIVER_PROVIDER").trimmed();
  if (providerId.isEmpty()
      || providerId.compare(QStringLiteral("archiver-appliance"),
          Qt::CaseInsensitive) == 0) {
    archiveProvider_ = new ArchiverApplianceProvider(this);
  } else {
    archiveProvider_ = QtedmPluginManager::instance().createArchiveProvider(
        providerId, this, &archiveProviderError_);
    if (archiveProvider_) {
      QObject *providerObject = dynamic_cast<QObject *>(archiveProvider_);
      if (!providerObject || providerObject->parent() != this) {
        archiveProviderError_ = QStringLiteral(
            "Plugin archive provider must return a QObject parented to its owner.");
        delete archiveProvider_;
        archiveProvider_ = nullptr;
      }
    }
  }
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
    pen.readAccessKnown = false;
    pen.canRead = false;
    pen.fieldType = -1;
    pen.elementCount = 1;
    pen.historicalSamples.clear();
    pen.liveSamples.clear();
    pen.archiveComplete = false;
    pen.archiveFailed = false;

    if (pen.channelName.isEmpty()) {
      continue;
    }

    subscribePen(i);
    if (element_->isArchivePlot()) {
      requestArchive(i);
    }
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
      },
      [this, index](bool canRead, bool canWrite) {
        handleAccessRightsEvent(index, canRead, canWrite);
      },
      ChannelDeliveryMode::kRealtime);
}

void StripChartRuntime::requestArchive(int index)
{
  if (!started_ || !element_ || !element_->isArchivePlot()
      || index < 0 || index >= kStripChartPenCount) {
    return;
  }
  if (!archiveProvider_) {
    element_->setArchiveStatus(QStringLiteral(
        "%1 Live data continues.")
        .arg(archiveProviderError_.isEmpty()
            ? QStringLiteral("Archive provider is unavailable.")
            : archiveProviderError_));
    return;
  }
  PenState &pen = pens_[index];
  if (pen.archiveRequest) {
    pen.archiveRequest->cancel();
  }
  const quint64 generation = ++pen.archiveGeneration;
  const QDateTime to = QDateTime::currentDateTimeUtc();
  ArchiveQuery query;
  query.channel = stripPvProtocol(pen.channelName);
  query.to = to;
  query.from = to.addMSecs(-static_cast<qint64>(
      element_->historyDurationSeconds() * 1000.0));
  query.maximumPoints = element_->archiveMaximumPoints();
  element_->setArchiveStatus(QStringLiteral("Loading archive history…"));

  QPointer<StripChartRuntime> self(this);
  pen.archiveRequest = archiveProvider_->query(query, this,
      [self, index, generation](const ArchiveResult &result) {
        if (!self || !self->started_ || !self->element_
            || index < 0 || index >= kStripChartPenCount) {
          return;
        }
        PenState &current = self->pens_[index];
        if (current.archiveGeneration != generation) {
          return;
        }
        current.archiveRequest.clear();
        current.archiveComplete = true;
        current.archiveFailed = !result.ok();
        if (result.ok()) {
          current.historicalSamples = result.samples;
          self->element_->setArchiveStatus(
              result.samples.isEmpty()
              ? QStringLiteral("Archive returned no samples; live data continues.")
              : QStringLiteral("Loaded %1 archive sample(s).")
                    .arg(result.samples.size()));
          self->applyMergedHistory(index);
        } else if (!result.cancelled) {
          self->element_->setArchiveStatus(
              QStringLiteral("%1 Live data continues.").arg(result.error));
        }
      });
}

void StripChartRuntime::applyMergedHistory(int index)
{
  if (!started_ || !element_ || index < 0
      || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  const qint64 minimumTimestamp = QDateTime::currentMSecsSinceEpoch()
      - static_cast<qint64>(element_->historyDurationSeconds() * 1000.0);
  const QVector<ArchiveSample> merged =
      ArchiverApplianceProvider::mergeAndDecimate(
          pen.historicalSamples,
          element_->archiveLiveMerge() ? pen.liveSamples
                                       : QVector<ArchiveSample>{},
          element_->archiveMaximumPoints(), minimumTimestamp);
  QVector<double> values;
  QVector<qint64> timestamps;
  values.reserve(merged.size());
  timestamps.reserve(merged.size());
  for (const ArchiveSample &sample : merged) {
    values.append(sample.value);
    timestamps.append(sample.timestampMs);
  }
  const qint64 windowEnd = QDateTime::currentMSecsSinceEpoch();
  invokeOnElement([index, values, timestamps, minimumTimestamp,
                      windowEnd](StripChartElement *element) {
    element->replaceRuntimeHistory(index, values, timestamps,
        minimumTimestamp, windowEnd);
  });
}

void StripChartRuntime::handleAccessRightsEvent(int index, bool canRead,
    bool canWrite)
{
  Q_UNUSED(canWrite);
  if (!started_ || index < 0 || index >= kStripChartPenCount) {
    return;
  }

  PenState &pen = pens_[index];
  pen.readAccessKnown = true;
  pen.canRead = canRead;

  invokeOnElement([index, canRead](StripChartElement *element) {
    element->setRuntimeReadAccessKnown(index, true);
    element->setRuntimeReadAccess(index, canRead);
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
    if (pen.readAccessKnown) {
      const bool canRead = pen.canRead;
      invokeOnElement([index, canRead](StripChartElement *element) {
        element->setRuntimeReadAccessKnown(index, true);
        element->setRuntimeReadAccess(index, canRead);
      });
    }

    if (data.hasControlInfo) {
      const double low = data.lopr;
      const double high = data.hopr;
      invokeOnElement([index, low, high](StripChartElement *element) {
        element->setRuntimeLimits(index, low, high);
      });
    }
  } else {
    pen.connected = false;
    pen.readAccessKnown = false;
    pen.canRead = false;
    if (element_ && element_->isArchivePlot() && pen.archiveComplete
        && !pen.archiveFailed && !pen.historicalSamples.isEmpty()) {
      invokeOnElement([index](StripChartElement *element) {
        element->setRuntimeConnected(index, true);
        element->setRuntimeReadAccessKnown(index, true);
        element->setRuntimeReadAccess(index, true);
        element->setArchiveStatus(
            QStringLiteral("Live PV disconnected; showing archive history."));
      });
      return;
    }
    invokeOnElement([index](StripChartElement *element) {
      element->setRuntimeConnected(index, false);
      element->setRuntimeReadAccessKnown(index, false);
      element->setRuntimeReadAccess(index, false);
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

  PenState &pen = pens_[index];
  if (!pen.readAccessKnown || !pen.canRead) {
    pen.readAccessKnown = true;
    pen.canRead = true;
    invokeOnElement([index](StripChartElement *element) {
      element->setRuntimeReadAccessKnown(index, true);
      element->setRuntimeReadAccess(index, true);
    });
  }

  const double numericValue = data.numericValue;
  qint64 timestampMs = 0;
  if (data.hasTimestamp) {
    timestampMs = epicsTimestampToMs(data.timestamp);
  } else {
    timestampMs = QDateTime::currentMSecsSinceEpoch();
  }

  if (element_ && element_->isArchivePlot()) {
    ArchiveSample live;
    live.timestampMs = timestampMs;
    live.value = numericValue;
    live.status = data.status;
    live.severity = data.severity;
    pen.liveSamples.append(live);
    const int liveLimit = std::max(element_->archiveMaximumPoints(), 2);
    if (pen.liveSamples.size() > liveLimit) {
      pen.liveSamples.remove(0, pen.liveSamples.size() - liveLimit);
    }
    if (pen.archiveComplete && !pen.archiveFailed) {
      if (element_->archiveLiveMerge()) {
        applyMergedHistory(index);
      }
      return;
    }
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
  pen.readAccessKnown = false;
  pen.canRead = false;
  pen.fieldType = -1;
  pen.elementCount = 1;
  pen.historicalSamples.clear();
  pen.liveSamples.clear();
  pen.archiveComplete = false;
  pen.archiveFailed = false;
}

void StripChartRuntime::unsubscribePen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  if (pen.archiveRequest) {
    pen.archiveRequest->cancel();
    pen.archiveRequest.clear();
  }
  ++pen.archiveGeneration;
  pen.subscription.reset();
}
