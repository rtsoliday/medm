#include <QtTest/QtTest>

#include <QComboBox>
#include <QFile>
#include <QMap>
#include <QTemporaryDir>
#include <QScreen>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QScrollBar>
#include <QScopeGuard>
#include <QTableWidget>
#include <QTimer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include "color_palette_dialog.h"
#include "arc_element.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>

#include <cmath>
#include <limits>
#include <memory>

#include "cartesian_plot_element.h"
#include "cartesian_axis_dialog.h"
#include "pv_limits_dialog.h"
#include "pv_table_element.h"
#include "shell_command_element.h"
#include "related_display_element.h"
#include "expression_channel_element.h"
#include "heatmap_element.h"
#include "slider_element.h"
#include "text_element.h"
#include "waterfall_plot_element.h"
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

namespace {
template<typename T>
T *findTypedWidget(QObject &root)
{
  for (auto *widget : root.findChildren<QWidget *>()) {
    if (auto *typed = dynamic_cast<T *>(widget)) return typed;
  }
  return nullptr;
}
}  // namespace

class TestExtensionResourcePalettes : public QObject
{
  Q_OBJECT

private slots:
  void resourcePaletteBuildsTypedExtensionFields();
  void extensionTextConfirmationsPreserveValues();
  void tabPageConfirmationsKeepDocumentClean();
  void symbolEditorPreservesAlpha_data();
  void symbolEditorPreservesAlpha();
  void ntNdArrayLimitsStayExact_data();
  void ntNdArrayLimitsStayExact();
  void extensionDialogConfirmationsStayClean();
  void arcColorPickerIdentifiesSelection();
  void textFieldsUseAvailablePaletteWidth();
  void ntNdArrayRangePalettePreservesFiniteValues_data();
  void ntNdArrayRangePalettePreservesFiniteValues();
  void numericEditorsPreserveValuesAndRejectInvalidInput();
  void numericValuesSurviveSaveAndReload_data();
  void numericValuesSurviveSaveAndReload();
  void configuredNumericValuesSurviveEditingAndReload_data();
  void configuredNumericValuesSurviveEditingAndReload();
  void cartesianTraceControlsIdentifyTheirRow();
  void savedRangesAndSymbolStatesStayExact_data();
  void savedRangesAndSymbolStatesStayExact();
  void entryConfirmationsStayClean();
  void secondaryDialogsStayCleanAndIdentifyFields();
  void narrowEntryPalettesKeepColumnsReachable();
  void unchangedAttributeEditsStayClean_data();
  void unchangedAttributeEditsStayClean();
  void penAndLedColorsIdentifyTheirIndex();
  void ruleRateSurvivesSaveAndReload_data();
  void ruleRateSurvivesSaveAndReload();
  void palettePreservesUserSize();
  void heatmapBooleansUseCommonLabels();
  void paletteColumnsAlignAndShrinkWithSelection();
  void paletteButtonsSupportKeyboardActivation();
  void tabPageColumnsAreReadableAndResizable();
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

void TestExtensionResourcePalettes::tabPageConfirmationsKeepDocumentClean()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("tabs.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("file { version=030122 } display { object { width=600 height=400 } } "
      "qtedm_tabbed_display { object { width=400 height=300 } "
      "page { id=first label=\"  Padded  \" } "
      "page { id=second label=Second } active_page=second }");
  file.close();
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *tabs = findTypedWidget<TabbedDisplayElement>(window);
  QVERIFY(tabs);
  window.selectAndScrollToWidget(tabs);
  auto *button = window.findChild<QPushButton *>(
      QStringLiteral("qtedmResourceProperty_pages"));
  QVERIFY(button);
  bool inspected = false;
  QTimer::singleShot(0, &window, [&] {
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    const auto close = qScopeGuard([&] {
      if (dialog->isVisible()) dialog->reject();
    });
    auto *table = dialog->findChild<QTableWidget *>();
    QVERIFY(table);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("  Padded  "));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    inspected = true;
  });
  button->click();
  QVERIFY(inspected);
  QVERIFY(!window.isDirty());
  QCOMPARE(tabs->pages().first().label, QStringLiteral("  Padded  "));
  QCOMPARE(tabs->activePageId(), QStringLiteral("second"));
  QVERIFY(window.saveToPath(path));
  DisplayWindow reopened(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  QVERIFY2(reopened.loadFromFile(path, &error), qPrintable(error));
  auto *reloaded = findTypedWidget<TabbedDisplayElement>(reopened);
  QVERIFY(reloaded);
  QCOMPARE(reloaded->pages().first().label, QStringLiteral("  Padded  "));
}

void TestExtensionResourcePalettes::symbolEditorPreservesAlpha_data()
{
  QTest::addColumn<QString>("color");
  QTest::newRow("translucent") << QStringLiteral("#80336699");
  QTest::newRow("transparent") << QStringLiteral("#00336699");
  QTest::newRow("opaque") << QStringLiteral("#ff336699");
}

void TestExtensionResourcePalettes::symbolEditorPreservesAlpha()
{
  QFETCH(QString, color);
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("symbol.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(QStringLiteral(
      "file { version=030122 } display { object { width=500 height=400 } } "
      "qtedm_symbol { object { width=200 height=100 } stateCount=1 "
      "state { min=0 max=1 label=Original color=\"%1\" } }").arg(color).toUtf8());
  file.close();
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *symbol = findTypedWidget<LedMonitorElement>(window);
  QVERIFY(symbol);
  const QColor original(color);
  QCOMPARE(symbol->symbolStates().first().color, original);
  for (int attempt = 0; attempt < 3; ++attempt) {
    bool inspected = false;
    QTimer::singleShot(0, &window, [&] {
      auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      const auto close = qScopeGuard([&] {
        if (dialog->isVisible()) dialog->reject();
      });
      auto *table = dialog->findChild<QTableWidget *>();
      QVERIFY(table);
      QCOMPARE(table->item(0, 4)->text(), color);
      if (attempt == 1) {
        table->item(0, 4)->setText(QStringLiteral("#ff000000"));
      } else if (attempt == 2) {
        table->item(0, 2)->setText(QStringLiteral("Edited"));
      }
      dialog->findChild<QDialogButtonBox *>()->button(
          attempt == 1 ? QDialogButtonBox::Cancel : QDialogButtonBox::Ok)->click();
      inspected = true;
    });
    window.showQtedmExtensionProperties(symbol);
    QVERIFY(inspected);
    QCOMPARE(symbol->symbolStates().first().color, original);
    QCOMPARE(window.isDirty(), attempt == 2);
  }
  QCOMPARE(symbol->symbolStates().first().label, QStringLiteral("Edited"));
}

