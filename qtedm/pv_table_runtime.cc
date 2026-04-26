#include "pv_table_runtime.h"

#include <cmath>

#include <QByteArray>

#include "channel_access_context.h"
#include "pv_table_element.h"
#include "pv_name_utils.h"

#include <cvtFast.h>
#include <db_access.h>

namespace {

constexpr short kInvalidAlarmSeverity = 3;

}

PvTableRuntime::PvTableRuntime(PvTableElement *element)
  : QObject(element)
  , element_(element)
{
}

PvTableRuntime::~PvTableRuntime()
{
  stop();
}

void PvTableRuntime::start()
{
  if (started_ || !element_) {
    return;
  }
  started_ = true;
  resetRuntimeState();

  const QVector<PvTableRowConfig> configs = element_->rows();
  rows_.clear();
  rows_.reserve(configs.size());
  for (int i = 0; i < configs.size(); ++i) {
    rows_.push_back(std::make_unique<RowSubscription>());
    RowSubscription &state = *rows_[i];
    state.channel = configs.at(i).channel.trimmed();
    if (!state.channel.isEmpty()
        && parsePvName(state.channel).protocol == PvProtocol::kCa) {
      ChannelAccessContext::instance().ensureInitializedForProtocol(
          PvProtocol::kCa);
    }
    subscribeRow(i);
  }
}

void PvTableRuntime::stop()
{
  if (!started_) {
    return;
  }
  started_ = false;
  for (auto &row : rows_) {
    if (row) {
      row->subscription.reset();
    }
  }
  resetRuntimeState();
  rows_.clear();
}

void PvTableRuntime::resetRuntimeState()
{
  if (!element_) {
    return;
  }
  element_->clearRuntimeState();
}

void PvTableRuntime::subscribeRow(int row)
{
  if (!element_ || row < 0 || row >= static_cast<int>(rows_.size())
      || !rows_[row]) {
    return;
  }

  RowSubscription &state = *rows_[row];
  if (state.channel.isEmpty()) {
    element_->setRowConnected(row, false);
    return;
  }

  auto &mgr = PvChannelManager::instance();
  state.subscription = mgr.subscribe(
      state.channel,
      DBR_TIME_DOUBLE,
      0,
      [this, row](const SharedChannelData &data) {
        handleChannelData(row, data);
      },
      [this, row](bool connected, const SharedChannelData &data) {
        handleChannelConnection(row, connected, data);
      });
}

chtype PvTableRuntime::determineSubscriptionType(short nativeFieldType,
    long elementCount) const
{
  switch (nativeFieldType) {
  case DBR_STRING:
    return DBR_TIME_STRING;
  case DBR_ENUM:
    return DBR_TIME_ENUM;
  case DBR_CHAR:
    return elementCount > 1 ? DBR_TIME_CHAR : DBR_TIME_DOUBLE;
  default:
    return DBR_TIME_DOUBLE;
  }
}

void PvTableRuntime::handleChannelConnection(int row, bool connected,
    const SharedChannelData &data)
{
  if (!started_ || row < 0 || row >= static_cast<int>(rows_.size())
      || !rows_[row]) {
    return;
  }

  RowSubscription &state = *rows_[row];
  state.connected = connected;

  if (connected) {
    if (state.nativeFieldType < 0 && data.nativeFieldType >= 0) {
      state.nativeFieldType = data.nativeFieldType;
      state.elementCount = data.nativeElementCount;
      const chtype neededType = determineSubscriptionType(state.nativeFieldType,
          state.elementCount);
      if (neededType != DBR_TIME_DOUBLE) {
        auto &mgr = PvChannelManager::instance();
        state.subscription.reset();
        state.subscription = mgr.subscribe(
            state.channel,
            neededType,
            (neededType == DBR_TIME_CHAR) ? state.elementCount : 1,
            [this, row](const SharedChannelData &newData) {
              handleChannelData(row, newData);
            },
            [this, row](bool conn, const SharedChannelData &connData) {
              handleChannelConnection(row, conn, connData);
            });
        return;
      }
    }
    element_->setRowConnected(row, true);
  } else {
    state.valueKind = ValueKind::kNone;
    state.hasNumericValue = false;
    state.lastStringValue.clear();
    state.enumStrings.clear();
    state.units.clear();
    state.lastSeverity = kInvalidAlarmSeverity;
    element_->setRowConnected(row, false);
    element_->setRowSeverity(row, kInvalidAlarmSeverity);
    element_->setRowValue(row, QString());
    element_->setRowMetadata(row, QString());
  }
}

