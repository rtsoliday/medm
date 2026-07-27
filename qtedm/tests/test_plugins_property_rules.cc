#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLibrary>
#include <QTemporaryDir>

#include "archive_provider.h"
#include "audit_logger.h"
#include "display_window.h"
#include "extension_object_registry.h"
#include "plugin_element.h"
#include "plugin_manager.h"
#include "property_rules.h"
#include "pv_channel_manager.h"
#include "soft_pv_registry.h"

namespace {

class FakePluginRuntime final : public QtedmPluginRuntime
{
public:
  explicit FakePluginRuntime(QtedmPluginHost *host)
    : host_(host)
  {
  }

  bool start(QString *) override
  {
    ++starts;
    started = true;
    return true;
  }

  void stop() override
  {
    if (started) {
      ++stops;
      started = false;
    }
  }

  QtedmPluginHost *host_ = nullptr;
  bool started = false;
  static int starts;
  static int stops;
};

int FakePluginRuntime::starts = 0;
int FakePluginRuntime::stops = 0;

class FakeDataSubscription final : public QtedmDataSubscription
{
public:
  explicit FakeDataSubscription(int *cancelCount)
    : cancelCount_(cancelCount)
  {
  }

  void cancel() override
  {
    if (!cancelled_) {
      cancelled_ = true;
      ++*cancelCount_;
    }
  }

private:
  int *cancelCount_ = nullptr;
  bool cancelled_ = false;
};

class FakeArchiveProvider final : public QObject, public ArchiveProvider
{
public:
  explicit FakeArchiveProvider(QObject *parent)
    : QObject(parent)
  {
  }

