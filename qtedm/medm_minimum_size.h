#pragma once

#include <QSize>
#include <QWidget>

/* MEDM's Xc BarGraph (with decorations) and Indicator widgets are never
 * smaller than 20x20 pixels (MIN_BG_WIDTH/HEIGHT, MIN_INDICATOR_WIDTH/HEIGHT
 * in xc/), so MEDM draws a smaller display object larger, growing right and
 * down from its origin.  This applies that minimum in execute mode while
 * remembering the size the display asked for, so the widget returns to it
 * when the minimum no longer applies. */
class MedmMinimumSize
{
public:
  static constexpr int kXcMinimumExtent = 20;

  /* Call from resizeEvent(): a size this helper did not set is a new
   * request from the display. */
  void noteResize(const QWidget &widget)
  {
    if (widget.size() != appliedSize_) {
      requestedSize_ = widget.size();
    }
  }

  /* Grow to the Xc minimum when enforce is true; otherwise return to the
   * requested size. */
  void apply(QWidget &widget, bool enforce)
  {
    /* Hidden widgets defer resizeEvent(). A display can therefore assign
     * its geometry and call a state setter before noteResize() runs. */
    if (!requestedSize_.isValid() || widget.size() != appliedSize_) {
      requestedSize_ = widget.size();
    }
    QSize target = requestedSize_;
    if (enforce) {
      target = target.expandedTo(QSize(kXcMinimumExtent, kXcMinimumExtent));
    }
    appliedSize_ = target;
    if (target != widget.size()) {
      widget.resize(target);
    }
  }

private:
  QSize requestedSize_;
  QSize appliedSize_;
};
