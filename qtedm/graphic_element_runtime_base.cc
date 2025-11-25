#include "graphic_element_runtime_base.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <QDebug>

#include <db_access.h>

#include "channel_access_context.h"
#include "runtime_utils.h"
#include "shared_channel_manager.h"

/* External C functions for MEDM calc expression evaluation */
extern "C" {
long calcPerform(double *parg, double *presult, char *post);
long postfix(char *pinfix, char *ppostfix, short *perror);
}

namespace {
using RuntimeUtils::kInvalidSeverity;
using RuntimeUtils::kVisibilityEpsilon;
using RuntimeUtils::kCalcInputCount;

} // namespace

template <typename ElementType, size_t ChannelCount>
GraphicElementRuntimeBase<ElementType, ChannelCount>::GraphicElementRuntimeBase(
    ElementType *element)
  : QObject(element)
  , element_(element)
{
  for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
    channels_[i].index = i;
  }
}

template <typename ElementType, size_t ChannelCount>
GraphicElementRuntimeBase<ElementType, ChannelCount>::~GraphicElementRuntimeBase()
{
  stop();
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::start()
{
  if (started_ || !element_) {
    return;
  }

  ChannelAccessContext &context = ChannelAccessContext::instance();
  context.ensureInitialized();
  if (!context.isInitialized()) {
    qWarning() << "Channel Access context not available";
    return;
  }

  resetState();
  started_ = true;

  /* Check if any channel is specified (mimic MEDM behavior) */
  bool hasChannel = false;
  for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
    if (!element_->channel(i).trimmed().isEmpty()) {
      hasChannel = true;
      break;
    }
  }

  /* Channels are needed only if a channel is specified AND
   * (color mode is dynamic OR visibility mode is dynamic) */
  channelsNeeded_ = hasChannel
      && ((element_->colorMode() != TextColorMode::kStatic)
          || (element_->visibilityMode() != TextVisibilityMode::kStatic));
  layeringNeeded_ = channelsNeeded_;
  if (!layeringNeeded_ && hasChannel
      && ElementLayeringTraits<ElementType>::kLayerOnAnyChannel) {
    layeringNeeded_ = true;
  }

  if (element_->visibilityMode() == TextVisibilityMode::kCalc) {
    const QString calcExpr = element_->visibilityCalc().trimmed();
    if (!calcExpr.isEmpty()) {
      /* Normalize expression: convert == to = and != to # for MEDM calc engine */
      QString normalized = RuntimeUtils::normalizeCalcExpression(calcExpr);
      QByteArray infix = normalized.toLatin1();
      calcPostfix_.resize(512);
      calcPostfix_.fill('\0');
      short error = 0;
      long status = postfix(infix.data(), calcPostfix_.data(), &error);
      if (status == 0) {
        calcValid_ = true;
      } else {
        calcValid_ = false;
        qWarning() << "Invalid visibility calc expression for" << elementTypeName() << ":"
                   << calcExpr << "(error" << error << ')';
      }
    }
  }

  initializeChannels();
  evaluateState();
  onStart();
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::stop()
{
  if (!started_) {
    return;
  }

  onStop();
  started_ = false;
  cleanupChannels();
  resetState();
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::resetState()
{
  calcPostfix_.clear();
  calcValid_ = false;
  channelsNeeded_ = true;
  layeringNeeded_ = true;

  for (auto &channel : channels_) {
    channel.name.clear();
    channel.subscription.reset();
    channel.connected = false;
    channel.hasValue = false;
    channel.hasControlInfo = false;
    channel.value = 0.0;
    channel.severity = 0;
    channel.status = 0;
    channel.hopr = 0.0;
    channel.lopr = 0.0;
    channel.precision = -1;
    channel.elementCount = 1;
  }

  if (element_) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(0);
    element_->setRuntimeVisible(true);
  }
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::initializeChannels()
{
  if (!element_) {
    return;
  }
  if (!channelsNeeded_) {
    for (auto &channel : channels_) {
      channel.name.clear();
    }
    return;
  }

  auto &mgr = SharedChannelManager::instance();

  for (auto &channel : channels_) {
    channel.name = element_->channel(channel.index).trimmed();
    if (channel.name.isEmpty()) {
      continue;
    }

    const int idx = channel.index;
    channel.subscription = mgr.subscribe(
        channel.name,
        DBR_TIME_DOUBLE,  /* Graphic elements use double for visibility calc */
        1,                /* Single element for visibility/color logic */
        [this, idx](const SharedChannelData &data) {
          handleChannelData(idx, data);
        },
        [this, idx](bool connected) {
          handleChannelConnection(idx, connected);
        });
  }
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::cleanupChannels()
{
  for (auto &channel : channels_) {
    if (channel.connected) {
      onChannelDisconnected(channel.index);
    }
    channel.subscription.reset();
  }
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::handleChannelConnection(
    int channelIndex, bool connected)
{
  if (!started_ || channelIndex < 0
      || channelIndex >= static_cast<int>(channels_.size())) {
    return;
  }

  ChannelRuntime &channel = channels_[channelIndex];

  if (connected) {
    channel.connected = true;
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
    channel.status = 0;
    onChannelConnected(channelIndex);
  } else {
    channel.connected = false;
    channel.hasValue = false;
    channel.value = 0.0;
    channel.severity = 0;
    channel.status = 0;
    onChannelDisconnected(channelIndex);
  }

  evaluateState();
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::handleChannelData(
    int channelIndex, const SharedChannelData &data)
{
  if (!started_ || channelIndex < 0
      || channelIndex >= static_cast<int>(channels_.size())) {
    return;
  }

  ChannelRuntime &channel = channels_[channelIndex];

  /* Extract numeric value from shared data */
  channel.value = data.numericValue;
  channel.severity = data.severity;
  channel.status = data.status;
  channel.hasValue = data.hasValue;
  channel.elementCount = data.nativeElementCount;

  /* Copy control info if available */
  if (data.hasControlInfo) {
    channel.hopr = data.hopr;
    channel.lopr = data.lopr;
    channel.precision = data.precision;
    channel.hasControlInfo = true;
  }

  if (channelIndex == 0 && element_) {
    element_->setRuntimeSeverity(data.severity);
  }

  evaluateState();
}

template <typename ElementType, size_t ChannelCount>
void GraphicElementRuntimeBase<ElementType, ChannelCount>::evaluateState()
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
    element_->setRuntimeConnected(true);
    element_->setRuntimeSeverity(0);
    element_->setRuntimeVisible(true);
    return;
  }

  if (!allConnected) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(kInvalidSeverity);
    element_->setRuntimeVisible(true);
    return;
  }

  ChannelRuntime &primary = channels_.front();
  element_->setRuntimeConnected(true);
  element_->setRuntimeSeverity(primary.severity);

  bool visible = true;
  switch (element_->visibilityMode()) {
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

  element_->setRuntimeVisible(visible);
  onStateEvaluated();
}

template <typename ElementType, size_t ChannelCount>
bool GraphicElementRuntimeBase<ElementType, ChannelCount>::evaluateCalcExpression(
    double &result) const
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

  const ChannelRuntime &primary = channels_[0];
  args[6] = static_cast<double>(std::max<long>(primary.elementCount, 1));
  args[7] = primary.hopr;
  args[8] = static_cast<double>(primary.status);
  args[9] = static_cast<double>(primary.severity);
  args[10] = static_cast<double>(primary.precision >= 0 ? primary.precision : 0);
  args[11] = primary.lopr;

  long status = calcPerform(args, &result,
      const_cast<char *>(calcPostfix_.constData()));
  return status == 0;
}

/* Explicit template instantiations for the element types that use this base class.
 * This allows the implementation to remain in the .cc file while still being
 * accessible to derived classes. */
#include "rectangle_element.h"
#include "oval_element.h"
#include "arc_element.h"
#include "line_element.h"
#include "polygon_element.h"
#include "polyline_element.h"
#include "image_element.h"
#include "text_element.h"

template class GraphicElementRuntimeBase<RectangleElement, 5>;
template class GraphicElementRuntimeBase<OvalElement, 5>;
template class GraphicElementRuntimeBase<ArcElement, 5>;
template class GraphicElementRuntimeBase<LineElement, 5>;
template class GraphicElementRuntimeBase<PolygonElement, 5>;
template class GraphicElementRuntimeBase<PolylineElement, 5>;
template class GraphicElementRuntimeBase<ImageElement, 5>;
template class GraphicElementRuntimeBase<TextElement, 5>;
