#include "text_monitor_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <QByteArray>
#include <QDebug>

#include "channel_access_context.h"
#include "text_monitor_element.h"
#include "text_format_utils.h"

#include <cvtFast.h>
#include <db_access.h>

namespace {
using TextFormatUtils::kMaxTextField;
constexpr short kInvalidAlarmSeverity = 3;
using TextFormatUtils::kMaxPrecision;
using TextFormatUtils::kPi;
using TextFormatUtils::clampPrecision;
using TextFormatUtils::localCvtDoubleToExpNotationString;
using TextFormatUtils::makeSexagesimal;
using TextFormatUtils::formatHex;
using TextFormatUtils::formatOctal;

} // namespace

TextMonitorRuntime::TextMonitorRuntime(TextMonitorElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel(0).trimmed();
  }
}

TextMonitorRuntime::~TextMonitorRuntime()
{
  stop();
}

void TextMonitorRuntime::start()
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
  resetRuntimeState();

  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &TextMonitorRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ":" << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  ca_set_puser(channelId_, this);

  ca_flush_io();
}

void TextMonitorRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  unsubscribe();
  resetRuntimeState();
}

void TextMonitorRuntime::resetRuntimeState()
{
  connected_ = false;
  valueKind_ = ValueKind::kNone;
  hasNumericValue_ = false;
  hasStringValue_ = false;
  lastEnumValue_ = 0;
  lastSeverity_ = 0;
  channelPrecision_ = -1;
  enumStrings_.clear();
  if (element_) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(0);
    element_->setRuntimeText(QString());
  }
}

void TextMonitorRuntime::unsubscribe()
{
  if (subscriptionId_) {
    ca_clear_subscription(subscriptionId_);
    subscriptionId_ = nullptr;
  }
  if (channelId_) {
    ca_clear_channel(channelId_);
    channelId_ = nullptr;
  }
  if (ChannelAccessContext::instance().isInitialized()) {
    ca_flush_io();
  }
}

void TextMonitorRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  switch (fieldType_) {
  case DBR_STRING:
    valueKind_ = ValueKind::kString;
    subscriptionType_ = DBR_TIME_STRING;
    elementCount_ = 1;
    break;
  case DBR_ENUM:
    valueKind_ = ValueKind::kEnum;
    subscriptionType_ = DBR_TIME_ENUM;
    elementCount_ = 1;
    break;
  case DBR_CHAR:
    /* Always treat CHAR as CharArray, matching MEDM behavior.
     * Format setting at display time determines string vs numeric display. */
    valueKind_ = ValueKind::kCharArray;
    subscriptionType_ = DBR_TIME_CHAR;
    break;
  default:
    valueKind_ = ValueKind::kNumeric;
    subscriptionType_ = DBR_TIME_DOUBLE;
    elementCount_ = std::max<long>(elementCount_, 1);
    break;
  }

  int status = ca_create_subscription(subscriptionType_, elementCount_,
      channelId_, DBE_VALUE | DBE_ALARM,
      &TextMonitorRuntime::valueEventCallback, this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ":"
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }
  ca_flush_io();
}

void TextMonitorRuntime::requestControlInfo()
{
  if (!channelId_) {
    return;
  }

  chtype controlType = 0;
  switch (fieldType_) {
  case DBR_ENUM:
    controlType = DBR_CTRL_ENUM;
    break;
  case DBR_CHAR:
  case DBR_SHORT:
  case DBR_LONG:
  case DBR_FLOAT:
  case DBR_DOUBLE:
    controlType = DBR_CTRL_DOUBLE;
    break;
  default:
    return;
  }

  int status = ca_array_get_callback(controlType, 1, channelId_,
      &TextMonitorRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  }
}

void TextMonitorRuntime::handleConnectionEvent(
    const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  if (args.op == CA_OP_CONN_UP) {
    connected_ = true;
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    subscribe();
    requestControlInfo();
    if (element_) {
      element_->setRuntimeConnected(true);
    }
  } else if (args.op == CA_OP_CONN_DOWN) {
    connected_ = false;
    hasNumericValue_ = false;
    hasStringValue_ = false;
    if (element_) {
      element_->setRuntimeConnected(false);
      element_->setRuntimeSeverity(kInvalidAlarmSeverity);
      element_->setRuntimeText(QString());
    }
  }
}

void TextMonitorRuntime::handleValueEvent(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.status != ECA_NORMAL) {
    return;
  }

  switch (args.type) {
  case DBR_TIME_STRING: {
    const auto *data = static_cast<const dbr_time_string *>(args.dbr);
    lastStringValue_ = QString::fromLatin1(data->value);
    hasStringValue_ = true;
    hasNumericValue_ = false;
    lastSeverity_ = data->severity;
    break;
  }
  case DBR_TIME_ENUM: {
    const auto *data = static_cast<const dbr_time_enum *>(args.dbr);
    lastEnumValue_ = data->value;
    lastNumericValue_ = static_cast<double>(data->value);
    hasNumericValue_ = true;
    lastSeverity_ = data->severity;
    break;
  }
  case DBR_TIME_DOUBLE: {
    const auto *data = static_cast<const dbr_time_double *>(args.dbr);
    lastNumericValue_ = data->value;
    hasNumericValue_ = true;
    lastSeverity_ = data->severity;
    break;
  }
  case DBR_TIME_CHAR: {
    const auto *data = static_cast<const dbr_time_char *>(args.dbr);
    const auto *raw = reinterpret_cast<const char *>(&data->value);
    QByteArray bytes(raw, static_cast<int>(args.count));
    lastStringValue_ = formatCharArray(bytes);
    /* Always mark as having string value for CHAR arrays, even if empty.
     * This matches MEDM behavior: format=STRING shows empty string for all zeros. */
    hasStringValue_ = true;
    lastNumericValue_ = static_cast<double>(data->value);
    hasNumericValue_ = true;
    lastSeverity_ = data->severity;
    break;
  }
  default:
    return;
  }

  updateElementDisplay();
}

