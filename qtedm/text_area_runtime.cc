#include "text_area_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <QDebug>

#include <cvtFast.h>
#include <db_access.h>

#include "audit_logger.h"
#include "channel_access_context.h"
#include "runtime_utils.h"
#include "statistics_tracker.h"
#include "text_area_element.h"
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

bool parseSexagesimalField(const char *text, char delimiter, int &sign,
    const char *&next, double &value)
{
  sign = 0;
  value = 0.0;
  next = text;

  while (*next == ' ' || *next == '\t') {
    ++next;
  }

  if (*next == '+' || *next == '-') {
    sign = (*next == '-') ? -1 : 1;
    ++next;
    while (*next == ' ' || *next == '\t') {
      ++next;
    }
    if (*next == '+' || *next == '-') {
      return false;
    }
  }

  if (*next == delimiter) {
    if (delimiter != '\0') {
      ++next;
      while (*next == ' ' || *next == '\t') {
        ++next;
      }
    }
    return true;
  }

  char *end = nullptr;
  value = std::strtod(next, &end);
  if (end == next) {
    return false;
  }
  value = std::abs(value);
  next = end;

  while (*next == ' ' || *next == '\t') {
    ++next;
  }
  if (*next == delimiter) {
    if (delimiter != '\0') {
      ++next;
      while (*next == ' ' || *next == '\t') {
        ++next;
      }
    }
  }

  return true;
}

} // namespace

TextAreaRuntime::TextAreaRuntime(TextAreaElement *element)
  : QObject(element)
  , element_(element)
{
  if (element_) {
    channelName_ = element_->channel().trimmed();
  }
}

TextAreaRuntime::~TextAreaRuntime()
{
  stop();
}

void TextAreaRuntime::start()
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
  element_->setActivationCallback([this](const QByteArray &bytes) {
    handleActivation(bytes);
  });

  if (channelName_.isEmpty()) {
    return;
  }

  requestedType_ = DBR_TIME_CHAR;
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

void TextAreaRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
  if (element_) {
    element_->setActivationCallback(std::function<void(const QByteArray &)>());
  }
  resetRuntimeState();
}

void TextAreaRuntime::resetRuntimeState()
{
  connected_ = false;
  fieldType_ = -1;
  elementCount_ = 1;
  valueKind_ = ValueKind::kNone;
  lastNumericValue_ = 0.0;
  hasNumericValue_ = false;
  lastStringValue_.clear();
  lastBytesValue_.clear();
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

  invokeOnElement([](TextAreaElement *element) {
    element->clearRuntimeState();
  });
}

void TextAreaRuntime::resubscribe(int requestedType, long elementCount)
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