  ArchiveRequest *query(const ArchiveQuery &, QObject *owner,
      Completion completion) override
  {
    auto *request = new ArchiveRequest(owner);
    ArchiveResult result;
    result.samples.append({1000, 4.5, 0, 0});
    completion(result);
    return request;
  }
};

class FakePlugin final : public QObject,
    public QtedmDisplayObjectPluginInterface,
    public QtedmDataProviderPluginInterface,
    public QtedmArchiveProviderPluginInterface
{
  Q_OBJECT
  Q_INTERFACES(QtedmDisplayObjectPluginInterface
      QtedmDataProviderPluginInterface
      QtedmArchiveProviderPluginInterface)

public:
  explicit FakePlugin(QString id = QStringLiteral("org.qtedm.tests.fake"))
    : id_(std::move(id))
  {
  }

  QString pluginId() const override { return id_; }
  QtedmPluginCompatibility compatibility() const override
  {
    return compatibility_;
  }

  QVector<QtedmDisplayObjectType> objectTypes() const override
  {
    QtedmDisplayObjectType descriptor;
    descriptor.typeId = typeId_;
    descriptor.displayName = QStringLiteral("Fake Label");
    descriptor.category = QStringLiteral("Tests");
    descriptor.defaultSize = QSize(140, 40);
    descriptor.properties = {
        {QStringLiteral("text"), QStringLiteral("Text"), QString(),
            QtedmPluginPropertyType::kString,
            QStringLiteral("Default"), true},
        {QStringLiteral("channel"), QStringLiteral("Channel"), QString(),
            QtedmPluginPropertyType::kString, QString(), false},
        {QStringLiteral("color"), QStringLiteral("Color"), QString(),
            QtedmPluginPropertyType::kColor,
            QColor(QStringLiteral("#123456")), false},
    };
    return {descriptor};
  }

  QWidget *createWidget(const QString &, QWidget *parent,
      QString *error) override
  {
    if (failConstruction_) {
      if (error) {
        *error = QStringLiteral("intentional construction failure");
      }
      return nullptr;
    }
    return new QLabel(parent);
  }

  bool applyProperties(QWidget *widget, const QString &,
      const QVariantMap &properties, QString *error) override
  {
    auto *label = qobject_cast<QLabel *>(widget);
    if (!label) {
      if (error) {
        *error = QStringLiteral("expected QLabel");
      }
      return false;
    }
    label->setText(properties.value(QStringLiteral("text")).toString());
    label->setProperty("_testPluginChannel",
        properties.value(QStringLiteral("channel")));
    const QColor color =
        properties.value(QStringLiteral("color")).value<QColor>();
    if (color.isValid()) {
      QPalette palette = label->palette();
      palette.setColor(QPalette::WindowText, color);
      label->setPalette(palette);
    }
    return true;
  }

  QVariantMap serializeProperties(QWidget *widget,
      const QString &) const override
  {
    const auto *label = qobject_cast<QLabel *>(widget);
    return {
        {QStringLiteral("text"), label ? label->text() : QString()},
        {QStringLiteral("channel"),
            label ? label->property("_testPluginChannel") : QVariant()},
        {QStringLiteral("color"),
            label ? label->palette().color(QPalette::WindowText)
                  : QColor(QStringLiteral("#123456"))},
    };
  }

  QStringList channels(const QString &,
      const QVariantMap &properties) const override
  {
    const QString channel =
        properties.value(QStringLiteral("channel")).toString().trimmed();
    return channel.isEmpty() ? QStringList() : QStringList{channel};
  }

  QtedmPluginRuntime *createRuntime(QWidget *, const QString &,
      QtedmPluginHost *host, QString *) override
  {
    return new FakePluginRuntime(host);
  }

  QStringList schemes() const override
  {
    return {scheme_};
  }

  std::unique_ptr<QtedmDataSubscription> subscribe(const QString &,
      const QtedmChannelCallbacks &callbacks, QtedmChannelDelivery,
      QString *) override
  {
    QtedmChannelSample sample;
    sample.connected = true;
    sample.canRead = true;
    sample.canWrite = true;
    sample.hasValue = true;
    sample.isNumeric = true;
    sample.numericValue = 8.25;
    if (callbacks.connection) {
      callbacks.connection(true, sample);
    }
    if (callbacks.accessRights) {
      callbacks.accessRights(true, true);
    }
    if (callbacks.value) {
      callbacks.value(sample);
    }
    return std::make_unique<FakeDataSubscription>(&cancelCount);
  }

  bool put(const QString &, const QVariant &value, QString *) override
  {
    ++putCount;
    lastPut = value;
    return true;
  }

  QString diagnostic(const QString &) const override
  {
    return QStringLiteral("fake provider ready");
  }

  QStringList providerIds() const override
  {
    return {QStringLiteral("fake-archive")};
  }

  ArchiveProvider *createArchiveProvider(const QString &, QObject *parent,
      QString *) override
  {
    return new FakeArchiveProvider(parent);
  }

  QString id_;
  QString typeId_ = QStringLiteral("fake_label");
  QString scheme_ = QStringLiteral("fake");
  QtedmPluginCompatibility compatibility_ =
      qtedmCurrentPluginCompatibility();
  bool failConstruction_ = false;
  int putCount = 0;
  int cancelCount = 0;
  QVariant lastPut;
};

AdlNode missingPluginNode()
{
  AdlNode node;
  node.name = QStringLiteral("qtedm_plugin");
  node.properties = {
      {QStringLiteral("pluginId"), QStringLiteral("org.missing.plugin")},
      {QStringLiteral("typeId"), QStringLiteral("mystery")},
      {QStringLiteral("schemaVersion"), QStringLiteral("7")},
      {QStringLiteral("futureFlag"), QStringLiteral("keep-me")},
  };
  AdlNode object;
  object.name = QStringLiteral("object");
  object.properties = {
      {QStringLiteral("x"), QStringLiteral("1")},
      {QStringLiteral("y"), QStringLiteral("2")},
      {QStringLiteral("width"), QStringLiteral("80")},
      {QStringLiteral("height"), QStringLiteral("30")},
  };
  AdlNode future;
  future.name = QStringLiteral("future_extension");
  future.properties = {
      {QStringLiteral("opaque"), QStringLiteral("preserve-me")},
  };
  node.children = {object, future};
  return node;
}

QString propertyValueForTest(const AdlNode &node, const QString &name)
{
  for (const AdlProperty &property : node.properties) {
    if (property.key.compare(name, Qt::CaseInsensitive) == 0) {
      return property.value;
    }
  }
  return {};
}

} // namespace

