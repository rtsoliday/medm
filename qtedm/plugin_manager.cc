#include "plugin_manager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <exception>
#include <utility>

#include "archive_provider.h"
#include "audit_logger.h"
#include "extension_object_registry.h"
#include "pv_channel_manager.h"

namespace {

constexpr qint64 kMaximumPluginMetadataBytes = 64 * 1024;

struct PluginMetadata
{
  QString pluginId;
  QSet<QString> interfaces;
};

bool readPluginMetadata(const QString &libraryPath, PluginMetadata *metadata,
    QString *error)
{
  if (!metadata) {
    return false;
  }
  const QString metadataPath =
      libraryPath + QStringLiteral(".qtedm-plugin.json");
  QFile file(metadataPath);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) {
      *error = QStringLiteral("Required plugin metadata is missing: %1")
          .arg(metadataPath);
    }
    return false;
  }
  if (file.size() < 2 || file.size() > kMaximumPluginMetadataBytes) {
    if (error) {
      *error = QStringLiteral("Plugin metadata must be between 2 bytes and 64 KiB.");
    }
    return false;
  }
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError
      || !document.isObject()) {
    if (error) {
      *error = QStringLiteral("Invalid plugin metadata JSON: %1")
          .arg(parseError.errorString());
    }
    return false;
  }
  const QJsonObject object = document.object();
  if (object.value(QStringLiteral("schema")).toString()
          != QStringLiteral("org.aps.qtedm.plugin-metadata")
      || object.value(QStringLiteral("schema_version")).toInt(-1) != 1) {
    if (error) {
      *error = QStringLiteral("Unsupported QtEDM plugin metadata schema.");
    }
    return false;
  }
  PluginMetadata parsed;
  parsed.pluginId =
      object.value(QStringLiteral("plugin_id")).toString().trimmed();
  const QJsonArray interfaces =
      object.value(QStringLiteral("interfaces")).toArray();
  static const QSet<QString> allowedInterfaces = {
      QStringLiteral("display"),
      QStringLiteral("data"),
      QStringLiteral("archive"),
  };
  for (const QJsonValue &value : interfaces) {
    const QString name = value.toString().trimmed().toLower();
    if (!allowedInterfaces.contains(name)
        || parsed.interfaces.contains(name)) {
      if (error) {
        *error = QStringLiteral("Plugin metadata has an invalid or duplicate interface.");
      }
      return false;
    }
    parsed.interfaces.insert(name);
  }
  if (parsed.pluginId.isEmpty() || parsed.interfaces.isEmpty()) {
    if (error) {
      *error = QStringLiteral("Plugin metadata requires plugin_id and interfaces.");
    }
    return false;
  }
  *metadata = parsed;
  return true;
}

QString objectKey(const QString &pluginId, const QString &typeId)
{
  return pluginId.trimmed().toLower() + QLatin1Char('/')
      + typeId.trimmed().toLower();
}

QtedmChannelDelivery pluginDelivery(ChannelDeliveryMode delivery)
{
  return delivery == ChannelDeliveryMode::kRealtime
      ? QtedmChannelDelivery::kRealtime : QtedmChannelDelivery::kPassive;
}

ChannelDeliveryMode hostDelivery(QtedmChannelDelivery delivery)
{
  return delivery == QtedmChannelDelivery::kRealtime
      ? ChannelDeliveryMode::kRealtime : ChannelDeliveryMode::kPassive;
}

class HostSubscription final : public QtedmPluginHostSubscription
{
public:
  explicit HostSubscription(SubscriptionHandle subscription)
    : subscription_(std::move(subscription))
  {
  }

  void cancel() override
  {
    subscription_.reset();
  }

private:
  SubscriptionHandle subscription_;
};

} // namespace

QtedmPluginManager &QtedmPluginManager::instance()
{
  /*
   * Keep the process-wide manager alive until process teardown.  Its explicit
   * shutdown() path owns deterministic plugin cancellation/unloading; relying
   * on C++ static destruction would race the independently constructed
   * extension registry and Qt plugin infrastructure.
   */
  static QtedmPluginManager *manager = new QtedmPluginManager;
  return *manager;
}

