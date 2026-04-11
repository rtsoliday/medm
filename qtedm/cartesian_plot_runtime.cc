#include "cartesian_plot_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QtGlobal>
#include <QtMath>

#include <db_access.h>
#include <epicsTime.h>

#include "cartesian_plot_element.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"

namespace {

using RuntimeUtils::isNumericFieldType;

constexpr qint64 kUnixEpicsEpochOffsetSeconds = 631152000LL;
constexpr int kPeriodicSampleIntervalMs = 1000;

qint64 epicsTimestampToMs(const epicsTimeStamp &stamp)
{
  const qint64 seconds = static_cast<qint64>(stamp.secPastEpoch)
      + kUnixEpicsEpochOffsetSeconds;
  return seconds * 1000LL + stamp.nsec / 1000000LL;
}

int axisIndexForYAxis(CartesianPlotYAxis axis)
{
  switch (axis) {
  case CartesianPlotYAxis::kY2:
    return 2;
  case CartesianPlotYAxis::kY3:
    return 3;
  case CartesianPlotYAxis::kY4:
    return 4;
  case CartesianPlotYAxis::kY1:
  default:
    return 1;
  }
}

#if MEDM_CARTESIAN_PLOT_DEBUG
static const char *axisNameForIndex(int axisIndex)
{
  switch (axisIndex) {
  case 0:
    return "X";
  case 1:
    return "Y1";
  case 2:
    return "Y2";
  case 3:
    return "Y3";
  case 4:
    return "Y4";
  default:
    return "Y?";
  }
}

static const char *axisSideString(int axisIndex, const CartesianPlotElement *element)
{
  if (!element) {
    return "Unknown";
  }
  if (axisIndex == 0) {
    return "Bottom";
  }
  const bool isLeft = element->isAxisDrawnOnLeft(axisIndex);
  return isLeft ? "Left" : "Right";
}

static const char *axisRangeStyleString(CartesianPlotRangeStyle style)
{
  switch (style) {
  case CartesianPlotRangeStyle::kChannel:
    return "channel";
  case CartesianPlotRangeStyle::kUserSpecified:
    return "user";
  case CartesianPlotRangeStyle::kAutoScale:
    return "auto";
  default:
    return "unknown";
  }
}

static void printAxisDebugInfo(const CartesianPlotElement *element,
    int axisIndex, double minValue, double maxValue, bool valid,
    const char *sourceLabel)
{
  if (!element || !valid) {
    return;
  }
  const char *axisName = axisNameForIndex(axisIndex);
  const char *sideName = axisSideString(axisIndex, element);
  const QString label = element->axisLabel(axisIndex);
  QByteArray labelUtf8 = label.toUtf8();
  printf("QTEDM DEBUG widget=%p axis=%s label=\"%s\" side=%s range=[%g, %g] (%s)\n",
      static_cast<const void *>(element), axisName,
      label.isEmpty() ? "<none>" : labelUtf8.constData(),
      sideName, minValue, maxValue, sourceLabel);
}

static void printRuntimeAxisInfo(const CartesianPlotElement *element,
    int axisIndex, double minValue, double maxValue, bool valid)
{
  printAxisDebugInfo(element, axisIndex, minValue, maxValue, valid,
      "runtime");
}

static void printConfiguredAxisInfo(const CartesianPlotElement *element,
    int axisIndex, double minValue, double maxValue, bool valid,
    CartesianPlotRangeStyle style)
{
  const char *source = axisRangeStyleString(style);
  printAxisDebugInfo(element, axisIndex, minValue, maxValue, valid,
      source);
}
#else
static inline void printRuntimeAxisInfo(const CartesianPlotElement *element,
  int axisIndex, double minValue, double maxValue, bool valid)
{
  Q_UNUSED(element);
  Q_UNUSED(axisIndex);
  Q_UNUSED(minValue);
  Q_UNUSED(maxValue);
  Q_UNUSED(valid);
}

static inline void printConfiguredAxisInfo(const CartesianPlotElement *element,
  int axisIndex, double minValue, double maxValue, bool valid,
  CartesianPlotRangeStyle style)
{
  Q_UNUSED(element);
  Q_UNUSED(axisIndex);
  Q_UNUSED(minValue);
  Q_UNUSED(maxValue);
  Q_UNUSED(valid);
  Q_UNUSED(style);
}
#endif

} // namespace

CartesianPlotRuntime::CartesianPlotRuntime(CartesianPlotElement *element)
  : QObject(element)
  , element_(element)
{
  periodicSampleTimer_.setInterval(kPeriodicSampleIntervalMs);
  periodicSampleTimer_.setSingleShot(false);
  connect(&periodicSampleTimer_, &QTimer::timeout,
      this, &CartesianPlotRuntime::handlePeriodicSampleTimeout);
  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    xContexts_[i].runtime = this;
    xContexts_[i].traceIndex = i;
    xContexts_[i].kind = ChannelKind::kTraceX;
    yContexts_[i].runtime = this;
    yContexts_[i].traceIndex = i;
    yContexts_[i].kind = ChannelKind::kTraceY;
  }
  triggerContext_.runtime = this;
  triggerContext_.kind = ChannelKind::kTrigger;
  eraseContext_.runtime = this;
  eraseContext_.kind = ChannelKind::kErase;
  countContext_.runtime = this;
  countContext_.kind = ChannelKind::kCount;
}

