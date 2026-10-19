#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "audit_logger.h"
#include "statistics_tracker.h"

#define private public
#include "memory_tracker.h"
#undef private

struct AuditWriteState
{
  QByteArray bytes;
  int attempts = 0;
  bool closed = false;
};

class AuditWriteDevice : public QIODevice
{
public:
  explicit AuditWriteDevice(AuditWriteState *state, bool fail,
      bool shortWrite = false)
    : state_(state), fail_(fail), shortWrite_(shortWrite)
  {
    open(QIODevice::WriteOnly | QIODevice::Unbuffered);
  }

  void close() override
  {
    state_->closed = true;
    QIODevice::close();
  }

protected:
  qint64 readData(char *, qint64) override { return -1; }

  qint64 writeData(const char *data, qint64 size) override
  {
    ++state_->attempts;
    if (fail_) {
      setErrorString(QStringLiteral("simulated audit write failure"));
      return -1;
    }
    const qint64 written = shortWrite_ ? qMax<qint64>(0, size - 1) : size;
    state_->bytes.append(data, static_cast<int>(written));
    if (shortWrite_) {
      setErrorString(QStringLiteral("simulated short audit write"));
    }
    return written;
  }

private:
  AuditWriteState *state_;
  bool fail_;
  bool shortWrite_;
};

class TestDiagnosticsTracking : public QObject
{
  Q_OBJECT

private slots:
  void statisticsSnapshotsResetIntervalCounters();
  void auditLogRecordsWritesBlocksAndEscapedValues();
  void auditLogWriteFailuresDisableSession_data();
  void auditLogWriteFailuresDisableSession();
  void auditLogWritesThroughDeviceAndKeepsSessionEnabled();
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

void TestDiagnosticsTracking::auditLogWriteFailuresDisableSession_data()
{
  QTest::addColumn<QString>("section");
  QTest::addColumn<bool>("shortWrite");
  QTest::newRow("header-error") << QStringLiteral("header") << false;
  QTest::newRow("header-short-write") << QStringLiteral("header") << true;
  QTest::newRow("record-error") << QStringLiteral("record") << false;
  QTest::newRow("record-short-write") << QStringLiteral("record") << true;
}

void TestDiagnosticsTracking::auditLogWriteFailuresDisableSession()
{
  QFETCH(QString, section);
  QFETCH(bool, shortWrite);
  AuditWriteState state;
  AuditLogger logger;
  logger.initialized_ = true;
  logger.logFile_ = std::make_unique<AuditWriteDevice>(
      &state, !shortWrite, shortWrite);

  QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
      QStringLiteral("AuditLogger: Failed to write %1 to .*"
                     "Audit logging disabled for this session\\.")
          .arg(section)));
  if (section == QStringLiteral("header")) {
    QVERIFY(!logger.writeLogText(QStringLiteral("# QtEDM Audit Log\n"),
        "header"));
  } else {
    logger.logPut(QStringLiteral("audit:test:value"),
        QStringLiteral("7"), QStringLiteral("TextEntry"));
  }
  QVERIFY(state.attempts > 0);
  QVERIFY(state.closed);
  QVERIFY(!logger.logFile_);
  QVERIFY(!logger.isEnabled());

  const int attemptsAfterFailure = state.attempts;
  logger.logPut(QStringLiteral("audit:test:later"),
      QStringLiteral("8"), QStringLiteral("TextEntry"));
  QCOMPARE(state.attempts, attemptsAfterFailure);
  QVERIFY(!logger.logFile_);
}

void TestDiagnosticsTracking::auditLogWritesThroughDeviceAndKeepsSessionEnabled()
{
  AuditWriteState state;
  AuditLogger logger;
  logger.initialized_ = true;
  logger.logFile_ = std::make_unique<AuditWriteDevice>(&state, false);
  QVERIFY(logger.writeLogText(QStringLiteral("# QtEDM Audit Log\n"),
      "header"));
  logger.logPut(QStringLiteral("audit:test:value"),
      QStringLiteral("a|b\nc"), QStringLiteral("TextEntry"),
      QStringLiteral("coverage.adl"));
  QVERIFY(state.bytes.startsWith("# QtEDM Audit Log\n"));
  QVERIFY(state.bytes.contains(
      "|TextEntry|audit:test:value|a\\|b\\nc|coverage.adl\n"));
  QVERIFY(logger.isEnabled());
  QVERIFY(logger.logFile_);
  QVERIFY(!state.closed);
  logger.shutdown();
  QVERIFY(state.closed);
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
