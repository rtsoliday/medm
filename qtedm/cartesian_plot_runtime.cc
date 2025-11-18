#include "cartesian_plot_runtime.h"

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QDebug>
#include <QtGlobal>
#include <QtMath>

#include <db_access.h>

#include "cartesian_plot_element.h"
#include "channel_access_context.h"
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

  ChannelAccessContext &context = ChannelAccessContext::instance();
  context.ensureInitialized();
  if (!context.isInitialized()) {
    qWarning() << "Channel Access context not available for Cartesian Plot";
    return;
  }

  eraseOldest_ = element_->eraseOldest();
  eraseMode_ = element_->eraseMode();
  configuredCount_ = element_->count();
  countFromChannel_ = 0;

  started_ = true;

  invokeOnElement([](CartesianPlotElement *element) {
    element->clearRuntimeState();
  });

  resetState();

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

  ca_flush_io();
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

  ca_flush_io();

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
    trace.x.channelId = nullptr;
    trace.x.subscriptionId = nullptr;
    trace.x.fieldType = -1;
    trace.x.elementCount = 0;
    trace.y.channelId = nullptr;
    trace.y.subscriptionId = nullptr;
    trace.y.fieldType = -1;
    trace.y.elementCount = 0;
  }
  triggerChannel_ = ChannelState{};
  eraseChannel_ = ChannelState{};
  countChannel_ = ChannelState{};
}

void CartesianPlotRuntime::createTraceChannel(int index, ChannelKind kind,
    ChannelState &state, ChannelContext &context)
{
  if (!started_ || state.name.isEmpty()) {
    return;
  }

  QByteArray bytes = state.name.toLatin1();
  int status = ca_create_channel(bytes.constData(),
      &CartesianPlotRuntime::channelConnectionCallback, &context,
      CA_PRIORITY_DEFAULT, &state.channelId);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << state.name << ':' << ca_message(status);
    state.channelId = nullptr;
    return;
  }
  ca_set_puser(state.channelId, &context);
}

void CartesianPlotRuntime::createAuxiliaryChannel(ChannelKind kind,
    ChannelState &state, ChannelContext &context)
{
  context.kind = kind;
  context.traceIndex = -1;
  if (!started_ || state.name.isEmpty()) {
    return;
  }

  QByteArray bytes = state.name.toLatin1();
  int status = ca_create_channel(bytes.constData(),
      &CartesianPlotRuntime::channelConnectionCallback, &context,
      CA_PRIORITY_DEFAULT, &state.channelId);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << state.name << ':' << ca_message(status);
    state.channelId = nullptr;
    return;
  }
  ca_set_puser(state.channelId, &context);
}

void CartesianPlotRuntime::subscribeChannel(ChannelState &state,
    ChannelContext &context)
{
  if (!state.channelId || state.subscriptionId) {
    return;
  }
  const long count = state.elementCount > 0 ? state.elementCount : 1;
  int status = ca_create_subscription(DBR_TIME_DOUBLE, count,
      state.channelId, DBE_VALUE | DBE_ALARM,
      &CartesianPlotRuntime::valueEventCallback, &context,
      &state.subscriptionId);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << state.name << ':'
               << ca_message(status);
    state.subscriptionId = nullptr;
  }
}

void CartesianPlotRuntime::unsubscribeChannel(ChannelState &state)
{
  if (state.subscriptionId) {
    ca_clear_subscription(state.subscriptionId);
    state.subscriptionId = nullptr;
  }
  if (state.channelId) {
    ca_clear_channel(state.channelId);
    state.channelId = nullptr;
  }
  state.connected = false;
  state.fieldType = -1;
  state.elementCount = 0;
}

void CartesianPlotRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  auto *context = static_cast<ChannelContext *>(ca_puser(args.chid));
  if (!context || !context->runtime) {
    return;
  }
  context->runtime->handleConnection(*context, args);
}

void CartesianPlotRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *context = static_cast<ChannelContext *>(args.usr);
  if (!context || !context->runtime) {
    return;
  }
  context->runtime->handleValue(*context, args);
}

void CartesianPlotRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *context = static_cast<ChannelContext *>(args.usr);
  if (!context || !context->runtime) {
    return;
  }
  context->runtime->handleControlInfo(*context, args);
}

