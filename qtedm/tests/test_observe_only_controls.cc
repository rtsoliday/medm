#include <QtTest/QtTest>

#include <cmath>

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

#include "audit_logger.h"
#include "composite_element.h"
#include "display_state.h"
#include "display_window.h"
#include "expression_channel_element.h"
#include "extension_object_registry.h"
#include "find_pv_dialog.h"
#include "heatmap_element.h"
#include "led_monitor_element.h"
#include "main_window_controller.h"
#include "message_button_element.h"
#include "message_button_runtime.h"
#include "meter_element.h"
#include "plugin_element.h"
#include "polyline_element.h"
#include "pv_channel_manager.h"
#include "pv_limits_dialog.h"
#include "pv_table_element.h"
#include "setpoint_control_element.h"
#include "slider_element.h"
#include "soft_pv_registry.h"
#include "tabbed_display_element.h"
#include "text_element.h"
#include "text_area_element.h"
#include "wave_table_element.h"
#include "waterfall_plot_element.h"
#include "wheel_switch_element.h"
#include "window_utils.h"

namespace {

QString pickPvInfoText(DisplayWindow *window, QWidget *target)
{
  if (!window || !target) {
    return QString();
  }
  QTest::mouseClick(target, Qt::LeftButton, Qt::NoModifier,
      target->rect().center());
  QCoreApplication::processEvents();
  auto *dialog = window->findChild<QDialog *>(
      QStringLiteral("qtedmPvInfoDialog"));
  auto *text = dialog ? dialog->findChild<QPlainTextEdit *>() : nullptr;
  return text ? text->toPlainText() : QString();
}

} // namespace


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
  void findPvIncludesEverySafetyControlFamily();
  void readOnlyPvInfoPickFindsSafetyControls();
  void embeddedDisplayTraversalKeepsSoftPvsLocal();
  void pvLimitsPickerRoutesEverySupportedControl();
  void middleButtonTooltipRoutesThroughChildControls();
  void editorOperationsCoverExtensionInventories();
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

  toggle.setConfirmationRequired(true);
  bool confirmationOpenedWhilePressed = false;
  QTimer::singleShot(10, &toggle, [&]() {
    auto *messageBox = qobject_cast<QMessageBox *>(
        QApplication::activeModalWidget());
    if (!messageBox) {
      return;
    }
    confirmationOpenedWhilePressed = button->isDown();
    messageBox->done(QMessageBox::No);
  });
  QTest::mousePress(button, Qt::LeftButton);
  if (!confirmationOpenedWhilePressed) {
    QTest::mouseRelease(button, Qt::LeftButton);
  }
  QVERIFY(!confirmationOpenedWhilePressed);
  QVERIFY(!button->isDown());
  QVERIFY(soft.infoSnapshot(channelName, snapshot));
  QCOMPARE(snapshot.value, 0.0);
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

void TestObserveOnlyControls::findPvIncludesEverySafetyControlFamily()
{
  const QString fixture =
      QFINDTESTDATA("../../tests/test_QtEDMSafetyControls.adl");
  QVERIFY2(!fixture.isEmpty(), "Safety-controls ADL fixture was not found");

  auto state = std::make_shared<DisplayState>();
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  QString error;
  QVERIFY2(window.loadFromFile(fixture, &error), qPrintable(error));
  state->displays.append(&window);
  state->activeDisplay = &window;

  FindPvDialog dialog(QApplication::palette(), QApplication::font(), state);
  auto *searchEdit = dialog.findChild<QLineEdit *>();
  auto *results = dialog.findChild<QListWidget *>();
  QVERIFY(searchEdit);
  QVERIFY(results);

  searchEdit->setText(QStringLiteral("led:test:discrete"));
  QVERIFY(QMetaObject::invokeMethod(&dialog, "handleSearchClicked",
      Qt::DirectConnection));
  QCOMPARE(results->count(), 1);
  QVERIFY2(results->item(0)->text().contains(
      QStringLiteral("Multi-State Symbol")),
      qPrintable(results->item(0)->text()));

  searchEdit->setText(QStringLiteral("led:test:binary_live"));
  QVERIFY(QMetaObject::invokeMethod(&dialog, "handleSearchClicked",
      Qt::DirectConnection));
  QCOMPARE(results->count(), 1);
  QVERIFY2(results->item(0)->text().contains(QStringLiteral("Toggle")),
      qPrintable(results->item(0)->text()));

  searchEdit->setText(QStringLiteral("sp:test:compact:setpoint"));
  QVERIFY(QMetaObject::invokeMethod(&dialog, "handleSearchClicked",
      Qt::DirectConnection));
  QCOMPARE(results->count(), 1);
  QVERIFY2(results->item(0)->text().contains(QStringLiteral("Spin Box")),
      qPrintable(results->item(0)->text()));

  state->displays.removeAll(&window);
  state->activeDisplay.clear();
}