void TextMonitorRuntime::handleControlInfo(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.status != ECA_NORMAL) {
    return;
  }

  switch (args.type) {
  case DBR_CTRL_DOUBLE: {
    const auto *ctrl = static_cast<const dbr_ctrl_double *>(args.dbr);
    channelPrecision_ = ctrl->precision;
    break;
  }
  case DBR_CTRL_ENUM: {
    const auto *ctrl = static_cast<const dbr_ctrl_enum *>(args.dbr);
    enumStrings_.clear();
    for (int i = 0; i < ctrl->no_str; ++i) {
      enumStrings_.append(QString::fromLatin1(ctrl->strs[i]));
    }
    break;
  }
  default:
    break;
  }

  updateElementDisplay();
}

void TextMonitorRuntime::updateElementDisplay()
{
  if (!element_) {
    return;
  }

  if (!connected_) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(kInvalidAlarmSeverity);
    element_->setRuntimeText(QString());
    return;
  }

  QString displayText;
  switch (valueKind_) {
  case ValueKind::kString:
    displayText = lastStringValue_;
    break;
  case ValueKind::kCharArray:
    if (element_->format() == TextMonitorFormat::kString && hasStringValue_) {
      displayText = lastStringValue_;
    } else if (hasNumericValue_) {
      displayText = formatNumeric(lastNumericValue_, resolvedPrecision());
    }
    break;
  case ValueKind::kEnum:
    if (!enumStrings_.isEmpty()) {
      int index = static_cast<int>(lastEnumValue_);
      if (index >= 0 && index < enumStrings_.size()) {
        displayText = enumStrings_.at(index);
      }
    }
    if (displayText.isEmpty() && hasNumericValue_) {
      displayText = formatNumeric(lastNumericValue_, resolvedPrecision());
    }
    break;
  case ValueKind::kNumeric:
  case ValueKind::kNone:
  default:
    if (hasNumericValue_) {
      displayText = formatNumeric(lastNumericValue_, resolvedPrecision());
    }
    break;
  }

  element_->setRuntimeConnected(true);
  element_->setRuntimeSeverity(lastSeverity_);
  element_->setRuntimeText(displayText);
}

int TextMonitorRuntime::resolvedPrecision() const
{
  if (!element_) {
    return 0;
  }
  if (element_->precisionSource() == PvLimitSource::kChannel
      && channelPrecision_ >= 0) {
    return clampPrecision(channelPrecision_);
  }
  return clampPrecision(element_->precisionDefault());
}

QString TextMonitorRuntime::formatNumeric(double value, int precision) const
{
  if (!element_) {
    return QString();
  }

  unsigned short epicsPrecision = static_cast<unsigned short>(precision);
  char buffer[kMaxTextField];
  buffer[0] = '\0';

  switch (element_->format()) {
  case TextMonitorFormat::kDecimal:
  case TextMonitorFormat::kString:
    cvtDoubleToString(value, buffer, epicsPrecision);
    break;
  case TextMonitorFormat::kExponential:
    std::snprintf(buffer, sizeof(buffer), "%.*e", epicsPrecision, value);
    break;
  case TextMonitorFormat::kEngineering:
    localCvtDoubleToExpNotationString(value, buffer, epicsPrecision);
    break;
  case TextMonitorFormat::kCompact:
    cvtDoubleToCompactString(value, buffer, epicsPrecision);
    break;
  case TextMonitorFormat::kTruncated:
    cvtLongToString(static_cast<long>(value), buffer);
    break;
  case TextMonitorFormat::kHexadecimal:
    return formatHex(static_cast<long>(value));
  case TextMonitorFormat::kOctal:
    return formatOctal(static_cast<long>(value));
  case TextMonitorFormat::kSexagesimal: {
    return makeSexagesimal(value, epicsPrecision);
  }
  case TextMonitorFormat::kSexagesimalHms: {
    return makeSexagesimal(value * 12.0 / kPi, epicsPrecision);
  }
  case TextMonitorFormat::kSexagesimalDms: {
    return makeSexagesimal(value * 180.0 / kPi, epicsPrecision);
  }
  }

  return QString::fromLatin1(buffer);
}

QString TextMonitorRuntime::formatEnumValue(short value) const
{
  if (!enumStrings_.isEmpty()) {
    int index = static_cast<int>(value);
    if (index >= 0 && index < enumStrings_.size()) {
      return enumStrings_.at(index);
    }
  }
  return QString::number(static_cast<int>(value));
}

QString TextMonitorRuntime::formatCharArray(const QByteArray &bytes) const
{
  if (bytes.isEmpty()) {
    return QString();
  }
  int nullIndex = bytes.indexOf('\0');
  if (nullIndex >= 0) {
    return QString::fromLatin1(bytes.constData(), nullIndex);
  }
  return QString::fromLatin1(bytes);
}

void TextMonitorRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (auto *runtime = static_cast<TextMonitorRuntime *>(
          ca_puser(args.chid))) {
    runtime->handleConnectionEvent(args);
  }
}

void TextMonitorRuntime::valueEventCallback(struct event_handler_args args)
{
  if (auto *runtime = static_cast<TextMonitorRuntime *>(args.usr)) {
    runtime->handleValueEvent(args);
  }
}

void TextMonitorRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (auto *runtime = static_cast<TextMonitorRuntime *>(args.usr)) {
    runtime->handleControlInfo(args);
  }
}
