#pragma once

#include "graphic_element_runtime_base.h"

class PolygonElement;

class PolygonRuntime : public GraphicElementRuntimeBase<PolygonElement>
{
public:
  explicit PolygonRuntime(PolygonElement *element)
    : GraphicElementRuntimeBase(element)
  {
  }
};