class TestPluginsPropertyRules : public QObject
{
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void exactCompatibilityAndDuplicateRegistration();
  void configuredPluginDirectoryIsDiscovered();
  void providerSubscriptionsWritesArchivesAndShutdown();
  void observeOnlyBlocksPluginProviderWrites();
  void pluginElementTypedRoundTripAndUnknownPreservation();
  void constructionFailureCreatesDiagnosticPlaceholder();
  void rulesParseRoundTripCyclesAndSandbox();
  void rulesEvaluateRateLimitDisconnectAndRestore();
  void displayRulesAndMissingPluginSurviveSaveAndUndo();
};

void TestPluginsPropertyRules::init()
{
  QtedmPluginManager::instance().resetForTesting();
  PvChannelManager::instance().setObserveOnly(false);
  AuditLogger::instance().initialize(false);
  FakePluginRuntime::starts = 0;
  FakePluginRuntime::stops = 0;
}

void TestPluginsPropertyRules::cleanup()
{
  PvChannelManager::instance().setObserveOnly(false);
  QtedmPluginManager::instance().resetForTesting();
  AuditLogger::instance().shutdown();
  qunsetenv("QTEDM_AUDIT_DIR");
}

void TestPluginsPropertyRules::exactCompatibilityAndDuplicateRegistration()
{
  QString reason;
  QVERIFY(QtedmPluginManager::isCompatible(
      qtedmCurrentPluginCompatibility(), &reason));

  QtedmPluginCompatibility incompatible =
      qtedmCurrentPluginCompatibility();
  ++incompatible.interfaceVersion;
  QVERIFY(!QtedmPluginManager::isCompatible(incompatible, &reason));
  QVERIFY(reason.contains(QStringLiteral("interface version")));
  incompatible = qtedmCurrentPluginCompatibility();
  incompatible.architecture += QStringLiteral("-wrong");
  QVERIFY(!QtedmPluginManager::isCompatible(incompatible, &reason));
  QVERIFY(reason.contains(QStringLiteral("architecture")));
  incompatible = qtedmCurrentPluginCompatibility();
  incompatible.compilerAbi += QStringLiteral("-wrong");
  QVERIFY(!QtedmPluginManager::isCompatible(incompatible, &reason));
  QVERIFY(reason.contains(QStringLiteral("compiler ABI")));

  FakePlugin first;
  QVERIFY(QtedmPluginManager::instance().registerPluginObject(&first));
  const auto *registered = QtedmPluginManager::instance().displayObject(
      first.id_, first.typeId_);
  QVERIFY(registered);
  const auto *paletteEntry =
      ExtensionObjectRegistry::instance().descriptor(first.typeId_);
  QVERIFY(paletteEntry);
  QCOMPARE(paletteEntry->pluginId, first.id_);

  FakePlugin duplicateId;
  QVERIFY(!QtedmPluginManager::instance().registerPluginObject(&duplicateId));

  FakePlugin duplicateType(QStringLiteral("org.qtedm.tests.other"));
  duplicateType.scheme_ = QStringLiteral("otherfake");
  QVERIFY(!QtedmPluginManager::instance().registerPluginObject(&duplicateType));

  FakePlugin builtInTypeCollision(
      QStringLiteral("org.qtedm.tests.builtin-collision"));
  builtInTypeCollision.typeId_ = QStringLiteral("qtedm_symbol");
  builtInTypeCollision.scheme_ = QStringLiteral("builtin-collision");
  QVERIFY(!QtedmPluginManager::instance().registerPluginObject(
      &builtInTypeCollision));

  QtedmPluginManager::instance().resetForTesting();
  FakePlugin badAbi;
  badAbi.compatibility_.qtMajorVersion += 1;
  QVERIFY(!QtedmPluginManager::instance().registerPluginObject(&badAbi));
}

