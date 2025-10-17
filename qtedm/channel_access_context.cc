#include "channel_access_context.h"

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>

#include <cadef.h>

namespace {
constexpr int kPollIntervalMs = 50;
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
  if (pollTimer_) {
    pollTimer_->stop();
    pollTimer_->deleteLater();
    pollTimer_ = nullptr;
  }
  if (initialized_) {
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
  int status = ca_context_create(ca_disable_preemptive_callback);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to initialize EPICS Channel Access context:" << ca_message(status);
    return;
  }

  initialized_ = true;

  pollTimer_ = new QTimer(this);
  pollTimer_->setInterval(kPollIntervalMs);
  pollTimer_->setTimerType(Qt::PreciseTimer);
  QObject::connect(pollTimer_, &QTimer::timeout, this,
      &ChannelAccessContext::pollOnce);
  pollTimer_->start();
}

void ChannelAccessContext::pollOnce()
{
  if (!initialized_) {
    return;
  }
  ca_poll();
}
