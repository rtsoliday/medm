#include "text_runtime.h"

#include "statistics_tracker.h"
#include "text_element.h"

TextRuntime::TextRuntime(TextElement *element)
  : GraphicElementRuntimeBase<TextElement>(element)
{
}

void TextRuntime::onStart()
{
  /* Register with statistics tracker */
  StatisticsTracker::instance().registerDisplayObjectStarted();
}

void TextRuntime::onStop()
{
  /* Unregister from statistics tracker */
  StatisticsTracker::instance().registerDisplayObjectStopped();
}

void TextRuntime::onChannelCreated(int channelIndex)
{
  (void)channelIndex;
  StatisticsTracker::instance().registerChannelCreated();
}

void TextRuntime::onChannelConnected(int channelIndex)
{
  (void)channelIndex;
  StatisticsTracker::instance().registerChannelConnected();
}

void TextRuntime::onChannelDisconnected(int channelIndex)
{
  (void)channelIndex;
  StatisticsTracker::instance().registerChannelDisconnected();
}

void TextRuntime::onChannelDestroyed(int channelIndex)
{
  (void)channelIndex;
  StatisticsTracker::instance().registerChannelDestroyed();
}
