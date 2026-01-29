/**
 * @file pvaSDDS.h
 * @brief Functions for managing and interacting with Process Variable Array (PVA) structures.
 *
 * @details
 * This file includes a set of functions to allocate, reallocate, free, connect, monitor, and 
 * extract values for Process Variable Arrays (PVA) using EPICS PVAccess and PVData libraries. 
 * It provides utilities for managing PVAs in scenarios where EPICS Channel Access (CA) and 
 * PVAccess (PVA) protocols are used to interact with control system process variables.
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

#include "pv/pvaClient.h"
#include "pv/pvaClientMultiChannel.h"
#include "pv/pvEnumerated.h"
//#include "../modules/pvAccess/src/ca/caChannel.h"

/* Example Callback Requesters

   To use these you will need to add one or more of the following:

   pva->stateChangeReqPtr = exampleStateChangeRequester::create();
   pva->getReqPtr = exampleGetRequester::create();
   pva->monitorReqPtr = exampleMonitorRequester::create();
   pva->putReqPtr = examplePutRequester::create();

*/
class exampleStateChangeRequester;
typedef std::tr1::shared_ptr<exampleStateChangeRequester> exampleStateChangeRequesterPtr;
class exampleGetRequester;
typedef std::tr1::shared_ptr<exampleGetRequester> exampleGetRequesterPtr;
class exampleMonitorRequester;
typedef std::tr1::shared_ptr<exampleMonitorRequester> exampleMonitorRequesterPtr;
class examplePutRequester;
typedef std::tr1::shared_ptr<examplePutRequester> examplePutRequesterPtr;

class epicsShareClass exampleStateChangeRequester : public epics::pvaClient::PvaClientChannelStateChangeRequester,
                                                    public std::tr1::enable_shared_from_this<exampleStateChangeRequester> {
public:
  POINTER_DEFINITIONS(exampleStateChangeRequester);
  exampleStateChangeRequester() {
  }

  static exampleStateChangeRequesterPtr create() {
    exampleStateChangeRequesterPtr client(exampleStateChangeRequesterPtr(new exampleStateChangeRequester()));
    return client;
  }

  virtual void channelStateChange(epics::pvaClient::PvaClientChannelPtr const &channel, bool isConnected) {
    if (isConnected)
      fprintf(stdout, "StateChange: %s is connected\n", channel->getChannelName().c_str());
    else
      fprintf(stdout, "StateChange: %s is not connected\n", channel->getChannelName().c_str());
  }
};

class epicsShareClass exampleGetRequester : public epics::pvaClient::PvaClientGetRequester,
                                            public std::tr1::enable_shared_from_this<exampleGetRequester> {
public:
  POINTER_DEFINITIONS(exampleGetRequester);
  exampleGetRequester() {
  }

  static exampleGetRequesterPtr create() {
    exampleGetRequesterPtr client(exampleGetRequesterPtr(new exampleGetRequester()));
    return client;
  }

  virtual void channelGetConnect(const epics::pvData::Status &status,
                                 epics::pvaClient::PvaClientGetPtr const &clientGet) {
    fprintf(stdout, "ChannelGetConnected: status=%s %s\n",
            epics::pvData::Status::StatusTypeName[status.getType()],
            clientGet->getPvaClientChannel()->getChannelName().c_str());
  }
  virtual void getDone(const epics::pvData::Status &status,
                       epics::pvaClient::PvaClientGetPtr const &clientGet) {
    fprintf(stdout, "GetDone: status=%s %s\n",
            epics::pvData::Status::StatusTypeName[status.getType()],
            clientGet->getPvaClientChannel()->getChannelName().c_str());
  }
};

class epicsShareClass exampleMonitorRequester : public epics::pvaClient::PvaClientMonitorRequester,
                                                public std::tr1::enable_shared_from_this<exampleMonitorRequester> {
public:
  POINTER_DEFINITIONS(exampleMonitorRequester);
  exampleMonitorRequester() {
  }

  static exampleMonitorRequesterPtr create() {
    exampleMonitorRequesterPtr client(exampleMonitorRequesterPtr(new exampleMonitorRequester()));
    return client;
  }

  virtual void monitorConnect(const epics::pvData::Status &status,
                              epics::pvaClient::PvaClientMonitorPtr const &monitor,
                              epics::pvData::StructureConstPtr const &structure) {
    fprintf(stdout, "MonitorConnected: status=%s %s\n",
            epics::pvData::Status::StatusTypeName[status.getType()],
            monitor->getPvaClientChannel()->getChannelName().c_str());
  }
  virtual void event(epics::pvaClient::PvaClientMonitorPtr const &monitor) {
    fprintf(stdout, "Event: %s\n", monitor->getPvaClientChannel()->getChannelName().c_str());
  }
  virtual void unlisten() {
    fprintf(stdout, "Unlisten: \n");
  }
};

