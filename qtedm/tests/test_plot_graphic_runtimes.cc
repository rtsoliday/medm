#include <QtTest/QtTest>

#include <QImage>
#include <QPainter>
#include <QFile>
#include <QTemporaryDir>

#include <limits>

#include "cartesian_plot_element.h"
#include "heatmap_element.h"
#include "image_element.h"
#include "plot_export_utils.h"
#include "rectangle_element.h"
#include "widget_image_export_utils.h"

namespace {

QImage renderWidget(QWidget *widget)
{
  QImage image(widget->size(), QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  widget->render(&painter);
  painter.end();
  return image;
}

}  // namespace

class TestPlotGraphicRuntimes : public QObject
{
  Q_OBJECT

private slots:
  void cartesianTracksTraceModesPointsAndCapacity();
  void heatmapTracksDimensionsRangeAndTransforms();
  void dynamicGraphicAppliesRuntimeVisibilityAndSeverity();
  void imageTracksRuntimeFrameSelection();
  void cartesianExportsCsvSddsAndFailures();
  void rendersWidgetsToExplicitPathsAndReportsFailures();
};

void TestPlotGraphicRuntimes::cartesianTracksTraceModesPointsAndCapacity()
{
  CartesianPlotElement plot;
  plot.resize(480, 300);
  plot.setCount(4);
  plot.setTraceXChannel(0, QStringLiteral("plot:x"));
  plot.setTraceYChannel(0, QStringLiteral("plot:y"));
  plot.setExecuteMode(true);
  plot.setTraceRuntimeMode(0, CartesianPlotTraceMode::kXYScalar);
  plot.setTraceRuntimeConnected(0, true);
  plot.updateTraceRuntimeData(0,
      {QPointF(1.0, 2.0), QPointF(3.0, 4.0)});

  QCOMPARE(plot.effectiveSampleCapacity(), 4);
  QCOMPARE(plot.dataPointCount(0), 2);
  QCOMPARE(plot.dataPoint(0, 1), QPointF(3.0, 4.0));
  QVERIFY(plot.traceHasData(0));

  plot.setRuntimeCount(2);
  QCOMPARE(plot.effectiveSampleCapacity(), 2);
  plot.setAxisRuntimeLimits(0, -5.0, 5.0, true);
  plot.setRuntimePaintReady(true, true);
  QVERIFY(!renderWidget(&plot).isNull());

  plot.clearTraceRuntimeData(0);
  QCOMPARE(plot.dataPointCount(0), 0);
  QVERIFY(!plot.traceHasData(0));
  plot.clearRuntimeState();
  QCOMPARE(plot.effectiveSampleCapacity(), 4);
}

void TestPlotGraphicRuntimes::heatmapTracksDimensionsRangeAndTransforms()
{
  HeatmapElement heatmap;
  heatmap.resize(320, 220);
  heatmap.setXDimensionSource(HeatmapDimensionSource::kChannel);
  heatmap.setYDimensionSource(HeatmapDimensionSource::kChannel);
  heatmap.setExecuteMode(true);
  heatmap.setRuntimeConnected(true);
  heatmap.setRuntimeDimensions(3, 2);
  heatmap.setRuntimeData({1.0, 2.0,
      std::numeric_limits<double>::quiet_NaN(), 4.0, 5.0, 6.0});

  QCOMPARE(heatmap.effectiveDimensions(), QSize(3, 2));
  QCOMPARE(heatmap.runtimeDataCount(), 6);
  QVERIFY(heatmap.hasRuntimeData());
  QVERIFY(!renderWidget(&heatmap).isNull());
  QVERIFY(heatmap.hasRuntimeRange());
  QCOMPARE(heatmap.runtimeMinimum(), 1.0);
  QCOMPARE(heatmap.runtimeMaximum(), 6.0);

  heatmap.setRotation(HeatmapRotation::k90);
  heatmap.setFlipHorizontal(true);
  double x = 0.25;
  double y = 0.75;
  heatmap.mapVisualToDataFraction(x, y);
  QCOMPARE(x, 0.25);
  QCOMPARE(y, 0.75);
  bool zoomX = true;
  bool zoomY = false;
  heatmap.mapVisualToDataZoomFlags(zoomX, zoomY);
  QVERIFY(!zoomX);
  QVERIFY(zoomY);

  heatmap.clearRuntimeState();
  QVERIFY(!heatmap.hasRuntimeData());
  QCOMPARE(heatmap.runtimeDataCount(), 0);
}

void TestPlotGraphicRuntimes::dynamicGraphicAppliesRuntimeVisibilityAndSeverity()
{
  QWidget parent;
  parent.resize(180, 120);
  RectangleElement rectangle(&parent);
  rectangle.setGeometry(10, 10, 120, 80);
  rectangle.setChannel(0, QStringLiteral("shape:value"));
  rectangle.setColorMode(TextColorMode::kAlarm);
  rectangle.setVisibilityMode(TextVisibilityMode::kCalc);
  rectangle.setVisibilityCalc(QStringLiteral("A>0"));
  parent.show();
  rectangle.show();
  rectangle.setExecuteMode(true);
  rectangle.setRuntimeConnected(true);
  rectangle.setRuntimeVisible(true);
  QCoreApplication::processEvents();
  QVERIFY(rectangle.isVisible());

  rectangle.setRuntimeSeverity(0);
  const QImage normal = renderWidget(&rectangle);
  rectangle.setRuntimeSeverity(2);
  const QImage major = renderWidget(&rectangle);
  QVERIFY(normal != major);

  rectangle.setRuntimeVisible(false);
  QVERIFY(!rectangle.isVisible());
  rectangle.setRuntimeVisible(true);
  QVERIFY(rectangle.isVisible());
  rectangle.setRuntimeConnected(false);
  QVERIFY(!renderWidget(&rectangle).isNull());
}

void TestPlotGraphicRuntimes::imageTracksRuntimeFrameSelection()
{
  ImageElement image;
  image.setBaseDirectory(QCoreApplication::applicationDirPath());
  image.setImageType(ImageType::kGif);
  image.setImageName(QStringLiteral("../../tests/test.gif"));
  QVERIFY(image.frameCount() >= 1);
  image.setRuntimeFrameValid(true);
  image.setRuntimeFrameIndex(1000);
  QVERIFY(image.runtimeFrameIndex() >= 0);
  QVERIFY(image.runtimeFrameIndex() < image.frameCount());
  QVERIFY(image.runtimeFrameValid());
  image.setRuntimeAnimate(true);
  QVERIFY(image.runtimeAnimating() || image.frameCount() == 1);
  image.setRuntimeFrameValid(false);
  QVERIFY(!image.runtimeFrameValid());
  QVERIFY(!image.runtimeAnimating());
}

void TestPlotGraphicRuntimes::cartesianExportsCsvSddsAndFailures()
{
  CartesianPlotElement plot;
  plot.setTraceXChannel(0, QStringLiteral("plot:x"));
  plot.setTraceYChannel(0, QStringLiteral("plot:y"));
  plot.updateTraceRuntimeData(0,
      {QPointF(1.25, 2.5), QPointF(3.75, 4.0)});
  QString error;
  const QByteArray csv = serializeCartesianPlotData(
      plot, PlotDataExportFormat::kCsv, &error);
  QVERIFY2(!csv.isEmpty(), qPrintable(error));
  QVERIFY(csv.startsWith("Index"));
  QVERIFY(csv.contains("plot_x"));
  QVERIFY(csv.contains("plot_y"));
  QVERIFY(csv.contains("0,1.25,2.5\n"));

  const QByteArray sdds = serializeCartesianPlotData(
      plot, PlotDataExportFormat::kSdds, &error);
  QVERIFY(sdds.startsWith("SDDS1\n"));
  QVERIFY(sdds.contains("&data mode=ascii &end\n2\n"));
  QVERIFY(sdds.contains("1 3.75 4\n"));

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString outputPath = directory.filePath(QStringLiteral("plot.csv"));
  QVERIFY(writeCartesianPlotData(
      plot, outputPath, PlotDataExportFormat::kCsv, &error));
  QFile output(outputPath);
  QVERIFY(output.open(QIODevice::ReadOnly));
  QCOMPARE(output.readAll(), csv);
  QVERIFY(!writeCartesianPlotData(plot, directory.path(),
      PlotDataExportFormat::kCsv, &error));
  QVERIFY(error.contains(QStringLiteral("Failed to open")));

  CartesianPlotElement empty;
  QVERIFY(serializeCartesianPlotData(empty,
      PlotDataExportFormat::kCsv, &error).isEmpty());
  QVERIFY(error.contains(QStringLiteral("no connected traces")));
}

void TestPlotGraphicRuntimes::
    rendersWidgetsToExplicitPathsAndReportsFailures()
{
  QWidget widget;
  widget.resize(137, 83);
  widget.setStyleSheet(QStringLiteral("background: #123456;"));
  widget.show();
  QCoreApplication::processEvents();

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QString error;
  const QString pngPath = directory.filePath(QStringLiteral("nested/widget.png"));
  QVERIFY2(renderWidgetImageToPath(&widget, pngPath,
      QStringLiteral("Raster Test"), QStringLiteral("PNG dimensions"),
      &error), qPrintable(error));
  const QImage png(pngPath);
  QVERIFY(!png.isNull());
  QCOMPARE(png.size(), widget.size());

  const QString svgPath = directory.filePath(QStringLiteral("widget.svg"));
  QVERIFY2(renderWidgetImageToPath(&widget, svgPath,
      QStringLiteral("Vector Test"), QStringLiteral("SVG dimensions"),
      &error), qPrintable(error));
  QFile svg(svgPath);
  QVERIFY(svg.open(QIODevice::ReadOnly));
  const QByteArray svgContents = svg.readAll();
  QVERIFY(svgContents.contains("<svg"));
  QVERIFY(svgContents.contains("Raster Test") == false);
  QVERIFY(svgContents.contains("Vector Test"));

  QVERIFY(!renderWidgetImageToPath(&widget,
      directory.filePath(QStringLiteral("unsupported.txt")), QString(),
      QString(), &error));
  QVERIFY(error.contains(QStringLiteral("Unsupported image extension")));
  QVERIFY(!renderWidgetImageToPath(nullptr, pngPath,
      QString(), QString(), &error));
  QVERIFY(error.contains(QStringLiteral("No widget")));

  const QString blockerPath = directory.filePath(QStringLiteral("blocker"));
  QFile blocker(blockerPath);
  QVERIFY(blocker.open(QIODevice::WriteOnly));
  QCOMPARE(blocker.write("not a directory"), qint64(15));
  blocker.close();
  QVERIFY(!renderWidgetImageToPath(&widget,
      blockerPath + QStringLiteral("/child.png"), QString(), QString(),
      &error));
  QVERIFY(error.contains(QStringLiteral("Failed to create image directory")));
}

QTEST_MAIN(TestPlotGraphicRuntimes)

#include "test_plot_graphic_runtimes.moc"
