#include <QtTest/QtTest>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <memory>
#include <vector>

#include "display_converter.h"
#include "extension_object_registry.h"
#include "ntndarray_image_decoder.h"
#include "ntndarray_image_element.h"
#include "ntndarray_image_runtime.h"
#include "adl_parser.h"

namespace {

template <typename T>
NtNdArrayFrame frameFrom(const std::vector<T> &values,
    NtNdArrayScalarType type, const QVector<int> &dimensions,
    NtNdArrayColorMode colorMode)
{
  auto owner = std::make_shared<std::vector<T>>(values);
  NtNdArrayFrame frame;
  frame.data = std::shared_ptr<const void>(owner, owner->data());
  frame.elementCount = owner->size();
  frame.byteCount = owner->size() * sizeof(T);
  frame.scalarType = type;
  frame.colorMode = colorMode;
  frame.uniqueId = 42;
  frame.secondsPastEpoch = 1700000000;
  for (int size : dimensions) {
    NtNdArrayDimension dimension;
    dimension.size = size;
    dimension.fullSize = size;
    frame.dimensions.append(dimension);
  }
  return frame;
}

QString fixturePath()
{
  return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
      QStringLiteral("../../tests/fixtures/display_import/caqtdm_mixed.ui"));
}

QByteArray readAll(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QByteArray();
  }
  return file.readAll();
}

} // namespace

class TestDisplayImportNdArray : public QObject
{
  Q_OBJECT

private slots:
  void registryContainsNtNdArrayImage();
  void converterProducesDeterministicDisplaysAndReport();
  void converterRejectsMalformedAndUnsupportedInputs();
  void decoderSupportsEveryScalarType();
  void decoderSupportsMonoAndRgbLayouts();
  void decoderRejectsUnsafeOrMalformedFrames();
  void decoderMapsTransformedPixelCoordinates();
  void imageElementTracksDisconnectsAndDimensionChanges();
  void asynchronousDecodePublishesAFrame();
  void newestFrameQueueDropsObsoletePendingFrames();
};

void TestDisplayImportNdArray::registryContainsNtNdArrayImage()
{
  const auto *descriptor = ExtensionObjectRegistry::instance().descriptor(
      QStringLiteral("qtedm_ndarray_image"));
  QVERIFY(descriptor);
  QCOMPARE(descriptor->createTool, CreateTool::kQtedmNdArrayImage);
  QCOMPARE(descriptor->category, QStringLiteral("Monitors"));
}