QtedmPluginManager::~QtedmPluginManager()
{
  shutdown();
}

void QtedmPluginManager::loadConfiguredPlugins()
{
  if (configuredPluginsLoaded_ || shutdownComplete_) {
    return;
  }
  configuredPluginsLoaded_ = true;

  QStringList directories;
  if (QCoreApplication::instance()) {
    directories.append(QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("plugins")));
  }
  const QString configured = qEnvironmentVariable("QTEDM_PLUGIN_PATH");
  for (const QString &entry :
       configured.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
    const QString trimmed = entry.trimmed();
    if (!trimmed.isEmpty()) {
      directories.append(trimmed);
    }
  }

  QSet<QString> visitedPaths;
  QStringList candidates;
  for (const QString &directoryPath : directories) {
    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isAbsolute() || !directoryInfo.exists()
        || !directoryInfo.isDir()) {
      if (!directoryPath.trimmed().isEmpty()) {
        addDiagnostic(directoryPath,
            QStringLiteral("Plugin directory does not exist or is not local."));
      }
      continue;
    }
    const QString canonical = directoryInfo.canonicalFilePath();
    if (canonical.isEmpty() || visitedPaths.contains(canonical)) {
      continue;
    }
    visitedPaths.insert(canonical);
    const QDir directory(canonical);
    for (const QFileInfo &entry : directory.entryInfoList(
         QDir::Files | QDir::Readable, QDir::Name)) {
      if (QLibrary::isLibrary(entry.absoluteFilePath())) {
        candidates.append(entry.absoluteFilePath());
      }
    }
  }
  std::sort(candidates.begin(), candidates.end());
  for (const QString &candidate : candidates) {
    loadPluginFile(candidate);
  }
}

bool QtedmPluginManager::loadPluginFile(const QString &path)
{
  const QFileInfo info(path);
  if (!info.isAbsolute() || !info.exists() || !info.isFile()) {
    addDiagnostic(path, QStringLiteral("Plugin file is not a local file."));
    return false;
  }

  PluginMetadata metadata;
  QString metadataError;
  if (!readPluginMetadata(info.absoluteFilePath(), &metadata,
          &metadataError)) {
    addDiagnostic(path, metadataError);
    return false;
  }
  if (!validatePluginId(metadata.pluginId, &metadataError)) {
    addDiagnostic(path, metadataError);
    return false;
  }

  auto loader = std::make_unique<QPluginLoader>(info.absoluteFilePath());
  QObject *instance = loader->instance();
  if (!instance) {
    addDiagnostic(path, loader->errorString());
    return false;
  }
  auto *displayPlugin =
      qobject_cast<QtedmDisplayObjectPluginInterface *>(instance);
  auto *dataPlugin =
      qobject_cast<QtedmDataProviderPluginInterface *>(instance);
  auto *archivePlugin =
      qobject_cast<QtedmArchiveProviderPluginInterface *>(instance);
  QSet<QString> actualInterfaces;
  if (displayPlugin) {
    actualInterfaces.insert(QStringLiteral("display"));
  }
  if (dataPlugin) {
    actualInterfaces.insert(QStringLiteral("data"));
  }
  if (archivePlugin) {
    actualInterfaces.insert(QStringLiteral("archive"));
  }
  QString actualPluginId;
  if (displayPlugin) {
    actualPluginId = displayPlugin->pluginId().trimmed();
  } else if (dataPlugin) {
    actualPluginId = dataPlugin->pluginId().trimmed();
  } else if (archivePlugin) {
    actualPluginId = archivePlugin->pluginId().trimmed();
  }
  if (metadata.pluginId.compare(actualPluginId,
          Qt::CaseInsensitive) != 0
      || metadata.interfaces != actualInterfaces) {
    addDiagnostic(path,
        QStringLiteral("Plugin metadata does not match the loaded plugin ID or interfaces."));
    loader->unload();
    return false;
  }
  if (!registerPluginObject(instance, info.absoluteFilePath())) {
    loader->unload();
    return false;
  }
  loaders_.push_back(std::move(loader));
  return true;
}

