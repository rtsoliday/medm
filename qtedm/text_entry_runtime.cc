#include "text_entry_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QByteArray>
#include <QDebug>

#include <cvtFast.h>
#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "runtime_utils.h"
#include "statistics_tracker.h"
#include "text_entry_element.h"
#include "text_format_utils.h"

namespace {
using RuntimeUtils::kInvalidSeverity;
using RuntimeUtils::isNumericFieldType;
using TextFormatUtils::formatHex;
using TextFormatUtils::formatOctal;
using TextFormatUtils::kMaxTextField;
using TextFormatUtils::kPi;
using TextFormatUtils::localCvtDoubleToExpNotationString;
using TextFormatUtils::makeSexagesimal;

} // namespace

TextEntryRuntime::TextEntryRuntime(TextEntryElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

TextEntryRuntime::~TextEntryRuntime()
{
  stop();
}

void TextEntryRuntime::start()
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

  resetRuntimeState();
  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();

  channelName_ = element_->channel().trimmed();
  element_->setActivationCallback([this](const QString &text) {
    handleActivation(text);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  QByteArray channelBytes = channelName_.toLatin1();
  int status = ca_create_channel(channelBytes.constData(),
      &TextEntryRuntime::channelConnectionCallback, this,
      CA_PRIORITY_DEFAULT, &channelId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to create Channel Access channel for"
               << channelName_ << ':' << ca_message(status);
    channelId_ = nullptr;
    return;
  }

  StatisticsTracker::instance().registerChannelCreated();

  ca_set_puser(channelId_, this);
  ca_replace_access_rights_event(channelId_,
      &TextEntryRuntime::accessRightsCallback);

  ca_flush_io();
}

void TextEntryRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  unsubscribe();
  if (element_) {
    element_->setActivationCallback(std::function<void(const QString &)>());
  }
  resetRuntimeState();
}

void TextEntryRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  valueKind_ = ValueKind::kNone;
  lastNumericValue_ = 0.0;
  hasNumericValue_ = false;
  lastStringValue_.clear();
  hasStringValue_ = false;
  lastEnumValue_ = 0;
  lastSeverity_ = 0;
  enumStrings_.clear();
  channelPrecision_ = -1;
  controlLow_ = 0.0;
  controlHigh_ = 0.0;
  hasControlLimits_ = false;
  lastWriteAccess_ = false;

  invokeOnElement([](TextEntryElement *element) {
    element->clearRuntimeState();
  });
}

void TextEntryRuntime::subscribe()
{
  if (subscriptionId_ || !channelId_) {
    return;
  }

  chtype subscriptionType = DBR_TIME_DOUBLE;
  elementCount_ = std::max<long>(elementCount_, 1);

  switch (fieldType_) {
  case DBR_STRING:
    valueKind_ = ValueKind::kString;
    subscriptionType = DBR_TIME_STRING;
    elementCount_ = 1;
    break;
  case DBR_ENUM:
    valueKind_ = ValueKind::kEnum;
    subscriptionType = DBR_TIME_ENUM;
    elementCount_ = 1;
    break;
  case DBR_CHAR:
    if (elementCount_ > 1) {
      valueKind_ = ValueKind::kCharArray;
    } else {
      valueKind_ = ValueKind::kNumeric;
    }
    subscriptionType = DBR_TIME_CHAR;
    break;
  default:
    valueKind_ = ValueKind::kNumeric;
    subscriptionType = DBR_TIME_DOUBLE;
    break;
  }

  int status = ca_create_subscription(subscriptionType, elementCount_,
      channelId_, DBE_VALUE | DBE_ALARM,
      &TextEntryRuntime::valueEventCallback, this, &subscriptionId_);
  if (status != ECA_NORMAL) {
    qWarning() << "Failed to subscribe to" << channelName_ << ":"
               << ca_message(status);
    subscriptionId_ = nullptr;
    return;
  }
  ca_flush_io();
}

void TextEntryRuntime::unsubscribe()
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

void TextEntryRuntime::requestControlInfo()
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
    break;
  }

  if (!controlType) {
    return;
  }

  int status = ca_array_get_callback(controlType, 1, channelId_,
      &TextEntryRuntime::controlInfoCallback, this);
  if (status == ECA_NORMAL) {
    ca_flush_io();
  }
}