void TestObserveOnlyControls::readOnlyPvInfoPickFindsSafetyControls()
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
  registerNumeric(QStringLiteral("mb:stat:01"), 0.0, 0.0, 1.0, 0);

  const QString fixture =
      QFINDTESTDATA("../../tests/test_QtEDMSafetyControls.adl");
  QVERIFY2(!fixture.isEmpty(), "Safety-controls ADL fixture was not found");

  PvChannelManager::instance().setObserveOnly(true);
  auto state = std::make_shared<DisplayState>();
  state->editMode = false;
  state->observeOnly = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  QString error;
  QVERIFY2(window.loadFromFile(fixture, &error), qPrintable(error));
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
    } else if (auto *candidate =
                   dynamic_cast<SetpointControlElement *>(widget);
               candidate && candidate->isQtedmSpinBox()) {
      spinbox = candidate;
    } else if (auto *candidate = dynamic_cast<LedMonitorElement *>(widget);
               candidate && candidate->isQtedmSymbol()) {
      symbol = candidate;
    }
  }
  QVERIFY(toggle);
  QVERIFY(spinbox);
  QVERIFY(symbol);

  auto *toggleButton = toggle->findChild<QPushButton *>();
  auto *incrementButton = spinbox->findChild<QToolButton *>(
      QStringLiteral("qtedmSpinBoxIncrementButton"));
  QVERIFY(toggleButton);
  QVERIFY(incrementButton);
  QTRY_VERIFY(!toggleButton->isEnabled());
  QTRY_VERIFY(!incrementButton->isEnabled());

  window.startPvInfoPickMode();
  QVERIFY(window.isPvInfoPickingActive());
  QString text = pickPvInfoText(&window, toggleButton);
  QVERIFY2(text.contains(QStringLiteral("Object: Toggle")),
      qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("led:test:binary_live")),
      qPrintable(text));

  window.startPvInfoPickMode();
  QVERIFY(window.isPvInfoPickingActive());
  text = pickPvInfoText(&window, incrementButton);
  QVERIFY2(text.contains(QStringLiteral("Object: Spin Box")),
      qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("sp:test:compact:setpoint")),
      qPrintable(text));

  window.startPvInfoPickMode();
  QVERIFY(window.isPvInfoPickingActive());
  text = pickPvInfoText(&window, symbol);
  QVERIFY2(text.contains(QStringLiteral("Object: Multi-State Symbol")),
      qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("led:test:discrete")),
      qPrintable(text));

  const QString messageFixture =
      QFINDTESTDATA("../../tests/test_MessageButton.adl");
  QVERIFY2(!messageFixture.isEmpty(),
      "Message Button ADL fixture was not found");
  DisplayWindow messageWindow(QApplication::palette(),
      QApplication::palette(), QApplication::font(), QApplication::font(),
      state);
  messageWindow.setAttribute(Qt::WA_DeleteOnClose, false);
  error.clear();
  QVERIFY2(messageWindow.loadFromFile(messageFixture, &error),
      qPrintable(error));
  messageWindow.show();
  messageWindow.enterExecuteMode();
  QCoreApplication::processEvents();

  MessageButtonElement *message = nullptr;
  for (QWidget *widget : messageWindow.findChildren<QWidget *>()) {
    if (auto *candidate = dynamic_cast<MessageButtonElement *>(widget);
        candidate && candidate->channel() == QStringLiteral("mb:stat:01")) {
      message = candidate;
      break;
    }
  }
  QVERIFY(message);
  auto *messageButton = message->findChild<QPushButton *>();
  QVERIFY(messageButton);
  QTRY_VERIFY(!messageButton->isEnabled());

  messageWindow.startPvInfoPickMode();
  QVERIFY(messageWindow.isPvInfoPickingActive());
  text = pickPvInfoText(&messageWindow, messageButton);
  QVERIFY2(text.contains(QStringLiteral("Object: Message Button")),
      qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("mb:stat:01")),
      qPrintable(text));

}