bool QtedmPluginManager::validatePluginId(const QString &pluginId,
    QString *reason) const
{
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{2,127}$"));
  if (!pattern.match(pluginId.trimmed()).hasMatch()) {
    if (reason) {
      *reason = QStringLiteral(
          "Plugin ID must be 3-128 letters, digits, dots, underscores, or dashes.");
    }
    return false;
  }
  return true;
}

bool QtedmPluginManager::registerPluginObject(QObject *object,
    const QString &source)
{
  if (!object || shutdownComplete_) {
    addDiagnostic(source, QStringLiteral("Plugin instance is unavailable."));
    return false;
  }

  auto *displayPlugin =
      qobject_cast<QtedmDisplayObjectPluginInterface *>(object);
  auto *dataPlugin = qobject_cast<QtedmDataProviderPluginInterface *>(object);
  auto *archivePlugin =
      qobject_cast<QtedmArchiveProviderPluginInterface *>(object);
  if (!displayPlugin && !dataPlugin && !archivePlugin) {
    addDiagnostic(source,
        QStringLiteral("Object implements no QtEDM version-1 plugin interface."));
    return false;
  }

  QString pluginId;
  QtedmPluginCompatibility compatibility;
  bool compatibilitySet = false;
  auto considerInterface = [&](const QString &candidateId,
      const QtedmPluginCompatibility &candidateCompatibility) {
    if (pluginId.isEmpty()) {
      pluginId = candidateId.trimmed();
    } else if (pluginId.compare(candidateId.trimmed(),
                   Qt::CaseInsensitive) != 0) {
      return false;
    }
    if (!compatibilitySet) {
      compatibility = candidateCompatibility;
      compatibilitySet = true;
    } else if (!(compatibility == candidateCompatibility)) {
      return false;
    }
    return true;
  };

  if ((displayPlugin && !considerInterface(displayPlugin->pluginId(),
          displayPlugin->compatibility()))
      || (dataPlugin && !considerInterface(dataPlugin->pluginId(),
          dataPlugin->compatibility()))
      || (archivePlugin && !considerInterface(archivePlugin->pluginId(),
          archivePlugin->compatibility()))) {
    addDiagnostic(source,
        QStringLiteral("Plugin interfaces disagree about plugin ID or ABI."));
    return false;
  }

  QString reason;
  if (!validatePluginId(pluginId, &reason)
      || !isCompatible(compatibility, &reason)) {
    addDiagnostic(source, reason);
    return false;
  }
  const QString normalizedPluginId = pluginId.toLower();
  if (loadedPluginIds_.contains(normalizedPluginId)) {
    addDiagnostic(source,
        QStringLiteral("Duplicate plugin ID: %1").arg(pluginId));
    return false;
  }

  QVector<QtedmLoadedDisplayObject> displayRegistrations;
  QSet<QString> newTypeIds;
  if (displayPlugin) {
    const QVector<QtedmDisplayObjectType> types = displayPlugin->objectTypes();
    if (types.isEmpty() || types.size() > 128) {
      addDiagnostic(source,
          QStringLiteral("Display plugin must register between 1 and 128 types."));
      return false;
    }
    for (QtedmDisplayObjectType descriptor : types) {
      descriptor.typeId = descriptor.typeId.trimmed().toLower();
      descriptor.displayName = descriptor.displayName.trimmed();
      descriptor.category = descriptor.category.trimmed();
      static const QRegularExpression typeIdPattern(
          QStringLiteral("^[a-z][a-z0-9_.-]{1,63}$"));
      if (!typeIdPattern.match(descriptor.typeId).hasMatch()
          || descriptor.displayName.isEmpty()
          || descriptor.schemaVersion < 1
          || descriptor.defaultSize.width() < 1
          || descriptor.defaultSize.height() < 1
          || descriptor.defaultSize.width() > 16384
          || descriptor.defaultSize.height() > 16384
          || descriptor.properties.size() > 256
          || newTypeIds.contains(descriptor.typeId)
          || displayTypeOwners_.contains(descriptor.typeId)
          || ExtensionObjectRegistry::instance().descriptor(
              descriptor.typeId)) {
        addDiagnostic(source,
            QStringLiteral("Invalid or duplicate display type ID: %1")
                .arg(descriptor.typeId));
        return false;
      }
      newTypeIds.insert(descriptor.typeId);
      QSet<QString> propertyNames;
      for (const QtedmPluginPropertySchema &property : descriptor.properties) {
        const QString name = property.name.trimmed().toLower();
        static const QRegularExpression propertyNamePattern(
            QStringLiteral("^[a-z][a-z0-9_.-]{0,63}$"));
        if (!propertyNamePattern.match(name).hasMatch()
            || propertyNames.contains(name)) {
          addDiagnostic(source,
              QStringLiteral("Duplicate or empty property schema in type %1.")
                  .arg(descriptor.typeId));
          return false;
        }
        propertyNames.insert(name);
      }
      displayRegistrations.append(
          {pluginId, descriptor, displayPlugin});
    }
  }

  QStringList dataSchemes;
  if (dataPlugin) {
    for (QString scheme : dataPlugin->schemes()) {
      scheme = scheme.trimmed().toLower();
      static const QRegularExpression schemePattern(
          QStringLiteral("^[a-z][a-z0-9+.-]{1,31}$"));
      if (!schemePattern.match(scheme).hasMatch()
          || scheme == QStringLiteral("ca")
          || scheme == QStringLiteral("pva")
          || dataSchemes.contains(scheme)
          || dataProvidersByScheme_.contains(scheme)) {
        addDiagnostic(source,
            QStringLiteral("Invalid or duplicate data-provider scheme: %1")
                .arg(scheme));
        return false;
      }
      dataSchemes.append(scheme);
    }
    if (dataSchemes.isEmpty()) {
      addDiagnostic(source,
          QStringLiteral("Data-provider plugin registered no URI schemes."));
      return false;
    }
  }

  QStringList archiveIds;
  if (archivePlugin) {
    for (QString providerId : archivePlugin->providerIds()) {
      providerId = providerId.trimmed().toLower();
      static const QRegularExpression providerIdPattern(
          QStringLiteral("^[a-z][a-z0-9_.-]{1,63}$"));
      if (!providerIdPattern.match(providerId).hasMatch()
          || providerId == QStringLiteral("archiver-appliance")
          || archiveIds.contains(providerId)
          || archiveProvidersById_.contains(providerId)) {
        addDiagnostic(source,
            QStringLiteral("Invalid or duplicate archive-provider ID: %1")
                .arg(providerId));
        return false;
      }
      archiveIds.append(providerId);
    }
    if (archiveIds.isEmpty()) {
      addDiagnostic(source,
          QStringLiteral("Archive-provider plugin registered no provider IDs."));
      return false;
    }
  }

  for (const QtedmLoadedDisplayObject &registration :
       displayRegistrations) {
    displayObjectsByKey_.insert(objectKey(pluginId,
        registration.descriptor.typeId), registration);
    displayTypeOwners_.insert(registration.descriptor.typeId, pluginId);
    ExtensionObjectRegistry::instance().registerPluginObject(
        pluginId, registration.descriptor);
  }
  for (const QString &scheme : dataSchemes) {
    dataProvidersByScheme_.insert(scheme, {pluginId, dataPlugin});
  }
  for (const QString &providerId : archiveIds) {
    archiveProvidersById_.insert(providerId, {pluginId, archivePlugin});
  }
  loadedPluginIds_.insert(normalizedPluginId);
  return true;
}

