#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QWheelEvent>

#include <cmath>
#include <limits>

#include "cartesian_plot_element.h"
#include "heatmap_element.h"
#include "image_element.h"
#include "plot_export_utils.h"
#include "pva_bridge.h"
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
  void zoomedAxisTicksSitWhereTheirValuesArePlotted();
  void hugeAxisRangesKeepFiniteTicks();
  void logAxisPanAndZoomStayDrawable();
  void horizontalWheelDoesNotZoom();
  void pvaKeepsDisplayLimitsAsSentForPlots();
  void heatmapTracksDimensionsRangeAndTransforms();
  void heatmapBoundsSyntheticPreviewForLargeConfiguredGrids();
  void dynamicGraphicAppliesRuntimeVisibilityAndSeverity();
  void imageTracksRuntimeFrameSelection();
  void uncachedGifSeeksAnyFrame();
  void cartesianExportsCsvSddsAndFailures();
  void cartesianExportReadsWithSdds();
  void rendersWidgetsToExplicitPathsAndReportsFailures();
};

/* MEDM's safeFloat() keeps SciPlot from hanging on values of FLT_MAX and
 * beyond. An axis autoscaled over a sentinel such as DBL_MAX must keep a
 * finite range and tick count: rounding 1.797e308 up to whole ticks
 * overflowed to infinity, and casting the infinite tick count to int was
 * undefined, INT_MIN on x86-64 (no ticks, every point drawn at the bottom)
 * and INT_MAX on arm64 (tick loops that never finish). */
void TestPlotGraphicRuntimes::hugeAxisRangesKeepFiniteTicks()
{
  const double big = std::numeric_limits<double>::max();
  struct Case
  {
    double minimum;
    double maximum;
    bool log;
  };
  const Case cases[] = {
      {0.0, big, false},
      {-big, big, false},
      {-big, 0.0, false},
      {0.0, std::numeric_limits<double>::denorm_min(), false},
      {1e-300, big, true},
      {std::numeric_limits<double>::denorm_min(), big, true},
  };
  for (const Case &axis : cases) {
    for (bool xAxis : {true, false}) {
      const auto nice = CartesianPlotElement::computeNiceAxisRange(
          axis.minimum, axis.maximum, axis.log, xAxis);
      const QByteArray label = QStringLiteral("%1..%2 log=%3 x=%4")
          .arg(axis.minimum).arg(axis.maximum).arg(axis.log).arg(xAxis)
          .toLatin1();
      QVERIFY2(std::isfinite(nice.drawMin) && std::isfinite(nice.drawMax),
          label.constData());
      QVERIFY2(nice.drawMax > nice.drawMin, label.constData());
      QVERIFY2(std::isfinite(nice.majorInc) && nice.majorInc > 0.0,
          label.constData());
      QVERIFY2(nice.numMajor >= 0 && nice.numMajor <= 1000,
          label.constData());
    }
  }

  /* Values far below the sentinel stay inside the axis. */
  const auto sentinel =
      CartesianPlotElement::computeNiceAxisRange(0.0, big, false, false);
  QVERIFY(sentinel.numMajor > 0);
  QCOMPARE(sentinel.drawMin, 0.0);
  QVERIFY(10.0 < sentinel.drawMax);

  /* Ordinary ranges are unchanged. */
  const auto ordinary =
      CartesianPlotElement::computeNiceAxisRange(0.0, 10.0, false, false);
  QCOMPARE(ordinary.drawMin, 0.0);
  QCOMPARE(ordinary.drawMax, 10.0);
  QCOMPARE(ordinary.majorInc, 2.0);
  QCOMPARE(ordinary.numMajor, 5);
}

/* A log axis is drawn in decades, so it must pan and zoom in decades.
 * Linear arithmetic moved the minimum of a 1e-3 to 1e3 axis below 0 with a
 * one-pixel drag, and every point vanished. A click that wobbles a pixel
 * must not freeze the axes as zoomed, and no zoom step may leave an axis
 * without a drawable window. */