void TestPluginsPropertyRules::configuredPluginDirectoryIsDiscovered()
{
  const QDir applicationPluginDirectory(
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("plugins")));
  QString discoveryLibrary;
  for (const QFileInfo &entry : applicationPluginDirectory.entryInfoList(
       QDir::Files | QDir::Readable, QDir::Name)) {
    if (QLibrary::isLibrary(entry.absoluteFilePath())) {
      discoveryLibrary = entry.absoluteFilePath();
      break;
    }
  }
  QVERIFY2(!discoveryLibrary.isEmpty(),
      "The plugin API discovery library was not built.");
  QTemporaryDir missingMetadataDirectory;
  QVERIFY(missingMetadataDirectory.isValid());
  const QString missingMetadataLibrary =
      missingMetadataDirectory.filePath(QFileInfo(discoveryLibrary).fileName());
  QVERIFY(QFile::copy(discoveryLibrary, missingMetadataLibrary));
  const bool hadPluginPath = qEnvironmentVariableIsSet("QTEDM_PLUGIN_PATH");
  const QByteArray oldPluginPath = qgetenv("QTEDM_PLUGIN_PATH");
  qputenv("QTEDM_PLUGIN_PATH",
      missingMetadataDirectory.path().toLocal8Bit());
  QtedmPluginManager::instance().loadConfiguredPlugins();
  if (hadPluginPath) {
    qputenv("QTEDM_PLUGIN_PATH", oldPluginPath);
  } else {
    qunsetenv("QTEDM_PLUGIN_PATH");
  }
  QVERIFY2(QtedmPluginManager::instance().loadedPluginIds().contains(
      QStringLiteral("org.qtedm.tests.discovery")),
      qPrintable(QtedmPluginManager::instance().diagnostics().join(
          QLatin1Char('\n'))));
  QVERIFY(QtedmPluginManager::instance().displayObject(
      QStringLiteral("org.qtedm.tests.discovery"),
      QStringLiteral("discovery_label")));
  QVERIFY(QtedmPluginManager::instance().diagnostics().join(
      QLatin1Char('\n')).contains(
          QStringLiteral("Required plugin metadata is missing")));
}

void TestPluginsPropertyRules::providerSubscriptionsWritesArchivesAndShutdown()
{
  FakePlugin plugin;
  QVERIFY(QtedmPluginManager::instance().registerPluginObject(&plugin));

  bool connected = false;
  bool received = false;
  bool writable = false;
  SubscriptionHandle subscription =
      QtedmPluginManager::instance().subscribeDataProvider(
          QStringLiteral("fake://temperature"),
          [&received](const SharedChannelData &data) {
            received = data.hasValue && qFuzzyCompare(
                data.numericValue, 8.25);
          },
          [&connected](bool state, const SharedChannelData &) {
            connected = state;
          },
          [&writable](bool, bool canWrite) { writable = canWrite; },
          ChannelDeliveryMode::kRealtime);
  QVERIFY(subscription.isValid());
  QVERIFY(connected);
  QVERIFY(received);
  QVERIFY(writable);
  QCOMPARE(QtedmPluginManager::instance().dataProviderDiagnostic(
      QStringLiteral("fake://temperature")),
      QStringLiteral("fake provider ready"));

  QVERIFY(PvChannelManager::instance().putValue(
      QStringLiteral("fake://temperature"), 3.5));
  QCOMPARE(plugin.putCount, 1);
  QCOMPARE(plugin.lastPut.toDouble(), 3.5);

  QObject archiveOwner;
  QString error;
  std::unique_ptr<ArchiveProvider> archive(
      QtedmPluginManager::instance().createArchiveProvider(
          QStringLiteral("fake-archive"), &archiveOwner, &error));
  QVERIFY2(archive != nullptr, qPrintable(error));
  bool completed = false;
  ArchiveQuery query;
  query.channel = QStringLiteral("TEST");
  QVERIFY(archive->query(query, &archiveOwner,
      [&completed](const ArchiveResult &result) {
        completed = result.ok() && result.samples.size() == 1;
      }));
  QVERIFY(completed);
  archive.release();

  subscription.reset();
  QCOMPARE(plugin.cancelCount, 1);
  SubscriptionHandle shutdownSubscription =
      QtedmPluginManager::instance().subscribeDataProvider(
          QStringLiteral("fake://shutdown"), {}, {}, {},
          ChannelDeliveryMode::kPassive);
  QVERIFY(shutdownSubscription.isValid());
  QtedmPluginManager::instance().shutdown();
  QCOMPARE(plugin.cancelCount, 2);
  QVERIFY(QtedmPluginManager::instance().loadedPluginIds().isEmpty());
  QVERIFY(!ExtensionObjectRegistry::instance().descriptor(plugin.typeId_));
}

