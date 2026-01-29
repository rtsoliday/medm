#include "cartesian_plot_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <QByteArray>
#include <QDebug>
#include <QtGlobal>
#include <QtMath>

#include <db_access.h>

#include "cartesian_plot_element.h"
#include "channel_access_context.h"
#include "pv_channel_manager.h"
#include "runtime_utils.h"

namespace {

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

}

void CartesianPlotRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;

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
    trace.pendingTrigger = false;
    trace.lastXScalar = 0.0;
    trace.lastYScalar = 0.0;
    trace.x.connected = false;
    trace.y.connected = false;
    trace.x.subscription.reset();
    trace.x.fieldType = -1;
    trace.x.elementCount = 0;
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
      });
}

void CartesianPlotRuntime::unsubscribeChannel(ChannelState &state)
{
  state.subscription.reset();
  state.connected = false;
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
    state->fieldType = data.nativeFieldType;
    state->elementCount = data.nativeElementCount;

    if (context.kind == ChannelKind::kTrigger
        || context.kind == ChannelKind::kErase
        || context.kind == ChannelKind::kCount) {
      return;
    }

    if (context.kind == ChannelKind::kTraceX
        || context.kind == ChannelKind::kTraceY) {
      const bool traceConn = traceConnected(traces_[context.traceIndex]);
      updateTraceMode(context.traceIndex);
      invokeOnElement([index = context.traceIndex, traceConn](CartesianPlotElement *element) {
        element->setTraceRuntimeConnected(index, traceConn);
      });
      handleTraceControlInfo(context.traceIndex,
          context.kind == ChannelKind::kTraceX, data);
    }
  } else {
    state->connected = false;
    if (context.kind == ChannelKind::kTraceX
        || context.kind == ChannelKind::kTraceY) {
      const bool traceConn = traceConnected(traces_[context.traceIndex]);
      invokeOnElement([index = context.traceIndex, traceConn](CartesianPlotElement *element) {
        element->setTraceRuntimeConnected(index, traceConn);
      });
    }
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
  if (!data.hasControlInfo) {
    return;
  }

  const double low = data.lopr;
  const double high = data.hopr;
  bool valid = std::isfinite(low) && std::isfinite(high) && high >= low;

  if (isX) {
    invokeOnElement([low, high, valid](CartesianPlotElement *element) {
      element->setAxisRuntimeLimits(0, low, high, valid);
      printRuntimeAxisInfo(element, 0, low, high, valid);
    });
  } else {
    const int axisIndex = traces_[index].yAxisIndex;
    invokeOnElement([axisIndex, low, high, valid](CartesianPlotElement *element) {
      element->setAxisRuntimeLimits(axisIndex, low, high, valid);
      printRuntimeAxisInfo(element, axisIndex, low, high, valid);
    });
  }
}

void CartesianPlotRuntime::handleTriggerValue(const SharedChannelData &data)
{
  Q_UNUSED(data);
  if (!started_) {
    return;
  }
  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    processTraceUpdate(i, true);
  }
}

void CartesianPlotRuntime::handleEraseValue(const SharedChannelData &data)
{
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
  QVector<double> values = extractValues(data);
  if (values.isEmpty()) {
    return;
  }
  int newCount = static_cast<int>(std::lround(values.front()));
  if (newCount <= 0) {
    countFromChannel_ = 0;
  } else {
  countFromChannel_ = std::clamp(newCount, 1, kCartesianPlotMaximumSampleCount);
  }

  invokeOnElement([count = countFromChannel_](CartesianPlotElement *element) {
    element->setRuntimeCount(count);
  });

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    clearTraceData(i, false);
    processTraceUpdate(i, true);
  }
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

  /* Update X-axis range for Y-only traces to match MEDM behavior */
  updateXAxisRangeForYOnlyTraces();
}

void CartesianPlotRuntime::updateXAxisRangeForYOnlyTraces()
{
  /* Match MEDM behavior: when Y-only vectors are used (no X channel),
   * the X-axis range should be 0 to (elementCount - 1) based on the
   * Y vector element count. This is the "from channel" range style
   * when there's no X channel to provide LOPR/HOPR. */
  double maxX = 0.0;
  bool hasYOnlyVector = false;

  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    const TraceState &trace = traces_[i];
    const bool hasX = !trace.x.name.isEmpty();
    const bool hasY = !trace.y.name.isEmpty();
    const bool yVector = hasY && trace.y.elementCount > 1;

    if (!hasX && yVector) {
      /* Y-only vector trace - X range is 0 to (elementCount - 1) */
      hasYOnlyVector = true;
      const double traceMaxX = static_cast<double>(trace.y.elementCount - 1);
      if (traceMaxX > maxX) {
        maxX = traceMaxX;
      }
    }
  }

  if (hasYOnlyVector) {
    invokeOnElement([maxX](CartesianPlotElement *element) {
      element->setAxisRuntimeLimits(0, 0.0, maxX, true);
    });
  }
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
  trace.xVector.clear();
  trace.yVector.clear();
  trace.hasXScalar = false;
  trace.hasYScalar = false;
  trace.pendingTrigger = false;
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
  if (isTriggerEnabled() && !forceAppend) {
    return;
  }

  switch (trace.mode) {
  case CartesianPlotTraceMode::kXYScalar:
    if (!trace.hasXScalar || !trace.hasYScalar) {
      break;
    }
    appendXYScalarPoint(trace);
    emitTraceData(index, trace.scalarPoints, trace.mode,
        traceConnected(trace));
    break;
  case CartesianPlotTraceMode::kXScalar:
    if (!trace.hasXScalar) {
      break;
    }
    appendXScalarPoint(trace);
    emitTraceData(index, buildXScalarPoints(trace), trace.mode,
        traceConnected(trace));
    break;
  case CartesianPlotTraceMode::kYScalar:
    if (!trace.hasYScalar) {
      break;
    }
    appendYScalarPoint(trace);
    emitTraceData(index, buildYScalarPoints(trace), trace.mode,
        traceConnected(trace));
    break;
  case CartesianPlotTraceMode::kXVector:
  case CartesianPlotTraceMode::kYVector:
  case CartesianPlotTraceMode::kXVectorYScalar:
  case CartesianPlotTraceMode::kYVectorXScalar:
  case CartesianPlotTraceMode::kXYVector:
    rebuildVectorPoints(trace);
    emitTraceData(index, trace.vectorPoints, trace.mode,
        traceConnected(trace));
    break;
  case CartesianPlotTraceMode::kNone:
    break;
  }
  trace.pendingTrigger = false;
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
    capacity = preferredCount > 0 ? preferredCount : kCartesianPlotMaximumSampleCount;
  }
  /* For vector data (allowConfiguredCount=false), don't apply the maximum
   * sample count limit - use the full vector size from the channel.
   * This matches MEDM behavior where vector element count is used directly.
   * The limit only applies to scalar accumulation modes. */
  if (allowConfiguredCount) {
    capacity = std::clamp(capacity, 1, kCartesianPlotMaximumSampleCount);
  } else {
    capacity = std::max(capacity, 1);
  }
  return capacity;
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