void TestExtensionResourcePalettes::extensionTextConfirmationsPreserveValues()
{
  ResourcePaletteDialog dialog(QApplication::palette(), QFont(), QFont());
  const QStringList original{QStringLiteral("a;b"), QString(),
      QStringLiteral(" c "), QStringLiteral("quote\"\\\n\u03a9")};
  QStringList list = original;
  int listCalls = 0;
  int textCalls = 0;
  int integerCalls = 0;
  ResourcePaletteProperty property;
  property.key = QStringLiteral("list");
  property.label = QStringLiteral("List");
  property.type = ResourcePalettePropertyType::kStringList;
  property.value = list;
  property.setter = [&](const QVariant &value) {
    ++listCalls;
    list = value.toStringList();
  };
  ResourcePaletteProperty text;
  text.key = QStringLiteral("text");
  text.label = QStringLiteral("Text");
  text.value = QStringLiteral("unchanged");
  text.setter = [&](const QVariant &) { ++textCalls; };
  ResourcePaletteProperty integer;
  integer.key = QStringLiteral("integer");
  integer.label = QStringLiteral("Integer");
  integer.type = ResourcePalettePropertyType::kInteger64;
  integer.value = 5000000000LL;
  integer.setter = [&](const QVariant &) { ++integerCalls; };
  dialog.showForExtension([] { return QRect(0, 0, 100, 100); },
      [](const QRect &) {}, QStringLiteral("Test"), {property, text, integer});
  auto *field = dialog.findChild<QLineEdit *>(QStringLiteral("qtedmResourceProperty_list"));
  QVERIFY(field);
  QCOMPARE(QJsonDocument::fromJson(field->text().toUtf8()).array(),
      QJsonArray::fromStringList(original));
  for (auto *edit : dialog.findChildren<QLineEdit *>()) {
    if (edit->isVisible())
      QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
  }
  QCOMPARE(listCalls, 0);
  QCOMPARE(textCalls, 0);
  QCOMPARE(integerCalls, 0);
  for (const QString &invalid : {QStringLiteral("a;b"), QStringLiteral("[1]")}) {
    field->setText(invalid);
    QMetaObject::invokeMethod(field, "editingFinished", Qt::DirectConnection);
    QCOMPARE(list, original);
    QCOMPARE(listCalls, 0);
    QVERIFY(QJsonDocument::fromJson(field->text().toUtf8()).isArray());
  }
  QStringList changed = original;
  changed.append(QStringLiteral("new;entry"));
  field->setText(QString::fromUtf8(QJsonDocument(
      QJsonArray::fromStringList(changed)).toJson(QJsonDocument::Compact)));
  QMetaObject::invokeMethod(field, "editingFinished", Qt::DirectConnection);
  QMetaObject::invokeMethod(field, "editingFinished", Qt::DirectConnection);
  QCOMPARE(list, changed);
  QCOMPARE(listCalls, 1);
}

void TestExtensionResourcePalettes::ntNdArrayLimitsStayExact_data()
{
  QTest::addColumn<quint64>("bytes");
  QTest::addColumn<int>("dimension");
  QTest::newRow("fractional-mib") << quint64(1572865) << 70000;
  QTest::newRow("one-byte") << quint64(1) << 1;
  QTest::newRow("maximum") << std::numeric_limits<quint64>::max()
      << std::numeric_limits<int>::max();
}

void TestExtensionResourcePalettes::ntNdArrayLimitsStayExact()
{
  QFETCH(quint64, bytes);
  QFETCH(int, dimension);
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("limits.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(QStringLiteral("file { version=030122 } display { object { width=500 height=400 } } "
      "qtedm_ndarray_image { object { width=300 height=200 } "
      "maxInputBytes=%1 maxOutputBytes=%1 maxDimension=%2 }").arg(bytes).arg(dimension).toUtf8());
  file.close();
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *image = findTypedWidget<NtNdArrayImageElement>(window);
  QVERIFY(image);
  window.selectAndScrollToWidget(image);
  auto *input = window.findChild<QLineEdit *>(QStringLiteral("qtedmResourceProperty_maximum_input_bytes"));
  auto *output = window.findChild<QLineEdit *>(QStringLiteral("qtedmResourceProperty_maximum_image_bytes"));
  auto *dimensionField = window.findChild<QSpinBox *>(QStringLiteral("qtedmResourceProperty_maximum_dimension"));
  QVERIFY(input && output && dimensionField);
  QCOMPARE(input->text().toULongLong(), bytes);
  QCOMPARE(output->text().toULongLong(), bytes);
  QCOMPARE(dimensionField->value(), dimension);
  for (auto *field : {input, output}) {
    QTest::keyClick(field, Qt::Key_Return);
    QCOMPARE(image->maximumInputBytes(), bytes);
    QVERIFY(!window.isDirty());
    for (const QString &invalid : {QStringLiteral("0"), QStringLiteral("-1"),
        QStringLiteral("18446744073709551616"), QStringLiteral("1.5")}) {
      field->setText(invalid);
      QMetaObject::invokeMethod(field, "editingFinished", Qt::DirectConnection);
      QCOMPARE(field->text().toULongLong(), bytes);
      QVERIFY(!window.isDirty());
    }
  }
  bool inspected = false;
  QTimer::singleShot(0, &window, [&] {
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    const auto close = qScopeGuard([&] {
      if (dialog->isVisible()) dialog->reject();
    });
    auto *inputEdit = dialog->findChild<QLineEdit *>(QStringLiteral("qtedmMaximumInputBytes"));
    auto *outputEdit = dialog->findChild<QLineEdit *>(QStringLiteral("qtedmMaximumOutputBytes"));
    QVERIFY(inputEdit && outputEdit);
    QCOMPARE(inputEdit->text().toULongLong(), bytes);
    QCOMPARE(outputEdit->text().toULongLong(), bytes);
    QCOMPARE(dialog->findChild<QSpinBox *>()->value(), dimension);
    auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    inputEdit->setText(QStringLiteral("0"));
    ok->click();
    QVERIFY(dialog->isVisible());
    QVERIFY(!window.isDirty());
    inputEdit->setText(QString::number(bytes));
    ok->click();
    inspected = true;
  });
  window.showQtedmExtensionProperties(image);
  QVERIFY(inspected);
  QVERIFY(!window.isDirty());
  QCOMPARE(image->maximumInputBytes(), bytes);
  QCOMPARE(image->maximumOutputBytes(), bytes);
  QCOMPARE(image->maximumDimension(), dimension);
  // Actual edits must still apply, and the other exact limit must survive saving.
  input->setText(QStringLiteral("2097153"));
  QMetaObject::invokeMethod(input, "editingFinished", Qt::DirectConnection);
  QCOMPARE(image->maximumInputBytes(), quint64(2097153));
  QVERIFY(window.isDirty());
  QVERIFY(window.saveToPath(path));
  DisplayWindow reopened(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QVERIFY2(reopened.loadFromFile(path, &error), qPrintable(error));
  auto *reloaded = findTypedWidget<NtNdArrayImageElement>(reopened);
  QVERIFY(reloaded);
  QCOMPARE(reloaded->maximumInputBytes(), quint64(2097153));
  QCOMPARE(reloaded->maximumOutputBytes(), bytes);
  QCOMPARE(reloaded->maximumDimension(), dimension);
  QTimer::singleShot(0, &reopened, [&] {
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    const auto close = qScopeGuard([&] {
      if (dialog->isVisible()) dialog->reject();
    });
    auto *edit = dialog->findChild<QLineEdit *>(QStringLiteral("qtedmMaximumOutputBytes"));
    QVERIFY(edit);
    edit->setText(QStringLiteral("3145729"));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  });
  reopened.showQtedmExtensionProperties(reloaded);
  QVERIFY(reopened.isDirty());
  QCOMPARE(reloaded->maximumInputBytes(), quint64(2097153));
  QCOMPARE(reloaded->maximumOutputBytes(), quint64(3145729));
}