void TestPlotGraphicRuntimes::logAxisPanAndZoomStayDrawable()
{
  CartesianPlotElement plot;
  plot.resize(400, 300);
  plot.setTraceYChannel(0, QStringLiteral("plot:log"));
  plot.setTraceYAxis(0, CartesianPlotYAxis::kY1);
  plot.setAxisStyle(1, CartesianPlotAxisStyle::kLog10);
  plot.setExecuteMode(true);
  plot.show();
  QCoreApplication::processEvents();
  plot.cachedAxisRanges_[0].minimum = 0.0;
  plot.cachedAxisRanges_[0].maximum = 10.0;
  plot.cachedAxisRanges_[0].valid = true;
  plot.cachedAxisRanges_[1].minimum = 1e-3;
  plot.cachedAxisRanges_[1].maximum = 1e3;
  plot.cachedAxisRanges_[1].valid = true;
  plot.cachedAxisRanges_[1].style = CartesianPlotAxisStyle::kLog10;
  plot.cachedAxisRangesValid_ = true;
  const QRectF chart = plot.chartRect();
  QVERIFY(chart.width() > 50.0 && chart.height() > 50.0);
  const QPoint start = chart.center().toPoint();
  /* The plot does not grab the mouse, so deliver the motion to it. */
  auto drag = [&plot, start](const QPoint &offset) {
    QTest::mousePress(&plot, Qt::LeftButton, Qt::NoModifier, start);
    const QPoint end = start + offset;
    QMouseEvent move(QEvent::MouseMove, QPointF(end),
        QPointF(plot.mapToGlobal(end)), Qt::NoButton, Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&plot, &move);
    QTest::mouseRelease(&plot, Qt::LeftButton, Qt::NoModifier, end);
  };

  drag(QPoint(0, -1));
  QVERIFY(!plot.isZoomed());

  const int pixels = qRound(chart.height() / 5.0);
  drag(QPoint(0, -pixels));
  QVERIFY(plot.isZoomed());
  double low = plot.zoomMinimums_[1];
  double high = plot.zoomMaximums_[1];
  QVERIFY(low > 0.0);
  QVERIFY(std::abs(std::log10(high / low) - 6.0) < 1e-9);
  QVERIFY(std::abs((std::log10(low) + 3.0)
      + 6.0 * pixels / chart.height()) < 1e-9);

  /* Wheel zoom keeps the decade under the pointer and stays above 0. */
  const QPointF center = chart.center();
  const double centerDecade = (std::log10(low) + std::log10(high)) / 2.0;
  auto wheel = [&](int delta, int notches) {
    for (int i = 0; i < notches; ++i) {
      QWheelEvent event(center, plot.mapToGlobal(center.toPoint()),
          QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
          Qt::NoScrollPhase, false);
      QApplication::sendEvent(&plot, &event);
    }
  };
  wheel(-120, 30);
  low = plot.zoomMinimums_[1];
  high = plot.zoomMaximums_[1];
  QVERIFY(low > 0.0 && std::isfinite(high) && high > low);
  QVERIFY(std::abs((std::log10(low) + std::log10(high)) / 2.0
      - centerDecade) < 1e-6);
  wheel(120, 1000);
  for (int axis : {0, 1}) {
    QVERIFY(std::isfinite(plot.zoomMinimums_[axis]));
    QVERIFY(std::isfinite(plot.zoomMaximums_[axis]));
    QVERIFY(plot.zoomMaximums_[axis] > plot.zoomMinimums_[axis]);
  }
  QVERIFY(plot.zoomMinimums_[1] > 0.0);
}

/* A horizontal wheel or tilt has no vertical delta. It must not leave the
 * plot zoomed with its window unchanged, which stopped the axes from
 * following the data; a vertical notch still zooms. */
