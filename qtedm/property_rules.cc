#include "property_rules.h"

#include <QAbstractButton>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMetaProperty>
#include <QPalette>
#include <QRegularExpression>
#include <QTextStream>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>

#include <db_access.h>

#include "adl_writer.h"
#include "plugin_element.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"
#include "text_element.h"

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {

constexpr int kMaximumRulesPerWidget = 64;
constexpr int kMaximumInputsPerRule = 12;
/* medm_calc.c has fixed 80-entry parser and evaluation stacks and no output
 * length argument. Keeping the infix below 80 characters bounds both stack
 * use and the worst-case encoded postfix size. */
constexpr int kMaximumExpressionLength = 79;
constexpr int kMaximumPostfixLength = 1024;
constexpr double kMinimumRateHz = 1.0;
constexpr double kMaximumRateHz = 60.0;

QStringList splitDependencies(const QString &value)
{
  QStringList result;
  for (const QString &entry :
       value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    const QString trimmed = entry.trimmed();
    if (!trimmed.isEmpty() && !result.contains(trimmed)) {
      result.append(trimmed);
    }
  }
  return result;
}

QRect rectFromString(const QString &value, bool *ok)
{
  const QStringList parts =
      value.split(QLatin1Char(','), Qt::KeepEmptyParts);
  if (parts.size() != 4) {
    if (ok) {
      *ok = false;
    }
    return {};
  }
  std::array<int, 4> numbers{};
  bool valid = true;
  for (int index = 0; index < 4; ++index) {
    bool partOk = false;
    numbers[static_cast<std::size_t>(index)] =
        parts.at(index).trimmed().toInt(&partOk);
    valid = valid && partOk;
  }
  valid = valid && numbers[2] > 0 && numbers[3] > 0;
  if (ok) {
    *ok = valid;
  }
  return valid ? QRect(numbers[0], numbers[1], numbers[2], numbers[3])
               : QRect();
}

bool boolFromString(const QString &value, bool defaultValue, bool *ok)
{
  const QString normalized = value.trimmed().toLower();
  if (normalized.isEmpty()) {
    if (ok) {
      *ok = true;
    }
    return defaultValue;
  }
  if (normalized == QStringLiteral("true")
      || normalized == QStringLiteral("1")
      || normalized == QStringLiteral("yes")
      || normalized == QStringLiteral("on")) {
    if (ok) {
      *ok = true;
    }
    return true;
  }
  if (normalized == QStringLiteral("false")
      || normalized == QStringLiteral("0")
      || normalized == QStringLiteral("no")
      || normalized == QStringLiteral("off")) {
    if (ok) {
      *ok = true;
    }
    return false;
  }
  if (ok) {
    *ok = false;
  }
  return defaultValue;
}

QString widgetText(QWidget *target)
{
  if (!target) {
    return {};
  }
  if (auto *text = dynamic_cast<TextElement *>(target)) {
    return text->text();
  }
  if (auto *plugin = dynamic_cast<PluginElement *>(target)) {
    return plugin->ruleText();
  }
  const QMetaObject *meta = target->metaObject();
  const int index = meta ? meta->indexOfProperty("text") : -1;
  if (index >= 0) {
    return meta->property(index).read(target).toString();
  }
  return {};
}

bool setWidgetText(QWidget *target, const QString &value)
{
  if (!target) {
    return false;
  }
  if (auto *text = dynamic_cast<TextElement *>(target)) {
    text->setText(value);
    return true;
  }
  if (auto *plugin = dynamic_cast<PluginElement *>(target)) {
    return plugin->setRuleText(value);
  }
  const QMetaObject *meta = target->metaObject();
  const int index = meta ? meta->indexOfProperty("text") : -1;
  return index >= 0 && meta->property(index).isWritable()
      && meta->property(index).write(target, value);
}

} // namespace

QString PropertyRules::propertyName(QtedmRuleProperty property)
{
  switch (property) {
  case QtedmRuleProperty::kVisible: return QStringLiteral("visibility");
  case QtedmRuleProperty::kEnabled: return QStringLiteral("enabled");
  case QtedmRuleProperty::kText: return QStringLiteral("text");
  case QtedmRuleProperty::kForeground: return QStringLiteral("foreground");
  case QtedmRuleProperty::kBackground: return QStringLiteral("background");
  case QtedmRuleProperty::kGeometry: return QStringLiteral("geometry");
  }
  return QStringLiteral("visibility");
}