void TestDisplayImportNdArray::converterProducesDeterministicDisplaysAndReport()
{
  QVERIFY2(QFileInfo::exists(fixturePath()), qPrintable(fixturePath()));
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  DisplayConversionOptions options;
  options.inputPath = fixturePath();
  options.outputPath = directory.filePath(QStringLiteral("converted.adl"));
  options.reportPath =
      directory.filePath(QStringLiteral("converted.report.json"));
  options.sourceCopyPath =
      directory.filePath(QStringLiteral("converted.source.ui"));

  const DisplayConversionResult first = DisplayConverter::convert(options);
  QVERIFY2(first.success, qPrintable(first.error));
  QCOMPARE(first.exitCode(), 2);
  QVERIFY(first.hasWarnings);
  const QByteArray firstAdl = readAll(first.outputPath);
  const QByteArray firstReport = readAll(first.reportPath);
  QVERIFY(firstAdl.contains("qtedm_tabbed_display"));
  QVERIFY(firstAdl.contains("qtedm_ndarray_image"));
  QVERIFY(firstAdl.contains("Unsupported import: QPushButton"));
  QVERIFY(firstAdl.contains("IMPORT:READBACK"));
  QCOMPARE(readAll(first.sourceCopyPath), readAll(fixturePath()));

  const QJsonObject report =
      QJsonDocument::fromJson(firstReport).object();
  QCOMPARE(report.value(QStringLiteral("schema_version")).toInt(), 1);
  QCOMPARE(report.value(QStringLiteral("source_format")).toString(),
      QStringLiteral("caqtdm-qt-designer-ui"));
  QVERIFY(report.value(QStringLiteral("source_object_count")).toInt() >= 13);
  QVERIFY(report.value(QStringLiteral("target_object_count")).toInt() >= 12);
  const QJsonObject counts =
      report.value(QStringLiteral("counts")).toObject();
  QVERIFY(counts.value(QStringLiteral("mapped")).toInt() > 0);
  QVERIFY(counts.value(QStringLiteral("approximated")).toInt() > 0);
  QVERIFY(counts.value(QStringLiteral("unsupported")).toInt() > 0);
  const QJsonArray generated =
      report.value(QStringLiteral("generated_displays")).toArray();
  QCOMPARE(generated.size(), 3);
  QVERIFY(QFileInfo::exists(
      directory.filePath(QStringLiteral("converted_tab_overview.adl"))));
  QVERIFY(QFileInfo::exists(
      directory.filePath(QStringLiteral("converted_tab_detail.adl"))));
  const QByteArray overviewAdl = readAll(
      directory.filePath(QStringLiteral("converted_tab_overview.adl")));
  QVERIFY(overviewAdl.contains("width=460"));
  QVERIFY(overviewAdl.contains("height=190"));
  for (const QJsonValue &pathValue : generated) {
    QString parseError;
    const auto parsed = AdlParser::parse(
        QString::fromUtf8(readAll(pathValue.toString())), &parseError);
    QVERIFY2(parsed.has_value(), qPrintable(parseError));
  }

  const DisplayConversionResult second = DisplayConverter::convert(options);
  QVERIFY2(second.success, qPrintable(second.error));
  QCOMPARE(readAll(second.outputPath), firstAdl);
  QCOMPARE(readAll(second.reportPath), firstReport);
}

void TestDisplayImportNdArray::converterRejectsMalformedAndUnsupportedInputs()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  const QString completePath =
      directory.filePath(QStringLiteral("complete.ui"));
  QFile complete(completePath);
  QVERIFY(complete.open(QIODevice::WriteOnly));
  const QByteArray completeUi =
      "<ui version=\"4.0\"><class>Complete</class>"
      "<widget class=\"QWidget\" name=\"Complete\">"
      "<property name=\"geometry\"><rect><x>0</x><y>0</y>"
      "<width>200</width><height>100</height></rect></property>"
      "<widget class=\"QLabel\" name=\"label\">"
      "<property name=\"geometry\"><rect><x>10</x><y>10</y>"
      "<width>180</width><height>30</height></rect></property>"
      "<property name=\"text\"><string>Complete mapping</string></property>"
      "</widget></widget></ui>";
  QCOMPARE(complete.write(completeUi), qint64(completeUi.size()));
  complete.close();
  DisplayConversionOptions completeOptions;
  completeOptions.inputPath = completePath;
  completeOptions.outputPath =
      directory.filePath(QStringLiteral("complete.adl"));
  const DisplayConversionResult completeResult =
      DisplayConverter::convert(completeOptions);
  QVERIFY2(completeResult.success, qPrintable(completeResult.error));
  QVERIFY(!completeResult.hasWarnings);
  QCOMPARE(completeResult.exitCode(), 0);

  const QString malformedPath =
      directory.filePath(QStringLiteral("broken.ui"));
  QFile malformed(malformedPath);
  QVERIFY(malformed.open(QIODevice::WriteOnly));
  QCOMPARE(malformed.write("<ui><widget>"), qint64(12));
  malformed.close();
  DisplayConversionOptions malformedOptions;
  malformedOptions.inputPath = malformedPath;
  const DisplayConversionResult malformedResult =
      DisplayConverter::convert(malformedOptions);
  QVERIFY(!malformedResult.success);
  QCOMPARE(malformedResult.exitCode(), 1);
  QVERIFY(malformedResult.error.contains(QStringLiteral("Malformed")));

  const QString wrongPath = directory.filePath(QStringLiteral("display.bob"));
  QFile wrong(wrongPath);
  QVERIFY(wrong.open(QIODevice::WriteOnly));
  wrong.write("not a ui");
  wrong.close();
  DisplayConversionOptions wrongOptions;
  wrongOptions.inputPath = wrongPath;
  const DisplayConversionResult wrongResult =
      DisplayConverter::convert(wrongOptions);
  QVERIFY(!wrongResult.success);
  QVERIFY(wrongResult.error.contains(QStringLiteral(".ui")));

  DisplayConversionOptions overwriteOptions;
  overwriteOptions.inputPath = fixturePath();
  overwriteOptions.outputPath = fixturePath();
  const DisplayConversionResult overwriteResult =
      DisplayConverter::convert(overwriteOptions);
  QVERIFY(!overwriteResult.success);
  QVERIFY(overwriteResult.error.contains(QStringLiteral("overwrite")));
}

