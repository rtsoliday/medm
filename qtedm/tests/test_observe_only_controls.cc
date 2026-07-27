#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolButton>

#include "audit_logger.h"
#include "extension_object_registry.h"
#include "led_monitor_element.h"
#include "message_button_element.h"
#include "message_button_runtime.h"
#include "pv_channel_manager.h"
#include "setpoint_control_element.h"
#include "soft_pv_registry.h"

class TestObserveOnlyControls : public QObject
{
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void registryContainsSafetyControlObjects();
  void observeOnlyBlocksEverySoftPvWriteKind();
  void toggleWritesAlternatingValuesThroughSoftPv();
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
