#include "pva_bridge.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <db_access.h>

#include "pvaSDDS.h"

namespace {

constexpr double kConnectTimeoutSeconds = 1.0;

using SharedDoubleVector = epics::pvData::shared_vector<const double>;

struct PvaBridgeChannelImpl
{
  std::string rawName;
  std::string pvName;
  long requestedType = 0;
  long elementCount = 0;
  bool monitoringPaused = false;
  PVA_OVERALL *pva = nullptr;
  PvaBridgeData cachedData;
};

static void updateAccess(PvaBridgeChannelImpl *channel)
{
  if (!channel || !channel->pva) {
    return;
  }

  channel->cachedData.canRead = HaveReadAccess(channel->pva, 0);
  channel->cachedData.canWrite = HaveWriteAccess(channel->pva, 0);
}

static void updateCachedData(PvaBridgeChannelImpl *channel, bool updatesPaused)
{
  if (!channel || !channel->pva) {
    return;
  }

  if (channel->cachedData.connected) {
    ExtractPVAUnits(channel->pva);
    ExtractPVAControlInfo(channel->pva);
  }

  PvaBridgeData data = channel->cachedData;
  data.connected = channel->cachedData.connected;
  data.severity = channel->pva->pvaData[0].alarmSeverity;
  data.status = 0;
  data.hasControlInfo = false;
  data.hasUnits = false;
  data.hasPrecision = false;
  data.hasValue = false;
  data.isNumeric = false;
  data.isString = false;
  data.isEnum = false;
  data.isCharArray = false;
  data.isArray = false;
  data.arrayValues.clear();
  data.sharedArrayData.reset();
  data.sharedArraySize = 0;
  data.stringValue.clear();
  data.enumStrings.clear();
  data.host = GetRemoteAddress(channel->pva, 0);

  const PVA_DATA_ALL_READINGS &reading = channel->pva->pvaData[0];
  const PVA_DATA *source = nullptr;
  long elementCount = 0;

  if (reading.numMonitorReadings > 0) {
    source = reading.monitorData;
    elementCount = reading.numMonitorElements;
  } else if (reading.numGetReadings > 0) {
    source = reading.getData;
    elementCount = reading.numGetElements;
  }

  if (source && reading.numeric && source[0].values) {
    data.numericValue = source[0].values[0];
    data.isNumeric = true;
    data.hasValue = true;
    if (elementCount > 1) {
      data.isArray = true;
      if (elementCount > 1000 && updatesPaused) {
        data.arrayValues.clear();
      } else if (reading.monitorOpaqueVector != nullptr) {
        auto *srcVec = static_cast<SharedDoubleVector *>(
            reading.monitorOpaqueVector);
        auto *keptVec = new SharedDoubleVector(*srcVec);
        data.sharedArrayData = std::shared_ptr<const double>(keptVec->data(),
            [keptVec](const double *) { delete keptVec; });
        data.sharedArraySize = static_cast<size_t>(elementCount);
      } else {
        data.arrayValues.resize(static_cast<size_t>(elementCount));
        std::memcpy(data.arrayValues.data(), source[0].values,
            static_cast<size_t>(elementCount) * sizeof(double));
      }
    }
  }

  if (source && reading.nonnumeric && source[0].stringValues) {
    const char *text = source[0].stringValues[0];
    data.stringValue = text ? text : "";
    data.isString = true;
    data.hasValue = true;
  }

  if (IsEnumFieldType(channel->pva, 0)) {
    char **choices = nullptr;
    const uint32_t count = GetEnumChoices(channel->pva, 0, &choices);
    if (count > 0 && choices) {
      data.enumStrings.reserve(static_cast<size_t>(count));
      for (uint32_t i = 0; i < count; ++i) {
        std::string choice = choices[i] ? choices[i] : "";
        if (choice.size() > 1 && choice.front() == '{'
            && choice.back() == '}') {
          choice = choice.substr(1, choice.size() - 2);
        }
        data.enumStrings.push_back(choice);
        free(choices[i]);
      }
      free(choices);
      data.isEnum = true;
      data.enumValue = static_cast<unsigned int>(data.numericValue);
      data.hasControlInfo = true;
    }
  }

  data.units = GetUnits(channel->pva, 0);
  data.hasUnits = !data.units.empty();

  if (reading.hasDisplayLimits) {
    data.lopr = reading.displayLimitLow;
    data.hopr = reading.displayLimitHigh;
    data.hasControlInfo = true;
  } else if (reading.hasControlLimits) {
    data.lopr = reading.controlLimitLow;
    data.hopr = reading.controlLimitHigh;
    data.hasControlInfo = true;
  }

  if (data.hasControlInfo) {
    const double low = std::min(data.lopr, data.hopr);
    double high = std::max(data.lopr, data.hopr);
    if (high == low) {
      high = low + 1.0;
    }
    data.lopr = low;
    data.hopr = high;
  }

  if (reading.hasPrecision) {
    data.precision = static_cast<short>(reading.displayPrecision);
    data.hasPrecision = true;
    data.hasControlInfo = true;
  }

  if (IsEnumFieldType(channel->pva, 0)) {
    data.nativeFieldType = DBF_ENUM;
  } else if (reading.nonnumeric) {
    data.nativeFieldType = DBF_STRING;
  } else {
    data.nativeFieldType = DBF_DOUBLE;
  }

  if (elementCount <= 0) {
    elementCount = 1;
  }
  data.nativeElementCount = static_cast<long>(GetElementCount(channel->pva, 0));
  if (data.nativeElementCount <= 0) {
    data.nativeElementCount = elementCount;
  }

  channel->cachedData = std::move(data);
}

} // namespace

