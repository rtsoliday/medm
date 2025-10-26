#pragma once

#include "graphic_element_runtime_base.h"

class ArcElement;

class ArcRuntime : public GraphicElementRuntimeBase<ArcElement>
{
public:
  explicit ArcRuntime(ArcElement *element)
    : GraphicElementRuntimeBase(element)
  {
  }
};
