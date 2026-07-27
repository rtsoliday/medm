#include "ntndarray_image_runtime.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>

#include <algorithm>

#include "ntndarray_image_element.h"
#include "pv_channel_manager.h"
#include "statistics_tracker.h"

namespace {
constexpr int kPollIntervalMilliseconds = 20;
}

class NtNdArrayDecodeTask : public QRunnable
{
public:
  NtNdArrayDecodeTask(QPointer<NtNdArrayImageRuntime> runtime,
      quint64 generation, const NtNdArrayFrame &frame,
      const NtNdArrayDecodeOptions &options)
    : runtime_(runtime)
    , generation_(generation)
    , frame_(frame)
    , options_(options)
  {
    setAutoDelete(true);
  }

  void run() override
  {
    const NtNdArrayDecodedFrame decoded =
        NtNdArrayImageDecoder::decode(frame_, options_);
    const QPointer<NtNdArrayImageRuntime> runtime = runtime_;
    QObject *dispatcher = QCoreApplication::instance();
    if (!runtime || !dispatcher) {
      return;
    }
    QMetaObject::invokeMethod(dispatcher,
        [runtime, generation = generation_, decoded]() {
          if (runtime) {
            runtime->decodeFinished(generation, decoded);
          }
        }, Qt::QueuedConnection);
  }

private:
  QPointer<NtNdArrayImageRuntime> runtime_;
  quint64 generation_ = 0;
  NtNdArrayFrame frame_;
  NtNdArrayDecodeOptions options_;
};

NtNdArrayImageRuntime::NtNdArrayImageRuntime(
    NtNdArrayImageElement *element)
  : HeatmapRuntime(element)
  , imageElement_(element)
{
  pollTimer_.setParent(this);
  pollTimer_.setInterval(kPollIntervalMilliseconds);
  pollTimer_.setTimerType(Qt::PreciseTimer);
  QObject::connect(&pollTimer_, &QTimer::timeout,
      this, [this]() { pollSource(); });
}

NtNdArrayImageRuntime::~NtNdArrayImageRuntime()
{
  stop();
}

void NtNdArrayImageRuntime::start()
{
  if (started_ || !imageElement_) {
    return;
  }

  const QString rawName = imageElement_->dataChannel().trimmed();
  const ParsedPvName parsed = parsePvName(rawName);
  if (parsed.protocol != PvProtocol::kPva || parsed.pvName.isEmpty()) {
    lastError_ = QStringLiteral(
        "NTNDArray image channels must use the pva:// prefix.");
    updateElementStatus(lastError_);
    return;
  }

  QString error;
  source_ = pvaNtNdArrayCreateSource(rawName, parsed.pvName, &error);
  if (!source_) {
    lastError_ = error.isEmpty()
        ? QStringLiteral("Could not create the NTNDArray PVA source.")
        : error;
    updateElementStatus(lastError_);
    return;
  }

  started_ = true;
  connected_ = false;
  decodeBusy_ = false;
  hasPendingFrame_ = false;
  droppedFrames_ = 0;
  lastError_.clear();
  ++generation_;
  StatisticsTracker::instance().registerDisplayObjectStarted();
  StatisticsTracker::instance().registerChannelCreated();
  imageElement_->clearNtNdArrayState();
  pollTimer_.start();
  pollSource();
}

void NtNdArrayImageRuntime::stop()
{
  if (!started_ && !source_) {
    return;
  }
  pollTimer_.stop();
  ++generation_;
  hasPendingFrame_ = false;
  pendingFrame_ = NtNdArrayFrame();
  decodeBusy_ = false;
  if (source_) {
    pvaNtNdArrayDestroySource(source_);
    source_ = nullptr;
    StatisticsTracker::instance().registerChannelDestroyed();
  }
  if (started_) {
    StatisticsTracker::instance().registerDisplayObjectStopped();
  }
  if (connected_) {
    StatisticsTracker::instance().registerChannelDisconnected();
  }
  connected_ = false;
  started_ = false;
  if (imageElement_) {
    imageElement_->clearNtNdArrayState();
  }
}

void NtNdArrayImageRuntime::pollSource()
{
  if (!started_ || !source_) {
    return;
  }
  const PvaNtNdArrayPollResult result = pvaNtNdArrayPoll(source_);
  if (result.connectionChanged) {
    if (result.connected) {
      StatisticsTracker::instance().registerChannelConnected();
    } else if (connected_) {
      StatisticsTracker::instance().registerChannelDisconnected();
    }
  }
  connected_ = result.connected;
  droppedFrames_ += static_cast<quint64>(
      std::max(0, result.droppedFrames));
  if (!result.error.trimmed().isEmpty()) {
    lastError_ = result.error;
  }
  if (result.hasFrame) {
    submitFrame(result.frame);
  }
  updateElementStatus();
}

void NtNdArrayImageRuntime::submitFrame(const NtNdArrayFrame &frame)
{
  if (!started_ || HeatmapRuntime::isGlobalUpdatesPaused()) {
    return;
  }
  StatisticsTracker::instance().registerUpdateRequest(true);
  if (decodeBusy_) {
    if (hasPendingFrame_) {
      ++droppedFrames_;
    }
    pendingFrame_ = frame;
    hasPendingFrame_ = true;
    return;
  }
  startDecode(frame);
}

void NtNdArrayImageRuntime::startDecode(const NtNdArrayFrame &frame)
{
  if (!imageElement_) {
    return;
  }
  decodeBusy_ = true;
  auto *task = new NtNdArrayDecodeTask(this, generation_, frame,
      imageElement_->decodeOptions());
  QThreadPool::globalInstance()->start(task);
}

void NtNdArrayImageRuntime::decodeFinished(quint64 generation,
    const NtNdArrayDecodedFrame &decoded)
{
  if (generation != generation_ || !started_) {
    return;
  }
  decodeBusy_ = false;
  if (decoded.valid) {
    lastError_.clear();
    StatisticsTracker::instance().registerUpdateExecuted();
  } else {
    lastError_ = decoded.error;
  }
  if (imageElement_) {
    imageElement_->setDecodedFrame(decoded);
    updateElementStatus();
  }
  if (hasPendingFrame_) {
    NtNdArrayFrame next = std::move(pendingFrame_);
    pendingFrame_ = NtNdArrayFrame();
    hasPendingFrame_ = false;
    startDecode(next);
  }
}

void NtNdArrayImageRuntime::updateElementStatus(const QString &error)
{
  if (!imageElement_) {
    return;
  }
  const QString message = error.isEmpty() ? lastError_ : error;
  imageElement_->setStreamStatus(
      connected_, droppedFrames_, message);
}
