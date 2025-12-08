#include "channel_access_context.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QDebug>

#include <cadef.h>

#include "startup_timing.h"

namespace {
/* Poll interval - needs to be fast enough to keep up with CA responses.
 * We use a very short interval and rely on the timer being the primary
 * driver of CA event processing. */
constexpr int kPollIntervalMs = 1;

/* Track if we're currently inside pollOnce to prevent re-entrancy */
static bool inPollOnce = false;
}

ChannelAccessContext &ChannelAccessContext::instance()
{
  static ChannelAccessContext *context = []() {
    auto *ctx = new ChannelAccessContext;
    return ctx;
  }();
  return *context;
}

ChannelAccessContext::ChannelAccessContext()
  : QObject(QCoreApplication::instance())
{
}

ChannelAccessContext::~ChannelAccessContext()
{
  /* Clean up socket notifiers */
  for (auto *notifier : socketNotifiers_) {
    notifier->setEnabled(false);
    notifier->deleteLater();
  }
  socketNotifiers_.clear();

  if (pollTimer_) {
    pollTimer_->stop();
    pollTimer_->deleteLater();
    pollTimer_ = nullptr;
  }
  if (initialized_) {
    /* Unregister FD callback before destroying context */
    ca_add_fd_registration(nullptr, nullptr);
    ca_context_destroy();
    initialized_ = false;
  }
}

void ChannelAccessContext::ensureInitialized()
{
  if (initialized_) {
    return;
  }
  initialize();
}

bool ChannelAccessContext::isInitialized() const
{
  return initialized_;
}

void ChannelAccessContext::initialize()
{
  QTEDM_TIMING_MARK("Channel Access: Creating context with preemptive callbacks");
  /* Use preemptive callbacks so CA can process events on its own thread.
   * This means our CA callbacks will be invoked from a non-main thread,
   * so we must use thread-safe mechanisms (Qt signals/QueuedConnection)
   * to marshal data to the main Qt thread for UI updates. */
  int status = ca_context_create(ca_enable_preemptive_callback);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to initialize EPICS Channel Access context:" << ca_message(status);
    return;
  }

  initialized_ = true;

  /* With preemptive callbacks, we don't need FD registration or polling.
   * CA will process events on its own thread and invoke our callbacks
   * asynchronously. We still set up a timer for deferred operations. */
  QTEDM_TIMING_MARK("Channel Access: Setting up deferred processing timer");
  
  /* Light-weight timer for deferred operations - no ca_poll needed */
  pollTimer_ = new QTimer(this);
  pollTimer_->setInterval(50);  /* 50ms for deferred processing */
  pollTimer_->setTimerType(Qt::CoarseTimer);
  QObject::connect(pollTimer_, &QTimer::timeout, this,
      &ChannelAccessContext::pollOnce);
  pollTimer_->start();
  QTEDM_TIMING_MARK("Channel Access: Initialization complete");
}

void ChannelAccessContext::pollOnce()
{
  if (!initialized_ || inPollOnce) {
    return;
  }
  inPollOnce = true;
  ++timerPollCount_;

  /* With preemptive callbacks, CA processes events on its own thread.
   * We only use this timer for deferred flush operations. 
   * Call ca_flush_io() to ensure any pending requests are sent. */
  ca_flush_io();
  inPollOnce = false;
}

void ChannelAccessContext::handleFdActivity(int fd)
{
  Q_UNUSED(fd);
  /* With preemptive callbacks, CA handles events on its own thread.
   * This function is kept for compatibility but does nothing. */
}

void ChannelAccessContext::maybeReportPollStats(const char *trigger)
{
  /* Only report when timing diagnostics are enabled */
  if (!StartupTiming::instance().isEnabled()) {
    return;
  }

  qint64 now = StartupTiming::instance().elapsedMs();
  /* Report every 500ms during startup */
  if (now - lastPollReportTime_ >= 500) {
    fprintf(stderr, "[TIMING] %8lld ms : ca_poll stats: timer=%d fd=%d (trigger=%s)\n",
        now, timerPollCount_, fdPollCount_, trigger);
    fflush(stderr);
    lastPollReportTime_ = now;
  }
}

void ChannelAccessContext::fdRegistrationCallback(void *user, int fd, int opened)
{
  auto *self = static_cast<ChannelAccessContext *>(user);
  if (!self) {
    return;
  }

  /* Debug: report FD registration events */
  if (StartupTiming::instance().isEnabled()) {
    fprintf(stderr, "[TIMING] %8lld ms : CA FD %s: fd=%d\n",
        StartupTiming::instance().elapsedMs(),
        opened ? "opened" : "closed", fd);
    fflush(stderr);
  }

  if (opened) {
    /* New file descriptor opened - create a socket notifier for it */
    if (!self->socketNotifiers_.contains(fd)) {
      auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, self);
      QObject::connect(notifier, &QSocketNotifier::activated, self,
          &ChannelAccessContext::handleFdActivity);
      self->socketNotifiers_.insert(fd, notifier);
    }
  } else {
    /* File descriptor closed - remove and delete the notifier */
    if (self->socketNotifiers_.contains(fd)) {
      auto *notifier = self->socketNotifiers_.take(fd);
      notifier->setEnabled(false);
      notifier->deleteLater();
    }
  }
}