void TestExtensionResourcePalettes::extensionDialogConfirmationsStayClean()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("unchanged.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("file { version=030122 } display { object { width=700 height=400 } } "
      "qtedm_spinbox { object { width=100 height=40 } step=0.125 } "
      "qtedm_toggle { object { x=120 width=100 height=40 } } "
      "qtedm_archive_plot { object { x=240 width=300 height=200 } } "
      "qtedm_ndarray_image { object { y=220 width=300 height=150 } title=Image dataPv=local:test:image } "
      "text { object { x=320 y=220 width=200 height=40 } textix=Label } "
      "qtedm_rules { targetIndex=4 rule { id=text property=text expression=1 trueValue=ON falseValue=OFF } }");
  file.close();
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *image = findTypedWidget<NtNdArrayImageElement>(window);
  QVERIFY(image);
  window.selectAndScrollToWidget(image);
  for (const QString &key : {QStringLiteral("title"), QStringLiteral("data_pv")}) {
    auto *edit = window.findChild<QLineEdit *>(QStringLiteral("qtedmResourceProperty_") + key);
    QVERIFY(edit);
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY(!window.isDirty());
  }
  QList<QWidget *> widgets{findTypedWidget<SetpointControlElement>(window),
      findTypedWidget<MessageButtonElement>(window), findTypedWidget<StripChartElement>(window), image};
  for (auto *widget : widgets) {
    QVERIFY(widget);
    bool accepted = false;
    QTimer::singleShot(0, &window, [&] {
      auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      const auto close = qScopeGuard([&] {
      if (dialog->isVisible()) dialog->reject();
    });
      dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
      accepted = true;
    });
    window.showQtedmExtensionProperties(widget);
    QVERIFY(accepted);
    QVERIFY(!window.isDirty());
  }
  auto *text = findTypedWidget<TextElement>(window);
  QVERIFY(text);
  bool accepted = false;
  QTimer::singleShot(0, &window, [&] {
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    const auto close = qScopeGuard([&] {
      if (dialog->isVisible()) dialog->reject();
    });
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    accepted = true;
  });
  window.showPropertyRuleEditor(text);
  QVERIFY(accepted);
  QVERIFY(!window.isDirty());
}

void TestExtensionResourcePalettes::arcColorPickerIdentifiesSelection()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  auto *arc = new ArcElement(window.centralWidget());
  window.selectAndScrollToWidget(arc);
  auto *palette = window.findChild<QWidget *>(QStringLiteral("qtedmResourcePalette"));
  QVERIFY(palette);
  QPushButton *color = nullptr;
  for (auto *button : palette->findChildren<QPushButton *>())
    if (button->isVisible() && button->accessibleName().startsWith(QStringLiteral("Color:"))) color = button;
  QVERIFY(color);
  color->click();
  auto *picker = window.findChild<QWidget *>(QStringLiteral("qtedmColorPalette"));
  QVERIFY(picker);
  bool found = false;
  for (auto *label : picker->findChildren<QLabel *>())
    found |= label->text().startsWith(QStringLiteral("Arc Color:"));
  QVERIFY(found);
}

void TestExtensionResourcePalettes::textFieldsUseAvailablePaletteWidth()
{
  ResourcePaletteDialog dialog(QApplication::palette(), QFont(), QFont());
  ResourcePaletteProperty channel;
  channel.key = QStringLiteral("channel");
  channel.label = QStringLiteral("Data PV");
  channel.value = QStringLiteral(
      "pva://qtedm:test:visual:missing:NTNDArray");
  channel.setter = [](const QVariant &) {};
  dialog.showForExtension([]() { return QRect(0, 0, 100, 100); },
      [](const QRect &) {}, QStringLiteral("Width Test"), {channel});
  QCoreApplication::processEvents();
  auto *field = dialog.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_channel"));
  QVERIFY(field);

  dialog.resize(500, dialog.height());
  QCoreApplication::processEvents();
  const int compactWidth = field->width();
  dialog.resize(900, dialog.height());
  QCoreApplication::processEvents();
  QVERIFY(field->width() >= compactWidth + 350);
  QVERIFY(field->width() >
      field->fontMetrics().horizontalAdvance(field->text()) + 16);
  dialog.resize(500, dialog.height());
  QCoreApplication::processEvents();
  QCOMPARE(field->width(), compactWidth);
  QVERIFY(field->isVisible());
}

void TestExtensionResourcePalettes::
    ntNdArrayRangePalettePreservesFiniteValues_data()
{
  QTest::addColumn<double>("minimum");
  QTest::addColumn<double>("maximum");
  QTest::newRow("reported-range") << -1.0e-9 << 2.0e12;
  QTest::newRow("large-range") << -2.5e200 << 3.0e200;
  QTest::newRow("small-range") << -1.0e-100 << 2.0e-100;
}

void TestExtensionResourcePalettes::
    ntNdArrayRangePalettePreservesFiniteValues()
{
  QFETCH(double, minimum);
  QFETCH(double, maximum);
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  auto *image = new NtNdArrayImageElement(window.centralWidget());
  image->setRangeMode(HeatmapRangeMode::kManual);
  image->setRangeMinimum(minimum);
  image->setRangeMaximum(maximum);
  window.selectAndScrollToWidget(image);
  auto *minimumField = window.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_range_minimum"));
  auto *maximumField = window.findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_range_maximum"));
  QVERIFY(minimumField);
  QVERIFY(maximumField);
  QVERIFY(minimumField->text().toDouble() == minimum);
  QVERIFY(maximumField->text().toDouble() == maximum);
  QVERIFY(image->rangeMinimum() == minimum);
  QVERIFY(image->rangeMaximum() == maximum);

  // Committing the displayed values must not round or clamp the model.
  QMetaObject::invokeMethod(minimumField, "editingFinished",
      Qt::DirectConnection);
  QMetaObject::invokeMethod(maximumField, "editingFinished",
      Qt::DirectConnection);
  QVERIFY(image->rangeMinimum() == minimum);
  QVERIFY(image->rangeMaximum() == maximum);

  minimumField->setText(QStringLiteral("-2.5e-9"));
  maximumField->setText(QStringLiteral("3.5e100"));
  QMetaObject::invokeMethod(minimumField, "editingFinished",
      Qt::DirectConnection);
  QMetaObject::invokeMethod(maximumField, "editingFinished",
      Qt::DirectConnection);
  QCOMPARE(image->rangeMinimum(), -2.5e-9);
  QCOMPARE(image->rangeMaximum(), 3.5e100);

  minimumField->setText(QStringLiteral("nan"));
  maximumField->setText(QStringLiteral("inf"));
  QMetaObject::invokeMethod(minimumField, "editingFinished",
      Qt::DirectConnection);
  QMetaObject::invokeMethod(maximumField, "editingFinished",
      Qt::DirectConnection);
  QCOMPARE(minimumField->text().toDouble(), -2.5e-9);
  QCOMPARE(maximumField->text().toDouble(), 3.5e100);
  QCOMPARE(image->rangeMinimum(), -2.5e-9);
  QCOMPARE(image->rangeMaximum(), 3.5e100);
}

void TestExtensionResourcePalettes::
    numericEditorsPreserveValuesAndRejectInvalidInput()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  auto *spin = new SetpointControlElement(window.centralWidget());
  spin->setQtedmSpinBox(true);
  for (double value : {1e-15, 2e12, 1e100}) {
    spin->setStepSize(value);
    window.selectAndScrollToWidget(spin);
    auto *edit = window.findChild<QLineEdit *>(
        QStringLiteral("qtedmResourceProperty_step_size"));
    QVERIFY(edit);
    QVERIFY(edit->text().toDouble() == value);
    for (const QString &invalid : {QStringLiteral("0"),
             QStringLiteral("-1"), QStringLiteral("nan")}) {
      edit->setText(invalid);
      QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
      QVERIFY(spin->stepSize() == value);
      QVERIFY(edit->text().toDouble() == value);
    }
    edit->setText(QStringLiteral("3e-14"));
    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    QVERIFY(spin->stepSize() == 3e-14);
  }
  auto *expression = new ExpressionChannelElement(window.centralWidget());
  for (double value : {1e-15, 1e-13, 1e-7, -2e12, 1e100}) {
    expression->setInitialValue(value);
    window.selectAndScrollToWidget(expression);
    auto *edit = window.findChild<QLineEdit *>(
        QStringLiteral("qtedmExpressionInitialValue"));
    QVERIFY(edit);
    QVERIFY(edit->text().toDouble() == value);
    edit->setText(QStringLiteral("inf"));
    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    QVERIFY(expression->initialValue() == value);
    QVERIFY(edit->text().toDouble() == value);
    edit->setText(QStringLiteral("2e-7"));
    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    QVERIFY(expression->initialValue() == 2e-7);
  }
}

