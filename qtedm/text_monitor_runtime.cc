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
#include "shared_channel_manager.h"

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

  ChannelAccessContext::instance().ensureInitialized();
  if (!ChannelAccessContext::instance().isInitialized()) {
    qWarning() << "Channel Access context not available";
    return;
  }

  started_ = true;
  resetRuntimeState();

  if (channelName_.isEmpty()) {
    return;
  }

  /* TextMonitor needs specific DBR types for proper display:
   * - DBR_TIME_STRING for string PVs
   * - DBR_TIME_ENUM for enum PVs (to get string representation)
   * - DBR_TIME_CHAR for char arrays (waveform strings)
   * - DBR_TIME_DOUBLE for numeric PVs
   *
   * We start with DBR_TIME_DOUBLE and will resubscribe if needed
   * once we know the native field type. Different DBR types create
   * different shared channels per the user's requirements. */
  auto &mgr = SharedChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      DBR_TIME_DOUBLE,  /* Initial type - may be refined */
      0,                /* Native element count */
      [this](const SharedChannelData &data) {
        handleChannelData(data);
      },
      [this](bool conn) {
        handleChannelConnection(conn);
      });
}

void TextMonitorRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  subscription_.reset();
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
  nativeFieldType_ = -1;
  elementCount_ = 1;
  enumStrings_.clear();
  if (element_) {
    element_->setRuntimeConnected(false);
    element_->setRuntimeSeverity(0);
    element_->setRuntimeText(QString());
  }
}

chtype TextMonitorRuntime::determineSubscriptionType(short nativeFieldType) const
{
  switch (nativeFieldType) {
  case DBR_STRING:
    return DBR_TIME_STRING;
  case DBR_ENUM:
    return DBR_TIME_ENUM;
  case DBR_CHAR:
    return DBR_TIME_CHAR;
  default:
    return DBR_TIME_DOUBLE;
  }
}

void TextMonitorRuntime::handleChannelConnection(bool connected)
{
  if (!started_) {
    return;
  }

  connected_ = connected;

  if (connected) {
    if (element_) {
      element_->setRuntimeConnected(true);
    }
  } else {
    hasNumericValue_ = false;
    hasStringValue_ = false;
    nativeFieldType_ = -1;
    if (element_) {
      element_->setRuntimeConnected(false);
      element_->setRuntimeSeverity(kInvalidAlarmSeverity);
      element_->setRuntimeText(QString());
    }
  }
}

void TextMonitorRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  /* Check if we need to resubscribe with a different DBR type.
   * This happens when native field type is STRING, ENUM, or CHAR
   * and we initially subscribed with DBR_TIME_DOUBLE. */
  if (nativeFieldType_ < 0 && data.nativeFieldType >= 0) {
    nativeFieldType_ = data.nativeFieldType;
    elementCount_ = data.nativeElementCount;

    chtype neededType = determineSubscriptionType(nativeFieldType_);
    if (neededType != DBR_TIME_DOUBLE) {
      /* Resubscribe with the appropriate type */
      auto &mgr = SharedChannelManager::instance();
      subscription_.reset();
      subscription_ = mgr.subscribe(
          channelName_,
          neededType,
          (neededType == DBR_TIME_CHAR) ? elementCount_ : 1,
          [this](const SharedChannelData &newData) {
            handleChannelData(newData);
          },
          [this](bool conn) {
            handleChannelConnection(conn);
          });
      return;  /* Wait for new subscription data */
    }
  }

  /* Determine value kind based on native field type */
  switch (nativeFieldType_) {
  case DBR_STRING:
    valueKind_ = ValueKind::kString;
    break;
  case DBR_ENUM:
    valueKind_ = ValueKind::kEnum;
    break;
  case DBR_CHAR:
    valueKind_ = ValueKind::kCharArray;
    break;
  default:
    valueKind_ = ValueKind::kNumeric;
    break;
  }

  /* Extract values from shared data */
  lastSeverity_ = data.severity;

  if (data.isString) {
    lastStringValue_ = data.stringValue;
    hasStringValue_ = true;
  }

  if (data.isCharArray) {
    lastStringValue_ = formatCharArray(data.charArrayValue);
    hasStringValue_ = true;
    lastNumericValue_ = data.numericValue;
    hasNumericValue_ = true;
  }

  if (data.isEnum) {
    lastEnumValue_ = data.enumValue;
    lastNumericValue_ = data.numericValue;
    hasNumericValue_ = true;
  }

  if (data.isNumeric && !data.isEnum && !data.isCharArray) {
    lastNumericValue_ = data.numericValue;
    hasNumericValue_ = true;
  }

  /* Copy control info */
  if (data.hasControlInfo) {
    channelPrecision_ = data.precision;
    enumStrings_ = data.enumStrings;
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

