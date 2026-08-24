#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "audit_logger.h"
#include "statistics_tracker.h"

#define private public
#include "memory_tracker.h"
#undef private

class TestDiagnosticsTracking : public QObject
{
  Q_OBJECT

private slots:
  void statisticsSnapshotsResetIntervalCounters();
  void auditLogRecordsWritesBlocksAndEscapedValues();
  void memoryTrackerParsesConfigurationAndWritesCsv();
};

void TestDiagnosticsTracking::statisticsSnapshotsResetIntervalCounters()
{
  StatisticsTracker &tracker = StatisticsTracker::instance();
  tracker.reset();
  tracker.registerChannelCreated();
  tracker.registerChannelConnected();
  tracker.registerDisplayObjectStarted();
  tracker.registerCaEvent();
  tracker.registerCaEvent();
  tracker.registerUpdateRequest(true);
  tracker.registerUpdateRequest(false);
  tracker.registerUpdateExecuted();

  const StatisticsSnapshot first = tracker.snapshotAndReset();
  QCOMPARE(first.channelCount, 1);
  QCOMPARE(first.channelConnected, 1);
  QCOMPARE(first.objectCount, 1);
  QCOMPARE(first.caEventCount, 2);
  QCOMPARE(first.updateRequestCount, 1);
  QCOMPARE(first.updateDiscardCount, 1);
  QCOMPARE(first.updateExecuted, 1);

  const StatisticsSnapshot second = tracker.snapshotAndReset();
  QCOMPARE(second.channelCount, 1);
  QCOMPARE(second.channelConnected, 1);
  QCOMPARE(second.objectCount, 1);
  QCOMPARE(second.caEventCount, 0);
  QCOMPARE(second.updateRequestCount, 0);
  QCOMPARE(second.updateDiscardCount, 0);
  QCOMPARE(second.updateExecuted, 0);

  tracker.registerChannelDisconnected();
  tracker.registerChannelDisconnected();
  tracker.registerChannelDestroyed();
  tracker.registerChannelDestroyed();
  tracker.registerDisplayObjectStopped();
  tracker.registerDisplayObjectStopped();
  QCOMPARE(tracker.channelCounts(), std::make_pair(0, 0));
  QCOMPARE(tracker.snapshotAndReset().objectCount, 0);
}

void TestDiagnosticsTracking::auditLogRecordsWritesBlocksAndEscapedValues()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  qputenv("QTEDM_AUDIT_DIR", directory.path().toUtf8());

  AuditLogger &logger = AuditLogger::instance();
  logger.initialize(true);
  logger.logPut(QStringLiteral("audit:test:value"),
      QStringLiteral("a|b\nc"), QStringLiteral("TextEntry"),
      QStringLiteral("coverage.adl"));
  logger.logBlockedPut(QStringLiteral("audit:test:readonly"),
      QStringLiteral("7"), QStringLiteral("read-only"));
  logger.shutdown();

  const QStringList logs = QDir(directory.path()).entryList(
      {QStringLiteral("audit_*.log")}, QDir::Files);
  QCOMPARE(logs.size(), 1);
  QFile file(QDir(directory.path()).filePath(logs.front()));
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString contents = QString::fromUtf8(file.readAll());
  QVERIFY(contents.contains(QStringLiteral("# QtEDM Audit Log")));
  QVERIFY(contents.contains(QStringLiteral(
      "|TextEntry|audit:test:value|a\\|b\\nc|coverage.adl")));
  QVERIFY(contents.contains(QStringLiteral(
      "|BLOCKED:read-only|audit:test:readonly|7|-")));
  qunsetenv("QTEDM_AUDIT_DIR");
}

void TestDiagnosticsTracking::memoryTrackerParsesConfigurationAndWritesCsv()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("memory.csv"));
  qputenv("TRACK_MEM", QByteArray("1:") + path.toUtf8());
  MemoryTracker &tracker = MemoryTracker::instance();
  QVERIFY(tracker.isEnabled());
  QCOMPARE(tracker.intervalSeconds_, 1);
  QCOMPARE(tracker.logFilePath_, path);
  tracker.start();
  tracker.logNow();
  tracker.stop();
  qunsetenv("TRACK_MEM");

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QStringList lines = QString::fromUtf8(file.readAll()).split(
      QLatin1Char('\n'), Qt::SkipEmptyParts);
  QVERIFY(lines.contains(QStringLiteral("# QtEDM Memory Tracking")));
  QVERIFY(lines.contains(QStringLiteral(
      "# elapsed_sec,vm_size_kb,vm_rss_kb,shared_kb,data_kb,rss_delta_kb")));
  int dataLines = 0;
  for (const QString &line : lines) {
    if (!line.startsWith(QLatin1Char('#'))) {
      QCOMPARE(line.count(QLatin1Char(',')), 5);
      ++dataLines;
    }
  }
  QCOMPARE(dataLines, 2);
}

QTEST_MAIN(TestDiagnosticsTracking)

#include "test_diagnostics_tracking.moc"
