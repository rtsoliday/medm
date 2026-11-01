#pragma once

#include <QVariant>
#include <QWidget>

/* MEDM unmanages a channel's Motif widget while the channel is disconnected
 * and paints a white rectangle at its object instead, in display order, so
 * later dynamic graphics draw over it. The widget-backed elements record that
 * state in this property; the display and composites stack an unmanaged
 * element with the graphics in its file position. */
inline constexpr char kMedmWidgetUnmanagedProperty[] = "medmWidgetUnmanaged";

inline void setMedmWidgetUnmanaged(QWidget &widget, bool unmanaged)
{
  if (widget.property(kMedmWidgetUnmanagedProperty).toBool() != unmanaged) {
    widget.setProperty(kMedmWidgetUnmanagedProperty, unmanaged);
  }
}

inline bool isMedmWidgetUnmanaged(const QWidget *widget)
{
  return widget && widget->property(kMedmWidgetUnmanagedProperty).toBool();
}

/* Records the state when it leaves scope, so every return path of a
 * function that changes it is covered. */
template <typename State>
class MedmWidgetUnmanagedUpdate
{
public:
  MedmWidgetUnmanagedUpdate(QWidget &widget, State state)
    : widget_(widget)
    , state_(state)
  {
  }

  ~MedmWidgetUnmanagedUpdate()
  {
    setMedmWidgetUnmanaged(widget_, state_());
  }

  MedmWidgetUnmanagedUpdate(const MedmWidgetUnmanagedUpdate &) = delete;
  MedmWidgetUnmanagedUpdate &operator=(const MedmWidgetUnmanagedUpdate &)
      = delete;

private:
  QWidget &widget_;
  State state_;
};