void TestExtensionResourcePalettes::numericValuesSurviveSaveAndReload_data()
{
  QTest::addColumn<bool>("nested");
  QTest::newRow("top-level") << false;
  QTest::newRow("composite") << true;
}

void TestExtensionResourcePalettes::numericValuesSurviveSaveAndReload()
{
  QFETCH(bool, nested);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("precision.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  const QByteArray elements =
      "qtedm_spinbox { object { x=10 y=10 width=250 height=40 } "
      "step=1.0000000000000002 }\n"
      "expression_channel { object { x=10 y=70 width=120 height=40 } "
      "variable=expr:tiny initialValue=1e-15 }\n"
      "heatmap { object { x=300 y=10 width=200 height=150 } "
      "rangeMode=manual rangeMin=1.234567890123 rangeMax=12.345678901234 }\n"
      "valuator { object { x=10 y=140 width=250 height=40 } "
      "dPrecision=0.123456789 }\n";
  file.write("file { version=030122 }\n"
      "display { object { width=600 height=350 } }\n");
  if (nested) file.write("composite { object { width=600 height=350 } children {\n");
  file.write(elements);
  if (nested) file.write("} }\n");
  file.close();

  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  QVERIFY(!window.isDirty());
  QMap<QString, double> expected{
      {QStringLiteral("Step Size"), 1.0000000000000002},
      {QStringLiteral("Initial Value"), 1e-15},
      {QStringLiteral("Range Minimum"), 1.234567890123},
      {QStringLiteral("Range Maximum"), 12.345678901234},
      {QStringLiteral("Increment"), 0.123456789}};
  QList<QWidget *> targets;
  for (auto *widget : window.findChildren<QWidget *>()) {
    if (dynamic_cast<SetpointControlElement *>(widget)
        || dynamic_cast<ExpressionChannelElement *>(widget)
        || dynamic_cast<HeatmapElement *>(widget)
        || dynamic_cast<SliderElement *>(widget)) targets.append(widget);
  }
  QCOMPARE(targets.size(), 4);
  int inspected = 0;
  for (auto *target : targets) {
    window.selectAndScrollToWidget(target);
    QCoreApplication::processEvents();
    for (auto *edit : window.findChildren<QLineEdit *>()) {
      if (!edit->isVisible() || !expected.contains(edit->accessibleName())) continue;
      const double value = expected.value(edit->accessibleName());
      QVERIFY(edit->text().toDouble() == value);
      QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
      QVERIFY2(!window.isDirty(), qPrintable(edit->accessibleName()));
      QTest::keyClick(edit, Qt::Key_Return);
      QVERIFY2(!window.isDirty(), qPrintable(edit->accessibleName()));
      ++inspected;
    }
  }
  QCOMPARE(inspected, expected.size());
  const QString saved = directory.filePath(QStringLiteral("saved.adl"));
  QVERIFY(window.saveToPath(saved));
  DisplayWindow reopened(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  QVERIFY2(reopened.loadFromFile(saved, &error), qPrintable(error));
  int checked = 0;
  for (auto *widget : reopened.findChildren<QWidget *>()) {
    if (auto *spin = dynamic_cast<SetpointControlElement *>(widget)) {
      QVERIFY(spin->stepSize() == 1.0000000000000002);
      ++checked;
    } else if (auto *expression = dynamic_cast<ExpressionChannelElement *>(widget)) {
      QVERIFY(expression->initialValue() == 1e-15);
      ++checked;
    } else if (auto *image = dynamic_cast<HeatmapElement *>(widget)) {
      QVERIFY(image->rangeMinimum() == 1.234567890123);
      QVERIFY(image->rangeMaximum() == 12.345678901234);
      ++checked;
    } else if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      QVERIFY(slider->increment() == 0.123456789);
      ++checked;
    }
  }
  QCOMPARE(checked, 4);
}

void TestExtensionResourcePalettes::configuredNumericValuesSurviveEditingAndReload_data()
{
  QTest::addColumn<QString>("kind");
  QTest::addColumn<bool>("nested");
  for (const QString &kind : {QStringLiteral("heatmap"),
           QStringLiteral("qtedm_ndarray_image"), QStringLiteral("strip chart"),
           QStringLiteral("setpoint_control"), QStringLiteral("qtedm_spinbox"),
           QStringLiteral("waterfall_plot")}) {
    QTest::newRow(qPrintable(kind + QStringLiteral("-top-level"))) << kind << false;
    QTest::newRow(qPrintable(kind + QStringLiteral("-composite"))) << kind << true;
  }
}

void TestExtensionResourcePalettes::configuredNumericValuesSurviveEditingAndReload()
{
  QFETCH(QString, kind);
  QFETCH(bool, nested);
  QMap<QString, double> expected;
  QString attributes;
  if (kind == QStringLiteral("heatmap") || kind == QStringLiteral("qtedm_ndarray_image")) {
    const double maximum = kind == QStringLiteral("heatmap")
        ? 1.0000000000000002 : 255.00000000000003;
    attributes = QStringLiteral("rangeMode=manual rangeMin=0.5000000000000001 rangeMax=%1")
        .arg(QString::number(maximum, 'g', 17));
    expected = {{QStringLiteral("Range Minimum"), 0.5000000000000001},
        {QStringLiteral("Range Maximum"), maximum}};
  } else if (kind == QStringLiteral("strip chart")) {
    attributes = QStringLiteral("period=1.2345678901234567");
    expected = {{QStringLiteral("Period"), 1.2345678901234567}};
  } else if (kind == QStringLiteral("waterfall_plot")) {
    attributes = QStringLiteral("intensityScale=manual intensityMin=1.2345678901234567 "
        "intensityMax=2.3456789012345678 samplePeriod=1.2345678901234567");
    expected = {{QStringLiteral("Intensity Minimum"), 1.2345678901234567},
        {QStringLiteral("Intensity Maximum"), 2.3456789012345678},
        {QStringLiteral("Sample Period"), 1.2345678901234567}};
  } else {
    const double tolerance = kind == QStringLiteral("setpoint_control")
        ? 1e-13 : 1.2345678901234567;
    attributes = QStringLiteral("toleranceMode=absolute tolerance=%1")
        .arg(QString::number(tolerance, 'g', 17));
    expected = {{QStringLiteral("Tolerance"), tolerance}};
  }
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("configured.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("file { version=030122 }\ndisplay { object { width=600 height=350 } }\n");
  if (nested) file.write("composite { object { width=600 height=350 } children {\n");
  file.write(QStringLiteral("\"%1\" { object { x=10 y=10 width=300 height=150 } %2 }\n")
      .arg(kind, attributes).toUtf8());
  if (nested) file.write("} }\n");
  file.close();

  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto target = [](DisplayWindow &display) -> QWidget * {
    for (auto *widget : display.findChildren<QWidget *>()) {
      if (dynamic_cast<HeatmapElement *>(widget)
          || dynamic_cast<StripChartElement *>(widget)
          || dynamic_cast<SetpointControlElement *>(widget)
          || dynamic_cast<WaterfallPlotElement *>(widget)) return widget;
    }
    return nullptr;
  };
  auto values = [](QWidget *widget) -> QMap<QString, double> {
    if (auto *image = dynamic_cast<HeatmapElement *>(widget)) {
      return {{QStringLiteral("Range Minimum"), image->rangeMinimum()},
          {QStringLiteral("Range Maximum"), image->rangeMaximum()}};
    }
    if (auto *strip = dynamic_cast<StripChartElement *>(widget))
      return {{QStringLiteral("Period"), strip->period()}};
    if (auto *setpoint = dynamic_cast<SetpointControlElement *>(widget))
      return {{QStringLiteral("Tolerance"), setpoint->tolerance()}};
    if (auto *waterfall = dynamic_cast<WaterfallPlotElement *>(widget)) {
      return {{QStringLiteral("Intensity Minimum"), waterfall->intensityMin()},
          {QStringLiteral("Intensity Maximum"), waterfall->intensityMax()},
          {QStringLiteral("Sample Period"), waterfall->samplePeriod()}};
    }
    return {};
  };
  QWidget *widget = target(window);
  QVERIFY(widget);
  QVERIFY(values(widget) == expected);
  window.selectAndScrollToWidget(widget);
  QCoreApplication::processEvents();
  int inspected = 0;
  for (auto *edit : window.findChildren<QLineEdit *>()) {
    const QString name = edit->accessibleName();
    if (!edit->isVisible() || !expected.contains(name)) continue;
    const double value = expected.value(name);
    QVERIFY2(edit->isEnabled(), qPrintable(name));
    QVERIFY2(edit->text().toDouble() == value, qPrintable(name));
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY2(!window.isDirty(), qPrintable(name));
    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    QVERIFY2(!window.isDirty(), qPrintable(name));
    QVERIFY(values(widget) == expected);
    ++inspected;
  }
  QCOMPARE(inspected, expected.size());
  // A one-representable-step change must apply, including tiny tolerances.
  for (auto *edit : window.findChildren<QLineEdit *>()) {
    const QString name = edit->accessibleName();
    if (!edit->isVisible() || !expected.contains(name)) continue;
    const double value = expected.value(name);
    const double changed = std::nextafter(value, std::numeric_limits<double>::infinity());
    edit->setText(QString::number(changed, 'g', 17));
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY(window.isDirty());
    QVERIFY2(values(widget).value(name) == changed, qPrintable(name));
    edit->setText(QString::number(value, 'g', 17));
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY(values(widget) == expected);
    edit->setText(QStringLiteral("nan"));
    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    QVERIFY(values(widget) == expected);
    QVERIFY(edit->text().toDouble() == value);
  }
  // Rebuilding the palette must show the committed values.
  window.selectAndScrollToWidget(new TextElement(window.centralWidget()));
  window.selectAndScrollToWidget(widget);
  for (auto *edit : window.findChildren<QLineEdit *>()) {
    if (edit->isVisible() && expected.contains(edit->accessibleName()))
      QVERIFY(edit->text().toDouble() == expected.value(edit->accessibleName()));
  }
  const QString saved = directory.filePath(QStringLiteral("saved.adl"));
  QVERIFY(window.saveToPath(saved));
  DisplayWindow reopened(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QVERIFY2(reopened.loadFromFile(saved, &error), qPrintable(error));
  QVERIFY(target(reopened));
  QVERIFY(values(target(reopened)) == expected);
}

void TestExtensionResourcePalettes::cartesianTraceControlsIdentifyTheirRow()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  auto *plot = new CartesianPlotElement(window.centralWidget());
  for (int pass = 0; pass < 2; ++pass) {
    window.selectAndScrollToWidget(plot);
    auto *palette = window.findChild<QWidget *>(QStringLiteral("qtedmResourcePalette"));
    QVERIFY(palette);
    for (int i = 1; i <= 8; ++i) {
      const QString role = QStringLiteral("Trace %1 Color").arg(i);
      int colors = 0;
      for (auto *button : palette->findChildren<QPushButton *>()) {
        if (button->accessibleName().startsWith(role + QStringLiteral(": #"))) {
          QVERIFY(button->accessibleName().endsWith(button->text()));
          ++colors;
        }
      }
      QCOMPARE(colors, 1);
      for (const QString &field : {QStringLiteral("Y Axis"), QStringLiteral("Side")}) {
        int matches = 0;
        for (auto *combo : palette->findChildren<QComboBox *>()) {
          const QString role = QStringLiteral("Trace %1 %2").arg(i).arg(field);
          if (combo->accessibleName() == role) {
            QCOMPARE(combo->accessibleDescription(), role);
            QCOMPARE(combo->toolTip(), role);
            ++matches;
          }
        }
        QCOMPARE(matches, 1);
      }
    }
    window.selectAndScrollToWidget(new TextElement(window.centralWidget()));
    plot->setTraceColor(0, Qt::red);
  }
}

void TestExtensionResourcePalettes::savedRangesAndSymbolStatesStayExact_data()
{
  QTest::addColumn<bool>("nested");
  QTest::addColumn<bool>("nearDefault");
  QTest::newRow("top-level") << false << false;
  QTest::newRow("composite") << true << false;
  QTest::newRow("near-default-top-level") << false << true;
  QTest::newRow("near-default-composite") << true << true;
}

void TestExtensionResourcePalettes::savedRangesAndSymbolStatesStayExact()
{
  QFETCH(bool, nested);
  QFETCH(bool, nearDefault);
  const double low = nearDefault ? 1e-13 : 1e-10;
  const double high = nearDefault ? 1.0000000000000002 : 1.23456789;
  const double axisLow = nearDefault ? 1e-13 : 1e-7;
  const double stateHigh = nearDefault ? 1.2345678901234567 : 1.0000000000000002;
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("precise.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("file { version=030122 } display { object { width=700 height=400 } }\n");
  if (nested) file.write("composite { object { width=700 height=400 } children {\n");
  const QString elements = QStringLiteral(
      "valuator { object { x=10 y=10 width=250 height=40 } limits { "
      "loprSrc=default loprDefault=%1 hoprSrc=default hoprDefault=%2 } }\n"
      "\"cartesian plot\" { object { x=300 y=10 width=300 height=200 } "
      "x_axis { rangeStyle=user-specified minRange=%3 maxRange=%2 } }\n"
      "qtedm_symbol { object { x=10 y=90 width=150 height=100 } stateCount=1 "
      "state { min=0 max=%4 label=First color=20 } }\n")
      .arg(QString::number(low, 'g', 17), QString::number(high, 'g', 17),
          QString::number(axisLow, 'g', 17), QString::number(stateHigh, 'g', 17));
  file.write(elements.toUtf8());
  if (nested) file.write("} }\n");
  file.close();
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *symbol = findTypedWidget<LedMonitorElement>(window);
  QVERIFY(symbol);
  QVERIFY(symbol->symbolStates().first().maximum == stateHigh);
  window.selectAndScrollToWidget(symbol);
  auto *action = window.findChild<QPushButton *>(QStringLiteral("qtedmResourceProperty_symbol_states"));
  QVERIFY(action);
  bool inspected = false;
  QTimer::singleShot(0, &window, [&]() {
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    const auto close = qScopeGuard([&]() { dialog->reject(); });
    auto *table = dialog->findChild<QTableWidget *>();
    QVERIFY(table);
    QVERIFY(table->item(0, 1)->text().toDouble() == stateHigh);
    inspected = true;
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
  });
  QTest::keyClick(action, Qt::Key_Space);
  QVERIFY(inspected);
  QVERIFY(!window.isDirty());
  QVERIFY(symbol->symbolStates().first().maximum == stateHigh);
  const QString saved = dir.filePath(QStringLiteral("saved.adl"));
  QVERIFY(window.saveToPath(saved));
  DisplayWindow reopened(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QVERIFY2(reopened.loadFromFile(saved, &error), qPrintable(error));
  auto *slider = findTypedWidget<SliderElement>(reopened);
  auto *plot = findTypedWidget<CartesianPlotElement>(reopened);
  auto *reloadedSymbol = findTypedWidget<LedMonitorElement>(reopened);
  QVERIFY(slider && plot && reloadedSymbol);
  QVERIFY(slider->limits().lowDefault == low);
  QVERIFY(slider->limits().highDefault == high);
  QVERIFY(plot->axisMinimum(0) == axisLow);
  QVERIFY(plot->axisMaximum(0) == high);
  QVERIFY(reloadedSymbol->symbolStates().first().maximum == stateHigh);
}

void TestExtensionResourcePalettes::entryConfirmationsStayClean()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("entries.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("file { version=030122 } display { object { width=700 height=400 } }\n"
      "pv_table { object { x=10 y=10 width=300 height=150 } "
      "row { label=First } row { label=Second } }\n"
      "\"shell command\" { object { x=320 y=10 width=250 height=40 } "
      "command[0] { label=Example name=echo } }\n"
      "\"related display\" { object { x=320 y=80 width=250 height=40 } "
      "display[0] { name=example.adl } }\n");
  file.close();
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *pv = findTypedWidget<PvTableElement>(window);
  auto *shell = findTypedWidget<ShellCommandElement>(window);
  auto *related = findTypedWidget<RelatedDisplayElement>(window);
  QVERIFY(pv && shell && related);
  for (QWidget *target : {static_cast<QWidget *>(pv), static_cast<QWidget *>(shell),
           static_cast<QWidget *>(related)}) {
    window.selectAndScrollToWidget(target);
    QVERIFY(!window.isDirty());
    int checked = 0;
    for (auto *edit : window.findChildren<QLineEdit *>()) {
      if (!edit->isVisible()) continue;
      const QString name = edit->accessibleName();
      if (!(name.startsWith(QStringLiteral("PV Table Row "))
              || name.startsWith(QStringLiteral("Command "))
              || name.startsWith(QStringLiteral("Display ")))) continue;
      QTest::keyClick(edit, Qt::Key_Return);
      QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
      QVERIFY2(!window.isDirty(), qPrintable(name));
      ++checked;
    }
    QCOMPARE(checked, target == pv ? 16 : 48);
    if (target == related) {
      int policies = 0;
      for (auto *combo : window.findChildren<QComboBox *>()) {
        if (!combo->isVisible() || !combo->accessibleName().endsWith(QStringLiteral(" Policy"))) continue;
        QCOMPARE(combo->accessibleDescription(), combo->accessibleName());
        ++policies;
      }
      QCOMPARE(policies, 16);
    }
  }
  QCOMPARE(pv->rowCount(), 2);
  window.selectAndScrollToWidget(pv);
  QLineEdit *last = nullptr;
  for (auto *edit : window.findChildren<QLineEdit *>()) {
    if (edit->accessibleName() == QStringLiteral("PV Table Row 8 Label")) last = edit;
  }
  QVERIFY(last);
  last->setText(QStringLiteral("Eighth"));
  QTest::keyClick(last, Qt::Key_Return);
  QVERIFY(window.isDirty());
  QCOMPARE(pv->rowCount(), 8);
  QCOMPARE(pv->rows().at(7).label, QStringLiteral("Eighth"));
}

void TestExtensionResourcePalettes::secondaryDialogsStayCleanAndIdentifyFields()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("dialogs.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("file { version=030122 } display { object { width=700 height=400 } }\n"
      "\"cartesian plot\" { object { x=10 y=10 width=300 height=200 } "
      "x_axis { rangeStyle=user-specified minRange=1e-7 maxRange=1.23456789 } }\n"
      "valuator { object { x=340 y=10 width=300 height=55 } "
      "limits { loprSrc=default loprDefault=1e-10 hoprSrc=default "
      "hoprDefault=1.23456789 precSrc=default precDefault=3 } }\n");
  file.close();
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *plot = findTypedWidget<CartesianPlotElement>(window);
  auto *slider = findTypedWidget<SliderElement>(window);
  QVERIFY(plot && slider);
  window.selectAndScrollToWidget(plot);
  QPushButton *axisButton = nullptr;
  for (auto *button : window.findChildren<QPushButton *>())
    if (button->accessibleName() == QStringLiteral("Axis Data")) axisButton = button;
  QVERIFY(axisButton);
  QVERIFY(!window.isDirty());
  axisButton->click();
  auto *axis = findTypedWidget<CartesianAxisDialog>(window);
  QVERIFY(axis);
  int checked = 0;
  for (auto *edit : axis->findChildren<QLineEdit *>()) {
    QVERIFY(!edit->accessibleName().isEmpty());
    QCOMPARE(edit->accessibleDescription(), edit->accessibleName());
    QVERIFY(edit->isEnabled());
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY(!window.isDirty());
    ++checked;
  }
  QCOMPARE(checked, 2);
  for (auto *combo : axis->findChildren<QComboBox *>()) {
    QVERIFY(!combo->accessibleName().isEmpty());
    QCOMPARE(combo->accessibleDescription(), combo->accessibleName());
  }
  axis->close();
  window.selectAndScrollToWidget(slider);
  QPushButton *limitsButton = nullptr;
  for (auto *button : window.findChildren<QPushButton *>())
    if (button->isVisible() && button->accessibleName() == QStringLiteral("Channel Limits")) limitsButton = button;
  QVERIFY(limitsButton);
  limitsButton->click();
  auto *dialog = findTypedWidget<PvLimitsDialog>(window);
  QVERIFY(dialog);
  checked = 0;
  for (auto *edit : dialog->findChildren<QLineEdit *>()) {
    QVERIFY(!edit->accessibleName().isEmpty());
    QCOMPARE(edit->accessibleDescription(), edit->accessibleName());
    QVERIFY(edit->isEnabled());
    QTest::keyClick(edit, Qt::Key_Return);
    QVERIFY(!window.isDirty());
    ++checked;
  }
  QCOMPARE(checked, 3);
  for (auto *combo : dialog->findChildren<QComboBox *>()) {
    QVERIFY(!combo->accessibleName().isEmpty());
    QCOMPARE(combo->accessibleDescription(), combo->accessibleName());
  }
}

void TestExtensionResourcePalettes::narrowEntryPalettesKeepColumnsReachable()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  window.selectAndScrollToWidget(new ShellCommandElement(window.centralWidget()));
  auto *palette = window.findChild<QWidget *>(QStringLiteral("qtedmResourcePalette"));
  QVERIFY(palette);
  QCoreApplication::processEvents();
  palette->resize(215, palette->height());
  QCoreApplication::processEvents();
  auto *scroll = palette->findChild<QScrollArea *>();
  QVERIFY(scroll);
  QTRY_VERIFY(scroll->horizontalScrollBar()->maximum() > 0);
  QVERIFY(scroll->horizontalScrollBar()->isVisible());
  scroll->horizontalScrollBar()->setValue(scroll->horizontalScrollBar()->maximum());
  QLineEdit *args = nullptr;
  for (auto *edit : palette->findChildren<QLineEdit *>())
    if (edit->accessibleName() == QStringLiteral("Command 1 Arguments")) args = edit;
  QVERIFY(args);
  QVERIFY(scroll->viewport()->rect().contains(
      args->mapTo(scroll->viewport(), args->rect().center())));
}

void TestExtensionResourcePalettes::unchangedAttributeEditsStayClean_data()
{
  QTest::addColumn<QString>("kind");
  QTest::addColumn<QString>("attributes");
  for (const QString &kind : {QStringLiteral("text"), QStringLiteral("text entry"),
           QStringLiteral("text update"), QStringLiteral("meter"), QStringLiteral("bar"),
           QStringLiteral("thermometer"), QStringLiteral("indicator"), QStringLiteral("byte"),
           QStringLiteral("led_monitor"), QStringLiteral("wheel switch"),
           QStringLiteral("strip chart"), QStringLiteral("message button"),
           QStringLiteral("text_area"), QStringLiteral("wave_table"),
           QStringLiteral("choice button"), QStringLiteral("menu"), QStringLiteral("valuator"),
           QStringLiteral("expression_channel"), QStringLiteral("rectangle"),
           QStringLiteral("oval"), QStringLiteral("polyline"), QStringLiteral("heatmap"),
           QStringLiteral("waterfall_plot"), QStringLiteral("cartesian plot")}) {
    QString attributes;
    if (kind == QStringLiteral("text")) attributes = QStringLiteral("textix=Example");
    if (kind == QStringLiteral("polyline"))
      attributes = QStringLiteral("points { (10,10) (310,190) }");
    if (kind == QStringLiteral("strip chart"))
      attributes = QStringLiteral("pen[0] { chan=local:test:pen clr=20 }");
    if (kind == QStringLiteral("led_monitor"))
      attributes = QStringLiteral("colorMode=discrete stateCount=4");
    QTest::newRow(qPrintable(kind)) << kind << attributes;
  }
}

void TestExtensionResourcePalettes::unchangedAttributeEditsStayClean()
{
  QFETCH(QString, kind);
  QFETCH(QString, attributes);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("unchanged.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(QStringLiteral("file { version=030122 }\n"
      "display { object { width=700 height=450 } }\n"
      "\"%1\" { object { x=10 y=10 width=300 height=180 } %2 }\n")
      .arg(kind, attributes).toUtf8());
  file.close();
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  auto *area = window.findChild<QWidget *>(QStringLiteral("displayArea"));
  QVERIFY(area);
  const auto targets = area->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
  // Text has a separate overflow-painting sibling in the display area.
  QWidget *target = kind == QStringLiteral("text")
      ? findTypedWidget<TextElement>(window)
      : (targets.size() == 1 ? targets.first() : nullptr);
  QVERIFY(target);
  const QRect original = target->geometry();
  window.selectAndScrollToWidget(target);
  QCoreApplication::processEvents();
  QVERIFY(!window.isDirty());
  auto *palette = window.findChild<QWidget *>(QStringLiteral("qtedmResourcePalette"));
  QVERIFY(palette);
  QLineEdit *xEdit = nullptr;
  int checked = 0;
  for (auto *edit : palette->findChildren<QLineEdit *>()) {
    const QString name = edit->accessibleName();
    if (!edit->isVisible() || !edit->isEnabled() || name.isEmpty()) continue;
    QTest::keyClick(edit, Qt::Key_Return);
    QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    QVERIFY2(!window.isDirty(), qPrintable(kind + QStringLiteral(": ") + name));
    if (name == QStringLiteral("X Position")) xEdit = edit;
    ++checked;
  }
  QVERIFY(checked >= 4);
  QCOMPARE(target->geometry(), original);
  QVERIFY(xEdit);
  // A real geometry change must still apply and dirty the document.
  xEdit->setText(QString::number(original.x() + 1));
  QTest::keyClick(xEdit, Qt::Key_Return);
  QCOMPARE(target->geometry().x(), original.x() + 1);
  QVERIFY(window.isDirty());
}