void TestPluginsPropertyRules::observeOnlyBlocksPluginProviderWrites()
{
  FakePlugin plugin;
  QVERIFY(QtedmPluginManager::instance().registerPluginObject(&plugin));
  QTemporaryDir auditDirectory;
  QVERIFY(auditDirectory.isValid());
  qputenv("QTEDM_AUDIT_DIR", auditDirectory.path().toLocal8Bit());
  AuditLogger::instance().initialize(true);
  PvChannelManager::instance().setObserveOnly(true);
  QVERIFY(!PvChannelManager::instance().putValue(
      QStringLiteral("fake://setpoint"), 9.0));
  QCOMPARE(plugin.putCount, 0);
  QString error;
  QVERIFY(!QtedmPluginManager::instance().put(
      QStringLiteral("fake://setpoint"), QVariant(10.0), &error));
  QCOMPARE(plugin.putCount, 0);
  QVERIFY(error.contains(QStringLiteral("observe-only"),
      Qt::CaseInsensitive));

  PvChannelManager::instance().setObserveOnly(false);
  QVERIFY(QtedmPluginManager::instance().put(
      QStringLiteral("fake://setpoint"), QVariant(11.0), &error));
  QCOMPARE(plugin.putCount, 1);
  AuditLogger::instance().shutdown();

  const QDir directory(auditDirectory.path());
  const QStringList logs = directory.entryList(
      {QStringLiteral("audit_*.log")}, QDir::Files, QDir::Time);
  QVERIFY(!logs.isEmpty());
  QFile logFile(directory.filePath(logs.first()));
  QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray audit = logFile.readAll();
  QVERIFY(audit.contains("BLOCKED:ObserveOnly"));
  QVERIFY(audit.contains("PluginDataProvider"));
  QVERIFY(audit.contains("fake://setpoint"));
}

