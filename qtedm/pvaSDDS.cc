/**
 * @file pvaSDDS.cc
 * @brief Functions for managing and interacting with Process Variable Array (PVA) structures.
 * 
 * @details
 * This file includes a set of functions to allocate, reallocate, free, connect, monitor, and 
 * extract values for Process Variable Arrays (PVA) using EPICS PVAccess and PVData libraries. 
 * It provides utilities for managing PVAs in scenarios where EPICS Channel Access (CA) and 
 * PVAccess (PVA) protocols are used to interact with control system process variables.
 * 
 * The file also defines types and utilities for handling multimap-based structures and
 * formatting channel names and field requests.
 * 
 * Key functionalities:
 * - Memory allocation and reallocation for PVA structures.
 * - Connection to PV channels using PvaClientMultiChannel.
 * - Monitoring and polling for events on PVs.
 * - Extracting and preparing values for PVs.
 * - Support for scalar, array, and enumerated types within PVs.
 * - Utilities for interacting with PV metadata such as units and alarm severity.
 * 
 * Dependencies:
 * - EPICS PVAccess
 * - EPICS PVData
 * - <unordered_map>, <string>, <vector>, <map>, <set>, and other C++ standard library components.
 * 
 * @see https://epics.anl.gov
 * @see PvaClientMultiChannel documentation for details on channel operations.
 * @see https://docs.epics-controls.org/projects/pvaclient-cpp/en/latest/
 * 
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * R. Soliday,
 */

#include "pvaSDDS.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <inttypes.h>
#include <map>
#include <sstream>

static uint32_t GetElementCountFromNelm(PVA_OVERALL *pva, long index, size_t currentCount);

static long ExtractUnionValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode);
static long ExtractByPath(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr root, const std::string &path, bool monitorMode);
static long PutByPath(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr root, const std::string &path);

static bool ParseIndexedToken(const std::string &token, std::string &name, long &arrayIndex, bool &hasIndex);
static epics::pvData::PVFieldPtr ResolveFieldByPath(PVA_OVERALL *pva, long index,
                                                    epics::pvData::PVStructurePtr root,
                                                    const std::string &path, bool reportErrors);
static epics::pvData::PVFieldPtr GetRequestedField(PVA_OVERALL *pva, long index);
static bool EnsureGetReadingCapacity(PVA_OVERALL *pva, long index, long required);
static bool RefreshConnectionState(PVA_OVERALL *pva);
static bool RefreshSharedGetRequests(PVA_OVERALL *pva);
static void MarkInternalDisconnected(PVA_OVERALL *pva, long internalIndex);
static bool GetPVARequestedPath(PVA_OVERALL *pva, long index, std::string &path);
static bool HasGetData(PVA_OVERALL *pva, long index);
static void UpdateAlarmSeverity(PVA_OVERALL *pva, long index,
                                epics::pvData::PVStructurePtr root);
static bool ValidatePutArguments(PVA_OVERALL *pva, long index, const void *values, long length);
static bool ParseIntValue(const char *text, int *value);
static bool ParseDoubleValue(const char *text, double *value);
static bool ResizeNumericValues(double **values, long count);
static bool ResetStringValues(char ***values, long oldCount, long newCount);
static bool ReplaceStringValues(char ***values, long oldCount,
                                const std::vector<std::string> &newValues);

std::string convertToProperRequestFormat(const std::vector<std::string>& input);

class MonitorEventGuard {
public:
  explicit MonitorEventGuard(const epics::pvaClient::PvaClientMonitorPtr &monitor) : monitor(monitor) {
  }
  ~MonitorEventGuard() {
    if (monitor) {
      try {
        monitor->releaseEvent();
      } catch (...) {
      }
    }
  }

private:
  epics::pvaClient::PvaClientMonitorPtr monitor;
};

static bool EnsureGetReadingCapacity(PVA_OVERALL *pva, long index, long required) {
  PVA_DATA_ALL_READINGS *readings = &pva->pvaData[index];
  if (required <= readings->numGetReadingsAllocated)
    return true;

  long newCapacity = readings->numGetReadingsAllocated > 0 ? readings->numGetReadingsAllocated : 1;
  while (newCapacity < required) {
    if (newCapacity > LONG_MAX / 2) {
      newCapacity = required;
      break;
    }
    newCapacity *= 2;
  }
  if ((size_t)newCapacity > SIZE_MAX / sizeof(*readings->getData))
    return false;

  PVA_DATA *newData = (PVA_DATA *)realloc(readings->getData, sizeof(*newData) * newCapacity);
  if (newData == NULL)
    return false;
  for (long i = readings->numGetReadingsAllocated; i < newCapacity; i++) {
    newData[i].values = NULL;
    newData[i].stringValues = NULL;
  }
  readings->getData = newData;
  readings->numGetReadingsAllocated = newCapacity;
  return true;
}

static bool RefreshConnectionState(PVA_OVERALL *pva) {
  if (pva == NULL || pva->numMultiChannels <= 0 || pva->numInternalPVs < 0 ||
      pva->pvaClientMultiChannelPtr.size() < (size_t)pva->numMultiChannels ||
      pva->pvaClientChannelArray.size() < (size_t)pva->numInternalPVs)
    return false;
  pva->isInternalConnected.resize(pva->numInternalPVs);
  size_t offset = 0;
  for (int i = 0; i < pva->numMultiChannels; i++) {
    if (!pva->pvaClientMultiChannelPtr[i])
      return false;
    epics::pvData::shared_vector<epics::pvData::boolean> connected = pva->pvaClientMultiChannelPtr[i]->getIsConnected();
    if (connected.size() > pva->isInternalConnected.size() - offset)
      return false;
    std::copy(connected.begin(), connected.end(), pva->isInternalConnected.begin() + offset);
    offset += connected.size();
  }
  return offset == pva->isInternalConnected.size();
}

static bool RefreshSharedGetRequests(PVA_OVERALL *pva) {
  if (pva == NULL || pva->numInternalPVs < 0 ||
      pva->pvaClientGetOwner.size() < (size_t)pva->numInternalPVs ||
      pva->pvaClientGetRequest.size() < (size_t)pva->numInternalPVs)
    return false;

  std::vector<std::vector<std::string> > requestedFields(pva->numInternalPVs);
  for (long i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip || pva->pvaProvider[i].compare("pva") != 0)
      continue;
    long internalIndex = pva->pvaData[i].L2Ptr;
    if (internalIndex < 0 || internalIndex >= pva->numInternalPVs)
      return false;
    requestedFields[internalIndex].push_back(pva->pvaChannelNamesSub[i]);
    if (pva->includeAlarmSeverity)
      requestedFields[internalIndex].push_back("alarm.severity");
  }

  std::vector<bool> changed(pva->numInternalPVs, false);
  for (long i = 0; i < pva->numInternalPVs; i++) {
    std::string request = convertToProperRequestFormat(requestedFields[i]);
    if (request != pva->pvaClientGetRequest[i]) {
      pva->pvaClientGetRequest[i] = request;
      pva->pvaClientGetOwner[i] = -1;
      changed[i] = true;
    }
  }

  for (long i = 0; i < pva->numPVs; i++) {
    long internalIndex = pva->pvaData[i].L2Ptr;
    if (pva->pvaProvider[i].compare("pva") == 0 && internalIndex >= 0 &&
        internalIndex < pva->numInternalPVs && changed[internalIndex]) {
      pva->pvaClientGetPtr[i].reset();
      pva->pvaData[i].haveGetPtr = false;
    }
  }
  return true;
}

static void MarkInternalDisconnected(PVA_OVERALL *pva, long internalIndex) {
  if (pva == NULL || internalIndex < 0 || internalIndex >= pva->numInternalPVs)
    return;
  if ((size_t)internalIndex < pva->isInternalConnected.size())
    pva->isInternalConnected[internalIndex] = false;
  for (long i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].L2Ptr == internalIndex && (size_t)i < pva->isConnected.size())
      pva->isConnected[i] = false;
  }
}

static bool GetPVARequestedPath(PVA_OVERALL *pva, long index, std::string &path) {
  if (pva == NULL || index < 0 || index >= pva->numPVs ||
      pva->pvaProvider[index].compare("pva") != 0)
    return false;
  size_t dot = pva->pvaChannelNames[index].find('.');
  if (dot == std::string::npos || dot + 1 >= pva->pvaChannelNames[index].size())
    return false;
  path = pva->pvaChannelNames[index].substr(dot + 1);
  return true;
}

static bool HasGetData(PVA_OVERALL *pva, long index) {
  return pva != NULL && index >= 0 && index < pva->numPVs &&
         (size_t)index < pva->isConnected.size() && pva->isConnected[index] &&
         (size_t)index < pva->pvaClientGetPtr.size() && pva->pvaClientGetPtr[index] &&
         pva->pvaClientGetPtr[index]->getData() &&
         pva->pvaClientGetPtr[index]->getData()->getPVStructure();
}

static void UpdateAlarmSeverity(PVA_OVERALL *pva, long index,
                                epics::pvData::PVStructurePtr root) {
  if (pva == NULL || !pva->includeAlarmSeverity || index < 0 ||
      index >= pva->numPVs || !root)
    return;
  try {
    epics::pvData::PVScalarPtr severity =
      root->getSubField<epics::pvData::PVScalar>("alarm.severity");
    if (severity)
      pva->pvaData[index].alarmSeverity = severity->getAs<int>();
  } catch (std::exception &) {
    /* Alarm metadata is optional.  A malformed alarm field must not prevent
       the requested value from being read or monitored. */
  }
}

static bool ValidatePutArguments(PVA_OVERALL *pva, long index, const void *values, long length) {
  if (pva == NULL || pva->pvaData == NULL || index < 0 || index >= pva->numPVs ||
      length < 0 || (length > 0 && values == NULL)) {
    fprintf(stderr, "error: invalid arguments passed to PrepPut\n");
    return false;
  }
  return true;
}

static bool ParseIntValue(const char *text, int *value) {
  if (text == NULL || value == NULL || *text == '\0')
    return false;
  char *endp = NULL;
  errno = 0;
  long parsed = strtol(text, &endp, 10);
  if (errno == ERANGE || endp == text || endp == NULL)
    return false;
  while (std::isspace((unsigned char)*endp))
    endp++;
  if (*endp != '\0' || parsed < INT_MIN || parsed > INT_MAX)
    return false;
  *value = (int)parsed;
  return true;
}

static bool ParseDoubleValue(const char *text, double *value) {
  if (text == NULL || value == NULL || *text == '\0')
    return false;
  char *endp = NULL;
  errno = 0;
  double parsed = strtod(text, &endp);
  if (errno == ERANGE || endp == text || endp == NULL)
    return false;
  while (std::isspace((unsigned char)*endp))
    endp++;
  if (*endp != '\0')
    return false;
  *value = parsed;
  return true;
}

static bool ResizeNumericValues(double **values, long count) {
  if (values == NULL || count < 0 ||
      (size_t)count > SIZE_MAX / sizeof(**values))
    return false;
  if (count == 0) {
    free(*values);
    *values = NULL;
    return true;
  }
  double *resized = (double *)realloc(*values, sizeof(*resized) * count);
  if (resized == NULL)
    return false;
  *values = resized;
  return true;
}

static bool ResetStringValues(char ***values, long oldCount, long newCount) {
  if (values == NULL || oldCount < 0 || newCount < 0 ||
      (size_t)newCount > SIZE_MAX / sizeof(**values))
    return false;
  if (*values != NULL) {
    for (long i = 0; i < oldCount; i++) {
      free((*values)[i]);
      (*values)[i] = NULL;
    }
  }
  if (newCount == 0) {
    free(*values);
    *values = NULL;
    return true;
  }
  char **resized = (char **)realloc(*values, sizeof(*resized) * newCount);
  if (resized == NULL)
    return false;
  *values = resized;
  for (long i = 0; i < newCount; i++)
    (*values)[i] = NULL;
  return true;
}

static bool ReplaceStringValues(char ***values, long oldCount,
                                const std::vector<std::string> &newValues) {
  if (values == NULL || oldCount < 0 ||
      newValues.size() > SIZE_MAX / sizeof(**values))
    return false;

  char **replacement = NULL;
  if (!newValues.empty()) {
    replacement = (char **)calloc(newValues.size(), sizeof(*replacement));
    if (replacement == NULL)
      return false;
    for (size_t i = 0; i < newValues.size(); i++) {
      replacement[i] = (char *)malloc(newValues[i].size() + 1);
      if (replacement[i] == NULL) {
        for (size_t j = 0; j < i; j++)
          free(replacement[j]);
        free(replacement);
        return false;
      }
      memcpy(replacement[i], newValues[i].c_str(), newValues[i].size() + 1);
    }
  }

  if (*values != NULL) {
    for (long i = 0; i < oldCount; i++)
      free((*values)[i]);
    free(*values);
  }
  *values = replacement;
  return true;
}

/*
  Allocate memory for the pva structure.
  repeats is currently only used for "get" requests where you plan to do statistics over a few readings.
*/
void allocPVA(PVA_OVERALL *pva, long PVs) {
  allocPVA(pva, PVs, 0);
}

void allocPVA(PVA_OVERALL *pva, long PVs, long repeats) {
  long i, j;
  if (pva == NULL || PVs < 0 || repeats < 0 ||
      (size_t)PVs > SIZE_MAX / sizeof(PVA_DATA_ALL_READINGS) ||
      (size_t)repeats > SIZE_MAX / sizeof(PVA_DATA)) {
    fprintf(stderr, "error: invalid arguments passed to allocPVA\n");
    return;
  }
  pva->numPVs = PVs;
  pva->prevNumPVs = 0;
  pva->pvaData = (PVA_DATA_ALL_READINGS *)malloc(sizeof(PVA_DATA_ALL_READINGS) * pva->numPVs);
  if (repeats < 2) {
    for (j = 0; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
      pva->pvaData[j].getData[0].values = NULL;
      pva->pvaData[j].getData[0].stringValues = NULL;
    }
  } else {
    for (j = 0; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA) * repeats);
      for (i = 0; i < repeats; i++) {
        pva->pvaData[j].getData[i].values = NULL;
        pva->pvaData[j].getData[i].stringValues = NULL;
      }
    }
  }
  for (j = 0; j < pva->numPVs; j++) {
    pva->pvaData[j].putData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].monitorData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].putData[0].values = NULL;
    pva->pvaData[j].putData[0].stringValues = NULL;
    pva->pvaData[j].monitorData[0].values = NULL;
    pva->pvaData[j].monitorData[0].stringValues = NULL;
  }
  for (j = 0; j < pva->numPVs; j++) {
    pva->pvaData[j].numGetElements = 0;
    pva->pvaData[j].numPutElements = 0;
    pva->pvaData[j].numMonitorElements = 0;
    pva->pvaData[j].numGetReadings = 0;
    pva->pvaData[j].numGetReadingsAllocated = repeats < 2 ? 1 : repeats;
    pva->pvaData[j].numMonitorReadings = 0; // Don't expect this to ever be greater than 1
    pva->pvaData[j].monitorGeneration = 0;
    pva->pvaData[j].numeric = false;
    pva->pvaData[j].nonnumeric = false;
    pva->pvaData[j].pvEnumeratedStructure = false;
    pva->pvaData[j].haveGetPtr = false;
    pva->pvaData[j].havePutPtr = false;
    pva->pvaData[j].haveMonitorPtr = false;
    pva->pvaData[j].putPrepared = false;
    pva->pvaData[j].units = NULL;
    pva->pvaData[j].displayLimitLow = 0.0;
    pva->pvaData[j].displayLimitHigh = 0.0;
    pva->pvaData[j].controlLimitLow = 0.0;
    pva->pvaData[j].controlLimitHigh = 0.0;
    pva->pvaData[j].displayPrecision = -1;
    pva->pvaData[j].hasDisplayLimits = false;
    pva->pvaData[j].hasControlLimits = false;
    pva->pvaData[j].hasPrecision = false;
    pva->pvaData[j].alarmSeverity = 0;
    pva->pvaData[j].L1Ptr = j;
    pva->pvaData[j].L2Ptr = j;
    pva->pvaData[j].skip = false;
    pva->pvaData[j].monitorOpaqueVector = NULL;
  }
  pva->numNotConnected = PVs;
  pva->limitGetReadings = false;
  pva->useStateChangeCallbacks = false;
  pva->useGetCallbacks = false;
  pva->useMonitorCallbacks = false;
  pva->usePutCallbacks = false;
  pva->includeAlarmSeverity = false;

  pva->numMultiChannels = 1;
  pva->pvaClientMultiChannelPtr.resize(pva->numMultiChannels);
  pva->pvaClientChannelArray.clear();
  pva->pvaClientGetOwner.clear();
  pva->pvaClientGetRequest.clear();

  pva->pvaClientGetPtr.resize(pva->numPVs);
  pva->pvaClientPutPtr.resize(pva->numPVs);
  pva->pvaClientMonitorPtr.resize(pva->numPVs);

  return;
}