CartesianPlotRuntime::~CartesianPlotRuntime()
{
  stop();
}

void CartesianPlotRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  bool needsCa = false;
  for (int i = 0; i < kCartesianPlotTraceCount && !needsCa; ++i) {
    const QString xName = element_->traceXChannel(i).trimmed();
    const QString yName = element_->traceYChannel(i).trimmed();
    if (!xName.isEmpty() && parsePvName(xName).protocol == PvProtocol::kCa) {
      needsCa = true;
      break;
    }
    if (!yName.isEmpty() && parsePvName(yName).protocol == PvProtocol::kCa) {
      needsCa = true;
      break;
    }
  }
  if (!needsCa) {
    const QString triggerName = element_->triggerChannel().trimmed();
    const QString eraseName = element_->eraseChannel().trimmed();
    const QString countName = element_->countChannel().trimmed();
    if (!triggerName.isEmpty() && parsePvName(triggerName).protocol == PvProtocol::kCa) {
      needsCa = true;
    } else if (!eraseName.isEmpty() && parsePvName(eraseName).protocol == PvProtocol::kCa) {
      needsCa = true;
    } else if (!countName.isEmpty() && parsePvName(countName).protocol == PvProtocol::kCa) {
      needsCa = true;
    }
  }

  if (needsCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available for Cartesian Plot";
      return;
    }
  }

  eraseOldest_ = element_->eraseOldest();
  eraseMode_ = element_->eraseMode();
  configuredCount_ = element_->count();
  countFromChannel_ = 0;
  configuredAxesLogged_ = false;

  started_ = true;

  invokeOnElement([](CartesianPlotElement *element) {
    element->clearRuntimeState();
  });

  resetState();
  logConfiguredAxisState();

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    TraceState &trace = traces_[i];
    trace.mode = CartesianPlotTraceMode::kNone;
    trace.scalarPoints.clear();
    trace.xScalarValues.clear();
    trace.yScalarValues.clear();
    trace.xVector.clear();
    trace.yVector.clear();
    trace.vectorPoints.clear();
    trace.hasXScalar = false;
    trace.hasYScalar = false;
    trace.pendingTrigger = false;
    trace.yAxisIndex = axisIndexForYAxis(element_->traceYAxis(i));

    trace.x.name = element_->traceXChannel(i).trimmed();
    trace.y.name = element_->traceYChannel(i).trimmed();

    if (!trace.x.name.isEmpty()) {
      createTraceChannel(i, ChannelKind::kTraceX, trace.x, xContexts_[i]);
    }
    if (!trace.y.name.isEmpty()) {
      createTraceChannel(i, ChannelKind::kTraceY, trace.y, yContexts_[i]);
    }

    invokeOnElement([i](CartesianPlotElement *element) {
      element->setTraceRuntimeMode(i, CartesianPlotTraceMode::kNone);
      element->setTraceRuntimeConnected(i, false);
      element->clearTraceRuntimeData(i);
    });
  }

  triggerChannel_.name = element_->triggerChannel().trimmed();
  if (!triggerChannel_.name.isEmpty()) {
    createAuxiliaryChannel(ChannelKind::kTrigger, triggerChannel_,
        triggerContext_);
  }

  eraseChannel_.name = element_->eraseChannel().trimmed();
  if (!eraseChannel_.name.isEmpty()) {
    createAuxiliaryChannel(ChannelKind::kErase, eraseChannel_,
        eraseContext_);
  }

  countChannel_.name = element_->countChannel().trimmed();
  if (!countChannel_.name.isEmpty()) {
    createAuxiliaryChannel(ChannelKind::kCount, countChannel_,
        countContext_);
  }

  if (triggerChannel_.name.isEmpty()) {
    periodicSampleTimer_.start();
  } else {
    periodicSampleTimer_.stop();
  }

}

void CartesianPlotRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  periodicSampleTimer_.stop();

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    unsubscribeChannel(traces_[i].x);
    unsubscribeChannel(traces_[i].y);
  }
  unsubscribeChannel(triggerChannel_);
  unsubscribeChannel(eraseChannel_);
  unsubscribeChannel(countChannel_);


  invokeOnElement([](CartesianPlotElement *element) {
    element->clearRuntimeState();
  });
}

void CartesianPlotRuntime::resetState()
{
  for (TraceState &trace : traces_) {
    trace.scalarPoints.clear();
    trace.xScalarValues.clear();
    trace.yScalarValues.clear();
    trace.xVector.clear();
    trace.yVector.clear();
    trace.vectorPoints.clear();
    trace.hasXScalar = false;
    trace.hasYScalar = false;
    trace.lastYTimestampMs = 0;
    trace.hasYTimestamp = false;
    trace.pendingTrigger = false;
    trace.initialSnapshotPending = true;
    trace.lastXScalar = 0.0;
    trace.lastYScalar = 0.0;
    trace.x.connected = false;
    trace.y.connected = false;
    trace.x.readAccessKnown = false;
    trace.x.canRead = false;
    trace.x.hasControlInfo = false;
    trace.x.precisionValid = false;
    trace.x.runtimeLimitsValid = false;
    trace.x.runtimeLow = 0.0;
    trace.x.runtimeHigh = 0.0;
    trace.x.subscription.reset();
    trace.x.fieldType = -1;
    trace.x.elementCount = 0;
    trace.y.readAccessKnown = false;
    trace.y.canRead = false;
    trace.y.hasControlInfo = false;
    trace.y.precisionValid = false;
    trace.y.runtimeLimitsValid = false;
    trace.y.runtimeLow = 0.0;
    trace.y.runtimeHigh = 0.0;
    trace.y.subscription.reset();
    trace.y.fieldType = -1;
    trace.y.elementCount = 0;
  }
  triggerChannel_ = ChannelState{};
  eraseChannel_ = ChannelState{};
  countChannel_ = ChannelState{};
}

