#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <limits>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>

#include <db_access.h>

#include "archive_provider.h"
#include "extension_object_registry.h"
#include "pv_snapshot.h"
#include "strip_chart_element.h"

class StalledNetworkReply : public QNetworkReply
{
public:
  explicit StalledNetworkReply(const QNetworkRequest &request,
      QObject *parent = nullptr)
    : QNetworkReply(parent)
  {
    setRequest(request);
    setUrl(request.url());
    open(QIODevice::ReadOnly);
  }

  void abort() override
  {
    if (isFinished()) {
      return;
    }
    setError(QNetworkReply::OperationCanceledError,
        QStringLiteral("aborted"));
    setFinished(true);
    emit errorOccurred(error());
    emit finished();
  }

protected:
  qint64 readData(char *, qint64) override { return -1; }
};

class StalledNetworkAccessManager : public QNetworkAccessManager
{
protected:
  QNetworkReply *createRequest(Operation,
      const QNetworkRequest &request, QIODevice *) override
  {
    return new StalledNetworkReply(request, this);
  }
};

class TestArchivesSnapshots : public QObject
{
  Q_OBJECT

private slots:
  void registryContainsArchivePlot();
  void archiverResponseParsingAndDecimation();
  void archiverResponseFailuresRemainVisible();
  void archiveMergePrefersLiveTimestamp();
  void archiveRequestCanBeCancelled();
  void archiveRequestTimesOut();
  void archiveHistoryPreservesTimestampGaps();
  void snapshotRoundTripPreservesTypedValues();
  void snapshotRejectsFutureSchemaAndUnsafeValues();
  void restoreChecksPolicyTypeAccessAndLimits();
};

void TestArchivesSnapshots::registryContainsArchivePlot()
{
  const auto *descriptor = ExtensionObjectRegistry::instance().descriptor(
      QStringLiteral("qtedm_archive_plot"));
  QVERIFY(descriptor);
  QCOMPARE(descriptor->createTool, CreateTool::kQtedmArchivePlot);
  QCOMPARE(descriptor->category, QStringLiteral("Monitors"));
}

void TestArchivesSnapshots::archiverResponseParsingAndDecimation()
{
  QJsonArray data;
  for (int index = 0; index < 20; ++index) {
    data.append(QJsonObject{
        {QStringLiteral("secs"), 1700000000 + index},
        {QStringLiteral("nanos"), index * 1000},
        {QStringLiteral("val"), index * 0.5},
        {QStringLiteral("status"), 0},
        {QStringLiteral("severity"), index % 4},
    });
  }
  const QByteArray payload = QJsonDocument(QJsonArray{
      QJsonObject{
          {QStringLiteral("meta"),
              QJsonObject{{QStringLiteral("name"),
                  QStringLiteral("TEST:PV")}}},
          {QStringLiteral("data"), data},
      }}).toJson(QJsonDocument::Compact);

  const ArchiveResult result =
      ArchiverApplianceProvider::parseJson(payload, 7, 1024 * 1024);
  QVERIFY2(result.ok(), qPrintable(result.error));
  QCOMPARE(result.samples.size(), 7);
  QCOMPARE(result.samples.first().value, 0.0);
  QCOMPARE(result.samples.last().value, 9.5);
  QVERIFY(std::is_sorted(result.samples.cbegin(), result.samples.cend(),
      [](const ArchiveSample &left, const ArchiveSample &right) {
        return left.timestampMs < right.timestampMs;
      }));

  const ArchiveResult empty = ArchiverApplianceProvider::parseJson(
      QByteArrayLiteral("[{\"meta\":{},\"data\":[]}]"), 10, 1024);
  QVERIFY(empty.ok());
  QVERIFY(empty.samples.isEmpty());
}

