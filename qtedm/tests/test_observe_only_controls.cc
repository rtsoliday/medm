#include <QtTest/QtTest>

#include <cmath>

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

#include "audit_logger.h"
#include "display_state.h"
#include "display_window.h"
#include "extension_object_registry.h"
#include "led_monitor_element.h"
#include "main_window_controller.h"
#include "message_button_element.h"
#include "message_button_runtime.h"
#include "pv_channel_manager.h"
#include "setpoint_control_element.h"
#include "soft_pv_registry.h"
#include "window_utils.h"

class TestObserveOnlyControls : public QObject
{
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void registryContainsSafetyControlObjects();
  void observeOnlyBlocksEverySoftPvWriteKind();
  void toggleWritesAlternatingValuesThroughSoftPv();
  void toggleMatchesNormalizedEnumAndStringValues();
  void oversizedWindowGetsRealOnScreenGeometry();
  void oversizedForcedExecuteDisplayStaysFitted();
  void modeSwitchReactivatesActiveDisplay();
  void forcedExecuteDisplayActivatesSafetyControls();
  void reopenedExecuteDisplayDeliversMessageButtonReleaseBeforeMove();
  void extensionWidgetPropertiesRoundTripInMemory();

private:
  QStringList registeredNames_;
};

void TestObserveOnlyControls::init()
{
  PvChannelManager::instance().setObserveOnly(false);
  AuditLogger::instance().initialize(false);
}

void TestObserveOnlyControls::cleanup()
{
  auto &soft = SoftPvRegistry::instance();
  for (const QString &name : registeredNames_) {
    soft.setConnected(name, false);
    soft.unregisterName(name, true);
  }
  registeredNames_.clear();
  PvChannelManager::instance().setObserveOnly(false);
  AuditLogger::instance().shutdown();
  AuditLogger::instance().initialize(false);
  qunsetenv("QTEDM_AUDIT_DIR");
}

void TestObserveOnlyControls::registryContainsSafetyControlObjects()
{
  auto &registry = ExtensionObjectRegistry::instance();
  const auto *symbol = registry.descriptor(QStringLiteral("qtedm_symbol"));
  const auto *toggle = registry.descriptor(CreateTool::kQtedmToggle);
  const auto *spinbox = registry.descriptor(QStringLiteral("qtedm_spinbox"));

  QVERIFY(symbol);
  QCOMPARE(symbol->createTool, CreateTool::kQtedmSymbol);
  QVERIFY(toggle);
  QCOMPARE(toggle->typeId, QStringLiteral("qtedm_toggle"));
  QVERIFY(spinbox);
  QCOMPARE(spinbox->createTool, CreateTool::kQtedmSpinBox);
  QVERIFY(registry.descriptors().size() >= 3);
}

