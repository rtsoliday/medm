#include "strip_chart_runtime.h"

#include <QByteArray>
#include <QDebug>

#include <db_access.h>
#include <epicsTime.h>

#include "strip_chart_element.h"
#include "channel_access_context.h"
#include "runtime_utils.h"

namespace {
constexpr qint64 kUnixEpicsEpochOffsetSeconds = 631152000LL;

using RuntimeUtils::isNumericFieldType;

qint64 epicsTimestampToMs(const epicsTimeStamp &stamp)
{
  const qint64 seconds = static_cast<qint64>(stamp.secPastEpoch)
      + kUnixEpicsEpochOffsetSeconds;
  const qint64 millis = seconds * 1000LL + stamp.nsec / 1000000LL;
  return millis;
}

} // namespace

StripChartRuntime::StripChartRuntime(StripChartElement *element)
  : QObject(element)
  , element_(element)
{
  for (int i = 0; i < kStripChartPenCount; ++i) {
    contexts_[i].runtime = this;
    contexts_[i].index = i;
  }
}

StripChartRuntime::~StripChartRuntime()
{
  stop();
}

void StripChartRuntime::start()
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

  started_ = true;
  invokeOnElement([](StripChartElement *element) {
    element->clearRuntimeState();
  });

  for (int i = 0; i < kStripChartPenCount; ++i) {
    PenState &pen = pens_[i];
    pen.channelName = element_->channel(i).trimmed();
    pen.channelId = nullptr;
    pen.subscriptionId = nullptr;
    pen.connected = false;
    pen.fieldType = -1;

    if (pen.channelName.isEmpty()) {
      continue;
    }

    QByteArray channelBytes = pen.channelName.toLatin1();
    int status = ca_create_channel(channelBytes.constData(),
        &StripChartRuntime::channelConnectionCallback,
        &contexts_[i], CA_PRIORITY_DEFAULT, &pen.channelId);
    if (status != ECA_NORMAL) {
      qWarning() << "Failed to create Channel Access channel for"
                 << pen.channelName << ':' << ca_message(status);
      pen.channelId = nullptr;
      continue;
    }
    ca_set_puser(pen.channelId, &contexts_[i]);
  }

  ca_flush_io();
}

void StripChartRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  for (int i = 0; i < kStripChartPenCount; ++i) {
    unsubscribePen(i);
    resetPen(i);
  }
  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }

  invokeOnElement([](StripChartElement *element) {
    element->clearRuntimeState();
  });
}

void StripChartRuntime::subscribePen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  if (pen.subscriptionId || !pen.channelId) {
    return;
  }
  int status = ca_create_subscription(DBR_TIME_DOUBLE, 1, pen.channelId,
      DBE_VALUE | DBE_ALARM, &StripChartRuntime::valueEventCallback,
      &contexts_[index], &pen.subscriptionId);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << pen.channelName << ':'
               << ca_message(status);
    pen.subscriptionId = nullptr;
    return;
  }
  ca_flush_io();
}

void StripChartRuntime::requestControlInfo(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  if (!pen.channelId || !isNumericFieldType(pen.fieldType)) {
    return;
  }
  int status = ca_array_get_callback(DBR_CTRL_DOUBLE, 1, pen.channelId,
      &StripChartRuntime::controlInfoCallback, &contexts_[index]);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  } else {
    qWarning() << "Failed to request control info for"
               << pen.channelName << ':' << ca_message(status);
  }
}

void StripChartRuntime::handleConnectionEvent(int index,
    const connection_handler_args &args)
{
  if (!started_ || index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  if (pen.channelId != args.chid) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    pen.connected = true;
    pen.fieldType = ca_field_type(pen.channelId);
    const long elementCount = ca_element_count(pen.channelId);
    if (!isNumericFieldType(pen.fieldType) || elementCount < 1) {
      qWarning() << "Strip chart channel" << pen.channelName
                 << "is not a numeric scalar";
      invokeOnElement([index](StripChartElement *element) {
        element->setRuntimeConnected(index, false);
      });
      return;
    }

    invokeOnElement([index](StripChartElement *element) {
      element->setRuntimeConnected(index, true);
    });
    subscribePen(index);
    requestControlInfo(index);
  } else if (args.op == CA_OP_CONN_DOWN) {
    pen.connected = false;
    invokeOnElement([index](StripChartElement *element) {
      element->setRuntimeConnected(index, false);
      element->clearPenRuntimeState(index);
    });
  }
}

void StripChartRuntime::handleValueEvent(int index,
    const event_handler_args &args)
{
  if (!started_ || index < 0 || index >= kStripChartPenCount) {
    return;
  }
  if (args.type != DBR_TIME_DOUBLE || !args.dbr) {
    return;
  }

  const auto *value = static_cast<const dbr_time_double *>(args.dbr);
  const double numericValue = value->value;
  const qint64 timestampMs = epicsTimestampToMs(value->stamp);

  invokeOnElement([index, numericValue, timestampMs](StripChartElement *element) {
    element->addRuntimeSample(index, numericValue, timestampMs);
  });
}

void StripChartRuntime::handleControlInfo(int index,
    const event_handler_args &args)
{
  if (!started_ || index < 0 || index >= kStripChartPenCount) {
    return;
  }
  if (args.type != DBR_CTRL_DOUBLE || !args.dbr) {
    return;
  }

  const auto *info = static_cast<const dbr_ctrl_double *>(args.dbr);
  const double low = info->lower_disp_limit;
  const double high = info->upper_disp_limit;

  invokeOnElement([index, low, high](StripChartElement *element) {
    element->setRuntimeLimits(index, low, high);
  });
}

void StripChartRuntime::resetPen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  pen.channelId = nullptr;
  pen.subscriptionId = nullptr;
  pen.connected = false;
  pen.fieldType = -1;
}

void StripChartRuntime::unsubscribePen(int index)
{
  if (index < 0 || index >= kStripChartPenCount) {
    return;
  }
  PenState &pen = pens_[index];
  if (pen.subscriptionId) {
    ca_clear_subscription(pen.subscriptionId);
    pen.subscriptionId = nullptr;
  }
  if (pen.channelId) {
    ca_clear_channel(pen.channelId);
    pen.channelId = nullptr;
  }
}

void StripChartRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  auto *context = static_cast<PenContext *>(ca_puser(args.chid));
  if (!context || !context->runtime) {
    return;
  }
  context->runtime->handleConnectionEvent(context->index, args);
}

void StripChartRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *context = static_cast<PenContext *>(args.usr);
  if (!context || !context->runtime) {
    return;
  }
  context->runtime->handleValueEvent(context->index, args);
}

void StripChartRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *context = static_cast<PenContext *>(args.usr);
  if (!context || !context->runtime) {
    return;
  }
  context->runtime->handleControlInfo(context->index, args);
}
