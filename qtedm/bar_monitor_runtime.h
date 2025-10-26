#pragma once

#include "single_channel_monitor_runtime_base.h"

class BarMonitorElement;

class BarMonitorRuntime : public SingleChannelMonitorRuntimeBase<BarMonitorElement>
{
public:
  explicit BarMonitorRuntime(BarMonitorElement *element)
    : SingleChannelMonitorRuntimeBase(element)
  {
  }
};