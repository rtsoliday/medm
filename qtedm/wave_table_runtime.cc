#include "wave_table_runtime.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QByteArray>
#include <QDebug>

#include <cvtFast.h>
#include <db_access.h>

#include "channel_access_context.h"
#include "pv_protocol.h"
#include "runtime_utils.h"
#include "statistics_tracker.h"
#include "text_format_utils.h"
#include "wave_table_element.h"

namespace {

constexpr short kInvalidAlarmSeverity = 3;
constexpr int kDefaultPrecision = 3;
constexpr int kMaxDisplayedElementsWithoutLimit = 10000;

QVector<double> numericVectorFromSharedData(const SharedChannelData &data)
{
  if (data.sharedArrayData && data.sharedArraySize > 0) {
    const size_t boundedSize = std::min(data.sharedArraySize,
        static_cast<size_t>(std::numeric_limits<int>::max()));
    QVector<double> result(static_cast<int>(boundedSize));
    const double *source = data.sharedArrayData.get();
    for (int i = 0; i < result.size(); ++i) {
      result[i] = source[i];
    }
    return result;
  }
  return data.arrayValues;
}

} // namespace

WaveTableRuntime::WaveTableRuntime(WaveTableElement *element)
  : QObject(element)
  , element_(element)
{
}

WaveTableRuntime::~WaveTableRuntime()
{
  stop();
}

void WaveTableRuntime::start()
{
  if (started_ || !element_) {
    return;
  }

  channelName_ = element_->channel().trimmed();
  resetRuntimeState();
  started_ = true;
  StatisticsTracker::instance().registerDisplayObjectStarted();

  if (channelName_.isEmpty()) {
    return;
  }

  if (parsePvName(channelName_).protocol == PvProtocol::kCa) {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitializedForProtocol(PvProtocol::kCa);
    if (!context.isInitialized()) {
      qWarning() << "Channel Access context not available for Waveform Table";
      return;
    }
  }

  resubscribe(DBR_TIME_DOUBLE, 0);
}

void WaveTableRuntime::stop()
{
  if (!started_) {
    return;
  }

  started_ = false;
  StatisticsTracker::instance().registerDisplayObjectStopped();
  subscription_.reset();
  resetRuntimeState();
}

void WaveTableRuntime::resetRuntimeState()
{
  connected_ = false;
  initialUpdateTracked_ = false;
  nativeFieldType_ = -1;
  nativeElementCount_ = 0;
  lastSeverity_ = kInvalidAlarmSeverity;
  channelPrecision_ = -1;
  units_.clear();
  enumStrings_.clear();
  requestedType_ = DBR_TIME_DOUBLE;
  requestedCount_ = 0;
  if (element_) {
    element_->clearRuntimeState();
  }
}

void WaveTableRuntime::resubscribe(chtype requestedType, long elementCount)
{
  if (!started_ || channelName_.isEmpty()) {
    return;
  }

  requestedType_ = requestedType;
  requestedCount_ = std::max<long>(0, elementCount);
  subscription_.reset();

  auto &mgr = PvChannelManager::instance();
  subscription_ = mgr.subscribe(
      channelName_,
      requestedType_,
      requestedCount_,
      [this](const SharedChannelData &data) { handleChannelData(data); },
      [this](bool connected, const SharedChannelData &data) {
        handleChannelConnection(connected, data);
      });
}

void WaveTableRuntime::handleChannelConnection(bool connected,
    const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  connected_ = connected;
  if (!connected) {
    lastSeverity_ = kInvalidAlarmSeverity;
    if (element_) {
      element_->setConnected(false);
      element_->setSeverity(kInvalidAlarmSeverity);
      element_->setValues(QVector<QString>(), 0);
    }
    return;
  }

  nativeFieldType_ = data.nativeFieldType;
  nativeElementCount_ = std::max<long>(data.nativeElementCount, 1);
  const chtype desiredType = subscriptionTypeForNativeType(nativeFieldType_);
  const long desiredCount = subscriptionCountForNativeType(nativeFieldType_,
      nativeElementCount_);
  const bool wildcardCountSatisfied = desiredType == requestedType_
      && requestedCount_ == 0 && desiredCount == nativeElementCount_;
  if (wildcardCountSatisfied) {
    requestedCount_ = desiredCount;
  } else if (desiredType != requestedType_ || desiredCount != requestedCount_) {
    resubscribe(desiredType, desiredCount);
    return;
  }

  if (data.hasPrecision) {
    channelPrecision_ = data.precision;
  }
  if (data.hasControlInfo || data.hasUnits) {
    units_ = data.units;
  }
  if (!data.enumStrings.isEmpty()) {
    enumStrings_ = data.enumStrings;
  }

  if (element_) {
    element_->setConnected(true);
    element_->setMetadata(nativeFieldType_, nativeElementCount_, units_,
        resolvedPrecision());
  }
}

