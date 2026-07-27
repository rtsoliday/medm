#include <QtTest/QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTemporaryDir>

#include "extension_object_registry.h"
#include "session_manager.h"
#include "tabbed_display_element.h"

class TestPhase2 : public QObject
{
  Q_OBJECT

private slots:
  void registryContainsTabbedDisplay();
  void tabbedDisplayLoadsLazily();
  void tabbedDisplayReportsDiagnostics();
  void tabbedDisplayNormalizesIdsAndSupportsStackedMode();
  void sessionRoundTrip();
  void sessionRejectsUnsafeNamesAndSchemas();
  void sessionInvalidWindowDataReportsWarnings();
  void sessionGeometryIsClamped();
};

void TestPhase2::registryContainsTabbedDisplay()
{
  const auto *descriptor = ExtensionObjectRegistry::instance().descriptor(
      QStringLiteral("qtedm_tabbed_display"));
  QVERIFY(descriptor);
  QCOMPARE(descriptor->createTool, CreateTool::kQtedmTabbedDisplay);
  QCOMPARE(descriptor->category, QStringLiteral("Containers"));
}

void TestPhase2::tabbedDisplayLoadsLazily()
{
  TabbedDisplayElement element;
  element.setPages({
      {QStringLiteral("one"), QStringLiteral("One"),
          QStringLiteral("one.adl"), QStringLiteral("P=parent"), false},
      {QStringLiteral("two"), QStringLiteral("Two"),
          QStringLiteral("two.adl"), QStringLiteral("P=child"), true},
  });
  int loadCount = 0;
  QStringList loadedPaths;
  element.setPageLoader(
      [&loadCount, &loadedPaths](const TabbedDisplayPage &page,
          QWidget *host, QString *) -> QWidget * {
        ++loadCount;
        loadedPaths.append(page.displayPath);
        return new QWidget(host);
      });

  element.setExecuteMode(true);
  QCOMPARE(loadCount, 1);
  QCOMPARE(element.loadedPageCount(), 1);
  QCOMPARE(loadedPaths, QStringList{QStringLiteral("one.adl")});

  element.setActivePageId(QStringLiteral("two"));
  QCOMPARE(loadCount, 2);
  QCOMPARE(element.loadedPageCount(), 1);
  QCOMPARE(element.activePageId(), QStringLiteral("two"));

  element.setActivePageId(QStringLiteral("one"));
  QCOMPARE(loadCount, 3);
  QCOMPARE(element.loadedPageCount(), 2);

  element.setExecuteMode(false);
  QCOMPARE(element.loadedPageCount(), 0);
}

void TestPhase2::tabbedDisplayReportsDiagnostics()
{
  TabbedDisplayElement element;
  element.setPages({
      {QStringLiteral("missing"), QStringLiteral("Missing"),
          QStringLiteral("missing.adl"), QString(), false},
  });
  element.setPageLoader([](const TabbedDisplayPage &, QWidget *,
      QString *error) -> QWidget * {
    *error = QStringLiteral("Child display not found");
    return nullptr;
  });

  element.setExecuteMode(true);
  QCOMPARE(element.loadedPageCount(), 0);
  QCOMPARE(element.pageDiagnostic(0),
      QStringLiteral("Child display not found"));
}

void TestPhase2::tabbedDisplayNormalizesIdsAndSupportsStackedMode()
{
  TabbedDisplayElement element;
  element.setPages({
      {QString(), QStringLiteral("First"), QString(), QString(), false},
      {QStringLiteral("duplicate"), QStringLiteral("Second"),
          QString(), QString(), false},
      {QStringLiteral("duplicate"), QStringLiteral("Third"),
          QString(), QString(), false},
  });

  const QList<TabbedDisplayPage> pages = element.pages();
  QCOMPARE(pages.size(), 3);
  QVERIFY(!pages.at(0).id.isEmpty());
  QVERIFY(pages.at(1).id != pages.at(2).id);
  QVERIFY(element.setActivePageId(pages.at(2).id));
  QCOMPARE(element.activePageId(), pages.at(2).id);

  element.setHiddenTabs(true);
  QVERIFY(element.hiddenTabs());
  element.setHiddenTabs(false);
  QVERIFY(!element.hiddenTabs());
}

void TestPhase2::sessionRoundTrip()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionManager manager(directory.path());

  QtEdmSession session;
  session.name = QStringLiteral("operator layout");
  QtEdmSessionWindow window;
  window.displayPath = QStringLiteral("/tmp/main.adl");
  window.macros = {
      {QStringLiteral("P"), QStringLiteral("TEST:")},
      {QStringLiteral("AREA"), QStringLiteral("INJ")},
  };
  window.geometry = QRect(10, 20, 640, 480);
  window.screenName = QStringLiteral("Primary");
  window.activeTabId = QStringLiteral("vacuum");
  window.editMode = true;
  session.windows.append(window);

  QString error;
  QVERIFY2(manager.save(session, &error), qPrintable(error));
  QCOMPARE(manager.sessionNames(),
      QStringList{QStringLiteral("operator layout")});

  const QtEdmSessionLoadResult loaded =
      manager.load(QStringLiteral("operator layout"));
  QVERIFY2(loaded.ok(), qPrintable(loaded.error));
  QCOMPARE(loaded.session.windows.size(), 1);
  const QtEdmSessionWindow restored = loaded.session.windows.first();
  QCOMPARE(restored.displayPath, window.displayPath);
  QCOMPARE(restored.macros, window.macros);
  QCOMPARE(restored.geometry, window.geometry);
  QCOMPARE(restored.screenName, window.screenName);
  QCOMPARE(restored.activeTabId, window.activeTabId);
  QVERIFY(restored.editMode);
}