bool PropertyRules::propertyFromName(const QString &name,
    QtedmRuleProperty *property)
{
  if (!property) {
    return false;
  }
  const QString normalized = name.trimmed().toLower();
  if (normalized == QStringLiteral("visibility")
      || normalized == QStringLiteral("visible")) {
    *property = QtedmRuleProperty::kVisible;
  } else if (normalized == QStringLiteral("enabled")) {
    *property = QtedmRuleProperty::kEnabled;
  } else if (normalized == QStringLiteral("text")) {
    *property = QtedmRuleProperty::kText;
  } else if (normalized == QStringLiteral("foreground")
      || normalized == QStringLiteral("foregroundcolor")) {
    *property = QtedmRuleProperty::kForeground;
  } else if (normalized == QStringLiteral("background")
      || normalized == QStringLiteral("backgroundcolor")) {
    *property = QtedmRuleProperty::kBackground;
  } else if (normalized == QStringLiteral("geometry")) {
    *property = QtedmRuleProperty::kGeometry;
  } else {
    return false;
  }
  return true;
}

QString PropertyRules::inputTypeName(QtedmRuleInputType type)
{
  switch (type) {
  case QtedmRuleInputType::kNumber: return QStringLiteral("number");
  case QtedmRuleInputType::kBoolean: return QStringLiteral("boolean");
  case QtedmRuleInputType::kEnum: return QStringLiteral("enum");
  }
  return QStringLiteral("number");
}

bool PropertyRules::inputTypeFromName(const QString &name,
    QtedmRuleInputType *type)
{
  if (!type) {
    return false;
  }
  const QString normalized = name.trimmed().toLower();
  if (normalized.isEmpty() || normalized == QStringLiteral("number")
      || normalized == QStringLiteral("double")) {
    *type = QtedmRuleInputType::kNumber;
  } else if (normalized == QStringLiteral("boolean")
      || normalized == QStringLiteral("bool")) {
    *type = QtedmRuleInputType::kBoolean;
  } else if (normalized == QStringLiteral("enum")) {
    *type = QtedmRuleInputType::kEnum;
  } else {
    return false;
  }
  return true;
}

QString PropertyRules::disconnectName(
    QtedmRuleDisconnectBehavior behavior)
{
  return behavior == QtedmRuleDisconnectBehavior::kFalseValue
      ? QStringLiteral("false") : QStringLiteral("restore");
}

bool PropertyRules::disconnectFromName(const QString &name,
    QtedmRuleDisconnectBehavior *behavior)
{
  if (!behavior) {
    return false;
  }
  const QString normalized = name.trimmed().toLower();
  if (normalized.isEmpty() || normalized == QStringLiteral("restore")) {
    *behavior = QtedmRuleDisconnectBehavior::kRestore;
  } else if (normalized == QStringLiteral("false")
      || normalized == QStringLiteral("falsevalue")) {
    *behavior = QtedmRuleDisconnectBehavior::kFalseValue;
  } else {
    return false;
  }
  return true;
}

