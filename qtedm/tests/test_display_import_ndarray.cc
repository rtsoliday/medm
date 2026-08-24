#include <QtTest/QtTest>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <memory>
#include <algorithm>
#include <vector>

#include <pv/ntndarray.h>

#include "display_converter.h"
#include "display_state.h"
#include "display_window.h"
#include "extension_object_registry.h"
#include "ntndarray_image_decoder.h"
#include "ntndarray_image_element.h"
#include "ntndarray_image_runtime.h"
#include "pva_ntndarray_source.h"
#include "adl_parser.h"

bool pvaNtNdArrayExtractFrame(
    const epics::pvData::PVStructurePtr &root,
    NtNdArrayFrame *frame, QString *error);

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

epics::pvData::PVStructurePtr rawNtNdArray(
    const std::vector<epics::pvData::uint8> &values,
    const QVector<int> &dimensions)
{
  using namespace epics::nt;
  using namespace epics::pvData;
  PVStructurePtr root = NTNDArray::createBuilder()->createPVStructure();
  PVUnionPtr value = root->getSubField<PVUnion>("value");
  PVUByteArrayPtr array = value->select<PVUByteArray>("ubyteValue");
  PVUByteArray::svector arrayValues(array->reuse());
  arrayValues.resize(values.size());
  std::copy(values.begin(), values.end(), arrayValues.begin());
  array->replace(freeze(arrayValues));

  PVStructureArrayPtr dimensionField =
      root->getSubField<PVStructureArray>("dimension");
  PVStructureArray::svector dimensionValues(dimensionField->reuse());
  dimensionValues.resize(dimensions.size());
  for (int index = 0; index < dimensions.size(); ++index) {
    PVStructurePtr dimension = getPVDataCreate()->createPVStructure(
        dimensionField->getStructureArray()->getStructure());
    dimension->getSubField<PVInt>("size")->put(dimensions.at(index));
    dimension->getSubField<PVInt>("offset")->put(index);
    dimension->getSubField<PVInt>("fullSize")->put(
        dimensions.at(index) + index);
    dimension->getSubField<PVInt>("binning")->put(index + 1);
    dimension->getSubField<PVBoolean>("reverse")->put(index != 0);
    dimensionValues[index] = dimension;
  }
  dimensionField->replace(freeze(dimensionValues));
  root->getSubField<PVString>("codec.name")->put("");
  root->getSubField<PVLong>("compressedSize")->put(values.size());
  root->getSubField<PVLong>("uncompressedSize")->put(values.size());
  root->getSubField<PVInt>("uniqueId")->put(73);
  root->getSubField<PVLong>(
      "dataTimeStamp.secondsPastEpoch")->put(1700000123);
  root->getSubField<PVInt>("dataTimeStamp.nanoseconds")->put(456);
  return root;
}

} // namespace

class TestDisplayImportNdArray : public QObject
{
  Q_OBJECT

private slots:
  void registryContainsNtNdArrayImage();
  void converterProducesDeterministicDisplaysAndReport();
  void converterRejectsMalformedAndUnsupportedInputs();
  void converterUsesSafeDefaultsAndAvoidsArtifactCollisions();
  void tabbedChildLoadFailureDoesNotLeakWindow();
  void decoderSupportsEveryScalarType();
  void decoderSupportsMonoAndRgbLayouts();
  void decoderRejectsUnsafeOrMalformedFrames();
  void decoderMapsTransformedPixelCoordinates();
  void imageElementTracksDisconnectsAndDimensionChanges();
  void asynchronousDecodePublishesAFrame();
  void newestFrameQueueDropsObsoletePendingFrames();
  void pvaExtractorRetainsRawFrameAndMetadata();
  void pvaExtractorRejectsMalformedFrames();
};

void TestDisplayImportNdArray::registryContainsNtNdArrayImage()
{
  const auto *descriptor = ExtensionObjectRegistry::instance().descriptor(
      QStringLiteral("qtedm_ndarray_image"));
  QVERIFY(descriptor);
  QCOMPARE(descriptor->createTool, CreateTool::kQtedmNdArrayImage);
  QCOMPARE(descriptor->category, QStringLiteral("Monitors"));
}

void TestDisplayImportNdArray::pvaExtractorRetainsRawFrameAndMetadata()
{
  const auto root = rawNtNdArray({1, 2, 3, 4, 5, 6}, {3, 2});
  NtNdArrayFrame frame;
  QString error;
  QVERIFY2(pvaNtNdArrayExtractFrame(root, &frame, &error),
      qPrintable(error));
  QCOMPARE(frame.scalarType, NtNdArrayScalarType::kUInt8);
  QCOMPARE(frame.colorMode, NtNdArrayColorMode::kMono);
  QCOMPARE(frame.elementCount, std::size_t(6));
  QCOMPARE(frame.byteCount, std::size_t(6));
  QCOMPARE(frame.dimensions.size(), 2);
  QCOMPARE(frame.dimensions.at(0).size, 3);
  QCOMPARE(frame.dimensions.at(1).size, 2);
  QCOMPARE(frame.dimensions.at(1).offset, 1);
  QCOMPARE(frame.dimensions.at(1).fullSize, 3);
  QCOMPARE(frame.dimensions.at(1).binning, 2);
  QVERIFY(frame.dimensions.at(1).reverse);
  QCOMPARE(frame.uniqueId, 73);
  QCOMPARE(frame.secondsPastEpoch, qint64(1700000123));
  QCOMPARE(frame.nanoseconds, 456);
  const auto *bytes =
      static_cast<const epics::pvData::uint8 *>(frame.data.get());
  QVERIFY(bytes);
  QCOMPARE(bytes[0], epics::pvData::uint8(1));
  QCOMPARE(bytes[5], epics::pvData::uint8(6));
}

