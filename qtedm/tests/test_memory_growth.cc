#include <QtTest/QtTest>

#include <QComboBox>
#include <QDateTimeEdit>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTextEdit>

#include <thread>
#include <algorithm>
#include <vector>

#include "audit_log_viewer_dialog.h"
#include "image_memory_budget.h"
#include "ntndarray_image_decoder.h"
#include "plugin_manager.h"
#include "pvaSDDS.h"
#include "shared_channel_manager.h"

class TestMemoryGrowth : public QObject
{
  Q_OBJECT

private slots:
  void pvaLatestGetReusesArrayStorage();
  void caInboxPreservesBurstsAndBoundsOverflow();
  void caInboxBoundsBytesAndDropsClosedChannels();
  void pluginDiagnosticsAreBoundedAndDeduplicated();
  void auditViewsAreBoundedAndReleaseOnHide();
  void imageReservationsFollowSharedFrameOwnership();
};

void TestMemoryGrowth::pvaLatestGetReusesArrayStorage()
{
  using namespace epics::pvData;
  PVA_OVERALL storage;
  allocPVA(&storage, 1);
  /* This is the policy selected by pvaBridgeCreateChannel. Exercise the
   * reconnect get/extraction path without an IOC or any network access. */
  storage.limitGetReadings = true;
  const auto array = std::tr1::static_pointer_cast<PVDoubleArray>(
      getPVDataCreate()->createPVScalarArray(pvDouble));
  for (int iteration = 0; iteration < 500; ++iteration) {
    shared_vector<double> values(iteration % 7);
    std::fill(values.begin(), values.end(), iteration);
    array->replace(freeze(values));
    QCOMPARE(ExtractScalarArrayValue(&storage, 0, array, false), 0L);
    QCOMPARE(storage.pvaData[0].numGetReadings, 1L);
    QCOMPARE(storage.pvaData[0].numGetReadingsAllocated, 1L);
    QCOMPARE(storage.pvaData[0].numGetElements, long(iteration % 7));
    if (iteration % 7) {
      QCOMPARE(storage.pvaData[0].getData[0].values[0], double(iteration));
    }
  }
  freePVA(&storage);
}

void TestMemoryGrowth::caInboxPreservesBurstsAndBoundsOverflow()
{
  SharedChannelManager manager;
  SharedChannelManager::SharedChannel channel;
  channel.instanceId = 1;
  channel.cachedData.nativeElementCount = 1;
  channel.cachedData.nativeFieldType = DBF_DOUBLE;
  channel.key.requestedType = DBR_TIME_DOUBLE;
  manager.instanceIdToChannel_.insert(1, &channel);
  manager.callbackChannels_.insert(1);
  QVector<double> observed;
  SharedChannelManager::Subscriber subscriber;
  subscriber.deliveryMode = ChannelDeliveryMode::kRealtime;
  subscriber.valueCallback = [&observed](const SharedChannelData &data) {
    observed.append(data.numericValue);
  };
  channel.subscribers.insert(1, subscriber);
  auto enqueue = [&manager](int index) {
    dbr_time_double value{};
    value.value = index;
    value.severity = index % 2; /* Alarm transitions force immediate delivery. */
    event_handler_args args{};
    args.usr = reinterpret_cast<void *>(quintptr(1));
    args.dbr = &value;
    args.type = DBR_TIME_DOUBLE;
    args.count = 1;
    args.status = ECA_NORMAL;
    manager.valueCallback(args);
  };
  for (int index = 0; index < 20; ++index) {
    enqueue(index);
  }
  dbr_time_double invalidValue{};
  event_handler_args invalidArgs{};
  invalidArgs.usr = reinterpret_cast<void *>(quintptr(1));
  invalidArgs.dbr = &invalidValue;
  invalidArgs.type = DBR_TIME_DOUBLE;
  invalidArgs.count = 0;
  invalidArgs.status = ECA_NORMAL;
  manager.valueCallback(invalidArgs);
  QCOMPARE(manager.callbackEvents_.size(), size_t(20));
  invalidArgs.count = 1;
  invalidArgs.type = LAST_BUFFER_TYPE + 1;
  manager.valueCallback(invalidArgs);
  QCOMPARE(manager.callbackEvents_.size(), size_t(20));
  QTRY_COMPARE(observed.size(), 20);
  for (int index = 0; index < 20; ++index) {
    QCOMPARE(observed.at(index), double(index));
  }
  /* Multiple producers cannot create more than one pending Qt drain or
   * exceed the fixed FIFO capacity while the UI isn't processing events. */
  std::thread first([&]() {
    for (int index = 0; index < 5000; ++index) enqueue(index);
  });
  std::thread second([&]() {
    for (int index = 0; index < 5000; ++index) enqueue(index);
  });
  first.join();
  second.join();
  QCOMPARE(manager.callbackEvents_.size(), size_t(4096));
  QVERIFY(manager.callbackDrainScheduled_);
  QVERIFY(manager.droppedCallbackEvents_ > 0);
  enqueue(123456);
  QTRY_VERIFY(manager.callbackEvents_.empty());
  QCOMPARE(channel.cachedData.numericValue, 123456.0);
  QCOMPARE(manager.callbackBytes_, size_t(0));
  manager.removeCallbackChannel(1);
  manager.instanceIdToChannel_.clear();
}