void TestArchivesSnapshots::archiverResponseFailuresRemainVisible()
{
  const ArchiveResult malformed = ArchiverApplianceProvider::parseJson(
      QByteArrayLiteral("{not-json"), 10, 1024);
  QVERIFY(!malformed.ok());
  QVERIFY(malformed.error.contains(QStringLiteral("Malformed")));
  const ArchiveResult malformedShape =
      ArchiverApplianceProvider::parseJson(
          QByteArrayLiteral("[{\"meta\":{}}]"), 10, 1024);
  QVERIFY(!malformedShape.ok());
  QVERIFY(malformedShape.error.contains(QStringLiteral("data array")));

  const ArchiveResult oversized = ArchiverApplianceProvider::parseJson(
      QByteArray(2048, 'x'), 10, 1024);
  QVERIFY(!oversized.ok());
  QVERIFY(oversized.oversized);

  const QByteArray overflowPayload = QJsonDocument(QJsonArray{
      QJsonObject{{QStringLiteral("data"), QJsonArray{
          QJsonObject{
              {QStringLiteral("secs"),
                  QString::number(std::numeric_limits<qint64>::max())},
              {QStringLiteral("nanos"), 0},
              {QStringLiteral("val"), 1.0},
          },
      }}},
  }).toJson(QJsonDocument::Compact);
  const ArchiveResult overflow = ArchiverApplianceProvider::parseJson(
      overflowPayload, 10, 1024 * 1024);
  QVERIFY(overflow.ok());
  QVERIFY(overflow.samples.isEmpty());

  const QByteArray invalidTimestampPayload = QJsonDocument(QJsonArray{
      QJsonObject{{QStringLiteral("data"), QJsonArray{
          QJsonObject{{QStringLiteral("val"), 1.0}},
          QJsonObject{{QStringLiteral("secs"), 1700000000.5},
              {QStringLiteral("nanos"), 0},
              {QStringLiteral("val"), 2.0}},
          QJsonObject{{QStringLiteral("secs"), 1700000001},
              {QStringLiteral("nanos"), QStringLiteral("0")},
              {QStringLiteral("val"), 3.0}},
          QJsonObject{{QStringLiteral("secs"), static_cast<double>(
              std::numeric_limits<qint64>::max() / 1000LL)},
              {QStringLiteral("nanos"), 999999999},
              {QStringLiteral("val"), 3.5}},
          QJsonObject{{QStringLiteral("secs"), 1700000002},
              {QStringLiteral("nanos"), 250000000},
              {QStringLiteral("val"), 4.0}},
      }}},
  }).toJson(QJsonDocument::Compact);
  const ArchiveResult timestamps = ArchiverApplianceProvider::parseJson(
      invalidTimestampPayload, 10, 1024 * 1024);
  QVERIFY2(timestamps.ok(), qPrintable(timestamps.error));
  QCOMPARE(timestamps.samples.size(), 1);
  QCOMPARE(timestamps.samples.first().timestampMs, 1700000002250LL);
  QCOMPARE(timestamps.samples.first().value, 4.0);
}

void TestArchivesSnapshots::archiveMergePrefersLiveTimestamp()
{
  QVector<ArchiveSample> historical{
      {1000, 1.0, 0, 0}, {2000, 2.0, 0, 0},
      {3000, 3.0, 0, 0}};
  QVector<ArchiveSample> live{
      {2000, 20.0, 0, 0}, {4000, 4.0, 0, 0}};
  const QVector<ArchiveSample> merged =
      ArchiverApplianceProvider::mergeAndDecimate(
          historical, live, 10, 1500);
  QCOMPARE(merged.size(), 3);
  QCOMPARE(merged.at(0).timestampMs, 2000);
  QCOMPARE(merged.at(0).value, 20.0);
  QCOMPARE(merged.last().timestampMs, 4000);
}

void TestArchivesSnapshots::archiveRequestCanBeCancelled()
{
  ArchiverApplianceProvider provider;
  provider.setRetrievalUrl(QString());
  bool completed = false;
  ArchiveResult observed;
  ArchiveQuery query;
  query.channel = QStringLiteral("TEST:PV");
  query.from = QDateTime::currentDateTimeUtc().addSecs(-60);
  query.to = QDateTime::currentDateTimeUtc();
  ArchiveRequest *request = provider.query(query, &provider,
      [&completed, &observed](const ArchiveResult &result) {
        completed = true;
        observed = result;
      });
  QVERIFY(request);
  request->cancel();
  QTRY_VERIFY(completed);
  QVERIFY(observed.cancelled);
}