void TestPlotGraphicRuntimes::horizontalWheelDoesNotZoom()
{
  CartesianPlotElement plot;
  plot.resize(400, 300);
  plot.setTraceYChannel(0, QStringLiteral("plot:wheel"));
  plot.setExecuteMode(true);
  plot.show();
  QCoreApplication::processEvents();
  for (int axis : {0, 1}) {
    plot.cachedAxisRanges_[axis].minimum = 0.0;
    plot.cachedAxisRanges_[axis].maximum = 10.0;
    plot.cachedAxisRanges_[axis].valid = true;
  }
  plot.cachedAxisRangesValid_ = true;
  const QPointF center = plot.chartRect().center();
  auto wheel = [&plot, center](const QPoint &delta) {
    QWheelEvent event(center, plot.mapToGlobal(center.toPoint()),
        QPoint(), delta, Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QApplication::sendEvent(&plot, &event);
  };
  wheel(QPoint(120, 0));
  QVERIFY(!plot.isZoomed());
  wheel(QPoint(0, 120));
  QVERIFY(plot.isZoomed());
}

/* A zoomed axis draws its data against the zoom window, so each tick must
 * sit where its value is plotted. Spreading the rounded range, 10 to 90
 * for a window of 13 to 87, over the axis put the label 20 at 12.5% of
 * the axis instead of 9.5%. */
void TestPlotGraphicRuntimes::zoomedAxisTicksSitWhereTheirValuesArePlotted()
{
  CartesianPlotElement::AxisRange zoomed;
  zoomed.minimum = 13.0;
  zoomed.maximum = 87.0;
  zoomed.valid = true;
  zoomed.zoomed = true;
  const auto ticks = CartesianPlotElement::axisTicks(zoomed, true);
  QCOMPARE(ticks.axisMin, 13.0);
  QCOMPARE(ticks.axisMax, 87.0);
  QVERIFY(ticks.drawMin < 13.0);
  int shown = 0;
  for (int i = 0; i <= ticks.numMajor; ++i) {
    const double value = ticks.drawMin + i * ticks.majorInc;
    const double fraction =
        CartesianPlotElement::tickFraction(ticks, value, false);
    if (value < 13.0 || value > 87.0) {
      QCOMPARE(fraction, -1.0);
      continue;
    }
    ++shown;
    QVERIFY2(std::abs(fraction - (value - 13.0) / 74.0) < 1e-12,
        qPrintable(QString::number(value)));
  }
  QVERIFY(shown >= 3);

  /* An unzoomed range is already rounded; its ticks span the axis. */
  CartesianPlotElement::AxisRange rounded;
  rounded.minimum = 10.0;
  rounded.maximum = 90.0;
  rounded.valid = true;
  rounded.majorInc = 20.0;
  rounded.numMajor = 4;
  rounded.numMinor = 4;
  const auto full = CartesianPlotElement::axisTicks(rounded, true);
  QCOMPARE(full.axisMin, 10.0);
  QCOMPARE(full.axisMax, 90.0);
  QCOMPARE(CartesianPlotElement::tickFraction(full, 10.0, false), 0.0);
  QCOMPARE(CartesianPlotElement::tickFraction(full, 50.0, false), 0.5);
  QCOMPARE(CartesianPlotElement::tickFraction(full, 90.0, false), 1.0);

  /* A zoomed log axis labels the decades inside its window. */
  CartesianPlotElement::AxisRange zoomedLog;
  zoomedLog.minimum = 2.0;
  zoomedLog.maximum = 50.0;
  zoomedLog.valid = true;
  zoomedLog.style = CartesianPlotAxisStyle::kLog10;
  zoomedLog.zoomed = true;
  const auto decades = CartesianPlotElement::axisTicks(zoomedLog, false);
  QCOMPARE(CartesianPlotElement::tickFraction(decades, 1.0, true), -1.0);
  QCOMPARE(CartesianPlotElement::tickFraction(decades, 100.0, true), -1.0);
  QVERIFY(std::abs(CartesianPlotElement::tickFraction(decades, 10.0, true)
      - (1.0 - std::log10(2.0)) / (std::log10(50.0) - std::log10(2.0)))
      < 1e-12);
}

/* A PVA record without display limits sends 0 and 0. Widgets range over
 * ordered limits that are never equal, 0 to 1 here, but a plot must see
 * the limits as sent, as for Channel Access, to autoscale instead of
 * clipping data far outside 0 to 1. */
void TestPlotGraphicRuntimes::pvaKeepsDisplayLimitsAsSentForPlots()
{
  PvaBridgeData data;
  pvaBridgeSetLimits(&data, 0.0, 0.0);
  QVERIFY(data.hasControlInfo);
  QCOMPARE(data.lopr, 0.0);
  QCOMPARE(data.hopr, 1.0);
  QCOMPARE(data.displayLow, 0.0);
  QCOMPARE(data.displayHigh, 0.0);

  pvaBridgeSetLimits(&data, 10.0, -5.0);
  QCOMPARE(data.lopr, -5.0);
  QCOMPARE(data.hopr, 10.0);
  QCOMPARE(data.displayLow, 10.0);
  QCOMPARE(data.displayHigh, -5.0);
}

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

void TestPlotGraphicRuntimes::heatmapBoundsSyntheticPreviewForLargeConfiguredGrids()
{
  HeatmapElement heatmap;
  heatmap.resize(320, 220);
  heatmap.setXDimension(50000);
  heatmap.setYDimension(40000);
  heatmap.setShowTopProfile(true);
  heatmap.setShowRightProfile(true);
  const QSize configuredDimensions(50000, 40000);
  QCOMPARE(heatmap.effectiveDimensions(), configuredDimensions);
  /* Check the bound before rendering so a regression cannot allocate the
   * 16 GB synthetic array represented by this otherwise valid grid. */
  QCOMPARE(heatmap.renderDataDimensions(), QSize(32, 24));
  QVERIFY(!renderWidget(&heatmap).isNull());
  QVERIFY(!heatmap.cachedImage_.isNull());
  QVERIFY(heatmap.cachedImage_.width() <= 32);
  QVERIFY(heatmap.cachedImage_.height() <= 24);
  QCOMPARE(heatmap.topProfileData_.size(), 32);
  QCOMPARE(heatmap.rightProfileData_.size(), 24);
  QCOMPARE(heatmap.xDimension(), configuredDimensions.width());
  QCOMPARE(heatmap.yDimension(), configuredDimensions.height());
  QCOMPARE(heatmap.effectiveDimensions(), configuredDimensions);

  heatmap.setExecuteMode(true);
  heatmap.setRuntimeConnected(true);
  heatmap.setRuntimeData({1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  QCOMPARE(heatmap.renderDataDimensions(), configuredDimensions);
  QCOMPARE(heatmap.runtimeDataCount(), 6);
  QVERIFY(!renderWidget(&heatmap).isNull());
  QCOMPARE(heatmap.runtimeMinimum(), 1.0);
  QCOMPARE(heatmap.runtimeMaximum(), 6.0);
  QCOMPARE(heatmap.effectiveDimensions(), configuredDimensions);
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
  auto *movie = image.findChild<QMovie *>();
  QVERIFY(movie);
  QCOMPARE(movie->cacheMode(), QMovie::CacheAll);
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

void TestPlotGraphicRuntimes::uncachedGifSeeksAnyFrame()
{
  /* GIFs too large for the frame cache play without CacheAll. Image calc
   * frame selection must still reach any frame, including earlier ones. */
  QMovie movie(QDir(QCoreApplication::applicationDirPath())
      .filePath(QStringLiteral("../../tests/test_frames.gif")));
  QVERIFY(movie.isValid());
  movie.setCacheMode(QMovie::CacheNone);
  QCOMPARE(movie.frameCount(), 5);
  movie.start();
  movie.setPaused(true);
  const QList<QColor> colors{QColor(Qt::red), QColor(Qt::green),
      QColor(Qt::blue), QColor(Qt::yellow), QColor(Qt::cyan)};
  for (const int frame : {3, 1, 4, 0, 2}) {
    QVERIFY2(seekMovieFrame(&movie, frame), qPrintable(QString::number(frame)));
    QCOMPARE(movie.currentFrameNumber(), frame);
    QCOMPARE(movie.currentPixmap().toImage().pixelColor(0, 0),
        colors.at(frame));
  }
  QVERIFY(!seekMovieFrame(&movie, colors.size()));
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

void TestPlotGraphicRuntimes::cartesianExportReadsWithSdds()
{
  const QString reader = qEnvironmentVariable("QTEDM_TEST_SDDS2STREAM");
  if (reader.isEmpty()) {
    QSKIP("Optional SDDS reader check: run make test-sdds-export.");
  }

  CartesianPlotElement plot;
  plot.setTraceXChannel(0, QStringLiteral("plot:x"));
  plot.setTraceYChannel(0, QStringLiteral("plot:y"));
  plot.updateTraceRuntimeData(0, {QPointF(1.25, -10.5),
      QPointF(3.75, std::numeric_limits<double>::quiet_NaN())});

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("plot.sdds"));
  QString error;
  QVERIFY2(writeCartesianPlotData(plot, path,
      PlotDataExportFormat::kSdds, &error), qPrintable(error));

  /* Exercise the independent reader and its numeric presentation. A newer
   * toolkit's ENUM support must not change the meaning of numeric exports. */
  QProcess process;
  process.start(reader, {path,
      QStringLiteral("-columns=Index,plot_x,plot_y"),
      QStringLiteral("-ignoreFormats")});
  QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
  QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
  const QByteArray diagnostic = process.readAllStandardError();
  QCOMPARE(process.exitStatus(), QProcess::NormalExit);
  QVERIFY2(process.exitCode() == 0, diagnostic.constData());
  const QStringList values = QString::fromUtf8(process.readAllStandardOutput())
      .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  QCOMPARE(values.size(), 6);
  const double expected[] = {0.0, 1.25, -10.5, 1.0, 3.75};
  for (int index = 0; index < 5; ++index) {
    bool ok = false;
    const double value = values.at(index).toDouble(&ok);
    QVERIFY2(ok, qPrintable(values.at(index)));
    QCOMPARE(value, expected[index]);
  }
  QCOMPARE(values.at(5).toLower(), QStringLiteral("nan"));
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