void QtedmPluginManager::shutdown()
{
  if (shutdownComplete_) {
    return;
  }
  shutdownComplete_ = true;
  for (auto it = dataSubscriptions_.begin();
       it != dataSubscriptions_.end(); ++it) {
    if (it->subscription) {
      it->subscription->cancel();
    }
  }
  dataSubscriptions_.clear();
  displayObjectsByKey_.clear();
  displayTypeOwners_.clear();
  dataProvidersByScheme_.clear();
  archiveProvidersById_.clear();
  loadedPluginIds_.clear();
  ExtensionObjectRegistry::instance().unregisterPluginObjects();
  for (auto &loader : loaders_) {
    if (loader) {
      loader->unload();
    }
  }
  loaders_.clear();
}

QStringList QtedmPluginManager::diagnostics() const
{
  return diagnostics_;
}

QStringList QtedmPluginManager::loadedPluginIds() const
{
  QStringList result = loadedPluginIds_.values();
  std::sort(result.begin(), result.end());
  return result;
}

const QtedmLoadedDisplayObject *QtedmPluginManager::displayObject(
    const QString &pluginId, const QString &typeId) const
{
  const auto it = displayObjectsByKey_.constFind(objectKey(pluginId, typeId));
  return it == displayObjectsByKey_.cend() ? nullptr : &it.value();
}