void TestPluginsPropertyRules::pluginElementTypedRoundTripAndUnknownPreservation()
{
  PluginElement missing;
  QString error;
  QVERIFY(missing.loadFromAdlNode(missingPluginNode(), &error));
  QVERIFY(!missing.pluginAvailable());
  QVERIFY(!missing.diagnostic().isEmpty());
  const AdlNode preserved = missing.toAdlNode(QRect(10, 20, 90, 40));
  QCOMPARE(propertyValueForTest(preserved, QStringLiteral("futureFlag")),
      QStringLiteral("keep-me"));
  bool foundFuture = false;
  for (const AdlNode &child : preserved.children) {
    if (normalizedAdlName(child.name)
        == QStringLiteral("future_extension")) {
      foundFuture = propertyValueForTest(child, QStringLiteral("opaque"))
          == QStringLiteral("preserve-me");
    }
  }
  QVERIFY(foundFuture);

  FakePlugin plugin;
  QVERIFY(QtedmPluginManager::instance().registerPluginObject(&plugin));
  AdlNode node;
  node.name = QStringLiteral("qtedm_plugin");
  node.properties = {
      {QStringLiteral("pluginId"), plugin.id_},
      {QStringLiteral("typeId"), plugin.typeId_},
      {QStringLiteral("schemaVersion"), QStringLiteral("1")},
  };
  AdlNode object;
  object.name = QStringLiteral("object");
  object.properties = {
      {QStringLiteral("x"), QStringLiteral("0")},
      {QStringLiteral("y"), QStringLiteral("0")},
      {QStringLiteral("width"), QStringLiteral("100")},
      {QStringLiteral("height"), QStringLiteral("30")},
  };
  AdlNode textProperty;
  textProperty.name = QStringLiteral("property");
  textProperty.properties = {
      {QStringLiteral("name"), QStringLiteral("text")},
      {QStringLiteral("type"), QStringLiteral("string")},
      {QStringLiteral("value"), QStringLiteral("Operator label")},
  };
  AdlNode colorProperty;
  colorProperty.name = QStringLiteral("property");
  colorProperty.properties = {
      {QStringLiteral("name"), QStringLiteral("color")},
      {QStringLiteral("type"), QStringLiteral("color")},
      {QStringLiteral("value"), QStringLiteral("#ff112233")},
  };
  AdlNode unknownProperty;
  unknownProperty.name = QStringLiteral("property");
  unknownProperty.properties = {
      {QStringLiteral("name"), QStringLiteral("futureKnob")},
      {QStringLiteral("type"), QStringLiteral("integer")},
      {QStringLiteral("value"), QStringLiteral("42")},
  };
  node.children = {object, textProperty, colorProperty, unknownProperty};

  PluginElement loaded;
  QVERIFY2(loaded.loadFromAdlNode(node, &error), qPrintable(error));
  QVERIFY(loaded.pluginAvailable());
  QCOMPARE(qobject_cast<QLabel *>(loaded.pluginWidget())->text(),
      QStringLiteral("Operator label"));
  QCOMPARE(loaded.properties().value(QStringLiteral("color")).value<QColor>(),
      QColor(QStringLiteral("#ff112233")));
  const AdlNode knownRoundTrip = loaded.toAdlNode(QRect(0, 0, 100, 30));
  bool retainedUnknownProperty = false;
  for (const AdlNode &child : knownRoundTrip.children) {
    if (normalizedAdlName(child.name) == QStringLiteral("property")
        && propertyValueForTest(child, QStringLiteral("name"))
            == QStringLiteral("futureKnob")) {
      retainedUnknownProperty =
          propertyValueForTest(child, QStringLiteral("value"))
              == QStringLiteral("42");
    }
  }
  QVERIFY(retainedUnknownProperty);
  QtedmPluginRuntime *runtime = loaded.createRuntime(&error);
  QVERIFY(runtime);
  QVERIFY(runtime->start(&error));
  QCOMPARE(FakePluginRuntime::starts, 1);
  runtime->stop();
  delete runtime;
  QCOMPARE(FakePluginRuntime::stops, 1);
}

void TestPluginsPropertyRules::constructionFailureCreatesDiagnosticPlaceholder()
{
  FakePlugin plugin;
  plugin.failConstruction_ = true;
  QVERIFY(QtedmPluginManager::instance().registerPluginObject(&plugin));
  AdlNode node = missingPluginNode();
  node.properties[0].value = plugin.id_;
  node.properties[1].value = plugin.typeId_;
  node.properties[2].value = QStringLiteral("1");
  PluginElement element;
  QString error;
  QVERIFY(element.loadFromAdlNode(node, &error));
  QVERIFY(!element.pluginAvailable());
  QVERIFY(element.diagnostic().contains(
      QStringLiteral("intentional construction failure")));
}

