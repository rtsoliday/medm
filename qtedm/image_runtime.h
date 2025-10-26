#pragma once

#include <QByteArray>

#include "graphic_element_runtime_base.h"

class ImageElement;

/* Runtime controller for image elements with EPICS Channel Access support.
 *
 * Extends GraphicElementRuntimeBase to add image-specific functionality:
 * - Frame selection via calc expression
 * - Animation support for multi-frame images
 */
class ImageRuntime : public GraphicElementRuntimeBase<ImageElement>
{
  friend class DisplayWindow;

public:
  explicit ImageRuntime(ImageElement *element);
  ~ImageRuntime() override = default;

protected:
  /* Override to add image-specific initialization */
  void onStart() override;

  /* Override to add image-specific cleanup */
  void onStop() override;

  /* Override to add frame selection evaluation */
  void onStateEvaluated() override;

private:
  void evaluateFrameSelection();
  bool evaluateImageCalc(double &result) const;

  QByteArray imageCalcPostfix_;
  bool imageCalcValid_ = false;
  bool hasImageCalcExpression_ = false;
  bool animate_ = false;
};
