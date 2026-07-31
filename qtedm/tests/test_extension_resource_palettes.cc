#include <QtTest/QtTest>

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>

#include <memory>

#include "display_state.h"
#include "display_window.h"
#include "extension_object_registry.h"
#include "led_monitor_element.h"
#include "message_button_element.h"
#include "ntndarray_image_element.h"
#include "object_palette_dialog.h"
#include "plugin_element.h"
#include "resource_palette_dialog.h"
#include "setpoint_control_element.h"
#include "strip_chart_element.h"
#include "tabbed_display_element.h"

class TestExtensionResourcePalettes : public QObject
{
  Q_OBJECT

private slots:
  void resourcePaletteBuildsTypedExtensionFields();
  void displaySelectionShowsEachExtensionPalette();
  void clipboardPreservesExtensionWidgetTypes();
  void objectPaletteGroupsAndDistinguishesExtensionTools();
};

void TestExtensionResourcePalettes::
    resourcePaletteBuildsTypedExtensionFields()
{
  ResourcePaletteDialog dialog(QApplication::palette(), QFont(), QFont());
  QRect geometry(10, 20, 160, 80);
  bool enabled = false;
  int maximum = 10;
  qlonglong wideInteger = 5000000000LL;
  double wideDouble = 1.0e100;
  QString name = QStringLiteral("before");
  bool actionInvoked = false;

  ResourcePaletteProperty boolean;
  boolean.key = QStringLiteral("enabled");
  boolean.label = QStringLiteral("Enabled");
  boolean.type = ResourcePalettePropertyType::kBoolean;
  boolean.value = enabled;
  boolean.setter = [&enabled](const QVariant &value) {
    enabled = value.toBool();
  };

  ResourcePaletteProperty integer;
  integer.key = QStringLiteral("maximum");
  integer.label = QStringLiteral("Maximum");
  integer.type = ResourcePalettePropertyType::kInteger;
  integer.value = maximum;
  integer.minimum = 1;
  integer.maximum = 100;
  integer.setter = [&maximum](const QVariant &value) {
    maximum = value.toInt();
  };

  ResourcePaletteProperty text;
  text.key = QStringLiteral("name");
  text.label = QStringLiteral("Name");
  text.type = ResourcePalettePropertyType::kString;
  text.value = name;
  text.setter = [&name](const QVariant &value) {
    name = value.toString();
  };

  ResourcePaletteProperty integer64;
  integer64.key = QStringLiteral("wide_integer");
  integer64.label = QStringLiteral("Wide Integer");
  integer64.type = ResourcePalettePropertyType::kInteger64;
  integer64.value = wideInteger;
  integer64.setter = [&wideInteger](const QVariant &value) {
    wideInteger = value.toLongLong();
  };

  ResourcePaletteProperty doubleText;
  doubleText.key = QStringLiteral("wide_double");
  doubleText.label = QStringLiteral("Wide Double");
  doubleText.type = ResourcePalettePropertyType::kDoubleText;
  doubleText.value = wideDouble;
  doubleText.setter = [&wideDouble](const QVariant &value) {
    wideDouble = value.toDouble();
  };

  ResourcePaletteProperty action;
  action.key = QStringLiteral("details");
  action.label = QStringLiteral("Details");
  action.type = ResourcePalettePropertyType::kAction;
  action.actionText = QStringLiteral("Edit...");
  action.action = [&actionInvoked]() { actionInvoked = true; };

  dialog.showForExtension([&geometry]() { return geometry; },
      [&geometry](const QRect &updated) { geometry = updated; },
      QStringLiteral("Extension Test"),
      {boolean, integer, integer64, doubleText, text, action});
  QCoreApplication::processEvents();

  auto *enabledField = dialog.findChild<QComboBox *>(
      QStringLiteral("qtedmResourceProperty_enabled"));
  auto *maximumField = dialog.findChild<QSpinBox *>(
      QStringLiteral("qtedmResourceProperty_maximum"));
  auto *nameField = dialog.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_name"));
  auto *wideIntegerField = dialog.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_wide_integer"));
  auto *wideDoubleField = dialog.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_wide_double"));
  auto *detailsButton = dialog.findChild<QPushButton *>(
      QStringLiteral("qtedmResourceProperty_details"));
  QVERIFY(enabledField);
  QVERIFY(maximumField);
  QVERIFY(nameField);
  QVERIFY(wideIntegerField);
  QVERIFY(wideDoubleField);
  QVERIFY(detailsButton);
  QVERIFY(enabledField->isVisible());

  enabledField->setCurrentIndex(1);
  maximumField->setValue(42);
  wideIntegerField->setText(QStringLiteral("6000000000"));
  QMetaObject::invokeMethod(wideIntegerField, "editingFinished",
      Qt::DirectConnection);
  nameField->setText(QStringLiteral("after"));
  QMetaObject::invokeMethod(nameField, "editingFinished",
      Qt::DirectConnection);
  detailsButton->click();
  QVERIFY(enabled);
  QCOMPARE(maximum, 42);
  QCOMPARE(wideInteger, 6000000000LL);
  QCOMPARE(wideDouble, 1.0e100);
  wideDoubleField->setText(QStringLiteral("2.5e200"));
  QMetaObject::invokeMethod(wideDoubleField, "editingFinished",
      Qt::DirectConnection);
  QCOMPARE(wideDouble, 2.5e200);
  wideDoubleField->setText(QStringLiteral("inf"));
  QMetaObject::invokeMethod(wideDoubleField, "editingFinished",
      Qt::DirectConnection);
  QCOMPARE(wideDouble, 2.5e200);
  QCOMPARE(name, QStringLiteral("after"));
  QVERIFY(actionInvoked);
}