void CartesianPlotRuntime::logConfiguredAxisState()
{
  if (configuredAxesLogged_) {
    return;
  }
  configuredAxesLogged_ = true;
  invokeOnElement([](CartesianPlotElement *element) {
    if (!element) {
      return;
    }
    for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
      const auto style = element->axisRangeStyle(axis);
      const double minVal = element->axisMinimum(axis);
      const double maxVal = element->axisMaximum(axis);
      const bool valid = std::isfinite(minVal) && std::isfinite(maxVal)
          && maxVal >= minVal;
      if (style == CartesianPlotRangeStyle::kUserSpecified && valid) {
        printConfiguredAxisInfo(element, axis, minVal, maxVal, valid, style);
      }
    }
  });
}

void CartesianPlotRuntime::createTraceChannel(int index, ChannelKind kind,
    ChannelState &state, ChannelContext &context)
{
  context.kind = kind;
  context.traceIndex = index;
  if (!started_ || state.name.isEmpty()) {
    return;
  }
  subscribeChannel(state, context);
}

void CartesianPlotRuntime::createAuxiliaryChannel(ChannelKind kind,
    ChannelState &state, ChannelContext &context)
{
  context.kind = kind;
  context.traceIndex = -1;
  if (!started_ || state.name.isEmpty()) {
    return;
  }
  subscribeChannel(state, context);
}

void CartesianPlotRuntime::subscribeChannel(ChannelState &state,
    ChannelContext &context)
{
  if (state.name.isEmpty()) {
    return;
  }

  state.subscription.reset();

  auto &mgr = PvChannelManager::instance();
  state.subscription = mgr.subscribe(
      state.name,
      DBR_TIME_DOUBLE,
      0,
      [this, context](const SharedChannelData &data) {
        handleValue(context, data);
      },
      [this, context](bool connected, const SharedChannelData &data) {
        handleConnection(context, connected, data);
      },
      [this, context](bool canRead, bool canWrite) {
        handleAccessRights(context, canRead, canWrite);
      });
}

void CartesianPlotRuntime::unsubscribeChannel(ChannelState &state)
{
  state.subscription.reset();
  state.connected = false;
  state.readAccessKnown = false;
  state.canRead = false;
  state.hasControlInfo = false;
  state.precisionValid = false;
  state.runtimeLimitsValid = false;
  state.runtimeLow = 0.0;
  state.runtimeHigh = 0.0;
  state.fieldType = -1;
  state.elementCount = 0;
}

void CartesianPlotRuntime::handleConnection(const ChannelContext &context,
    bool connected, const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  ChannelState *state = nullptr;
  if (context.kind == ChannelKind::kTraceX && context.traceIndex >= 0
      && context.traceIndex < kCartesianPlotTraceCount) {
    state = &traces_[context.traceIndex].x;
  } else if (context.kind == ChannelKind::kTraceY && context.traceIndex >= 0
      && context.traceIndex < kCartesianPlotTraceCount) {
    state = &traces_[context.traceIndex].y;
  } else if (context.kind == ChannelKind::kTrigger) {
    state = &triggerChannel_;
  } else if (context.kind == ChannelKind::kErase) {
    state = &eraseChannel_;
  } else if (context.kind == ChannelKind::kCount) {
    state = &countChannel_;
  }

  if (!state) {
    return;
  }

  if (connected) {
    state->connected = true;
    state->readAccessKnown = false;
    state->canRead = false;
    state->hasControlInfo = false;
    state->precisionValid = false;
    state->runtimeLimitsValid = false;
    state->runtimeLow = 0.0;
    state->runtimeHigh = 0.0;
    state->fieldType = data.nativeFieldType;
    state->elementCount = std::max<long>(data.nativeElementCount, 1);
    updateChannelControlInfo(*state, data);

    if (context.kind == ChannelKind::kTraceX
        || context.kind == ChannelKind::kTraceY) {
      const bool traceConn = traceConnected(traces_[context.traceIndex]);
      updateTraceMode(context.traceIndex);
      invokeOnElement([index = context.traceIndex, traceConn](CartesianPlotElement *element) {
        element->setTraceRuntimeConnected(index, traceConn);
      });
    }
  } else {
    state->connected = false;
    state->readAccessKnown = false;
    state->canRead = false;
    state->hasControlInfo = false;
    state->precisionValid = false;
    state->runtimeLimitsValid = false;
    state->runtimeLow = 0.0;
    state->runtimeHigh = 0.0;
    state->fieldType = -1;
    state->elementCount = 0;
    if (context.kind == ChannelKind::kTraceX
        || context.kind == ChannelKind::kTraceY) {
      const bool traceConn = traceConnected(traces_[context.traceIndex]);
      invokeOnElement([index = context.traceIndex, traceConn](CartesianPlotElement *element) {
        element->setTraceRuntimeConnected(index, traceConn);
      });
    }
  }

  recomputeAxisRuntimeLimits();
  updateRuntimePaintState();
}