void TestArchivesSnapshots::archiveRequestTimesOut()
{
  StalledNetworkAccessManager network;
  ArchiverApplianceProvider provider(nullptr, &network);
  provider.setRetrievalUrl(
      QStringLiteral("https://archive.example/retrieval"));
  bool completed = false;
  ArchiveResult observed;
  ArchiveQuery query;
  query.channel = QStringLiteral("TEST:PV");
  query.from = QDateTime::currentDateTimeUtc().addSecs(-60);
  query.to = QDateTime::currentDateTimeUtc();
  query.timeoutMs = 100;
  QVERIFY(provider.query(query, &provider,
      [&completed, &observed](const ArchiveResult &result) {
        completed = true;
        observed = result;
      }));
  QTRY_VERIFY_WITH_TIMEOUT(completed, 1000);
  QVERIFY(observed.timedOut);
  QVERIFY(observed.error.contains(QStringLiteral("timed out")));
}

void TestArchivesSnapshots::archiveHistoryPreservesTimestampGaps()
{
  StripChartElement chart;
  chart.resize(640, 320);
  chart.setExecuteMode(true);
  chart.setChannel(0, QStringLiteral("TEST:PV"));
  chart.setRuntimeConnected(0, true);
  chart.setRuntimeReadAccessKnown(0, true);
  chart.setRuntimeReadAccess(0, true);
  const qint64 start = 100000;
  const qint64 end = 110000;
  chart.replaceRuntimeHistory(0, {1.0, 2.0, 3.0},
      {start + 100, start + 5000, end - 100}, start, end);

  QVERIFY(chart.sampleCount() > 3);
  QVector<int> finiteIndices;
  for (int index = 0; index < chart.sampleCount(); ++index) {
    if (std::isfinite(chart.sampleValue(0, index))) {
      finiteIndices.append(index);
    }
  }
  QCOMPARE(finiteIndices.size(), 3);
  QVERIFY(finiteIndices.at(0) < finiteIndices.at(1));
  QVERIFY(finiteIndices.at(1) < finiteIndices.at(2));
  QVERIFY(finiteIndices.at(1) - finiteIndices.at(0) > 1);
  QVERIFY(finiteIndices.at(2) - finiteIndices.at(1) > 1);
  QVERIFY(chart.sampleIntervalSeconds() > 0.0);
}

void TestArchivesSnapshots::snapshotRoundTripPreservesTypedValues()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  PvSnapshotDocument document;
  document.createdAt = QDateTime::currentDateTimeUtc();
  document.displayPath = QStringLiteral("/tmp/operator.adl");

  PvSnapshotEntry numeric;
  numeric.provider = QStringLiteral("ca");
  numeric.pvName = QStringLiteral("TEST:DOUBLE");
  numeric.fieldType = DBF_DOUBLE;
  numeric.exactType = QStringLiteral("DBF_DOUBLE");
  numeric.kind = PvSnapshotValueKind::kNumeric;
  numeric.value = 4.25;
  numeric.connected = true;
  numeric.writeAccess = true;
  numeric.hasLimits = true;
  numeric.lowerLimit = -10.0;
  numeric.upperLimit = 10.0;
  document.entries.append(numeric);

  PvSnapshotEntry chars;
  chars.provider = QStringLiteral("ca");
  chars.pvName = QStringLiteral("TEST:CHAR");
  chars.fieldType = DBF_CHAR;
  chars.exactType = QStringLiteral("DBF_CHAR[4]");
  chars.kind = PvSnapshotValueKind::kCharArray;
  chars.value = QString::fromLatin1(QByteArray("A\0B", 3).toBase64());
  chars.connected = true;
  chars.writeAccess = true;
  document.entries.append(chars);

  PvSnapshotEntry array;
  array.provider = QStringLiteral("pva");
  array.pvName = QStringLiteral("pva://TEST:ARRAY");
  array.fieldType = DBF_DOUBLE;
  array.exactType = QStringLiteral("PVA_double[3]");
  array.kind = PvSnapshotValueKind::kNumericArray;
  array.value = QJsonArray{1.0, 2.0, 3.0};
  array.connected = true;
  array.writeAccess = true;
  document.entries.append(array);

  const QString path = directory.filePath(
      QStringLiteral("typed.qtedm-snapshot.json"));
  QString error;
  QVERIFY2(PvSnapshot::save(path, document, &error), qPrintable(error));
  const PvSnapshotLoadResult loaded = PvSnapshot::load(path);
  QVERIFY2(loaded.ok(), qPrintable(loaded.error));
  QCOMPARE(loaded.document.entries.size(), 3);
  QCOMPARE(loaded.document.entries.at(0).value.toDouble(), 4.25);
  QCOMPARE(QByteArray::fromBase64(
      loaded.document.entries.at(1).value.toString().toLatin1()),
      QByteArray("A\0B", 3));
  QCOMPARE(loaded.document.entries.at(2).value.toArray().size(), 3);
}