void TestObserveOnlyControls::observeOnlyBlocksEverySoftPvWriteKind()
{
  auto &soft = SoftPvRegistry::instance();
  auto registerWritable = [this, &soft](const QString &name) {
    soft.registerName(name, true);
    soft.setConnected(name, true);
    registeredNames_.append(name);
  };

  const QString numericName = QStringLiteral("__test:readonly_numeric");
  const QString stringName = QStringLiteral("__test:readonly_string");
  const QString enumName = QStringLiteral("__test:readonly_enum");
  const QString charName = QStringLiteral("__test:readonly_char");
  const QString arrayName = QStringLiteral("__test:readonly_array");
  for (const QString &name :
       {numericName, stringName, enumName, charName, arrayName}) {
    registerWritable(name);
  }

  soft.publishValue(numericName, 1.0);
  soft.publishStringValue(stringName, QStringLiteral("before"));
  soft.publishEnumValue(enumName, 0, {QStringLiteral("Off"),
                                     QStringLiteral("On")});
  soft.publishCharArrayValue(charName, QByteArray("old", 3));
  soft.publishArrayValue(arrayName, {1.0, 2.0});

  auto &manager = PvChannelManager::instance();
  QVERIFY(manager.putValue(numericName, 2.0));
  QVERIFY(manager.putValue(stringName, QStringLiteral("normal")));
  QVERIFY(manager.putValue(enumName, static_cast<dbr_enum_t>(1)));
  QVERIFY(manager.putCharArrayValue(charName, QByteArray("normal", 6)));
  QVERIFY(manager.putArrayValue(arrayName, {3.0, 4.0}));

  manager.setObserveOnly(true);
  bool observedCanWrite = true;
  SubscriptionHandle subscription = manager.subscribe(numericName,
      DBR_TIME_DOUBLE, 1, [](const SharedChannelData &) {}, nullptr,
      [&observedCanWrite](bool, bool canWrite) {
        observedCanWrite = canWrite;
      });
  QVERIFY(subscription.isValid());
  QVERIFY(!observedCanWrite);

  QTemporaryDir auditDirectory;
  QVERIFY(auditDirectory.isValid());
  qputenv("QTEDM_AUDIT_DIR", auditDirectory.path().toLocal8Bit());
  AuditLogger::instance().initialize(true);
  QVERIFY(!manager.putValue(numericName, 99.0));
  QVERIFY(!manager.putValue(stringName, QStringLiteral("blocked")));
  QVERIFY(!manager.putValue(enumName, static_cast<dbr_enum_t>(0)));
  QVERIFY(!manager.putCharArrayValue(charName, QByteArray("blocked", 7)));
  QVERIFY(!manager.putArrayValue(arrayName, {9.0, 9.0}));
  QVERIFY(!manager.putValue(QStringLiteral("ca:__test:readonly_transport"),
      1.0));
  QVERIFY(!manager.putValue(QStringLiteral(
      "pva://__test:readonly_transport"), 1.0));

  SoftPvInfoSnapshot snapshot;
  QVERIFY(soft.infoSnapshot(numericName, snapshot));
  QCOMPARE(snapshot.value, 2.0);
  QVERIFY(soft.infoSnapshot(stringName, snapshot));
  QCOMPARE(snapshot.stringValue, QStringLiteral("normal"));
  QVERIFY(soft.infoSnapshot(enumName, snapshot));
  QCOMPARE(snapshot.enumValue, static_cast<dbr_enum_t>(1));
  QVERIFY(soft.infoSnapshot(charName, snapshot));
  QCOMPARE(snapshot.charArrayValue, QByteArray("normal", 6));
  QVERIFY(soft.infoSnapshot(arrayName, snapshot));
  QCOMPARE(snapshot.arrayValues, QVector<double>({3.0, 4.0}));

  const QDir auditDir(auditDirectory.path());
  const QStringList logs = auditDir.entryList(
      {QStringLiteral("audit_*.log")}, QDir::Files, QDir::Time);
  QVERIFY(!logs.isEmpty());
  QFile logFile(auditDir.filePath(logs.first()));
  QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray audit = logFile.readAll();
  QCOMPARE(audit.count("BLOCKED:ObserveOnly"), 7);
  QVERIFY(audit.contains("ca:__test:readonly_transport"));
  QVERIFY(audit.contains("pva://__test:readonly_transport"));
  logFile.close();
  AuditLogger::instance().shutdown();
}