void TestObserveOnlyControls::embeddedDisplayTraversalKeepsSoftPvsLocal()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString pvName = QStringLiteral("__qtedm:embedded:expression");
  const QString childPath = directory.filePath(QStringLiteral("child.adl"));
  const QString parentPath = directory.filePath(QStringLiteral("parent.adl"));

  QFile childFile(childPath);
  QVERIFY(childFile.open(QIODevice::WriteOnly | QIODevice::Text));
  const QString childText = QStringLiteral(
      "file { name=\"child.adl\" version=040004 }\n"
      "display { object { x=0 y=0 width=240 height=160 } clr=1 bclr=0 }\n"
      "color map { ncolors=2 colors { ffffff, 000000, } }\n"
      "meter {\n"
      "  object { x=20 y=20 width=80 height=80 }\n"
      "  monitor { chan=\"%1\" clr=1 bclr=0 }\n"
      "  limits { loprDefault=0 hoprDefault=10 precDefault=1 }\n"
      "}\n"
      "expression_channel {\n"
      "  object { x=130 y=20 width=80 height=40 }\n"
      "  variable=\"%1\" calc=\"1\" initialValue=1\n"
      "}\n").arg(pvName);
  const QByteArray childAdl = childText.toUtf8();
  QCOMPARE(childFile.write(childAdl), qint64(childAdl.size()));
  childFile.close();

  QFile parentFile(parentPath);
  QVERIFY(parentFile.open(QIODevice::WriteOnly | QIODevice::Text));
  const QByteArray parentAdl =
      "file { name=\"parent.adl\" version=040004 }\n"
      "display { object { x=0 y=0 width=300 height=220 } clr=1 bclr=0 }\n"
      "color map { ncolors=2 colors { ffffff, 000000, } }\n"
      "qtedm_tabbed_display {\n"
      "  object { x=10 y=10 width=280 height=200 }\n"
      "  mode=\"tabs\" active_page=\"child\"\n"
      "  page { id=\"child\" label=\"Child\" display=\"child.adl\""
      " keepAlive=\"true\" }\n"
      "}\n";
  QCOMPARE(parentFile.write(parentAdl), qint64(parentAdl.size()));
  parentFile.close();

  auto state = std::make_shared<DisplayState>();
  state->editMode = false;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  QString error;
  QVERIFY2(window.loadFromFile(parentPath, &error), qPrintable(error));
  state->displays.append(&window);
  state->activeDisplay = &window;
  window.show();
  window.enterExecuteMode();
  QCoreApplication::processEvents();

  QCOMPARE(window.tabbedDisplayElements_.size(), 1);
  TabbedDisplayElement *tabbed = window.tabbedDisplayElements_.front();
  QVERIFY(tabbed);
  QTRY_VERIFY(tabbed->pageContent(0));
  QObject *childObject = tabbed->pageContent(0)
      ->property("_qtedmChildDisplay").value<QObject *>();
  auto *child = dynamic_cast<DisplayWindow *>(childObject);
  QVERIFY(child);
  QTRY_VERIFY(child->executeModeActive_);

  SoftPvInfoSnapshot snapshot;
  QTRY_VERIFY(SoftPvRegistry::instance().infoSnapshot(pvName, snapshot));
  QTRY_COMPARE(snapshot.producerCount, 1);
  QTRY_VERIFY(snapshot.subscriberCount >= 1);

  FindPvDialog dialog(QApplication::palette(), QApplication::font(), state);
  auto *searchEdit = dialog.findChild<QLineEdit *>();
  auto *results = dialog.findChild<QListWidget *>();
  QVERIFY(searchEdit);
  QVERIFY(results);
  searchEdit->setText(pvName);
  QVERIFY(QMetaObject::invokeMethod(&dialog, "handleSearchClicked",
      Qt::DirectConnection));
  QVERIFY(results->count() >= 2);

  QCOMPARE(child->meterElements_.size(), 1);
  MeterElement *meter = child->meterElements_.front();
  QVERIFY(meter);
  QTest::mousePress(meter, Qt::MiddleButton, Qt::NoModifier,
      meter->rect().center());
  QTRY_VERIFY(child->executeDragTooltipVisible_);
  QCOMPARE(child->executeDragTooltipText_, pvName);
  QTest::mouseRelease(meter, Qt::MiddleButton, Qt::NoModifier,
      meter->rect().center());
  QTRY_VERIFY(!child->executeDragTooltipVisible_);
  QVERIFY(!child->executeDragPending_);

  auto menuShown = std::make_shared<bool>(false);
  QTimer::singleShot(0, [menuShown]() {
    auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    if (!menu) {
      return;
    }
    *menuShown = menu->objectName()
        == QStringLiteral("executeModeContextMenu");
    menu->close();
  });
  QTest::mouseClick(meter, Qt::RightButton, Qt::NoModifier,
      meter->rect().center());
  QCoreApplication::processEvents();
  QVERIFY(*menuShown);

  window.retryChannelConnections();
  QCoreApplication::processEvents();
  QTRY_VERIFY(tabbed->pageContent(0));
  childObject = tabbed->pageContent(0)
      ->property("_qtedmChildDisplay").value<QObject *>();
  child = dynamic_cast<DisplayWindow *>(childObject);
  QVERIFY(child);
  QTRY_VERIFY(child->executeModeActive_);
  QTRY_VERIFY(SoftPvRegistry::instance().infoSnapshot(pvName, snapshot));
  QTRY_VERIFY(snapshot.subscriberCount >= 1);

  window.leaveExecuteMode();
  state->displays.removeAll(&window);
  state->activeDisplay.clear();
  QCoreApplication::processEvents();
}

