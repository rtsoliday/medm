#include <QtTest/QtTest>

#include <QFile>

#include "adl_parser.h"

class TestAdlParser : public QObject
{
  Q_OBJECT

private slots:
  void parsesMinimalFixture();
  void parsesCommentsAndQuotedValues();
  void parsesEscapedQuotedValues();
  void parsesExpressionChannelBlock();
  void parsesLedMonitorBlock();
  void parsesTextAreaBlock();
  void parsesPvTableBlock();
  void parsesWaveTableBlock();
  void rejectsMalformedInput();
};

void TestAdlParser::parsesMinimalFixture()
{
  QFile file(QStringLiteral("tests/data/minimal/basic_display.adl"));
  QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
      qPrintable(file.errorString()));

  QString errorMessage;
  const std::optional<AdlNode> root =
      AdlParser::parse(QString::fromUtf8(file.readAll()), &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *fileNode = ::findChild(*root, QStringLiteral("file"));
  QVERIFY(fileNode);
  QCOMPARE(propertyValue(*fileNode, QStringLiteral("name")),
      QStringLiteral("basic-display"));

  const AdlNode *displayNode = ::findChild(*fileNode, QStringLiteral("DISPLAY"));
  QVERIFY(displayNode);

  const AdlNode *objectNode = ::findChild(*displayNode, QStringLiteral("object"));
  QVERIFY(objectNode);
  QCOMPARE(propertyValue(*objectNode, QStringLiteral("width")),
      QStringLiteral("300"));

  const AdlNode *specialNode =
      ::findChild(*fileNode, QStringLiteral("special_block"));
  QVERIFY(specialNode);
  QCOMPARE(propertyValue(*specialNode, QStringLiteral("title")),
      QStringLiteral("Hello world"));
}

void TestAdlParser::parsesCommentsAndQuotedValues()
{
  const QString text = QStringLiteral(
      "# leading comment\n"
      "<<Composite>> {\n"
      "  text=\"hello there\"\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));
  QCOMPARE(normalizedAdlName(QStringLiteral(" <<Composite>> ")),
      QStringLiteral("composite"));

  const AdlNode *compositeNode =
      ::findChild(*root, QStringLiteral("composite"));
  QVERIFY(compositeNode);
  QCOMPARE(propertyValue(*compositeNode, QStringLiteral("text")),
      QStringLiteral("hello there"));
}

void TestAdlParser::parsesEscapedQuotedValues()
{
  const QString text = QStringLiteral(
      "text {\n"
      "  textix=\"chan=\\\" \\\" path=\\\\tmp\\\\screen adl\\\\n\"\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *textNode = ::findChild(*root, QStringLiteral("text"));
  QVERIFY(textNode);
  QCOMPARE(propertyValue(*textNode, QStringLiteral("textix")),
      QStringLiteral("chan=\" \" path=\\tmp\\screen adl\\n"));
}

void TestAdlParser::parsesExpressionChannelBlock()
{
  const QString text = QStringLiteral(
      "expression_channel {\n"
      "  object {\n"
      "    x=10\n"
      "    y=12\n"
      "    width=80\n"
      "    height=40\n"
      "  }\n"
      "  variable=\"soft:sum\"\n"
      "  calc=\"A+B\"\n"
      "  channelA=\"src:a\"\n"
      "  channelB=\"src:b\"\n"
      "  initialValue=1.5\n"
      "  eventSignal=\"onAnyChange\"\n"
      "  clr=14\n"
      "  bclr=4\n"
      "  precision=3\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *expressionNode =
      ::findChild(*root, QStringLiteral("expression_channel"));
  QVERIFY(expressionNode);
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("variable")),
      QStringLiteral("soft:sum"));
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("calc")),
      QStringLiteral("A+B"));
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("channelA")),
      QStringLiteral("src:a"));
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("channelB")),
      QStringLiteral("src:b"));
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("initialValue")),
      QStringLiteral("1.5"));
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("eventSignal")),
      QStringLiteral("onAnyChange"));
  QCOMPARE(propertyValue(*expressionNode, QStringLiteral("precision")),
      QStringLiteral("3"));
}

