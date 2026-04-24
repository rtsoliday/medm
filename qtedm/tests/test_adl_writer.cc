#include <QtTest/QtTest>

#include <QFile>
#include <QTextStream>

#include "adl_writer.h"

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
  void writesDynamicAttributeSection();
  void writesLimitsSection();
  void writesTextAreaEnumStrings();
  void writesHeatmapProfileModeEnumStrings();
  void omitsDefaultCartesianAxisSection();
};

void TestAdlWriter::escapesSpecialCharacters()
{
  QCOMPARE(AdlWriter::escapeAdlString(QStringLiteral("a\"b\nc\t\\")),
      QStringLiteral("a\\\"b\\nc\\t\\\\"));
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