void TestExtensionResourcePalettes::penAndLedColorsIdentifyTheirIndex()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  auto *strip = new StripChartElement(window.centralWidget());
  auto *led = new LedMonitorElement(window.centralWidget());
  led->setColorMode(TextColorMode::kDiscrete);
  led->setStateCount(4);
  for (int pass = 0; pass < 2; ++pass) {
    window.selectAndScrollToWidget(strip);
    auto *palette = window.findChild<QWidget *>(QStringLiteral("qtedmResourcePalette"));
    QVERIFY(palette);
    for (int i = 1; i <= 8; ++i) {
      const QString prefix = QStringLiteral("Pen %1 ").arg(i);
      int found = 0;
      for (auto *field : palette->findChildren<QWidget *>()) {
        if (field->accessibleName() == prefix + QStringLiteral("Channel")) ++found;
        if (field->accessibleName() == prefix + QStringLiteral("Limits")) {
          QCOMPARE(field->accessibleDescription(), prefix + QStringLiteral("Limits"));
          ++found;
        }
        if (field->accessibleName().startsWith(prefix + QStringLiteral("Color: #"))) {
          auto *button = qobject_cast<QPushButton *>(field);
          QVERIFY(button);
          QVERIFY(button->accessibleName().endsWith(button->text()));
          ++found;
        }
      }
      QCOMPARE(found, 3);
    }
    window.selectAndScrollToWidget(led);
    for (int i = 0; i < 16; ++i) {
      int found = 0;
      for (auto *button : palette->findChildren<QPushButton *>()) {
        if (button->accessibleName().startsWith(QStringLiteral("State %1 Color: #").arg(i))) {
          QVERIFY(button->accessibleName().endsWith(button->text()));
          ++found;
        }
      }
      QCOMPARE(found, 1);
    }
    // Refreshing the palette after a color change must retain its index label.
    strip->setPenColor(0, Qt::red);
    led->setStateColor(0, Qt::red);
  }
}