void TestObserveOnlyControls::pvLimitsPickerRoutesEverySupportedControl()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = false;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  window.resize(640, 360);
  window.show();
  QCoreApplication::processEvents();

  auto *slider = new SliderElement(window.displayArea_);
  slider->setGeometry(20, 20, 180, 45);
  slider->setChannel(QStringLiteral("limits:test:slider"));
  slider->setActivationCallback([](double) {});
  slider->setExecuteMode(true);
  slider->setRuntimeConnected(true);
  slider->setRuntimeWriteAccess(true);
  window.sliderElements_.append(slider);
  window.ensureElementInStack(slider);
  slider->show();

  auto *wheel = new WheelSwitchElement(window.displayArea_);
  wheel->setGeometry(220, 20, 180, 45);
  wheel->setChannel(QStringLiteral("limits:test:wheel"));
  wheel->setActivationCallback([](double) {});
  wheel->setExecuteMode(true);
  wheel->setRuntimeConnected(true);
  wheel->setRuntimeWriteAccess(true);
  window.wheelSwitchElements_.append(wheel);
  window.ensureElementInStack(wheel);
  wheel->show();

  auto *textArea = new TextAreaElement(window.displayArea_);
  textArea->setGeometry(20, 90, 260, 100);
  textArea->setChannel(QStringLiteral("limits:test:text_area"));
  textArea->setActivationCallback([](const QByteArray &) {});
  textArea->setExecuteMode(true);
  textArea->setRuntimeConnected(true);
  textArea->setRuntimeWriteAccess(true);
  window.textAreaElements_.append(textArea);
  window.ensureElementInStack(textArea);
  textArea->show();

  auto *setpoint = new SetpointControlElement(window.displayArea_);
  setpoint->setGeometry(310, 90, 260, 100);
  setpoint->setQtedmSpinBox(true);
  setpoint->setSetpointChannel(QStringLiteral("limits:test:setpoint"));
  setpoint->setActivationCallback([](const QString &) {});
  setpoint->setExecuteMode(true);
  setpoint->setSetpointConnected(true);
  setpoint->setSetpointWriteAccess(true);
  window.setpointControlElements_.append(setpoint);
  window.ensureElementInStack(setpoint);
  setpoint->show();

  window.executeModeActive_ = true;
  window.displayArea_->setExecuteMode(true);
  QCoreApplication::processEvents();

  auto pickLimits = [&window](QWidget *target, const QString &channel) {
    window.startPvLimitsPickMode();
    if (!window.isPvLimitsPickingActive()) {
      return false;
    }
    QTest::mouseClick(target, Qt::LeftButton, Qt::NoModifier,
        target->rect().center());
    QCoreApplication::processEvents();
    PvLimitsDialog *dialog = window.pvLimitsDialog_.data();
    bool foundChannel = false;
    if (dialog && dialog->isVisible()) {
      for (QLabel *label : dialog->findChildren<QLabel *>()) {
        if (label && label->text() == channel) {
          foundChannel = true;
          break;
        }
      }
      dialog->hide();
    }
    return foundChannel && !window.isPvLimitsPickingActive();
  };

  QVERIFY(pickLimits(slider, slider->channel()));
  QVERIFY(pickLimits(wheel, wheel->channel()));
  auto *editor = textArea->findChild<QTextEdit *>();
  QVERIFY(editor);
  QVERIFY(pickLimits(editor->viewport(), textArea->channel()));
  auto *setpointEdit = setpoint->findChild<QLineEdit *>();
  QVERIFY(setpointEdit);
  QVERIFY(pickLimits(setpointEdit, setpoint->setpointChannel()));

  window.cancelPvLimitsPickMode();
  window.executeModeActive_ = false;
  window.displayArea_->setExecuteMode(false);
}

