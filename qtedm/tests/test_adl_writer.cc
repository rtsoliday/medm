#include <QtTest/QtTest>

#include <QFile>
#include <QTextStream>

#include "adl_writer.h"
#include "adl_parser.h"

namespace {

QString readFixture(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  QString text = QString::fromUtf8(file.readAll());
  if (text.endsWith(QLatin1Char('\n'))) {
    text.chop(1);
  }
  return text;
}

}  // namespace

class TestAdlWriter : public QObject
{
  Q_OBJECT

private slots:
  void escapesSpecialCharacters();
  void roundTripsUnicodeStrings_data();
  void roundTripsUnicodeStrings();
  void preservesSmallNonDefaultDoubles();
  void writesDynamicAttributeSection();
  void writesLimitsSection();
  void preciseRangesRoundTrip_data();
  void preciseRangesRoundTrip();
  void writesTextAreaEnumStrings();
  void writesHeatmapProfileModeEnumStrings();
  void writesHeatmapRangeModeEnumStrings();
  void writesPvTableColumnStrings();
  void writesWaveTableEnumStrings();
  void omitsDefaultCartesianAxisSection();
};

void TestAdlWriter::escapesSpecialCharacters()
{
  QCOMPARE(AdlWriter::escapeAdlString(QStringLiteral("a\"b\nc\t\\")),
      QStringLiteral("a\\\"b\\nc\\t\\\\"));
}

void TestAdlWriter::roundTripsUnicodeStrings_data()
{
  QTest::addColumn<QString>("original");
  QTest::addColumn<QString>("escaped");
  QTest::newRow("latin1-degree")
      << QStringLiteral("Temperature \u00b0C")
      << QStringLiteral("Temperature \u00b0C");
  QTest::newRow("non-latin1")
      << QStringLiteral("\u03a9 \u4e16\u754c")
      << QStringLiteral("\\u03a9 \\u4e16\\u754c");
  QTest::newRow("nonprinting-format")
      << QStringLiteral("left\u200bright")
      << QStringLiteral("left\\u200bright");
  QTest::newRow("line-paragraph-separators")
      << QStringLiteral("left\u2028middle\u2029right")
      << QStringLiteral("left\\u2028middle\\u2029right");
  QTest::newRow("supplementary")
      << QStringLiteral("\U0001f680")
      << QStringLiteral("\\ud83d\\ude80");
  QTest::newRow("literal-unicode-escape")
      << QStringLiteral("\\u03a9")
      << QStringLiteral("\\\\u03a9");
}

void TestAdlWriter::roundTripsUnicodeStrings()
{
  QFETCH(QString, original);
  QFETCH(QString, escaped);
  QCOMPARE(AdlWriter::escapeAdlString(original), escaped);

  /* Exercise the actual file encoding, not only an in-memory UTF-16 parse. */
  const QByteArray serialized = (QStringLiteral("text { textix=\"")
      + escaped + QStringLiteral("\" }")).toLatin1();
  QString error;
  const auto root = AdlParser::parse(QString::fromLatin1(serialized), &error);
  QVERIFY2(root.has_value(), qPrintable(error));
  const AdlNode *text = ::findChild(*root, QStringLiteral("text"));
  QVERIFY(text);
  QCOMPARE(propertyValue(*text, QStringLiteral("textix")), original);
}

void TestAdlWriter::preservesSmallNonDefaultDoubles()
{
  QVERIFY(AdlWriter::isNonDefaultDouble(1e-13, 0.0));
  QVERIFY(AdlWriter::isNonDefaultDouble(1.0 - 1e-13, 1.0));
  QVERIFY(!AdlWriter::isNonDefaultDouble(0.0, 0.0));
  QVERIFY(!AdlWriter::isNonDefaultDouble(1.0, 1.0));
}

void TestAdlWriter::writesDynamicAttributeSection()
{
  QString output;
  QTextStream stream(&output);
  std::array<QString, 5> channels{};
  channels[0] = QStringLiteral("pv:a");
  channels[1] = QStringLiteral("pv:b");

  AdlWriter::writeDynamicAttributeSection(stream, 1, TextColorMode::kAlarm,
      TextVisibilityMode::kCalc, QStringLiteral("A==B\nC"), channels);

  const QString expected =
      readFixture(QStringLiteral("tests/data/expected/dynamic_attribute.adlfrag"));
  QCOMPARE(output, expected);
}

void TestAdlWriter::preciseRangesRoundTrip_data()
{
  QTest::addColumn<double>("low");
  QTest::addColumn<double>("high");
  QTest::addColumn<int>("source");
  for (int source = 0; source < 3; ++source) {
    QTest::newRow(qPrintable(QStringLiteral("small-%1").arg(source)))
        << 1e-10 << 1.2345678901234567 << source;
    QTest::newRow(qPrintable(QStringLiteral("near-default-%1").arg(source)))
        << 1e-13 << 1.0000000000000002 << source;
    QTest::newRow(qPrintable(QStringLiteral("large-%1").arg(source)))
        << -1.2345678901234567e100 << 2.3456789012345678e100 << source;
  }
}