bool PropertyRules::parseAdl(const AdlNode &node, QtedmRuleSet *ruleSet,
    int *targetIndex, QString *error)
{
  if (!ruleSet
      || normalizedAdlName(node.name) != QStringLiteral("qtedm_rules")) {
    if (error) {
      *error = QStringLiteral("Expected a qtedm_rules node.");
    }
    return false;
  }
  QtedmRuleSet parsed;
  bool targetOk = false;
  const int parsedTarget = propertyValue(node,
      QStringLiteral("targetIndex"), QStringLiteral("-1")).toInt(&targetOk);
  if (!targetOk || parsedTarget < 0) {
    if (error) {
      *error = QStringLiteral("qtedm_rules requires a non-negative targetIndex.");
    }
    return false;
  }

  for (const AdlNode &child : node.children) {
    if (normalizedAdlName(child.name) != QStringLiteral("rule")) {
      continue;
    }
    QtedmPropertyRule rule;
    rule.id = propertyValue(child, QStringLiteral("id")).trimmed();
    if (!propertyFromName(propertyValue(child,
            QStringLiteral("property")), &rule.property)) {
      if (error) {
        *error = QStringLiteral("Rule %1 has an unsupported property.")
            .arg(rule.id);
      }
      return false;
    }
    rule.expression =
        propertyValue(child, QStringLiteral("expression")).trimmed();
    rule.trueValue = propertyValue(child, QStringLiteral("trueValue"));
    rule.falseValue = propertyValue(child, QStringLiteral("falseValue"));
    if (!disconnectFromName(propertyValue(child,
            QStringLiteral("disconnect")), &rule.disconnectBehavior)) {
      if (error) {
        *error = QStringLiteral("Rule %1 has an invalid disconnect behavior.")
            .arg(rule.id);
      }
      return false;
    }
    bool rateOk = false;
    rule.rateLimitHz = propertyValue(child,
        QStringLiteral("rateHz"), QStringLiteral("10")).toDouble(&rateOk);
    if (!rateOk) {
      if (error) {
        *error = QStringLiteral("Rule %1 has an invalid rateHz.")
            .arg(rule.id);
      }
      return false;
    }
    rule.dependsOn = splitDependencies(
        propertyValue(child, QStringLiteral("dependsOn")));
    for (const AdlNode &inputNode : child.children) {
      if (normalizedAdlName(inputNode.name) != QStringLiteral("input")) {
        continue;
      }
      QtedmRuleInput input;
      const QString variable = propertyValue(inputNode,
          QStringLiteral("variable")).trimmed().toUpper();
      if (variable.size() != 1) {
        if (error) {
          *error = QStringLiteral("Rule %1 has an invalid input variable.")
              .arg(rule.id);
        }
        return false;
      }
      input.variable = variable.at(0);
      input.channel = propertyValue(inputNode,
          QStringLiteral("channel")).trimmed();
      if (!inputTypeFromName(propertyValue(inputNode,
              QStringLiteral("type")), &input.type)) {
        if (error) {
          *error = QStringLiteral("Rule %1 has an invalid input type.")
              .arg(rule.id);
        }
        return false;
      }
      rule.inputs.append(input);
    }
    parsed.rules.append(rule);
  }

  QStringList diagnostics;
  if (!validate(&parsed, &diagnostics)) {
    if (error) {
      *error = diagnostics.join(QLatin1Char('\n'));
    }
    return false;
  }
  if (targetIndex) {
    *targetIndex = parsedTarget;
  }
  *ruleSet = parsed;
  return true;
}

void PropertyRules::writeAdl(QTextStream &stream, int targetIndex,
    const QtedmRuleSet &ruleSet, int level)
{
  if (ruleSet.rules.isEmpty() || targetIndex < 0) {
    return;
  }
  AdlWriter::writeIndentedLine(stream, level,
      QStringLiteral("qtedm_rules {"));
  AdlWriter::writeIndentedLine(stream, level + 1,
      QStringLiteral("targetIndex=%1").arg(targetIndex));
  for (const QtedmPropertyRule &rule : ruleSet.rules) {
    AdlWriter::writeIndentedLine(stream, level + 1, QStringLiteral("rule {"));
    auto writeString = [&](const QString &key, const QString &value) {
      AdlWriter::writeIndentedLine(stream, level + 2,
          QStringLiteral("%1=\"%2\"").arg(key,
              AdlWriter::escapeAdlString(value)));
    };
    writeString(QStringLiteral("id"), rule.id);
    writeString(QStringLiteral("property"), propertyName(rule.property));
    writeString(QStringLiteral("expression"), rule.expression);
    if (!rule.trueValue.isEmpty()) {
      writeString(QStringLiteral("trueValue"), rule.trueValue);
    }
    if (!rule.falseValue.isEmpty()) {
      writeString(QStringLiteral("falseValue"), rule.falseValue);
    }
    writeString(QStringLiteral("disconnect"),
        disconnectName(rule.disconnectBehavior));
    AdlWriter::writeIndentedLine(stream, level + 2,
        QStringLiteral("rateHz=%1")
            .arg(QString::number(rule.rateLimitHz, 'g', 15)));
    if (!rule.dependsOn.isEmpty()) {
      writeString(QStringLiteral("dependsOn"),
          rule.dependsOn.join(QLatin1Char(',')));
    }
    for (const QtedmRuleInput &input : rule.inputs) {
      AdlWriter::writeIndentedLine(stream, level + 2,
          QStringLiteral("input {"));
      AdlWriter::writeIndentedLine(stream, level + 3,
          QStringLiteral("variable=\"%1\"").arg(input.variable));
      AdlWriter::writeIndentedLine(stream, level + 3,
          QStringLiteral("channel=\"%1\"").arg(
              AdlWriter::escapeAdlString(input.channel)));
      AdlWriter::writeIndentedLine(stream, level + 3,
          QStringLiteral("type=\"%1\"").arg(inputTypeName(input.type)));
      AdlWriter::writeIndentedLine(stream, level + 2, QStringLiteral("}"));
    }
    AdlWriter::writeIndentedLine(stream, level + 1, QStringLiteral("}"));
  }
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
}