void TestExtensionResourcePalettes::ruleRateSurvivesSaveAndReload_data()
{
  QTest::addColumn<double>("rate");
  QTest::newRow("near-one") << 1.0000000000000002;
  QTest::newRow("fractional") << 1.2345678901234567;
}

void TestExtensionResourcePalettes::ruleRateSurvivesSaveAndReload()
{
  QFETCH(double, rate);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("rules.adl"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(QStringLiteral("file { version=030122 } display { object { width=600 height=400 } }\n"
      "text { object { x=10 y=10 width=200 height=50 } textix=Example }\n"
      "qtedm_rules { targetIndex=0 rule { id=visible property=visible expression=1 "
      "trueValue=true falseValue=false rateHz=%1 } }\n")
      .arg(QString::number(rate, 'g', 17)).toUtf8());
  file.close();
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QString error;
  QVERIFY2(window.loadFromFile(path, &error), qPrintable(error));
  const QString saved = dir.filePath(QStringLiteral("saved.adl"));
  QVERIFY(window.saveToPath(saved));
  DisplayWindow reopened(QApplication::palette(), QApplication::palette(), QFont(), QFont(), state);
  QVERIFY2(reopened.loadFromFile(saved, &error), qPrintable(error));
  const QString resaved = dir.filePath(QStringLiteral("resaved.adl"));
  QVERIFY(reopened.saveToPath(resaved));
  QFile result(resaved);
  QVERIFY(result.open(QIODevice::ReadOnly));
  const auto parsed = AdlParser::parse(QString::fromLatin1(result.readAll()), &error);
  QVERIFY(parsed.has_value());
  const auto *rules = ::findChild(*parsed, QStringLiteral("qtedm_rules"));
  QVERIFY(rules);
  const auto *rule = ::findChild(*rules, QStringLiteral("rule"));
  QVERIFY(rule);
  QVERIFY(propertyValue(*rule, QStringLiteral("rateHz")).toDouble() == rate);
}

void TestExtensionResourcePalettes::palettePreservesUserSize()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  auto *first = new TextElement(window.centralWidget());
  auto *second = new TextElement(window.centralWidget());
  auto *plot = new CartesianPlotElement(window.centralWidget());
  window.selectAndScrollToWidget(first);
  QCoreApplication::processEvents();
  auto *palette = window.findChild<QWidget *>(QStringLiteral("qtedmResourcePalette"));
  QVERIFY(palette);
  const QSize available = QGuiApplication::primaryScreen()->availableGeometry().size();
  const QSize preferred = (palette->size() + QSize(150, 40)).boundedTo(available);
  palette->resize(preferred);
  QCoreApplication::processEvents();
  window.selectAndScrollToWidget(second);
  QCoreApplication::processEvents();
  QCOMPARE(palette->size(), preferred);
  window.selectAndScrollToWidget(plot);
  QCoreApplication::processEvents();
  window.selectAndScrollToWidget(first);
  QCoreApplication::processEvents();
  QCOMPARE(palette->size(), preferred);
}