void TestAdlWriter::preciseRangesRoundTrip()
{
  QFETCH(double, low);
  QFETCH(double, high);
  QFETCH(int, source);
  PvLimits limits;
  const auto selected = source == 0 ? PvLimitSource::kChannel
      : source == 1 ? PvLimitSource::kDefault : PvLimitSource::kUser;
  limits.lowSource = selected;
  limits.highSource = selected;
  limits.lowDefault = low;
  limits.highDefault = high;
  QString output;
  QTextStream stream(&output);
  AdlWriter::writeLimitsSection(stream, 0, limits, true, false, true, true, true, true);
  AdlWriter::writeCartesianAxisSection(stream, 0, 0,
      CartesianPlotAxisStyle::kLinear, CartesianPlotRangeStyle::kUserSpecified,
      low, high, CartesianPlotTimeFormat::kHhMmSs, true);
  QString error;
  const auto parsed = AdlParser::parse(output, &error);
  QVERIFY2(parsed.has_value(), qPrintable(error));
  const AdlNode *saved = ::findChild(*parsed, QStringLiteral("limits"));
  QVERIFY(saved);
  QVERIFY(propertyValue(*saved, QStringLiteral("loprDefault")).toDouble() == low);
  QVERIFY(propertyValue(*saved, QStringLiteral("hoprDefault")).toDouble() == high);
  const AdlNode *axis = ::findChild(*parsed, QStringLiteral("x_axis"));
  QVERIFY(axis);
  QVERIFY(propertyValue(*axis, QStringLiteral("minRange")).toDouble() == low);
  QVERIFY(propertyValue(*axis, QStringLiteral("maxRange")).toDouble() == high);
}

void TestAdlWriter::writesLimitsSection()
{
  QString output;
  QTextStream stream(&output);
  PvLimits limits;
  limits.lowSource = PvLimitSource::kUser;
  limits.lowDefault = -1.5;
  limits.highSource = PvLimitSource::kUser;
  limits.highDefault = 12.5;
  limits.precisionSource = PvLimitSource::kUser;
  limits.precisionDefault = 3;

  AdlWriter::writeLimitsSection(stream, 1, limits, false, false, false, false,
      false, false);

  const QString expected =
      readFixture(QStringLiteral("tests/data/expected/limits_section.adlfrag"));
  QCOMPARE(output, expected);
}

void TestAdlWriter::writesTextAreaEnumStrings()
{
  QCOMPARE(AdlWriter::textAreaWrapModeString(TextAreaWrapMode::kNoWrap),
      QStringLiteral("noWrap"));
  QCOMPARE(AdlWriter::textAreaWrapModeString(
      TextAreaWrapMode::kWidgetWidth), QStringLiteral("widgetWidth"));
  QCOMPARE(AdlWriter::textAreaWrapModeString(
      TextAreaWrapMode::kFixedColumnWidth),
      QStringLiteral("fixedColumnWidth"));

  QCOMPARE(AdlWriter::textAreaCommitModeString(
      TextAreaCommitMode::kCtrlEnter), QStringLiteral("ctrlEnter"));
  QCOMPARE(AdlWriter::textAreaCommitModeString(
      TextAreaCommitMode::kEnter), QStringLiteral("enter"));
  QCOMPARE(AdlWriter::textAreaCommitModeString(
      TextAreaCommitMode::kOnFocusLost), QStringLiteral("onFocusLost"));
  QCOMPARE(AdlWriter::textAreaCommitModeString(
      TextAreaCommitMode::kExplicit), QStringLiteral("explicit"));
}

void TestAdlWriter::writesHeatmapProfileModeEnumStrings()
{
  QCOMPARE(AdlWriter::heatmapProfileModeString(
      HeatmapProfileMode::kAbsolute), QStringLiteral("absolute"));
  QCOMPARE(AdlWriter::heatmapProfileModeString(
      HeatmapProfileMode::kAveraged), QStringLiteral("averaged"));
}

void TestAdlWriter::writesHeatmapRangeModeEnumStrings()
{
  QCOMPARE(AdlWriter::heatmapRangeModeString(
      HeatmapRangeMode::kAuto), QStringLiteral("auto"));
  QCOMPARE(AdlWriter::heatmapRangeModeString(
      HeatmapRangeMode::kManual), QStringLiteral("manual"));
}

void TestAdlWriter::writesPvTableColumnStrings()
{
  QCOMPARE(AdlWriter::pvTableColumnString(QStringLiteral("label")),
      QStringLiteral("label"));
  QCOMPARE(AdlWriter::pvTableColumnString(QStringLiteral("units")),
      QStringLiteral("units"));
  QCOMPARE(AdlWriter::pvTableColumnString(QStringLiteral("bogus")),
      QString());
}

void TestAdlWriter::writesWaveTableEnumStrings()
{
  QCOMPARE(AdlWriter::waveTableLayoutString(WaveTableLayout::kGrid),
      QStringLiteral("grid"));
  QCOMPARE(AdlWriter::waveTableLayoutString(WaveTableLayout::kColumn),
      QStringLiteral("column"));
  QCOMPARE(AdlWriter::waveTableValueFormatString(
      WaveTableValueFormat::kScientific), QStringLiteral("scientific"));
  QCOMPARE(AdlWriter::waveTableValueFormatString(
      WaveTableValueFormat::kEngineering), QStringLiteral("engineering"));
  QCOMPARE(AdlWriter::waveTableCharModeString(WaveTableCharMode::kString),
      QStringLiteral("string"));
  QCOMPARE(AdlWriter::waveTableCharModeString(WaveTableCharMode::kBytes),
      QStringLiteral("bytes"));
}

void TestAdlWriter::omitsDefaultCartesianAxisSection()
{
  QString output;
  QTextStream stream(&output);

  AdlWriter::writeCartesianAxisSection(stream, 1, 0,
      CartesianPlotAxisStyle::kLinear, CartesianPlotRangeStyle::kChannel, 0.0,
      1.0, CartesianPlotTimeFormat::kHhMmSs, false);

  QVERIFY(output.isEmpty());
}

QTEST_APPLESS_MAIN(TestAdlWriter)

#include "test_adl_writer.moc"
