#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVector>

#include <array>
#include <memory>
#include <vector>

#include "adl_parser.h"
#include "channel_subscription.h"

class QTextStream;
class QWidget;

enum class QtedmRuleProperty {
  kVisible,
  kEnabled,
  kText,
  kForeground,
  kBackground,
  kGeometry,
};

enum class QtedmRuleInputType {
  kNumber,
  kBoolean,
  kEnum,
};

enum class QtedmRuleDisconnectBehavior {
  kRestore,
  kFalseValue,
};

struct QtedmRuleInput
{
  QChar variable = QLatin1Char('A');
  QString channel;
  QtedmRuleInputType type = QtedmRuleInputType::kNumber;
};

struct QtedmPropertyRule
{
  QString id;
  QtedmRuleProperty property = QtedmRuleProperty::kVisible;
  QString expression;
  QVector<QtedmRuleInput> inputs;
  QString trueValue;
  QString falseValue;
  QtedmRuleDisconnectBehavior disconnectBehavior =
      QtedmRuleDisconnectBehavior::kRestore;
  double rateLimitHz = 10.0;
  QStringList dependsOn;
};

struct QtedmRuleSet
{
  QVector<QtedmPropertyRule> rules;
  QString diagnostic;

  bool isEmpty() const { return rules.isEmpty(); }
};

class PropertyRules
{
public:
  static bool parseAdl(const AdlNode &node, QtedmRuleSet *ruleSet,
      int *targetIndex = nullptr, QString *error = nullptr);
  static void writeAdl(QTextStream &stream, int targetIndex,
      const QtedmRuleSet &ruleSet, int level = 0);

  static QJsonArray toJson(const QtedmRuleSet &ruleSet);
  static bool fromJson(const QJsonArray &array, QtedmRuleSet *ruleSet,
      QString *error = nullptr);

  static bool validate(QtedmRuleSet *ruleSet,
      QStringList *diagnostics = nullptr);
  static bool isExpressionSandboxed(const QString &expression,
      QString *error = nullptr);

  static QString propertyName(QtedmRuleProperty property);
  static bool propertyFromName(const QString &name,
      QtedmRuleProperty *property);
  static QString inputTypeName(QtedmRuleInputType type);
  static bool inputTypeFromName(const QString &name,
      QtedmRuleInputType *type);
  static QString disconnectName(QtedmRuleDisconnectBehavior behavior);
  static bool disconnectFromName(const QString &name,
      QtedmRuleDisconnectBehavior *behavior);
};

class PropertyRuleRuntime : public QObject
{
public:
  PropertyRuleRuntime(QWidget *target, const QtedmRuleSet &ruleSet);
  ~PropertyRuleRuntime() override;

  bool start(QString *error = nullptr);
  void stop();
  bool isStarted() const;
  QString diagnostic() const;
  quint64 evaluationCount() const;

private:
  struct RuleState
  {
    QtedmPropertyRule rule;
    std::array<SubscriptionHandle, 12> subscriptions;
    std::array<double, 12> values{};
    std::array<bool, 12> connected{};
    std::array<bool, 12> hasValue{};
    std::array<char, 1024> postfix{};
    bool postfixValid = false;
    bool pending = true;
    qint64 lastEvaluationMs = -1;
    quint64 evaluationCount = 0;
  };

  void handleValue(RuleState *state, int inputIndex,
      const SharedChannelData &data);
  void handleConnection(RuleState *state, int inputIndex, bool connected);
  void processPendingRules();
  void evaluateRule(RuleState *state, qint64 nowMs);
  bool allInputsReady(const RuleState &state) const;
  void applyDisconnected(RuleState *state);
  void applyResult(const QtedmPropertyRule &rule, double result);
  void restoreProperty(QtedmRuleProperty property);
  QVariant captureProperty(QtedmRuleProperty property) const;
  bool applyProperty(QtedmRuleProperty property,
      const QVariant &value, QString *error = nullptr);

  QPointer<QWidget> target_;
  QtedmRuleSet ruleSet_;
  std::vector<std::unique_ptr<RuleState>> states_;
  QHash<int, QVariant> originalValues_;
  QElapsedTimer clock_;
  QTimer evaluationTimer_;
  QString diagnostic_;
  bool started_ = false;
};