void TestObserveOnlyControls::toggleWritesAlternatingValuesThroughSoftPv()
{
  const QString channelName = QStringLiteral("__test:toggle_interaction");
  auto &soft = SoftPvRegistry::instance();
  soft.registerName(channelName, true);
  registeredNames_.append(channelName);
  soft.setConnected(channelName, true);
  soft.setControlInfo(channelName, 0.0, 1.0, 0);
  soft.publishValue(channelName, 0.0);

  MessageButtonElement toggle;
  toggle.resize(180, 44);
  toggle.setQtedmToggle(true);
  toggle.setChannel(channelName);
  toggle.setOffValue(QStringLiteral("0"));
  toggle.setOnValue(QStringLiteral("1"));
  toggle.setOffLabel(QStringLiteral("Disabled"));
  toggle.setOnLabel(QStringLiteral("Enabled"));
  toggle.setExecuteMode(true);

  MessageButtonRuntime runtime(&toggle);
  runtime.start();
  toggle.show();
  QCoreApplication::processEvents();

  auto *button = toggle.findChild<QPushButton *>();
  QVERIFY(button);
  QTRY_VERIFY(button->isEnabled());
  QTRY_COMPARE(button->text(), QStringLiteral("Disabled"));
  QVERIFY(!button->isChecked());

  QTest::mouseClick(button, Qt::LeftButton);
  SoftPvInfoSnapshot snapshot;
  QVERIFY(soft.infoSnapshot(channelName, snapshot));
  QCOMPARE(snapshot.value, 1.0);
  QTRY_COMPARE(button->text(), QStringLiteral("Enabled"));
  QVERIFY(button->isChecked());

  QTest::mouseClick(button, Qt::LeftButton);
  QVERIFY(soft.infoSnapshot(channelName, snapshot));
  QCOMPARE(snapshot.value, 0.0);
  QTRY_COMPARE(button->text(), QStringLiteral("Disabled"));
  QVERIFY(!button->isChecked());
}

void TestObserveOnlyControls::toggleMatchesNormalizedEnumAndStringValues()
{
  auto &soft = SoftPvRegistry::instance();

  const QString enumName = QStringLiteral("__test:toggle_enum_matching");
  soft.registerName(enumName, true);
  registeredNames_.append(enumName);
  soft.setConnected(enumName, true);
  soft.publishEnumValue(enumName, 1,
      {QStringLiteral("Off"), QStringLiteral("On")});

  MessageButtonElement enumToggle;
  enumToggle.setQtedmToggle(true);
  enumToggle.setChannel(enumName);
  enumToggle.setOffValue(QStringLiteral("0"));
  enumToggle.setOnValue(QStringLiteral("0.6"));
  enumToggle.setOffLabel(QStringLiteral("Off"));
  enumToggle.setOnLabel(QStringLiteral("On"));
  enumToggle.setExecuteMode(true);
  MessageButtonRuntime enumRuntime(&enumToggle);
  enumRuntime.start();
  enumToggle.show();
  auto *enumButton = enumToggle.findChild<QPushButton *>();
  QVERIFY(enumButton);
  QTRY_VERIFY(enumButton->isChecked());
  QTest::mouseClick(enumButton, Qt::LeftButton);
  SoftPvInfoSnapshot snapshot;
  QVERIFY(soft.infoSnapshot(enumName, snapshot));
  QCOMPARE(snapshot.enumValue, static_cast<dbr_enum_t>(0));
  QTRY_VERIFY(!enumButton->isChecked());

  const QString stringName =
      QStringLiteral("__test:toggle_string_matching");
  soft.registerName(stringName, true);
  registeredNames_.append(stringName);
  soft.setConnected(stringName, true);
  soft.publishStringValue(stringName, QStringLiteral("RUNNING"));

  MessageButtonElement stringToggle;
  stringToggle.setQtedmToggle(true);
  stringToggle.setChannel(stringName);
  stringToggle.setOffValue(QStringLiteral("  STOPPED  "));
  stringToggle.setOnValue(QStringLiteral("  RUNNING  "));
  stringToggle.setExecuteMode(true);
  MessageButtonRuntime stringRuntime(&stringToggle);
  stringRuntime.start();
  stringToggle.show();
  auto *stringButton = stringToggle.findChild<QPushButton *>();
  QVERIFY(stringButton);
  QTRY_VERIFY(stringButton->isChecked());
  QTest::mouseClick(stringButton, Qt::LeftButton);
  QVERIFY(soft.infoSnapshot(stringName, snapshot));
  QCOMPARE(snapshot.stringValue, QStringLiteral("STOPPED"));
  QTRY_VERIFY(!stringButton->isChecked());
}