void PvTableRuntime::handleChannelData(int row, const SharedChannelData &data)
{
  if (!started_ || row < 0 || row >= static_cast<int>(rows_.size())
      || !rows_[row]) {
    return;
  }

  RowSubscription &state = *rows_[row];
  if (state.nativeFieldType < 0 && data.nativeFieldType >= 0) {
    state.nativeFieldType = data.nativeFieldType;
    state.elementCount = data.nativeElementCount;
  }

  switch (state.nativeFieldType) {
  case DBR_STRING:
    state.valueKind = ValueKind::kString;
    break;
  case DBR_ENUM:
    state.valueKind = ValueKind::kEnum;
    break;
  case DBR_CHAR:
    state.valueKind = state.elementCount > 1 ? ValueKind::kCharArray
                                             : ValueKind::kNumeric;
    break;
  default:
    state.valueKind = ValueKind::kNumeric;
    break;
  }

  state.lastSeverity = data.severity;

  if (data.isString) {
    state.lastStringValue = data.stringValue;
  }
  if (data.isCharArray) {
    state.lastStringValue = formatCharArray(data.charArrayValue);
    state.lastNumericValue = data.numericValue;
    state.hasNumericValue = true;
  }
  if (data.isEnum) {
    state.lastEnumValue = data.enumValue;
    state.lastNumericValue = data.numericValue;
    state.hasNumericValue = true;
  }
  if (data.isNumeric && !data.isEnum && !data.isCharArray) {
    state.lastNumericValue = data.numericValue;
    state.hasNumericValue = true;
  }
  if (data.hasControlInfo) {
    state.channelPrecision = data.precision;
    state.enumStrings = data.enumStrings;
    state.units = data.units;
  }

  applyRowState(row);
}

QString PvTableRuntime::formatRowValue(const RowSubscription &row) const
{
  switch (row.valueKind) {
  case ValueKind::kString:
    return row.lastStringValue;
  case ValueKind::kCharArray:
    return row.lastStringValue;
  case ValueKind::kEnum:
    return formatEnumValue(row);
  case ValueKind::kNumeric:
    if (row.hasNumericValue) {
      return formatNumeric(row.lastNumericValue, resolvedPrecision(row));
    }
    break;
  case ValueKind::kNone:
    break;
  }
  return QString();
}

QString PvTableRuntime::formatNumeric(double value, int precision) const
{
  if (!std::isfinite(value)) {
    return QStringLiteral("NaN");
  }
  char buffer[64];
  cvtDoubleToString(value, buffer, precision);
  return QString::fromLatin1(buffer);
}

QString PvTableRuntime::formatCharArray(const QByteArray &bytes) const
{
  int end = bytes.indexOf('\0');
  if (end < 0) {
    end = bytes.size();
  }
  return QString::fromLatin1(bytes.constData(), end);
}

QString PvTableRuntime::formatEnumValue(const RowSubscription &row) const
{
  const int index = static_cast<int>(row.lastEnumValue);
  if (index >= 0 && index < row.enumStrings.size()) {
    return row.enumStrings.at(index);
  }
  if (row.hasNumericValue) {
    return formatNumeric(row.lastNumericValue, resolvedPrecision(row));
  }
  return QString();
}

int PvTableRuntime::resolvedPrecision(const RowSubscription &row) const
{
  return row.channelPrecision >= 0 ? row.channelPrecision : 3;
}

void PvTableRuntime::applyRowState(int row)
{
  if (!element_ || row < 0 || row >= static_cast<int>(rows_.size())
      || !rows_[row]) {
    return;
  }
  const RowSubscription &state = *rows_[row];
  element_->setRowConnected(row, state.connected);
  element_->setRowSeverity(row, state.lastSeverity);
  element_->setRowValue(row, formatRowValue(state));
  element_->setRowMetadata(row, state.units);
}
