#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QDebug>

#include <cstdio>
#include <cstdlib>

/*
 * Startup timing diagnostics for QtEDM.
 *
 * This module provides timing instrumentation to track how long various
 * phases of display startup take, from launch to when the display is
 * fully populated and mostly idle waiting for PV changes.
 *
 * Enable diagnostics by setting the environment variable:
 *   QTEDM_TIMING_DIAGNOSTICS=1
 *
 * Output is printed to stderr with timestamps relative to program start.
 */

class StartupTiming {
public:
  static StartupTiming &instance()
  {
    static StartupTiming instance;
    return instance;
  }

  /* Check if timing diagnostics are enabled */
  bool isEnabled() const
  {
    return enabled_;
  }

  /* Record start of a phase or event */
  void mark(const char *event)
  {
    if (!enabled_) {
      return;
    }
    const qint64 elapsed = timer_.elapsed();
    fprintf(stderr, "[TIMING] %8lld ms : %s\n", static_cast<long long>(elapsed), event);
    fflush(stderr);
  }

  /* Record start of a phase with additional detail */
  void mark(const char *event, const QString &detail)
  {
    if (!enabled_) {
      return;
    }
    const qint64 elapsed = timer_.elapsed();
    fprintf(stderr, "[TIMING] %8lld ms : %s: %s\n",
        static_cast<long long>(elapsed), event,
        detail.toLocal8Bit().constData());
    fflush(stderr);
  }

  /* Record start of a phase with integer detail */
  void mark(const char *event, int count)
  {
    if (!enabled_) {
      return;
    }
    const qint64 elapsed = timer_.elapsed();
    fprintf(stderr, "[TIMING] %8lld ms : %s: %d\n",
        static_cast<long long>(elapsed), event, count);
    fflush(stderr);
  }

  /* Record start of a phase with a duration measurement */
  void markDuration(const char *event, qint64 durationMs)
  {
    if (!enabled_) {
      return;
    }
    const qint64 elapsed = timer_.elapsed();
    fprintf(stderr, "[TIMING] %8lld ms : %s (took %lld ms)\n",
        static_cast<long long>(elapsed), event,
        static_cast<long long>(durationMs));
    fflush(stderr);
  }

  /* Get current elapsed time in milliseconds */
  qint64 elapsedMs() const
  {
    return timer_.elapsed();
  }

  /* Reset the timer (normally only done at program start) */
  void reset()
  {
    timer_.restart();
  }

private:
  StartupTiming()
  {
    timer_.start();
    const QByteArray env = qgetenv("QTEDM_TIMING_DIAGNOSTICS");
    enabled_ = (!env.isEmpty() && env != "0");
    if (enabled_) {
      fprintf(stderr, "[TIMING] Startup timing diagnostics enabled\n");
      fprintf(stderr, "[TIMING] %8lld ms : Program started\n", 0LL);
      fflush(stderr);
    }
  }

  QElapsedTimer timer_;
  bool enabled_ = false;
};

/* Convenience macro for timing marks */
#define QTEDM_TIMING_MARK(event) \
  StartupTiming::instance().mark(event)

#define QTEDM_TIMING_MARK_DETAIL(event, detail) \
  StartupTiming::instance().mark(event, detail)

#define QTEDM_TIMING_MARK_COUNT(event, count) \
  StartupTiming::instance().mark(event, count)

/* RAII helper to time a scope and report duration */
class ScopedTiming {
public:
  explicit ScopedTiming(const char *event)
    : event_(event)
    , enabled_(StartupTiming::instance().isEnabled())
  {
    if (enabled_) {
      startTime_ = StartupTiming::instance().elapsedMs();
    }
  }

  ~ScopedTiming()
  {
    if (enabled_) {
      const qint64 endTime = StartupTiming::instance().elapsedMs();
      StartupTiming::instance().markDuration(event_, endTime - startTime_);
    }
  }

private:
  const char *event_;
  bool enabled_;
  qint64 startTime_ = 0;
};

#define QTEDM_SCOPED_TIMING(event) \
  ScopedTiming scopedTiming_##__LINE__(event)