void TestExtensionResourcePalettes::heatmapBooleansUseCommonLabels()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  auto *image = new HeatmapElement(window.centralWidget());
  window.selectAndScrollToWidget(image);
  const QStringList labels{QStringLiteral("Invert Color Scale"),
      QStringLiteral("Preserve Aspect Ratio"), QStringLiteral("Flip Horizontal"),
      QStringLiteral("Flip Vertical"), QStringLiteral("Show Top Profile"),
      QStringLiteral("Show Right Profile")};
  int checked = 0;
  for (auto *combo : window.findChildren<QComboBox *>()) {
    if (!combo->isVisible() || !labels.contains(combo->accessibleName())) continue;
    QCOMPARE(combo->itemText(0), QStringLiteral("false"));
    QCOMPARE(combo->itemText(1), QStringLiteral("true"));
    ++checked;
  }
  QCOMPARE(checked, labels.size());
}

void TestExtensionResourcePalettes::paletteColumnsAlignAndShrinkWithSelection()
{
  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QFont(), QFont(), state);
  auto *plot = new CartesianPlotElement(window.centralWidget());
  window.selectAndScrollToWidget(plot);
  QCoreApplication::processEvents();
  auto *palette = window.findChild<QWidget *>(
      QStringLiteral("qtedmResourcePalette"));
  QVERIFY(palette);
  const int plotHeight = palette->height();
  QLineEdit *xPv = nullptr;
  QLineEdit *yPv = nullptr;
  for (auto *edit : palette->findChildren<QLineEdit *>()) {
    if (edit->accessibleName() == QStringLiteral("Trace 1 X PV")) xPv = edit;
    if (edit->accessibleName() == QStringLiteral("Trace 1 Y PV")) yPv = edit;
  }
  QVERIFY(xPv);
  QVERIFY(yPv);
  QStringList headers;
  for (auto *label : palette->findChildren<QLabel *>()) {
    if (label->isVisible()) headers.append(label->text());
  }
  QVERIFY(headers.contains(QStringLiteral("X PV")));
  QVERIFY(headers.contains(QStringLiteral("Y PV")));

  auto *image = new NtNdArrayImageElement(window.centralWidget());
  window.selectAndScrollToWidget(image);
  QCoreApplication::processEvents();
  auto *pv = palette->findChild<QLineEdit *>(
      QStringLiteral("qtedmResourceProperty_data_pv"));
  QVERIFY(pv);
  for (auto *edit : palette->findChildren<QLineEdit *>()) {
    if (edit->isVisible() && edit->accessibleName() == QStringLiteral("X Position")) {
      QCOMPARE(edit->mapTo(palette, QPoint()).x(),
          pv->mapTo(palette, QPoint()).x());
    }
  }
  auto *text = new TextElement(window.centralWidget());
  text->setText(QStringLiteral("Simple"));
  window.selectAndScrollToWidget(text);
  QCoreApplication::processEvents();
  QVERIFY(palette->height() < plotHeight);
}