QVector<QtedmLoadedDisplayObject> QtedmPluginManager::displayObjects() const
{
  QVector<QtedmLoadedDisplayObject> result;
  result.reserve(displayObjectsByKey_.size());
  for (const QtedmLoadedDisplayObject &object : displayObjectsByKey_) {
    result.append(object);
  }
  std::sort(result.begin(), result.end(),
      [](const QtedmLoadedDisplayObject &left,
          const QtedmLoadedDisplayObject &right) {
        const int category = left.descriptor.category.compare(
            right.descriptor.category, Qt::CaseInsensitive);
        if (category != 0) {
          return category < 0;
        }
        return left.descriptor.displayName.compare(
            right.descriptor.displayName, Qt::CaseInsensitive) < 0;
      });
  return result;
}

QWidget *QtedmPluginManager::createDisplayWidget(const QString &pluginId,
    const QString &typeId, QWidget *parent, QString *error)
{
  const QtedmLoadedDisplayObject *registration =
      displayObject(pluginId, typeId);
  if (!registration || !registration->plugin) {
    if (error) {
      *error = QStringLiteral("Plugin %1 type %2 is not loaded.")
          .arg(pluginId, typeId);
    }
    return nullptr;
  }
  try {
    return registration->plugin->createWidget(
        registration->descriptor.typeId, parent, error);
  } catch (const std::exception &exception) {
    if (error) {
      *error = QStringLiteral("Plugin construction threw: %1")
          .arg(QString::fromLocal8Bit(exception.what()));
    }
  } catch (...) {
    if (error) {
      *error = QStringLiteral("Plugin construction threw an unknown exception.");
    }
  }
  return nullptr;
}

bool QtedmPluginManager::applyDisplayProperties(const QString &pluginId,
    const QString &typeId, QWidget *widget, const QVariantMap &properties,
    QString *error)
{
  const QtedmLoadedDisplayObject *registration =
      displayObject(pluginId, typeId);
  if (!registration || !registration->plugin || !widget) {
    if (error) {
      *error = QStringLiteral("Plugin widget is unavailable.");
    }
    return false;
  }
  try {
    return registration->plugin->applyProperties(widget,
        registration->descriptor.typeId, properties, error);
  } catch (...) {
    if (error) {
      *error = QStringLiteral("Plugin property application threw an exception.");
    }
    return false;
  }
}

QVariantMap QtedmPluginManager::serializeDisplayProperties(
    const QString &pluginId, const QString &typeId, QWidget *widget) const
{
  const QtedmLoadedDisplayObject *registration =
      displayObject(pluginId, typeId);
  if (!registration || !registration->plugin || !widget) {
    return {};
  }
  try {
    return registration->plugin->serializeProperties(widget,
        registration->descriptor.typeId);
  } catch (...) {
    return {};
  }
}

QStringList QtedmPluginManager::displayChannels(const QString &pluginId,
    const QString &typeId, const QVariantMap &properties) const
{
  const QtedmLoadedDisplayObject *registration =
      displayObject(pluginId, typeId);
  if (!registration || !registration->plugin) {
    return {};
  }
  try {
    return registration->plugin->channels(
        registration->descriptor.typeId, properties);
  } catch (...) {
    return {};
  }
}

