#include <QtTest/QtTest>

#include <QPaintEvent>
#include <QPointer>
#include <QRegion>
#include <QWidget>

#include "update_coordinator.h"
#include "strip_chart_element.h"

class PaintProbe : public QWidget
{
public:
  int paintCount = 0;
  QRegion paintedRegion;

protected:
  void paintEvent(QPaintEvent *event) override
  {
    ++paintCount;
    paintedRegion |= event->region();
  }
};

class TestUpdateCoordinator : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase();
  void mergesRegionsAndDeduplicatesWidgets();
  void ignoresDestroyedQueuedWidget();
  void circularCacheWraparoundOrder();
};

void TestUpdateCoordinator::initTestCase()
{
  UpdateCoordinator::instance().setUpdateInterval(100);
}

void TestUpdateCoordinator::mergesRegionsAndDeduplicatesWidgets()
{
  PaintProbe probe;
  probe.resize(80, 60);
  probe.show();
  QCoreApplication::processEvents();
  probe.paintCount = 0;
  probe.paintedRegion = QRegion();

  const QRegion first(QRect(2, 3, 10, 11));
  const QRegion second(QRect(40, 30, 12, 13));
  UpdateCoordinator::instance().requestUpdate(&probe, first);
  UpdateCoordinator::instance().requestUpdate(&probe, second);
  UpdateCoordinator::instance().requestUpdate(&probe, first);

  QTRY_COMPARE_WITH_TIMEOUT(probe.paintCount, 1, 400);
  QVERIFY(probe.paintedRegion.intersects(first));
  QVERIFY(probe.paintedRegion.intersects(second));
}

void TestUpdateCoordinator::ignoresDestroyedQueuedWidget()
{
  auto *doomed = new PaintProbe;
  QPointer<PaintProbe> guarded(doomed);
  UpdateCoordinator::instance().requestUpdate(doomed, QRegion(QRect(0, 0, 4, 4)));
  delete doomed;
  QVERIFY(guarded.isNull());

  PaintProbe survivor;
  survivor.resize(30, 30);
  survivor.show();
  QCoreApplication::processEvents();
  survivor.paintCount = 0;
  UpdateCoordinator::instance().requestUpdate(
      &survivor, QRegion(QRect(1, 1, 5, 5)));
  QTRY_COMPARE_WITH_TIMEOUT(survivor.paintCount, 1, 400);
}

void TestUpdateCoordinator::circularCacheWraparoundOrder()
{
  QCOMPARE(StripChartElement::circularDisplayWidths(8, 0),
      (std::array<int, 2>{8, 0}));
  QCOMPARE(StripChartElement::circularDisplayWidths(8, 3),
      (std::array<int, 2>{5, 3}));
  QCOMPARE(StripChartElement::circularDisplayWidths(8, 7),
      (std::array<int, 2>{1, 7}));
  QCOMPARE(StripChartElement::circularDisplayWidths(0, 0),
      (std::array<int, 2>{0, 0}));
}

QTEST_MAIN(TestUpdateCoordinator)

#include "test_update_coordinator.moc"
