#include "composite_runtime.h"
#include "composite_element.h"
#include "display_properties.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "statistics_tracker.h"

#include <QDebug>

#include <cmath>

#include <db_access.h>

extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {

constexpr int kCalcInputCount = 12;
constexpr double kVisibilityEpsilon = 1e-9;

/* Normalize calc expression to MEDM calc engine syntax.
 * MEDM calc uses single '=' for equality (not '==') and '#' for inequality (not '!=').
 * This function converts modern C-style operators to MEDM syntax. */
QString normalizeCalcExpression(const QString &expr)
{
  QString result = expr;
  /* Replace != with # (must do this before replacing ==) */
  result.replace("!=", "#");
  /* Replace == with = */
  result.replace("==", "=");
  return result;
}

}

CompositeRuntime::CompositeRuntime(CompositeElement *element)
  : QObject(nullptr)
  , element_(element)
{
}

CompositeRuntime::~CompositeRuntime()
{
  stop();
}

void CompositeRuntime::start()
{
  if (started_ || !element_) {
    return;
  }
  started_ = true;

  /* Parse calc expression if visibility mode is calc */
  if (element_->visibilityMode() == TextVisibilityMode::kCalc) {
    const QString calcExpr = element_->visibilityCalc().trimmed();
    if (!calcExpr.isEmpty()) {
      QString normalized = normalizeCalcExpression(calcExpr);
      QByteArray infix = normalized.toLatin1();
      calcPostfix_.resize(512);
      calcPostfix_.fill('\0');
      short error = 0;
      long status = postfix(infix.data(), calcPostfix_.data(), &error);
      if (status == 0) {
        calcValid_ = true;
      } else {
        calcValid_ = false;
        qWarning() << "CompositeRuntime: Invalid calc expression:"
                   << calcExpr;
      }
    }
  }

  resetState();
  initializeChannels();
}

void CompositeRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  cleanupChannels();
  resetState();
  calcPostfix_.clear();
  calcValid_ = false;
}

void CompositeRuntime::resetState()
{
  /* Reset connection state */
  for (auto &channel : channels_) {
    channel.connected = false;
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
  }
}

void CompositeRuntime::initializeChannels()
{
  if (!element_) {
    return;
  }

  /* Only monitor channels if visibility mode is not static */
  const TextVisibilityMode visibilityMode = element_->visibilityMode();
  if (visibilityMode == TextVisibilityMode::kStatic) {
    /* Static mode - always visible and connected */
    element_->setChannelConnected(true);
    element_->setRuntimeVisible(true);
    return;
  }

    bool needsCa = false;
    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
      const QString channelName = element_->channel(i).trimmed();
      if (channelName.isEmpty()) {
        continue;
      }
      if (parsePvName(channelName).protocol == PvProtocol::kCa) {
        needsCa = true;
        break;
      }
    }
    if (needsCa) {
      ChannelAccessContext::instance().ensureInitializedForProtocol(PvProtocol::kCa);
      if (!ChannelAccessContext::instance().isInitialized()) {
        qWarning() << "Channel Access context not available";
        return;
      }
    }

  auto &mgr = PvChannelManager::instance();

  /* Create subscriptions for all non-empty channel names */
  for (std::size_t i = 0; i < channels_.size(); ++i) {
    const QString channelName = element_->channel(static_cast<int>(i)).trimmed();
    if (channelName.isEmpty()) {
      continue;
    }
    channels_[i].name = channelName;

    const int index = static_cast<int>(i);
    channels_[i].subscription = mgr.subscribe(
        channelName,
        DBR_TIME_DOUBLE,
        1,  /* Single element */
        [this, index](const SharedChannelData &data) {
          handleChannelValue(index, data);
        },
        [this, index](bool connected, const SharedChannelData &) {
          handleChannelConnection(index, connected);
        });
  }
}

void CompositeRuntime::cleanupChannels()
{
  for (auto &channel : channels_) {
    channel.subscription.reset();
    channel.name.clear();
    channel.connected = false;
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
  }
}

void CompositeRuntime::handleChannelConnection(int index, bool connected)
{
  if (!started_ || index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }

  auto &channel = channels_[static_cast<std::size_t>(index)];
  channel.connected = connected;

  if (!connected) {
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
  }

  evaluateVisibility();
}

void CompositeRuntime::handleChannelValue(int index, const SharedChannelData &data)
{
  if (!started_ || index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }

  auto &channel = channels_[static_cast<std::size_t>(index)];
  channel.value = data.numericValue;
  channel.severity = data.severity;
  channel.hasValue = data.hasValue;

  evaluateVisibility();
}

void CompositeRuntime::evaluateVisibility()
{
  if (!element_) {
    return;
  }

  bool anyChannels = false;
  bool allConnected = true;
  for (const auto &channel : channels_) {
    if (channel.name.isEmpty()) {
      continue;
    }
    anyChannels = true;
    if (!channel.connected) {
      allConnected = false;
      break;
    }
  }

  if (!anyChannels) {
    element_->setChannelConnected(true);
    element_->setRuntimeVisible(true);
    return;
  }

  if (!allConnected) {
    element_->setChannelConnected(false);
    element_->setRuntimeVisible(true);
    return;
  }

  const TextVisibilityMode visibilityMode = element_->visibilityMode();
  const ChannelState &primary = channels_[0];

  bool visible = true;
  switch (visibilityMode) {
  case TextVisibilityMode::kStatic:
    visible = true;
    break;
  case TextVisibilityMode::kIfNotZero:
    visible = std::fabs(primary.value) > kVisibilityEpsilon;
    break;
  case TextVisibilityMode::kIfZero:
    visible = std::fabs(primary.value) <= kVisibilityEpsilon;
    break;
  case TextVisibilityMode::kCalc: {
    double result = 0.0;
    if (calcValid_ && evaluateCalcExpression(result)) {
      visible = std::fabs(result) > kVisibilityEpsilon;
    } else {
      visible = false;
    }
    break;
  }
  }

  element_->setChannelConnected(true);
  element_->setRuntimeVisible(visible);
}

bool CompositeRuntime::evaluateCalcExpression(double &result) const
{
  if (!calcValid_ || calcPostfix_.isEmpty()) {
    return false;
  }

  double args[kCalcInputCount] = {0.0};
  args[0] = channels_[0].value;
  args[1] = channels_[1].value;
  args[2] = channels_[2].value;
  args[3] = channels_[3].value;
  args[4] = 0.0;
  args[5] = 0.0;
  args[6] = 0.0;
  args[7] = 0.0;
  args[8] = 0.0;
  args[9] = 0.0;
  args[10] = 0.0;
  args[11] = 0.0;

  long status = calcPerform(args, &result,
      const_cast<char *>(calcPostfix_.constData()));
  return status == 0;
}
