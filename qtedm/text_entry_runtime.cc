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
#include "pv_channel_manager.h"
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

  const QString initialChannel = element_->channel().trimmed();
  const bool needsCa = parsePvName(initialChannel).protocol == PvProtocol::kCa;
  if (needsCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available";
      return;
    }
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

  requestedType_ = DBR_TIME_DOUBLE;
  requestedCount_ = 0;

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      requestedType_,
      requestedCount_,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &data) {
        handleChannelConnection(connected, data);
      },
      [this](bool canRead, bool canWrite) { handleAccessRights(canRead, canWrite); });
}

void TextEntryRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
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
  initialUpdateTracked_ = false;

  invokeOnElement([](TextEntryElement *element) {
    element->clearRuntimeState();
  });
}

void TextEntryRuntime::resubscribe(int requestedType, long elementCount)
{
  if (!started_) {
    return;
  }

  requestedType_ = requestedType;
  requestedCount_ = elementCount;
  subscription_.reset();

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      requestedType_,
      requestedCount_,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &data) {
        handleChannelConnection(connected, data);
      },
      [this](bool canRead, bool canWrite) { handleAccessRights(canRead, canWrite); });
}

void TextEntryRuntime::handleChannelConnection(bool connected,
    const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  auto &stats = StatisticsTracker::instance();

  if (connected) {
    const bool wasConnected = connected_;
    connected_ = true;
    if (!wasConnected) {
      stats.registerChannelConnected();
    }

    fieldType_ = data.nativeFieldType;
    elementCount_ = std::max<long>(data.nativeElementCount, 1);

    int desiredType = DBR_TIME_DOUBLE;
    long desiredCount = 1;
    switch (fieldType_) {
    case DBR_STRING:
      valueKind_ = ValueKind::kString;
      desiredType = DBR_TIME_STRING;
      desiredCount = 1;
      break;
    case DBR_ENUM:
      valueKind_ = ValueKind::kEnum;
      desiredType = DBR_TIME_ENUM;
      desiredCount = 1;
      break;
    case DBR_CHAR:
      if (elementCount_ > 1) {
        valueKind_ = ValueKind::kCharArray;
      } else {
        valueKind_ = ValueKind::kNumeric;
      }
      desiredType = DBR_TIME_CHAR;
      desiredCount = elementCount_;
      break;
    default:
      valueKind_ = ValueKind::kNumeric;
      desiredType = DBR_TIME_DOUBLE;
      desiredCount = 1;
      break;
    }

    if (valueKind_ == ValueKind::kNumeric && !isNumericFieldType(fieldType_)) {
      valueKind_ = ValueKind::kString;
    }

    if (desiredType != requestedType_ || desiredCount != requestedCount_) {
      resubscribe(desiredType, desiredCount);
      return;
    }

    enumStrings_ = data.enumStrings;
    if (data.hasPrecision) {
      channelPrecision_ = data.precision;
    }
    if (data.hasControlInfo) {
      controlLow_ = data.lopr;
      controlHigh_ = data.hopr;
      hasControlLimits_ = std::isfinite(controlLow_)
          && std::isfinite(controlHigh_);
      const double low = controlLow_;
      const double high = controlHigh_;
      const int precision = data.precision;
      invokeOnElement([low, high, precision](TextEntryElement *element) {
        element->setRuntimeLimits(low, high);
        element->setRuntimePrecision(precision);
      });
    }

    invokeOnElement([](TextEntryElement *element) {
      element->setRuntimeConnected(true);
    });
  } else {
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

void TextEntryRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  if (data.isString) {
    lastStringValue_ = data.stringValue;
    hasStringValue_ = true;
    hasNumericValue_ = false;
    lastSeverity_ = data.severity;
  } else if (data.isEnum) {
    lastEnumValue_ = static_cast<short>(data.enumValue);
    lastNumericValue_ = data.numericValue;
    hasNumericValue_ = true;
    hasStringValue_ = false;
    lastSeverity_ = data.severity;
  } else if (data.isCharArray) {
    lastStringValue_ = formatCharArray(data.charArrayValue);
    hasStringValue_ = !lastStringValue_.isEmpty();
    if (data.isNumeric) {
      lastNumericValue_ = data.numericValue;
      hasNumericValue_ = true;
    }
    lastSeverity_ = data.severity;
  } else if (data.isNumeric) {
    lastNumericValue_ = data.numericValue;
    hasNumericValue_ = true;
    hasStringValue_ = false;
    lastSeverity_ = data.severity;
  } else {
    return;
  }

  if (!data.enumStrings.isEmpty() && enumStrings_ != data.enumStrings) {
    enumStrings_ = data.enumStrings;
  }

  if (data.hasControlInfo) {
    channelPrecision_ = data.precision;
    controlLow_ = data.lopr;
    controlHigh_ = data.hopr;
    hasControlLimits_ = std::isfinite(controlLow_)
        && std::isfinite(controlHigh_);
    const double low = controlLow_;
    const double high = controlHigh_;
    const int precision = data.precision;
    invokeOnElement([low, high, precision](TextEntryElement *element) {
      element->setRuntimeLimits(low, high);
      element->setRuntimePrecision(precision);
    });
  }

  auto &stats = StatisticsTracker::instance();
  stats.registerCaEvent();
  stats.registerUpdateRequest(true);
  stats.registerUpdateExecuted();

  if (!initialUpdateTracked_) {
    auto &tracker = StartupUiSettlingTracker::instance();
    if (tracker.enabled()) {
      tracker.recordInitialUpdateQueued();
    }
  }

  updateElementDisplay();
}

void TextEntryRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }
  if (canWrite == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = canWrite;
  invokeOnElement([canWrite](TextEntryElement *element) {
    element->setRuntimeWriteAccess(canWrite);
  });
}