struct PvaBridgeChannel
{
  PvaBridgeChannelImpl impl;
};

PvaBridgeChannel *pvaBridgeCreateChannel(const std::string &rawName,
    const std::string &pvName, long requestedType, long elementCount)
{
  std::unique_ptr<PvaBridgeChannel> channel(new PvaBridgeChannel);
  channel->impl.rawName = rawName;
  channel->impl.pvName = pvName;
  channel->impl.requestedType = requestedType;
  channel->impl.elementCount = elementCount;
  channel->impl.pva = new PVA_OVERALL();

  allocPVA(channel->impl.pva, 1);
  channel->impl.pva->includeAlarmSeverity = true;

  epics::pvData::shared_vector<std::string> names(1);
  names[0] = pvName;
  channel->impl.pva->pvaChannelNames = freeze(names);

  epics::pvData::shared_vector<std::string> providers(1);
  providers[0] = "pva";
  channel->impl.pva->pvaProvider = freeze(providers);

  ConnectPVA(channel->impl.pva, kConnectTimeoutSeconds);
  GetPVAValues(channel->impl.pva);
  MonitorPVAValues(channel->impl.pva);

  if (channel->impl.pva->isConnected.size() > 0) {
    channel->impl.cachedData.connected = channel->impl.pva->isConnected[0];
  }
  updateAccess(&channel->impl);
  updateCachedData(&channel->impl, false);

  return channel.release();
}

void pvaBridgeDestroyChannel(PvaBridgeChannel *channel)
{
  if (!channel) {
    return;
  }

  if (channel->impl.pva) {
    freePVA(channel->impl.pva);
    delete channel->impl.pva;
    channel->impl.pva = nullptr;
  }
  delete channel;
}

bool pvaBridgeRefresh(PvaBridgeChannel *channel, bool updatesPaused)
{
  if (!channel || !channel->impl.pva) {
    return false;
  }

  if (channel->impl.pva->isConnected.size() > 0) {
    channel->impl.cachedData.connected = channel->impl.pva->isConnected[0];
  }
  updateAccess(&channel->impl);
  updateCachedData(&channel->impl, updatesPaused);
  return true;
}

int pvaBridgePoll(PvaBridgeChannel *channel, bool *connectionChanged,
    bool updatesPaused)
{
  if (connectionChanged) {
    *connectionChanged = false;
  }
  if (!channel || !channel->impl.pva) {
    return 0;
  }

  const bool wasConnected = channel->impl.cachedData.connected;
  const int events = PollMonitoredPVA(channel->impl.pva);

  if (channel->impl.pva->isConnected.size() > 0) {
    channel->impl.cachedData.connected = channel->impl.pva->isConnected[0];
  }

  if (channel->impl.cachedData.connected != wasConnected) {
    if (connectionChanged) {
      *connectionChanged = true;
    }
    updateAccess(&channel->impl);
    updateCachedData(&channel->impl, updatesPaused);
  } else if (events > 0) {
    updateCachedData(&channel->impl, updatesPaused);
  }

  return events;
}

void pvaBridgeSetMonitoringPaused(PvaBridgeChannel *channel, bool paused)
{
  if (!channel || !channel->impl.pva || channel->impl.monitoringPaused == paused) {
    return;
  }

  if (paused) {
    PausePVAMonitoring(channel->impl.pva);
  } else {
    ResumePVAMonitoring(channel->impl.pva);
  }
  channel->impl.monitoringPaused = paused;
}

bool pvaBridgeGetData(const PvaBridgeChannel *channel, PvaBridgeData *data)
{
  if (!channel || !data) {
    return false;
  }

  *data = channel->impl.cachedData;
  return true;
}

bool pvaBridgePutDouble(PvaBridgeChannel *channel, double value)
{
  if (!channel || !channel->impl.pva || !channel->impl.cachedData.connected) {
    return false;
  }

  if (PrepPut(channel->impl.pva, 0, value) != 0) {
    return false;
  }
  return PutPVAValues(channel->impl.pva) == 0;
}

bool pvaBridgePutString(PvaBridgeChannel *channel, const std::string &value)
{
  if (!channel || !channel->impl.pva || !channel->impl.cachedData.connected) {
    return false;
  }

  std::vector<char> bytes(value.begin(), value.end());
  bytes.push_back('\0');
  if (PrepPut(channel->impl.pva, 0, bytes.data()) != 0) {
    return false;
  }
  return PutPVAValues(channel->impl.pva) == 0;
}

bool pvaBridgePutDoubleArray(PvaBridgeChannel *channel,
    const double *values, size_t count)
{
  if (!channel || !channel->impl.pva || !channel->impl.cachedData.connected
      || !values) {
    return false;
  }

  if (PrepPut(channel->impl.pva, 0, const_cast<double *>(values),
          static_cast<long>(count)) != 0) {
    return false;
  }
  return PutPVAValues(channel->impl.pva) == 0;
}
