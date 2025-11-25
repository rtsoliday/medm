#pragma once

#include "graphic_element_runtime_base.h"

class TextElement;

/* Runtime controller for text elements with EPICS Channel Access support.
 *
 * Extends GraphicElementRuntimeBase to add statistics tracking for text elements.
 */
class TextRuntime : public GraphicElementRuntimeBase<TextElement>
{
  friend class DisplayWindow;

public:
  explicit TextRuntime(TextElement *element);
  ~TextRuntime() override = default;

protected:
  /* Override to add statistics tracking */
  void onStart() override;
  void onStop() override;
  void onChannelConnected(int channelIndex) override;
  void onChannelDisconnected(int channelIndex) override;

  /* Override to provide element type name for warning messages */
  const char *elementTypeName() const override { return "text element"; }
};