void reallocPVA(PVA_OVERALL *pva, long PVs) {
  reallocPVA(pva, PVs, 0);
}

void reallocPVA(PVA_OVERALL *pva, long PVs, long repeats) {
  long i, j;
  if (pva == NULL || PVs < pva->numPVs || repeats < 0 ||
      (size_t)PVs > SIZE_MAX / sizeof(PVA_DATA_ALL_READINGS) ||
      (size_t)repeats > SIZE_MAX / sizeof(PVA_DATA)) {
    fprintf(stderr, "error: reallocPVA only supports growing a valid PVA allocation\n");
    return;
  }
  if (PVs == pva->numPVs)
    return;
  pva->prevNumPVs = pva->numPVs;
  PVA_DATA_ALL_READINGS *resized = (PVA_DATA_ALL_READINGS *)realloc(
    pva->pvaData, sizeof(PVA_DATA_ALL_READINGS) * PVs);
  if (resized == NULL) {
    fprintf(stderr, "error: unable to grow PVA allocation from %ld to %ld entries\n",
            pva->numPVs, PVs);
    return;
  }
  pva->pvaData = resized;
  pva->numPVs = PVs;
  pva->pvaChannelNames.resize(pva->numPVs);
  pva->pvaProvider.resize(pva->numPVs);

  if (repeats < 2) {
    for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
      pva->pvaData[j].getData[0].values = NULL;
      pva->pvaData[j].getData[0].stringValues = NULL;
    }
  } else {
    for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
      pva->pvaData[j].getData = (PVA_DATA *)malloc(sizeof(PVA_DATA) * repeats);
      for (i = 0; i < repeats; i++) {
        pva->pvaData[j].getData[i].values = NULL;
        pva->pvaData[j].getData[i].stringValues = NULL;
      }
    }
  }
  for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
    pva->pvaData[j].putData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].monitorData = (PVA_DATA *)malloc(sizeof(PVA_DATA));
    pva->pvaData[j].putData[0].values = NULL;
    pva->pvaData[j].putData[0].stringValues = NULL;
    pva->pvaData[j].monitorData[0].values = NULL;
    pva->pvaData[j].monitorData[0].stringValues = NULL;
  }
  for (j = pva->prevNumPVs; j < pva->numPVs; j++) {
    pva->pvaData[j].numGetElements = 0;
    pva->pvaData[j].numPutElements = 0;
    pva->pvaData[j].numMonitorElements = 0;
    pva->pvaData[j].numGetReadings = 0;
    pva->pvaData[j].numGetReadingsAllocated = repeats < 2 ? 1 : repeats;
    pva->pvaData[j].numMonitorReadings = 0; // Don't expect this to ever be greater than 1
    pva->pvaData[j].monitorGeneration = 0;
    pva->pvaData[j].numeric = false;
    pva->pvaData[j].nonnumeric = false;
    pva->pvaData[j].pvEnumeratedStructure = false;
    pva->pvaData[j].haveGetPtr = false;
    pva->pvaData[j].havePutPtr = false;
    pva->pvaData[j].haveMonitorPtr = false;
    pva->pvaData[j].putPrepared = false;
    pva->pvaData[j].units = NULL;
    pva->pvaData[j].displayLimitLow = 0.0;
    pva->pvaData[j].displayLimitHigh = 0.0;
    pva->pvaData[j].controlLimitLow = 0.0;
    pva->pvaData[j].controlLimitHigh = 0.0;
    pva->pvaData[j].displayPrecision = -1;
    pva->pvaData[j].hasDisplayLimits = false;
    pva->pvaData[j].hasControlLimits = false;
    pva->pvaData[j].hasPrecision = false;
    pva->pvaData[j].alarmSeverity = 0;
    pva->pvaData[j].L1Ptr = j;
    pva->pvaData[j].L2Ptr = j;
    pva->pvaData[j].skip = false;
    pva->pvaData[j].monitorOpaqueVector = NULL;
  }
  pva->numNotConnected += pva->numPVs - pva->prevNumPVs;

  pva->numMultiChannels++;
  pva->pvaClientMultiChannelPtr.resize(pva->numMultiChannels);

  pva->pvaClientGetPtr.resize(pva->numPVs);
  pva->pvaClientPutPtr.resize(pva->numPVs);
  pva->pvaClientMonitorPtr.resize(pva->numPVs);

  return;
}

/*
  Free memory for the pva structure.
*/
void freePVA(PVA_OVERALL *pva) {
  long i, j, k;

  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    //get variables
    for (j = 0; j < pva->pvaData[i].numGetReadings; j++) {
      if (pva->pvaData[i].getData[j].values) {
        free(pva->pvaData[i].getData[j].values);
      }
      if (pva->pvaData[i].getData[j].stringValues) {
        for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
          if (pva->pvaData[i].getData[j].stringValues[k])
            free(pva->pvaData[i].getData[j].stringValues[k]);
        }
        free(pva->pvaData[i].getData[j].stringValues);
      }
    }
    //monitor variables
    if (pva->pvaData[i].monitorOpaqueVector) {
      delete (epics::pvData::shared_vector<const double> *)
        pva->pvaData[i].monitorOpaqueVector;
      pva->pvaData[i].monitorOpaqueVector = NULL;
      pva->pvaData[i].monitorData[0].values = NULL;
    } else if (pva->pvaData[i].monitorData[0].values) {
      free(pva->pvaData[i].monitorData[0].values);
    }
    if (pva->pvaData[i].monitorData[0].stringValues) {
      for (k = 0; k < pva->pvaData[i].numMonitorElements; k++) {
        if (pva->pvaData[i].monitorData[0].stringValues[k])
          free(pva->pvaData[i].monitorData[0].stringValues[k]);
      }
      free(pva->pvaData[i].monitorData[0].stringValues);
    }
    //put variables
    if (pva->pvaData[i].putData[0].values) {
      free(pva->pvaData[i].putData[0].values);
    }
    if (pva->pvaData[i].putData[0].stringValues) {
      /* PutPVAValues frees submitted strings and resets numPutElements.  Any
         strings still counted here were prepared but never submitted. */
      for (k = 0; k < pva->pvaData[i].numPutElements; k++) {
        if (pva->pvaData[i].putData[0].stringValues[k])
          free(pva->pvaData[i].putData[0].stringValues[k]);
      }
      free(pva->pvaData[i].putData[0].stringValues);
    }
    //pva client pointers
    pva->pvaClientGetPtr[i].reset();
    pva->pvaClientPutPtr[i].reset();
    pva->pvaClientMonitorPtr[i].reset();

    free(pva->pvaData[i].getData);
    free(pva->pvaData[i].putData);
    free(pva->pvaData[i].monitorData);
    if (pva->pvaData[i].units) {
      free(pva->pvaData[i].units);
    }
  }
  free(pva->pvaData);
  pva->pvaData = NULL;
  pva->pvaClientGetPtr.clear();
  pva->pvaClientPutPtr.clear();
  pva->pvaClientMonitorPtr.clear();
  pva->pvaClientMultiChannelPtr.clear();
  pva->pvaClientChannelArray.clear();
  pva->pvaClientGetOwner.clear();
  pva->pvaClientGetRequest.clear();
  pva->pvaClientPtr.reset();
  pva->numPVs = 0;
  pva->numInternalPVs = 0;
  pva->numMultiChannels = 0;

  return;
}

/*
  Free the "get" readings
*/
void freePVAGetReadings(PVA_OVERALL *pva) {
  long i, j, k;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    for (j = 0; j < pva->pvaData[i].numGetReadings; j++) {
      if (pva->limitGetReadings == false) {
        if (pva->pvaData[i].getData[j].values) {
          free(pva->pvaData[i].getData[j].values);
          pva->pvaData[i].getData[j].values = NULL;
        }
      }
      if (pva->pvaData[i].getData[j].stringValues) {
        for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
          if (pva->pvaData[i].getData[j].stringValues[k]) {
            free(pva->pvaData[i].getData[j].stringValues[k]);
            pva->pvaData[i].getData[j].stringValues[k] = NULL;
          }
        }
        if (pva->limitGetReadings == false) {
          free(pva->pvaData[i].getData[j].stringValues);
          pva->pvaData[i].getData[j].stringValues = NULL;
        }
      }
    }
    if (pva->limitGetReadings == false) {
      pva->pvaData[i].numGetReadings = 0;
    }
  }
  return;
}

/*
  Free the "monitor" readings
*/
void freePVAMonitorReadings(PVA_OVERALL *pva) {
  long i, k;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].monitorOpaqueVector) {
      delete (epics::pvData::shared_vector<const double> *)
        pva->pvaData[i].monitorOpaqueVector;
      pva->pvaData[i].monitorOpaqueVector = NULL;
      pva->pvaData[i].monitorData[0].values = NULL;
    } else if (pva->pvaData[i].monitorData[0].values) {
      free(pva->pvaData[i].monitorData[0].values);
      pva->pvaData[i].monitorData[0].values = NULL;
    }
    if (pva->pvaData[i].monitorData[0].stringValues) {
      for (k = 0; k < pva->pvaData[i].numMonitorElements; k++) {
        if (pva->pvaData[i].monitorData[0].stringValues[k])
          free(pva->pvaData[i].monitorData[0].stringValues[k]);
      }
      free(pva->pvaData[i].monitorData[0].stringValues);
      pva->pvaData[i].monitorData[0].stringValues = NULL;
    }
    pva->pvaData[i].numMonitorReadings = 0;
  }
  return;
}

/*
  Connect to the PVs using PvaClientMultiChannel
*/
void ConnectPVA(PVA_OVERALL *pva, double pendIOTime) {
  if (pva == NULL || pva->pvaData == NULL || pva->numPVs <= 0 ||
      pva->pvaChannelNames.size() < (size_t)pva->numPVs ||
      pva->pvaProvider.size() < (size_t)pva->numPVs) {
    fprintf(stderr, "error: invalid or incomplete PVA state passed to ConnectPVA\n");
    return;
  }

  long i, j, num = 0, numInternalPVs;
  size_t pos;
  epics::pvData::shared_vector<std::string> namesTmp(pva->numPVs);
  epics::pvData::shared_vector<std::string> subnames(pva->numPVs);
  epics::pvData::Status status;
  epics::pvaClient::PvaClientChannelArray newChannels;
  epics::pvData::shared_vector<epics::pvData::boolean> connected(pva->numPVs);
  std::map<std::pair<std::string, std::string>, long> channels;

  i = 0;
  for (j = 0; j < pva->numPVs; j++) {
    if (pva->pvaProvider[j].compare("pva") == 0) {
      pos = pva->pvaChannelNames[j].find('.');
      if (pos == std::string::npos) {
        namesTmp[j] = pva->pvaChannelNames[j];
        subnames[j] = "";
      } else {
        namesTmp[j] = pva->pvaChannelNames[j].substr(0, pos);
        subnames[j] = pva->pvaChannelNames[j].substr(pos + 1);
        /* If the user requests an indexed array element (e.g. dimension[0].size),
           request the unindexed field over the network and apply indexing client-side. */
        pos = subnames[j].find_first_of("[(@");
        if (pos != std::string::npos) {
          subnames[j] = subnames[j].substr(0, pos);
        }
      }
    } else {
      namesTmp[j] = pva->pvaChannelNames[j];
      subnames[j] = "";
    }
    std::pair<std::string, std::string> channelKey(pva->pvaProvider[j], namesTmp[j]);
    std::pair<std::map<std::pair<std::string, std::string>, long>::iterator, bool> inserted =
      channels.insert(std::make_pair(channelKey, j));
    if (inserted.second) {
      pva->pvaData[j].L1Ptr = j;
      pva->pvaData[j].L2Ptr = i;
      i++;
    } else {
      pva->pvaData[j].L1Ptr = inserted.first->second;
      pva->pvaData[j].L2Ptr = pva->pvaData[pva->pvaData[j].L1Ptr].L2Ptr;
    }
  }

  if (pva->numMultiChannels == 1) {
    pva->numInternalPVs = numInternalPVs = i;
    epics::pvData::shared_vector<std::string> names(pva->numInternalPVs);
    epics::pvData::shared_vector<std::string> provider(pva->numInternalPVs);
    epics::pvData::shared_vector<const std::string> constProvider;
    
    for (j = 0; j < pva->numPVs; j++) {
      names[pva->pvaData[j].L2Ptr] = namesTmp[j];
      provider[pva->pvaData[j].L2Ptr] = pva->pvaProvider[j];
    }
    pva->pvaChannelNamesTop = freeze(names);
    pva->pvaChannelNamesSub = freeze(subnames);
    constProvider = freeze(provider);
    //Connect to PVs all at once
    pva->pvaClientPtr = epics::pvaClient::PvaClient::get("pva ca");
    //pva->pvaClientPtr->setDebug(true);
    pva->pvaClientMultiChannelPtr[0] = epics::pvaClient::PvaClientMultiChannel::create(pva->pvaClientPtr, pva->pvaChannelNamesTop, "pva", numInternalPVs, constProvider);
    status = pva->pvaClientMultiChannelPtr[0]->connect(pendIOTime);

    newChannels = pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray();
    pva->pvaClientChannelArray = newChannels;
    pva->pvaClientGetOwner.assign(pva->numInternalPVs, -1);
    pva->pvaClientGetRequest.assign(pva->numInternalPVs, "");
  } else {
    //This will execute if we are adding additional PVs. It is sort of a hack
    pva->prevNumInternalPVs = pva->numInternalPVs;
    pva->numInternalPVs = i;
    numInternalPVs = pva->numInternalPVs - pva->prevNumInternalPVs;
    epics::pvData::shared_vector<std::string> names(pva->numInternalPVs);
    epics::pvData::shared_vector<std::string> newnames(numInternalPVs);
    epics::pvData::shared_vector<std::string> provider(numInternalPVs);
    epics::pvData::shared_vector<const std::string> constNames;
    
    epics::pvData::shared_vector<const std::string> constProvider;
    
    for (j = 0; j < pva->numPVs; j++) {
      names[pva->pvaData[j].L2Ptr] = namesTmp[j];
      if (pva->pvaData[j].L2Ptr >= pva->prevNumInternalPVs) {
        newnames[pva->pvaData[j].L2Ptr - pva->prevNumInternalPVs] = namesTmp[j];
        provider[pva->pvaData[j].L2Ptr - pva->prevNumInternalPVs] = pva->pvaProvider[j];
      }
    }
    pva->pvaChannelNamesTop = freeze(names);
    pva->pvaChannelNamesSub = freeze(subnames);
    constNames = freeze(newnames);
    constProvider = freeze(provider);

    if (numInternalPVs > 0) {
      pva->pvaClientMultiChannelPtr[pva->numMultiChannels - 1] = epics::pvaClient::PvaClientMultiChannel::create(pva->pvaClientPtr, constNames, "pva", numInternalPVs, constProvider);
      status = pva->pvaClientMultiChannelPtr[pva->numMultiChannels - 1]->connect(pendIOTime);

      newChannels = pva->pvaClientMultiChannelPtr[pva->numMultiChannels - 1]->getPvaClientChannelArray();
      size_t oldChannelCount = pva->pvaClientChannelArray.size();
      pva->pvaClientChannelArray.resize(oldChannelCount + newChannels.size());
      std::copy(newChannels.begin(), newChannels.end(), pva->pvaClientChannelArray.begin() + oldChannelCount);
    } else {
      /* reallocPVA may add only another field of an existing top-level PVA.
         In that case there is no new channel group to create. */
      pva->numMultiChannels--;
      pva->pvaClientMultiChannelPtr.resize(pva->numMultiChannels);
    }
    pva->pvaClientGetOwner.resize(pva->numInternalPVs, -1);
    pva->pvaClientGetRequest.resize(pva->numInternalPVs);
  }

  if (!RefreshConnectionState(pva)) {
    pva->isInternalConnected.resize(pva->numInternalPVs);
    std::fill(pva->isInternalConnected.begin(), pva->isInternalConnected.end(), false);
  }

  for (j = 0; j < pva->numPVs; j++) {
    connected[j] = pva->isInternalConnected[pva->pvaData[j].L2Ptr];
    if (connected[j] == false) {
      num++;
    }
  }
  pva->isConnected = connected;
  pva->numNotConnected = num;
  for (j = 0; j < numInternalPVs && (size_t)j < newChannels.size(); j++) {
    if (pva->useStateChangeCallbacks) {
      newChannels[j]->setStateChangeRequester((epics::pvaClient::PvaClientChannelStateChangeRequesterPtr)pva->stateChangeReqPtr);
    }
  }
}