void CartesianPlotRuntime::handleAccessRights(const ChannelContext &context,
    bool canRead, bool canWrite)
{
  Q_UNUSED(canWrite);
  if (!started_) {
    return;
  }

  ChannelState *state = nullptr;
  if (context.kind == ChannelKind::kTraceX && context.traceIndex >= 0
      && context.traceIndex < kCartesianPlotTraceCount) {
    state = &traces_[context.traceIndex].x;
  } else if (context.kind == ChannelKind::kTraceY && context.traceIndex >= 0
      && context.traceIndex < kCartesianPlotTraceCount) {
    state = &traces_[context.traceIndex].y;
  } else if (context.kind == ChannelKind::kTrigger) {
    state = &triggerChannel_;
  } else if (context.kind == ChannelKind::kErase) {
    state = &eraseChannel_;
  } else if (context.kind == ChannelKind::kCount) {
    state = &countChannel_;
  }

  if (!state) {
    return;
  }

  state->readAccessKnown = true;
  state->canRead = canRead;
  updateRuntimePaintState();
}

void CartesianPlotRuntime::updateChannelControlInfo(ChannelState &state,
    const SharedChannelData &data)
{
  if (!data.hasControlInfo) {
    return;
  }

  state.hasControlInfo = true;
  state.precisionValid = !isNumericFieldType(state.fieldType)
      || data.precision >= 0;

  if (areAxisLimitsUsable(data.lopr, data.hopr)) {
    state.runtimeLimitsValid = true;
    state.runtimeLow = data.lopr;
    state.runtimeHigh = data.hopr;
  } else {
    state.runtimeLimitsValid = false;
    state.runtimeLow = 0.0;
    state.runtimeHigh = 0.0;
  }
}

void CartesianPlotRuntime::handleValue(const ChannelContext &context,
    const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  switch (context.kind) {
  case ChannelKind::kTraceX:
    if (context.traceIndex >= 0 && context.traceIndex < kCartesianPlotTraceCount) {
      handleTraceValue(context.traceIndex, true, data);
    }
    break;
  case ChannelKind::kTraceY:
    if (context.traceIndex >= 0 && context.traceIndex < kCartesianPlotTraceCount) {
      handleTraceValue(context.traceIndex, false, data);
    }
    break;
  case ChannelKind::kTrigger:
    handleTriggerValue(data);
    break;
  case ChannelKind::kErase:
    handleEraseValue(data);
    break;
  case ChannelKind::kCount:
    handleCountValue(data);
    break;
  }
}

void CartesianPlotRuntime::handleTraceValue(int index, bool isX,
    const SharedChannelData &data)
{
  if (index < 0 || index >= kCartesianPlotTraceCount) {
    return;
  }
  ChannelState &channel = isX ? traces_[index].x : traces_[index].y;
  if (!channel.readAccessKnown || !channel.canRead) {
    channel.readAccessKnown = true;
    channel.canRead = true;
    updateRuntimePaintState();
  }
  if (data.hasControlInfo) {
    handleTraceControlInfo(index, isX, data);
  }
  TraceState &trace = traces_[index];
  QVector<double> values = extractValues(data);
  if (values.isEmpty()) {
    return;
  }

  if (isX) {
    if (values.size() == 1) {
      trace.lastXScalar = values.front();
      trace.hasXScalar = true;
    } else {
      trace.xVector = values;
    }
  } else {
    trace.lastYTimestampMs = data.hasTimestamp
        ? epicsTimestampToMs(data.timestamp)
        : QDateTime::currentMSecsSinceEpoch();
    trace.hasYTimestamp = true;
    if (values.size() == 1) {
      trace.lastYScalar = values.front();
      trace.hasYScalar = true;
    } else {
      trace.yVector = values;
    }
  }

  if (isTriggerEnabled()) {
    trace.pendingTrigger = true;
    return;
  }

  processTraceUpdate(index, false);
}

void CartesianPlotRuntime::handleTraceControlInfo(int index, bool isX,
    const SharedChannelData &data)
{
  if (!started_ || index < 0 || index >= kCartesianPlotTraceCount) {
    return;
  }
  ChannelState &state = isX ? traces_[index].x : traces_[index].y;
  updateChannelControlInfo(state, data);
  recomputeAxisRuntimeLimits();
  updateRuntimePaintState();
}

void CartesianPlotRuntime::handleTriggerValue(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }
  if (!triggerChannel_.readAccessKnown || !triggerChannel_.canRead) {
    triggerChannel_.readAccessKnown = true;
    triggerChannel_.canRead = true;
  }
  updateChannelControlInfo(triggerChannel_, data);
  updateRuntimePaintState();
  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    processTraceUpdate(i, true);
  }
}