QJsonArray PropertyRules::toJson(const QtedmRuleSet &ruleSet)
{
  QJsonArray array;
  for (const QtedmPropertyRule &rule : ruleSet.rules) {
    QJsonObject object;
    object[QStringLiteral("id")] = rule.id;
    object[QStringLiteral("property")] = propertyName(rule.property);
    object[QStringLiteral("expression")] = rule.expression;
    object[QStringLiteral("true_value")] = rule.trueValue;
    object[QStringLiteral("false_value")] = rule.falseValue;
    object[QStringLiteral("disconnect")] =
        disconnectName(rule.disconnectBehavior);
    object[QStringLiteral("rate_hz")] = rule.rateLimitHz;
    QJsonArray dependencies;
    for (const QString &dependency : rule.dependsOn) {
      dependencies.append(dependency);
    }
    object[QStringLiteral("depends_on")] = dependencies;
    QJsonArray inputs;
    for (const QtedmRuleInput &input : rule.inputs) {
      inputs.append(QJsonObject{
          {QStringLiteral("variable"), QString(input.variable)},
          {QStringLiteral("channel"), input.channel},
          {QStringLiteral("type"), inputTypeName(input.type)},
      });
    }
    object[QStringLiteral("inputs")] = inputs;
    array.append(object);
  }
  return array;
}

bool PropertyRules::fromJson(const QJsonArray &array,
    QtedmRuleSet *ruleSet, QString *error)
{
  if (!ruleSet) {
    return false;
  }
  QtedmRuleSet parsed;
  for (const QJsonValue &entry : array) {
    if (!entry.isObject()) {
      if (error) {
        *error = QStringLiteral("Each rule must be a JSON object.");
      }
      return false;
    }
    const QJsonObject object = entry.toObject();
    QtedmPropertyRule rule;
    rule.id = object.value(QStringLiteral("id")).toString().trimmed();
    rule.expression =
        object.value(QStringLiteral("expression")).toString().trimmed();
    rule.trueValue =
        object.value(QStringLiteral("true_value")).toString();
    rule.falseValue =
        object.value(QStringLiteral("false_value")).toString();
    rule.rateLimitHz =
        object.value(QStringLiteral("rate_hz")).toDouble(10.0);
    if (!propertyFromName(object.value(
            QStringLiteral("property")).toString(), &rule.property)
        || !disconnectFromName(object.value(
            QStringLiteral("disconnect")).toString(),
            &rule.disconnectBehavior)) {
      if (error) {
        *error = QStringLiteral("Rule %1 has an invalid property or disconnect mode.")
            .arg(rule.id);
      }
      return false;
    }
    for (const QJsonValue &dependency :
         object.value(QStringLiteral("depends_on")).toArray()) {
      const QString id = dependency.toString().trimmed();
      if (!id.isEmpty()) {
        rule.dependsOn.append(id);
      }
    }
    for (const QJsonValue &inputValue :
         object.value(QStringLiteral("inputs")).toArray()) {
      const QJsonObject inputObject = inputValue.toObject();
      const QString variable =
          inputObject.value(QStringLiteral("variable"))
              .toString().trimmed().toUpper();
      QtedmRuleInput input;
      if (variable.size() != 1
          || !inputTypeFromName(inputObject.value(
              QStringLiteral("type")).toString(), &input.type)) {
        if (error) {
          *error = QStringLiteral("Rule %1 has an invalid input.")
              .arg(rule.id);
        }
        return false;
      }
      input.variable = variable.at(0);
      input.channel =
          inputObject.value(QStringLiteral("channel")).toString().trimmed();
      rule.inputs.append(input);
    }
    parsed.rules.append(rule);
  }
  QStringList diagnostics;
  if (!validate(&parsed, &diagnostics)) {
    if (error) {
      *error = diagnostics.join(QLatin1Char('\n'));
    }
    return false;
  }
  *ruleSet = parsed;
  return true;
}