/*
  Read the PV values over the network and place the values in the pva structure.
*/
long GetPVAValues(PVA_OVERALL *pva) {
  if (pva == NULL)
    return (1);
  PVA_OVERALL *pvaArray[] = {pva};
  return GetPVAValues(pvaArray, 1);
}

long GetPVAValuesOld(PVA_OVERALL **pva, long count) {
  long i, num = 0, n;
  epics::pvData::Status status;

  for (n = 0; n < count; n++) {
    if (pva[n] != NULL) {
      if (!RefreshConnectionState(pva[n])) {
        pva[n]->numNotConnected = pva[n]->numPVs;
        continue;
      }
      const epics::pvaClient::PvaClientChannelArray &pvaClientChannelArray = pva[n]->pvaClientChannelArray;
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        pva[n]->isConnected[i] = pva[n]->isInternalConnected[pva[n]->pvaData[i].L2Ptr];
        if (pva[n]->isConnected[i]) {
          if (pva[n]->pvaData[i].haveGetPtr == false) {
            pva[n]->pvaClientGetPtr[i] = pvaClientChannelArray[pva[n]->pvaData[i].L2Ptr]->createGet(pva[n]->pvaChannelNamesSub[i]);
            pva[n]->pvaData[i].haveGetPtr = true;
            if (pva[n]->useGetCallbacks) {
              pva[n]->pvaClientGetPtr[i]->setRequester((epics::pvaClient::PvaClientGetRequesterPtr)pva[n]->getReqPtr);
            }
          }
          try {
            pva[n]->pvaClientGetPtr[i]->issueGet();
          } catch (std::exception &e) {
            num++;
            pva[n]->isConnected[i] = false;
            //std::cerr << "Error: invalid sub-field name: " + pva[n]->pvaChannelNamesSub[i] + "\n";
            //return 1;
          }
        } else {
          num++;
        }
      }
      pva[n]->numNotConnected = num;
    }
  }

  for (n = 0; n < count; n++) {
    if ((pva[n] != NULL) && (pva[n]->useGetCallbacks == false)) {
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        if (pva[n]->isConnected[i]) {
          status = pva[n]->pvaClientGetPtr[i]->waitGet();
          if (!status.isSuccess()) {
            fprintf(stderr, "error: %s did not respond to the \"get\" request\n", pva[n]->pvaChannelNames[i].c_str());
            pva[n]->isConnected[i] = false;
            pva[n]->numNotConnected++;
            //return (1);
          }
        }
      }
    }
  }
  for (n = 0; n < count; n++) {
    if ((pva[n] != NULL) && (pva[n]->useGetCallbacks == false)) {
      if (ExtractPVAValues(pva[n]) == 1) {
        return (1);
      }
    }
  }
  return (0);
}

struct RequestFieldNode {
  RequestFieldNode() : wholeField(false) {
  }
  bool wholeField;
  std::map<std::string, RequestFieldNode> children;
};

static void AppendRequestFields(std::ostringstream &result,
                                const std::map<std::string, RequestFieldNode> &fields) {
  bool first = true;
  for (std::map<std::string, RequestFieldNode>::const_iterator it = fields.begin();
       it != fields.end(); ++it) {
    if (!first)
      result << ',';
    first = false;
    result << it->first;
    if (!it->second.wholeField && !it->second.children.empty()) {
      result << '{';
      AppendRequestFields(result, it->second.children);
      result << '}';
    }
  }
}

std::string convertToProperRequestFormat(const std::vector<std::string>& input) {
  RequestFieldNode root;

  for (std::vector<std::string>::const_iterator pathIt = input.begin();
       pathIt != input.end(); ++pathIt) {
    if (pathIt->empty()) {
      /* An empty request selects the complete top-level structure. */
      root.wholeField = true;
      root.children.clear();
      break;
    }

    RequestFieldNode *node = &root;
    size_t start = 0;
    bool valid = true;
    while (start <= pathIt->size()) {
      size_t dot = pathIt->find('.', start);
      std::string field = pathIt->substr(start, dot == std::string::npos ?
                                               std::string::npos : dot - start);
      if (field.empty()) {
        valid = false;
        break;
      }
      if (node->wholeField)
        break;
      node = &node->children[field];
      if (dot == std::string::npos) {
        node->wholeField = true;
        node->children.clear();
        break;
      }
      start = dot + 1;
    }
    if (!valid) {
      /* Let createGet report malformed user input instead of synthesizing a
         different valid request. */
      return *pathIt;
    }
  }

  if (root.wholeField)
    return "";
  std::ostringstream result;
  AppendRequestFields(result, root.children);
  return result.str();
}

long GetPVAValues(PVA_OVERALL **pva, long count) {
  long i, n;
  epics::pvData::Status status;

  if (pva == NULL || count < 0)
    return (1);

  for (n = 0; n < count; n++) {
    if (pva[n] != NULL) {
      long numNotConnected = 0;
      if (!RefreshConnectionState(pva[n])) {
        fprintf(stderr, "error: PVA channels have not been connected\n");
        return (1);
      }
      if (!RefreshSharedGetRequests(pva[n])) {
        fprintf(stderr, "error: invalid PVA shared-get state\n");
        return (1);
      }
      const epics::pvaClient::PvaClientChannelArray &pvaClientChannelArray = pva[n]->pvaClientChannelArray;
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        pva[n]->isConnected[i] = pva[n]->isInternalConnected[pva[n]->pvaData[i].L2Ptr];
        if (pva[n]->isConnected[i]) {
          long internalIndex = pva[n]->pvaData[i].L2Ptr;
          try {
            if (pva[n]->pvaProvider[i].compare("pva") != 0) {
              // CA PVs
              if (pva[n]->pvaData[i].haveGetPtr == false) {
                pva[n]->pvaClientGetPtr[i] = pvaClientChannelArray[internalIndex]->createGet(pva[n]->pvaChannelNamesSub[i]);
                pva[n]->pvaData[i].haveGetPtr = true;
                if (pva[n]->useGetCallbacks) {
                  pva[n]->pvaClientGetPtr[i]->setRequester((epics::pvaClient::PvaClientGetRequesterPtr)pva[n]->getReqPtr);
                }
              }
            } else {
              // PVA fields on one top-level channel share a single network get.
              long owner = pva[n]->pvaClientGetOwner[internalIndex];
              if (owner >= 0 && (owner >= pva[n]->numPVs || pva[n]->pvaData[owner].skip ||
                                 !pva[n]->pvaClientGetPtr[owner])) {
                owner = -1;
                pva[n]->pvaClientGetOwner[internalIndex] = -1;
              }
              if (owner < 0) {
                pva[n]->pvaClientGetPtr[i] = pvaClientChannelArray[internalIndex]->createGet(pva[n]->pvaClientGetRequest[internalIndex]);
                pva[n]->pvaClientGetOwner[internalIndex] = i;
                pva[n]->pvaData[i].haveGetPtr = true;
                if (pva[n]->useGetCallbacks) {
                  pva[n]->pvaClientGetPtr[i]->setRequester((epics::pvaClient::PvaClientGetRequesterPtr)pva[n]->getReqPtr);
                }
              } else if (i != owner) {
                pva[n]->pvaClientGetPtr[i] = pva[n]->pvaClientGetPtr[owner];
                pva[n]->pvaData[i].haveGetPtr = true;
              }
            }

            bool ownsGetRequest = pva[n]->pvaProvider[i].compare("pva") != 0 ||
                                  pva[n]->pvaClientGetOwner[internalIndex] == i;
            if (ownsGetRequest) {
              pva[n]->pvaClientGetPtr[i]->issueGet();
            }
          } catch (std::exception &e) {
            if (pva[n]->pvaProvider[i].compare("pva") == 0) {
              MarkInternalDisconnected(pva[n], internalIndex);
            } else {
              pva[n]->isConnected[i] = false;
            }
          }
        }
      }
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (!pva[n]->pvaData[i].skip && !pva[n]->isConnected[i])
          numNotConnected++;
      }
      pva[n]->numNotConnected = numNotConnected;
    }
  }
  for (n = 0; n < count; n++) {
    if ((pva[n] != NULL) && (pva[n]->useGetCallbacks == false)) {
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        bool ownsGetRequest = pva[n]->pvaProvider[i].compare("pva") != 0 ||
                              pva[n]->pvaClientGetOwner[pva[n]->pvaData[i].L2Ptr] == i;
        if (pva[n]->isConnected[i] && ownsGetRequest) {
          bool getFailed = false;
          try {
            status = pva[n]->pvaClientGetPtr[i]->waitGet();
          } catch (std::exception &e) {
            getFailed = true;
          }
          if (getFailed || !status.isSuccess()) {
            fprintf(stderr, "error: %s did not respond to the \"get\" request\n", pva[n]->pvaChannelNames[i].c_str());
            if (pva[n]->pvaProvider[i].compare("pva") == 0)
              MarkInternalDisconnected(pva[n], pva[n]->pvaData[i].L2Ptr);
            else
              pva[n]->isConnected[i] = false;
          }
        }
      }
      pva[n]->numNotConnected = 0;
      for (i = 0; i < pva[n]->numPVs; i++) {
        if (!pva[n]->pvaData[i].skip && !pva[n]->isConnected[i])
          pva[n]->numNotConnected++;
      }
    }
  }
  for (n = 0; n < count; n++) {
    if ((pva[n] != NULL) && (pva[n]->useGetCallbacks == false)) {
      if (ExtractPVAValues(pva[n]) == 1) {
        return (1);
      }
    }
  }
  return (0);
}

long ExtractScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  epics::pvData::ScalarConstPtr scalarConstPtr;
  epics::pvData::PVScalarPtr pvScalarPtr;
  long i = 0;
  scalarConstPtr = std::tr1::static_pointer_cast<const epics::pvData::Scalar>(PVFieldPtr->getField());
  pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtr);

  if (monitorMode) {
    i = 0;
    if (pva->pvaData[index].numMonitorReadings == 0) {
      pva->pvaData[index].fieldType = scalarConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarConstPtr->getScalarType();
      pva->pvaData[index].numMonitorElements = 1;
    } else {
      if (pva->pvaData[index].nonnumeric) {
        if (pva->pvaData[index].monitorData[0].stringValues[0])
          free(pva->pvaData[index].monitorData[0].stringValues[0]);
      }
    }
  } else {
    i = pva->pvaData[index].numGetReadings;
    if (pva->pvaData[index].numGetReadings == 0) {
      pva->pvaData[index].fieldType = scalarConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarConstPtr->getScalarType();
      pva->pvaData[index].numGetElements = 1;
    } else if (pva->limitGetReadings) {
      i = 0;
    }
    if (!EnsureGetReadingCapacity(pva, index, i + 1)) {
      std::cerr << "ERROR: unable to allocate storage for another PVA reading" << std::endl;
      return (1);
    }
  }
  switch (pva->pvaData[index].scalarType) {
  case epics::pvData::pvDouble:
  case epics::pvData::pvFloat:
  case epics::pvData::pvLong:
  case epics::pvData::pvULong:
  case epics::pvData::pvInt:
  case epics::pvData::pvUInt:
  case epics::pvData::pvShort:
  case epics::pvData::pvUShort:
  case epics::pvData::pvByte:
  case epics::pvData::pvUByte: {
    if (monitorMode) {
      if (pva->pvaData[index].monitorData[0].values == NULL) {
        pva->pvaData[index].monitorData[0].values = (double *)malloc(sizeof(double));
        pva->pvaData[index].numeric = true;
      }
      pva->pvaData[index].monitorData[0].values[0] = pvScalarPtr->getAs<double>();
    } else {
      if (pva->pvaData[index].getData[i].values == NULL) {
        pva->pvaData[index].getData[i].values = (double *)malloc(sizeof(double));
        pva->pvaData[index].numeric = true;
      }
      pva->pvaData[index].getData[i].values[0] = pvScalarPtr->getAs<double>();
    }
    break;
  }
  case epics::pvData::pvString:
  case epics::pvData::pvBoolean: {
    std::string s = pvScalarPtr->getAs<std::string>();
    if (monitorMode) {
      if (pva->pvaData[index].monitorData[0].stringValues == NULL) {
        pva->pvaData[index].monitorData[0].stringValues = (char **)malloc(sizeof(char *) * 1);
      }
      pva->pvaData[index].monitorData[0].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
      strcpy(pva->pvaData[index].monitorData[0].stringValues[0], s.c_str());
      if (pva->pvaData[index].numMonitorReadings == 0) {
        pva->pvaData[index].nonnumeric = true;
      }
    } else {
      if (pva->pvaData[index].getData[i].stringValues == NULL) {
        pva->pvaData[index].getData[i].stringValues = (char **)malloc(sizeof(char *) * 1);
        pva->pvaData[index].getData[i].stringValues[0] = NULL;
      } else if (pva->pvaData[index].getData[i].stringValues[0]) {
        free(pva->pvaData[index].getData[i].stringValues[0]);
        pva->pvaData[index].getData[i].stringValues[0] = NULL;
      }
      pva->pvaData[index].getData[i].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
      strcpy(pva->pvaData[index].getData[i].stringValues[0], s.c_str());
      if (pva->pvaData[index].numGetReadings == 0) {
        pva->pvaData[index].nonnumeric = true;
      }
    }
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle scalar type " << pva->pvaData[index].scalarType << std::endl;
    return (1);
  }
  }
  if (monitorMode) {
    pva->pvaData[index].numMonitorReadings = 1;
    pva->pvaData[index].monitorGeneration++;
  } else {
    if (pva->limitGetReadings) {
      pva->pvaData[index].numGetReadings = 1;
    } else {
      pva->pvaData[index].numGetReadings++;
    }
  }
  return (0);
}

long ExtractNTScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (ExtractScalarValue(pva, index, PVFieldPtrArray[j], monitorMode)) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  epics::pvData::ScalarArrayConstPtr scalarArrayConstPtr;
  epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;
  long i = 0;
  long previousElementCount = 0;
  long currentElementCount;
  bool replacingExisting = false;
  scalarArrayConstPtr = std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(PVFieldPtr->getField());
  pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(PVFieldPtr);
  currentElementCount = GetElementCountFromNelm(pva, index, pvScalarArrayPtr->getLength());

  if (monitorMode) {
    i = 0;
    previousElementCount = pva->pvaData[index].numMonitorElements;
    replacingExisting = pva->pvaData[index].numMonitorReadings != 0;
    if (pva->pvaData[index].numMonitorReadings == 0) {
      pva->pvaData[index].fieldType = scalarArrayConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarArrayConstPtr->getElementType();
    }
    /* Monitor payload sizes can change, including from an initially empty
       array.  Keep both the reported length and the reusable buffer current. */
    pva->pvaData[index].numMonitorElements = currentElementCount;
  } else {
    i = pva->pvaData[index].numGetReadings;
    previousElementCount = pva->pvaData[index].numGetElements;
    if (pva->pvaData[index].numGetReadings == 0) {
      pva->pvaData[index].fieldType = scalarArrayConstPtr->getType(); //should always be epics::pvData::scalar
      pva->pvaData[index].scalarType = scalarArrayConstPtr->getElementType();
      pva->pvaData[index].numGetElements = currentElementCount;
    } else if (pva->limitGetReadings) {
      i = 0;
      replacingExisting = true;
      /* limitGetReadings reuses slot zero, so its buffer must follow a
         dynamically changing waveform length. */
      pva->pvaData[index].numGetElements = currentElementCount;
    } else {
      /* Accumulated readings share one element-count field.  Preserve the
         first reading's shape and continue to pad or truncate later samples. */
      currentElementCount = pva->pvaData[index].numGetElements;
    }
    if (!EnsureGetReadingCapacity(pva, index, i + 1)) {
      std::cerr << "ERROR: unable to allocate storage for another PVA reading" << std::endl;
      return (1);
    }
  }
  switch (pva->pvaData[index].scalarType) {
  case epics::pvData::pvDouble:
  case epics::pvData::pvFloat:
  case epics::pvData::pvLong:
  case epics::pvData::pvULong:
  case epics::pvData::pvInt:
  case epics::pvData::pvUInt:
  case epics::pvData::pvShort:
  case epics::pvData::pvUShort:
  case epics::pvData::pvByte:
  case epics::pvData::pvUByte: {
    epics::pvData::PVDoubleArray::const_svector dataVector;
    pvScalarArrayPtr->PVScalarArray::getAs<double>(dataVector);
    if (monitorMode) {
      if (pva->pvaData[index].monitorOpaqueVector) {
        delete (epics::pvData::shared_vector<const double> *)
          pva->pvaData[index].monitorOpaqueVector;
      } else if (pva->pvaData[index].monitorData[0].values) {
        free(pva->pvaData[index].monitorData[0].values);
      }
      epics::pvData::shared_vector<const double> *newVector =
        new epics::pvData::shared_vector<const double>(dataVector);
      pva->pvaData[index].monitorOpaqueVector = newVector;
      pva->pvaData[index].monitorData[0].values =
        const_cast<double *>(newVector->data());
      pva->pvaData[index].numeric = true;
      pva->pvaData[index].numMonitorElements =
        static_cast<long>(newVector->size());
    } else {
      if (pva->pvaData[index].getData[i].values == NULL ||
          (replacingExisting && previousElementCount != currentElementCount)) {
        if (!ResizeNumericValues(&pva->pvaData[index].getData[i].values, currentElementCount)) {
          pva->pvaData[index].numGetElements = previousElementCount;
          std::cerr << "ERROR: unable to resize PVA get array storage" << std::endl;
          return (1);
        }
      }
      pva->pvaData[index].numeric = true;
      long count = pva->pvaData[index].numGetElements;
      long have = dataVector.size();
      long copyCount = (count < have ? count : have);
      if (copyCount > 0)
        std::copy(dataVector.begin(), dataVector.begin() + copyCount, pva->pvaData[index].getData[i].values);
      for (long k = copyCount; k < count; k++) {
        pva->pvaData[index].getData[i].values[k] = 0;
      }
    }
    break;
  }
  case epics::pvData::pvString:
  case epics::pvData::pvBoolean: {
    epics::pvData::PVStringArray::const_svector dataVector;
    pvScalarArrayPtr->PVScalarArray::getAs<std::string>(dataVector);
    if (monitorMode) {
      if (!ResetStringValues(&pva->pvaData[index].monitorData[0].stringValues,
                             replacingExisting ? previousElementCount : 0,
                             currentElementCount)) {
        pva->pvaData[index].numMonitorElements = previousElementCount;
        std::cerr << "ERROR: unable to resize PVA monitor string-array storage" << std::endl;
        return (1);
      }
      pva->pvaData[index].nonnumeric = true;
      long count = pva->pvaData[index].numMonitorElements;
      long have = dataVector.size();
      long copyCount = (count < have ? count : have);
      for (long k = 0; k < copyCount; k++) {
        pva->pvaData[index].monitorData[0].stringValues[k] = (char *)malloc(sizeof(char) * (dataVector[k].length() + 1));
        strcpy(pva->pvaData[index].monitorData[0].stringValues[k], dataVector[k].c_str());
      }
      for (long k = copyCount; k < count; k++) {
        pva->pvaData[index].monitorData[0].stringValues[k] = (char *)malloc(sizeof(char));
        pva->pvaData[index].monitorData[0].stringValues[k][0] = 0;
      }
    } else {
      if (!ResetStringValues(&pva->pvaData[index].getData[i].stringValues,
                             replacingExisting ? previousElementCount : 0,
                             currentElementCount)) {
        pva->pvaData[index].numGetElements = previousElementCount;
        std::cerr << "ERROR: unable to resize PVA get string-array storage" << std::endl;
        return (1);
      }
      pva->pvaData[index].nonnumeric = true;
      long count = pva->pvaData[index].numGetElements;
      long have = dataVector.size();
      long copyCount = (count < have ? count : have);
      for (long k = 0; k < copyCount; k++) {
        pva->pvaData[index].getData[i].stringValues[k] = (char *)malloc(sizeof(char) * (dataVector[k].length() + 1));
        strcpy(pva->pvaData[index].getData[i].stringValues[k], dataVector[k].c_str());
      }
      for (long k = copyCount; k < count; k++) {
        pva->pvaData[index].getData[i].stringValues[k] = (char *)malloc(sizeof(char));
        pva->pvaData[index].getData[i].stringValues[k][0] = 0;
      }
    }
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle scalar array type " << pva->pvaData[index].scalarType << std::endl;
    return (1);
  }
  }
  if (monitorMode) {
    pva->pvaData[index].numMonitorReadings = 1;
    pva->pvaData[index].monitorGeneration++;
  } else {
    if (pva->limitGetReadings) {
      pva->pvaData[index].numGetReadings = 1;
    } else {
      pva->pvaData[index].numGetReadings++;
    }
  }
  return (0);
}

static long ExtractUnionValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  epics::pvData::PVUnionPtr pvUnionPtr;
  epics::pvData::PVFieldPtr selectedField;

  pvUnionPtr = std::tr1::static_pointer_cast<epics::pvData::PVUnion>(PVFieldPtr);
  selectedField = pvUnionPtr->get();

  if (!selectedField) {
    std::cerr << "ERROR: union has no selected field" << std::endl;
    return (1);
  }

  switch (selectedField->getField()->getType()) {
  case epics::pvData::scalar: {
    return ExtractScalarValue(pva, index, selectedField, monitorMode);
  }
  case epics::pvData::scalarArray: {
    return ExtractScalarArrayValue(pva, index, selectedField, monitorMode);
  }
  case epics::pvData::structure: {
    return ExtractStructureValue(pva, index, selectedField, monitorMode);
  }
  case epics::pvData::union_: {
    /* Nested unions are allowed; recurse. */
    return ExtractUnionValue(pva, index, selectedField, monitorMode);
  }
  default: {
    std::cerr << "ERROR: Need code to handle union selected field type " << selectedField->getField()->getType() << std::endl;
    return (1);
  }
  }
}

static bool ParseIndexedToken(const std::string &token, std::string &name, long &arrayIndex, bool &hasIndex) {
  size_t lb = token.find_first_of("[(");
  if (lb == std::string::npos) {
    size_t at = token.find('@');
    if (at == std::string::npos) {
      name = token;
      hasIndex = false;
      return true;
    }
    if (token.find('@', at + 1) != std::string::npos) {
      return false;
    }
    name = token.substr(0, at);
    std::string indexText = token.substr(at + 1);
    if (name.empty() || indexText.empty()) {
      return false;
    }
    char *endp = NULL;
    errno = 0;
    long v = strtol(indexText.c_str(), &endp, 10);
    if (errno == ERANGE || (endp == indexText.c_str()) || (endp == NULL) || (*endp != '\0')) {
      return false;
    }
    arrayIndex = v;
    hasIndex = true;
    return true;
  }

  char openCh = token[lb];
  char closeCh = (openCh == '[' ? ']' : ')');
  size_t rb = token.find(closeCh, lb + 1);
  if (rb == std::string::npos) {
    return false;
  }
  if (rb + 1 != token.size()) {
    /* Only support a single trailing [index] on the token. */
    return false;
  }

  name = token.substr(0, lb);
  std::string indexText = token.substr(lb + 1, rb - lb - 1);
  if (name.empty() || indexText.empty()) {
    return false;
  }
  char *endp = NULL;
  errno = 0;
  long v = strtol(indexText.c_str(), &endp, 10);
  if (errno == ERANGE || (endp == indexText.c_str()) || (endp == NULL) || (*endp != '\0')) {
    return false;
  }
  arrayIndex = v;
  hasIndex = true;
  return true;
}

static epics::pvData::PVFieldPtr ResolveFieldByPath(PVA_OVERALL *pva, long index,
                                                    epics::pvData::PVStructurePtr root,
                                                    const std::string &path, bool reportErrors) {
  if (!root) {
    if (reportErrors)
      std::cerr << "ERROR: NULL root structure" << std::endl;
    return epics::pvData::PVFieldPtr();
  }
  if (path.empty()) {
    if (reportErrors)
      std::cerr << "Error: sub-field is not specific enough" << std::endl;
    return epics::pvData::PVFieldPtr();
  }

  epics::pvData::PVFieldPtr current = root;
  std::string remaining = path;
  while (!remaining.empty()) {
    std::string token;
    size_t dot = remaining.find('.');
    if (dot == std::string::npos) {
      token = remaining;
      remaining.clear();
    } else {
      token = remaining.substr(0, dot);
      remaining = remaining.substr(dot + 1);
    }

    std::string fieldName;
    long arrayIndex = 0;
    bool hasIndex = false;
    if (!ParseIndexedToken(token, fieldName, arrayIndex, hasIndex)) {
      if (reportErrors)
        std::cerr << "Error: invalid indexed field syntax: " << token << std::endl;
      return epics::pvData::PVFieldPtr();
    }
    if (fieldName.empty()) {
      if (reportErrors)
        std::cerr << "Error: invalid field name in path: " << token << std::endl;
      return epics::pvData::PVFieldPtr();
    }

    if (!current || current->getField()->getType() != epics::pvData::structure) {
      if (reportErrors)
        std::cerr << "Error: path element is not a structure while resolving: " << fieldName << std::endl;
      return epics::pvData::PVFieldPtr();
    }
    epics::pvData::PVStructurePtr currentStruct = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(current);
    epics::pvData::PVFieldPtr next = currentStruct->getSubField(fieldName);
    if (!next) {
      if (reportErrors)
        std::cerr << "Error: sub-field does not exist for " << pva->pvaChannelNames[index] << std::endl;
      return epics::pvData::PVFieldPtr();
    }
    current = next;

    if (hasIndex) {
      if (arrayIndex < 0) {
        if (reportErrors)
          std::cerr << "Error: negative index in " << token << std::endl;
        return epics::pvData::PVFieldPtr();
      }
      if (current->getField()->getType() != epics::pvData::structureArray) {
        if (reportErrors)
          std::cerr << "ERROR: indexed access requires structureArray for " << token << std::endl;
        return epics::pvData::PVFieldPtr();
      }
      epics::pvData::PVStructureArrayPtr arrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVStructureArray>(current);
      epics::pvData::PVStructureArray::const_svector elements = arrayPtr->view();
      if ((size_t)arrayIndex >= elements.size()) {
        if (reportErrors)
          std::cerr << "Error: index out of range in " << token << " (have " << elements.size() << ")" << std::endl;
        return epics::pvData::PVFieldPtr();
      }
      if (!elements[(size_t)arrayIndex]) {
        if (reportErrors)
          std::cerr << "Error: NULL structure array element in " << token << std::endl;
        return epics::pvData::PVFieldPtr();
      }
      current = elements[(size_t)arrayIndex];
    }
  }
  return current;
}