void CartesianPlotRuntime::handleEraseValue(const SharedChannelData &data)
{
  if (!eraseChannel_.readAccessKnown || !eraseChannel_.canRead) {
    eraseChannel_.readAccessKnown = true;
    eraseChannel_.canRead = true;
  }
  updateChannelControlInfo(eraseChannel_, data);
  updateRuntimePaintState();
  QVector<double> values = extractValues(data);
  if (values.isEmpty()) {
    return;
  }
  const double value = values.front();
  bool erase = false;
  switch (eraseMode_) {
  case CartesianPlotEraseMode::kIfZero:
    erase = qFuzzyIsNull(value);
    break;
  case CartesianPlotEraseMode::kIfNotZero:
    erase = !qFuzzyIsNull(value);
    break;
  }
  if (!erase) {
    return;
  }

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    clearTraceData(i, true);
  }
}

void CartesianPlotRuntime::handleCountValue(const SharedChannelData &data)
{
  if (!countChannel_.readAccessKnown || !countChannel_.canRead) {
    countChannel_.readAccessKnown = true;
    countChannel_.canRead = true;
  }
  updateChannelControlInfo(countChannel_, data);
  QVector<double> values = extractValues(data);
  if (values.isEmpty()) {
    updateRuntimePaintState();
    return;
  }
  const double roundedValue = std::lround(values.front());
  const int newCount = roundedValue > static_cast<double>(std::numeric_limits<int>::max())
      ? std::numeric_limits<int>::max()
      : roundedValue < static_cast<double>(std::numeric_limits<int>::min())
          ? std::numeric_limits<int>::min()
          : static_cast<int>(roundedValue);
  if (newCount <= 0) {
    countFromChannel_ = 0;
  } else {
    countFromChannel_ = newCount;
  }

  invokeOnElement([count = countFromChannel_](CartesianPlotElement *element) {
    element->setRuntimeCount(count);
  });

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    TraceState &trace = traces_[i];
    resizeScalarHistory(trace);
    QVector<QPointF> outputPoints;
    switch (trace.mode) {
    case CartesianPlotTraceMode::kXYScalar:
      outputPoints = trace.scalarPoints;
      break;
    case CartesianPlotTraceMode::kXScalar:
      outputPoints = buildXScalarPoints(trace);
      break;
    case CartesianPlotTraceMode::kYScalar:
      outputPoints = buildYScalarPoints(trace);
      break;
    case CartesianPlotTraceMode::kXVector:
    case CartesianPlotTraceMode::kYVector:
    case CartesianPlotTraceMode::kXVectorYScalar:
    case CartesianPlotTraceMode::kYVectorXScalar:
    case CartesianPlotTraceMode::kXYVector:
      rebuildVectorPoints(trace);
      outputPoints = trace.vectorPoints;
      break;
    case CartesianPlotTraceMode::kNone:
      break;
    }
    trace.initialSnapshotPending = outputPoints.isEmpty();
    trace.pendingTrigger = false;
    emitTraceData(i, outputPoints, trace.mode, traceConnected(trace));
  }
  recomputeAxisRuntimeLimits();
  updateRuntimePaintState();
}

void CartesianPlotRuntime::updateTraceMode(int index)
{
  if (index < 0 || index >= kCartesianPlotTraceCount) {
    return;
  }
  TraceState &trace = traces_[index];
  const bool hasX = !trace.x.name.isEmpty();
  const bool hasY = !trace.y.name.isEmpty();
  const bool xVector = hasX && trace.x.elementCount > 1;
  const bool yVector = hasY && trace.y.elementCount > 1;

  CartesianPlotTraceMode mode = CartesianPlotTraceMode::kNone;
  if (hasX && hasY) {
    if (xVector && yVector) {
      mode = CartesianPlotTraceMode::kXYVector;
    } else if (xVector) {
      mode = CartesianPlotTraceMode::kXVectorYScalar;
    } else if (yVector) {
      mode = CartesianPlotTraceMode::kYVectorXScalar;
    } else {
      mode = CartesianPlotTraceMode::kXYScalar;
    }
  } else if (hasX) {
    mode = xVector ? CartesianPlotTraceMode::kXVector
                   : CartesianPlotTraceMode::kXScalar;
  } else if (hasY) {
    mode = yVector ? CartesianPlotTraceMode::kYVector
                   : CartesianPlotTraceMode::kYScalar;
  }

  if (trace.mode == mode) {
    return;
  }
  trace.mode = mode;
  invokeOnElement([index, mode](CartesianPlotElement *element) {
    element->setTraceRuntimeMode(index, mode);
  });

  recomputeAxisRuntimeLimits();
}

void CartesianPlotRuntime::updateRuntimePaintState()
{
  bool hasConfiguredTrace = false;
  bool ready = true;

  for (const TraceState &trace : traces_) {
    const bool needsX = !trace.x.name.isEmpty();
    const bool needsY = !trace.y.name.isEmpty();
    if (!needsX && !needsY) {
      continue;
    }

    hasConfiguredTrace = true;
    if ((needsX && !channelReadyForDisplay(trace.x))
        || (needsY && !channelReadyForDisplay(trace.y))) {
      ready = false;
      break;
    }
  }

  if (ready && !triggerChannel_.name.isEmpty()
      && !channelReadyForDisplay(triggerChannel_)) {
    ready = false;
  }
  if (ready && !eraseChannel_.name.isEmpty()
      && !channelReadyForDisplay(eraseChannel_)) {
    ready = false;
  }
  if (ready && !countChannel_.name.isEmpty()
      && !channelReadyForDisplay(countChannel_)) {
    ready = false;
  }

  invokeOnElement([hasConfiguredTrace, ready](CartesianPlotElement *element) {
    element->setRuntimePaintReady(hasConfiguredTrace, ready);
  });
}