bool PropertyRules::isExpressionSandboxed(const QString &expression,
    QString *error)
{
  if (expression.size() > kMaximumExpressionLength) {
    if (error) {
      *error = QStringLiteral("Expression exceeds 79 characters.");
    }
    return false;
  }
  static const QRegularExpression forbidden(
      QStringLiteral(
          R"(\b(python|javascript|system|exec|spawn|process|shell|file|filesystem|open|read|write|put|http|https|socket|network|curl|wget)\b)"),
      QRegularExpression::CaseInsensitiveOption);
  if (forbidden.match(expression).hasMatch()) {
    if (error) {
      *error = QStringLiteral(
          "Expression requests a forbidden scripting or I/O capability.");
    }
    return false;
  }
  const QString normalized =
      RuntimeUtils::normalizeCalcExpression(expression.trimmed());
  QByteArray infix = normalized.toLatin1();
  std::array<char, kMaximumPostfixLength> compiled{};
  short parseError = 0;
  if (infix.isEmpty()
      || postfix(infix.data(), compiled.data(), &parseError) != 0) {
    if (error) {
      *error = QStringLiteral("Calculation grammar rejected the expression (error %1).")
          .arg(parseError);
    }
    return false;
  }
  return true;
}

bool PropertyRules::validate(QtedmRuleSet *ruleSet,
    QStringList *diagnostics)
{
  QStringList messages;
  if (!ruleSet) {
    messages.append(QStringLiteral("Rule set is unavailable."));
  } else if (ruleSet->rules.size() > kMaximumRulesPerWidget) {
    messages.append(QStringLiteral("A widget may contain at most 64 rules."));
  }
  if (!ruleSet) {
    if (diagnostics) {
      *diagnostics = messages;
    }
    return false;
  }

  QHash<QString, int> indexById;
  for (int index = 0; index < ruleSet->rules.size(); ++index) {
    QtedmPropertyRule &rule = ruleSet->rules[index];
    rule.id = rule.id.trimmed();
    if (rule.id.isEmpty()) {
      messages.append(QStringLiteral("Every rule requires an ID."));
    } else if (indexById.contains(rule.id)) {
      messages.append(QStringLiteral("Duplicate rule ID: %1").arg(rule.id));
    } else {
      indexById.insert(rule.id, index);
    }
    QString sandboxError;
    if (!isExpressionSandboxed(rule.expression, &sandboxError)) {
      messages.append(QStringLiteral("%1: %2").arg(rule.id, sandboxError));
    }
    if (!std::isfinite(rule.rateLimitHz)
        || rule.rateLimitHz < kMinimumRateHz
        || rule.rateLimitHz > kMaximumRateHz) {
      messages.append(QStringLiteral(
          "%1: rate_hz must be between 1 and 60.").arg(rule.id));
    }
    if (rule.inputs.size() > kMaximumInputsPerRule) {
      messages.append(QStringLiteral(
          "%1: at most 12 inputs are allowed.").arg(rule.id));
    }
    QSet<QChar> variables;
    for (QtedmRuleInput &input : rule.inputs) {
      input.variable = input.variable.toUpper();
      if (input.variable < QLatin1Char('A')
          || input.variable > QLatin1Char('L')
          || variables.contains(input.variable)
          || input.channel.trimmed().isEmpty()) {
        messages.append(QStringLiteral(
            "%1: inputs require unique A-L variables and non-empty PVs.")
                .arg(rule.id));
      }
      variables.insert(input.variable);
      input.channel = input.channel.trimmed();
    }
    if ((rule.property == QtedmRuleProperty::kForeground
            || rule.property == QtedmRuleProperty::kBackground)
        && ((rule.trueValue.isEmpty()
                || !QColor(rule.trueValue).isValid())
            || (rule.falseValue.isEmpty()
                || !QColor(rule.falseValue).isValid()))) {
      messages.append(QStringLiteral(
          "%1: color values must be valid Qt colors.").arg(rule.id));
    }
    if (rule.property == QtedmRuleProperty::kVisible
        || rule.property == QtedmRuleProperty::kEnabled) {
      bool trueOk = false;
      bool falseOk = false;
      boolFromString(rule.trueValue, true, &trueOk);
      boolFromString(rule.falseValue, false, &falseOk);
      if (!trueOk || !falseOk) {
        messages.append(QStringLiteral(
            "%1: Boolean values must be true/false, yes/no, on/off, or 1/0.")
                .arg(rule.id));
      }
    }
    if (rule.property == QtedmRuleProperty::kGeometry) {
      bool trueOk = false;
      bool falseOk = false;
      rectFromString(rule.trueValue, &trueOk);
      rectFromString(rule.falseValue, &falseOk);
      if (!trueOk || !falseOk) {
        messages.append(QStringLiteral(
            "%1: geometry values must be x,y,width,height.")
                .arg(rule.id));
      }
    }
  }

  QVector<int> visit(ruleSet->rules.size(), 0);
  std::function<void(int)> visitRule = [&](int index) {
    if (index < 0 || index >= visit.size() || visit[index] == 2) {
      return;
    }
    if (visit[index] == 1) {
      messages.append(QStringLiteral("Rule dependency cycle includes %1.")
          .arg(ruleSet->rules.at(index).id));
      return;
    }
    visit[index] = 1;
    for (const QString &dependency : ruleSet->rules.at(index).dependsOn) {
      const auto it = indexById.constFind(dependency);
      if (it == indexById.cend()) {
        messages.append(QStringLiteral("%1: missing dependency %2.")
            .arg(ruleSet->rules.at(index).id, dependency));
      } else {
        visitRule(it.value());
      }
    }
    visit[index] = 2;
  };
  for (int index = 0; index < ruleSet->rules.size(); ++index) {
    visitRule(index);
  }

  messages.removeDuplicates();
  ruleSet->diagnostic = messages.join(QLatin1Char('\n'));
  if (diagnostics) {
    *diagnostics = messages;
  }
  return messages.isEmpty();
}