void TextEntryRuntime::handleActivation(const QString &text)
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
    return;
  }

  QString trimmed = text.trimmed();

  switch (valueKind_) {
  case ValueKind::kString: {
    QByteArray bytes = trimmed.toLatin1();
    if (bytes.size() >= MAX_STRING_SIZE) {
      bytes.truncate(MAX_STRING_SIZE - 1);
    }
    if (!PvChannelManager::instance().putValue(channelName_, trimmed)) {
      qWarning() << "Failed to write Text Entry value" << trimmed << "to"
                 << channelName_;
      return;
    }
    AuditLogger::instance().logPut(channelName_, trimmed,
        QStringLiteral("TextEntry"));
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
      if (!PvChannelManager::instance().putCharArrayValue(channelName_, bytes)) {
        qWarning() << "Failed to write Text Entry value" << trimmed << "to"
                   << channelName_;
        return;
      }
      AuditLogger::instance().logPut(channelName_, trimmed,
          QStringLiteral("TextEntry"));
    } else {
      double numeric = 0.0;
      if (!parseNumericInput(trimmed, numeric)) {
        qWarning() << "Text Entry numeric parse failed for" << channelName_
                   << "value" << trimmed;
        return;
      }
      if (!PvChannelManager::instance().putValue(channelName_, numeric)) {
        qWarning() << "Failed to write Text Entry value" << trimmed << "to"
                   << channelName_;
        return;
      }
      AuditLogger::instance().logPut(channelName_, numeric,
          QStringLiteral("TextEntry"));
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
    if (!PvChannelManager::instance().putValue(channelName_,
        static_cast<dbr_enum_t>(enumValue))) {
      qWarning() << "Failed to write Text Entry value" << trimmed << "to"
                 << channelName_;
      return;
    }
    AuditLogger::instance().logPut(channelName_, static_cast<int>(enumValue),
        QStringLiteral("TextEntry"));
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
    if (!PvChannelManager::instance().putValue(channelName_, numeric)) {
      qWarning() << "Failed to write Text Entry value" << trimmed << "to"
                 << channelName_;
      return;
    }
    AuditLogger::instance().logPut(channelName_, numeric,
        QStringLiteral("TextEntry"));
    break;
  }
  }
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
  const bool needsInitialMark = !initialUpdateTracked_;
  invokeOnElement([displayText, severity, needsInitialMark, this](TextEntryElement *element) {
    element->setRuntimeConnected(true);
    element->setRuntimeSeverity(severity);
    element->setRuntimeText(displayText);
    if (needsInitialMark && !initialUpdateTracked_) {
      auto &tracker = StartupUiSettlingTracker::instance();
      if (tracker.enabled()) {
        tracker.recordInitialUpdateApplied();
      }
      initialUpdateTracked_ = true;
    }
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
