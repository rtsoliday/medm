#pragma once

#include "single_channel_monitor_runtime_base.h"

class ThermometerElement;

class ThermometerRuntime
  : public SingleChannelMonitorRuntimeBase<ThermometerElement>
{
public:
  explicit ThermometerRuntime(ThermometerElement *element)
    : SingleChannelMonitorRuntimeBase(element)
  {
  }
};