void TestExtensionResourcePalettes::
    displaySelectionShowsEachExtensionPalette()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  auto requireField = [&window](const QString &name) {
    QWidget *field = window.findChild<QWidget *>(name);
    QVERIFY2(field, qPrintable(name));
  };

  auto *spinbox = new SetpointControlElement(window.centralWidget());
  spinbox->setQtedmSpinBox(true);
  window.selectAndScrollToWidget(spinbox);
  requireField(QStringLiteral("qtedmResourceProperty_step_size"));

  auto *toggle = new MessageButtonElement(window.centralWidget());
  toggle->setQtedmToggle(true);
  window.selectAndScrollToWidget(toggle);
  requireField(QStringLiteral("qtedmResourceProperty_off_label"));
  requireField(QStringLiteral("qtedmResourceProperty_on_label"));
  requireField(QStringLiteral("qtedmResourceProperty_confirm"));

  auto *archivePlot = new StripChartElement(window.centralWidget());
  archivePlot->setArchivePlot(true);
  window.selectAndScrollToWidget(archivePlot);
  requireField(QStringLiteral(
      "qtedmResourceProperty_archive_maximum_points"));
  requireField(QStringLiteral("qtedmResourceProperty_archive_live_merge"));

  auto *symbol = new LedMonitorElement(window.centralWidget());
  symbol->setQtedmSymbol(true);
  window.selectAndScrollToWidget(symbol);
  requireField(QStringLiteral("qtedmResourceProperty_symbol_states"));

  auto *image = new NtNdArrayImageElement(window.centralWidget());
  window.selectAndScrollToWidget(image);
  requireField(QStringLiteral("qtedmResourceProperty_data_pv"));
  requireField(QStringLiteral("qtedmResourceProperty_pixel_probe"));
  requireField(QStringLiteral("qtedmResourceProperty_maximum_input_mib"));
  requireField(QStringLiteral("qtedmResourceProperty_maximum_image_mib"));
  requireField(QStringLiteral("qtedmResourceProperty_maximum_dimension"));

  auto *tabs = new TabbedDisplayElement(window.centralWidget());
  window.selectAndScrollToWidget(tabs);
  requireField(QStringLiteral("qtedmResourceProperty_stacked_mode"));
  requireField(QStringLiteral("qtedmResourceProperty_pages"));

  QtedmDisplayObjectType pluginType;
  pluginType.typeId = QStringLiteral("org.qtedm.tests.palette.integration");
  pluginType.displayName = QStringLiteral("Integration Plugin");
  pluginType.properties = {
      {QStringLiteral("caption"), QStringLiteral("Caption"), QString(),
          QtedmPluginPropertyType::kString, QStringLiteral("Test"), false},
      {QStringLiteral("enabled"), QStringLiteral("Enabled"), QString(),
          QtedmPluginPropertyType::kBoolean, true, false},
      {QStringLiteral("wide"), QStringLiteral("Wide"), QString(),
          QtedmPluginPropertyType::kInteger, 5000000000LL, false},
      {QStringLiteral("huge"), QStringLiteral("Huge"), QString(),
          QtedmPluginPropertyType::kDouble, 1.0e100, false},
  };
  QVERIFY(ExtensionObjectRegistry::instance().registerPluginObject(
      QStringLiteral("org.qtedm.tests.integration"), pluginType));
  AdlNode pluginNode;
  pluginNode.name = QStringLiteral("qtedm_plugin");
  pluginNode.properties = {
      {QStringLiteral("pluginId"),
          QStringLiteral("org.qtedm.tests.integration")},
      {QStringLiteral("typeId"), pluginType.typeId},
      {QStringLiteral("schemaVersion"), QStringLiteral("1")},
  };
  AdlNode object;
  object.name = QStringLiteral("object");
  object.properties = {
      {QStringLiteral("x"), QStringLiteral("0")},
      {QStringLiteral("y"), QStringLiteral("0")},
      {QStringLiteral("width"), QStringLiteral("100")},
      {QStringLiteral("height"), QStringLiteral("40")},
  };
  pluginNode.children.append(object);
  auto *plugin = new PluginElement(window.centralWidget());
  QString error;
  QVERIFY2(plugin->loadFromAdlNode(pluginNode, &error), qPrintable(error));
  window.selectAndScrollToWidget(plugin);
  requireField(QStringLiteral("qtedmResourceProperty_plugin"));
  requireField(QStringLiteral("qtedmResourceProperty_status"));
  requireField(QStringLiteral("qtedmResourceProperty_caption"));
  requireField(QStringLiteral("qtedmResourceProperty_enabled"));
  QVERIFY(window.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_wide")));
  QVERIFY(window.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_huge")));
  window.selectAndScrollToWidget(spinbox);
  QVERIFY(!plugin->isSelected());
  ExtensionObjectRegistry::instance().unregisterPluginObjects();
}

void TestExtensionResourcePalettes::clipboardPreservesExtensionWidgetTypes()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);

  auto *toggle = new MessageButtonElement(window.centralWidget());
  toggle->setGeometry(10, 10, 100, 30);
  toggle->setQtedmToggle(true);
  toggle->setOffValue(QStringLiteral("Disabled"));
  toggle->setOnValue(QStringLiteral("Enabled"));
  toggle->setOffLabel(QStringLiteral("Stopped"));
  toggle->setOnLabel(QStringLiteral("Running"));
  toggle->setConfirmationRequired(true);
  window.selectAndScrollToWidget(toggle);
  window.copySelection();
  window.pasteSelection();
  int toggleCopies = 0;
  for (QWidget *widget : window.findChildren<QWidget *>()) {
    auto *candidate = dynamic_cast<MessageButtonElement *>(widget);
    if (!candidate) {
      continue;
    }
    if (candidate->isQtedmToggle()) {
      ++toggleCopies;
      QCOMPARE(candidate->onLabel(), QStringLiteral("Running"));
      QVERIFY(candidate->confirmationRequired());
    }
  }
  QCOMPARE(toggleCopies, 2);

  auto *archive = new StripChartElement(window.centralWidget());
  archive->setGeometry(20, 60, 240, 120);
  archive->setArchivePlot(true);
  archive->setArchiveMaximumPoints(12345);
  archive->setArchiveLiveMerge(false);
  window.selectAndScrollToWidget(archive);
  window.copySelection();
  window.pasteSelection();
  int archiveCopies = 0;
  for (QWidget *widget : window.findChildren<QWidget *>()) {
    auto *candidate = dynamic_cast<StripChartElement *>(widget);
    if (!candidate) {
      continue;
    }
    if (candidate->isArchivePlot()) {
      ++archiveCopies;
      QCOMPARE(candidate->archiveMaximumPoints(), 12345);
      QVERIFY(!candidate->archiveLiveMerge());
    }
  }
  QCOMPARE(archiveCopies, 2);

  auto *tabs = new TabbedDisplayElement(window.centralWidget());
  tabs->setGeometry(30, 200, 260, 140);
  tabs->setHiddenTabs(true);
  tabs->setPages({
      {QStringLiteral("first"), QStringLiteral("First"),
          QStringLiteral("first.adl"), QStringLiteral("P=TEST:"), true},
      {QStringLiteral("second"), QStringLiteral("Second"),
          QStringLiteral("second.adl"), QString(), false},
  });
  window.selectAndScrollToWidget(tabs);
  window.copySelection();
  window.pasteSelection();
  int tabCopies = 0;
  for (QWidget *widget : window.findChildren<QWidget *>()) {
    auto *candidate = dynamic_cast<TabbedDisplayElement *>(widget);
    if (!candidate) {
      continue;
    }
    ++tabCopies;
    QVERIFY(candidate->hiddenTabs());
    QCOMPARE(candidate->pages().size(), 2);
    QCOMPARE(candidate->pages().first().displayPath,
        QStringLiteral("first.adl"));
  }
  QCOMPARE(tabCopies, 2);
}