PropertyRuleRuntime::PropertyRuleRuntime(QWidget *target,
    const QtedmRuleSet &ruleSet)
  : QObject(target)
  , target_(target)
  , ruleSet_(ruleSet)
{
  evaluationTimer_.setParent(this);
  evaluationTimer_.setInterval(10);
  QObject::connect(&evaluationTimer_, &QTimer::timeout, this,
      [this]() { processPendingRules(); });
}

PropertyRuleRuntime::~PropertyRuleRuntime()
{
  stop();
}

bool PropertyRuleRuntime::start(QString *error)
{
  if (started_) {
    return true;
  }
  if (!target_) {
    if (error) {
      *error = QStringLiteral("Rule target is unavailable.");
    }
    return false;
  }
  QStringList diagnostics;
  if (!PropertyRules::validate(&ruleSet_, &diagnostics)) {
    diagnostic_ = diagnostics.join(QLatin1Char('\n'));
    if (error) {
      *error = diagnostic_;
    }
    return false;
  }

  for (const QtedmPropertyRule &rule : ruleSet_.rules) {
    const int propertyKey = static_cast<int>(rule.property);
    if (!originalValues_.contains(propertyKey)) {
      originalValues_.insert(propertyKey, captureProperty(rule.property));
    }
    auto state = std::make_unique<RuleState>();
    state->rule = rule;
    state->postfix.fill('\0');
    const QString normalized =
        RuntimeUtils::normalizeCalcExpression(rule.expression);
    QByteArray infix = normalized.toLatin1();
    short parseError = 0;
    state->postfixValid =
        postfix(infix.data(), state->postfix.data(), &parseError) == 0;
    if (!state->postfixValid) {
      diagnostic_ = QStringLiteral("%1: expression compile failed (%2).")
          .arg(rule.id).arg(parseError);
      if (error) {
        *error = diagnostic_;
      }
      states_.clear();
      originalValues_.clear();
      return false;
    }
    states_.push_back(std::move(state));
  }

  started_ = true;
  clock_.start();
  auto &manager = PvChannelManager::instance();
  for (const auto &statePointer : states_) {
    RuleState *state = statePointer.get();
    for (int inputIndex = 0; inputIndex < state->rule.inputs.size();
         ++inputIndex) {
      const QtedmRuleInput &input = state->rule.inputs.at(inputIndex);
      state->subscriptions[static_cast<std::size_t>(inputIndex)] =
          manager.subscribe(input.channel, DBR_TIME_DOUBLE, 1,
              [this, state, inputIndex](const SharedChannelData &data) {
                handleValue(state, inputIndex, data);
              },
              [this, state, inputIndex](bool connected,
                  const SharedChannelData &) {
                handleConnection(state, inputIndex, connected);
              },
              nullptr, ChannelDeliveryMode::kRealtime);
    }
  }
  evaluationTimer_.start();
  processPendingRules();
  return true;
}

void PropertyRuleRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  evaluationTimer_.stop();
  for (auto &state : states_) {
    for (SubscriptionHandle &subscription : state->subscriptions) {
      subscription.reset();
    }
  }
  for (auto it = originalValues_.cbegin(); it != originalValues_.cend();
       ++it) {
    applyProperty(static_cast<QtedmRuleProperty>(it.key()), it.value());
  }
  states_.clear();
  originalValues_.clear();
}

bool PropertyRuleRuntime::isStarted() const
{
  return started_;
}

QString PropertyRuleRuntime::diagnostic() const
{
  return diagnostic_;
}

quint64 PropertyRuleRuntime::evaluationCount() const
{
  quint64 total = 0;
  for (const auto &state : states_) {
    total += state->evaluationCount;
  }
  return total;
}

void PropertyRuleRuntime::handleValue(RuleState *state, int inputIndex,
    const SharedChannelData &data)
{
  if (!started_ || !state || inputIndex < 0
      || inputIndex >= state->rule.inputs.size()) {
    return;
  }
  const QtedmRuleInput &input = state->rule.inputs.at(inputIndex);
  double value = data.numericValue;
  bool valid = data.hasValue && (data.isNumeric || data.isEnum);
  if (input.type == QtedmRuleInputType::kBoolean) {
    value = std::fabs(value) > RuntimeUtils::kVisibilityEpsilon ? 1.0 : 0.0;
  }
  const std::size_t slot = static_cast<std::size_t>(
      input.variable.unicode() - QLatin1Char('A').unicode());
  state->values[slot] = valid ? value : 0.0;
  state->hasValue[slot] = valid;
  state->pending = true;
}

void PropertyRuleRuntime::handleConnection(RuleState *state, int inputIndex,
    bool connected)
{
  if (!started_ || !state || inputIndex < 0
      || inputIndex >= state->rule.inputs.size()) {
    return;
  }
  const QChar variable = state->rule.inputs.at(inputIndex).variable;
  const std::size_t slot = static_cast<std::size_t>(
      variable.unicode() - QLatin1Char('A').unicode());
  state->connected[slot] = connected;
  if (!connected) {
    state->hasValue[slot] = false;
    applyDisconnected(state);
  } else {
    state->pending = true;
  }
}

void PropertyRuleRuntime::processPendingRules()
{
  if (!started_ || !target_) {
    return;
  }
  const qint64 nowMs = clock_.elapsed();
  for (const auto &state : states_) {
    if (!state->pending) {
      continue;
    }
    const qint64 interval = std::max<qint64>(1,
        static_cast<qint64>(std::ceil(1000.0
            / std::clamp(state->rule.rateLimitHz,
                kMinimumRateHz, kMaximumRateHz))));
    if (state->lastEvaluationMs >= 0
        && nowMs - state->lastEvaluationMs < interval) {
      continue;
    }
    if (!allInputsReady(*state)) {
      applyDisconnected(state.get());
      state->pending = false;
      continue;
    }
    evaluateRule(state.get(), nowMs);
  }
}

void PropertyRuleRuntime::evaluateRule(RuleState *state, qint64 nowMs)
{
  if (!state || !state->postfixValid) {
    return;
  }
  std::array<double, RuntimeUtils::kCalcInputCount> args{};
  args = state->values;
  bool hasGInput = false;
  for (const QtedmRuleInput &input : state->rule.inputs) {
    if (input.variable == QLatin1Char('G')) {
      hasGInput = true;
      break;
    }
  }
  if (!hasGInput) {
    args[6] = 1.0;
  }
  double result = 0.0;
  const long status =
      calcPerform(args.data(), &result, state->postfix.data());
  state->lastEvaluationMs = nowMs;
  state->pending = false;
  ++state->evaluationCount;
  if (status != 0 || !std::isfinite(result)) {
    diagnostic_ = QStringLiteral("%1: evaluation failed.").arg(state->rule.id);
    applyDisconnected(state);
    return;
  }
  applyResult(state->rule, result);
}

bool PropertyRuleRuntime::allInputsReady(const RuleState &state) const
{
  for (const QtedmRuleInput &input : state.rule.inputs) {
    const std::size_t slot = static_cast<std::size_t>(
        input.variable.unicode() - QLatin1Char('A').unicode());
    if (!state.connected[slot] || !state.hasValue[slot]) {
      return false;
    }
  }
  return true;
}