void TextEntryRuntime::handleConnectionEvent(
    const connection_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }

  auto &stats = StatisticsTracker::instance();

  if (args.op == CA_OP_CONN_UP) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }
    fieldType_ = ca_field_type(channelId_);
    elementCount_ = std::max<long>(ca_element_count(channelId_), 1);
    if (valueKind_ == ValueKind::kNumeric && !isNumericFieldType(fieldType_)) {
      valueKind_ = ValueKind::kString;
    }
    updateWriteAccess();
    subscribe();
    requestControlInfo();
    invokeOnElement([](TextEntryElement *element) {
      element->setRuntimeConnected(true);
    });
  } else if (args.op == CA_OP_CONN_DOWN) {
    const bool wasConnected = connected_;
    connected_ = false;
    if (wasConnected) {
      stats.registerChannelDisconnected();
    }
    lastWriteAccess_ = false;
    hasNumericValue_ = false;
    hasStringValue_ = false;
    invokeOnElement([](TextEntryElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
      element->setRuntimeText(QString());
    });
  }
}

void TextEntryRuntime::handleValueEvent(const event_handler_args &args)
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
    hasStringValue_ = false;
    lastSeverity_ = data->severity;
    break;
  }
  case DBR_TIME_DOUBLE: {
    const auto *data = static_cast<const dbr_time_double *>(args.dbr);
    lastNumericValue_ = data->value;
    hasNumericValue_ = true;
    hasStringValue_ = false;
    lastSeverity_ = data->severity;
    break;
  }
  case DBR_TIME_CHAR: {
    const auto *data = static_cast<const dbr_time_char *>(args.dbr);
    const auto *raw = reinterpret_cast<const char *>(&data->value);
    QByteArray bytes(raw, static_cast<int>(args.count));
    lastStringValue_ = formatCharArray(bytes);
    hasStringValue_ = !lastStringValue_.isEmpty();
    lastNumericValue_ = static_cast<double>(data->value);
    hasNumericValue_ = true;
    lastSeverity_ = data->severity;
    break;
  }
  default:
    return;
  }

  auto &stats = StatisticsTracker::instance();
  stats.registerCaEvent();
  stats.registerUpdateRequest(true);
  stats.registerUpdateExecuted();

  updateElementDisplay();
}

void TextEntryRuntime::handleControlInfo(const event_handler_args &args)
{
  if (!started_ || args.usr != this || !args.dbr) {
    return;
  }
  if (args.status != ECA_NORMAL) {
    return;
  }

  switch (args.type) {
  case DBR_CTRL_ENUM: {
    const auto *ctrl = static_cast<const dbr_ctrl_enum *>(args.dbr);
    enumStrings_.clear();
    const int count = ctrl->no_str;
    for (int i = 0; i < count; ++i) {
      const char *state = ctrl->strs[i];
      if (state && std::strlen(state) > 0) {
        enumStrings_.append(QString::fromLatin1(state));
      } else {
        enumStrings_.append(QString());
      }
    }
    break;
  }
  case DBR_CTRL_DOUBLE: {
    const auto *ctrl = static_cast<const dbr_ctrl_double *>(args.dbr);
    channelPrecision_ = ctrl->precision;
    controlLow_ = ctrl->lower_disp_limit;
    controlHigh_ = ctrl->upper_disp_limit;
    hasControlLimits_ = std::isfinite(controlLow_)
        && std::isfinite(controlHigh_);
    double low = ctrl->lower_disp_limit;
    double high = ctrl->upper_disp_limit;
    int precision = ctrl->precision;
    invokeOnElement([low, high, precision](TextEntryElement *element) {
      element->setRuntimeLimits(low, high);
      element->setRuntimePrecision(precision);
    });
    updateElementDisplay();
    break;
  }
  default:
    break;
  }
}

void TextEntryRuntime::handleAccessRightsEvent(
    const access_rights_handler_args &args)
{
  if (!started_ || args.chid != channelId_) {
    return;
  }
  updateWriteAccess();
}