void TestMemoryGrowth::caInboxBoundsBytesAndDropsClosedChannels()
{
  SharedChannelManager manager;
  manager.callbackChannels_.insert(1);
  const QByteArray payload(1024 * 1024, 'x');
  SharedChannelManager::CallbackEvent event;
  event.channelInstanceId = 1;
  for (int index = 0; index < 140; ++index) {
    manager.queueCallbackEvent(event, payload.constData(), payload.size());
  }
  QVERIFY(manager.callbackBytes_ <= size_t(128 * 1024 * 1024));
  QVERIFY(manager.droppedCallbackEvents_ > 0);
  const size_t sizeBefore = manager.callbackEvents_.size();
  /* A bogus byte count must be rejected before copying its data pointer. */
  manager.queueCallbackEvent(event, payload.constData(), 65 * 1024 * 1024);
  QCOMPARE(manager.callbackEvents_.size(), sizeBefore);
  manager.removeCallbackChannel(1);
  QVERIFY(manager.callbackEvents_.empty());
  QCOMPARE(manager.callbackBytes_, size_t(0));
  manager.queueCallbackEvent(event, payload.constData(), payload.size());
  QVERIFY(manager.callbackEvents_.empty());
}

void TestMemoryGrowth::pluginDiagnosticsAreBoundedAndDeduplicated()
{
  auto &manager = QtedmPluginManager::instance();
  manager.resetForTesting();
  for (int index = 0; index < 10000; ++index) {
    manager.reportDiagnostic(QStringLiteral("test"), QStringLiteral("failure"));
  }
  QCOMPARE(manager.diagnostics().size(), 1);
  QVERIFY(manager.diagnostics().first().contains(QStringLiteral("10000")));
  for (int index = 0; index < 1000; ++index) {
    manager.reportDiagnostic(QStringLiteral("test"),
        QString::number(index) + QString(4000, QLatin1Char('x')));
  }
  const QStringList messages = manager.diagnostics();
  QVERIFY(messages.size() <= 256);
  qsizetype characters = 0;
  for (const QString &message : messages) characters += message.size();
  QVERIFY(characters <= 256 * 1024);
  QVERIFY(messages.last().startsWith(QStringLiteral("test: 999")));
  QVERIFY(messages.last().contains(QStringLiteral("truncated")));
  manager.resetForTesting();
}

void TestMemoryGrowth::auditViewsAreBoundedAndReleaseOnHide()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray previousDirectory = qgetenv("QTEDM_AUDIT_DIR");
  qputenv("QTEDM_AUDIT_DIR", directory.path().toUtf8());
  QFile file(directory.filePath(QStringLiteral("audit_20200101_000000_1.log")));
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("# User: unit\n");
  for (int chunk = 0; chunk < 32; ++chunk) {
    file.write(QByteArray(64 * 1024, 'x'));
  }
  file.write("\n");
  const QByteArray timestamp = QDateTime::currentDateTimeUtc()
      .toString(Qt::ISODate).toUtf8();
  const QByteArray record = timestamp + "|unit|TextEntry|TEST:PV|1|"
      + QByteArray(200, 'd') + "\n";
  for (int index = 0; index < 8000; ++index) file.write(record);
  file.write(timestamp + "|unit|TextEntry|LAST:PV|2|display.adl\n");
  file.close();

  AuditLogViewerDialog dialog(QApplication::palette(), QFont());
  dialog.currentUser_ = QStringLiteral("unit");
  dialog.showAndRaise();
  QVERIFY(dialog.logContent_->toPlainText().contains(QStringLiteral("LAST:PV")));
  QVERIFY(dialog.logContent_->toPlainText().contains(QStringLiteral("bounded tail")));
  QVERIFY(dialog.logContent_->toPlainText().size() < 540000);
  dialog.pvSearchEdit_->setText(QStringLiteral("TEST:*"));
  dialog.startDateEdit_->setDateTime(QDateTime::currentDateTime().addSecs(-3600));
  dialog.endDateEdit_->setDateTime(QDateTime::currentDateTime().addSecs(3600));
  dialog.performSearch();
  QVERIFY(dialog.searchResults_->toPlainText().contains(QStringLiteral("truncated")));
  QVERIFY(dialog.searchResults_->toPlainText().size() < 530000);
  dialog.hide();
  QVERIFY(dialog.logContent_->toPlainText().isEmpty());
  QVERIFY(dialog.searchResults_->toPlainText().isEmpty());
  QCOMPARE(dialog.logFileCombo_->count(), 0);
  if (previousDirectory.isNull()) qunsetenv("QTEDM_AUDIT_DIR");
  else qputenv("QTEDM_AUDIT_DIR", previousDirectory);
}

void TestMemoryGrowth::imageReservationsFollowSharedFrameOwnership()
{
  ImageMemoryBudget budget(100);
  auto pixels = std::make_shared<std::vector<quint8>>(4, 1);
  NtNdArrayFrame frame;
  frame.data = std::shared_ptr<const void>(pixels, pixels->data());
  frame.byteCount = frame.elementCount = 4;
  frame.dimensions = {{2}, {2}};
  frame.colorMode = NtNdArrayColorMode::kMono;
  NtNdArrayDecodeOptions options;
  quint64 workingBytes = 0;
  QString error;
  QVERIFY(NtNdArrayImageDecoder::validate(frame, options, &workingBytes, &error));
  QCOMPARE(workingBytes, quint64(36));
  frame.memoryReservation = budget.reserve(workingBytes);
  QVERIFY(frame.memoryReservation);
  QVERIFY(!budget.reserve(65));
  auto decoded = NtNdArrayImageDecoder::decode(frame, options);
  QVERIFY(decoded.valid);
  auto copy = decoded;
  decoded = {};
  frame = {};
  QCOMPARE(budget.bytesInUse(), uint64_t(36));
  copy = {};
  QCOMPARE(budget.bytesInUse(), uint64_t(0));
  QVERIFY(budget.reserve(100));
}

QTEST_MAIN(TestMemoryGrowth)
#include "test_memory_growth.moc"