void TestDisplayImportNdArray::decoderSupportsEveryScalarType()
{
  NtNdArrayDecodeOptions options;
  options.maximumInputBytes = 1024;
  options.maximumOutputBytes = 1024;
  auto verify = [&options](const NtNdArrayFrame &frame) {
    const NtNdArrayDecodedFrame decoded =
        NtNdArrayImageDecoder::decode(frame, options);
    QVERIFY2(decoded.valid, qPrintable(decoded.error));
    QCOMPARE(decoded.image.size(), QSize(2, 1));
  };
  verify(frameFrom<qint8>({-1, 2}, NtNdArrayScalarType::kInt8,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<quint8>({1, 2}, NtNdArrayScalarType::kUInt8,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<qint16>({-1, 2}, NtNdArrayScalarType::kInt16,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<quint16>({1, 2}, NtNdArrayScalarType::kUInt16,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<qint32>({-1, 2}, NtNdArrayScalarType::kInt32,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<quint32>({1, 2}, NtNdArrayScalarType::kUInt32,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<qint64>({-1, 2}, NtNdArrayScalarType::kInt64,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<quint64>({1, 2}, NtNdArrayScalarType::kUInt64,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<float>({-1.5F, 2.5F}, NtNdArrayScalarType::kFloat32,
      {2, 1}, NtNdArrayColorMode::kMono));
  verify(frameFrom<double>({-1.5, 2.5}, NtNdArrayScalarType::kFloat64,
      {2, 1}, NtNdArrayColorMode::kMono));
}

void TestDisplayImportNdArray::decoderSupportsMonoAndRgbLayouts()
{
  NtNdArrayDecodeOptions options;
  const NtNdArrayFrame mono = frameFrom<quint8>(
      {0, 64, 128, 255}, NtNdArrayScalarType::kUInt8,
      {2, 2}, NtNdArrayColorMode::kMono);
  const NtNdArrayDecodedFrame monoDecoded =
      NtNdArrayImageDecoder::decode(mono, options);
  QVERIFY2(monoDecoded.valid, qPrintable(monoDecoded.error));
  QCOMPARE(monoDecoded.image.size(), QSize(2, 2));
  QCOMPARE(NtNdArrayImageDecoder::pixelValues(
      monoDecoded, 1, 1), QVector<double>({255.0}));

  const QVector<QPair<NtNdArrayFrame, QVector<int>>> rgbFrames{
      {frameFrom<quint8>({255, 0, 0, 0, 255, 0},
           NtNdArrayScalarType::kUInt8, {3, 2, 1},
           NtNdArrayColorMode::kRgb1), {3, 2, 1}},
      {frameFrom<quint8>({255, 0, 0, 255, 0, 0},
           NtNdArrayScalarType::kUInt8, {2, 3, 1},
           NtNdArrayColorMode::kRgb2), {2, 3, 1}},
      {frameFrom<quint8>({255, 0, 0, 255, 0, 0},
           NtNdArrayScalarType::kUInt8, {2, 1, 3},
           NtNdArrayColorMode::kRgb3), {2, 1, 3}},
  };
  for (const auto &entry : rgbFrames) {
    const NtNdArrayDecodedFrame decoded =
        NtNdArrayImageDecoder::decode(entry.first, options);
    QVERIFY2(decoded.valid, qPrintable(decoded.error));
    QCOMPARE(decoded.image.size(), QSize(2, 1));
    QCOMPARE(NtNdArrayImageDecoder::pixelValues(
        decoded, 0, 0), QVector<double>({255.0, 0.0, 0.0}));
    QCOMPARE(NtNdArrayImageDecoder::pixelValues(
        decoded, 1, 0), QVector<double>({0.0, 255.0, 0.0}));
  }
}

void TestDisplayImportNdArray::decoderRejectsUnsafeOrMalformedFrames()
{
  NtNdArrayDecodeOptions options;
  options.maximumInputBytes = 3;
  NtNdArrayFrame frame = frameFrom<quint8>(
      {0, 1, 2, 3}, NtNdArrayScalarType::kUInt8,
      {2, 2}, NtNdArrayColorMode::kMono);
  QVERIFY(!NtNdArrayImageDecoder::decode(frame, options).valid);
  options.maximumInputBytes = 1024;
  options.maximumOutputBytes = 8;
  QVERIFY(!NtNdArrayImageDecoder::decode(frame, options).valid);
  options.maximumOutputBytes = 1024;
  frame.codec = QStringLiteral("lz4");
  QVERIFY(NtNdArrayImageDecoder::decode(frame, options).error.contains(
      QStringLiteral("Compressed")));
  frame.codec.clear();
  frame.dimensions[0].size = 3;
  QVERIFY(!NtNdArrayImageDecoder::decode(frame, options).valid);
  frame.dimensions = {
      NtNdArrayDimension{2, 0, 2, 1, false},
      NtNdArrayDimension{2, 0, 2, 1, false}};
  frame.byteCount = 3;
  QVERIFY(!NtNdArrayImageDecoder::decode(frame, options).valid);
}

void TestDisplayImportNdArray::decoderMapsTransformedPixelCoordinates()
{
  const NtNdArrayFrame frame = frameFrom<quint16>(
      {10, 20, 30, 40, 50, 60}, NtNdArrayScalarType::kUInt16,
      {3, 2}, NtNdArrayColorMode::kMono);
  NtNdArrayDecodeOptions options;
  options.rotation = HeatmapRotation::k90;
  options.flipHorizontal = true;
  const NtNdArrayDecodedFrame decoded =
      NtNdArrayImageDecoder::decode(frame, options);
  QVERIFY2(decoded.valid, qPrintable(decoded.error));
  QCOMPARE(decoded.image.size(), QSize(2, 3));
  QCOMPARE(NtNdArrayImageDecoder::pixelValues(
      decoded, 0, 0), QVector<double>({60.0}));
  QCOMPARE(qGray(decoded.image.pixel(0, 0)), 255);
  QCOMPARE(NtNdArrayImageDecoder::pixelValues(
      decoded, 1, 0), QVector<double>({30.0}));
  QCOMPARE(qGray(decoded.image.pixel(1, 0)), 102);
  QCOMPARE(NtNdArrayImageDecoder::pixelValues(
      decoded, 0, 2), QVector<double>({40.0}));
  QCOMPARE(qGray(decoded.image.pixel(0, 2)), 153);
  QCOMPARE(NtNdArrayImageDecoder::pixelValues(
      decoded, 1, 2), QVector<double>({10.0}));
  QCOMPARE(qGray(decoded.image.pixel(1, 2)), 0);
  QVERIFY(NtNdArrayImageDecoder::pixelValues(decoded, -1, 0).isEmpty());
  QVERIFY(NtNdArrayImageDecoder::pixelValues(decoded, 2, 0).isEmpty());
}

void TestDisplayImportNdArray::imageElementTracksDisconnectsAndDimensionChanges()
{
  NtNdArrayImageElement element;
  NtNdArrayDecodeOptions options;
  NtNdArrayDecodedFrame first = NtNdArrayImageDecoder::decode(
      frameFrom<quint8>({1, 2, 3, 4}, NtNdArrayScalarType::kUInt8,
          {2, 2}, NtNdArrayColorMode::kMono),
      options);
  QVERIFY(first.valid);
  element.setDecodedFrame(first);
  element.setStreamStatus(true, 2, QString());
  QVERIFY(element.streamConnected());
  QCOMPARE(element.decodedFrame().image.size(), QSize(2, 2));
  QCOMPARE(element.droppedFrames(), quint64(2));

  NtNdArrayDecodedFrame second = NtNdArrayImageDecoder::decode(
      frameFrom<quint8>({5, 6, 7}, NtNdArrayScalarType::kUInt8,
          {3, 1}, NtNdArrayColorMode::kMono),
      options);
  QVERIFY(second.valid);
  element.setDecodedFrame(second);
  QCOMPARE(element.decodedFrame().image.size(), QSize(3, 1));

  element.setStreamStatus(
      false, 3, QStringLiteral("Structured PVA disconnected."));
  QVERIFY(!element.streamConnected());
  QCOMPARE(element.droppedFrames(), quint64(3));
  QCOMPARE(element.lastError(),
      QStringLiteral("Structured PVA disconnected."));
  element.clearNtNdArrayState();
  QVERIFY(!element.decodedFrame().valid);
  QCOMPARE(element.droppedFrames(), quint64(0));
  QVERIFY(element.lastError().isEmpty());
  element.setStreamStatus(
      false, 0, QStringLiteral("Structured PVA disconnected."));
  element.resize(320, 180);
  QImage rendered(element.size(), QImage::Format_ARGB32);
  rendered.fill(Qt::transparent);
  element.render(&rendered);
  bool sawDisconnectIndicator = false;
  for (int y = 0; y < rendered.height() && !sawDisconnectIndicator; ++y) {
    for (int x = 0; x < rendered.width(); ++x) {
      const QColor color = rendered.pixelColor(x, y);
      if (color.red() > 200 && color.green() < 140 && color.blue() < 140) {
        sawDisconnectIndicator = true;
        break;
      }
    }
  }
  QVERIFY(sawDisconnectIndicator);
}

void TestDisplayImportNdArray::asynchronousDecodePublishesAFrame()
{
  NtNdArrayImageElement element;
  NtNdArrayImageRuntime runtime(&element);
  runtime.started_ = true;
  NtNdArrayFrame frame = frameFrom<quint8>(
      {1, 2, 3, 4}, NtNdArrayScalarType::kUInt8,
      {2, 2}, NtNdArrayColorMode::kMono);
  frame.uniqueId = 99;
  runtime.submitFrame(frame);
  QVERIFY(runtime.decodeBusy_);
  QTRY_VERIFY_WITH_TIMEOUT(element.decodedFrame().valid, 2000);
  QCOMPARE(element.decodedFrame().source.uniqueId, 99);
  QVERIFY(!runtime.decodeBusy_);
  runtime.started_ = false;
}

void TestDisplayImportNdArray::newestFrameQueueDropsObsoletePendingFrames()
{
  NtNdArrayImageElement element;
  NtNdArrayImageRuntime runtime(&element);
  runtime.started_ = true;
  runtime.decodeBusy_ = true;
  for (int id = 1; id <= 4; ++id) {
    NtNdArrayFrame frame = frameFrom<quint8>(
        {static_cast<quint8>(id)}, NtNdArrayScalarType::kUInt8,
        {1, 1}, NtNdArrayColorMode::kMono);
    frame.uniqueId = id;
    runtime.submitFrame(frame);
  }
  QVERIFY(runtime.hasPendingFrame_);
  QCOMPARE(runtime.pendingFrame_.uniqueId, 4);
  QCOMPARE(runtime.droppedFrames_, quint64(3));
  runtime.started_ = false;
  runtime.decodeBusy_ = false;
  runtime.hasPendingFrame_ = false;
}

QTEST_MAIN(TestDisplayImportNdArray)
#include "test_display_import_ndarray.moc"