void TextEntryRuntime::handleActivation(const QString &text)
{
  if (!started_ || !channelId_ || !connected_ || !lastWriteAccess_) {
    return;
  }

  QString trimmed = text.trimmed();
  int status = ECA_NORMAL;

  switch (valueKind_) {
  case ValueKind::kString: {
    QByteArray bytes = trimmed.toLatin1();
    if (bytes.size() >= MAX_STRING_SIZE) {
      bytes.truncate(MAX_STRING_SIZE - 1);
    }
    bytes.append('\0');
    status = ca_put(DBR_STRING, channelId_, bytes.constData());
    if (status == ECA_NORMAL) {
      AuditLogger::instance().logPut(channelName_, trimmed,
          QStringLiteral("TextEntry"));
    }
    break;
  }
  case ValueKind::kCharArray: {
    if (element_ && element_->format() == TextMonitorFormat::kString) {
      QByteArray bytes;
      if (!parseCharArrayInput(trimmed, bytes)) {
        qWarning() << "Text Entry char array parse failed for" << channelName_
                   << "value" << trimmed;
        return;
      }
      status = ca_array_put(DBR_CHAR, bytes.size(), channelId_,
          bytes.constData());
      if (status == ECA_NORMAL) {
        AuditLogger::instance().logPut(channelName_, trimmed,
            QStringLiteral("TextEntry"));
      }
    } else {
      double numeric = 0.0;
      if (!parseNumericInput(trimmed, numeric)) {
        qWarning() << "Text Entry numeric parse failed for" << channelName_
                   << "value" << trimmed;
        return;
      }
      dbr_double_t value = static_cast<dbr_double_t>(numeric);
      status = ca_put(DBR_DOUBLE, channelId_, &value);
      if (status == ECA_NORMAL) {
        AuditLogger::instance().logPut(channelName_, numeric,
            QStringLiteral("TextEntry"));
      }
    }
    break;
  }
  case ValueKind::kEnum: {
    short enumValue = 0;
    if (!parseEnumInput(trimmed, enumValue)) {
      qWarning() << "Text Entry enum parse failed for" << channelName_
                 << "value" << trimmed;
      return;
    }
    status = ca_put(DBR_SHORT, channelId_, &enumValue);
    if (status == ECA_NORMAL) {
      AuditLogger::instance().logPut(channelName_, static_cast<int>(enumValue),
          QStringLiteral("TextEntry"));
    }
    break;
  }
  case ValueKind::kNumeric:
  case ValueKind::kNone:
  default: {
    double numeric = 0.0;
    if (!parseNumericInput(trimmed, numeric)) {
      qWarning() << "Text Entry numeric parse failed for" << channelName_
                 << "value" << trimmed;
      return;
    }
    dbr_double_t value = static_cast<dbr_double_t>(numeric);
    status = ca_put(DBR_DOUBLE, channelId_, &value);
    if (status == ECA_NORMAL) {
      AuditLogger::instance().logPut(channelName_, numeric,
          QStringLiteral("TextEntry"));
    }
    break;
  }
  }

  if (status != ECA_NORMAL) {
    qWarning() << "Failed to write Text Entry value" << trimmed << "to"
               << channelName_ << ':' << ca_message(status);
    return;
  }
  ca_flush_io();
}

void TextEntryRuntime::updateWriteAccess()
{
  if (!channelId_) {
    return;
  }
  const bool writeAccess = ca_write_access(channelId_) != 0;
  if (writeAccess == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = writeAccess;
  invokeOnElement([writeAccess](TextEntryElement *element) {
    element->setRuntimeWriteAccess(writeAccess);
  });
}

void TextEntryRuntime::updateElementDisplay()
{
  if (!element_) {
    return;
  }
  if (!connected_) {
    invokeOnElement([](TextEntryElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
      element->setRuntimeText(QString());
    });
    return;
  }

  QString displayText;
  switch (valueKind_) {
  case ValueKind::kString:
    displayText = lastStringValue_;
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
  case ValueKind::kCharArray:
    if (element_->format() == TextMonitorFormat::kString) {
      /* For char arrays in STRING format, always show the string value
       * even if empty (all null bytes). Don't fall back to numeric. */
      displayText = lastStringValue_;
    } else if (hasNumericValue_) {
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

  const short severity = lastSeverity_;
  invokeOnElement([displayText, severity](TextEntryElement *element) {
    element->setRuntimeConnected(true);
    element->setRuntimeSeverity(severity);
    element->setRuntimeText(displayText);
  });
}

int TextEntryRuntime::resolvedPrecision() const
{
  if (!element_) {
    return 0;
  }
  if (element_->precisionSource() == PvLimitSource::kChannel
      && channelPrecision_ >= 0) {
    return std::clamp(channelPrecision_, 0, 17);
  }
  return std::clamp(element_->precisionDefault(), 0, 17);
}

QString TextEntryRuntime::formatNumeric(double value, int precision) const
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
    return formatHex(static_cast<long>(std::llround(value)));
  case TextMonitorFormat::kOctal:
    return formatOctal(static_cast<long>(std::llround(value)));
  case TextMonitorFormat::kSexagesimal:
    return makeSexagesimal(value, epicsPrecision);
  case TextMonitorFormat::kSexagesimalHms:
    return makeSexagesimal(value * 12.0 / kPi, epicsPrecision);
  case TextMonitorFormat::kSexagesimalDms:
    return makeSexagesimal(value * 180.0 / kPi, epicsPrecision);
  }

  return QString::fromLatin1(buffer);
}

QString TextEntryRuntime::formatCharArray(const QByteArray &bytes) const
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

bool TextEntryRuntime::parseNumericInput(const QString &text,
    double &value) const
{
  QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  bool ok = false;
  auto format = element_ ? element_->format() : TextMonitorFormat::kDecimal;

  switch (format) {
  case TextMonitorFormat::kHexadecimal:
    value = static_cast<double>(trimmed.toLongLong(&ok, 16));
    return ok;
  case TextMonitorFormat::kOctal:
    value = static_cast<double>(trimmed.toLongLong(&ok, 8));
    return ok;
  case TextMonitorFormat::kSexagesimal:
    if (!parseSexagesimal(trimmed, value)) {
      return false;
    }
    return true;
  case TextMonitorFormat::kSexagesimalHms:
    if (!parseSexagesimal(trimmed, value)) {
      return false;
    }
    value *= kPi / 12.0;
    return true;
  case TextMonitorFormat::kSexagesimalDms:
    if (!parseSexagesimal(trimmed, value)) {
      return false;
    }
    value *= kPi / 180.0;
    return true;
  case TextMonitorFormat::kTruncated:
  case TextMonitorFormat::kCompact:
  case TextMonitorFormat::kEngineering:
  case TextMonitorFormat::kExponential:
  case TextMonitorFormat::kDecimal:
  case TextMonitorFormat::kString:
  default:
    break;
  }

  value = trimmed.toDouble(&ok);
  if (ok) {
    return true;
  }

  if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
    value = static_cast<double>(trimmed.mid(2).toLongLong(&ok, 16));
    if (ok) {
      return true;
    }
  }

  value = trimmed.toDouble(&ok);
  return ok;
}