void WaveTableRuntime::handleChannelData(const SharedChannelData &data)
{
  if (!started_) {
    return;
  }

  connected_ = data.connected;
  if (data.nativeFieldType >= 0) {
    nativeFieldType_ = data.nativeFieldType;
  }
  if (data.nativeElementCount > 0) {
    nativeElementCount_ = data.nativeElementCount;
  }
  lastSeverity_ = data.severity;
  if (data.hasPrecision) {
    channelPrecision_ = data.precision;
  }
  if (data.hasControlInfo || data.hasUnits) {
    units_ = data.units;
  }
  if (!data.enumStrings.isEmpty()) {
    enumStrings_ = data.enumStrings;
  }

  QVector<QString> values;
  long receivedCount = 0;

  if (!data.stringArrayValue.isEmpty()) {
    receivedCount = data.stringArrayValue.size();
    values = formatStringValues(data.stringArrayValue);
  } else if (data.isCharArray) {
    receivedCount = data.charArrayValue.size();
    values = formatCharValues(data.charArrayValue);
  } else if (data.isArray) {
    const QVector<double> numericValues = numericVectorFromSharedData(data);
    receivedCount = numericValues.size();
    values = data.isEnum ? formatEnumValues(numericValues)
                         : formatNumericValues(numericValues);
  } else if (data.isEnum) {
    receivedCount = 1;
    values.append(formatEnum(static_cast<double>(data.enumValue)));
  } else if (data.isString) {
    receivedCount = 1;
    values.append(data.stringValue);
  } else if (data.isNumeric) {
    receivedCount = 1;
    values.append(formatNumeric(data.numericValue));
  } else {
    return;
  }

  nativeElementCount_ = std::max<long>(nativeElementCount_, receivedCount);
  applyElementState(values, receivedCount);
}

chtype WaveTableRuntime::subscriptionTypeForNativeType(
    short nativeFieldType) const
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

long WaveTableRuntime::subscriptionCountForNativeType(short nativeFieldType,
    long nativeElementCount) const
{
  switch (nativeFieldType) {
  case DBR_STRING:
  case DBR_ENUM:
  case DBR_CHAR:
    return std::max<long>(1, nativeElementCount);
  default:
    return 0;
  }
}

QVector<QString> WaveTableRuntime::formatNumericValues(
    const QVector<double> &values) const
{
  QVector<QString> result;
  const int count = displayLimit(values.size());
  result.reserve(count);
  for (int i = 0; i < count; ++i) {
    result.append(formatNumeric(values.at(i)));
  }
  return result;
}

QVector<QString> WaveTableRuntime::formatEnumValues(
    const QVector<double> &values) const
{
  QVector<QString> result;
  const int count = displayLimit(values.size());
  result.reserve(count);
  for (int i = 0; i < count; ++i) {
    result.append(formatEnum(values.at(i)));
  }
  return result;
}

QVector<QString> WaveTableRuntime::formatStringValues(
    const QStringList &values) const
{
  QVector<QString> result;
  const int count = displayLimit(values.size());
  result.reserve(count);
  for (int i = 0; i < count; ++i) {
    result.append(values.at(i));
  }
  return result;
}