void TestArchivesSnapshots::snapshotRejectsFutureSchemaAndUnsafeValues()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(
      QStringLiteral("future.qtedm-snapshot.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.write(QJsonDocument(QJsonObject{
      {QStringLiteral("schemaVersion"), 99},
      {QStringLiteral("entries"), QJsonArray()},
  }).toJson()) > 0);
  file.close();
  QVERIFY(!PvSnapshot::load(path).ok());

  const QString missingEntriesPath = directory.filePath(
      QStringLiteral("missing-entries.qtedm-snapshot.json"));
  QFile missingEntriesFile(missingEntriesPath);
  QVERIFY(missingEntriesFile.open(QIODevice::WriteOnly));
  QVERIFY(missingEntriesFile.write(QJsonDocument(QJsonObject{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("createdAt"),
          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
  }).toJson()) > 0);
  missingEntriesFile.close();
  QVERIFY(!PvSnapshot::load(missingEntriesPath).ok());

  const QString invalidTimePath = directory.filePath(
      QStringLiteral("invalid-time.qtedm-snapshot.json"));
  QFile invalidTimeFile(invalidTimePath);
  QVERIFY(invalidTimeFile.open(QIODevice::WriteOnly));
  QVERIFY(invalidTimeFile.write(QJsonDocument(QJsonObject{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("createdAt"), QStringLiteral("not-a-time")},
      {QStringLiteral("entries"), QJsonArray()},
  }).toJson()) > 0);
  invalidTimeFile.close();
  QVERIFY(!PvSnapshot::load(invalidTimePath).ok());

  const QString badLimitsPath = directory.filePath(
      QStringLiteral("bad-limits.qtedm-snapshot.json"));
  QFile badLimitsFile(badLimitsPath);
  QVERIFY(badLimitsFile.open(QIODevice::WriteOnly));
  QVERIFY(badLimitsFile.write(QJsonDocument(QJsonObject{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("createdAt"),
          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
      {QStringLiteral("displayPath"), QString()},
      {QStringLiteral("entries"), QJsonArray{QJsonObject{
          {QStringLiteral("provider"), QStringLiteral("ca")},
          {QStringLiteral("pv"), QStringLiteral("TEST:PV")},
          {QStringLiteral("fieldType"), DBF_DOUBLE},
          {QStringLiteral("exactType"), QStringLiteral("DBF_DOUBLE")},
          {QStringLiteral("kind"), QStringLiteral("numeric")},
          {QStringLiteral("value"), 1.0},
          {QStringLiteral("timestamp"), QString()},
          {QStringLiteral("units"), QString()},
          {QStringLiteral("connected"), true},
          {QStringLiteral("writeAccess"), true},
          {QStringLiteral("limits"), QJsonObject{
              {QStringLiteral("low"), QStringLiteral("zero")},
              {QStringLiteral("high"), 10.0},
          }},
      }}},
  }).toJson()) > 0);
  badLimitsFile.close();
  QVERIFY(!PvSnapshot::load(badLimitsPath).ok());

  PvSnapshotDocument invalid;
  invalid.createdAt = QDateTime::currentDateTimeUtc();
  PvSnapshotEntry entry;
  entry.provider = QStringLiteral("ca");
  entry.pvName = QStringLiteral("TEST:NAN");
  entry.exactType = QStringLiteral("DBF_DOUBLE");
  entry.kind = PvSnapshotValueKind::kNumeric;
  entry.value = QStringLiteral("not-a-number");
  entry.connected = true;
  invalid.entries.append(entry);
  QString error;
  QVERIFY(!PvSnapshot::save(
      directory.filePath(QStringLiteral("invalid.json")), invalid, &error));
  QVERIFY(!error.isEmpty());

  PvSnapshotDocument missingCreationTime;
  QVERIFY(!PvSnapshot::save(
      directory.filePath(QStringLiteral("missing-time.json")),
      missingCreationTime, &error));
  QVERIFY(error.contains(QStringLiteral("creation time")));

  PvSnapshotDocument invalidChars;
  invalidChars.createdAt = QDateTime::currentDateTimeUtc();
  PvSnapshotEntry chars;
  chars.provider = QStringLiteral("ca");
  chars.pvName = QStringLiteral("TEST:CHARS");
  chars.fieldType = DBF_CHAR;
  chars.exactType = QStringLiteral("DBF_CHAR[4]");
  chars.kind = PvSnapshotValueKind::kCharArray;
  chars.value = QStringLiteral("not-base64");
  chars.connected = true;
  invalidChars.entries.append(chars);
  QVERIFY(!PvSnapshot::save(
      directory.filePath(QStringLiteral("bad-chars.json")),
      invalidChars, &error));

  PvSnapshotDocument fractionalInteger;
  fractionalInteger.createdAt = QDateTime::currentDateTimeUtc();
  PvSnapshotEntry integer;
  integer.provider = QStringLiteral("ca");
  integer.pvName = QStringLiteral("TEST:LONG");
  integer.fieldType = DBF_LONG;
  integer.exactType = QStringLiteral("DBF_LONG");
  integer.kind = PvSnapshotValueKind::kNumeric;
  integer.value = 1.5;
  integer.connected = true;
  fractionalInteger.entries.append(integer);
  QVERIFY(!PvSnapshot::save(
      directory.filePath(QStringLiteral("fractional-long.json")),
      fractionalInteger, &error));
}