static epics::pvData::PVFieldPtr GetRequestedField(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return epics::pvData::PVFieldPtr();

  epics::pvData::PVStructurePtr root = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  std::string requestedPath;
  if (GetPVARequestedPath(pva, index, requestedPath))
    return ResolveFieldByPath(pva, index, root, requestedPath, false);

  epics::pvData::PVFieldPtr value = root->getSubField("value");
  if (value)
    return value;
  epics::pvData::PVFieldPtrArray fields = root->getPVFields();
  return fields.empty() ? epics::pvData::PVFieldPtr() : fields[0];
}

static long ExtractByPath(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr root, const std::string &path, bool monitorMode) {
  epics::pvData::PVFieldPtr current = ResolveFieldByPath(pva, index, root, path, true);

  if (!current) {
    return (1);
  }
  switch (current->getField()->getType()) {
  case epics::pvData::scalar:
    return ExtractScalarValue(pva, index, current, monitorMode);
  case epics::pvData::scalarArray:
    return ExtractScalarArrayValue(pva, index, current, monitorMode);
  case epics::pvData::union_:
    return ExtractUnionValue(pva, index, current, monitorMode);
  case epics::pvData::structure: {
    std::cerr << "Error: sub-field is not specific enough" << std::endl;
    return (1);
  }
  case epics::pvData::structureArray: {
    std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
    return (1);
  }
  default:
    std::cerr << "ERROR: Need code to handle " << current->getField()->getType() << std::endl;
    return (1);
  }
}

static long PutByPath(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr root, const std::string &path) {
  epics::pvData::PVFieldPtr current = ResolveFieldByPath(pva, index, root, path, true);

  if (!current) {
    return (1);
  }
  switch (current->getField()->getType()) {
  case epics::pvData::scalar:
    return PutScalarValue(pva, index, current);
  case epics::pvData::scalarArray:
    return PutScalarArrayValue(pva, index, current);
  case epics::pvData::structure: {
    std::cerr << "Error: sub-field is not specific enough" << std::endl;
    return (1);
  }
  case epics::pvData::structureArray: {
    std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
    return (1);
  }
  default:
    std::cerr << "ERROR: Need code to handle " << current->getField()->getType() << std::endl;
    return (1);
  }
}

long ExtractNTScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (ExtractScalarArrayValue(pva, index, PVFieldPtrArray[j], monitorMode)) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractNTEnumValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode) {
  long i, j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      epics::pvData::PVStructurePtr pvStructurePtr;
      epics::pvData::PVEnumerated pvEnumerated;
      std::string s;
      pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
      if (!pvEnumerated.attach(pvStructurePtr)) {
        std::cerr << "ERROR: NTEnum value field is not a valid enumerated structure" << std::endl;
        return (1);
      }
      if (monitorMode) {
        if (pva->pvaData[index].numMonitorReadings == 0) {
          pva->pvaData[index].fieldType = pvStructurePtr->getField()->getType(); //should always be epics::pvData::structure
          pva->pvaData[index].pvEnumeratedStructure = true;
          pva->pvaData[index].numMonitorElements = 1;
          pva->pvaData[index].numeric = true;
          pva->pvaData[index].nonnumeric = true;
          pva->pvaData[index].scalarType = epics::pvData::pvString;
          pva->pvaData[index].monitorData[0].values = (double *)malloc(sizeof(double));
          pva->pvaData[index].monitorData[0].stringValues = (char **)malloc(sizeof(char *) * 1);
        } else {
          if (pva->pvaData[index].monitorData[0].stringValues[0])
            free(pva->pvaData[index].monitorData[0].stringValues[0]);
        }
        pva->pvaData[index].monitorData[0].values[0] = pvEnumerated.getIndex();
        s = pvEnumerated.getChoice();
        pva->pvaData[index].monitorData[0].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
        strcpy(pva->pvaData[index].monitorData[0].stringValues[0], s.c_str());
        pva->pvaData[index].numMonitorReadings = 1;
        pva->pvaData[index].monitorGeneration++;
      } else {
        i = pva->pvaData[index].numGetReadings;
        if (pva->pvaData[index].numGetReadings == 0) {
          pva->pvaData[index].fieldType = pvStructurePtr->getField()->getType(); //should always be epics::pvData::structure
          pva->pvaData[index].pvEnumeratedStructure = true;
          pva->pvaData[index].numGetElements = 1;
          pva->pvaData[index].numeric = true;
          pva->pvaData[index].nonnumeric = true;
          pva->pvaData[index].scalarType = epics::pvData::pvString;
        } else if (pva->limitGetReadings) {
          i = 0;
        }
        if (!EnsureGetReadingCapacity(pva, index, i + 1)) {
          std::cerr << "ERROR: unable to allocate storage for another PVA reading" << std::endl;
          pvEnumerated.detach();
          return (1);
        }
        if (pva->pvaData[index].getData[i].values == NULL) {
          pva->pvaData[index].getData[i].values = (double *)malloc(sizeof(double));
        }
        if (pva->pvaData[index].getData[i].stringValues == NULL) {
          pva->pvaData[index].getData[i].stringValues = (char **)malloc(sizeof(char *) * 1);
          pva->pvaData[index].getData[i].stringValues[0] = NULL;
        } else if (pva->pvaData[index].getData[i].stringValues[0]) {
          free(pva->pvaData[index].getData[i].stringValues[0]);
          pva->pvaData[index].getData[i].stringValues[0] = NULL;
        }
        pva->pvaData[index].getData[i].values[0] = pvEnumerated.getIndex();
        s = pvEnumerated.getChoice();
        pva->pvaData[index].getData[i].stringValues[0] = (char *)malloc(sizeof(char) * (s.length() + 1));
        strcpy(pva->pvaData[index].getData[i].stringValues[0], s.c_str());
        if (pva->limitGetReadings) {
          pva->pvaData[index].numGetReadings = 1;
        } else {
          pva->pvaData[index].numGetReadings++;
        }
      }
      pvEnumerated.detach();
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractStructureValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode) {
  long fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  epics::pvData::PVStructurePtr pvStructurePtr;
  epics::pvData::PVFieldPtr pvFieldPtr;
  std::string afterDot;
  pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtr);

  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  if (fieldCount == 0) {
    fprintf(stderr, "Error: structure has no fields for %s\n", pva->pvaChannelNames[index].c_str());
    return (1);
  }
  if (fieldCount > 1) {
    size_t pos = pva->pvaChannelNames[index].find('.');
    if (pos != std::string::npos) {
      afterDot = pva->pvaChannelNames[index].substr(pos + 1);
      pos = afterDot.find('.');
      if (pos != std::string::npos) {
        afterDot = afterDot.substr(pos + 1);
      } else {
        pva->pvaClientGetPtr[index]->getData()->getPVStructure()->dumpValue(std::cerr);
        fprintf(stderr, "Error: sub-field is not specific enough\n");
        return (1);
      }
    } else {
      pva->pvaClientGetPtr[index]->getData()->getPVStructure()->dumpValue(std::cerr);
      fprintf(stderr, "Error: sub-field is not specific enough\n");
      return (1);
    }
    if (afterDot.find_first_of("[(@") != std::string::npos) {
      return ExtractByPath(pva, index, pvStructurePtr, afterDot, monitorMode);
    }
    pvFieldPtr = pvStructurePtr->getSubField(afterDot);
    if (pvFieldPtr == NULL) {
      fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[index].c_str());
      return (1);
    }
    switch (pvFieldPtr->getField()->getType()) {
    case epics::pvData::scalar: {
      if (ExtractScalarValue(pva, index, pvFieldPtr, monitorMode)) {
        return (1);
      }
      break;
    }
    case epics::pvData::scalarArray: {
      if (ExtractScalarArrayValue(pva, index, pvFieldPtr, monitorMode)) {
        return (1);
      }
      break;
    }
    case epics::pvData::structure: {
      if (ExtractStructureValue(pva, index, pvFieldPtr, monitorMode)) {
        return (1);
      }
      break;
    }
    case epics::pvData::union_: {
      if (ExtractUnionValue(pva, index, pvFieldPtr, monitorMode)) {
        return (1);
      }
      break;
    }
    case epics::pvData::structureArray: {
      std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
      return (1);
    }
    default: {
      std::cerr << "ERROR: Need code to handle " << pvFieldPtr->getField()->getType() << std::endl;
      return (1);
    }
    }
    return (0);
  }
  fieldName = PVFieldPtrArray[0]->getFieldName();
  switch (PVFieldPtrArray[0]->getField()->getType()) {
  case epics::pvData::scalar: {
    if (ExtractScalarValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::scalarArray: {
    if (ExtractScalarArrayValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::structure: {
    if (ExtractStructureValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::union_: {
    if (ExtractUnionValue(pva, index, PVFieldPtrArray[0], monitorMode)) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::structureArray: {
    std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
    return (1);
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
    return (1);
  }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long ExtractPVAValuesOld(PVA_OVERALL *pva) {
  long i, j;
  std::string id;
  std::string afterDot;
  bool monitorMode = false;
  epics::pvData::PVStructurePtr pvStructurePtr;
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i]) {
      pvStructurePtr = pva->pvaClientGetPtr[i]->getData()->getPVStructure();
      UpdateAlarmSeverity(pva, i, pvStructurePtr);
      if (GetPVARequestedPath(pva, i, afterDot)) {
        if (ExtractByPath(pva, i, pvStructurePtr, afterDot, monitorMode))
          return (1);
        continue;
      }
      id = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getID();
      if (id == "epics:nt/NTScalar:1.0") {
        if (ExtractNTScalarValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "epics:nt/NTScalarArray:1.0") {
        if (ExtractNTScalarArrayValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "epics:nt/NTEnum:1.0") {
        if (ExtractNTEnumValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "structure") {
        epics::pvData::PVFieldPtrArray PVFieldPtrArray;
        long fieldCount;
        PVFieldPtrArray = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getPVFields();
        fieldCount = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
        if (fieldCount > 1) {
          if (PVFieldPtrArray[0]->getFieldName() != "value") {
            pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
            fprintf(stderr, "Error: sub-field is not specific enough\n");
            return (1);
          }
        }
        if (fieldCount == 0) {
          fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
        switch (PVFieldPtrArray[0]->getField()->getType()) {
        case epics::pvData::scalar: {
          if (ExtractScalarValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::scalarArray: {
          if (ExtractScalarArrayValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::structure: {
          if (ExtractStructureValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::union_: {
          if (ExtractUnionValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        default: {
          std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
          return (1);
        }
        }
        if (pva->includeAlarmSeverity && (fieldCount > 1)) {
          for (j = 0; j < fieldCount; j++) {
            if (PVFieldPtrArray[j]->getFieldName() == "alarm") {
              if (PVFieldPtrArray[j]->getField()->getType() == epics::pvData::structure) {
                epics::pvData::PVStructurePtr alarmStructurePtr;

                alarmStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
                epics::pvData::PVScalarPtr severity = alarmStructurePtr->getSubField<epics::pvData::PVScalar>("severity");
                if (!severity) {
                  pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
                  fprintf(stderr, "Error: alarm->severity field is missing\n");
                  return (1);
                }
                pva->pvaData[i].alarmSeverity = severity->getAs<int>();
              }
              break;
            }
          }
        }
      } else {
#ifdef DEBUG
        pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
#endif
        std::cerr << "Error: unrecognized structure ID (" << id << ")" << std::endl;
        return (1);
      }
    }
  }
  return (0);
}

long ExtractPVAValues(PVA_OVERALL *pva) {
  long i, j;
  std::string id;
  bool monitorMode = false;
  epics::pvData::PVStructurePtr pvStructurePtr;
  epics::pvData::PVFieldPtr pvFieldPtr;
  std::string afterDot;

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i]) {
      pvStructurePtr = pva->pvaClientGetPtr[i]->getData()->getPVStructure();
      UpdateAlarmSeverity(pva, i, pvStructurePtr);
      if (GetPVARequestedPath(pva, i, afterDot)) {
        if (ExtractByPath(pva, i, pvStructurePtr, afterDot, monitorMode))
          return (1);
        continue;
      }
      id = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getID();
      if (id == "epics:nt/NTScalar:1.0") {
        if (ExtractNTScalarValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "epics:nt/NTScalarArray:1.0") {
        if (ExtractNTScalarArrayValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "epics:nt/NTEnum:1.0") {
        if (ExtractNTEnumValue(pva, i, pvStructurePtr, monitorMode)) {
          return (1);
        }
      } else if (id == "structure") {
        epics::pvData::PVFieldPtrArray PVFieldPtrArray;
        long fieldCount;
        PVFieldPtrArray = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getPVFields();
        fieldCount = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
        if (fieldCount == 0) {
          fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
        if (fieldCount > 1) {
          if (PVFieldPtrArray[0]->getFieldName() != "value") {
            size_t pos = pva->pvaChannelNames[i].find('.');
            if (pos != std::string::npos) {
              afterDot = pva->pvaChannelNames[i].substr(pos + 1);
            } else {
              pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
              fprintf(stderr, "Error: sub-field is not specific enough\n");
              return (1);
            }
            if (afterDot.find_first_of("[(@") != std::string::npos) {
              if (ExtractByPath(pva, i, pva->pvaClientGetPtr[i]->getData()->getPVStructure(), afterDot, monitorMode)) {
                return (1);
              }
              continue;
            }
            pvFieldPtr = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getSubField(afterDot);
            if (pvFieldPtr == NULL) {
              fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[i].c_str());
              return (1);
            }
            switch (pvFieldPtr->getField()->getType()) {
            case epics::pvData::scalar: {
              if (ExtractScalarValue(pva, i, pvFieldPtr, monitorMode)) {
                return (1);
              }
              break;
            }
            case epics::pvData::scalarArray: {
              if (ExtractScalarArrayValue(pva, i, pvFieldPtr, monitorMode)) {
                return (1);
              }
              break;
            }
            case epics::pvData::structure: {
              if (ExtractStructureValue(pva, i, pvFieldPtr, monitorMode)) {
                return (1);
              }
              break;
            }
            case epics::pvData::union_: {
              if (ExtractUnionValue(pva, i, pvFieldPtr, monitorMode)) {
                return (1);
              }
              break;
            }
            case epics::pvData::structureArray: {
              std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
              return (1);
            }
            default: {
              std::cerr << "ERROR: Need code to handle " << pvFieldPtr->getField()->getType() << std::endl;
              return (1);
            }
            }
            continue;
          }
        }
        switch (PVFieldPtrArray[0]->getField()->getType()) {
        case epics::pvData::scalar: {
          if (ExtractScalarValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::scalarArray: {
          if (ExtractScalarArrayValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::structure: {
          if (ExtractStructureValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::union_: {
          if (ExtractUnionValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
            return (1);
          }
          break;
        }
        case epics::pvData::structureArray: {
          size_t pos = pva->pvaChannelNames[i].find('.');
          if (pos != std::string::npos) {
            afterDot = pva->pvaChannelNames[i].substr(pos + 1);
            if (ExtractByPath(pva, i, pva->pvaClientGetPtr[i]->getData()->getPVStructure(), afterDot, monitorMode)) {
              return (1);
            }
            break;
          }
          std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
          return (1);
        }
        default: {
          std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
          return (1);
        }
        }
        if (pva->includeAlarmSeverity && (fieldCount > 1)) {
          for (j = 0; j < fieldCount; j++) {
            if (PVFieldPtrArray[j]->getFieldName() == "alarm") {
              if (PVFieldPtrArray[j]->getField()->getType() == epics::pvData::structure) {
                epics::pvData::PVStructurePtr alarmStructurePtr;

                alarmStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
                epics::pvData::PVScalarPtr severity = alarmStructurePtr->getSubField<epics::pvData::PVScalar>("severity");
                if (!severity) {
                  pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
                  fprintf(stderr, "Error: alarm->severity field is missing\n");
                  return (1);
                }
                pva->pvaData[i].alarmSeverity = severity->getAs<int>();
              }
              break;
            }
          }
        }
      } else {
#ifdef DEBUG
        pva->pvaClientGetPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
#endif
        std::cerr << "Error: unrecognized structure ID (" << id << ")" << std::endl;
        return (1);
      }
    }
  }
  return (0);
}

long count_chars(char *string, char c) {
  long i = 0;
  while (*string) {
    if (*string++ == c)
      i++;
  }
  return i;
}

long PutScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr) {
  epics::pvData::PVScalarPtr pvScalarPtr;
  pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtr);
  try {
    if (pva->pvaData[index].putData[0].values != NULL) {
      pvScalarPtr->putFrom<double>(pva->pvaData[index].putData[0].values[0]);
    } else if (pva->pvaData[index].putData[0].stringValues != NULL &&
               pva->pvaData[index].putData[0].stringValues[0] != NULL) {
      pvScalarPtr->putFrom<std::string>(pva->pvaData[index].putData[0].stringValues[0]);
    } else {
      std::cerr << "Error: no scalar put value was prepared" << std::endl;
      return 1;
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return (0);
}

long PutNTScalarValue(PVA_OVERALL *pva, long index) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getPVFields();
  fieldCount = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (PutScalarValue(pva, index, PVFieldPtrArray[j])) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PutScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr) {
  long n;
  epics::pvData::PVScalarArrayPtr pvScalarArrayPtr;
  pvScalarArrayPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(PVFieldPtr);
  try {
    if (pva->pvaData[index].putData[0].values != NULL ||
        (pva->pvaData[index].numPutElements == 0 && pva->pvaData[index].numeric)) {
      epics::pvData::shared_vector<double> values(pva->pvaData[index].numPutElements);
      for (n = 0; n < pva->pvaData[index].numPutElements; n++) {
        values[n] = pva->pvaData[index].putData[0].values[n];
      }
      pvScalarArrayPtr->setLength(pva->pvaData[index].numPutElements);
      pvScalarArrayPtr->putFrom(freeze(values));
    } else {
      epics::pvData::shared_vector<std::string> values(pva->pvaData[index].numPutElements);
      for (n = 0; n < pva->pvaData[index].numPutElements; n++) {
        values[n] = pva->pvaData[index].putData[0].stringValues[n];
      }
      pvScalarArrayPtr->setLength(pva->pvaData[index].numPutElements);
      pvScalarArrayPtr->putFrom(freeze(values));
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return (0);
}

long PutNTScalarArrayValue(PVA_OVERALL *pva, long index) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getPVFields();
  fieldCount = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      if (PutScalarArrayValue(pva, index, PVFieldPtrArray[j])) {
        return (1);
      }
      return (0);
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PutNTEnumValue(PVA_OVERALL *pva, long index) {
  long j, fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  PVFieldPtrArray = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getPVFields();
  fieldCount = pva->pvaClientPutPtr[index]->getData()->getPVStructure()->getStructure()->getNumberFields();
  for (j = 0; j < fieldCount; j++) {
    fieldName = PVFieldPtrArray[j]->getFieldName();
    if (fieldName == "value") {
      epics::pvData::PVStructurePtr pvStructurePtr;
      epics::pvData::PVEnumerated pvEnumerated;
      bool result;
      pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
      result = pvEnumerated.attach(pvStructurePtr);
      if (result) {
        try {
          int enumindex, numChoices;
          epics::pvData::PVStringArray::const_svector choices;
          numChoices = pvEnumerated.getNumberChoices();

          if (pva->pvaData[index].putData[0].stringValues != NULL &&
              pva->pvaData[index].putData[0].stringValues[0] != NULL) {
            enumindex = -1;
            choices = pvEnumerated.getChoices();
            for (size_t i = 0; i < choices.size(); i++) {
              if (pva->pvaData[index].putData[0].stringValues[0] == choices[i]) {
                enumindex = i;
              }
            }
            if (enumindex == -1) {
              if (!ParseIntValue(pva->pvaData[index].putData[0].stringValues[0], &enumindex)) {
                fprintf(stderr, "error: value (%s) for %s is not a valid option.\n", pva->pvaData[index].putData[0].stringValues[0], pva->pvaChannelNames[index].c_str());
                return (1);
              }
              if ((enumindex < 0) || (enumindex >= numChoices)) {
                fprintf(stderr, "error: value (%s) for %s is out of range.\n", pva->pvaData[index].putData[0].stringValues[0], pva->pvaChannelNames[index].c_str());
                return (1);
              }
            }
          } else {
            enumindex = (int)pva->pvaData[index].putData[0].values[0];
            if ((enumindex < 0) || (enumindex >= numChoices)) {
              fprintf(stderr, "error: value (%d) for %s is out of range.\n", enumindex, pva->pvaChannelNames[index].c_str());
              return (1);
            }
          }
          pvEnumerated.setIndex(enumindex);
          pvEnumerated.detach();
        } catch (std::exception &e) {
          std::cerr << "Error: " << e.what() << "\n";
          return 1;
        }
        return (0);
      } else {
        std::cerr << "Error: Need code to handle a non-enumerated structure" << std::endl;
        return (1);
      }
    }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PutStructureValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr) {
  long fieldCount;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray;
  std::string fieldName;
  epics::pvData::PVStructurePtr pvStructurePtr;
  pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtr);

  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  PVFieldPtrArray = pvStructurePtr->getPVFields();
  if (fieldCount == 0) {
    fprintf(stderr, "Error: structure has no fields for %s\n", pva->pvaChannelNames[index].c_str());
    return (1);
  }
  if (fieldCount > 1) {
    pvStructurePtr->dumpValue(std::cerr);
    fprintf(stderr, "Error: sub-field is not specific enough\n");
    return (1);
  }
  fieldName = PVFieldPtrArray[0]->getFieldName();
  switch (PVFieldPtrArray[0]->getField()->getType()) {
  case epics::pvData::scalar: {
    if (PutScalarValue(pva, index, PVFieldPtrArray[0])) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::scalarArray: {
    if (PutScalarArrayValue(pva, index, PVFieldPtrArray[0])) {
      return (1);
    }
    return (0);
    break;
  }
  case epics::pvData::structure: {
    if (PutStructureValue(pva, index, PVFieldPtrArray[0])) {
      return (1);
    }
    return (0);
    break;
  }
  default: {
    std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
    return (1);
  }
  }
  std::cerr << "ERROR: Value field is missing." << std::endl;
  return (1);
}

long PrepPut(PVA_OVERALL *pva, long index, double value) {
  if (!ValidatePutArguments(pva, index, &value, 1))
    return (1);
  long oldCount = pva->pvaData[index].numPutElements;
  if (pva->pvaData[index].numeric) {
    if (!ResizeNumericValues(&pva->pvaData[index].putData[0].values, 1)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResetStringValues(&pva->pvaData[index].putData[0].stringValues, oldCount, 0);
    pva->pvaData[index].putData[0].values[0] = value;
  } else {
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%.17g", value);
    std::vector<std::string> strings(1, buffer);
    if (!ReplaceStringValues(&pva->pvaData[index].putData[0].stringValues,
                             oldCount, strings)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResizeNumericValues(&pva->pvaData[index].putData[0].values, 0);
  }
  pva->pvaData[index].numPutElements = 1;
  pva->pvaData[index].putPrepared = true;
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, double *value, long length) {
  if (!ValidatePutArguments(pva, index, value, length))
    return (1);

  long oldCount = pva->pvaData[index].numPutElements;
  if (pva->pvaData[index].numeric) {
    if (!ResizeNumericValues(&pva->pvaData[index].putData[0].values, length)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResetStringValues(&pva->pvaData[index].putData[0].stringValues, oldCount, 0);
    for (long i = 0; i < length; i++) {
      pva->pvaData[index].putData[0].values[i] = value[i];
    }
  } else {
    char buffer[100];
    std::vector<std::string> strings;
    strings.reserve(length);
    for (long i = 0; i < length; i++) {
      snprintf(buffer, sizeof(buffer), "%.17g", value[i]);
      strings.push_back(buffer);
    }
    if (!ReplaceStringValues(&pva->pvaData[index].putData[0].stringValues,
                             oldCount, strings)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResizeNumericValues(&pva->pvaData[index].putData[0].values, 0);
  }
  pva->pvaData[index].numPutElements = length;
  pva->pvaData[index].putPrepared = true;
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, int64_t value) {
  if (!ValidatePutArguments(pva, index, &value, 1))
    return (1);
  long oldCount = pva->pvaData[index].numPutElements;
  char buffer[100];
  snprintf(buffer, sizeof(buffer), "%" PRId64, value);
  std::vector<std::string> strings(1, buffer);
  if (!ReplaceStringValues(&pva->pvaData[index].putData[0].stringValues,
                           oldCount, strings)) {
    fprintf(stderr, "error: unable to allocate put storage for %s\n",
            pva->pvaChannelNames[index].c_str());
    return (1);
  }
  ResizeNumericValues(&pva->pvaData[index].putData[0].values, 0);
  pva->pvaData[index].numPutElements = 1;
  pva->pvaData[index].putPrepared = true;
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, int64_t *value, long length) {
  if (!ValidatePutArguments(pva, index, value, length))
    return (1);

  long oldCount = pva->pvaData[index].numPutElements;
  char buffer[100];
  std::vector<std::string> strings;
  strings.reserve(length);
  for (long i = 0; i < length; i++) {
    snprintf(buffer, sizeof(buffer), "%" PRId64, value[i]);
    strings.push_back(buffer);
  }
  if (!ReplaceStringValues(&pva->pvaData[index].putData[0].stringValues,
                           oldCount, strings)) {
    fprintf(stderr, "error: unable to allocate put storage for %s\n",
            pva->pvaChannelNames[index].c_str());
    return (1);
  }
  ResizeNumericValues(&pva->pvaData[index].putData[0].values, 0);
  pva->pvaData[index].numPutElements = length;
  pva->pvaData[index].putPrepared = true;
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, char *value) {
  if (!ValidatePutArguments(pva, index, value, 1))
    return (1);
  long oldCount = pva->pvaData[index].numPutElements;
  if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
    double parsed;
    if (!ParseDoubleValue(value, &parsed)) {
      fprintf(stderr, "error: value (%s) for %s is not numerical\n", value, pva->pvaChannelNames[index].c_str());
      return (1);
    }
    if (!ResizeNumericValues(&pva->pvaData[index].putData[0].values, 1)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResetStringValues(&pva->pvaData[index].putData[0].stringValues, oldCount, 0);
    pva->pvaData[index].putData[0].values[0] = parsed;
  } else {
    std::vector<std::string> strings(1, value);
    if (!ReplaceStringValues(&pva->pvaData[index].putData[0].stringValues,
                             oldCount, strings)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResizeNumericValues(&pva->pvaData[index].putData[0].values, 0);
  }
  pva->pvaData[index].numPutElements = 1;
  pva->pvaData[index].putPrepared = true;
  return (0);
}

long PrepPut(PVA_OVERALL *pva, long index, char **value, long length) {
  if (!ValidatePutArguments(pva, index, value, length))
    return (1);
  for (long i = 0; i < length; i++) {
    if (value[i] == NULL) {
      fprintf(stderr, "error: NULL string passed to PrepPut\n");
      return (1);
    }
  }

  long oldCount = pva->pvaData[index].numPutElements;
  if (pva->pvaData[index].numeric && (pva->pvaData[index].pvEnumeratedStructure == false)) {
    std::vector<double> parsed(length);
    for (long i = 0; i < length; i++) {
      if (!ParseDoubleValue(value[i], &parsed[i])) {
        fprintf(stderr, "error: value (%s) for %s is not numerical\n", value[i], pva->pvaChannelNames[index].c_str());
        return (1);
      }
    }
    if (!ResizeNumericValues(&pva->pvaData[index].putData[0].values, length)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResetStringValues(&pva->pvaData[index].putData[0].stringValues, oldCount, 0);
    std::copy(parsed.begin(), parsed.end(), pva->pvaData[index].putData[0].values);
  } else {
    std::vector<std::string> strings;
    strings.reserve(length);
    for (long i = 0; i < length; i++) {
      strings.push_back(value[i]);
    }
    if (!ReplaceStringValues(&pva->pvaData[index].putData[0].stringValues,
                             oldCount, strings)) {
      fprintf(stderr, "error: unable to allocate put storage for %s\n",
              pva->pvaChannelNames[index].c_str());
      return (1);
    }
    ResizeNumericValues(&pva->pvaData[index].putData[0].values, 0);
  }
  pva->pvaData[index].numPutElements = length;
  pva->pvaData[index].putPrepared = true;
  return (0);
}

/*
  Put the values from the pva structure and send them to the PVs. See cavput.cc for an example on how to populate this pva structure.
*/
long PutPVAValues(PVA_OVERALL *pva) {
  long i, j, num = 0;
  std::string id;
  epics::pvData::Status status;

  if (!RefreshConnectionState(pva)) {
    fprintf(stderr, "error: PVA channels have not been connected\n");
    return (1);
  }
  const epics::pvaClient::PvaClientChannelArray &pvaClientChannelArray = pva->pvaClientChannelArray;
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    pva->isConnected[i] = pva->isInternalConnected[pva->pvaData[i].L2Ptr];
    if (pva->isConnected[i] == false) {
      if (pva->pvaData[i].putPrepared) {
        fprintf(stderr, "Error: Can't put value to %s. Not connected.\n", pva->pvaChannelNames[i].c_str());
        return (1);
      }
      num++;
    } else if (pva->pvaData[i].putPrepared && (pva->pvaData[i].havePutPtr == false)) {
      try {
        pva->pvaClientPutPtr[i] = pvaClientChannelArray[pva->pvaData[i].L2Ptr]->createPut(pva->pvaChannelNamesSub[i]);
        if (pva->usePutCallbacks) {
          pva->pvaClientPutPtr[i]->setRequester((epics::pvaClient::PvaClientPutRequesterPtr)pva->putReqPtr);
        }
        pva->pvaData[i].havePutPtr = true;
      } catch (std::exception &e) {
        fprintf(stderr, "error: unable to create put request for %s: %s\n",
                pva->pvaChannelNames[i].c_str(), e.what());
        pva->pvaClientPutPtr[i].reset();
        pva->pvaData[i].havePutPtr = false;
        return (1);
      }
    }
  }
  pva->numNotConnected = num;

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].putPrepared) {
      if (!pva->pvaClientPutPtr[i] || !HasGetData(pva, i)) {
        fprintf(stderr, "error: %s has no valid get/put data for the put request\n",
                pva->pvaChannelNames[i].c_str());
        return (1);
      }
#ifdef DEBUG
      pva->pvaClientPutPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
#endif
      std::string requestedPath;
      if (GetPVARequestedPath(pva, i, requestedPath)) {
        if (PutByPath(pva, i, pva->pvaClientPutPtr[i]->getData()->getPVStructure(), requestedPath))
          return (1);
        continue;
      }
      //get the id string from the GetPtr instead of the PutPtr
      id = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getID();
      if (id == "epics:nt/NTScalar:1.0") {
        if (PutNTScalarValue(pva, i)) {
          return (1);
        }
      } else if (id == "epics:nt/NTScalarArray:1.0") {
        if (PutNTScalarArrayValue(pva, i)) {
          return (1);
        }
      } else if (id == "epics:nt/NTEnum:1.0") {
        if (PutNTEnumValue(pva, i)) {
          return (1);
        }
      } else if (id == "structure") {
        epics::pvData::PVFieldPtrArray PVFieldPtrArray;
        long fieldCount;
        PVFieldPtrArray = pva->pvaClientPutPtr[i]->getData()->getPVStructure()->getPVFields();
        fieldCount = pva->pvaClientPutPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
        if (fieldCount > 1) {
          if (PVFieldPtrArray[0]->getFieldName() != "value") {
            pva->pvaClientPutPtr[i]->getData()->getPVStructure()->dumpValue(std::cerr);
            fprintf(stderr, "Error: sub-field is not specific enough\n");
            return (1);
          }
        }
        if (fieldCount == 0) {
          fprintf(stderr, "Error: sub-field does not exist for %s\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
        switch (PVFieldPtrArray[0]->getField()->getType()) {
        case epics::pvData::scalar: {
          if (PutScalarValue(pva, i, PVFieldPtrArray[0])) {
            return (1);
          }
          break;
        }
        case epics::pvData::scalarArray: {
          if (PutScalarArrayValue(pva, i, PVFieldPtrArray[0])) {
            return (1);
          }
          break;
        }
        case epics::pvData::structure: {
          if (PutStructureValue(pva, i, PVFieldPtrArray[0])) {
            return (1);
          }
          break;
        }
        case epics::pvData::structureArray: {
          size_t pos = pva->pvaChannelNames[i].find('.');
          if (pos != std::string::npos) {
            std::string afterDot = pva->pvaChannelNames[i].substr(pos + 1);
            if (PutByPath(pva, i, pva->pvaClientPutPtr[i]->getData()->getPVStructure(), afterDot)) {
              return (1);
            }
            break;
          }
          std::cerr << "Error: structureArray requires an index and a member (e.g. dimension[0].size, dimension(0).size, or dimension@0.size)" << std::endl;
          return (1);
        }
        default: {
          std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
          return (1);
        }
        }
      } else {
        std::cerr << "Error: unrecognized structure ID (" << id << ")" << std::endl;
        return (1);
      }
    }
  }

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].putPrepared) {
      try {
        pva->pvaClientPutPtr[i]->issuePut();
      } catch (std::exception &e) {
        fprintf(stderr, "error: unable to issue put request for %s: %s\n",
                pva->pvaChannelNames[i].c_str(), e.what());
        return (1);
      }
    }
  }

  if (pva->usePutCallbacks == false) {
    for (i = 0; i < pva->numPVs; i++) {
      if (pva->pvaData[i].skip == true) {
        continue;
      }
      if (pva->pvaData[i].putPrepared) {
        bool putFailed = false;
        try {
          status = pva->pvaClientPutPtr[i]->waitPut();
        } catch (std::exception &e) {
          putFailed = true;
        }
        if (putFailed || !status.isSuccess()) {
          fprintf(stderr, "error: %s did not respond to the \"put\" request\n", pva->pvaChannelNames[i].c_str());
          return (1);
        }
      }
    }
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].putPrepared) {
      if (pva->pvaData[i].putData[0].stringValues != NULL) {
        for (j = 0; j < pva->pvaData[i].numPutElements; j++) {
          free(pva->pvaData[i].putData[0].stringValues[j]);
          pva->pvaData[i].putData[0].stringValues[j] = NULL;
        }
      }
      pva->pvaData[i].numPutElements = 0;
      pva->pvaData[i].putPrepared = false;
    }
  }
  return (0);
}

/*
  Start monitoring the PVs. Use the PollMonitoredPVA values to identify if an event has occurred.
  FIX THIS There is a unique problem of what to do with PVs that are not connected when the program starts but become connected later
*/
long MonitorPVAValues(PVA_OVERALL *pva) {
  long i, num;
  epics::pvData::Status status;

  if (pva == NULL) {
    return (0);
  }
  num = 0;
  if (!RefreshConnectionState(pva)) {
    fprintf(stderr, "error: PVA channels have not been connected\n");
    return (1);
  }
  const epics::pvaClient::PvaClientChannelArray &pvaClientChannelArray = pva->pvaClientChannelArray;
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    pva->isConnected[i] = pva->isInternalConnected[pva->pvaData[i].L2Ptr];
    if (pva->isConnected[i]) {
      if (pva->pvaData[i].haveMonitorPtr == false) {
        try {
          std::string monitorRequest = pva->pvaChannelNamesSub[i];
          if (pva->includeAlarmSeverity && pva->pvaProvider[i].compare("pva") == 0) {
            std::vector<std::string> fields;
            fields.push_back(monitorRequest);
            fields.push_back("alarm.severity");
            monitorRequest = convertToProperRequestFormat(fields);
          }
          pva->pvaClientMonitorPtr[i] =
            pvaClientChannelArray[pva->pvaData[i].L2Ptr]->createMonitor(monitorRequest);
          if (pva->useMonitorCallbacks) {
            pva->pvaClientMonitorPtr[i]->setRequester((epics::pvaClient::PvaClientMonitorRequesterPtr)pva->monitorReqPtr);
          }
          pva->pvaClientMonitorPtr[i]->issueConnect();
          status = pva->pvaClientMonitorPtr[i]->waitConnect();
          if (!status.isSuccess()) {
            fprintf(stderr, "error: %s did not respond to the \"waitConnect\" request\n", pva->pvaChannelNames[i].c_str());
            pva->pvaClientMonitorPtr[i].reset();
            return (1);
          }
          pva->pvaClientMonitorPtr[i]->start();
          pva->pvaData[i].haveMonitorPtr = true;
        } catch (std::exception &e) {
          fprintf(stderr, "error: unable to start monitor for %s: %s\n",
                  pva->pvaChannelNames[i].c_str(), e.what());
          pva->pvaClientMonitorPtr[i].reset();
          pva->pvaData[i].haveMonitorPtr = false;
          return (1);
        }
      }
    } else {
      num++;
    }
  }
  pva->numNotConnected = num;
  return (0);
}

void PausePVAMonitoring(PVA_OVERALL **pva, long count) {
  if (pva == NULL || count <= 0)
    return;
  long i;
  for (i = 0; i < count; i++) {
    PausePVAMonitoring(pva[i]);
  }
}

void PausePVAMonitoring(PVA_OVERALL *pva) {
  long i;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i] && pva->pvaData[i].haveMonitorPtr && pva->pvaClientMonitorPtr[i]) {
      pva->pvaClientMonitorPtr[i]->stop();
    }
  }
}

void ResumePVAMonitoring(PVA_OVERALL **pva, long count) {
  if (pva == NULL || count <= 0)
    return;
  long i;
  for (i = 0; i < count; i++) {
    ResumePVAMonitoring(pva[i]);
  }
}

void ResumePVAMonitoring(PVA_OVERALL *pva) {
  long i;
  if (pva == NULL) {
    return;
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->isConnected[i] && pva->pvaData[i].haveMonitorPtr && pva->pvaClientMonitorPtr[i]) {
      pva->pvaClientMonitorPtr[i]->start();
    }
  }
}

/*
  Check to see if an event has occurred on a monitored PV and if so, place the data into the pva structure.
  Returns number of events found or -1 for error
*/
long PollMonitoredPVA(PVA_OVERALL *pva) {
  PVA_OVERALL *pvaArray[] = {pva};
  return PollMonitoredPVA(pvaArray, 1);
}

/* Returns number of events found or -1 for error
 */
long PollMonitoredPVA(PVA_OVERALL **pva, long count) {
  long result = 0, i, n;
  std::string id;
  bool monitorMode = true, connectionChange = false;
  epics::pvData::PVStructurePtr pvStructurePtr;

  if (pva == NULL || count < 0)
    return -1;

  for (n = 0; n < count; n++) {
    if (pva[n] != NULL) {
      //A PV which was initially unconnected may have connected and we need to start monitoring it
      for (i = 0; i < pva[n]->numMultiChannels; i++) {
        if (pva[n]->pvaClientMultiChannelPtr[i]->connectionChange()) {
          connectionChange = true;
        }
      }
      if (connectionChange) {
        if (MonitorPVAValues(pva[n]) != 0) {
          return (-1);
        }
        connectionChange = false;
      }

      for (long i = 0; i < pva[n]->numPVs; i++) {
        if (pva[n]->pvaData[i].skip == true) {
          continue;
        }
        if (pva[n]->isConnected[i] && pva[n]->pvaData[i].haveMonitorPtr && pva[n]->pvaClientMonitorPtr[i]) {
          if (pva[n]->pvaClientMonitorPtr[i]->poll()) {
            MonitorEventGuard eventGuard(pva[n]->pvaClientMonitorPtr[i]);
            result++;
            pvStructurePtr = pva[n]->pvaClientMonitorPtr[i]->getData()->getPVStructure();
            UpdateAlarmSeverity(pva[n], i, pvStructurePtr);
            std::string requestedPath;
            if (GetPVARequestedPath(pva[n], i, requestedPath)) {
              if (ExtractByPath(pva[n], i, pvStructurePtr, requestedPath, monitorMode))
                return (-1);
              continue;
            }
            id = pvStructurePtr->getStructure()->getID();
            if (id == "epics:nt/NTScalar:1.0") {
              if (ExtractNTScalarValue(pva[n], i, pvStructurePtr, monitorMode)) {
                return (-1);
              }
            } else if (id == "epics:nt/NTScalarArray:1.0") {
              if (ExtractNTScalarArrayValue(pva[n], i, pvStructurePtr, monitorMode)) {
                return (-1);
              }
            } else if (id == "epics:nt/NTEnum:1.0") {
              if (ExtractNTEnumValue(pva[n], i, pvStructurePtr, monitorMode)) {
                return (-1);
              }
            } else if (id == "structure") {
              epics::pvData::PVFieldPtrArray PVFieldPtrArray;
              long fieldCount;
              PVFieldPtrArray = pvStructurePtr->getPVFields();
              fieldCount = pvStructurePtr->getStructure()->getNumberFields();
              if (fieldCount == 0) {
                fprintf(stderr, "Error: monitored structure has no fields for %s\n", pva[n]->pvaChannelNames[i].c_str());
                return (-1);
              }
              if (fieldCount > 1) {
                if (PVFieldPtrArray[0]->getFieldName() != "value") {
                  size_t pos = pva[n]->pvaChannelNames[i].find('.');
                  if (pos == std::string::npos ||
                      ExtractByPath(pva[n], i, pvStructurePtr,
                                    pva[n]->pvaChannelNames[i].substr(pos + 1), monitorMode)) {
                    fprintf(stderr, "Error: sub-field is not specific enough\n");
                    return (-1);
                  }
                  continue;
                }
              }
              switch (PVFieldPtrArray[0]->getField()->getType()) {
              case epics::pvData::scalar: {
                if (ExtractScalarValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              case epics::pvData::scalarArray: {
                if (ExtractScalarArrayValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              case epics::pvData::structure: {
                if (ExtractStructureValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              case epics::pvData::union_: {
                if (ExtractUnionValue(pva[n], i, PVFieldPtrArray[0], monitorMode)) {
                  return (-1);
                }
                break;
              }
              default: {
                std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
                return (-1);
              }
              }
            } else {
              std::cerr << "Error: unrecognized monitored structure ID (" << id << ")" << std::endl;
              return (-1);
            }
          }
        }
      }
    }
  }
  return result;
}

/*
  Wait for an event on a monitored PV and place the data into the pva structure.
  result: -1 no event, 0 event, 1 error
*/
long WaitEventMonitoredPVA(PVA_OVERALL *pva, long index, double secondsToWait) {
  long result = -1;
  std::string id;
  bool monitorMode = true;
  epics::pvData::PVStructurePtr pvStructurePtr;
  if (pva == NULL || index < 0 || index >= pva->numPVs) {
    return (1);
  }
  for (long i = index; i <= index; i++) {
    if (pva->isConnected[i] && pva->pvaData[i].haveMonitorPtr && pva->pvaClientMonitorPtr[i]) {
      if (pva->pvaClientMonitorPtr[i]->waitEvent(secondsToWait)) {
        MonitorEventGuard eventGuard(pva->pvaClientMonitorPtr[i]);
        pvStructurePtr = pva->pvaClientMonitorPtr[i]->getData()->getPVStructure();
        UpdateAlarmSeverity(pva, i, pvStructurePtr);
        std::string requestedPath;
        if (GetPVARequestedPath(pva, i, requestedPath)) {
          if (ExtractByPath(pva, i, pvStructurePtr, requestedPath, monitorMode))
            return (1);
          result = 0;
          continue;
        }
        id = pvStructurePtr->getStructure()->getID();
        if (id == "epics:nt/NTScalar:1.0") {
          if (ExtractNTScalarValue(pva, i, pvStructurePtr, monitorMode)) {
            return (1);
          }
        } else if (id == "epics:nt/NTScalarArray:1.0") {
          if (ExtractNTScalarArrayValue(pva, i, pvStructurePtr, monitorMode)) {
            return (1);
          }
        } else if (id == "epics:nt/NTEnum:1.0") {
          if (ExtractNTEnumValue(pva, i, pvStructurePtr, monitorMode)) {
            return (1);
          }
        } else if (id == "structure") {
          epics::pvData::PVFieldPtrArray PVFieldPtrArray;
          long fieldCount;
          PVFieldPtrArray = pvStructurePtr->getPVFields();
          fieldCount = pvStructurePtr->getStructure()->getNumberFields();
          if (fieldCount == 0) {
            fprintf(stderr, "Error: monitored structure has no fields for %s\n", pva->pvaChannelNames[i].c_str());
            return (1);
          }
          if (fieldCount > 1) {
            if (PVFieldPtrArray[0]->getFieldName() != "value") {
              size_t pos = pva->pvaChannelNames[i].find('.');
              if (pos == std::string::npos ||
                  ExtractByPath(pva, i, pvStructurePtr,
                                pva->pvaChannelNames[i].substr(pos + 1), monitorMode)) {
                fprintf(stderr, "Error: sub-field is not specific enough\n");
                return (1);
              }
              result = 0;
              continue;
            }
          }
          switch (PVFieldPtrArray[0]->getField()->getType()) {
          case epics::pvData::scalar: {
            if (ExtractScalarValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          case epics::pvData::scalarArray: {
            if (ExtractScalarArrayValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          case epics::pvData::structure: {
            if (ExtractStructureValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          case epics::pvData::union_: {
            if (ExtractUnionValue(pva, i, PVFieldPtrArray[0], monitorMode)) {
              return (1);
            }
            break;
          }
          default: {
            std::cerr << "ERROR: Need code to handle " << PVFieldPtrArray[0]->getField()->getType() << std::endl;
            return (1);
          }
          }
        } else {
          std::cerr << "Error: unrecognized monitored structure ID (" << id << ")" << std::endl;
          return (1);
        }
        result = 0;
      }
    }
  }
  return result;
}

long ExtractPVAUnits(PVA_OVERALL *pva) {
  long i, j, n, fieldCount, fieldCount2;
  epics::pvData::PVStructurePtr pvStructurePtr;
  epics::pvData::PVFieldPtrArray PVFieldPtrArray, PVFieldPtrArray2;
  epics::pvData::PVScalarPtr pvScalarPtr;
  std::string s;
  if (pva == NULL)
    return (1);
  for (i = 0; i < pva->numPVs; i++) {
    if (pva->pvaData[i].skip == true) {
      continue;
    }
    if (pva->pvaData[i].units) {
      free(pva->pvaData[i].units);
    }
    pva->pvaData[i].units = NULL;
    if (HasGetData(pva, i)) {
      PVFieldPtrArray = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getPVFields();
      fieldCount = pva->pvaClientGetPtr[i]->getData()->getPVStructure()->getStructure()->getNumberFields();
      for (j = 0; j < fieldCount; j++) {
        if (PVFieldPtrArray[j]->getFieldName() == "display") {
          pvStructurePtr = std::tr1::static_pointer_cast<epics::pvData::PVStructure>(PVFieldPtrArray[j]);
          PVFieldPtrArray2 = pvStructurePtr->getPVFields();
          fieldCount2 = pvStructurePtr->getStructure()->getNumberFields();
          for (n = 0; n < fieldCount2; n++) {
            if (PVFieldPtrArray2[n]->getFieldName() == "units") {
              pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(PVFieldPtrArray2[n]);
              s = pvScalarPtr->getAs<std::string>();
              pva->pvaData[i].units = (char *)malloc(sizeof(char) * (s.length() + 1));
              strcpy(pva->pvaData[i].units, s.c_str());
              break;
            }
          }
          break;
        }
      }
    }
  }
  return (0);
}

long ExtractPVAControlInfo(PVA_OVERALL *pva) {
  if (pva == NULL || pva->pvaData == NULL)
    return (1);

  for (long i = 0; i < pva->numPVs; i++) {
    PVA_DATA_ALL_READINGS &reading = pva->pvaData[i];
    reading.displayLimitLow = 0.0;
    reading.displayLimitHigh = 0.0;
    reading.controlLimitLow = 0.0;
    reading.controlLimitHigh = 0.0;
    reading.displayPrecision = -1;
    reading.hasDisplayLimits = false;
    reading.hasControlLimits = false;
    reading.hasPrecision = false;

    if (reading.skip || !HasGetData(pva, i))
      continue;

    try {
      epics::pvData::PVStructurePtr root =
        pva->pvaClientGetPtr[i]->getData()->getPVStructure();
      epics::pvData::PVStructurePtr display =
        root->getSubField<epics::pvData::PVStructure>("display");
      if (display) {
        epics::pvData::PVScalarPtr low =
          display->getSubField<epics::pvData::PVScalar>("limitLow");
        epics::pvData::PVScalarPtr high =
          display->getSubField<epics::pvData::PVScalar>("limitHigh");
        epics::pvData::PVScalarPtr precision =
          display->getSubField<epics::pvData::PVScalar>("precision");
        if (low && high) {
          reading.displayLimitLow = low->getAs<double>();
          reading.displayLimitHigh = high->getAs<double>();
          reading.hasDisplayLimits = true;
        }
        if (precision) {
          reading.displayPrecision = precision->getAs<int>();
          reading.hasPrecision = true;
        }
      }

      epics::pvData::PVStructurePtr control =
        root->getSubField<epics::pvData::PVStructure>("control");
      if (control) {
        epics::pvData::PVScalarPtr low =
          control->getSubField<epics::pvData::PVScalar>("limitLow");
        epics::pvData::PVScalarPtr high =
          control->getSubField<epics::pvData::PVScalar>("limitHigh");
        if (low && high) {
          reading.controlLimitLow = low->getAs<double>();
          reading.controlLimitHigh = high->getAs<double>();
          reading.hasControlLimits = true;
        }
      }
    } catch (std::exception &) {
      /* Display and control metadata are optional. */
    }
  }
  return (0);
}

static epics::pvaClient::PvaClientChannelPtr GetChannelForIndex(PVA_OVERALL *pva, long index) {
  if (pva == NULL || index < 0 || index >= pva->numPVs)
    return epics::pvaClient::PvaClientChannelPtr();

  long internalIndex = pva->pvaData[index].L2Ptr;
  if (internalIndex < 0 || internalIndex >= (long)pva->pvaClientChannelArray.size())
    return epics::pvaClient::PvaClientChannelPtr();
  return pva->pvaClientChannelArray[internalIndex];
}

std::string GetProviderName(PVA_OVERALL *pva, long index) {
  if (pva == NULL || index < 0 || index >= pva->numPVs ||
      (size_t)index >= pva->isConnected.size() || pva->isConnected[index] == false)
    return "unknown";
  epics::pvaClient::PvaClientChannelPtr channel = GetChannelForIndex(pva, index);
  if (!channel || !channel->getChannel() || !channel->getChannel()->getProvider())
    return "unknown";
  return channel->getChannel()->getProvider()->getProviderName();
}
std::string GetRemoteAddress(PVA_OVERALL *pva, long index) {
  if (pva == NULL || index < 0 || index >= pva->numPVs ||
      (size_t)index >= pva->isConnected.size() || pva->isConnected[index] == false)
    return "unknown";
  epics::pvaClient::PvaClientChannelPtr channel = GetChannelForIndex(pva, index);
  if (!channel || !channel->getChannel())
    return "unknown";
  return channel->getChannel()->getRemoteAddress();
}
bool HaveReadAccess(PVA_OVERALL *pva, long index) {
  epics::pvData::PVFieldPtr requestedField = GetRequestedField(pva, index);
  epics::pvaClient::PvaClientChannelPtr channel = GetChannelForIndex(pva, index);
  if (!requestedField || !channel || !channel->getChannel())
    return false;
  uint32_t value = channel->getChannel()->getAccessRights(requestedField);
  return value == 1 || value == 2;
  /*
    std::string provider;
    epics::pvAccess::ca::CAChannel::shared_pointer caChan;
    if (pva->isConnected[index] == false)
    return false;
    provider = GetProviderName(pva, index);
    if (provider == "ca") {
    caChan = std::dynamic_pointer_cast<epics::pvAccess::ca::CAChannel>(pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel());
    if (ca_read_access(caChan->getChannelID()) == 0)
    return false;
    else
    return true;
    } else {
    return true;
    }
  */
}
bool HaveWriteAccess(PVA_OVERALL *pva, long index) {
  epics::pvData::PVFieldPtr requestedField = GetRequestedField(pva, index);
  epics::pvaClient::PvaClientChannelPtr channel = GetChannelForIndex(pva, index);
  if (!requestedField || !channel || !channel->getChannel())
    return false;
  return channel->getChannel()->getAccessRights(requestedField) == 2;
    
  /*
    {
    std::string provider;
    epics::pvAccess::ca::CAChannel::shared_pointer caChan;
    provider = GetProviderName(pva, index);
    if (provider == "ca") {
    caChan = std::dynamic_pointer_cast<epics::pvAccess::ca::CAChannel>(pva->pvaClientMultiChannelPtr[0]->getPvaClientChannelArray()[pva->pvaData[index].L2Ptr]->getChannel());
    if (ca_write_access(caChan->getChannelID()) == 0)
    return false;
    else
    return true;
    } else {
    return true;
    }
    }
  */
}
std::string GetAlarmSeverity(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return "unknown";
  if (pva->pvaData[index].alarmSeverity == 0) {
    return "NONE";
  } else if (pva->pvaData[index].alarmSeverity == 1) {
    return "MINOR";
  } else if (pva->pvaData[index].alarmSeverity == 2) {
    return "MAJOR";
  } else if (pva->pvaData[index].alarmSeverity == 3) {
    return "INVALID";
  }
  return "unknown";
}
std::string GetStructureID(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return "unknown";
  return pva->pvaClientGetPtr[index]->getData()->getPVStructure()->getStructure()->getID();
}
std::string GetFieldType(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return "unknown";

  epics::pvData::PVStructurePtr root = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  std::string requestedPath;
  if (GetPVARequestedPath(pva, index, requestedPath)) {
    epics::pvData::PVFieldPtr requestedField = ResolveFieldByPath(pva, index, root, requestedPath, false);
    return requestedField ? epics::pvData::TypeFunc::name(requestedField->getField()->getType()) : "unknown";
  }

  if (root->getStructure()->getID() == "epics:nt/NTEnum:1.0")
    return "ENUM structure";
  epics::pvData::PVFieldPtr requestedField = GetRequestedField(pva, index);
  return requestedField ? epics::pvData::TypeFunc::name(requestedField->getField()->getType()) : "unknown";
}

bool IsEnumFieldType(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return false;
  if (pva->pvaClientGetPtr[index]->getData()->getPVStructure()->getStructure()->getID() == "epics:nt/NTEnum:1.0")
    return true;
  else
    return false;
}
static uint32_t GetElementCountFromNelm(PVA_OVERALL *pva, long index, size_t currentCount) {
  if (currentCount != 0)
    return currentCount > UINT32_MAX ? UINT32_MAX : (uint32_t)currentCount;
  if (!pva || index < 0 || index >= pva->numPVs ||
      (size_t)index >= pva->pvaProvider.size() ||
      pva->pvaProvider[index].compare("ca") != 0 || !pva->pvaClientPtr)
    return 0;

  long internalIndex = pva->pvaData[index].L2Ptr;
  if (internalIndex < 0 || (size_t)internalIndex >= pva->pvaChannelNamesTop.size())
    return 0;
  std::string baseName = pva->pvaChannelNamesTop[internalIndex];
  size_t dotPos = baseName.find('.');
  if (dotPos != std::string::npos)
    baseName = baseName.substr(0, dotPos);
  std::string nelmName = baseName + ".NELM";

  try {
    epics::pvaClient::PvaClientChannelPtr channel = pva->pvaClientPtr->channel(nelmName, "ca", 1.0);
    epics::pvaClient::PvaClientGetPtr getPtr = channel->createGet();
    getPtr->issueGet();
    epics::pvData::Status status = getPtr->waitGet();
    if (!status.isSuccess())
      return 0;
    epics::pvaClient::PvaClientGetDataPtr getData = getPtr->getData();
    epics::pvData::PVStructurePtr pvStructurePtr = getData->getPVStructure();
    epics::pvData::PVFieldPtr pvField = pvStructurePtr->getSubField("value");
    if (!pvField || pvField->getField()->getType() != epics::pvData::scalar)
      return 0;
    epics::pvData::PVScalarPtr pvScalarPtr = std::tr1::static_pointer_cast<epics::pvData::PVScalar>(pvField);
    return pvScalarPtr->getAs<uint32_t>();
  } catch (std::exception &e) {
    return 0;
  }
}
uint32_t GetElementCount(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return 0;
  epics::pvData::PVStructurePtr root = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  if (root->getStructure()->getID() == "epics:nt/NTEnum:1.0")
    return 1;
  epics::pvData::PVFieldPtr requestedField = GetRequestedField(pva, index);
  if (!requestedField)
    return 0;
  if (requestedField->getField()->getType() == epics::pvData::scalar)
    return 1;
  if (requestedField->getField()->getType() == epics::pvData::scalarArray)
    return GetElementCountFromNelm(
      pva, index,
      std::tr1::static_pointer_cast<epics::pvData::PVScalarArray>(requestedField)->getLength());
  return 0;
}

std::string GetNativeDataType(PVA_OVERALL *pva, long index) {
  if (!HasGetData(pva, index))
    return "unknown";
  epics::pvData::PVStructurePtr root = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  if (root->getStructure()->getID() == "epics:nt/NTEnum:1.0")
    return "string";
  epics::pvData::PVFieldPtr requestedField = GetRequestedField(pva, index);
  if (!requestedField)
    return "unknown";
  if (requestedField->getField()->getType() == epics::pvData::scalar)
    return epics::pvData::ScalarTypeFunc::name(
      std::tr1::static_pointer_cast<const epics::pvData::Scalar>(
        requestedField->getField())->getScalarType());
  if (requestedField->getField()->getType() == epics::pvData::scalarArray)
    return epics::pvData::ScalarTypeFunc::name(
      std::tr1::static_pointer_cast<const epics::pvData::ScalarArray>(
        requestedField->getField())->getElementType());
  return "unknown";
}

std::string GetUnits(PVA_OVERALL *pva, long index) {
  if (pva != NULL && index >= 0 && index < pva->numPVs && pva->pvaData[index].units)
    return pva->pvaData[index].units;
  else
    return "";
}
uint32_t GetEnumChoices(PVA_OVERALL *pva, long index, char ***enumChoices) {
  uint32_t count = 0;
  std::string id;
  epics::pvData::PVStructurePtr pvStructurePtr;
  size_t fieldCount, n, m;
  if (enumChoices == NULL)
    return 0;
  *enumChoices = NULL;
  if (!HasGetData(pva, index))
    return 0;
  pvStructurePtr = pva->pvaClientGetPtr[index]->getData()->getPVStructure();
  fieldCount = pvStructurePtr->getStructure()->getNumberFields();
  id = pvStructurePtr->getStructure()->getID();
  if (id == "epics:nt/NTEnum:1.0") {
    epics::pvData::PVStringArray::const_svector choices;
    for (n = 0; n < fieldCount; n++) {
      if (pvStructurePtr->getPVFields()[n]->getFieldName() == "value") {
        epics::pvData::PVEnumerated pvEnumerated;
        if (!pvEnumerated.attach(std::tr1::static_pointer_cast<epics::pvData::PVStructure>(pvStructurePtr->getPVFields()[n])))
          return 0;
        choices = pvEnumerated.getChoices();
        if (choices.size() > UINT32_MAX ||
            choices.size() > SIZE_MAX / sizeof(**enumChoices))
          return 0;
        count = (uint32_t)choices.size();
        if (count == 0)
          return 0;
        *enumChoices = (char **)calloc(count, sizeof(**enumChoices));
        if (*enumChoices == NULL)
          return 0;
        for (m = 0; m < choices.size(); m++) {
          std::string val = "{" + choices[m] + "}";
          (*enumChoices)[m] = (char *)malloc(val.size() + 1);
          if ((*enumChoices)[m] == NULL) {
            for (size_t k = 0; k < m; k++)
              free((*enumChoices)[k]);
            free(*enumChoices);
            *enumChoices = NULL;
            return 0;
          }
          memcpy((*enumChoices)[m], val.c_str(), val.size() + 1);
        }
        break;
      }
    }
    return count;
  } else {
    return 0;
  }
}
