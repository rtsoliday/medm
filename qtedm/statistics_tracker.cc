#include "statistics_tracker.h"

namespace {
void decrementNonNegative(std::atomic<int> &counter)
{
  int current = counter.load(std::memory_order_relaxed);
  while (current > 0
      && !counter.compare_exchange_weak(current, current - 1,
          std::memory_order_relaxed, std::memory_order_relaxed)) {
  }
}
}

StatisticsTracker &StatisticsTracker::instance()
{
  static StatisticsTracker tracker;
  return tracker;
}

StatisticsTracker::StatisticsTracker()
{
  intervalTimer_.start();
  timerInitialized_ = true;
}

void StatisticsTracker::registerDisplayObjectStarted()
{
  objectCount_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsTracker::registerDisplayObjectStopped()
{
  decrementNonNegative(objectCount_);
}

void StatisticsTracker::registerChannelCreated()
{
  channelCount_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsTracker::registerChannelDestroyed()
{
  decrementNonNegative(channelCount_);
}

void StatisticsTracker::registerChannelConnected()
{
  channelConnected_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsTracker::registerChannelDisconnected()
{
  decrementNonNegative(channelConnected_);
}

void StatisticsTracker::registerCaEvent()
{
  caEventCount_.fetch_add(1, std::memory_order_relaxed);
}

void StatisticsTracker::registerUpdateRequest(bool accepted)
{
  if (accepted) {
    updateRequestCount_.fetch_add(1, std::memory_order_relaxed);
  } else {
    updateDiscardCount_.fetch_add(1, std::memory_order_relaxed);
  }
}

void StatisticsTracker::registerUpdateExecuted()
{
  updateExecutedCount_.fetch_add(1, std::memory_order_relaxed);
}

StatisticsSnapshot StatisticsTracker::snapshotAndReset()
{
  if (!timerInitialized_) {
    intervalTimer_.start();
    timerInitialized_ = true;
  }
  double interval = intervalTimer_.elapsed() / 1000.0;
  intervalTimer_.restart();

  StatisticsSnapshot snapshot;
  snapshot.intervalSeconds = interval;
  snapshot.channelCount = channelCount_.load(std::memory_order_relaxed);
  snapshot.channelConnected =
      channelConnected_.load(std::memory_order_relaxed);
  snapshot.objectCount = objectCount_.load(std::memory_order_relaxed);
  snapshot.caEventCount =
      caEventCount_.exchange(0, std::memory_order_relaxed);
  snapshot.updateRequestCount =
      updateRequestCount_.exchange(0, std::memory_order_relaxed);
  snapshot.updateDiscardCount =
      updateDiscardCount_.exchange(0, std::memory_order_relaxed);
  snapshot.updateExecuted =
      updateExecutedCount_.exchange(0, std::memory_order_relaxed);
  snapshot.updateRequestQueued =
      updateRequestQueued_.load(std::memory_order_relaxed);

  return snapshot;
}

void StatisticsTracker::reset()
{
  caEventCount_.store(0, std::memory_order_relaxed);
  updateRequestCount_.store(0, std::memory_order_relaxed);
  updateDiscardCount_.store(0, std::memory_order_relaxed);
  updateExecutedCount_.store(0, std::memory_order_relaxed);
  updateRequestQueued_.store(0, std::memory_order_relaxed);
  intervalTimer_.restart();
  timerInitialized_ = true;
}

std::pair<int, int> StatisticsTracker::channelCounts() const
{
  return {channelCount_.load(std::memory_order_relaxed),
      channelConnected_.load(std::memory_order_relaxed)};
}
