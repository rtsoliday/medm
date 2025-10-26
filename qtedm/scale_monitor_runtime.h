#pragma once

#include "single_channel_monitor_runtime_base.h"

class ScaleMonitorElement;

class ScaleMonitorRuntime : public SingleChannelMonitorRuntimeBase<ScaleMonitorElement>
{
public:
  explicit ScaleMonitorRuntime(ScaleMonitorElement *element)
    : SingleChannelMonitorRuntimeBase(element)
  {
  }
};