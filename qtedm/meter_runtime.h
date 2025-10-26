#pragma once

#include "single_channel_monitor_runtime_base.h"

class MeterElement;

class MeterRuntime : public SingleChannelMonitorRuntimeBase<MeterElement>
{
public:
  explicit MeterRuntime(MeterElement *element)
    : SingleChannelMonitorRuntimeBase(element)
  {
  }
};