void TestArchivesSnapshots::restoreChecksPolicyTypeAccessAndLimits()
{
  PvSnapshotEntry saved;
  saved.provider = QStringLiteral("ca");
  saved.pvName = QStringLiteral("TEST:PV");
  saved.fieldType = DBF_DOUBLE;
  saved.exactType = QStringLiteral("DBF_DOUBLE");
  saved.kind = PvSnapshotValueKind::kNumeric;
  saved.value = 5.0;
  saved.connected = true;

  PvSnapshotEntry current = saved;
  current.connected = true;
  current.writeAccess = true;
  current.hasLimits = true;
  current.lowerLimit = 0.0;
  current.upperLimit = 10.0;
  QVERIFY(PvSnapshot::canRestore(saved, current, false).allowed);
  QVERIFY(!PvSnapshot::canRestore(saved, current, true).allowed);

  current.writeAccess = false;
  QVERIFY(!PvSnapshot::canRestore(saved, current, false).allowed);
  current.writeAccess = true;
  current.exactType = QStringLiteral("DBF_FLOAT");
  QVERIFY(!PvSnapshot::canRestore(saved, current, false).allowed);
  current.exactType = saved.exactType;
  current.upperLimit = 4.0;
  QVERIFY(!PvSnapshot::canRestore(saved, current, false).allowed);

  saved.kind = PvSnapshotValueKind::kEnum;
  saved.fieldType = DBF_ENUM;
  saved.exactType = QStringLiteral("DBF_ENUM");
  saved.value = 1;
  saved.enumStrings =
      QStringList{QStringLiteral("Off"), QStringLiteral("On")};
  current = saved;
  current.connected = true;
  current.writeAccess = true;
  current.enumStrings =
      QStringList{QStringLiteral("Closed"), QStringLiteral("Open")};
  const PvSnapshotRestoreCheck enumCheck =
      PvSnapshot::canRestore(saved, current, false);
  QVERIFY(!enumCheck.allowed);
  QVERIFY(enumCheck.reason.contains(QStringLiteral("choices")));

  saved.kind = PvSnapshotValueKind::kStringArray;
  saved.value = QJsonArray{QStringLiteral("a"), QStringLiteral("b")};
  current = saved;
  current.connected = true;
  current.writeAccess = true;
  const PvSnapshotRestoreCheck stringArrayCheck =
      PvSnapshot::canRestore(saved, current, false);
  QVERIFY(!stringArrayCheck.allowed);
  QVERIFY(stringArrayCheck.reason.contains(QStringLiteral("not supported")));

  PvSnapshotEntry unavailable;
  unavailable.value = QJsonValue(QJsonValue::Null);
  QCOMPARE(unavailable.displayValue(), QStringLiteral("Unavailable"));
}

QTEST_MAIN(TestArchivesSnapshots)

#include "test_archives_snapshots.moc"