void TestObserveOnlyControls::middleButtonTooltipRoutesThroughChildControls()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = false;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  window.resize(680, 360);

  auto *setpoint = new SetpointControlElement(window.displayArea_);
  setpoint->setGeometry(20, 20, 260, 90);
  setpoint->setQtedmSpinBox(true);
  setpoint->setSetpointChannel(QStringLiteral("middle:test:setpoint"));
  setpoint->setReadbackChannel(QStringLiteral("middle:test:readback"));
  setpoint->setActivationCallback([](const QString &) {});
  window.setpointControlElements_.append(setpoint);
  window.ensureElementInStack(setpoint);
  setpoint->show();

  auto *pvTable = new PvTableElement(window.displayArea_);
  pvTable->setGeometry(310, 20, 330, 120);
  pvTable->setRows({{QStringLiteral("PV"),
      QStringLiteral("middle:test:pv_table")}});
  window.pvTableElements_.append(pvTable);
  window.ensureElementInStack(pvTable);
  pvTable->show();

  auto *waveTable = new WaveTableElement(window.displayArea_);
  waveTable->setGeometry(20, 150, 280, 150);
  waveTable->setChannel(QStringLiteral("middle:test:wave_table"));
  window.waveTableElements_.append(waveTable);
  window.ensureElementInStack(waveTable);
  waveTable->show();

  auto *plugin = new PluginElement(window.displayArea_);
  plugin->setGeometry(330, 170, 240, 80);
  auto *pluginButton = new QToolButton(plugin);
  pluginButton->setGeometry(plugin->rect());
  pluginButton->setText(QStringLiteral("Plugin Child"));
  pluginButton->show();
  window.pluginElements_.append(plugin);
  window.ensureElementInStack(plugin);
  plugin->show();
  QtedmPropertyRule rule;
  rule.inputs.append({QLatin1Char('A'),
      QStringLiteral("middle:test:plugin"),
      QtedmRuleInputType::kNumber});
  QtedmRuleSet ruleSet;
  ruleSet.rules.append(rule);

  state->displays.append(&window);
  state->activeDisplay = &window;
  window.show();
  window.enterExecuteMode();
  QCoreApplication::processEvents();
  QVERIFY(window.executeMiddleButtonApplicationFilterInstalled_);
  window.propertyRuleSets_.insert(plugin, ruleSet);

  auto verifyMiddleButton = [&window](QWidget *target,
      const QString &expectedText) {
    if (!target) {
      return false;
    }
    QTest::mousePress(target, Qt::MiddleButton, Qt::NoModifier,
        target->rect().center());
    QCoreApplication::processEvents();
    const bool shown = window.executeDragPending_
        && window.executeDragTooltipVisible_
        && window.executeDragTooltipText_ == expectedText;
    QTest::mouseRelease(target, Qt::MiddleButton, Qt::NoModifier,
        target->rect().center());
    QCoreApplication::processEvents();
    return shown && !window.executeDragPending_
        && !window.executeDragTooltipVisible_;
  };

  auto *setpointEdit = setpoint->findChild<QLineEdit *>();
  QVERIFY(setpointEdit);
  const QString setpointChannels = QStringLiteral(
      "middle:test:setpoint middle:test:readback");
  QVERIFY(verifyMiddleButton(setpointEdit, setpointChannels));
  auto *decrement = setpoint->findChild<QToolButton *>(
      QStringLiteral("qtedmSpinBoxDecrementButton"));
  auto *increment = setpoint->findChild<QToolButton *>(
      QStringLiteral("qtedmSpinBoxIncrementButton"));
  QVERIFY(verifyMiddleButton(decrement, setpointChannels));
  QVERIFY(verifyMiddleButton(increment, setpointChannels));
  QVERIFY(verifyMiddleButton(pvTable->viewport(),
      QStringLiteral("middle:test:pv_table")));
  QVERIFY(verifyMiddleButton(waveTable->viewport(),
      QStringLiteral("middle:test:wave_table")));
  QVERIFY(verifyMiddleButton(pluginButton,
      QStringLiteral("middle:test:plugin")));

  auto verifyRightButton = [](QWidget *target) {
    auto menuShown = std::make_shared<bool>(false);
    auto actions = std::make_shared<QStringList>();
    QTimer::singleShot(0, [menuShown, actions]() {
      auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
      if (!menu) {
        return;
      }
      *menuShown = menu->objectName()
          == QStringLiteral("executeModeContextMenu");
      for (QAction *action : menu->actions()) {
        actions->append(action->text());
      }
      menu->close();
    });
    QTest::mouseClick(target, Qt::RightButton, Qt::NoModifier,
        target->rect().center());
    QCoreApplication::processEvents();
    return *menuShown && actions->contains(QStringLiteral("Print"));
  };
  QVERIFY(verifyRightButton(setpointEdit));
  QVERIFY(verifyRightButton(pluginButton));

  window.leaveExecuteMode();
  QVERIFY(!window.executeMiddleButtonApplicationFilterInstalled_);
  state->displays.removeAll(&window);
  state->activeDisplay.clear();
}