void TestObserveOnlyControls::oversizedWindowGetsRealOnScreenGeometry()
{
  QScreen *screen = QGuiApplication::primaryScreen();
  QVERIFY(screen);
  const QSize available = screen->availableGeometry().size();
  QVERIFY(available.isValid());

  QWidget window;
  const QSize oversized(available.width() * 2, available.height() + 1);
  window.resize(oversized);
  QVERIFY(fitWindowToAvailableScreen(&window));
  QVERIFY(window.width() <= available.width());
  QVERIFY(window.height() <= available.height());
  QVERIFY(window.size() != oversized);

  const qreal originalAspect =
      static_cast<qreal>(oversized.width()) / oversized.height();
  const qreal fittedAspect =
      static_cast<qreal>(window.width()) / window.height();
  QVERIFY(std::abs(originalAspect - fittedAspect) < 0.01);
  QVERIFY(!fitWindowToAvailableScreen(&window));

  /* Reproduce Cocoa's initial state for an oversized display: the native
   * top-level window is already clamped, while its display canvas still
   * reports the larger ADL dimensions. */
  window.resize(available);
  QVERIFY(fitWindowToAvailableScreen(&window, oversized));
  QVERIFY(window.width() <= available.width());
  QVERIFY(window.height() <= available.height());
  const qreal canvasFittedAspect =
      static_cast<qreal>(window.width()) / window.height();
  QVERIFY(std::abs(originalAspect - canvasFittedAspect) < 0.01);
}

void TestObserveOnlyControls::oversizedForcedExecuteDisplayStaysFitted()
{
#if !defined(Q_OS_MAC)
  QSKIP("Cocoa oversized-window compositor behavior is macOS-specific");
#else
  if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
    QSKIP("Requires Qt's native Cocoa platform");
  }
  QScreen *screen = QGuiApplication::primaryScreen();
  QVERIFY(screen);
  const QSize available = screen->availableGeometry().size();
  if (available.width() >= 1560 && available.height() >= 1230) {
    QSKIP("The test screen is large enough for the unscaled demo");
  }

  const QString fixture =
      QFINDTESTDATA("../resources/demo/QtEDM_Demo.adl");
  QVERIFY2(!fixture.isEmpty(), "QtEDM demo ADL fixture was not found");

  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  const QPalette palette = QApplication::palette();
  const QFont font = QApplication::font();
  DisplayWindow window(palette, palette, font, font,
      std::weak_ptr<DisplayState>(state));
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  QString error;
  QVERIFY2(window.loadFromFile(fixture, &error), qPrintable(error));

  window.show();
  window.enterExecuteMode();
  QTest::qWait(150);

  QWidget *displayArea =
      window.findChild<QWidget *>(QStringLiteral("displayArea"));
  QVERIFY(displayArea);
  QVERIFY(window.width() <= available.width());
  QVERIFY(window.height() <= available.height());
  QVERIFY(displayArea->width() <= available.width());
  QVERIFY(displayArea->height() <= available.height());

  MessageButtonElement *toggle = nullptr;
  SetpointControlElement *spinbox = nullptr;
  for (QWidget *widget : window.findChildren<QWidget *>()) {
    if (auto *candidate = dynamic_cast<MessageButtonElement *>(widget);
        candidate && candidate->isQtedmToggle()) {
      toggle = candidate;
    }
    if (auto *candidate = dynamic_cast<SetpointControlElement *>(widget);
        candidate && candidate->isQtedmSpinBox()) {
      spinbox = candidate;
    }
  }
  QVERIFY(toggle);
  QVERIFY(spinbox);
  QVERIFY(displayArea->rect().contains(toggle->geometry()));
  QVERIFY(displayArea->rect().contains(spinbox->geometry()));

  window.close();
#endif
}

