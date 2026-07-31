#pragma once

#include <QPointer>
#include <QVariantMap>
#include <QWidget>

#include <functional>

#include "adl_parser.h"

class QPaintEvent;
class QResizeEvent;
class QTextStream;
class QtedmPluginRuntime;

class PluginElement : public QWidget
{
public:
  explicit PluginElement(QWidget *parent = nullptr);

  bool loadFromAdlNode(const AdlNode &node, QString *error = nullptr);
  AdlNode toAdlNode(const QRect &serializedGeometry) const;
  static void writeAdlNode(QTextStream &stream, const AdlNode &node,
      int level = 0);

  QString pluginId() const;
  QString typeId() const;
  int schemaVersion() const;
  QVariantMap properties() const;
  bool setProperties(const QVariantMap &properties);
  QStringList channels() const;

  QWidget *pluginWidget() const;
  bool pluginAvailable() const;
  QString diagnostic() const;
  void setRuntimeDiagnostic(const QString &diagnostic);

  QtedmPluginRuntime *createRuntime(QString *error = nullptr);

  void setSelected(bool selected);
  bool isSelected() const;
  void setExecuteMode(bool execute);
  bool isExecuteMode() const;

  QString ruleText() const;
  bool setRuleText(const QString &text);

  void setChangedCallback(std::function<void()> callback);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  bool constructPluginWidget(QString *error);
  void updatePluginGeometry();
  void notifyChanged();

  QString pluginId_;
  QString typeId_;
  int schemaVersion_ = 1;
  QVariantMap properties_;
  AdlNode rawNode_;
  bool hasRawNode_ = false;
  QPointer<QWidget> pluginWidget_;
  QString diagnostic_;
  bool selected_ = false;
  bool executeMode_ = false;
  std::function<void()> changedCallback_;
};