void CartesianPlotRuntime::recomputeAxisRuntimeLimits()
{
  std::array<double, kCartesianAxisCount> minimums{};
  std::array<double, kCartesianAxisCount> maximums{};
  std::array<bool, kCartesianAxisCount> valid{};

  minimums.fill(std::numeric_limits<double>::infinity());
  maximums.fill(-std::numeric_limits<double>::infinity());

  auto accumulateAxis = [&](int axisIndex, double low, double high) {
    if (axisIndex < 0 || axisIndex >= kCartesianAxisCount
        || !areAxisLimitsUsable(low, high)) {
      return;
    }
    if (!valid[axisIndex]) {
      minimums[axisIndex] = low;
      maximums[axisIndex] = high;
      valid[axisIndex] = true;
      return;
    }
    minimums[axisIndex] = std::min(minimums[axisIndex], low);
    maximums[axisIndex] = std::max(maximums[axisIndex], high);
  };

  const int scalarCapacity = effectiveCapacity();

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    const TraceState &trace = traces_[i];
    if (trace.x.connected && trace.x.runtimeLimitsValid) {
      accumulateAxis(0, trace.x.runtimeLow, trace.x.runtimeHigh);
    }
    if (trace.y.connected && trace.y.runtimeLimitsValid) {
      accumulateAxis(trace.yAxisIndex, trace.y.runtimeLow, trace.y.runtimeHigh);
    }

    switch (trace.mode) {
    case CartesianPlotTraceMode::kXVector:
      if (trace.x.connected) {
        accumulateAxis(trace.yAxisIndex, 0.0,
            static_cast<double>(std::max<long>(trace.x.elementCount - 1, 0)));
      }
      break;
    case CartesianPlotTraceMode::kYVector:
      if (trace.y.connected) {
        accumulateAxis(0, 0.0,
            static_cast<double>(std::max<long>(trace.y.elementCount - 1, 0)));
      }
      break;
    case CartesianPlotTraceMode::kXScalar:
      if (trace.x.connected) {
        accumulateAxis(trace.yAxisIndex, 0.0,
            static_cast<double>(std::max(scalarCapacity, 0)));
      }
      break;
    case CartesianPlotTraceMode::kYScalar:
      if (trace.y.connected) {
        accumulateAxis(0, 0.0, static_cast<double>(std::max(scalarCapacity, 0)));
      }
      break;
    case CartesianPlotTraceMode::kNone:
    case CartesianPlotTraceMode::kXYScalar:
    case CartesianPlotTraceMode::kXVectorYScalar:
    case CartesianPlotTraceMode::kYVectorXScalar:
    case CartesianPlotTraceMode::kXYVector:
      break;
    }
  }

  invokeOnElement([minimums, maximums, valid](CartesianPlotElement *element) {
    for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
      element->setAxisRuntimeLimits(axis, minimums[axis], maximums[axis],
          valid[axis]);
      if (valid[axis]) {
        printRuntimeAxisInfo(element, axis, minimums[axis], maximums[axis],
            true);
      }
    }
  });
}

void CartesianPlotRuntime::resizeScalarHistory(TraceState &trace)
{
  switch (trace.mode) {
  case CartesianPlotTraceMode::kXYScalar: {
    const int capacity = effectiveCapacity();
    if (capacity <= 0) {
      trace.scalarPoints.clear();
      break;
    }
    if (trace.scalarPoints.size() > capacity) {
      trace.scalarPoints.resize(capacity);
    }
    break;
  }
  case CartesianPlotTraceMode::kXScalar: {
    const int capacity = effectiveCapacity();
    if (capacity <= 0) {
      trace.xScalarValues.clear();
      break;
    }
    if (trace.xScalarValues.size() > capacity) {
      trace.xScalarValues.resize(capacity);
    }
    break;
  }
  case CartesianPlotTraceMode::kYScalar: {
    const int capacity = effectiveCapacity();
    if (capacity <= 0) {
      trace.yScalarValues.clear();
      break;
    }
    if (trace.yScalarValues.size() > capacity) {
      trace.yScalarValues.resize(capacity);
    }
    break;
  }
  case CartesianPlotTraceMode::kNone:
  case CartesianPlotTraceMode::kXVector:
  case CartesianPlotTraceMode::kYVector:
  case CartesianPlotTraceMode::kXVectorYScalar:
  case CartesianPlotTraceMode::kYVectorXScalar:
  case CartesianPlotTraceMode::kXYVector:
    break;
  }
}

bool CartesianPlotRuntime::channelReadyForDisplay(const ChannelState &state) const
{
  if (!state.connected || !state.readAccessKnown || !state.canRead) {
    return false;
  }
  return state.hasControlInfo && state.precisionValid;
}