QVector<QString> WaveTableRuntime::formatCharValues(
    const QByteArray &bytes) const
{
  QVector<QString> result;
  if (element_ && element_->charMode() == WaveTableCharMode::kString) {
    result.append(formatCharString(bytes));
    return result;
  }

  const int count = displayLimit(bytes.size());
  result.reserve(count);
  for (int i = 0; i < count; ++i) {
    result.append(formatCharByte(static_cast<unsigned char>(bytes.at(i))));
  }
  return result;
}

QString WaveTableRuntime::formatNumeric(double value) const
{
  if (!std::isfinite(value)) {
    return QStringLiteral("NaN");
  }

  const int precision = resolvedPrecision();
  const WaveTableValueFormat format = element_
      ? element_->valueFormat()
      : WaveTableValueFormat::kDefault;

  switch (format) {
  case WaveTableValueFormat::kFixed:
    return QString::number(value, 'f', precision);
  case WaveTableValueFormat::kScientific:
    return QString::number(value, 'e', precision);
  case WaveTableValueFormat::kHex:
    return TextFormatUtils::formatHex(static_cast<long>(std::llround(value)));
  case WaveTableValueFormat::kEngineering: {
    char buffer[TextFormatUtils::kMaxTextField];
    TextFormatUtils::localCvtDoubleToExpNotationString(value, buffer,
        static_cast<unsigned short>(precision));
    return QString::fromLatin1(buffer);
  }
  case WaveTableValueFormat::kDefault:
  default: {
    char buffer[TextFormatUtils::kMaxTextField];
    cvtDoubleToString(value, buffer, static_cast<unsigned short>(precision));
    return QString::fromLatin1(buffer);
  }
  }
}

QString WaveTableRuntime::formatEnum(double value) const
{
  const int index = static_cast<int>(std::llround(value));
  if (index >= 0 && index < enumStrings_.size()) {
    return enumStrings_.at(index);
  }
  return formatNumeric(value);
}

QString WaveTableRuntime::formatCharByte(unsigned char value) const
{
  const WaveTableCharMode mode = element_ ? element_->charMode()
                                          : WaveTableCharMode::kNumeric;
  if (mode == WaveTableCharMode::kBytes) {
    return QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0'));
  }
  if (mode == WaveTableCharMode::kAscii) {
    if (value == '\0') {
      return QStringLiteral("\\0");
    }
    if (value == '\n') {
      return QStringLiteral("\\n");
    }
    if (value == '\t') {
      return QStringLiteral("\\t");
    }
    if (value >= 32 && value <= 126) {
      return QString(QChar(static_cast<ushort>(value)));
    }
    return QStringLiteral("\\x%1").arg(value, 2, 16, QLatin1Char('0'));
  }
  return QString::number(static_cast<unsigned int>(value));
}

QString WaveTableRuntime::formatCharString(const QByteArray &bytes) const
{
  if (bytes.isEmpty()) {
    return QString();
  }
  QByteArray payload = bytes;
  const int nullIndex = payload.indexOf('\0');
  if (nullIndex >= 0) {
    payload.truncate(nullIndex);
  }
  const int maxElements = element_ ? element_->maxElements() : 0;
  if (maxElements > 0 && payload.size() > maxElements) {
    payload.truncate(maxElements);
  }
  return QString::fromLatin1(payload.constData(), payload.size());
}

int WaveTableRuntime::resolvedPrecision() const
{
  return std::clamp(channelPrecision_ >= 0 ? channelPrecision_
                                           : kDefaultPrecision,
      0, TextFormatUtils::kMaxPrecision);
}

int WaveTableRuntime::displayLimit(int receivedCount) const
{
  if (receivedCount <= 0) {
    return 0;
  }
  const int configured = element_ ? element_->maxElements() : 0;
  const int limit = configured > 0 ? configured
                                  : kMaxDisplayedElementsWithoutLimit;
  return std::min(receivedCount, limit);
}

void WaveTableRuntime::applyElementState(const QVector<QString> &values,
    long receivedCount)
{
  if (!element_) {
    return;
  }
  element_->setConnected(connected_);
  element_->setSeverity(lastSeverity_);
  element_->setMetadata(nativeFieldType_, nativeElementCount_, units_,
      resolvedPrecision());
  element_->setValues(values, receivedCount);
  initialUpdateTracked_ = true;
}
