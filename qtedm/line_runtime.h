#pragma once

#include "graphic_element_runtime_base.h"

class LineElement;

class LineRuntime : public GraphicElementRuntimeBase<LineElement>
{
public:
  explicit LineRuntime(LineElement *element)
    : GraphicElementRuntimeBase(element)
  {
  }
};