void TestObserveOnlyControls::editorOperationsCoverExtensionInventories()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  window.setAttribute(Qt::WA_DeleteOnClose, false);
  window.resize(700, 500);
  window.show();
  QCoreApplication::processEvents();

  auto prepareWidget = [&window](QWidget *widget, const QRect &geometry) {
    widget->setGeometry(geometry);
    window.ensureElementInStack(widget);
    widget->show();
  };

  auto *pvTable = new PvTableElement(window.displayArea_);
  prepareWidget(pvTable, QRect(420, 20, 100, 80));
  window.pvTableElements_.append(pvTable);
  auto *waveTable = new WaveTableElement(window.displayArea_);
  prepareWidget(waveTable, QRect(540, 20, 100, 80));
  window.waveTableElements_.append(waveTable);
  QCoreApplication::processEvents();

  /* A table-only display must still enable editor selection operations. */
  QVERIFY(window.hasSelectableElements());
  QVERIFY(window.selectWidgetForEditing(pvTable));
  QVERIFY(window.hasAnyElementSelection());
  window.clearSelection();

  auto *plugin = new PluginElement(window.displayArea_);
  prepareWidget(plugin, QRect(420, 120, 100, 60));
  window.pluginElements_.append(plugin);
  window.selectAllElements();
  QVERIFY(window.isWidgetInMultiSelection(pvTable));
  QVERIFY(window.isWidgetInMultiSelection(waveTable));
  QVERIFY(window.isWidgetInMultiSelection(plugin));
  window.clearSelection();

  auto *setpoint = new SetpointControlElement(window.displayArea_);
  prepareWidget(setpoint, QRect(420, 200, 160, 60));
  window.setpointControlElements_.append(setpoint);
  QVERIFY(window.selectWidgetForEditing(setpoint));
  QVERIFY(setpoint->isSelected());
  window.clearSelection();
  QVERIFY(!setpoint->isSelected());

  auto *expression = new ExpressionChannelElement(window.displayArea_);
  prepareWidget(expression, QRect(20, 20, 80, 50));
  window.expressionChannelElements_.append(expression);
  auto *composite = new CompositeElement(window.displayArea_);
  prepareWidget(composite, QRect(120, 20, 80, 50));
  window.compositeElements_.append(composite);
  auto *tabbed = new TabbedDisplayElement(window.displayArea_);
  prepareWidget(tabbed, QRect(20, 100, 180, 100));
  window.tabbedDisplayElements_.append(tabbed);
  plugin->setGeometry(220, 100, 80, 50);
  QCoreApplication::processEvents();

  window.applySelectionRect(QRect(0, 0, 320, 220));
  QVERIFY(window.isWidgetInMultiSelection(expression));
  QVERIFY(window.isWidgetInMultiSelection(composite));
  QVERIFY(window.isWidgetInMultiSelection(tabbed));
  QVERIFY(window.isWidgetInMultiSelection(plugin));
  window.clearSelection();

  QVERIFY(window.selectWidgetForEditing(plugin));
  QVERIFY(plugin->isSelected());
  window.handleDisplayBackgroundClick();
  QVERIFY(!plugin->isSelected());
  QVERIFY(window.displaySelected_);
  window.handleDisplayBackgroundClick();
  QVERIFY(!window.displaySelected_);

  expression->setGeometry(10, 10, 20, 20);
  tabbed->setGeometry(20, 20, 30, 30);
  plugin->setGeometry(30, 30, 40, 40);
  window.scaleAllElements(100, 100, 200, 300);
  QCOMPARE(expression->geometry(), QRect(20, 30, 40, 60));
  QCOMPARE(tabbed->geometry(), QRect(40, 60, 60, 90));
  QCOMPARE(plugin->geometry(), QRect(60, 90, 80, 120));

  pvTable->setGeometry(20, 250, 100, 80);
  waveTable->setGeometry(140, 250, 100, 80);
  setpoint->setGeometry(260, 250, 160, 60);
  expression->setGeometry(20, 350, 80, 50);
  composite->setGeometry(120, 350, 80, 50);
  tabbed->setGeometry(220, 330, 180, 100);
  plugin->setGeometry(window.displayArea_->width() + 20, 20, 80, 60);
  QTimer::singleShot(0, []() {
    for (QWidget *widget : QApplication::topLevelWidgets()) {
      if (auto *box = qobject_cast<QMessageBox *>(widget)) {
        box->accept();
      }
    }
  });
  window.findOutliers();
  QVERIFY(plugin->isSelected());
  QVERIFY(window.hasAnyElementSelection());
  QCOMPARE(window.pvInfoElementLabel(plugin),
      QStringLiteral("Plugin Object"));
  QCOMPARE(window.pvInfoElementLabel(tabbed),
      QStringLiteral("Tabbed Display"));

  auto regressionState = std::make_shared<DisplayState>();
  regressionState->editMode = true;
  DisplayWindow regression(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), regressionState);
  regression.setAttribute(Qt::WA_DeleteOnClose, false);
  regression.resize(640, 420);
  regression.show();

  auto addRegressionWidget = [&regression](QWidget *widget,
                                 const QRect &geometry) {
    widget->setGeometry(geometry);
    regression.ensureElementInStack(widget);
    widget->show();
  };
  auto *heatmap = new HeatmapElement(regression.displayArea_);
  addRegressionWidget(heatmap, QRect(20, 200, 120, 80));
  regression.heatmapElements_.append(heatmap);
  auto *waterfall = new WaterfallPlotElement(regression.displayArea_);
  addRegressionWidget(waterfall, QRect(160, 200, 120, 80));
  regression.waterfallPlotElements_.append(waterfall);
  auto *textA = new TextElement(regression.displayArea_);
  textA->setText(QStringLiteral("A"));
  addRegressionWidget(textA, QRect(20, 20, 60, 30));
  regression.textElements_.append(textA);
  auto *textB = new TextElement(regression.displayArea_);
  textB->setText(QStringLiteral("B"));
  addRegressionWidget(textB, QRect(100, 20, 60, 30));
  regression.textElements_.append(textB);

  regression.selectHeatmapElement(heatmap);
  QVERIFY(regression.selectedWidgets().contains(heatmap));
  regression.selectTextElement(textA);
  QVERIFY(!heatmap->isSelected());
  QVERIFY(!regression.selectedWidgets().contains(heatmap));
  regression.selectWaterfallPlotElement(waterfall);
  QVERIFY(regression.selectedWidgets().contains(waterfall));
  regression.handleResourcePaletteClosed();
  QVERIFY(!waterfall->isSelected());
  QVERIFY(regression.selectedWidgets().isEmpty());

  auto *selectionTabbed = new TabbedDisplayElement(regression.displayArea_);
  addRegressionWidget(selectionTabbed, QRect(300, 200, 120, 80));
  regression.tabbedDisplayElements_.append(selectionTabbed);
  regression.selectTabbedDisplayElement(selectionTabbed);
  regression.removeWidgetFromSelection(selectionTabbed);
  QVERIFY(!selectionTabbed->isSelected());
  QVERIFY(!regression.selectedTabbedDisplayElement_);
  auto *selectionPlugin = new PluginElement(regression.displayArea_);
  addRegressionWidget(selectionPlugin, QRect(440, 200, 100, 60));
  regression.pluginElements_.append(selectionPlugin);
  regression.selectPluginElement(selectionPlugin);
  regression.removeWidgetFromSelection(selectionPlugin);
  QVERIFY(!selectionPlugin->isSelected());
  QVERIFY(!regression.selectedPluginElement_);
  QVERIFY(regression.cutWidgetFromDisplay(selectionTabbed));
  QVERIFY(regression.cutWidgetFromDisplay(selectionPlugin));

  QtedmPropertyRule textRule;
  textRule.id = QStringLiteral("copy-rule");
  textRule.expression = QStringLiteral("A>0");
  textRule.inputs.append({QLatin1Char('A'),
      QStringLiteral("__test:clipboard_rule"),
      QtedmRuleInputType::kNumber});
  QtedmRuleSet textRules;
  textRules.rules.append(textRule);
  regression.propertyRuleSets_.insert(textA, textRules);
  regression.propertyRuleSets_.insert(textB, textRules);

  regression.clearSelections();
  regression.addWidgetToMultiSelection(textA);
  regression.addWidgetToMultiSelection(textB);
  QCOMPARE(regression.multiSelection_.size(), 2);
  regression.copySelection();
  regression.pasteSelection();
  QCOMPARE(regression.textElements_.size(), 4);
  QCOMPARE(regression.multiSelection_.size(), 2);
  QCOMPARE(regression.propertyRuleSets_.size(), 4);
  regression.cutSelection();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCOMPARE(regression.textElements_.size(), 2);
  QCOMPARE(regression.propertyRuleSets_.size(), 2);

  auto *polyline = new PolylineElement(regression.displayArea_);
  const QVector<QPoint> originalPoints = {
      QPoint(300, 20), QPoint(320, 35), QPoint(340, 45)};
  polyline->setAbsolutePoints(originalPoints);
  regression.polylineElements_.append(polyline);
  regression.ensureElementInStack(polyline);
  polyline->show();
  regression.clearSelections();
  regression.addWidgetToMultiSelection(textA);
  regression.addWidgetToMultiSelection(polyline);
  regression.copySelection();
  regression.pasteSelection();
  QCOMPARE(regression.textElements_.size(), 3);
  QCOMPARE(regression.polylineElements_.size(), 2);
  const QVector<QPoint> pastedPoints =
      regression.polylineElements_.last()->absolutePoints();
  QCOMPARE(pastedPoints, QVector<QPoint>({QPoint(310, 30), QPoint(330, 45),
          QPoint(350, 55)}));
  regression.cutSelection();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCOMPARE(regression.textElements_.size(), 2);
  QCOMPARE(regression.polylineElements_.size(), 1);
  QCOMPARE(regression.propertyRuleSets_.size(), 2);

  regression.selectTextElement(textA);
  regression.copySelection();
  regression.pasteSelection();
  QCOMPARE(regression.textElements_.size(), 3);
  TextElement *singleCopy = regression.textElements_.last();
  QVERIFY(regression.propertyRuleSets_.contains(singleCopy));
  regression.cutSelection();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCOMPARE(regression.textElements_.size(), 2);

  regression.clearSelections();
  regression.addWidgetToMultiSelection(textA);
  regression.addWidgetToMultiSelection(textB);
  regression.groupSelectedElements();
  QCOMPARE(regression.compositeElements_.size(), 1);
  CompositeElement *group = regression.compositeElements_.front();
  QCOMPARE(group->childWidgets().size(), 2);
  QVERIFY(regression.propertyRuleSets_.contains(textA));
  QVERIFY(regression.propertyRuleSets_.contains(textB));

  const QByteArray groupedState = regression.serializeStateForUndo();
  DisplayWindow restored(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), regressionState);
  QVERIFY(restored.restoreSerializedState(groupedState));
  QCOMPARE(restored.compositeElements_.size(), 1);
  QCOMPARE(restored.compositeElements_.front()->childWidgets().size(), 2);
  QCOMPARE(restored.propertyRuleSets_.size(), 2);

  QtedmPropertyRule compositeRule = textRule;
  compositeRule.id = QStringLiteral("group-rule");
  QtedmRuleSet compositeRules;
  compositeRules.rules.append(compositeRule);
  regression.propertyRuleSets_.insert(group, compositeRules);
  regression.selectCompositeElement(group);
  regression.ungroupSelectedElements();
  QCOMPARE(regression.compositeElements_.size(), 0);
  QCOMPARE(regression.textElements_.size(), 2);
  QCOMPARE(regression.propertyRuleSets_.size(), 2);
  for (TextElement *text : std::as_const(regression.textElements_)) {
    QCOMPARE(regression.propertyRuleSets_.value(text).rules.size(), 2);
  }
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
  toggle.resize(180, 44);
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
  QCOMPARE(toggleButton->property("_qtedmToggleOn").toBool(), true);
  toggle.show();
  QCoreApplication::processEvents();
  const QImage readOnlyToggle = toggleButton->grab().toImage();
  QCOMPARE(toggleButton->cursor().shape(), Qt::ForbiddenCursor);
  QCOMPARE(toggle.cursor().shape(), Qt::ForbiddenCursor);
  toggle.setRuntimeWriteAccess(true);
  QVERIFY(toggleButton->isEnabled());
  QCoreApplication::processEvents();
  const QImage writableToggle = toggleButton->grab().toImage();
  const int centerX = readOnlyToggle.width() / 2;
  QCOMPARE(readOnlyToggle.pixelColor(centerX, 0),
      writableToggle.pixelColor(centerX, 0));
  QCOMPARE(readOnlyToggle.pixelColor(centerX, readOnlyToggle.height() - 1),
      writableToggle.pixelColor(centerX, writableToggle.height() - 1));
  QCOMPARE(toggleButton->cursor().shape(), Qt::ArrowCursor);
  QCOMPARE(toggle.cursor().shape(), Qt::ArrowCursor);
  toggleButton->click();
  QVERIFY(toggleButton->isChecked());
  toggle.setRuntimeToggleState(false, true);
  QCOMPARE(toggleButton->property("_qtedmToggleOn").toBool(), false);

  TextAreaElement textArea;
  textArea.setActivationCallback([](const QByteArray &) {});
  textArea.setExecuteMode(true);
  textArea.setRuntimeConnected(true);
  auto *textEditor = textArea.findChild<QTextEdit *>();
  QVERIFY(textEditor);
  QCOMPARE(textEditor->cursor().shape(), Qt::ForbiddenCursor);
  QCOMPARE(textEditor->viewport()->cursor().shape(), Qt::ForbiddenCursor);
  QCOMPARE(textArea.cursor().shape(), Qt::ForbiddenCursor);
  textArea.setRuntimeWriteAccess(true);
  QCOMPARE(textEditor->viewport()->cursor().shape(), Qt::IBeamCursor);
  QCOMPARE(textArea.cursor().shape(), Qt::ArrowCursor);
  textArea.setRuntimeWriteAccess(false);
  QCOMPARE(textEditor->viewport()->cursor().shape(), Qt::ForbiddenCursor);
  textArea.setRuntimeConnected(false);
  QCOMPARE(textEditor->viewport()->cursor().shape(), Qt::ArrowCursor);
  QCOMPARE(textArea.cursor().shape(), Qt::ArrowCursor);

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
