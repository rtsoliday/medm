#include <QLabel>

#include "qtedm_plugin_api.h"

class PluginApiDiscoveryPlugin final : public QObject,
    public QtedmDisplayObjectPluginInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID QTEDM_DISPLAY_OBJECT_PLUGIN_IID)
  Q_INTERFACES(QtedmDisplayObjectPluginInterface)

public:
  QString pluginId() const override
  {
    return QStringLiteral("org.qtedm.tests.discovery");
  }

  QtedmPluginCompatibility compatibility() const override
  {
    return qtedmCurrentPluginCompatibility();
  }

  QVector<QtedmDisplayObjectType> objectTypes() const override
  {
    QtedmDisplayObjectType descriptor;
    descriptor.typeId = QStringLiteral("discovery_label");
    descriptor.displayName = QStringLiteral("Discovery Label");
    descriptor.category = QStringLiteral("Tests");
    descriptor.properties = {
        {QStringLiteral("text"), QStringLiteral("Text"), QString(),
            QtedmPluginPropertyType::kString,
            QStringLiteral("Discovered"), false},
    };
    return {descriptor};
  }

  QWidget *createWidget(const QString &, QWidget *parent,
      QString *) override
  {
    return new QLabel(parent);
  }

  bool applyProperties(QWidget *widget, const QString &,
      const QVariantMap &properties, QString *) override
  {
    auto *label = qobject_cast<QLabel *>(widget);
    if (!label) {
      return false;
    }
    label->setText(properties.value(
        QStringLiteral("text"), QStringLiteral("Discovered")).toString());
    return true;
  }

  QVariantMap serializeProperties(QWidget *widget,
      const QString &) const override
  {
    const auto *label = qobject_cast<QLabel *>(widget);
    return {{QStringLiteral("text"),
        label ? label->text() : QStringLiteral("Discovered")}};
  }

  QStringList channels(const QString &,
      const QVariantMap &) const override
  {
    return {};
  }

  QtedmPluginRuntime *createRuntime(QWidget *, const QString &,
      QtedmPluginHost *, QString *) override
  {
    return nullptr;
  }
};

#include "plugin_api_discovery_plugin.moc"
