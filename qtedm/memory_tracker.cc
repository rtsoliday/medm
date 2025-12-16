#include "memory_tracker.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace {

/** Read memory stats from /proc/self/statm on Linux */
MemorySnapshot readProcStatm()
{
  MemorySnapshot snapshot;

#ifdef Q_OS_LINUX
  QFile statm(QStringLiteral("/proc/self/statm"));
  if (statm.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QByteArray line = statm.readLine();
    statm.close();

    /* Format: size resident shared text lib data dt
     * All values are in pages */
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
      pageSize = 4096;  /* Fallback */
    }
    const long pageSizeKB = pageSize / 1024;

    QList<QByteArray> parts = line.split(' ');
    if (parts.size() >= 6) {
      snapshot.vmSizeKB = parts[0].toLongLong() * pageSizeKB;
      snapshot.vmRssKB = parts[1].toLongLong() * pageSizeKB;
      snapshot.sharedKB = parts[2].toLongLong() * pageSizeKB;
      /* parts[3] is text (code), parts[4] is lib (unused), parts[5] is data+stack */
      snapshot.dataKB = parts[5].toLongLong() * pageSizeKB;
    }
  }
#endif

  return snapshot;
}

}  // namespace

MemoryTracker &MemoryTracker::instance()
{
  static MemoryTracker tracker;
  return tracker;
}

MemoryTracker::MemoryTracker(QObject *parent)
  : QObject(parent)
{
  parseEnvironment();
  if (enabled_) {
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MemoryTracker::logNow);
  }
}

MemoryTracker::~MemoryTracker()
{
  stop();
  if (logFile_) {
    logFile_->close();
    delete logFile_;
    logFile_ = nullptr;
  }
}

void MemoryTracker::parseEnvironment()
{
  const QByteArray env = qgetenv("TRACK_MEM");
  if (env.isEmpty()) {
    enabled_ = false;
    return;
  }

  enabled_ = true;
  QString value = QString::fromLocal8Bit(env).trimmed();

  /* Parse format: [interval:]/path or interval or 1 */
  if (value.contains(QLatin1Char(':'))) {
    int colonPos = value.indexOf(QLatin1Char(':'));
    QString intervalPart = value.left(colonPos);
    QString pathPart = value.mid(colonPos + 1);

    bool ok = false;
    int interval = intervalPart.toInt(&ok);
    if (ok && interval > 0) {
      intervalSeconds_ = interval;
    }
    if (!pathPart.isEmpty()) {
      logFilePath_ = pathPart;
    }
  } else if (value.startsWith(QLatin1Char('/'))) {
    /* Just a path */
    logFilePath_ = value;
  } else {
    /* Try to parse as interval */
    bool ok = false;
    int interval = value.toInt(&ok);
    if (ok && interval > 0) {
      intervalSeconds_ = interval;
    }
  }
}

void MemoryTracker::start()
{
  if (!enabled_ || !timer_) {
    return;
  }

  elapsedTimer_.start();
  initialSnapshot_ = currentMemory();
  lastSnapshot_ = initialSnapshot_;

  /* Open log file if specified */
  if (!logFilePath_.isEmpty()) {
    logFile_ = new QFile(logFilePath_);
    if (!logFile_->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
      fprintf(stderr, "TRACK_MEM: Failed to open log file: %s\n",
          logFilePath_.toLocal8Bit().constData());
      delete logFile_;
      logFile_ = nullptr;
    }
  }

  writeHeader();
  logNow();  /* Log initial state */

  timer_->start(intervalSeconds_ * 1000);
}

void MemoryTracker::stop()
{
  if (timer_) {
    timer_->stop();
  }
}

MemorySnapshot MemoryTracker::currentMemory()
{
  return readProcStatm();
}

void MemoryTracker::logNow()
{
  MemorySnapshot snapshot = currentMemory();
  snapshot.elapsedSeconds = elapsedTimer_.elapsed() / 1000.0;
  writeSnapshot(snapshot);
  lastSnapshot_ = snapshot;
}

void MemoryTracker::writeHeader()
{
  if (headerWritten_) {
    return;
  }

  const char *header = "# QtEDM Memory Tracking\n"
                       "# elapsed_sec,vm_size_kb,vm_rss_kb,shared_kb,data_kb,rss_delta_kb\n";

  if (logFile_) {
    QTextStream out(logFile_);
    out << header;
    out.flush();
  } else {
    fprintf(stderr, "%s", header);
    fflush(stderr);
  }

  headerWritten_ = true;
}

void MemoryTracker::writeSnapshot(const MemorySnapshot &snapshot)
{
  int64_t rssDelta = snapshot.vmRssKB - initialSnapshot_.vmRssKB;

  QString line = QString::asprintf("%.1f,%lld,%lld,%lld,%lld,%lld\n",
      snapshot.elapsedSeconds,
      static_cast<long long>(snapshot.vmSizeKB),
      static_cast<long long>(snapshot.vmRssKB),
      static_cast<long long>(snapshot.sharedKB),
      static_cast<long long>(snapshot.dataKB),
      static_cast<long long>(rssDelta));

  if (logFile_) {
    QTextStream out(logFile_);
    out << line;
    out.flush();
  } else {
    fprintf(stderr, "TRACK_MEM: %s", line.toLocal8Bit().constData());
    fflush(stderr);
  }
}

#include "moc_memory_tracker.cpp"