void TestObserveOnlyControls::modeSwitchReactivatesActiveDisplay()
{
#if !defined(Q_OS_MAC)
  QSKIP("Inactive-window first-click behavior is specific to macOS");
#else
  auto state = std::make_shared<DisplayState>();
  QMainWindow controlWindow;
  const QPalette palette = QApplication::palette();
  const QFont font = QApplication::font();
  DisplayWindow displayWindow(palette, palette, font, font,
      std::weak_ptr<DisplayState>(state));
  controlWindow.resize(240, 120);
  displayWindow.resize(320, 180);
  controlWindow.show();
  displayWindow.show();
  state->displays.append(&displayWindow);
  state->activeDisplay = &displayWindow;

  MainWindowController controller(&controlWindow,
      std::weak_ptr<DisplayState>(state));
  controlWindow.raise();
  controlWindow.activateWindow();
  QTRY_VERIFY(controlWindow.isActiveWindow());

  controller.reactivateActiveDisplayAfterModeChange();
  QTRY_VERIFY(displayWindow.isActiveWindow());
#endif
}

void TestObserveOnlyControls::forcedExecuteDisplayActivatesSafetyControls()
{
  auto &soft = SoftPvRegistry::instance();
  auto registerNumeric = [this, &soft](const QString &name, double value,
                             double low, double high, short precision) {
    soft.registerName(name, true);
    registeredNames_.append(name);
    soft.setConnected(name, true);
    soft.setControlInfo(name, low, high, precision);
    soft.publishValue(name, value);
  };
  registerNumeric(QStringLiteral("led:test:discrete"), 2.0, 0.0, 10.0, 0);
  registerNumeric(QStringLiteral("led:test:binary_live"), 0.0, 0.0, 1.0, 0);
  registerNumeric(QStringLiteral("sp:test:compact:setpoint"), 42.0,
      -10.0, 100.0, 2);

  const QString fixture =
      QFINDTESTDATA("../../tests/test_QtEDMSafetyControls.adl");
  QVERIFY2(!fixture.isEmpty(), "Safety-controls ADL fixture was not found");

  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  const QPalette palette = QApplication::palette();
  const QFont font = QApplication::font();
  DisplayWindow window(palette, palette, font, font,
      std::weak_ptr<DisplayState>(state));
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  QString error;
  QVERIFY2(window.loadFromFile(fixture, &error), qPrintable(error));

  /* Match the built-in demo path: show the display while the shared
   * application remains in Edit, then force only this display to execute. */
  window.show();
  window.enterExecuteMode();
  QCoreApplication::processEvents();

  MessageButtonElement *toggle = nullptr;
  SetpointControlElement *spinbox = nullptr;
  LedMonitorElement *symbol = nullptr;
  for (QWidget *widget : window.findChildren<QWidget *>()) {
    if (auto *candidate = dynamic_cast<MessageButtonElement *>(widget);
        candidate && candidate->isQtedmToggle()) {
      toggle = candidate;
    }
    if (auto *candidate = dynamic_cast<SetpointControlElement *>(widget);
        candidate && candidate->isQtedmSpinBox()) {
      spinbox = candidate;
    }
    if (auto *candidate = dynamic_cast<LedMonitorElement *>(widget);
        candidate && candidate->isQtedmSymbol()) {
      symbol = candidate;
    }
  }

  QVERIFY(toggle);
  QVERIFY(spinbox);
  QVERIFY(symbol);
  QVERIFY(state->editMode);
  QVERIFY(toggle->isExecuteMode());
  QVERIFY(spinbox->isExecuteMode());
  QVERIFY(symbol->isExecuteMode());

  auto *toggleButton = toggle->findChild<QPushButton *>();
  auto *incrementButton = spinbox->findChild<QToolButton *>(
      QStringLiteral("qtedmSpinBoxIncrementButton"));
  QVERIFY(toggleButton);
  QVERIFY(incrementButton);
  QTRY_VERIFY(toggleButton->isEnabled());
  QTRY_VERIFY(incrementButton->isEnabled());

  QWidget *toggleHit = QApplication::widgetAt(
      toggleButton->mapToGlobal(toggleButton->rect().center()));
  QCOMPARE(toggleHit, static_cast<QWidget *>(toggleButton));
  auto acceptToggleConfirmation = [] {
    QTimer::singleShot(0, [] {
      if (auto *messageBox =
              qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        if (QAbstractButton *yesButton =
                messageBox->button(QMessageBox::Yes)) {
          yesButton->click();
        }
      }
    });
  };
  acceptToggleConfirmation();
  QVERIFY(window.windowHandle());
  QTest::mouseClick(window.windowHandle(), Qt::LeftButton,
      Qt::NoModifier,
      window.mapFromGlobal(toggleButton->mapToGlobal(
          toggleButton->rect().center())));
  SoftPvInfoSnapshot snapshot;
  QVERIFY(soft.infoSnapshot(QStringLiteral("led:test:binary_live"), snapshot));
  QCOMPARE(snapshot.value, 1.0);
  QTRY_COMPARE(toggleButton->text(), QStringLiteral("Enabled"));

  QWidget *incrementHit = QApplication::widgetAt(
      incrementButton->mapToGlobal(incrementButton->rect().center()));
  QCOMPARE(incrementHit, static_cast<QWidget *>(incrementButton));
  QTest::mouseClick(window.windowHandle(), Qt::LeftButton,
      Qt::NoModifier,
      window.mapFromGlobal(incrementButton->mapToGlobal(
          incrementButton->rect().center())));
  QVERIFY(soft.infoSnapshot(QStringLiteral("sp:test:compact:setpoint"),
      snapshot));
  QCOMPARE(snapshot.value, 42.25);
  QTRY_COMPARE(spinbox->runtimeSetpointText(), QStringLiteral("42.25"));

  state->editMode = false;
  window.handleEditModeChanged(false);
  state->editMode = true;
  window.handleEditModeChanged(true);
  state->editMode = false;
  window.handleEditModeChanged(false);
  QCoreApplication::processEvents();
  QTRY_VERIFY(toggleButton->isEnabled());
  QTRY_VERIFY(incrementButton->isEnabled());

  acceptToggleConfirmation();
  QTest::mouseClick(window.windowHandle(), Qt::LeftButton,
      Qt::NoModifier,
      window.mapFromGlobal(toggleButton->mapToGlobal(
          toggleButton->rect().center())));
  QVERIFY(soft.infoSnapshot(QStringLiteral("led:test:binary_live"), snapshot));
  QCOMPARE(snapshot.value, 0.0);
  QTRY_COMPARE(toggleButton->text(), QStringLiteral("Disabled"));

  QTest::mouseClick(window.windowHandle(), Qt::LeftButton,
      Qt::NoModifier,
      window.mapFromGlobal(incrementButton->mapToGlobal(
          incrementButton->rect().center())));
  QVERIFY(soft.infoSnapshot(QStringLiteral("sp:test:compact:setpoint"),
      snapshot));
  QCOMPARE(snapshot.value, 42.5);
  QTRY_COMPARE(spinbox->runtimeSetpointText(), QStringLiteral("42.50"));
}

