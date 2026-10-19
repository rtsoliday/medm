#include "ntndarray_image_runtime.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>

#include <algorithm>

#include "ntndarray_image_element.h"
#include "pv_channel_manager.h"
#include "statistics_tracker.h"
#include "image_memory_budget.h"

namespace {
constexpr int kPollIntervalMilliseconds = 20;

ImageMemoryBudget &imageMemoryBudget()
{
  /* Application-owned raw frames, decoded images and transform scratch.
   * EPICS transport buffers are controlled separately by the provider. */
  static ImageMemoryBudget budget([]() -> quint64 {
    bool ok = false;
    const int megabytes = qEnvironmentVariableIntValue(
        "QTEDM_IMAGE_MEMORY_MB", &ok);
    return static_cast<quint64>(ok && megabytes > 0
        ? std::clamp(megabytes, 1, 16384) : 1024) * 1024 * 1024;
  }());
  return budget;
}
}

class NtNdArrayDecodeTask : public QRunnable
{
public:
  NtNdArrayDecodeTask(QPointer<NtNdArrayImageRuntime> runtime,
      quint64 generation, const NtNdArrayFrame &frame,
      const NtNdArrayDecodeOptions &options,
      std::shared_ptr<std::atomic<bool>> cancelled)
    : runtime_(runtime)
    , generation_(generation)
    , frame_(frame)
    , options_(options)
    , cancelled_(std::move(cancelled))
  {
    setAutoDelete(true);
  }

  void run() override
  {
    if (cancelled_->load(std::memory_order_relaxed)) {
      return;
    }
    const NtNdArrayDecodedFrame decoded =
        NtNdArrayImageDecoder::decode(frame_, options_, cancelled_.get());
    if (cancelled_->load(std::memory_order_relaxed)) {
      return;
    }
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
  std::shared_ptr<std::atomic<bool>> cancelled_;
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
  if (decodeCancelled_) {
    decodeCancelled_->store(true, std::memory_order_relaxed);
    decodeCancelled_.reset();
  }
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
  const PvaNtNdArrayPollResult result = pvaNtNdArrayPoll(source_,
      imageElement_->decodeOptions().maximumInputBytes);
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
  } else if (!decodeBusy_ && hasPendingFrame_) {
    NtNdArrayFrame next = std::move(pendingFrame_);
    pendingFrame_ = NtNdArrayFrame();
    hasPendingFrame_ = false;
    startDecode(next);
  }
  updateElementStatus();
}

void NtNdArrayImageRuntime::submitFrame(const NtNdArrayFrame &frame)
{
  if (!started_) {
    return;
  }
  StatisticsTracker::instance().registerUpdateRequest(true);
  if (!imageElement_) {
    return;
  }
  quint64 workingBytes = 0;
  QString error;
  if (!NtNdArrayImageDecoder::validate(frame, imageElement_->decodeOptions(),
          &workingBytes, &error)) {
    ++droppedFrames_;
    lastError_ = error;
    updateElementStatus();
    return;
  }
  /* Release the obsolete pending reservation before reserving its successor. */
  if (hasPendingFrame_) {
    ++droppedFrames_;
    pendingFrame_ = NtNdArrayFrame();
    hasPendingFrame_ = false;
  }
  NtNdArrayFrame retained = frame;
  retained.memoryReservation = imageMemoryBudget().reserve(workingBytes);
  if (!retained.memoryReservation) {
    ++droppedFrames_;
    lastError_ = QStringLiteral("NTNDArray process memory budget exhausted "
        "(QTEDM_IMAGE_MEMORY_MB).");
    updateElementStatus();
    return;
  }
  if (decodeBusy_) {
    pendingFrame_ = std::move(retained);
    hasPendingFrame_ = true;
    return;
  }
  startDecode(retained);
}

void NtNdArrayImageRuntime::startDecode(const NtNdArrayFrame &frame)
{
  if (!imageElement_) {
    return;
  }
  decodeBusy_ = true;
  decodeCancelled_ = std::make_shared<std::atomic<bool>>(false);
  auto *task = new NtNdArrayDecodeTask(this, generation_, frame,
      imageElement_->decodeOptions(), decodeCancelled_);
  /* Never build an unbounded global thread-pool queue while displays are
   * repeatedly opened and closed. Retry the newest frame on the poll timer. */
  if (!QThreadPool::globalInstance()->tryStart(task)) {
    delete task;
    decodeBusy_ = false;
    decodeCancelled_.reset();
    pendingFrame_ = frame;
    hasPendingFrame_ = true;
  }
}

void NtNdArrayImageRuntime::decodeFinished(quint64 generation,
    const NtNdArrayDecodedFrame &decoded)
{
  if (generation != generation_ || !started_) {
    return;
  }
  decodeBusy_ = false;
  decodeCancelled_.reset();
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
