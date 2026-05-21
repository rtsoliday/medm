#include <QtTest/QtTest>

#include "cartesian_plot_properties.h"

class TestCartesianPlotProperties : public QObject
{
  Q_OBJECT

private slots:
  void defaultTraceAxisMatchesMedm();
  void defaultTraceSideMatchesMedm();
};

void TestCartesianPlotProperties::defaultTraceAxisMatchesMedm()
{
  QCOMPARE(defaultCartesianPlotYAxisForTrace(0), CartesianPlotYAxis::kY1);
  QCOMPARE(defaultCartesianPlotYAxisForTrace(1), CartesianPlotYAxis::kY2);
  QCOMPARE(defaultCartesianPlotYAxisForTrace(7), CartesianPlotYAxis::kY2);
}

void TestCartesianPlotProperties::defaultTraceSideMatchesMedm()
{
  QVERIFY(!defaultCartesianPlotUsesRightAxisForTrace(0));
  QVERIFY(defaultCartesianPlotUsesRightAxisForTrace(1));
  QVERIFY(defaultCartesianPlotUsesRightAxisForTrace(7));
}

QTEST_APPLESS_MAIN(TestCartesianPlotProperties)

#include "test_cartesian_plot_properties.moc"