QtedmPluginRuntime *QtedmPluginManager::createDisplayRuntime(
    const QString &pluginId, const QString &typeId, QWidget *widget,
    QString *error)
{
  const QtedmLoadedDisplayObject *registration =
      displayObject(pluginId, typeId);
  if (!registration || !registration->plugin || !widget) {
    if (error) {
      *error = QStringLiteral("Plugin widget is unavailable.");
    }
    return nullptr;
  }
  try {
    return registration->plugin->createRuntime(widget,
        registration->descriptor.typeId, this, error);
  } catch (...) {
    if (error) {
      *error = QStringLiteral("Plugin runtime construction threw an exception.");
    }
    return nullptr;
  }
}

QString QtedmPluginManager::uriScheme(const QString &uri)
{
  static const QRegularExpression pattern(
      QStringLiteral("^([A-Za-z][A-Za-z0-9+.-]{1,31})://"));
  const QRegularExpressionMatch match = pattern.match(uri.trimmed());
  return match.hasMatch() ? match.captured(1).toLower() : QString();
}

bool QtedmPluginManager::supportsDataProvider(const QString &uri) const
{
  return dataProvidersByScheme_.contains(uriScheme(uri));
}

SubscriptionHandle QtedmPluginManager::subscribeDataProvider(
    const QString &uri, ChannelValueCallback valueCallback,
    ChannelConnectionCallback connectionCallback,
    ChannelAccessRightsCallback accessRightsCallback,
    ChannelDeliveryMode deliveryMode)
{
  const QString scheme = uriScheme(uri);
  const auto it = dataProvidersByScheme_.constFind(scheme);
  if (it == dataProvidersByScheme_.cend() || !it->plugin) {
    return {};
  }

  QtedmChannelCallbacks callbacks;
  callbacks.value =
      [callback = std::move(valueCallback)](
          const QtedmChannelSample &sample) {
        if (!callback) {
          return;
        }
        SharedChannelData data;
        data.connected = sample.connected;
        data.hasValue = sample.hasValue;
        data.isNumeric = sample.isNumeric;
        data.isString = sample.isString;
        data.isEnum = sample.isEnum;
        data.isArray = sample.isArray;
        data.numericValue = sample.numericValue;
        data.stringValue = sample.stringValue;
        data.enumValue = static_cast<dbr_enum_t>(sample.enumValue);
        data.arrayValues = sample.arrayValues;
        data.enumStrings = sample.enumStrings;
        data.severity = sample.severity;
        data.status = sample.status;
        data.nativeFieldType = static_cast<short>(sample.nativeType);
        data.nativeElementCount = sample.elementCount;
        callback(data);
      };
  callbacks.connection =
      [callback = std::move(connectionCallback)](bool connected,
          const QtedmChannelSample &sample) {
        if (!callback) {
          return;
        }
        SharedChannelData data;
        data.connected = connected;
        data.nativeFieldType = static_cast<short>(sample.nativeType);
        data.nativeElementCount = sample.elementCount;
        data.hasValue = sample.hasValue;
        data.isNumeric = sample.isNumeric;
        data.numericValue = sample.numericValue;
        callback(connected, data);
      };
  callbacks.accessRights = std::move(accessRightsCallback);

  QString error;
  std::unique_ptr<QtedmDataSubscription> subscription;
  try {
    subscription = it->plugin->subscribe(uri, callbacks,
        pluginDelivery(deliveryMode), &error);
  } catch (...) {
    error = QStringLiteral("Data-provider subscription threw an exception.");
  }
  if (!subscription) {
    addDiagnostic(it->pluginId,
        error.isEmpty() ? QStringLiteral("Subscription construction failed.")
                        : error);
    return {};
  }
  const quint64 id = nextSubscriptionId_++;
  dataSubscriptions_.insert(id,
      {std::shared_ptr<QtedmDataSubscription>(std::move(subscription))});
  return SubscriptionHandle(id, this);
}