class epicsShareClass examplePutRequester : public epics::pvaClient::PvaClientPutRequester,
                                            public std::tr1::enable_shared_from_this<examplePutRequester> {
public:
  POINTER_DEFINITIONS(examplePutRequester);
  examplePutRequester() {
  }

  static examplePutRequesterPtr create() {
    examplePutRequesterPtr client(examplePutRequesterPtr(new examplePutRequester()));
    return client;
  }

  virtual void channelPutConnect(const epics::pvData::Status &status,
                                 epics::pvaClient::PvaClientPutPtr const &clientPut) {
    fprintf(stdout, "ChannelPutConnected: status=%s %s\n",
            epics::pvData::Status::StatusTypeName[status.getType()],
            clientPut->getPvaClientChannel()->getChannelName().c_str());
  }
  virtual void getDone(const epics::pvData::Status &status,
                       epics::pvaClient::PvaClientPutPtr const &clientPut) {
    fprintf(stdout, "GetDone: status=%s %s\n",
            epics::pvData::Status::StatusTypeName[status.getType()],
            clientPut->getPvaClientChannel()->getChannelName().c_str());
  }
  virtual void putDone(const epics::pvData::Status &status,
                       epics::pvaClient::PvaClientPutPtr const &clientPut) {
    fprintf(stdout, "PutDone: status=%s %s\n",
            epics::pvData::Status::StatusTypeName[status.getType()],
            clientPut->getPvaClientChannel()->getChannelName().c_str());
  }
};

typedef struct
{
  double *values;
  char **stringValues;
} PVA_DATA;

typedef struct
{
  long numGetElements;
  long numPutElements;
  long numMonitorElements;
  long numGetReadings;
  long numMonitorReadings;
  bool numeric;
  bool nonnumeric;
  bool pvEnumeratedStructure;
  epics::pvData::Type fieldType;
  epics::pvData::ScalarType scalarType;
  PVA_DATA *getData;
  PVA_DATA *putData;
  PVA_DATA *monitorData;
  double mean, median, sigma, min, max, spread, stDev, rms, MAD;
  bool haveGetPtr, havePutPtr, haveMonitorPtr;
  char *units;
  double displayLimitLow;
  double displayLimitHigh;
  double controlLimitLow;
  double controlLimitHigh;
  int displayPrecision;
  bool hasDisplayLimits;
  bool hasControlLimits;
  bool hasPrecision;
  int alarmSeverity;
  int L1Ptr;
  int L2Ptr;
  bool skip;
} PVA_DATA_ALL_READINGS;

typedef struct
{
  epics::pvaClient::PvaClientPtr pvaClientPtr;
  std::vector<epics::pvaClient::PvaClientMultiChannelPtr> pvaClientMultiChannelPtr;
  int numMultiChannels;
  std::vector<epics::pvaClient::PvaClientGetPtr> pvaClientGetPtr;
  std::vector<epics::pvaClient::PvaClientPutPtr> pvaClientPutPtr;
  std::vector<epics::pvaClient::PvaClientMonitorPtr> pvaClientMonitorPtr;

  epics::pvData::shared_vector<const std::string> pvaChannelNames;
  epics::pvData::shared_vector<const std::string> pvaChannelNamesTop;
  epics::pvData::shared_vector<const std::string> pvaChannelNamesSub;
  epics::pvData::shared_vector<epics::pvData::boolean> isConnected;         //numInternalPVs
  epics::pvData::shared_vector<epics::pvData::boolean> isInternalConnected; //numInternalPVs
  epics::pvData::shared_vector<const std::string> pvaProvider;
  epics::pvaClient::PvaClientChannelStateChangeRequesterPtr stateChangeReqPtr;
  epics::pvaClient::PvaClientGetRequesterPtr getReqPtr;
  epics::pvaClient::PvaClientMonitorRequesterPtr monitorReqPtr;
  epics::pvaClient::PvaClientPutRequesterPtr putReqPtr;
  bool useStateChangeCallbacks;
  bool useGetCallbacks;
  bool useMonitorCallbacks;
  bool usePutCallbacks;
  bool includeAlarmSeverity;
  long numPVs, prevNumPVs, numInternalPVs, prevNumInternalPVs, numNotConnected;
  PVA_DATA_ALL_READINGS *pvaData;
  bool limitGetReadings;
} PVA_OVERALL;

