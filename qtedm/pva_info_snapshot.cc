#include "pva_info_snapshot.h"

#include "pva_channel_manager.h"

bool getPvaInfoSnapshot(const QString &pvName, PvaInfoSnapshot &snapshot)
{
  PvaChannelManager::PvaInfoSnapshot pvaSnapshot;
  if (!PvaChannelManager::instance().getInfoSnapshot(pvName, pvaSnapshot)) {
    snapshot = PvaInfoSnapshot{};
    return false;
  }

  snapshot.pvName = pvaSnapshot.pvName;
  snapshot.connected = pvaSnapshot.connected;
  snapshot.canRead = pvaSnapshot.canRead;
  snapshot.canWrite = pvaSnapshot.canWrite;
  snapshot.fieldType = pvaSnapshot.fieldType;
  snapshot.elementCount = pvaSnapshot.elementCount;
  snapshot.nativeDataType = pvaSnapshot.nativeDataType;
  snapshot.host = pvaSnapshot.host;
  snapshot.value = pvaSnapshot.value;
  snapshot.numericValue = pvaSnapshot.numericValue;
  snapshot.enumValue = pvaSnapshot.enumValue;
  snapshot.isNumeric = pvaSnapshot.isNumeric;
  snapshot.isEnum = pvaSnapshot.isEnum;
  snapshot.hasValue = pvaSnapshot.hasValue;
  snapshot.arrayValues = pvaSnapshot.arrayValues;
  snapshot.isArray = pvaSnapshot.isArray;
  snapshot.severity = pvaSnapshot.severity;
  snapshot.hopr = pvaSnapshot.hopr;
  snapshot.lopr = pvaSnapshot.lopr;
  snapshot.hasLimits = pvaSnapshot.hasLimits;
  snapshot.precision = pvaSnapshot.precision;
  snapshot.hasPrecision = pvaSnapshot.hasPrecision;
  snapshot.units = pvaSnapshot.units;
  snapshot.hasUnits = pvaSnapshot.hasUnits;
  snapshot.states = pvaSnapshot.states;
  snapshot.hasStates = pvaSnapshot.hasStates;
  return true;
}