void TestExtensionResourcePalettes::
    objectPaletteGroupsAndDistinguishesExtensionTools()
{
  QtedmDisplayObjectType pluginType;
  pluginType.typeId = QStringLiteral("org.qtedm.tests.palette");
  pluginType.displayName = QStringLiteral("Palette Plugin");
  QVERIFY(ExtensionObjectRegistry::instance().registerPluginObject(
      QStringLiteral("org.qtedm.tests"), pluginType));

  auto state = std::make_shared<DisplayState>();
  ObjectPaletteDialog dialog(QApplication::palette(), QFont(), QFont(),
      state);
  dialog.show();
  QCoreApplication::processEvents();

  bool hasContainerCategory = false;
  for (QLabel *label : dialog.findChildren<QLabel *>()) {
    if (label->text() == QStringLiteral("Container")) {
      hasContainerCategory = true;
      break;
    }
  }
  QVERIFY(hasContainerCategory);

  const QStringList extensionLabels{
      QStringLiteral("Multi-State Symbol"), QStringLiteral("Toggle"),
      QStringLiteral("Spin Box"), QStringLiteral("Tabbed Display"),
      QStringLiteral("Archive Plot"), QStringLiteral("NTNDArray Image"),
      QStringLiteral("Palette Plugin")};
  QList<QImage> icons;
  for (const QString &label : extensionLabels) {
    QToolButton *matched = nullptr;
    for (QToolButton *button : dialog.findChildren<QToolButton *>()) {
      if (button->toolTip() == label) {
        matched = button;
        break;
      }
    }
    QVERIFY2(matched, qPrintable(label));
    QVERIFY2(!matched->icon().isNull(), qPrintable(label));
    const QImage image = matched->icon().pixmap(25, 25).toImage();
    QVERIFY2(!image.isNull(), qPrintable(label));
    for (const QImage &previous : icons) {
      QVERIFY2(image != previous, qPrintable(label));
    }
    icons.append(image);
  }

  ExtensionObjectRegistry::instance().unregisterPluginObjects();
}

QTEST_MAIN(TestExtensionResourcePalettes)

#include "test_extension_resource_palettes.moc"