bool TextEntryRuntime::parseSexagesimal(const QString &text,
    double &value) const
{
  QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  bool negative = false;
  if (trimmed.startsWith('-')) {
    negative = true;
    trimmed = trimmed.mid(1);
  } else if (trimmed.startsWith('+')) {
    trimmed = trimmed.mid(1);
  }
  trimmed = trimmed.trimmed();

  const QStringList parts = trimmed.split(':');
  if (parts.isEmpty()) {
    bool ok = false;
    double numeric = trimmed.toDouble(&ok);
    if (!ok) {
      return false;
    }
    value = negative ? -numeric : numeric;
    return true;
  }

  double total = 0.0;
  double divisor = 1.0;
  for (int i = 0; i < parts.size(); ++i) {
    const QString part = parts.at(i).trimmed();
    if (part.isEmpty()) {
      return false;
    }
    bool ok = false;
    double numeric = part.toDouble(&ok);
    if (!ok) {
      return false;
    }
    if (i == 0) {
      total = numeric;
    } else {
      divisor *= 60.0;
      total += numeric / divisor;
    }
  }

  value = negative ? -total : total;
  return true;
}

bool TextEntryRuntime::parseEnumInput(const QString &text, short &value) const
{
  QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  for (int i = 0; i < enumStrings_.size(); ++i) {
    if (trimmed == enumStrings_.at(i)) {
      value = static_cast<short>(i);
      return true;
    }
  }

  bool ok = false;
  auto format = element_ ? element_->format() : TextMonitorFormat::kDecimal;
  switch (format) {
  case TextMonitorFormat::kHexadecimal:
    value = static_cast<short>(trimmed.toLongLong(&ok, 16));
    break;
  case TextMonitorFormat::kOctal:
    value = static_cast<short>(trimmed.toLongLong(&ok, 8));
    break;
  default:
    value = static_cast<short>(trimmed.toLongLong(&ok, 10));
    if (!ok && trimmed.startsWith(QStringLiteral("0x"),
            Qt::CaseInsensitive)) {
      value = static_cast<short>(trimmed.mid(2).toLongLong(&ok, 16));
    }
    break;
  }
  return ok;
}

bool TextEntryRuntime::parseCharArrayInput(const QString &text,
    QByteArray &bytes) const
{
  QByteArray latin = text.toLatin1();
  const int count = static_cast<int>(elementCount_);
  if (count <= 0) {
    return false;
  }
  bytes.resize(count);
  std::memset(bytes.data(), 0, static_cast<size_t>(count));
  const int copyLen = std::min(count, latin.size());
  std::memcpy(bytes.data(), latin.constData(), static_cast<size_t>(copyLen));
  if (copyLen < count) {
    bytes[copyLen] = '\0';
  }
  return true;
}

void TextEntryRuntime::channelConnectionCallback(
    struct connection_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<TextEntryRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleConnectionEvent(args);
  }
}

void TextEntryRuntime::valueEventCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<TextEntryRuntime *>(args.usr);
  if (self) {
    self->handleValueEvent(args);
  }
}

void TextEntryRuntime::controlInfoCallback(struct event_handler_args args)
{
  if (!args.usr) {
    return;
  }
  auto *self = static_cast<TextEntryRuntime *>(args.usr);
  if (self) {
    self->handleControlInfo(args);
  }
}

void TextEntryRuntime::accessRightsCallback(
    struct access_rights_handler_args args)
{
  if (!args.chid) {
    return;
  }
  auto *self = static_cast<TextEntryRuntime *>(ca_puser(args.chid));
  if (self) {
    self->handleAccessRightsEvent(args);
  }
}
