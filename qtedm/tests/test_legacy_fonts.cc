#include <QtTest/QtTest>

#include <QFontInfo>
#include <QFontMetrics>

#include "legacy_fonts.h"

class TestLegacyFonts : public QObject
{
  Q_OBJECT

private slots:
  void loadsEmbeddedBitstreamCharter();
  void preservesLegacyPixelSizeSemantics();
  void fixedFontsKeepCellWidths_data();
  void fixedFontsKeepCellWidths();
};

void TestLegacyFonts::loadsEmbeddedBitstreamCharter()
{
  const QFont charter = LegacyFonts::font(QStringLiteral(
      "-bitstream-charter-bold-r-normal--24-240-75-75-p-150-iso8859-1"));
  const QFontInfo info(charter);
  QVERIFY2(charter.family().contains(QStringLiteral("Charter"),
      Qt::CaseInsensitive),
      qPrintable(QStringLiteral("requested=%1 resolved=%2")
          .arg(charter.family(), info.family())));
  QCOMPARE(charter.pixelSize(), 24);
  QVERIFY(charter.bold());
}

void TestLegacyFonts::preservesLegacyPixelSizeSemantics()
{
  LegacyFonts::setWidgetDMAliasMode(LegacyFonts::WidgetDMAliasMode::kFixed);

  const QFont base = LegacyFonts::font(QStringLiteral("widgetDM_10"));
  QVERIFY(!base.family().isEmpty());
  const int basePixelSize = QFontInfo(base).pixelSize();
  const int baseHeight = QFontMetrics(base).height();
  QVERIFY(basePixelSize > 0);

  QFont pointSized = base;
  pointSized.setPointSize(10);
  QCOMPARE(pointSized.pointSize(), 10);
  QCOMPARE(pointSized.pixelSize(), -1);
  QVERIFY(base.pixelSize() > 0);
  QCOMPARE(base.pointSize(), -1);

  const QFont resolved10 = LegacyFonts::fontForLegacySize(base, 10);
  QCOMPARE(QFontInfo(resolved10).pixelSize(), basePixelSize);
  QCOMPARE(QFontMetrics(resolved10).height(), baseHeight);

  const QFont resolved9 = LegacyFonts::fontForLegacySize(base, 9);
  QVERIFY(QFontInfo(resolved9).pixelSize() <= basePixelSize);
  QVERIFY(QFontMetrics(resolved9).height() <= baseHeight);

  const QFont resolved30 = LegacyFonts::fontForLegacySize(base, 30);
  const int resolved30PixelSize = QFontInfo(resolved30).pixelSize();

  QVERIFY2(resolved30PixelSize >= 30,
      qPrintable(QStringLiteral("expected at least 30px, resolved=%1")
          .arg(resolved30PixelSize)));
}

void TestLegacyFonts::fixedFontsKeepCellWidths_data()
{
  QTest::addColumn<QString>("key");
  QTest::addColumn<int>("cellWidth");

  /* MEDM's character-cell aliases and the widths of the X11 fonts that
   * fonts.alias assigns to them. */
  QTest::newRow("widgetDM_4") << QStringLiteral("widgetDM_4") << 5;
  QTest::newRow("widgetDM_6") << QStringLiteral("widgetDM_6") << 5;
  QTest::newRow("widgetDM_8") << QStringLiteral("widgetDM_8") << 6;
  QTest::newRow("widgetDM_10") << QStringLiteral("widgetDM_10") << 6;
  QTest::newRow("widgetDM_12") << QStringLiteral("widgetDM_12") << 7;
  QTest::newRow("widgetDM_14") << QStringLiteral("widgetDM_14") << 7;
  QTest::newRow("widgetDM_16") << QStringLiteral("widgetDM_16") << 9;
  QTest::newRow("widgetDM_18") << QStringLiteral("widgetDM_18") << 8;
  QTest::newRow("widgetDM_20") << QStringLiteral("widgetDM_20") << 10;
  QTest::newRow("widgetDM_22") << QStringLiteral("widgetDM_22") << 12;
  QTest::newRow("widgetDM_24") << QStringLiteral("widgetDM_24") << 12;
}

void TestLegacyFonts::fixedFontsKeepCellWidths()
{
  QFETCH(QString, key);
  QFETCH(int, cellWidth);

  /* MEDM draws text in these X11 character-cell fonts, so every character,
   * spaces included, advances one cell.  A space missing from the font is
   * drawn with a wider fallback font and long labels no longer fit. */
  LegacyFonts::setWidgetDMAliasMode(LegacyFonts::WidgetDMAliasMode::kFixed);
  const QFontMetrics metrics(LegacyFonts::font(key));
  const QString text = QStringLiteral("The RF water load is used 0123456789");
  QCOMPARE(metrics.horizontalAdvance(text), cellWidth * text.size());
}

QTEST_MAIN(TestLegacyFonts)

#include "test_legacy_fonts.moc"