void CartesianPlotRuntime::handleConnection(const ChannelContext &context,
    const connection_handler_args &args)
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

  if (!state || (!state->channelId) || state->channelId != args.chid) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    state->connected = true;
    state->fieldType = ca_field_type(args.chid);
    state->elementCount = ca_element_count(args.chid);

    if ((context.kind == ChannelKind::kTraceX
            || context.kind == ChannelKind::kTraceY)
        && !RuntimeUtils::isNumericFieldType(state->fieldType)) {
      qWarning() << "Cartesian Plot channel" << state->name
                 << "is not numeric";
      return;
    }

    ChannelContext *subscriptionContext = nullptr;
    if (context.kind == ChannelKind::kTraceX && context.traceIndex >= 0
        && context.traceIndex < kCartesianPlotTraceCount) {
      subscriptionContext = &xContexts_[context.traceIndex];
    } else if (context.kind == ChannelKind::kTraceY && context.traceIndex >= 0
        && context.traceIndex < kCartesianPlotTraceCount) {
      subscriptionContext = &yContexts_[context.traceIndex];
    } else {
      subscriptionContext = const_cast<ChannelContext *>(&context);
    }
    subscribeChannel(*state, *subscriptionContext);

    if (RuntimeUtils::isNumericFieldType(state->fieldType)) {
      ChannelContext *controlContext = subscriptionContext;
      int status = ca_array_get_callback(DBR_CTRL_DOUBLE, 1, state->channelId,
          &CartesianPlotRuntime::controlInfoCallback, controlContext);
      if (status != ECA_NORMAL) {
        qWarning() << "Failed to request control info for" << state->name
                   << ':' << ca_message(status);
      }
    }

    if (context.kind == ChannelKind::kTraceX
        || context.kind == ChannelKind::kTraceY) {
      updateTraceMode(context.traceIndex);
      const bool connected = traceConnected(traces_[context.traceIndex]);
      invokeOnElement([index = context.traceIndex, connected](CartesianPlotElement *element) {
        element->setTraceRuntimeConnected(index, connected);
      });
    }
  } else if (args.op == CA_OP_CONN_DOWN) {
    state->connected = false;
    state->fieldType = -1;
    state->elementCount = 0;
    if (context.kind == ChannelKind::kTraceX
        || context.kind == ChannelKind::kTraceY) {
      const bool connected = traceConnected(traces_[context.traceIndex]);
      invokeOnElement([index = context.traceIndex, connected](CartesianPlotElement *element) {
        element->setTraceRuntimeConnected(index, connected);
      });
    }
  }
}

void CartesianPlotRuntime::handleValue(const ChannelContext &context,
    const event_handler_args &args)
{
  if (!started_) {
    return;
  }

  switch (context.kind) {
  case ChannelKind::kTraceX:
    if (context.traceIndex >= 0 && context.traceIndex < kCartesianPlotTraceCount) {
      handleTraceValue(context.traceIndex, true, args);
    }
    break;
  case ChannelKind::kTraceY:
    if (context.traceIndex >= 0 && context.traceIndex < kCartesianPlotTraceCount) {
      handleTraceValue(context.traceIndex, false, args);
    }
    break;
  case ChannelKind::kTrigger:
    handleTriggerValue(args);
    break;
  case ChannelKind::kErase:
    handleEraseValue(args);
    break;
  case ChannelKind::kCount:
    handleCountValue(args);
    break;
  }
}

void CartesianPlotRuntime::handleControlInfo(const ChannelContext &context,
    const event_handler_args &args)
{
  if (!started_ || args.type != DBR_CTRL_DOUBLE || !args.dbr) {
    return;
  }

  const auto *info = static_cast<const dbr_ctrl_double *>(args.dbr);
  const double low = info->lower_disp_limit;
  const double high = info->upper_disp_limit;
  bool valid = std::isfinite(low) && std::isfinite(high) && high >= low;

  if (context.kind == ChannelKind::kTraceX && context.traceIndex >= 0
      && context.traceIndex < kCartesianPlotTraceCount) {
    invokeOnElement([low, high, valid](CartesianPlotElement *element) {
      element->setAxisRuntimeLimits(0, low, high, valid);
    });
  } else if (context.kind == ChannelKind::kTraceY && context.traceIndex >= 0
      && context.traceIndex < kCartesianPlotTraceCount) {
    const int axisIndex = traces_[context.traceIndex].yAxisIndex;
    invokeOnElement([axisIndex, low, high, valid](CartesianPlotElement *element) {
      element->setAxisRuntimeLimits(axisIndex, low, high, valid);
    });
  }
}

void CartesianPlotRuntime::handleTraceValue(int index, bool isX,
    const event_handler_args &args)
{
  if (index < 0 || index >= kCartesianPlotTraceCount) {
    return;
  }
  TraceState &trace = traces_[index];
  QVector<double> values = extractValues(args);
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

void CartesianPlotRuntime::handleTriggerValue(const event_handler_args &args)
{
  Q_UNUSED(args);
  if (!started_) {
    return;
  }
  for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
    processTraceUpdate(i, true);
  }
}

void CartesianPlotRuntime::handleEraseValue(const event_handler_args &args)
{
  QVector<double> values = extractValues(args);
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

void CartesianPlotRuntime::handleCountValue(const event_handler_args &args)
{
  QVector<double> values = extractValues(args);
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
  capacity = std::clamp(capacity, 1, kCartesianPlotMaximumSampleCount);
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
    const event_handler_args &args)
{
  QVector<double> values;
  if (args.type != DBR_TIME_DOUBLE || !args.dbr) {
    return values;
  }
  const auto *timeValue = static_cast<const dbr_time_double *>(args.dbr);
  const int count = static_cast<int>(args.count > 0 ? args.count : 1);
  values.resize(count);
  const double *data = reinterpret_cast<const double *>(&timeValue->value);
  for (int i = 0; i < count; ++i) {
    values[i] = data[i];
  }
  return values;
}
