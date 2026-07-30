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

QTEST_MAIN(TestLegacyFonts)

#include "test_legacy_fonts.moc"
