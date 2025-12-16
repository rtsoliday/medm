#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QString>

#include <cstdint>

class QTimer;
class QFile;

/**
 * Memory usage snapshot at a point in time.
 * All values in kilobytes (KB).
 */
struct MemorySnapshot {
  double elapsedSeconds = 0.0;
  int64_t vmSizeKB = 0;      /* Virtual memory size */
  int64_t vmRssKB = 0;       /* Resident set size */
  int64_t sharedKB = 0;      /* Shared pages */
  int64_t dataKB = 0;        /* Data + stack */
};

/**
 * MemoryTracker monitors process memory usage over time.
 * 
 * Activated by setting the TRACK_MEM environment variable:
 *   TRACK_MEM=1           Log to stderr every 60 seconds
 *   TRACK_MEM=30          Log to stderr every 30 seconds  
 *   TRACK_MEM=/path/file  Log to file every 60 seconds
 *   TRACK_MEM=30:/path    Log to file every 30 seconds
 *
 * Output format is CSV-compatible for easy analysis:
 *   elapsed_sec,vm_size_kb,vm_rss_kb,shared_kb,data_kb
 */
class MemoryTracker : public QObject
{
  Q_OBJECT

public:
  static MemoryTracker &instance();

  /** Check if tracking is enabled (TRACK_MEM is set) */
  bool isEnabled() const { return enabled_; }

  /** Start tracking (called automatically if enabled) */
  void start();

  /** Stop tracking */
  void stop();

  /** Get current memory snapshot */
  static MemorySnapshot currentMemory();

  /** Force an immediate log entry */
  void logNow();

private:
  explicit MemoryTracker(QObject *parent = nullptr);
  ~MemoryTracker() override;

  void parseEnvironment();
  void writeHeader();
  void writeSnapshot(const MemorySnapshot &snapshot);

  bool enabled_ = false;
  int intervalSeconds_ = 60;
  QString logFilePath_;
  QTimer *timer_ = nullptr;
  QFile *logFile_ = nullptr;
  QElapsedTimer elapsedTimer_;
  bool headerWritten_ = false;
  MemorySnapshot initialSnapshot_;
  MemorySnapshot lastSnapshot_;
};
