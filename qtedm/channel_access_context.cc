#include "channel_access_context.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QDebug>

#include <cadef.h>

#include "startup_timing.h"

namespace {
/* Fallback poll interval - with FD registration this mainly serves as a
 * safety net in case any events are missed. Can be longer than before. */
constexpr int kPollIntervalMs = 100;
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
  QTEDM_TIMING_MARK("Channel Access: Creating context");
  int status = ca_context_create(ca_disable_preemptive_callback);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to initialize EPICS Channel Access context:" << ca_message(status);
    return;
  }

  initialized_ = true;

  /* Register for FD notifications - this allows us to process CA events
   * immediately when data arrives on sockets, rather than waiting for
   * the poll timer. This is the key optimization that makes MEDM fast. */
  QTEDM_TIMING_MARK("Channel Access: Registering FD callback");
  status = ca_add_fd_registration(&ChannelAccessContext::fdRegistrationCallback, this);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to register CA FD callback:" << ca_message(status)
               << "- falling back to timer-only polling";
  }

  QTEDM_TIMING_MARK("Channel Access: Starting fallback poll timer");

  /* Keep a fallback timer but at a longer interval since FD registration
   * handles the primary event processing */
  pollTimer_ = new QTimer(this);
  pollTimer_->setInterval(kPollIntervalMs);
  pollTimer_->setTimerType(Qt::CoarseTimer);
  QObject::connect(pollTimer_, &QTimer::timeout, this,
      &ChannelAccessContext::pollOnce);
  pollTimer_->start();
  QTEDM_TIMING_MARK("Channel Access: Initialization complete");
}

void ChannelAccessContext::pollOnce()
{
  if (!initialized_) {
    return;
  }
  ca_poll();
}

void ChannelAccessContext::handleFdActivity(int fd)
{
  Q_UNUSED(fd);
  if (!initialized_) {
    return;
  }
  /* Process pending CA events immediately when socket has data */
  ca_poll();
}

void ChannelAccessContext::fdRegistrationCallback(void *user, int fd, int opened)
{
  auto *self = static_cast<ChannelAccessContext *>(user);
  if (!self) {
    return;
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
