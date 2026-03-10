#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct PvaBridgeData
{
  bool connected = false;
  bool canRead = false;
  bool canWrite = false;
  int nativeFieldType = -1;
  long nativeElementCount = 0;
  double numericValue = 0.0;
  std::string stringValue;
  unsigned int enumValue = 0;
  std::vector<double> arrayValues;
  std::shared_ptr<const double> sharedArrayData;
  size_t sharedArraySize = 0;
  short severity = 0;
  short status = 0;
  double hopr = 0.0;
  double lopr = 0.0;
  short precision = -1;
  std::string units;
  std::vector<std::string> enumStrings;
  bool hasControlInfo = false;
  bool hasUnits = false;
  bool hasPrecision = false;
  bool hasValue = false;
  bool isNumeric = false;
  bool isString = false;
  bool isEnum = false;
  bool isCharArray = false;
  bool isArray = false;
  std::string host;
};

struct PvaBridgeChannel;

PvaBridgeChannel *pvaBridgeCreateChannel(const std::string &rawName,
    const std::string &pvName, long requestedType, long elementCount);
void pvaBridgeDestroyChannel(PvaBridgeChannel *channel);

bool pvaBridgeRefresh(PvaBridgeChannel *channel, bool updatesPaused);
int pvaBridgePoll(PvaBridgeChannel *channel, bool *connectionChanged,
    bool updatesPaused);
void pvaBridgeSetMonitoringPaused(PvaBridgeChannel *channel, bool paused);

bool pvaBridgeGetData(const PvaBridgeChannel *channel, PvaBridgeData *data);

bool pvaBridgePutDouble(PvaBridgeChannel *channel, double value);
bool pvaBridgePutString(PvaBridgeChannel *channel, const std::string &value);
bool pvaBridgePutDoubleArray(PvaBridgeChannel *channel,
    const double *values, size_t count);