void TestDisplayImportNdArray::pvaExtractorRejectsMalformedFrames()
{
  NtNdArrayFrame frame;
  QString error;
  QVERIFY(!pvaNtNdArrayExtractFrame(
      epics::pvData::PVStructurePtr(), &frame, &error));

  const auto oneDimension = rawNtNdArray({1, 2, 3}, {3});
  QVERIFY(!pvaNtNdArrayExtractFrame(oneDimension, &frame, &error));
  QVERIFY(error.contains(QStringLiteral("2D")));

  const auto empty = rawNtNdArray({}, {2, 2});
  QVERIFY(!pvaNtNdArrayExtractFrame(empty, &frame, &error));
  QVERIFY(error.contains(QStringLiteral("empty")));
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
  QVERIFY(firstAdl.contains(
      "\"composite file\"=\"child_panel.adl;DEVICE=IMPORT\""));
  QVERIFY(!firstAdl.contains("child_paneladl"));
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

void TestDisplayImportNdArray::
    converterUsesSafeDefaultsAndAvoidsArtifactCollisions()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  const QString rootlessGeometryPath =
      directory.filePath(QStringLiteral("rootless.ui"));
  QFile rootlessGeometry(rootlessGeometryPath);
  QVERIFY(rootlessGeometry.open(QIODevice::WriteOnly));
  const QByteArray rootlessUi =
      "<ui version=\"4.0\"><class>Rootless</class>"
      "<widget class=\"QWidget\" name=\"Rootless\">"
      "<widget class=\"QLabel\" name=\"label\">"
      "<property name=\"text\"><string>Default size</string></property>"
      "</widget></widget></ui>";
  QCOMPARE(rootlessGeometry.write(rootlessUi), qint64(rootlessUi.size()));
  rootlessGeometry.close();

  DisplayConversionOptions defaultOptions;
  defaultOptions.inputPath = rootlessGeometryPath;
  defaultOptions.outputPath =
      directory.filePath(QStringLiteral("rootless.adl"));
  const DisplayConversionResult defaultResult =
      DisplayConverter::convert(defaultOptions);
  QVERIFY2(defaultResult.success, qPrintable(defaultResult.error));
  const QByteArray defaultAdl = readAll(defaultResult.outputPath);
  QVERIFY(defaultAdl.contains("width=800"));
  QVERIFY(defaultAdl.contains("height=600"));

  DisplayConversionOptions collisionOptions;
  collisionOptions.inputPath = fixturePath();
  collisionOptions.outputPath =
      directory.filePath(QStringLiteral("collision.adl"));
  collisionOptions.reportPath = directory.filePath(
      QStringLiteral("collision_tab_overview.adl"));
  collisionOptions.sourceCopyPath =
      directory.filePath(QStringLiteral("collision.source.ui"));
  const DisplayConversionResult collisionResult =
      DisplayConverter::convert(collisionOptions);
  QVERIFY2(collisionResult.success, qPrintable(collisionResult.error));
  const QJsonDocument report =
      QJsonDocument::fromJson(readAll(collisionResult.reportPath));
  QVERIFY(report.isObject());
  const QJsonArray generated = report.object()
      .value(QStringLiteral("generated_displays")).toArray();
  bool foundRenamedChild = false;
  for (const QJsonValue &path : generated) {
    foundRenamedChild = foundRenamedChild
        || path.toString().endsWith(
            QStringLiteral("collision_tab_overview_2.adl"));
  }
  QVERIFY(foundRenamedChild);
  QVERIFY(QFileInfo::exists(directory.filePath(
      QStringLiteral("collision_tab_overview_2.adl"))));
}

void TestDisplayImportNdArray::tabbedChildLoadFailureDoesNotLeakWindow()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString childPath =
      directory.filePath(QStringLiteral("broken-child.adl"));
  QFile child(childPath);
  QVERIFY(child.open(QIODevice::WriteOnly));
  QCOMPARE(child.write("file {"), qint64(6));
  child.close();

  const QString parentPath =
      directory.filePath(QStringLiteral("parent.adl"));
  QFile parent(parentPath);
  QVERIFY(parent.open(QIODevice::WriteOnly));
  const QByteArray parentAdl =
      "file { name=\"parent.adl\" version=040004 }\n"
      "display { object { x=0 y=0 width=300 height=200 } "
      "clr=14 bclr=4 cmap=\"\" gridSpacing=5 gridOn=0 snapToGrid=0 }\n"
      "qtedm_tabbed_display {\n"
      "  object { x=10 y=10 width=280 height=180 }\n"
      "  mode=\"tabs\" active_page=\"broken\"\n"
      "  page { id=\"broken\" label=\"Broken\" "
      "display=\"broken-child.adl\" keepAlive=\"false\" }\n"
      "}\n";
  QCOMPARE(parent.write(parentAdl), qint64(parentAdl.size()));
  parent.close();

  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  QString error;
  QVERIFY2(window.loadFromFile(parentPath, &error), qPrintable(error));
  auto topLevelDisplayCount = []() {
    int count = 0;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
      if (dynamic_cast<DisplayWindow *>(widget)) {
        ++count;
      }
    }
    return count;
  };
  const int before = topLevelDisplayCount();
  window.enterExecuteMode();
  QCoreApplication::processEvents();
  QCOMPARE(topLevelDisplayCount(), before);
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