void PropertyRuleRuntime::applyDisconnected(RuleState *state)
{
  if (!state) {
    return;
  }
  if (state->rule.disconnectBehavior
      == QtedmRuleDisconnectBehavior::kRestore) {
    restoreProperty(state->rule.property);
  } else {
    applyResult(state->rule, 0.0);
  }
}

void PropertyRuleRuntime::applyResult(const QtedmPropertyRule &rule,
    double result)
{
  const bool condition =
      std::fabs(result) > RuntimeUtils::kVisibilityEpsilon;
  switch (rule.property) {
  case QtedmRuleProperty::kVisible:
  case QtedmRuleProperty::kEnabled: {
    bool ok = false;
    const bool selected = boolFromString(
        condition ? rule.trueValue : rule.falseValue, condition, &ok);
    if (ok) {
      applyProperty(rule.property, selected);
    }
    break;
  }
  case QtedmRuleProperty::kText: {
    const QString selected = condition ? rule.trueValue : rule.falseValue;
    applyProperty(rule.property, selected.isEmpty()
        ? QVariant(QString::number(result, 'g', 15)) : QVariant(selected));
    break;
  }
  case QtedmRuleProperty::kForeground:
  case QtedmRuleProperty::kBackground:
    applyProperty(rule.property,
        QColor(condition ? rule.trueValue : rule.falseValue));
    break;
  case QtedmRuleProperty::kGeometry: {
    bool ok = false;
    const QRect rect = rectFromString(
        condition ? rule.trueValue : rule.falseValue, &ok);
    if (ok) {
      applyProperty(rule.property, rect);
    }
    break;
  }
  }
}

void PropertyRuleRuntime::restoreProperty(QtedmRuleProperty property)
{
  const auto it = originalValues_.constFind(static_cast<int>(property));
  if (it != originalValues_.cend()) {
    applyProperty(property, it.value());
  }
}

QVariant PropertyRuleRuntime::captureProperty(
    QtedmRuleProperty property) const
{
  if (!target_) {
    return {};
  }
  switch (property) {
  case QtedmRuleProperty::kVisible:
    return target_->isVisible();
  case QtedmRuleProperty::kEnabled:
    return target_->isEnabled();
  case QtedmRuleProperty::kText:
    return widgetText(target_);
  case QtedmRuleProperty::kForeground:
    return target_->palette().color(QPalette::WindowText);
  case QtedmRuleProperty::kBackground:
    return target_->palette().color(QPalette::Window);
  case QtedmRuleProperty::kGeometry:
    return target_->geometry();
  }
  return {};
}

bool PropertyRuleRuntime::applyProperty(QtedmRuleProperty property,
    const QVariant &value, QString *error)
{
  if (!target_) {
    return false;
  }
  switch (property) {
  case QtedmRuleProperty::kVisible:
    target_->setVisible(value.toBool());
    return true;
  case QtedmRuleProperty::kEnabled:
    target_->setEnabled(value.toBool());
    return true;
  case QtedmRuleProperty::kText:
    if (setWidgetText(target_, value.toString())) {
      return true;
    }
    break;
  case QtedmRuleProperty::kForeground: {
    const QColor color = value.value<QColor>();
    if (color.isValid()) {
      QPalette palette = target_->palette();
      palette.setColor(QPalette::WindowText, color);
      palette.setColor(QPalette::Text, color);
      palette.setColor(QPalette::ButtonText, color);
      target_->setPalette(palette);
      target_->update();
      return true;
    }
    break;
  }
  case QtedmRuleProperty::kBackground: {
    const QColor color = value.value<QColor>();
    if (color.isValid()) {
      QPalette palette = target_->palette();
      palette.setColor(QPalette::Window, color);
      palette.setColor(QPalette::Base, color);
      palette.setColor(QPalette::Button, color);
      target_->setPalette(palette);
      target_->setAutoFillBackground(true);
      target_->update();
      return true;
    }
    break;
  }
  case QtedmRuleProperty::kGeometry:
    if (value.canConvert<QRect>()) {
      const QRect rect = value.toRect();
      if (rect.width() > 0 && rect.height() > 0) {
        target_->setGeometry(rect);
        return true;
      }
    }
    break;
  }
  if (error) {
    *error = QStringLiteral("Target does not support rule property %1.")
        .arg(PropertyRules::propertyName(property));
  }
  return false;
}