void TestPluginsPropertyRules::rulesParseRoundTripCyclesAndSandbox()
{
  QtedmRuleSet rules;
  QtedmPropertyRule rule;
  rule.id = QStringLiteral("visible_when_enabled");
  rule.property = QtedmRuleProperty::kVisible;
  rule.expression = QStringLiteral("A>0");
  rule.trueValue = QStringLiteral("true");
  rule.falseValue = QStringLiteral("false");
  rule.inputs = {{QLatin1Char('A'), QStringLiteral("soft:plugins_rules:rule"),
      QtedmRuleInputType::kBoolean}};
  rules.rules.append(rule);
  QStringList diagnostics;
  QVERIFY(PropertyRules::validate(&rules, &diagnostics));

  QString text;
  QTextStream stream(&text);
  PropertyRules::writeAdl(stream, 3, rules);
  QString parseError;
  const auto document = AdlParser::parse(text, &parseError);
  QVERIFY2(document.has_value(), qPrintable(parseError));
  QCOMPARE(document->children.size(), 1);
  QtedmRuleSet parsed;
  int targetIndex = -1;
  QVERIFY(PropertyRules::parseAdl(document->children.first(), &parsed,
      &targetIndex, &parseError));
  QCOMPARE(targetIndex, 3);
  QCOMPARE(parsed.rules.first().id, rule.id);
  QCOMPARE(parsed.rules.first().inputs.first().channel,
      QStringLiteral("soft:plugins_rules:rule"));

  QtedmRuleSet invalidBoolean = rules;
  invalidBoolean.rules.first().trueValue = QStringLiteral("maybe");
  QVERIFY(!PropertyRules::validate(&invalidBoolean, &diagnostics));
  QVERIFY(diagnostics.join(QLatin1Char('\n')).contains(
      QStringLiteral("Boolean values")));

  for (const QString &expression : {
           QStringLiteral("python(A)"),
           QStringLiteral("javascript(A)"),
           QStringLiteral("system(A)"),
           QStringLiteral("spawn(A)"),
           QStringLiteral("file(A)"),
           QStringLiteral("filesystem(A)"),
           QStringLiteral("read(A)"),
           QStringLiteral("write(A)"),
           QStringLiteral("http(A)"),
           QStringLiteral("socket(A)"),
           QStringLiteral("put(A)")}) {
    QVERIFY2(!PropertyRules::isExpressionSandboxed(
        expression, &parseError), qPrintable(expression));
  }

  QtedmRuleSet cyclic;
  QtedmPropertyRule first = rule;
  first.id = QStringLiteral("first");
  first.dependsOn = {QStringLiteral("second")};
  QtedmPropertyRule second = rule;
  second.id = QStringLiteral("second");
  second.dependsOn = {QStringLiteral("first")};
  cyclic.rules = {first, second};
  QVERIFY(!PropertyRules::validate(&cyclic, &diagnostics));
  QVERIFY(diagnostics.join(QLatin1Char('\n')).contains(
      QStringLiteral("cycle"), Qt::CaseInsensitive));
}

void TestPluginsPropertyRules::rulesEvaluateRateLimitDisconnectAndRestore()
{
  const QString pv = QStringLiteral("__plugins_rules:rule_value");
  auto &soft = SoftPvRegistry::instance();
  soft.registerName(pv);
  soft.setConnected(pv, true);
  soft.publishValue(pv, 1.0);

  QLabel target(QStringLiteral("Original"));
  QtedmPropertyRule rule;
  rule.id = QStringLiteral("text_rule");
  rule.property = QtedmRuleProperty::kText;
  rule.expression = QStringLiteral("A>0");
  rule.trueValue = QStringLiteral("On");
  rule.falseValue = QStringLiteral("Off");
  rule.rateLimitHz = 1.0;
  rule.inputs = {{QLatin1Char('A'), pv, QtedmRuleInputType::kNumber}};
  QtedmPropertyRule enabledRule = rule;
  enabledRule.id = QStringLiteral("enabled_rule");
  enabledRule.property = QtedmRuleProperty::kEnabled;
  enabledRule.trueValue = QStringLiteral("false");
  enabledRule.falseValue = QStringLiteral("true");
  QtedmRuleSet rules;
  rules.rules = {rule, enabledRule};

  PropertyRuleRuntime runtime(&target, rules);
  QString error;
  QVERIFY2(runtime.start(&error), qPrintable(error));
  QTRY_COMPARE(target.text(), QStringLiteral("On"));
  QTRY_VERIFY(!target.isEnabled());
  const quint64 initialEvaluations = runtime.evaluationCount();
  for (int index = 0; index < 20; ++index) {
    soft.publishValue(pv, index % 2);
  }
  QTest::qWait(100);
  QVERIFY(runtime.evaluationCount() <= initialEvaluations + 1);

  QTest::qWait(1000);
  soft.publishValue(pv, 0.0);
  QTRY_COMPARE(target.text(), QStringLiteral("Off"));
  QTRY_VERIFY(target.isEnabled());
  soft.setConnected(pv, false);
  QTRY_COMPARE(target.text(), QStringLiteral("Original"));
  QTRY_VERIFY(target.isEnabled());

  soft.setConnected(pv, true);
  soft.publishValue(pv, 1.0);
  QTest::qWait(1000);
  QTRY_COMPARE(target.text(), QStringLiteral("On"));
  QTRY_VERIFY(!target.isEnabled());
  runtime.stop();
  QCOMPARE(target.text(), QStringLiteral("Original"));
  QVERIFY(target.isEnabled());
  soft.setConnected(pv, false);
  soft.unregisterName(pv);
}

