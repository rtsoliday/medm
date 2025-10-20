#pragma once

#include <QElapsedTimer>
#include <QMutex>

#include <utility>

struct StatisticsSnapshot {
  double intervalSeconds = 0.0;
  int channelCount = 0;
  int channelConnected = 0;
  int objectCount = 0;
  int caEventCount = 0;
  int updateRequestCount = 0;
  int updateDiscardCount = 0;
  int updateExecuted = 0;
  int updateRequestQueued = 0;
};

class StatisticsTracker {
public:
  static StatisticsTracker &instance();

  void registerDisplayObjectStarted();
  void registerDisplayObjectStopped();

  void registerChannelCreated();
  void registerChannelDestroyed();
  void registerChannelConnected();
  void registerChannelDisconnected();

  void registerCaEvent();
  void registerUpdateRequest(bool accepted);
  void registerUpdateExecuted();

  StatisticsSnapshot snapshotAndReset();
  void reset();

  std::pair<int, int> channelCounts() const;

private:
  StatisticsTracker();

  mutable QMutex mutex_;
  QElapsedTimer intervalTimer_;
  bool timerInitialized_ = false;

  int channelCount_ = 0;
  int channelConnected_ = 0;
  int objectCount_ = 0;
  int updateRequestQueued_ = 0;

  int caEventCount_ = 0;
  int updateRequestCount_ = 0;
  int updateDiscardCount_ = 0;
  int updateExecutedCount_ = 0;
};
