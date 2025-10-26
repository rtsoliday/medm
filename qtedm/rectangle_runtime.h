#pragma once

#include "graphic_element_runtime_base.h"

class RectangleElement;
class DisplayWindow;

/* Runtime component for RectangleElement that handles EPICS Channel Access
 * for dynamic visibility and color modes.
 *
 * This class now inherits from GraphicElementRuntimeBase which provides all
 * the channel management, subscription, and state evaluation logic. */
class RectangleRuntime : public GraphicElementRuntimeBase<RectangleElement>
{
  friend class DisplayWindow;
public:
  explicit RectangleRuntime(RectangleElement *element)
    : GraphicElementRuntimeBase<RectangleElement>(element) {}
};
