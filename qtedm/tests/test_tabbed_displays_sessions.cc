#include <QtTest/QtTest>

#include <QFile>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QTableWidget>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QStackedWidget>
#include <QTabBar>
#include <QTemporaryDir>

#include "extension_object_registry.h"
#include "session_manager.h"
#include "tabbed_display_element.h"

class TestTabbedDisplaysSessions : public QObject
{
  Q_OBJECT

private slots:
  void registryContainsTabbedDisplay();
  void pageEditorPreservesValuesAndSkipsUnchanged();
  void tabbedDisplayLoadsLazily();
  void tabbedDisplayRebuildsSelectedPage();
  void tabbedDisplayReportsDiagnostics();
  void tabbedDisplayNormalizesIdsAndSupportsStackedMode();
  void tabbedDisplayRoutesEditModeMouseEventsToContainer();
  void sessionRoundTrip();
  void sessionRejectsUnsafeNamesAndSchemas();
  void sessionBoundsFileAndWindowCount();
  void sessionInvalidWindowDataReportsWarnings();
  void sessionGeometryIsClamped();
};

void TestTabbedDisplaysSessions::registryContainsTabbedDisplay()
{
  const auto *descriptor = ExtensionObjectRegistry::instance().descriptor(
      QStringLiteral("qtedm_tabbed_display"));
  QVERIFY(descriptor);
  QCOMPARE(descriptor->createTool, CreateTool::kQtedmTabbedDisplay);
  QCOMPARE(descriptor->category, QStringLiteral("Containers"));
}

void TestTabbedDisplaysSessions::pageEditorPreservesValuesAndSkipsUnchanged()
{
  TabbedDisplayElement element;
  const QList<TabbedDisplayPage> original = {
      {QStringLiteral("one"), QStringLiteral("  Padded  "),
          QStringLiteral("  child.adl  "), QStringLiteral("  A=1  "), true},
      {QStringLiteral("two"), QStringLiteral("Second"), {}, {}, false},
  };
  element.setPages(original);
  element.setActivePageId(QStringLiteral("two"));
  int changes = 0;
  element.setChangedCallback([&] { ++changes; });
  for (int attempt = 0; attempt < 4; ++attempt) {
    bool inspected = false;
    QTimer::singleShot(0, &element, [&] {
      auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      const auto close = qScopeGuard([&] {
        if (dialog->isVisible()) dialog->reject();
      });
      auto *table = dialog->findChild<QTableWidget *>();
      auto *buttons = dialog->findChild<QDialogButtonBox *>();
      QVERIFY(table && buttons);
      QCOMPARE(table->item(0, 1)->text(), original.first().label);
      QCOMPARE(table->item(0, 2)->text(), original.first().displayPath);
      QCOMPARE(table->item(0, 3)->text(), original.first().macros);
      if (attempt == 1 || attempt == 2) {
        table->setCurrentCell(1, 1);
        table->editItem(table->item(1, 1));
        auto *editor = table->findChild<QLineEdit *>();
        QVERIFY(editor);
        editor->setText(QStringLiteral("  Edited  "));
      }
      if (attempt == 3) {
        dialog->findChild<QCheckBox *>()->setChecked(true);
      }
      buttons->button(attempt == 1 ? QDialogButtonBox::Cancel
                                  : QDialogButtonBox::Ok)->click();
      inspected = true;
    });
    element.showPageEditor();
    QVERIFY(inspected);
    QCOMPARE(changes, attempt < 2 ? 0 : attempt - 1);
    const auto pages = element.pages();
    QCOMPARE(pages.first().label, original.first().label);
    QCOMPARE(pages.first().displayPath, original.first().displayPath);
    QCOMPARE(pages.first().macros, original.first().macros);
    QCOMPARE(pages.first().keepAlive, original.first().keepAlive);
    QCOMPARE(pages.last().label, attempt < 2 ? original.last().label
                                          : QStringLiteral("  Edited  "));
    QCOMPARE(element.activePageId(), QStringLiteral("two"));
  }
  QVERIFY(element.hiddenTabs());
}

void TestTabbedDisplaysSessions::tabbedDisplayLoadsLazily()
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

