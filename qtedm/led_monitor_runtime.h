#pragma once

#include "single_channel_monitor_runtime_base.h"

class LedMonitorElement;

class LedMonitorRuntime
  : public SingleChannelMonitorRuntimeBase<LedMonitorElement>
{
public:
  explicit LedMonitorRuntime(LedMonitorElement *element)
    : SingleChannelMonitorRuntimeBase(element)
  {
  }
};
