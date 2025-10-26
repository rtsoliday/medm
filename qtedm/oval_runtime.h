#pragma once

#include "graphic_element_runtime_base.h"

class OvalElement;
class DisplayWindow;

/* Runtime component for OvalElement that handles EPICS Channel Access
 * for dynamic visibility and color modes.
 *
 * This class now inherits from GraphicElementRuntimeBase which provides all
 * the channel management, subscription, and state evaluation logic. */
class OvalRuntime : public GraphicElementRuntimeBase<OvalElement>
{
  friend class DisplayWindow;
public:
  explicit OvalRuntime(OvalElement *element)
    : GraphicElementRuntimeBase<OvalElement>(element) {}
};
