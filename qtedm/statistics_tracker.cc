#include "statistics_tracker.h"

#include <algorithm>

#include <QMutexLocker>

namespace {
int clampNonNegative(int value)
{
  return std::max(value, 0);
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
  QMutexLocker locker(&mutex_);
  ++objectCount_;
}

void StatisticsTracker::registerDisplayObjectStopped()
{
  QMutexLocker locker(&mutex_);
  objectCount_ = clampNonNegative(objectCount_ - 1);
}

void StatisticsTracker::registerChannelCreated()
{
  QMutexLocker locker(&mutex_);
  ++channelCount_;
}

void StatisticsTracker::registerChannelDestroyed()
{
  QMutexLocker locker(&mutex_);
  channelCount_ = clampNonNegative(channelCount_ - 1);
}

void StatisticsTracker::registerChannelConnected()
{
  QMutexLocker locker(&mutex_);
  ++channelConnected_;
}

void StatisticsTracker::registerChannelDisconnected()
{
  QMutexLocker locker(&mutex_);
  channelConnected_ = clampNonNegative(channelConnected_ - 1);
}

void StatisticsTracker::registerCaEvent()
{
  QMutexLocker locker(&mutex_);
  ++caEventCount_;
}

void StatisticsTracker::registerUpdateRequest(bool accepted)
{
  QMutexLocker locker(&mutex_);
  if (accepted) {
    ++updateRequestCount_;
  } else {
    ++updateDiscardCount_;
  }
}

void StatisticsTracker::registerUpdateExecuted()
{
  QMutexLocker locker(&mutex_);
  ++updateExecutedCount_;
}

StatisticsSnapshot StatisticsTracker::snapshotAndReset()
{
  QMutexLocker locker(&mutex_);
  if (!timerInitialized_) {
    intervalTimer_.start();
    timerInitialized_ = true;
  }
  double interval = intervalTimer_.elapsed() / 1000.0;
  intervalTimer_.restart();

  StatisticsSnapshot snapshot;
  snapshot.intervalSeconds = interval;
  snapshot.channelCount = channelCount_;
  snapshot.channelConnected = channelConnected_;
  snapshot.objectCount = objectCount_;
  snapshot.caEventCount = caEventCount_;
  snapshot.updateRequestCount = updateRequestCount_;
  snapshot.updateDiscardCount = updateDiscardCount_;
  snapshot.updateExecuted = updateExecutedCount_;
  snapshot.updateRequestQueued = updateRequestQueued_;

  caEventCount_ = 0;
  updateRequestCount_ = 0;
  updateDiscardCount_ = 0;
  updateExecutedCount_ = 0;

  return snapshot;
}

void StatisticsTracker::reset()
{
  QMutexLocker locker(&mutex_);
  caEventCount_ = 0;
  updateRequestCount_ = 0;
  updateDiscardCount_ = 0;
  updateExecutedCount_ = 0;
  updateRequestQueued_ = 0;
  intervalTimer_.restart();
  timerInitialized_ = true;
}