void TestAdlParser::parsesLedMonitorBlock()
{
  const QString text = QStringLiteral(
      "led_monitor {\n"
      "  object {\n"
      "    x=16\n"
      "    y=18\n"
      "    width=24\n"
      "    height=26\n"
      "  }\n"
      "  monitor {\n"
      "    chan=\"soft:state\"\n"
      "    clr=25\n"
      "    bclr=4\n"
      "  }\n"
      "  \"dynamic attribute\" {\n"
      "    attr {\n"
      "      vis=\"calc\"\n"
      "      calc=\"A\"\n"
      "    }\n"
      "    chan=\"soft:state\"\n"
      "    chanB=\"soft:mask\"\n"
      "  }\n"
      "  colorMode=\"discrete\"\n"
      "  shape=\"rounded_square\"\n"
      "  bezel=1\n"
      "  stateCount=4\n"
      "  stateColor0=12\n"
      "  stateColor1=15\n"
      "  stateColor2=33\n"
      "  stateColor3=20\n"
      "  undefinedColor=7\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *ledNode = ::findChild(*root, QStringLiteral("led_monitor"));
  QVERIFY(ledNode);
  QCOMPARE(propertyValue(*ledNode, QStringLiteral("colorMode")),
      QStringLiteral("discrete"));
  QCOMPARE(propertyValue(*ledNode, QStringLiteral("shape")),
      QStringLiteral("rounded_square"));
  QCOMPARE(propertyValue(*ledNode, QStringLiteral("bezel")),
      QStringLiteral("1"));
  QCOMPARE(propertyValue(*ledNode, QStringLiteral("stateCount")),
      QStringLiteral("4"));
  QCOMPARE(propertyValue(*ledNode, QStringLiteral("stateColor2")),
      QStringLiteral("33"));
  QCOMPARE(propertyValue(*ledNode, QStringLiteral("undefinedColor")),
      QStringLiteral("7"));

  const AdlNode *monitorNode = ::findChild(*ledNode, QStringLiteral("monitor"));
  QVERIFY(monitorNode);
  QCOMPARE(propertyValue(*monitorNode, QStringLiteral("chan")),
      QStringLiteral("soft:state"));
  QCOMPARE(propertyValue(*monitorNode, QStringLiteral("clr")),
      QStringLiteral("25"));
  QCOMPARE(propertyValue(*monitorNode, QStringLiteral("bclr")),
      QStringLiteral("4"));

  const AdlNode *dynamicNode =
      ::findChild(*ledNode, QStringLiteral("dynamic attribute"));
  QVERIFY(dynamicNode);
  QCOMPARE(propertyValue(*dynamicNode, QStringLiteral("chan")),
      QStringLiteral("soft:state"));
  QCOMPARE(propertyValue(*dynamicNode, QStringLiteral("chanB")),
      QStringLiteral("soft:mask"));
  const AdlNode *attrNode = ::findChild(*dynamicNode, QStringLiteral("attr"));
  QVERIFY(attrNode);
  QCOMPARE(propertyValue(*attrNode, QStringLiteral("vis")),
      QStringLiteral("calc"));
  QCOMPARE(propertyValue(*attrNode, QStringLiteral("calc")),
      QStringLiteral("A"));
}

void TestAdlParser::parsesTextAreaBlock()
{
  const QString text = QStringLiteral(
      "text_area {\n"
      "  object {\n"
      "    x=20\n"
      "    y=24\n"
      "    width=180\n"
      "    height=90\n"
      "  }\n"
      "  control {\n"
      "    chan=\"soft:longText\"\n"
      "    clr=14\n"
      "    bclr=4\n"
      "  }\n"
      "  clrmod=\"alarm\"\n"
      "  format=\"string\"\n"
      "  readOnly=1\n"
      "  wordWrap=0\n"
      "  lineWrapMode=\"fixedColumnWidth\"\n"
      "  wrapColumnWidth=96\n"
      "  showVerticalScrollBar=0\n"
      "  showHorizontalScrollBar=1\n"
      "  commitMode=\"explicit\"\n"
      "  tabInsertsSpaces=0\n"
      "  tabWidth=4\n"
      "  fontFamily=\"DejaVu Sans Mono\"\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *textAreaNode =
      ::findChild(*root, QStringLiteral("text_area"));
  QVERIFY(textAreaNode);
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("clrmod")),
      QStringLiteral("alarm"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("format")),
      QStringLiteral("string"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("readOnly")),
      QStringLiteral("1"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("wordWrap")),
      QStringLiteral("0"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("lineWrapMode")),
      QStringLiteral("fixedColumnWidth"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("wrapColumnWidth")),
      QStringLiteral("96"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("showVerticalScrollBar")),
      QStringLiteral("0"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("showHorizontalScrollBar")),
      QStringLiteral("1"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("commitMode")),
      QStringLiteral("explicit"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("tabInsertsSpaces")),
      QStringLiteral("0"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("tabWidth")),
      QStringLiteral("4"));
  QCOMPARE(propertyValue(*textAreaNode, QStringLiteral("fontFamily")),
      QStringLiteral("DejaVu Sans Mono"));

  const AdlNode *controlNode =
      ::findChild(*textAreaNode, QStringLiteral("control"));
  QVERIFY(controlNode);
  QCOMPARE(propertyValue(*controlNode, QStringLiteral("chan")),
      QStringLiteral("soft:longText"));
  QCOMPARE(propertyValue(*controlNode, QStringLiteral("clr")),
      QStringLiteral("14"));
  QCOMPARE(propertyValue(*controlNode, QStringLiteral("bclr")),
      QStringLiteral("4"));
}

