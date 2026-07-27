#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <memory>
#include <vector>

#include "channel_subscription.h"
#include "qtedm_plugin_api.h"

class ArchiveProvider;
class QPluginLoader;
class QWidget;

struct QtedmLoadedDisplayObject
{
  QString pluginId;
  QtedmDisplayObjectType descriptor;
  QtedmDisplayObjectPluginInterface *plugin = nullptr;
};

class QtedmPluginManager : public SubscriptionOwner, public QtedmPluginHost
{
public:
  static QtedmPluginManager &instance();

  void loadConfiguredPlugins();
  void shutdown();

  bool registerPluginObject(QObject *object,
      const QString &source = QStringLiteral("<registered>"));
  QStringList diagnostics() const;
  QStringList loadedPluginIds() const;

  const QtedmLoadedDisplayObject *displayObject(
      const QString &pluginId, const QString &typeId) const;
  QVector<QtedmLoadedDisplayObject> displayObjects() const;
  QWidget *createDisplayWidget(const QString &pluginId,
      const QString &typeId, QWidget *parent, QString *error = nullptr);
  bool applyDisplayProperties(const QString &pluginId,
      const QString &typeId, QWidget *widget,
      const QVariantMap &properties, QString *error = nullptr);
  QVariantMap serializeDisplayProperties(const QString &pluginId,
      const QString &typeId, QWidget *widget) const;
  QStringList displayChannels(const QString &pluginId,
      const QString &typeId, const QVariantMap &properties) const;
  QtedmPluginRuntime *createDisplayRuntime(const QString &pluginId,
      const QString &typeId, QWidget *widget, QString *error = nullptr);

  bool supportsDataProvider(const QString &uri) const;
  SubscriptionHandle subscribeDataProvider(const QString &uri,
      ChannelValueCallback valueCallback,
      ChannelConnectionCallback connectionCallback,
      ChannelAccessRightsCallback accessRightsCallback,
      ChannelDeliveryMode deliveryMode);
  bool putDataProvider(const QString &uri, const QVariant &value,
      QString *error = nullptr);
  QString dataProviderDiagnostic(const QString &uri) const;

  ArchiveProvider *createArchiveProvider(const QString &providerId,
      QObject *parent, QString *error = nullptr);

  static QString uriScheme(const QString &uri);
  static bool isCompatible(const QtedmPluginCompatibility &compatibility,
      QString *reason = nullptr);

  void resetForTesting();

  std::unique_ptr<QtedmPluginHostSubscription> subscribe(
      const QString &uri, const QtedmChannelCallbacks &callbacks,
      QtedmChannelDelivery delivery) override;
  bool put(const QString &uri, const QVariant &value,
      QString *error = nullptr) override;
  bool observeOnly() const override;
  void reportDiagnostic(const QString &pluginId,
      const QString &message) override;

  void unsubscribe(quint64 subscriptionId) override;

private:
  QtedmPluginManager() = default;
  ~QtedmPluginManager() override;
  QtedmPluginManager(const QtedmPluginManager &) = delete;
  QtedmPluginManager &operator=(const QtedmPluginManager &) = delete;

  struct DataProviderRegistration
  {
    QString pluginId;
    QtedmDataProviderPluginInterface *plugin = nullptr;
  };

  struct ArchiveProviderRegistration
  {
    QString pluginId;
    QtedmArchiveProviderPluginInterface *plugin = nullptr;
  };

  struct DataSubscription
  {
    std::shared_ptr<QtedmDataSubscription> subscription;
  };

  bool loadPluginFile(const QString &path);
  bool validatePluginId(const QString &pluginId, QString *reason) const;
  void addDiagnostic(const QString &source, const QString &message);
  static QtedmChannelSample sampleFromShared(const SharedChannelData &data,
      bool canRead = true, bool canWrite = false);

  QHash<QString, QtedmLoadedDisplayObject> displayObjectsByKey_;
  QHash<QString, QString> displayTypeOwners_;
  QHash<QString, DataProviderRegistration> dataProvidersByScheme_;
  QHash<QString, ArchiveProviderRegistration> archiveProvidersById_;
  QHash<quint64, DataSubscription> dataSubscriptions_;
  std::vector<std::unique_ptr<QPluginLoader>> loaders_;
  QSet<QString> loadedPluginIds_;
  QStringList diagnostics_;
  quint64 nextSubscriptionId_ = 1;
  bool configuredPluginsLoaded_ = false;
  bool shutdownComplete_ = false;
};
