#include "expression_channel_runtime.h"

#include <cmath>

#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "expression_channel_element.h"
#include "runtime_utils.h"
#include "soft_pv_registry.h"

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {

using RuntimeUtils::kVisibilityEpsilon;
using RuntimeUtils::kCalcInputCount;

} // namespace

ExpressionChannelRuntime::ExpressionChannelRuntime(
    ExpressionChannelElement *element)
  : QObject(element)
  , element_(element)
{
}

ExpressionChannelRuntime::~ExpressionChannelRuntime()
{
  stop();
}

void ExpressionChannelRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  bool needsCa = false;
  for (int i = 0; i < static_cast<int>(subscriptions_.size()); ++i) {
    const QString channelName = element_->channel(i).trimmed();
    if (channelName.isEmpty()) {
      continue;
    }
    const ParsedPvName parsed = parsePvName(channelName);
    if (parsed.protocol == PvProtocol::kCa
        && !SoftPvRegistry::instance().isRegistered(parsed.pvName)) {
      needsCa = true;
      break;
    }
  }
  if (needsCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available";
      return;
    }
  }

  started_ = true;
  postfix_.fill('\0');
  postfixValid_ = false;
  firstEvaluationPublished_ = false;
  hasLastEvaluatedResult_ = false;
  hasLastPublishedResult_ = false;
  outputName_ = element_->resolvedVariableName();

  SoftPvRegistry::instance().registerName(outputName_);
  SoftPvRegistry::instance().setConnected(outputName_, true);

  const QString calcExpression = element_->calc().trimmed();
  if (!calcExpression.isEmpty()) {
    QString normalized = RuntimeUtils::normalizeCalcExpression(calcExpression);
    QByteArray infix = normalized.toLatin1();
    short error = 0;
    const long status = postfix(infix.data(), postfix_.data(), &error);
    if (status == 0) {
      postfixValid_ = true;
    } else {
      qWarning() << "ExpressionChannelRuntime: invalid calc expression"
                 << calcExpression << "(error" << error << ')';
    }
  } else {
    qWarning() << "ExpressionChannelRuntime: empty calc expression for"
               << outputName_;
  }

  auto &manager = PvChannelManager::instance();
  for (int i = 0; i < static_cast<int>(subscriptions_.size()); ++i) {
    const QString channelName = element_->channel(i).trimmed();
    if (channelName.isEmpty()) {
      continue;
    }
    subscriptions_[static_cast<std::size_t>(i)] = manager.subscribe(
        channelName,
        DBR_TIME_DOUBLE,
        1,
        [this, i](const SharedChannelData &data) {
          handleChannelData(i, data);
        },
        [this, i](bool connected, const SharedChannelData &) {
          handleChannelConnection(i, connected);
        });
  }

  const double initialValue = element_->initialValue();
  SoftPvRegistry::instance().publishValue(outputName_, initialValue);
  lastPublishedResult_ = initialValue;
  hasLastPublishedResult_ = true;
}

void ExpressionChannelRuntime::stop()
{
  if (!started_) {
    return;
  }

  for (SubscriptionHandle &subscription : subscriptions_) {
    subscription.reset();
  }

  SoftPvRegistry::instance().setConnected(outputName_, false);
  SoftPvRegistry::instance().unregisterName(outputName_);

  outputName_.clear();
  postfix_.fill('\0');
  postfixValid_ = false;
  started_ = false;
  firstEvaluationPublished_ = false;
  hasLastEvaluatedResult_ = false;
  hasLastPublishedResult_ = false;
  values_.fill(0.0);
  connected_.fill(false);
}

void ExpressionChannelRuntime::handleChannelData(int index,
    const SharedChannelData &data)
{
  if (!started_ || index < 0
      || index >= static_cast<int>(values_.size())) {
    return;
  }

  if (data.hasValue && std::isfinite(data.numericValue)) {
    values_[static_cast<std::size_t>(index)] = data.numericValue;
  } else if (!data.hasValue) {
    values_[static_cast<std::size_t>(index)] = 0.0;
  }

  evaluateAndMaybePublish();
}

void ExpressionChannelRuntime::handleChannelConnection(int index, bool connected)
{
  if (index < 0 || index >= static_cast<int>(connected_.size())) {
    return;
  }
  connected_[static_cast<std::size_t>(index)] = connected;
}

void ExpressionChannelRuntime::evaluateAndMaybePublish()
{
  if (!started_ || !postfixValid_ || outputName_.isEmpty()) {
    return;
  }

  std::array<double, kCalcInputCount> args{};
  for (int i = 0; i < 4; ++i) {
    args[static_cast<std::size_t>(i)] = values_[static_cast<std::size_t>(i)];
  }
  args[6] = 1.0;

  double result = 0.0;
  const long status = calcPerform(args.data(), &result, postfix_.data());
  if (status != 0 || !std::isfinite(result)) {
    return;
  }

  bool shouldPublish = false;
  switch (element_->eventSignalMode()) {
  case ExpressionChannelEventSignalMode::kNever:
    break;
  case ExpressionChannelEventSignalMode::kOnFirstChange:
    shouldPublish = !firstEvaluationPublished_;
    break;
  case ExpressionChannelEventSignalMode::kOnAnyChange:
    shouldPublish = !hasLastPublishedResult_
        || std::fabs(result - lastPublishedResult_) > kVisibilityEpsilon;
    break;
  case ExpressionChannelEventSignalMode::kTriggerZeroToOne:
    shouldPublish = hasLastEvaluatedResult_
        && lastEvaluatedResult_ <= 0.0 && result > 0.0;
    break;
  case ExpressionChannelEventSignalMode::kTriggerOneToZero:
    shouldPublish = hasLastEvaluatedResult_
        && lastEvaluatedResult_ > 0.0 && result <= 0.0;
    break;
  }

  lastEvaluatedResult_ = result;
  hasLastEvaluatedResult_ = true;

  if (!shouldPublish) {
    return;
  }

  SoftPvRegistry::instance().publishValue(outputName_, result);
  firstEvaluationPublished_ = true;
  lastPublishedResult_ = result;
  hasLastPublishedResult_ = true;
}