void QtedmPluginManager::unsubscribe(quint64 subscriptionId)
{
  auto it = dataSubscriptions_.find(subscriptionId);
  if (it == dataSubscriptions_.end()) {
    return;
  }
  if (it->subscription) {
    it->subscription->cancel();
  }
  dataSubscriptions_.erase(it);
}

bool QtedmPluginManager::putDataProvider(const QString &uri,
    const QVariant &value, QString *error)
{
  const auto it = dataProvidersByScheme_.constFind(uriScheme(uri));
  if (it == dataProvidersByScheme_.cend() || !it->plugin) {
    if (error) {
      *error = QStringLiteral("No plugin data provider is registered for %1.")
          .arg(uriScheme(uri));
    }
    return false;
  }
  try {
    return it->plugin->put(uri, value, error);
  } catch (...) {
    if (error) {
      *error = QStringLiteral("Data-provider put threw an exception.");
    }
    return false;
  }
}

QString QtedmPluginManager::dataProviderDiagnostic(const QString &uri) const
{
  const auto it = dataProvidersByScheme_.constFind(uriScheme(uri));
  if (it == dataProvidersByScheme_.cend() || !it->plugin) {
    return QStringLiteral("No plugin data provider is registered.");
  }
  try {
    return it->plugin->diagnostic(uri);
  } catch (...) {
    return QStringLiteral("Data-provider diagnostic threw an exception.");
  }
}

ArchiveProvider *QtedmPluginManager::createArchiveProvider(
    const QString &providerId, QObject *parent, QString *error)
{
  const auto it = archiveProvidersById_.constFind(
      providerId.trimmed().toLower());
  if (it == archiveProvidersById_.cend() || !it->plugin) {
    if (error) {
      *error = QStringLiteral("Archive provider %1 is not registered.")
          .arg(providerId);
    }
    return nullptr;
  }
  try {
    return it->plugin->createArchiveProvider(providerId, parent, error);
  } catch (...) {
    if (error) {
      *error = QStringLiteral("Archive-provider construction threw an exception.");
    }
    return nullptr;
  }
}

bool QtedmPluginManager::isCompatible(
    const QtedmPluginCompatibility &compatibility, QString *reason)
{
  const QtedmPluginCompatibility expected =
      qtedmCurrentPluginCompatibility();
  if (compatibility.interfaceVersion != expected.interfaceVersion) {
    if (reason) {
      *reason = QStringLiteral("QtEDM plugin interface version mismatch.");
    }
    return false;
  }
  if (compatibility.qtMajorVersion != expected.qtMajorVersion) {
    if (reason) {
      *reason = QStringLiteral("Qt major version mismatch.");
    }
    return false;
  }
  if (compatibility.architecture.compare(expected.architecture,
          Qt::CaseInsensitive) != 0) {
    if (reason) {
      *reason = QStringLiteral("Plugin architecture mismatch: expected %1.")
          .arg(expected.architecture);
    }
    return false;
  }
  if (compatibility.compilerAbi.compare(expected.compilerAbi,
          Qt::CaseInsensitive) != 0) {
    if (reason) {
      *reason = QStringLiteral("Plugin compiler ABI mismatch: expected %1.")
          .arg(expected.compilerAbi);
    }
    return false;
  }
  return true;
}

void QtedmPluginManager::resetForTesting()
{
  for (auto it = dataSubscriptions_.begin();
       it != dataSubscriptions_.end(); ++it) {
    if (it->subscription) {
      it->subscription->cancel();
    }
  }
  dataSubscriptions_.clear();
  displayObjectsByKey_.clear();
  displayTypeOwners_.clear();
  dataProvidersByScheme_.clear();
  archiveProvidersById_.clear();
  loadedPluginIds_.clear();
  ExtensionObjectRegistry::instance().unregisterPluginObjects();
  for (auto &loader : loaders_) {
    if (loader) {
      loader->unload();
    }
  }
  loaders_.clear();
  diagnostics_.clear();
  nextSubscriptionId_ = 1;
  configuredPluginsLoaded_ = false;
  shutdownComplete_ = false;
}