void TestPhase2::sessionRejectsUnsafeNamesAndSchemas()
{
  QVERIFY(!SessionManager::isValidSessionName(QStringLiteral("../escape")));
  QVERIFY(!SessionManager::isValidSessionName(QStringLiteral("a/b")));
  QVERIFY(SessionManager::isValidSessionName(QStringLiteral("night-shift.1")));

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionManager manager(directory.path());
  QFile file(directory.filePath(QStringLiteral(
      "future.qtedm-session.json")));
  QVERIFY(file.open(QIODevice::WriteOnly));
  QJsonObject root{
      {QStringLiteral("schemaVersion"), 99},
      {QStringLiteral("windows"), QJsonArray()},
  };
  QVERIFY(file.write(QJsonDocument(root).toJson()) > 0);
  file.close();

  const QtEdmSessionLoadResult loaded =
      manager.load(QStringLiteral("future"));
  QVERIFY(!loaded.ok());
  QVERIFY(loaded.error.contains(QStringLiteral("schema")));
}

void TestPhase2::sessionInvalidWindowDataReportsWarnings()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionManager manager(directory.path());
  QFile file(directory.filePath(QStringLiteral(
      "damaged.qtedm-session.json")));
  QVERIFY(file.open(QIODevice::WriteOnly));
  QJsonObject damagedWindow{
      {QStringLiteral("displayPath"), QStringLiteral("/tmp/main.adl")},
      {QStringLiteral("macros"), QStringLiteral("not-an-object")},
      {QStringLiteral("geometry"), QJsonObject{
          {QStringLiteral("x"), 10},
          {QStringLiteral("y"), 20},
          {QStringLiteral("width"), -1},
          {QStringLiteral("height"), 480},
      }},
      {QStringLiteral("mode"), QStringLiteral("unexpected")},
  };
  QJsonObject missingPathWindow{
      {QStringLiteral("geometry"), QJsonObject{
          {QStringLiteral("x"), 0},
          {QStringLiteral("y"), 0},
          {QStringLiteral("width"), 640},
          {QStringLiteral("height"), 480},
      }},
  };
  QJsonObject root{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("windows"),
          QJsonArray{damagedWindow, missingPathWindow}},
  };
  QVERIFY(file.write(QJsonDocument(root).toJson()) > 0);
  file.close();

  const QtEdmSessionLoadResult loaded =
      manager.load(QStringLiteral("damaged"));
  QVERIFY2(loaded.ok(), qPrintable(loaded.error));
  QCOMPARE(loaded.session.windows.size(), 1);
  QCOMPARE(loaded.session.windows.first().geometry, QRect(0, 0, 640, 480));
  QVERIFY(loaded.session.windows.first().macros.isEmpty());
  QVERIFY(!loaded.session.windows.first().editMode);
  QVERIFY(loaded.warnings.join(QLatin1Char('\n')).contains(
      QStringLiteral("invalid geometry")));
  QVERIFY(loaded.warnings.join(QLatin1Char('\n')).contains(
      QStringLiteral("invalid macros")));
  QVERIFY(loaded.warnings.join(QLatin1Char('\n')).contains(
      QStringLiteral("unknown mode")));
  QVERIFY(loaded.warnings.join(QLatin1Char('\n')).contains(
      QStringLiteral("no display path")));
}

void TestPhase2::sessionGeometryIsClamped()
{
  const QList<QPair<QString, QRect>> screens{
      qMakePair(QStringLiteral("left"), QRect(0, 0, 1920, 1080)),
      qMakePair(QStringLiteral("right"), QRect(1920, 0, 1280, 1024)),
  };

  QCOMPARE(SessionManager::clampGeometryToScreens(
      QRect(9000, -4000, 4000, 3000), QStringLiteral("right"), screens),
      QRect(1920, 0, 1280, 1024));
  QCOMPARE(SessionManager::clampGeometryToScreens(
      QRect(100, 100, 800, 600), QStringLiteral("left"), screens),
      QRect(100, 100, 800, 600));
  QCOMPARE(SessionManager::clampGeometryToScreens(
      QRect(0, 0, -1, -1), QStringLiteral("left"), screens),
      QRect(0, 0, 640, 480));
}

QTEST_MAIN(TestPhase2)

#include "test_phase2.moc"
