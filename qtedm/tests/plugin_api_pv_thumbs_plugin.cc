#include <QFont>
#include <QLabel>
#include <QMetaObject>
#include <QPointer>

#include <utility>

#include "qtedm_plugin_api.h"

namespace {

constexpr char CHANNEL_PROPERTY[] = "_qtedmPvThumbsChannel";

QString unavailableText()
{
  return QString::fromUtf8("\xE2\x80\x94");
}

QString thumbsUpText()
{
  return QString::fromUtf8("\xF0\x9F\x91\x8D");
}

QString thumbsDownText()
{
  return QString::fromUtf8("\xF0\x9F\x91\x8E");
}

void setLabelText(const QPointer<QLabel> &label, const QString &text)
{
  if (!label) {
    return;
  }
  QMetaObject::invokeMethod(label.data(), "setText", Qt::AutoConnection,
      Q_ARG(QString, text));
}

void showSample(const QPointer<QLabel> &label,
    const QtedmChannelSample &sample)
{
  if (!sample.hasValue || !sample.isNumeric) {
    setLabelText(label, unavailableText());
    return;
  }
  setLabelText(label,
      sample.numericValue == 0.0 ? thumbsDownText() : thumbsUpText());
}

class PvThumbsRuntime final : public QtedmPluginRuntime
{
public:
  PvThumbsRuntime(QLabel *label, QString channel, QtedmPluginHost *host)
    : label_(label), channel_(std::move(channel)), host_(host)
  {
  }

  ~PvThumbsRuntime() override
  {
    stop();
  }

  bool start(QString *error) override
  {
    if (subscription_) {
      return true;
    }
    if (!label_ || !host_ || channel_.trimmed().isEmpty()) {
      diagnostic_ = QStringLiteral(
          "PV Thumbs Indicator requires a widget, host, and channel.");
      if (error) {
        *error = diagnostic_;
      }
      return false;
    }

    setLabelText(label_, unavailableText());
    QtedmChannelCallbacks callbacks;
    callbacks.value = [label = label_](const QtedmChannelSample &sample) {
      showSample(label, sample);
    };
    callbacks.connection = [label = label_](bool connected,
        const QtedmChannelSample &sample) {
      if (connected) {
        showSample(label, sample);
      } else {
        setLabelText(label, unavailableText());
      }
    };
    callbacks.accessRights = [label = label_](bool canRead, bool) {
      if (!canRead) {
        setLabelText(label, unavailableText());
      }
    };
    subscription_ = host_->subscribe(channel_.trimmed(), callbacks,
        QtedmChannelDelivery::kRealtime);
    if (!subscription_) {
      diagnostic_ = QStringLiteral("Unable to subscribe to %1.")
          .arg(channel_.trimmed());
      if (error) {
        *error = diagnostic_;
      }
      return false;
    }
    diagnostic_.clear();
    return true;
  }

  void stop() override
  {
    if (subscription_) {
      subscription_->cancel();
      subscription_.reset();
    }
    setLabelText(label_, unavailableText());
  }

  QString diagnostic() const override
  {
    return diagnostic_;
  }

private:
  QPointer<QLabel> label_;
  QString channel_;
  QtedmPluginHost *host_ = nullptr;
  std::unique_ptr<QtedmPluginHostSubscription> subscription_;
  QString diagnostic_;
};

} // namespace

class PluginApiPvThumbsPlugin final : public QObject,
    public QtedmDisplayObjectPluginInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID QTEDM_DISPLAY_OBJECT_PLUGIN_IID)
  Q_INTERFACES(QtedmDisplayObjectPluginInterface)

public:
  QString pluginId() const override
  {
    return QStringLiteral("org.qtedm.examples.pv-thumbs");
  }

  QtedmPluginCompatibility compatibility() const override
  {
    return qtedmCurrentPluginCompatibility();
  }

  QVector<QtedmDisplayObjectType> objectTypes() const override
  {
    QtedmDisplayObjectType descriptor;
    descriptor.typeId = QStringLiteral("pv_thumbs_indicator");
    descriptor.displayName = QStringLiteral("PV Thumbs Indicator");
    descriptor.category = QStringLiteral("Examples");
    descriptor.defaultSize = QSize(100, 80);
    descriptor.properties = {
        {QStringLiteral("channel"), QStringLiteral("Channel"),
            QStringLiteral(
                "PV whose zero/nonzero value selects the emoji."),
            QtedmPluginPropertyType::kString,
            QStringLiteral("led:test:binary_live"), true},
    };
    return {descriptor};
  }

  QWidget *createWidget(const QString &typeId, QWidget *parent,
      QString *error) override
  {
    if (typeId != QStringLiteral("pv_thumbs_indicator")) {
      if (error) {
        *error = QStringLiteral("Unsupported PV Thumbs object type.");
      }
      return nullptr;
    }
    auto *label = new QLabel(unavailableText(), parent);
    label->setAlignment(Qt::AlignCenter);
    QFont font = label->font();
    font.setPixelSize(48);
    label->setFont(font);
    label->setAccessibleName(QStringLiteral("PV Thumbs Indicator"));
    return label;
  }

  bool applyProperties(QWidget *widget, const QString &typeId,
      const QVariantMap &properties, QString *error) override
  {
    auto *label = qobject_cast<QLabel *>(widget);
    if (!label || typeId != QStringLiteral("pv_thumbs_indicator")) {
      if (error) {
        *error = QStringLiteral("PV Thumbs Indicator requires a QLabel.");
      }
      return false;
    }
    const QString channel = properties.value(QStringLiteral("channel"),
        QStringLiteral("led:test:binary_live")).toString().trimmed();
    if (channel.isEmpty()) {
      if (error) {
        *error = QStringLiteral("PV Thumbs Indicator requires a channel.");
      }
      return false;
    }
    label->setProperty(CHANNEL_PROPERTY, channel);
    label->setToolTip(channel);
    return true;
  }

  QVariantMap serializeProperties(QWidget *widget,
      const QString &) const override
  {
    const auto *label = qobject_cast<QLabel *>(widget);
    return {{QStringLiteral("channel"),
        label ? label->property(CHANNEL_PROPERTY).toString() : QString()}};
  }

  QStringList channels(const QString &typeId,
      const QVariantMap &properties) const override
  {
    if (typeId != QStringLiteral("pv_thumbs_indicator")) {
      return {};
    }
    const QString channel =
        properties.value(QStringLiteral("channel")).toString().trimmed();
    return channel.isEmpty() ? QStringList() : QStringList{channel};
  }

  QtedmPluginRuntime *createRuntime(QWidget *widget, const QString &typeId,
      QtedmPluginHost *host, QString *error) override
  {
    auto *label = qobject_cast<QLabel *>(widget);
    if (!label || typeId != QStringLiteral("pv_thumbs_indicator")) {
      if (error) {
        *error = QStringLiteral("PV Thumbs Indicator requires a QLabel.");
      }
      return nullptr;
    }
    return new PvThumbsRuntime(label,
        label->property(CHANNEL_PROPERTY).toString(), host);
  }
};

#include "plugin_api_pv_thumbs_plugin.moc"