void TestObserveOnlyControls::
    reopenedExecuteDisplayDeliversMessageButtonReleaseBeforeMove()
{
  const QString channelName =
      QStringLiteral("__test:reopened_message_button");
  auto &soft = SoftPvRegistry::instance();
  soft.registerName(channelName, true);
  registeredNames_.append(channelName);
  soft.setConnected(channelName, true);
  soft.setControlInfo(channelName, 0.0, 1.0, 0);
  soft.publishValue(channelName, 0.0);

  QTemporaryDir fixtureDirectory;
  QVERIFY(fixtureDirectory.isValid());
  const QString fixturePath =
      fixtureDirectory.filePath(QStringLiteral("reopened_message_button.adl"));
  QFile fixture(fixturePath);
  QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Text));
  const QByteArray fixtureContents = QByteArrayLiteral(
      "file {\n"
      "  name=\"reopened_message_button.adl\"\n"
      "  version=030122\n"
      "}\n"
      "display {\n"
      "  object { x=100 y=100 width=320 height=180 }\n"
      "  clr=14\n"
      "  bclr=4\n"
      "  cmap=\"\"\n"
      "  gridSpacing=5\n"
      "  gridOn=0\n"
      "  snapToGrid=0\n"
      "}\n"
      "\"message button\" {\n"
      "  object { x=70 y=60 width=180 height=44 }\n"
      "  control {\n"
      "    chan=\"__test:reopened_message_button\"\n"
      "    clr=14\n"
      "    bclr=4\n"
      "  }\n"
      "  label=\"Press\"\n"
      "  press_msg=\"1\"\n"
      "  release_msg=\"0\"\n"
      "}\n");
  QCOMPARE(fixture.write(fixtureContents), fixtureContents.size());
  fixture.close();

  auto state = std::make_shared<DisplayState>();
  state->editMode = false;
  const QPalette palette = QApplication::palette();
  const QFont font = QApplication::font();

  auto exerciseOpenDisplay = [&]() {
    DisplayWindow window(palette, palette, font, font,
        std::weak_ptr<DisplayState>(state));
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    QString error;
    QVERIFY2(window.loadFromFile(fixturePath, &error), qPrintable(error));
    state->displays.append(&window);

    window.show();
    activateWindowWhenExposed(&window);
    QPointer<DisplayWindow> delayedDisplay(&window);
    runWhenWindowExposed(&window, [delayedDisplay]() {
      if (delayedDisplay) {
        delayedDisplay->handleEditModeChanged(false);
      }
    });

    QTRY_VERIFY(window.windowHandle());
    QTRY_VERIFY(window.windowHandle()->isExposed());
#if defined(Q_OS_MAC)
    QTRY_VERIFY(window.isActiveWindow());
#endif

    MessageButtonElement *message = nullptr;
    for (QWidget *widget : window.findChildren<QWidget *>()) {
      if (auto *candidate = dynamic_cast<MessageButtonElement *>(widget);
          candidate && !candidate->isQtedmToggle()) {
        message = candidate;
        break;
      }
    }
    QVERIFY(message);
    auto *button = message->findChild<QPushButton *>();
    QVERIFY(button);
    QTRY_VERIFY(message->isExecuteMode());
    QTRY_VERIFY(button->isEnabled());

    const QPoint originalPosition = window.pos();
    const QPoint clickPosition = window.mapFromGlobal(
        button->mapToGlobal(button->rect().center()));
    QSignalSpy pressedSpy(button, &QPushButton::pressed);
    QSignalSpy releasedSpy(button, &QPushButton::released);

    QTest::mousePress(window.windowHandle(), Qt::LeftButton,
        Qt::NoModifier, clickPosition);
    QTRY_COMPARE(pressedSpy.count(), 1);
    SoftPvInfoSnapshot snapshot;
    QVERIFY(soft.infoSnapshot(channelName, snapshot));
    QCOMPARE(snapshot.value, 1.0);

    QTest::mouseRelease(window.windowHandle(), Qt::LeftButton,
        Qt::NoModifier, clickPosition);
    QTRY_COMPARE(releasedSpy.count(), 1);
    QVERIFY(soft.infoSnapshot(channelName, snapshot));
    QCOMPARE(snapshot.value, 0.0);
    QCOMPARE(window.pos(), originalPosition);

    state->displays.removeAll(&window);
    window.close();
    QCoreApplication::processEvents();
  };

  exerciseOpenDisplay();
  exerciseOpenDisplay();
}