void CartesianPlotRuntime::clearTraceData(int index, bool notifyElement)
{
  if (index < 0 || index >= kCartesianPlotTraceCount) {
    return;
  }
  TraceState &trace = traces_[index];
  trace.scalarPoints.clear();
  trace.xScalarValues.clear();
  trace.yScalarValues.clear();
  trace.vectorPoints.clear();
  trace.pendingTrigger = false;
  trace.initialSnapshotPending = true;
  if (notifyElement) {
    invokeOnElement([index](CartesianPlotElement *element) {
      element->updateTraceRuntimeData(index, {});
    });
  }
}

void CartesianPlotRuntime::emitTraceData(int index,
    const QVector<QPointF> &points, CartesianPlotTraceMode mode,
    bool connected)
{
  invokeOnElement([index, mode, connected, points](CartesianPlotElement *element) mutable {
    element->setTraceRuntimeMode(index, mode);
    element->setTraceRuntimeConnected(index, connected);
    element->updateTraceRuntimeData(index, QVector<QPointF>(points));
  });
}

void CartesianPlotRuntime::processTraceUpdate(int index, bool forceAppend)
{
  if (index < 0 || index >= kCartesianPlotTraceCount) {
    return;
  }
  TraceState &trace = traces_[index];
  if (isTriggerEnabled() && !forceAppend && !trace.initialSnapshotPending) {
    return;
  }

  QVector<QPointF> outputPoints;
  bool emitted = false;
  switch (trace.mode) {
  case CartesianPlotTraceMode::kXYScalar:
    if (!trace.hasXScalar || !trace.hasYScalar) {
      break;
    }
    appendXYScalarPoint(trace);
    outputPoints = trace.scalarPoints;
    emitTraceData(index, outputPoints, trace.mode, traceConnected(trace));
    emitted = !outputPoints.isEmpty();
    break;
  case CartesianPlotTraceMode::kXScalar:
    if (!trace.hasXScalar) {
      break;
    }
    appendXScalarPoint(trace);
    outputPoints = buildXScalarPoints(trace);
    emitTraceData(index, outputPoints, trace.mode, traceConnected(trace));
    emitted = !outputPoints.isEmpty();
    break;
  case CartesianPlotTraceMode::kYScalar:
    if (!trace.hasYScalar) {
      break;
    }
    appendYScalarPoint(trace);
    outputPoints = buildYScalarPoints(trace);
    emitTraceData(index, outputPoints, trace.mode, traceConnected(trace));
    emitted = !outputPoints.isEmpty();
    break;
  case CartesianPlotTraceMode::kXVector:
  case CartesianPlotTraceMode::kYVector:
  case CartesianPlotTraceMode::kXVectorYScalar:
  case CartesianPlotTraceMode::kYVectorXScalar:
  case CartesianPlotTraceMode::kXYVector:
    rebuildVectorPoints(trace);
    outputPoints = trace.vectorPoints;
    emitTraceData(index, outputPoints, trace.mode, traceConnected(trace));
    emitted = !outputPoints.isEmpty();
    break;
  case CartesianPlotTraceMode::kNone:
    break;
  }
  if (emitted) {
    trace.initialSnapshotPending = false;
  }
  trace.pendingTrigger = false;
}

void CartesianPlotRuntime::handlePeriodicSampleTimeout()
{
  if (!started_ || isTriggerEnabled()) {
    return;
  }

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    TraceState &trace = traces_[i];
    if (!trace.x.name.isEmpty() || trace.y.name.isEmpty()) {
      continue;
    }
    if (!trace.y.connected || !trace.y.readAccessKnown || !trace.y.canRead) {
      continue;
    }
    if (!trace.hasYTimestamp || nowMs - trace.lastYTimestampMs <= kPeriodicSampleIntervalMs) {
      continue;
    }
    processTraceUpdate(i, false);
  }
}

void CartesianPlotRuntime::appendXYScalarPoint(TraceState &trace)
{
  const int capacity = effectiveCapacity(static_cast<int>(trace.scalarPoints.size()));
  if (capacity <= 0) {
    return;
  }
  if (!eraseOldest_ && trace.scalarPoints.size() >= capacity) {
    return;
  }
  if (eraseOldest_ && trace.scalarPoints.size() >= capacity) {
    trace.scalarPoints.removeFirst();
  }
  trace.scalarPoints.append(QPointF(trace.lastXScalar, trace.lastYScalar));
}

void CartesianPlotRuntime::appendXScalarPoint(TraceState &trace)
{
  const int capacity = effectiveCapacity(static_cast<int>(trace.xScalarValues.size()));
  if (capacity <= 0) {
    return;
  }
  if (!eraseOldest_ && trace.xScalarValues.size() >= capacity) {
    return;
  }
  if (eraseOldest_ && trace.xScalarValues.size() >= capacity) {
    trace.xScalarValues.removeFirst();
  }
  trace.xScalarValues.append(trace.lastXScalar);
}

void CartesianPlotRuntime::appendYScalarPoint(TraceState &trace)
{
  const int capacity = effectiveCapacity(static_cast<int>(trace.yScalarValues.size()));
  if (capacity <= 0) {
    return;
  }
  if (!eraseOldest_ && trace.yScalarValues.size() >= capacity) {
    return;
  }
  if (eraseOldest_ && trace.yScalarValues.size() >= capacity) {
    trace.yScalarValues.removeFirst();
  }
  trace.yScalarValues.append(trace.lastYScalar);
}