void TextAreaRuntime::handleChannelConnection(bool connected,
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

    int desiredType = DBR_TIME_CHAR;
    long desiredCount = 0;
    bool runtimeSingleLine = false;
    int runtimeByteLimit = -1;
    QString runtimeNotice;
    switch (fieldType_) {
    case DBR_STRING:
      valueKind_ = ValueKind::kString;
      desiredType = DBR_TIME_STRING;
      desiredCount = 1;
      runtimeSingleLine = true;
      runtimeNotice = QStringLiteral(
          "DBR_STRING PV: Text Area falls back to single-line editing and "
          "writes may be truncated to 40 bytes.");
      break;
    case DBR_ENUM:
      valueKind_ = ValueKind::kEnum;
      desiredType = DBR_TIME_ENUM;
      desiredCount = 1;
      break;
    case DBR_CHAR:
      if (elementCount_ > 1) {
        valueKind_ = ValueKind::kCharArray;
        desiredType = DBR_TIME_CHAR;
        desiredCount = elementCount_;
        runtimeByteLimit = static_cast<int>(std::max<long>(0, elementCount_ - 1));
      } else {
        valueKind_ = ValueKind::kNumeric;
        desiredType = DBR_TIME_CHAR;
        desiredCount = 1;
      }
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

    /* A count of zero already subscribes using the native element count. */
    const bool wildcardCountSatisfied = desiredType == requestedType_
        && requestedCount_ == 0 && desiredCount == elementCount_;
    if (wildcardCountSatisfied) {
      requestedCount_ = desiredCount;
    } else if (desiredType != requestedType_
        || desiredCount != requestedCount_) {
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
      invokeOnElement([low, high, precision](TextAreaElement *element) {
        element->setRuntimeLimits(low, high);
        element->setRuntimePrecision(precision);
      });
    }

    invokeOnElement([runtimeSingleLine, runtimeByteLimit,
                        runtimeNotice](TextAreaElement *element) {
      element->setRuntimeSingleLine(runtimeSingleLine);
      element->setRuntimeByteLimit(runtimeByteLimit);
      element->setRuntimeNotice(runtimeNotice);
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
    invokeOnElement([](TextAreaElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeWriteAccess(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
  }
}

void TextAreaRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  if (data.isString) {
    lastStringValue_ = data.stringValue;
    lastBytesValue_ = data.stringValue.toUtf8();
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
    lastBytesValue_ = data.charArrayValue;
    lastStringValue_ = formatCharArray(data.charArrayValue);
    hasStringValue_ = true;
    hasNumericValue_ = data.isNumeric;
    if (hasNumericValue_) {
      lastNumericValue_ = data.numericValue;
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
    invokeOnElement([low, high, precision](TextAreaElement *element) {
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

void TextAreaRuntime::handleAccessRights(bool /*canRead*/, bool canWrite)
{
  if (!started_) {
    return;
  }
  if (canWrite == lastWriteAccess_) {
    return;
  }
  lastWriteAccess_ = canWrite;
  invokeOnElement([canWrite](TextAreaElement *element) {
    element->setRuntimeWriteAccess(canWrite);
  });
}

void TextAreaRuntime::handleActivation(const QByteArray &bytes)
{
  if (!started_ || !connected_ || !lastWriteAccess_) {
    if (element_) {
      element_->rejectRuntimeCommit(QStringLiteral(
          "Text Area write rejected: channel is not writable."));
    }
    return;
  }

  const QString rawText = QString::fromUtf8(bytes);
  const QString trimmed = rawText.trimmed();
  bool success = false;
  QString errorMessage;

  switch (valueKind_) {
  case ValueKind::kString:
    success = PvChannelManager::instance().putValue(channelName_, rawText);
    if (!success) {
      errorMessage = QStringLiteral("Text Area write failed for %1.")
                         .arg(channelName_);
      qWarning() << errorMessage << rawText;
      break;
    }
    AuditLogger::instance().logPut(channelName_, rawText,
        QStringLiteral("TextArea"));
    break;
  case ValueKind::kCharArray: {
    QByteArray payload = bytes;
    payload.append('\0');
    success = PvChannelManager::instance().putCharArrayValue(channelName_,
        payload);
    if (!success) {
      errorMessage = QStringLiteral("Text Area write failed for %1.")
                         .arg(channelName_);
      qWarning() << errorMessage << rawText;
      break;
    }
    AuditLogger::instance().logPut(channelName_, rawText,
        QStringLiteral("TextArea"));
    break;
  }
  case ValueKind::kEnum: {
    short enumValue = 0;
    if (!parseEnumInput(trimmed, enumValue)) {
      errorMessage = QStringLiteral(
          "Text Area write rejected: value is not a valid enum choice.");
      qWarning() << errorMessage << channelName_ << trimmed;
      break;
    }
    success = PvChannelManager::instance().putValue(channelName_,
        static_cast<dbr_enum_t>(enumValue));
    if (!success) {
      errorMessage = QStringLiteral("Text Area write failed for %1.")
                         .arg(channelName_);
      qWarning() << errorMessage << trimmed;
      break;
    }
    AuditLogger::instance().logPut(channelName_, static_cast<int>(enumValue),
        QStringLiteral("TextArea"));
    break;
  }
  case ValueKind::kNumeric:
  case ValueKind::kNone:
  default: {
    double numeric = 0.0;
    if (!parseNumericInput(trimmed, numeric)) {
      errorMessage = QStringLiteral(
          "Text Area write rejected: value is not a valid number.");
      qWarning() << errorMessage << channelName_ << trimmed;
      break;
    }
    success = PvChannelManager::instance().putValue(channelName_, numeric);
    if (!success) {
      errorMessage = QStringLiteral("Text Area write failed for %1.")
                         .arg(channelName_);
      qWarning() << errorMessage << trimmed;
      break;
    }
    AuditLogger::instance().logPut(channelName_, numeric,
        QStringLiteral("TextArea"));
    break;
  }
  }

  if (!element_) {
    return;
  }
  if (success) {
    element_->acceptRuntimeCommit(bytes);
  } else if (!errorMessage.isEmpty()) {
    element_->rejectRuntimeCommit(errorMessage);
  }
}

void TextAreaRuntime::updateElementDisplay()
{
  if (!element_) {
    return;
  }
  if (!connected_) {
    invokeOnElement([](TextAreaElement *element) {
      element->setRuntimeConnected(false);
      element->setRuntimeSeverity(kInvalidSeverity);
    });
    return;
  }

  QString displayText;
  QByteArray displayBytes;
  bool useBytes = false;
  switch (valueKind_) {
  case ValueKind::kString:
    displayText = lastStringValue_;
    break;
  case ValueKind::kEnum:
    if (!enumStrings_.isEmpty()) {
      const int index = static_cast<int>(lastEnumValue_);
      if (index >= 0 && index < enumStrings_.size()) {
        displayText = enumStrings_.at(index);
      }
    }
    if (displayText.isEmpty() && hasNumericValue_) {
      displayText = formatNumeric(lastNumericValue_, resolvedPrecision());
    }
    break;
  case ValueKind::kCharArray:
    displayBytes = lastBytesValue_;
    useBytes = true;
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
  invokeOnElement([displayText, displayBytes, useBytes, severity,
                      needsInitialMark, this](TextAreaElement *element) {
    element->setRuntimeConnected(true);
    element->setRuntimeSeverity(severity);
    if (useBytes) {
      element->setRuntimeTextBytes(displayBytes);
    } else {
      element->setRuntimeText(displayText);
    }
    if (needsInitialMark && !initialUpdateTracked_) {
      auto &tracker = StartupUiSettlingTracker::instance();
      if (tracker.enabled()) {
        tracker.recordInitialUpdateApplied();
      }
      initialUpdateTracked_ = true;
    }
  });
}

int TextAreaRuntime::resolvedPrecision() const
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

QString TextAreaRuntime::formatNumeric(double value, int precision) const
{
  if (!element_) {
    return QString();
  }

  const unsigned short epicsPrecision = static_cast<unsigned short>(precision);
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

QString TextAreaRuntime::formatCharArray(const QByteArray &bytes) const
{
  if (bytes.isEmpty()) {
    return QString();
  }
  QByteArray payload = bytes;
  const int nullIndex = payload.indexOf('\0');
  if (nullIndex >= 0) {
    payload.truncate(nullIndex);
  }
  return QString::fromUtf8(payload.constData(), payload.size());
}

bool TextAreaRuntime::parseNumericInput(const QString &text, double &value) const
{
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  bool ok = false;
  const auto format = element_ ? element_->format() : TextMonitorFormat::kDecimal;

  switch (format) {
  case TextMonitorFormat::kHexadecimal:
    value = static_cast<double>(trimmed.toLongLong(&ok, 16));
    return ok;
  case TextMonitorFormat::kOctal:
    value = static_cast<double>(trimmed.toLongLong(&ok, 8));
    return ok;
  case TextMonitorFormat::kTruncated:
    value = static_cast<double>(trimmed.toLongLong(&ok, 10));
    return ok;
  case TextMonitorFormat::kSexagesimal:
  case TextMonitorFormat::kSexagesimalHms:
  case TextMonitorFormat::kSexagesimalDms:
    return parseSexagesimal(trimmed, value);
  case TextMonitorFormat::kDecimal:
  case TextMonitorFormat::kExponential:
  case TextMonitorFormat::kEngineering:
  case TextMonitorFormat::kCompact:
  case TextMonitorFormat::kString:
  default:
    value = trimmed.toDouble(&ok);
    return ok;
  }
}

bool TextAreaRuntime::parseSexagesimal(const QString &text, double &value) const
{
  const QByteArray ascii = text.toLatin1();
  const char *cursor = ascii.constData();
  int sign1 = 0;
  int sign2 = 0;
  int sign3 = 0;
  double first = 0.0;
  double second = 0.0;
  double third = 0.0;
  if (!parseSexagesimalField(cursor, ':', sign1, cursor, first)) {
    return false;
  }
  if (!parseSexagesimalField(cursor, ':', sign2, cursor, second)) {
    return false;
  }
  if (!parseSexagesimalField(cursor, '\0', sign3, cursor, third)) {
    return false;
  }
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  if (*cursor != '\0') {
    return false;
  }

  const int sign = sign1 != 0 ? sign1 : (sign2 != 0 ? sign2 : sign3);
  value = first + second / 60.0 + third / 3600.0;
  if (sign < 0) {
    value = -value;
  }
  if (element_) {
    switch (element_->format()) {
    case TextMonitorFormat::kSexagesimalHms:
      value = value * kPi / 12.0;
      break;
    case TextMonitorFormat::kSexagesimalDms:
      value = value * kPi / 180.0;
      break;
    default:
      break;
    }
  }
  return true;
}

bool TextAreaRuntime::parseEnumInput(const QString &text, short &value) const
{
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }

  bool ok = false;
  const int numeric = trimmed.toInt(&ok);
  if (ok) {
    value = static_cast<short>(numeric);
    return true;
  }

  for (int i = 0; i < enumStrings_.size(); ++i) {
    if (enumStrings_.at(i).compare(trimmed, Qt::CaseInsensitive) == 0) {
      value = static_cast<short>(i);
      return true;
    }
  }

  return false;
}