std::unique_ptr<QtedmPluginHostSubscription>
QtedmPluginManager::subscribe(const QString &uri,
    const QtedmChannelCallbacks &callbacks, QtedmChannelDelivery delivery)
{
  SubscriptionHandle handle = PvChannelManager::instance().subscribe(uri,
      DBR_TIME_DOUBLE, 0,
      [callback = callbacks.value](const SharedChannelData &data) {
        if (callback) {
          callback(sampleFromShared(data));
        }
      },
      [callback = callbacks.connection](bool connected,
          const SharedChannelData &data) {
        if (callback) {
          callback(connected, sampleFromShared(data));
        }
      },
      callbacks.accessRights, hostDelivery(delivery));
  if (!handle.isValid()) {
    return {};
  }
  return std::make_unique<HostSubscription>(std::move(handle));
}

bool QtedmPluginManager::put(const QString &uri, const QVariant &value,
    QString *error)
{
  const bool pluginDataProvider = supportsDataProvider(uri);
  bool success = false;
  const int valueType = value.userType();
  if (valueType == QMetaType::Double
      || valueType == QMetaType::Float
      || valueType == QMetaType::Int
      || valueType == QMetaType::UInt
      || valueType == QMetaType::LongLong
      || valueType == QMetaType::ULongLong) {
    bool ok = false;
    const double numeric = value.toDouble(&ok);
    success = ok && PvChannelManager::instance().putValue(uri, numeric);
  } else if (valueType == QMetaType::QByteArray) {
    success = PvChannelManager::instance().putCharArrayValue(
        uri, value.toByteArray());
  } else if (valueType == QMetaType::QVariantList) {
    QVector<double> values;
    bool ok = true;
    for (const QVariant &entry : value.toList()) {
      bool entryOk = false;
      const double number = entry.toDouble(&entryOk);
      if (!entryOk) {
        ok = false;
        break;
      }
      values.append(number);
    }
    success = ok && PvChannelManager::instance().putArrayValue(uri, values);
  } else {
    success = PvChannelManager::instance().putValue(uri, value.toString());
  }
  if (!success && error) {
    *error = observeOnly()
        ? QStringLiteral("Write blocked by observe-only policy.")
        : QStringLiteral("PV write failed.");
  }
  if (success && !pluginDataProvider) {
    AuditLogger::instance().logPut(uri, value.toString(),
        QStringLiteral("PluginWidget"));
  }
  return success;
}

bool QtedmPluginManager::observeOnly() const
{
  return PvChannelManager::instance().isObserveOnly();
}

void QtedmPluginManager::reportDiagnostic(const QString &pluginId,
    const QString &message)
{
  addDiagnostic(pluginId, message);
}

void QtedmPluginManager::addDiagnostic(const QString &source,
    const QString &message)
{
  diagnostics_.append(QStringLiteral("%1: %2").arg(source, message));
}

QtedmChannelSample QtedmPluginManager::sampleFromShared(
    const SharedChannelData &data, bool canRead, bool canWrite)
{
  QtedmChannelSample sample;
  sample.connected = data.connected;
  sample.canRead = canRead;
  sample.canWrite = canWrite;
  sample.hasValue = data.hasValue;
  sample.isNumeric = data.isNumeric;
  sample.isString = data.isString;
  sample.isEnum = data.isEnum;
  sample.isArray = data.isArray;
  sample.numericValue = data.numericValue;
  sample.stringValue = data.stringValue;
  sample.enumValue = data.enumValue;
  sample.arrayValues = data.arrayValues;
  sample.enumStrings = data.enumStrings;
  sample.severity = data.severity;
  sample.status = data.status;
  sample.nativeType = data.nativeFieldType;
  sample.elementCount = data.nativeElementCount;
  if (data.hasTimestamp) {
    const qint64 unixSeconds =
        static_cast<qint64>(data.timestamp.secPastEpoch) + 631152000LL;
    sample.timestamp = QDateTime::fromMSecsSinceEpoch(
        unixSeconds * 1000LL + data.timestamp.nsec / 1000000).toUTC();
  }
  return sample;
}
