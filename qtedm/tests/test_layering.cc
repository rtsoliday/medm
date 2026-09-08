#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>

#include "adl_parser.h"

namespace {

QString steeringFixturePath()
{
  return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
      QStringLiteral("../tests/data/layering/steering_label.adl"));
}

} // namespace

class TestLayering : public QObject
{
  Q_OBJECT

private slots:
  void steeringLabelSurvivesArrowText();
  void steeringLabelRendersLikeMedmReference_data();
  void steeringLabelRendersLikeMedmReference();
};

void TestLayering::steeringLabelSurvivesArrowText()
{
  QFile fixture(steeringFixturePath());
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  QString error;
  const auto root = AdlParser::parse(QString::fromUtf8(fixture.readAll()), &error);
  QVERIFY2(root.has_value(), qPrintable(error));

  /* MEDM preserves the literal arrow and the following, separate label. */
  const auto texts = ::findChildren(*root, QStringLiteral("text"));
  QCOMPARE(texts.size(), 7);
  QCOMPARE(propertyValue(*texts[0], QStringLiteral("textix")),
      QStringLiteral("L2:SC3"));
  QCOMPARE(propertyValue(*texts[1], QStringLiteral("textix")),
      QStringLiteral("/\\"));
  QCOMPARE(propertyValue(*texts[2], QStringLiteral("textix")),
      QStringLiteral("L2:SC3"));
  QCOMPARE(propertyValue(*texts[3], QStringLiteral("textix")),
      QStringLiteral("|"));
  QCOMPARE(propertyValue(*texts[4], QStringLiteral("textix")),
      QStringLiteral("\\/"));
  const auto *object = ::findChild(*texts[2], QStringLiteral("object"));
  QVERIFY(object);
  QCOMPARE(propertyValue(*object, QStringLiteral("x")), QStringLiteral("760"));
  QCOMPARE(propertyValue(*object, QStringLiteral("y")), QStringLiteral("19"));
}

void TestLayering::steeringLabelRendersLikeMedmReference_data()
{
  QTest::addColumn<bool>("literalBackslash");
  QTest::newRow("control-without-backslash") << false;
  QTest::newRow("medm-literal-backslash") << true;
}

void TestLayering::steeringLabelRendersLikeMedmReference()
{
  QFETCH(bool, literalBackslash);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QString fixturePath = steeringFixturePath();
  if (!literalBackslash) {
    /* Positive control: alter only the triggering character in a data copy.
     * This must pass with the current application and validates the oracle. */
    QFile fixture(fixturePath);
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    QByteArray contents = fixture.readAll();
    QVERIFY(contents.contains("textix=\"/\\\""));
    contents.replace("textix=\"/\\\"", "textix=\"/|\"");
    fixturePath = directory.filePath(QStringLiteral("control.adl"));
    QFile control(fixturePath);
    QVERIFY(control.open(QIODevice::WriteOnly));
    QCOMPARE(control.write(contents), qint64(contents.size()));
  }

  const QString path = directory.filePath(QStringLiteral("steering.png"));
  const QDir buildDirectory(QCoreApplication::applicationDirPath());
  QString executable = buildDirectory.filePath(QStringLiteral("qtedm"));
#ifdef Q_OS_WIN
  executable += QStringLiteral(".exe");
#endif
  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
      QStringLiteral("offscreen"));
  environment.insert(QStringLiteral("QTEDM_NOLOG"), QStringLiteral("1"));
  process.setProcessEnvironment(environment);
  process.start(executable, {QStringLiteral("-x"),
      QStringLiteral("-testCaptureScreenshot"), path,
      QStringLiteral("-testExitAfterMs"), QStringLiteral("1000"), fixturePath});
  QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
  QVERIFY2(process.waitForFinished(20000), qPrintable(process.errorString()));
  QCOMPARE(process.exitStatus(), QProcess::NormalExit);
  QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
  const QImage screenshot(path);
  QCOMPARE(screenshot.size(), QSize(880, 140));
  QVERIFY(screenshot.save(buildDirectory.filePath(literalBackslash
      ? QStringLiteral("steering-label-actual.png")
      : QStringLiteral("steering-label-control.png"))));

  /* Both labels are identical in MEDM. Compare within one capture to avoid
   * platform font goldens, and reject an empty reference before comparing. */
  /* Compare the MEDM literal arrowhead with an escaped-backslash reference.
   * The full arrow retains the original display's relative coordinates. */
  QVERIFY2(screenshot.copy(QRect(49, 99, 34, 14))
          == screenshot.copy(QRect(149, 99, 34, 14)),
      "The downward arrowhead must retain both strokes");

  const QImage reference = screenshot.copy(QRect(760, 59, 70, 20));
  const QImage actual = screenshot.copy(QRect(760, 19, 70, 20));
  int darkPixels = 0;
  for (int y = 0; y < reference.height(); ++y) {
    for (int x = 0; x < reference.width(); ++x) {
      const QColor pixel = reference.pixelColor(x, y);
      if (pixel.red() < 64 && pixel.green() < 64 && pixel.blue() < 64) {
        ++darkPixels;
      }
    }
  }
  QVERIFY2(darkPixels > 20, "Reference label must contain visible text");
  QVERIFY2(actual == reference,
      "L2:SC3 after the arrow must render like the unaffected MEDM reference");
}

QTEST_MAIN(TestLayering)
#include "test_layering.moc"
