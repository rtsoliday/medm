#include "text_runtime.h"

#include "statistics_tracker.h"
#include "text_element.h"

TextRuntime::TextRuntime(TextElement *element)
  : GraphicElementRuntimeBase<TextElement>(element)
{
}

void TextRuntime::onStart()
{
  if (!channelsNeeded() && element()) {
    const int channelCount = static_cast<int>(channels().size());
    for (int i = 0; i < channelCount; ++i) {
      if (!element()->channel(i).trimmed().isEmpty()) {
        setLayeringNeeded(true);
        break;
      }
    }
  }

  /* Register with statistics tracker */
  StatisticsTracker::instance().registerDisplayObjectStarted();
}

void TextRuntime::onStop()
{
  /* Unregister from statistics tracker */
  StatisticsTracker::instance().registerDisplayObjectStopped();
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