void TestObserveOnlyControls::extensionWidgetPropertiesRoundTripInMemory()
{
  LedMonitorElement symbol;
  symbol.setQtedmSymbol(true);
  LedMonitorElement::SymbolState state;
  state.minimum = -1.5;
  state.maximum = 2.5;
  state.color = QColor(QStringLiteral("#123456"));
  state.label = QStringLiteral("Mapped");
  state.imagePath = QStringLiteral("state.png");
  symbol.setSymbolStates({state});
  QVERIFY(symbol.isQtedmSymbol());
  QCOMPARE(symbol.symbolStates().first().label, QStringLiteral("Mapped"));
  QCOMPARE(symbol.symbolStates().first().minimum, -1.5);

  MessageButtonElement toggle;
  toggle.setQtedmToggle(true);
  toggle.setOffValue(QStringLiteral("-4"));
  toggle.setOnValue(QStringLiteral("7"));
  toggle.setOffLabel(QStringLiteral("Stopped"));
  toggle.setOnLabel(QStringLiteral("Running"));
  toggle.setConfirmationRequired(true);
  QVERIFY(toggle.isQtedmToggle());
  QCOMPARE(toggle.offValue(), QStringLiteral("-4"));
  QCOMPARE(toggle.onLabel(), QStringLiteral("Running"));
  QVERIFY(toggle.confirmationRequired());
  toggle.setExecuteMode(true);
  toggle.setRuntimeConnected(true);
  toggle.setRuntimeWriteAccess(false);
  toggle.setRuntimeToggleState(true, true);
  auto *toggleButton = toggle.findChild<QPushButton *>();
  QVERIFY(toggleButton);
  QVERIFY(!toggleButton->isEnabled());
  QVERIFY(toggleButton->isChecked());
  toggle.setRuntimeWriteAccess(true);
  QVERIFY(toggleButton->isEnabled());
  toggleButton->click();
  QVERIFY(toggleButton->isChecked());

  SetpointControlElement spinbox;
  spinbox.setQtedmSpinBox(true);
  spinbox.setStepSize(0.25);
  spinbox.setPrecision(3);
  PvLimits limits = spinbox.limits();
  limits.lowSource = PvLimitSource::kDefault;
  limits.highSource = PvLimitSource::kDefault;
  limits.lowDefault = -10.0;
  limits.highDefault = 10.0;
  spinbox.setLimits(limits);
  QVERIFY(spinbox.isQtedmSpinBox());
  QCOMPARE(spinbox.stepSize(), 0.25);
  QCOMPARE(spinbox.precision(), 3);
  QCOMPARE(spinbox.displayLowLimit(), -10.0);
  QCOMPARE(spinbox.displayHighLimit(), 10.0);
  spinbox.setSetpointChannel(QStringLiteral("__test:spinbox"));
  spinbox.setActivationCallback([](const QString &) {});
  spinbox.setExecuteMode(true);
  spinbox.setSetpointConnected(true);
  spinbox.setSetpointWriteAccess(false);
  const QList<QToolButton *> stepButtons =
      spinbox.findChildren<QToolButton *>();
  QCOMPARE(stepButtons.size(), 2);
  auto *decrementButton = spinbox.findChild<QToolButton *>(
      QStringLiteral("qtedmSpinBoxDecrementButton"));
  auto *incrementButton = spinbox.findChild<QToolButton *>(
      QStringLiteral("qtedmSpinBoxIncrementButton"));
  QVERIFY(decrementButton);
  QVERIFY(incrementButton);
  QCOMPARE(decrementButton->text(), QStringLiteral("-"));
  QCOMPARE(incrementButton->text(), QStringLiteral("+"));
  QCOMPARE(decrementButton->accessibleName(),
      QStringLiteral("Decrease value"));
  QCOMPARE(incrementButton->accessibleName(),
      QStringLiteral("Increase value"));
  for (QToolButton *button : stepButtons) {
    QVERIFY(!button->isEnabled());
  }
  spinbox.setSetpointWriteAccess(true);
  for (QToolButton *button : stepButtons) {
    QVERIFY(button->isEnabled());
  }
}

QTEST_MAIN(TestObserveOnlyControls)

#include "test_observe_only_controls.moc"