void TestAdlParser::parsesPvTableBlock()
{
  const QString text = QStringLiteral(
      "pv_table {\n"
      "  object {\n"
      "    x=40\n"
      "    y=80\n"
      "    width=520\n"
      "    height=220\n"
      "  }\n"
      "  \"basic attribute\" {\n"
      "    clr=14\n"
      "    bclr=4\n"
      "  }\n"
      "  colorMode=\"alarm\"\n"
      "  showHeaders=1\n"
      "  fontSize=14\n"
      "  columns=\"label,pv,value,severity\"\n"
      "  row {\n"
      "    label=\"Beam Current\"\n"
      "    chan=\"pvtable:test:beamCurrent\"\n"
      "  }\n"
      "  row {\n"
      "    label=\"Mode\"\n"
      "    chan=\"pvtable:test:mode\"\n"
      "  }\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *tableNode = ::findChild(*root, QStringLiteral("pv_table"));
  QVERIFY(tableNode);
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("colorMode")),
      QStringLiteral("alarm"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("showHeaders")),
      QStringLiteral("1"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("fontSize")),
      QStringLiteral("14"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("columns")),
      QStringLiteral("label,pv,value,severity"));
  const AdlNode *row0 = ::findChild(*tableNode, QStringLiteral("row"));
  QVERIFY(row0);
  QCOMPARE(propertyValue(*row0, QStringLiteral("label")),
      QStringLiteral("Beam Current"));
  QCOMPARE(propertyValue(*row0, QStringLiteral("chan")),
      QStringLiteral("pvtable:test:beamCurrent"));
}

void TestAdlParser::parsesWaveTableBlock()
{
  const QString text = QStringLiteral(
      "wave_table {\n"
      "  object {\n"
      "    x=40\n"
      "    y=80\n"
      "    width=520\n"
      "    height=220\n"
      "  }\n"
      "  \"basic attribute\" {\n"
      "    clr=14\n"
      "    bclr=4\n"
      "  }\n"
      "  chan=\"wavetable:test:doubleWave\"\n"
      "  colorMode=\"alarm\"\n"
      "  showHeaders=1\n"
      "  fontSize=13\n"
      "  layout=\"grid\"\n"
      "  columns=8\n"
      "  maxElements=32\n"
      "  indexBase=1\n"
      "  valueFormat=\"scientific\"\n"
      "  charMode=\"bytes\"\n"
      "}\n");

  QString errorMessage;
  const std::optional<AdlNode> root = AdlParser::parse(text, &errorMessage);

  QVERIFY2(root.has_value(), qPrintable(errorMessage));

  const AdlNode *tableNode = ::findChild(*root, QStringLiteral("wave_table"));
  QVERIFY(tableNode);
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("chan")),
      QStringLiteral("wavetable:test:doubleWave"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("fontSize")),
      QStringLiteral("13"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("layout")),
      QStringLiteral("grid"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("columns")),
      QStringLiteral("8"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("maxElements")),
      QStringLiteral("32"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("indexBase")),
      QStringLiteral("1"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("valueFormat")),
      QStringLiteral("scientific"));
  QCOMPARE(propertyValue(*tableNode, QStringLiteral("charMode")),
      QStringLiteral("bytes"));
}

void TestAdlParser::rejectsMalformedInput()
{
  const QStringList invalidFixtures = {
      QStringLiteral("tests/data/invalid/unterminated_block.adl"),
      QStringLiteral("tests/data/invalid/unterminated_string.adl"),
  };

  for (const QString &fixture : invalidFixtures) {
    QFile file(fixture);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
        qPrintable(file.errorString()));

    QString errorMessage;
    const std::optional<AdlNode> root =
        AdlParser::parse(QString::fromUtf8(file.readAll()), &errorMessage);

    QVERIFY(!root.has_value());
    QVERIFY(!errorMessage.isEmpty());
  }
}

QTEST_APPLESS_MAIN(TestAdlParser)

#include "test_adl_parser.moc"
