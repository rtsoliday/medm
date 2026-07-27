#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QSysInfo>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>

#include <functional>
#include <memory>

class ArchiveProvider;
class QWidget;

constexpr int QTEDM_PLUGIN_INTERFACE_VERSION = 1;

struct QtedmPluginCompatibility
{
  int interfaceVersion = QTEDM_PLUGIN_INTERFACE_VERSION;
  int qtMajorVersion = QT_VERSION_MAJOR;
  QString architecture;
  QString compilerAbi;
};

inline QString qtedmCompilerAbi()
{
#if defined(_MSC_VER)
  return QStringLiteral("msvc-%1").arg(_MSC_VER);
#elif defined(__clang__)
  return QStringLiteral("clang-%1.%2.%3")
      .arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__);
#elif defined(__GNUC__)
  return QStringLiteral("gcc-%1.%2.%3")
      .arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#else
  return QStringLiteral("unknown");
#endif
}

inline QtedmPluginCompatibility qtedmCurrentPluginCompatibility()
{
  QtedmPluginCompatibility compatibility;
  compatibility.architecture = QSysInfo::buildCpuArchitecture().toLower();
  compatibility.compilerAbi = qtedmCompilerAbi().toLower();
  return compatibility;
}

inline bool operator==(const QtedmPluginCompatibility &left,
    const QtedmPluginCompatibility &right)
{
  return left.interfaceVersion == right.interfaceVersion
      && left.qtMajorVersion == right.qtMajorVersion
      && left.architecture.compare(right.architecture,
          Qt::CaseInsensitive) == 0
      && left.compilerAbi.compare(right.compilerAbi,
          Qt::CaseInsensitive) == 0;
}

enum class QtedmPluginPropertyType {
  kBoolean,
  kInteger,
  kDouble,
  kString,
  kColor,
  kStringList,
};

struct QtedmPluginPropertySchema
{
  QString name;
  QString displayName;
  QString description;
  QtedmPluginPropertyType type = QtedmPluginPropertyType::kString;
  QVariant defaultValue;
  bool required = false;
};

struct QtedmDisplayObjectType
{
  QString typeId;
  QString displayName;
  QString category;
  int schemaVersion = 1;
  QSize defaultSize = QSize(160, 80);
  QVector<QtedmPluginPropertySchema> properties;
};

struct QtedmChannelSample
{
  bool connected = false;
  bool canRead = false;
  bool canWrite = false;
  bool hasValue = false;
  bool isNumeric = false;
  bool isString = false;
  bool isEnum = false;
  bool isArray = false;
  double numericValue = 0.0;
  QString stringValue;
  unsigned int enumValue = 0;
  QVector<double> arrayValues;
  QStringList enumStrings;
  short severity = 0;
  short status = 0;
  int nativeType = -1;
  long elementCount = 0;
  QDateTime timestamp;
};

struct QtedmChannelCallbacks
{
  std::function<void(const QtedmChannelSample &)> value;
  std::function<void(bool, const QtedmChannelSample &)> connection;
  std::function<void(bool, bool)> accessRights;
};

enum class QtedmChannelDelivery {
  kPassive,
  kRealtime,
};

class QtedmPluginHostSubscription
{
public:
  virtual ~QtedmPluginHostSubscription() = default;
  virtual void cancel() = 0;
};

class QtedmPluginHost
{
public:
  virtual ~QtedmPluginHost() = default;

  virtual std::unique_ptr<QtedmPluginHostSubscription> subscribe(
      const QString &uri, const QtedmChannelCallbacks &callbacks,
      QtedmChannelDelivery delivery = QtedmChannelDelivery::kPassive) = 0;
  virtual bool put(const QString &uri, const QVariant &value,
      QString *error = nullptr) = 0;
  virtual bool observeOnly() const = 0;
  virtual void reportDiagnostic(const QString &pluginId,
      const QString &message) = 0;
};

class QtedmPluginRuntime
{
public:
  virtual ~QtedmPluginRuntime() = default;
  virtual bool start(QString *error = nullptr) = 0;
  virtual void stop() = 0;
  virtual QString diagnostic() const { return QString(); }
};

class QtedmDisplayObjectPluginInterface
{
public:
  virtual ~QtedmDisplayObjectPluginInterface() = default;

  virtual QString pluginId() const = 0;
  virtual QtedmPluginCompatibility compatibility() const = 0;
  virtual QVector<QtedmDisplayObjectType> objectTypes() const = 0;
  virtual QWidget *createWidget(const QString &typeId, QWidget *parent,
      QString *error = nullptr) = 0;
  virtual bool applyProperties(QWidget *widget, const QString &typeId,
      const QVariantMap &properties, QString *error = nullptr) = 0;
  virtual QVariantMap serializeProperties(QWidget *widget,
      const QString &typeId) const = 0;
  virtual QStringList channels(const QString &typeId,
      const QVariantMap &properties) const = 0;
  virtual QtedmPluginRuntime *createRuntime(QWidget *widget,
      const QString &typeId, QtedmPluginHost *host,
      QString *error = nullptr) = 0;
};

class QtedmDataSubscription
{
public:
  virtual ~QtedmDataSubscription() = default;
  virtual void cancel() = 0;
};

class QtedmDataProviderPluginInterface
{
public:
  virtual ~QtedmDataProviderPluginInterface() = default;

  virtual QString pluginId() const = 0;
  virtual QtedmPluginCompatibility compatibility() const = 0;
  virtual QStringList schemes() const = 0;
  virtual std::unique_ptr<QtedmDataSubscription> subscribe(
      const QString &uri, const QtedmChannelCallbacks &callbacks,
      QtedmChannelDelivery delivery, QString *error = nullptr) = 0;
  virtual bool put(const QString &uri, const QVariant &value,
      QString *error = nullptr) = 0;
  virtual QString diagnostic(const QString &uri) const = 0;
};

class QtedmArchiveProviderPluginInterface
{
public:
  virtual ~QtedmArchiveProviderPluginInterface() = default;

  virtual QString pluginId() const = 0;
  virtual QtedmPluginCompatibility compatibility() const = 0;
  virtual QStringList providerIds() const = 0;
  virtual ArchiveProvider *createArchiveProvider(const QString &providerId,
      QObject *parent, QString *error = nullptr) = 0;
};

#define QTEDM_DISPLAY_OBJECT_PLUGIN_IID \
  "org.aps.qtedm.DisplayObjectPlugin/1.0"
#define QTEDM_DATA_PROVIDER_PLUGIN_IID \
  "org.aps.qtedm.DataProviderPlugin/1.0"
#define QTEDM_ARCHIVE_PROVIDER_PLUGIN_IID \
  "org.aps.qtedm.ArchiveProviderPlugin/1.0"

Q_DECLARE_INTERFACE(QtedmDisplayObjectPluginInterface,
    QTEDM_DISPLAY_OBJECT_PLUGIN_IID)
Q_DECLARE_INTERFACE(QtedmDataProviderPluginInterface,
    QTEDM_DATA_PROVIDER_PLUGIN_IID)
Q_DECLARE_INTERFACE(QtedmArchiveProviderPluginInterface,
    QTEDM_ARCHIVE_PROVIDER_PLUGIN_IID)

