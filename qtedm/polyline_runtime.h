#pragma once

#include "graphic_element_runtime_base.h"

class PolylineElement;

class PolylineRuntime : public GraphicElementRuntimeBase<PolylineElement>
{
public:
  explicit PolylineRuntime(PolylineElement *element)
    : GraphicElementRuntimeBase(element)
  {
  }
};