void TestTabbedDisplaysSessions::tabbedDisplayRebuildsSelectedPage()
{
  TabbedDisplayElement element;
  QList<TabbedDisplayPage> pages = {
      {QStringLiteral("one"), QStringLiteral("One"),
          QStringLiteral("one.adl"), {}, false},
      {QStringLiteral("two"), QStringLiteral("Two"),
          QStringLiteral("two.adl"), {}, true},
      {QStringLiteral("three"), QStringLiteral("Three"),
          QStringLiteral("three.adl"), {}, false},
  };
  element.setPages(pages);
  QList<QPointer<QWidget>> contents;
  element.setPageLoader([&](const TabbedDisplayPage &, QWidget *host,
      QString *) -> QWidget * {
    auto *content = new QWidget(host);
    contents.append(content);
    return content;
  });
  element.setExecuteMode(true);
  element.setActivePageId(QStringLiteral("two"));
  element.setActivePageId(QStringLiteral("three"));
  QCOMPARE(element.loadedPageCount(), 2);
  element.setExecuteMode(false);
  for (const auto &content : contents) {
    QVERIFY(content.isNull());
  }
  /* Accepting the editor with the third tab selected used to dereference
   * deleted hosts from the tab bar's synchronous currentChanged signal. */
  element.setPages(pages);
  QCOMPARE(element.activePageId(), QStringLiteral("three"));
  QCOMPARE(element.loadedPageCount(), 0);
  element.setExecuteMode(true);
  QCOMPARE(element.loadedPageCount(), 1);
  const QPointer<QWidget> oldContent = contents.last();
  element.setPages(pages);
  QVERIFY(oldContent.isNull());
  QCOMPARE(element.activePageId(), QStringLiteral("three"));
  QCOMPARE(element.loadedPageCount(), 1);
  pages.removeLast();
  element.setPages(pages);
  QCOMPARE(element.activePageId(), QStringLiteral("one"));
  QCOMPARE(element.loadedPageCount(), 1);
  element.setPages({});
  QCOMPARE(element.activePageId(), QStringLiteral("page-1"));
  QCOMPARE(element.loadedPageCount(), 0);
}

void TestTabbedDisplaysSessions::tabbedDisplayReportsDiagnostics()
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

void TestTabbedDisplaysSessions::tabbedDisplayNormalizesIdsAndSupportsStackedMode()
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

void TestTabbedDisplaysSessions::
    tabbedDisplayRoutesEditModeMouseEventsToContainer()
{
  TabbedDisplayElement element;
  auto *tabBar = element.findChild<QTabBar *>();
  auto *stack = element.findChild<QStackedWidget *>();
  QVERIFY(tabBar);
  QVERIFY(stack);

  QVERIFY(tabBar->testAttribute(Qt::WA_TransparentForMouseEvents));
  QVERIFY(stack->testAttribute(Qt::WA_TransparentForMouseEvents));

  element.setExecuteMode(true);
  QVERIFY(!tabBar->testAttribute(Qt::WA_TransparentForMouseEvents));
  QVERIFY(!stack->testAttribute(Qt::WA_TransparentForMouseEvents));

  element.setExecuteMode(false);
  QVERIFY(tabBar->testAttribute(Qt::WA_TransparentForMouseEvents));
  QVERIFY(stack->testAttribute(Qt::WA_TransparentForMouseEvents));
}

void TestTabbedDisplaysSessions::sessionRoundTrip()
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

void TestTabbedDisplaysSessions::sessionRejectsUnsafeNamesAndSchemas()
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

  QtEdmSession invalidSession;
  invalidSession.name = QStringLiteral("invalid-window");
  QtEdmSessionWindow invalidWindow;
  invalidWindow.geometry = QRect(0, 0, 640, 480);
  invalidSession.windows.append(invalidWindow);
  QString error;
  QVERIFY(!manager.save(invalidSession, &error));
  QVERIFY(error.contains(QStringLiteral("display path")));
  QVERIFY(!QFileInfo::exists(manager.sessionPath(invalidSession.name)));
}

void TestTabbedDisplaysSessions::sessionBoundsFileAndWindowCount()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  SessionManager manager(directory.path());

  QtEdmSession session;
  session.name = QStringLiteral("too-many");
  QtEdmSessionWindow window;
  window.displayPath = QStringLiteral("main.adl");
  window.geometry = QRect(0, 0, 640, 480);
  for (int index = 0; index < 101; ++index) session.windows.append(window);
  QString error;
  QVERIFY(!manager.save(session, &error));
  QVERIFY(error.contains(QStringLiteral("100 window")));

  QFile oversized(manager.sessionPath(QStringLiteral("oversized")));
  QVERIFY(oversized.open(QIODevice::WriteOnly));
  QVERIFY(oversized.resize(4LL * 1024 * 1024 + 1));
  oversized.close();
  const auto oversizedResult = manager.load(QStringLiteral("oversized"));
  QVERIFY(!oversizedResult.ok());
  QVERIFY(oversizedResult.error.contains(QStringLiteral("byte limit")));

  QJsonArray windows;
  for (int index = 0; index < 101; ++index) {
    windows.append(QJsonObject{
        {QStringLiteral("displayPath"), QStringLiteral("main.adl")}});
  }
  QFile many(manager.sessionPath(QStringLiteral("many")));
  QVERIFY(many.open(QIODevice::WriteOnly));
  QVERIFY(many.write(QJsonDocument(QJsonObject{
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("windows"), windows}}).toJson()) > 0);
  many.close();
  const auto manyResult = manager.load(QStringLiteral("many"));
  QVERIFY(!manyResult.ok());
  QVERIFY(manyResult.error.contains(QStringLiteral("100 window")));
}

void TestTabbedDisplaysSessions::sessionInvalidWindowDataReportsWarnings()
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

void TestTabbedDisplaysSessions::sessionGeometryIsClamped()
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

QTEST_MAIN(TestTabbedDisplaysSessions)

#include "test_tabbed_displays_sessions.moc"
