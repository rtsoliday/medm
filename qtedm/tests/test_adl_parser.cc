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