void TestExtensionResourcePalettes::paletteButtonsSupportKeyboardActivation()
{
  ResourcePaletteDialog dialog(QApplication::palette(), QFont(), QFont());
  bool invoked = false;
  ResourcePaletteProperty action;
  action.key = QStringLiteral("action");
  action.label = QStringLiteral("Details");
  action.type = ResourcePalettePropertyType::kAction;
  action.action = [&]() { invoked = true; };
  ResourcePaletteProperty color;
  color.key = QStringLiteral("color");
  color.label = QStringLiteral("Color");
  color.type = ResourcePalettePropertyType::kColor;
  color.value = QColor(Qt::red);
  color.setter = [](const QVariant &) {};
  dialog.showForExtension([]() { return QRect(0, 0, 100, 50); },
      [](const QRect &) {}, QStringLiteral("Keyboard"), {action, color});
  auto *button = dialog.findChild<QPushButton *>(
      QStringLiteral("qtedmResourceProperty_action"));
  auto *colorButton = dialog.findChild<QPushButton *>(
      QStringLiteral("qtedmResourceProperty_color"));
  QVERIFY(button);
  QVERIFY(colorButton);
  QVERIFY(button->focusPolicy() & Qt::TabFocus);
  QVERIFY(colorButton->focusPolicy() & Qt::TabFocus);
  QVERIFY(colorButton->accessibleName().contains(QStringLiteral("Color")));
  QVERIFY(colorButton->accessibleName().contains(QStringLiteral("#FF0000")));
  QVERIFY(colorButton->accessibleDescription().contains(QStringLiteral("#FF0000")));
  QTest::keyClick(button, Qt::Key_Space);
  QVERIFY(invoked);
  QSignalSpy clicked(colorButton, &QPushButton::clicked);
  QTest::keyClick(colorButton, Qt::Key_Space);
  QCOMPARE(clicked.count(), 1);
}

void TestExtensionResourcePalettes::tabPageColumnsAreReadableAndResizable()
{
  TabbedDisplayElement tabs;
  const QString path = QStringLiteral("a/long/display/path/child-display.adl");
  const QString macros = QStringLiteral("TITLE=Live Overview,DETAIL=Inactive pages unsubscribe");
  tabs.setPages({{QStringLiteral("live"), QStringLiteral("Live"), path, macros, false}});
  bool inspected = false;
  QTimer::singleShot(0, &tabs, [&]() {
    auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    const auto close = qScopeGuard([&]() { dialog->reject(); });
    auto *table = dialog->findChild<QTableWidget *>(
        QStringLiteral("qtedmTabPagesTable"));
    QVERIFY(table);
    QCOMPARE(table->horizontalHeader()->sectionResizeMode(2), QHeaderView::Interactive);
    QVERIFY(table->columnWidth(2) > table->fontMetrics().horizontalAdvance(path));
    QVERIFY(table->columnWidth(3) > table->fontMetrics().horizontalAdvance(macros));
    const int width = table->columnWidth(2);
    table->horizontalHeader()->resizeSection(2, width + 80);
    QCOMPARE(table->columnWidth(2), width + 80);
    inspected = true;
  });
  tabs.showPageEditor();
  QVERIFY(inspected);
  QCOMPARE(tabs.pages().first().displayPath, path);
  QCOMPARE(tabs.pages().first().macros, macros);
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
  requireField(QStringLiteral("qtedmResourceProperty_maximum_input_bytes"));
  requireField(QStringLiteral("qtedmResourceProperty_maximum_image_bytes"));
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