void CartesianPlotRuntime::rebuildVectorPoints(TraceState &trace)
{
  trace.vectorPoints.clear();
  switch (trace.mode) {
  case CartesianPlotTraceMode::kXVector: {
    const int capacity = effectiveCapacity(
        static_cast<int>(trace.xVector.size()), false);
    const int limit = std::min(capacity,
        static_cast<int>(trace.xVector.size()));
    trace.vectorPoints.reserve(limit);
    for (int i = 0; i < limit; ++i) {
      trace.vectorPoints.append(QPointF(trace.xVector.at(i), i));
    }
    break;
  }
  case CartesianPlotTraceMode::kYVector: {
    const int capacity = effectiveCapacity(
        static_cast<int>(trace.yVector.size()), false);
    const int limit = std::min(capacity,
        static_cast<int>(trace.yVector.size()));
    trace.vectorPoints.reserve(limit);
    for (int i = 0; i < limit; ++i) {
      trace.vectorPoints.append(QPointF(i, trace.yVector.at(i)));
    }
    break;
  }
  case CartesianPlotTraceMode::kXVectorYScalar: {
    if (!trace.hasYScalar || trace.xVector.isEmpty()) {
      break;
    }
    const int capacity = effectiveCapacity(
        static_cast<int>(trace.xVector.size()), false);
    const int limit = std::min(capacity,
        static_cast<int>(trace.xVector.size()));
    trace.vectorPoints.reserve(limit);
    for (int i = 0; i < limit; ++i) {
      trace.vectorPoints.append(QPointF(trace.xVector.at(i), trace.lastYScalar));
    }
    break;
  }
  case CartesianPlotTraceMode::kYVectorXScalar: {
    if (!trace.hasXScalar || trace.yVector.isEmpty()) {
      break;
    }
    const int capacity = effectiveCapacity(
        static_cast<int>(trace.yVector.size()), false);
    const int limit = std::min(capacity,
        static_cast<int>(trace.yVector.size()));
    trace.vectorPoints.reserve(limit);
    for (int i = 0; i < limit; ++i) {
      trace.vectorPoints.append(QPointF(trace.lastXScalar, trace.yVector.at(i)));
    }
    break;
  }
  case CartesianPlotTraceMode::kXYVector: {
    if (trace.xVector.isEmpty() || trace.yVector.isEmpty()) {
      break;
    }
    const int native = std::min(trace.xVector.size(), trace.yVector.size());
    const int capacity = effectiveCapacity(native, false);
    const int limit = std::min(capacity, native);
    trace.vectorPoints.reserve(limit);
    for (int i = 0; i < limit; ++i) {
      trace.vectorPoints.append(QPointF(trace.xVector.at(i), trace.yVector.at(i)));
    }
    break;
  }
  default:
    break;
  }
}

QVector<QPointF> CartesianPlotRuntime::buildXScalarPoints(
    const TraceState &trace) const
{
  QVector<QPointF> points;
  points.reserve(trace.xScalarValues.size());
  for (int i = 0; i < trace.xScalarValues.size(); ++i) {
    points.append(QPointF(trace.xScalarValues.at(i), i));
  }
  return points;
}

QVector<QPointF> CartesianPlotRuntime::buildYScalarPoints(
    const TraceState &trace) const
{
  QVector<QPointF> points;
  points.reserve(trace.yScalarValues.size());
  for (int i = 0; i < trace.yScalarValues.size(); ++i) {
    points.append(QPointF(i, trace.yScalarValues.at(i)));
  }
  return points;
}

int CartesianPlotRuntime::effectiveCapacity(int preferredCount,
    bool allowConfiguredCount) const
{
  int capacity = countFromChannel_ > 0 ? countFromChannel_ : 0;
  if (capacity <= 0 && allowConfiguredCount && configuredCount_ > 0) {
    capacity = configuredCount_;
  }
  if (capacity <= 0) {
    if (allowConfiguredCount) {
      return 0;
    }
    capacity = preferredCount;
  }
  return std::max(capacity, 0);
}

bool CartesianPlotRuntime::traceConnected(const TraceState &trace) const
{
  bool needsX = !trace.x.name.isEmpty();
  bool needsY = !trace.y.name.isEmpty();
  if (needsX && needsY) {
    return trace.x.connected && trace.y.connected;
  }
  if (needsX) {
    return trace.x.connected;
  }
  if (needsY) {
    return trace.y.connected;
  }
  return false;
}

bool CartesianPlotRuntime::isTriggerEnabled() const
{
  return !triggerChannel_.name.isEmpty();
}

bool CartesianPlotRuntime::areAxisLimitsUsable(double low, double high)
{
  if (!std::isfinite(low) || !std::isfinite(high) || high < low) {
    return false;
  }
  const double floatMax = static_cast<double>(std::numeric_limits<float>::max());
  if (std::fabs(low) >= floatMax || std::fabs(high) >= floatMax) {
    return false;
  }
  return true;
}

QVector<double> CartesianPlotRuntime::extractValues(
    const SharedChannelData &data)
{
  QVector<double> values;
  if (data.isArray && !data.arrayValues.isEmpty()) {
    values = data.arrayValues;
    return values;
  }
  if (data.isNumeric) {
    values.append(data.numericValue);
  }
  return values;
}