void TestPluginsPropertyRules::displayRulesAndMissingPluginSurviveSaveAndUndo()
{
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString source = directory.filePath(
      QStringLiteral("plugins-property-rules.adl"));
  QFile file(source);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
  const QByteArray adl =
      "file { name=\"plugins-property-rules.adl\" version=040004 }\n"
      "display { object { x=0 y=0 width=320 height=160 } clr=14 bclr=0 }\n"
      "color map { ncolors=2 colors { ffffff, 000000, } }\n"
      "text { object { x=20 y=20 width=120 height=30 } textix=\"Rules\" }\n"
      "qtedm_plugin {\n"
      "  pluginId=\"org.missing.plugin\"\n"
      "  typeId=\"future_widget\"\n"
      "  schemaVersion=9\n"
      "  futureFlag=\"preserve-me\"\n"
      "  object { x=20 y=70 width=180 height=40 }\n"
      "  future_extension { opaque=\"still-here\" }\n"
      "}\n"
      "qtedm_rules {\n"
      "  targetIndex=0\n"
      "  rule {\n"
      "    id=\"show\"\n"
      "    property=\"visible\"\n"
      "    expression=\"A>0\"\n"
      "    trueValue=\"true\"\n"
      "    falseValue=\"false\"\n"
      "    disconnect=\"restore\"\n"
      "    rateHz=10\n"
      "    input { variable=\"A\" channel=\"__plugins_rules:display\" type=\"number\" }\n"
      "  }\n"
      "}\n";
  QCOMPARE(file.write(adl), qint64(adl.size()));
  file.close();

  auto state = std::make_shared<DisplayState>();
  state->editMode = true;
  DisplayWindow window(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  QString error;
  QVERIFY2(window.loadFromFile(source, &error), qPrintable(error));
  QJsonObject stateObject = window.testStateObject();
  QCOMPARE(stateObject.value(QStringLiteral("rule_target_count")).toInt(), 1);
  bool foundMissingPlugin = false;
  for (const QJsonValue &value :
       stateObject.value(QStringLiteral("widgets")).toArray()) {
    const QJsonObject widget = value.toObject();
    if (widget.value(QStringLiteral("type")).toString()
        == QStringLiteral("qtedm_plugin")) {
      foundMissingPlugin = true;
      QVERIFY(!widget.value(QStringLiteral("available")).toBool());
    }
  }
  QVERIFY(foundMissingPlugin);

  const QString firstSave =
      directory.filePath(QStringLiteral("plugins-property-rules-saved.adl"));
  QVERIFY(window.saveToPath(firstSave));
  QFile saved(firstSave);
  QVERIFY(saved.open(QIODevice::ReadOnly));
  QByteArray savedText = saved.readAll();
  QVERIFY(savedText.contains("qtedm_rules"));
  QVERIFY(savedText.contains("futureFlag=\"preserve-me\""));
  QVERIFY(savedText.contains("opaque=\"still-here\""));

  window.setGridSpacing(window.gridSpacing() + 1);
  QVERIFY(window.undoStack()->count() > 0);
  window.triggerUndo();
  const QString afterUndo =
      directory.filePath(
          QStringLiteral("plugins-property-rules-after-undo.adl"));
  QVERIFY(window.saveToPath(afterUndo));
  QFile undoFile(afterUndo);
  QVERIFY(undoFile.open(QIODevice::ReadOnly));
  savedText = undoFile.readAll();
  QVERIFY(savedText.contains("qtedm_rules"));
  QVERIFY(savedText.contains("futureFlag=\"preserve-me\""));

  DisplayWindow reopened(QApplication::palette(), QApplication::palette(),
      QApplication::font(), QApplication::font(), state);
  QVERIFY2(reopened.loadFromFile(afterUndo, &error), qPrintable(error));
  QCOMPARE(reopened.testStateObject().value(
      QStringLiteral("rule_target_count")).toInt(), 1);
}

QTEST_MAIN(TestPluginsPropertyRules)
#include "test_plugins_property_rules.moc"