void allocPVA(PVA_OVERALL *pva, long PVs);
void allocPVA(PVA_OVERALL *pva, long PVs, long repeats);
void reallocPVA(PVA_OVERALL *pva, long PVs);
void reallocPVA(PVA_OVERALL *pva, long PVs, long repeats);
void freePVA(PVA_OVERALL *pva);
void freePVAGetReadings(PVA_OVERALL *pva);
void freePVAMonitorReadings(PVA_OVERALL *pva);
void ConnectPVA(PVA_OVERALL *pva, double pendIOTime);
long GetPVAValues(PVA_OVERALL *pva);
long GetPVAValues(PVA_OVERALL **pva, long count);
long PrepPut(PVA_OVERALL *pva, long index, double value);
long PrepPut(PVA_OVERALL *pva, long index, int64_t value);
long PrepPut(PVA_OVERALL *pva, long index, char *value);
long PrepPut(PVA_OVERALL *pva, long index, double *value, long length);
long PrepPut(PVA_OVERALL *pva, long index, int64_t *value, long length);
long PrepPut(PVA_OVERALL *pva, long index, char **value, long length);
long PutPVAValues(PVA_OVERALL *pva);
long MonitorPVAValues(PVA_OVERALL *pva);
long PollMonitoredPVA(PVA_OVERALL *pva);
long PollMonitoredPVA(PVA_OVERALL **pva, long count);
long WaitEventMonitoredPVA(PVA_OVERALL *pva, long index, double secondsToWait);
long count_chars(char *string, char c);
long ExtractPVAUnits(PVA_OVERALL *pva);
long ExtractPVAControlInfo(PVA_OVERALL *pva);
void PausePVAMonitoring(PVA_OVERALL **pva, long count);
void PausePVAMonitoring(PVA_OVERALL *pva);
void ResumePVAMonitoring(PVA_OVERALL **pva, long count);
void ResumePVAMonitoring(PVA_OVERALL *pva);

std::string GetProviderName(PVA_OVERALL *pva, long index);
std::string GetRemoteAddress(PVA_OVERALL *pva, long index);
bool HaveReadAccess(PVA_OVERALL *pva, long index);
bool HaveWriteAccess(PVA_OVERALL *pva, long index);
std::string GetAlarmSeverity(PVA_OVERALL *pva, long index);
std::string GetStructureID(PVA_OVERALL *pva, long index);
std::string GetFieldType(PVA_OVERALL *pva, long index);
bool IsEnumFieldType(PVA_OVERALL *pva, long index);
uint32_t GetElementCount(PVA_OVERALL *pva, long index);
std::string GetNativeDataType(PVA_OVERALL *pva, long index);
std::string GetUnits(PVA_OVERALL *pva, long index);
uint32_t GetEnumChoices(PVA_OVERALL *pva, long index, char ***choices);

//Internal Functions
long ExtractPVAValues(PVA_OVERALL *pva);
long ExtractNTScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode);
long ExtractNTScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode);
long ExtractNTEnumValue(PVA_OVERALL *pva, long index, epics::pvData::PVStructurePtr pvStructurePtr, bool monitorMode);
long ExtractScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode);
long ExtractScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode);
long ExtractStructureValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr, bool monitorMode);

long PutNTScalarValue(PVA_OVERALL *pva, long index);
long PutNTScalarArrayValue(PVA_OVERALL *pva, long index);
long PutNTEnumValue(PVA_OVERALL *pva, long index);
long PutScalarValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr);
long PutScalarArrayValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr);
long PutStructureValue(PVA_OVERALL *pva, long index, epics::pvData::PVFieldPtr PVFieldPtr);
