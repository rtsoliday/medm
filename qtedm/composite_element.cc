#include "composite_element.h"

#include <climits>

#include <QApplication>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QDebug>
#include <QTimer>

#include "text_element.h"
#include "text_entry_element.h"
#include "choice_button_element.h"
#include "slider_element.h"
#include "wheel_switch_element.h"
#include "rectangle_element.h"
#include "oval_element.h"
#include "arc_element.h"
#include "line_element.h"
#include "polyline_element.h"
#include "polygon_element.h"
#include "image_element.h"
#include "menu_element.h"
#include "message_button_element.h"
#include "shell_command_element.h"
#include "related_display_element.h"
#include "text_monitor_element.h"
#include "meter_element.h"
#include "bar_monitor_element.h"
#include "scale_monitor_element.h"
#include "byte_monitor_element.h"
#include "strip_chart_element.h"
#include "cartesian_plot_element.h"

namespace {

constexpr int kCompositeGraphicChannelCount = 5;
constexpr char kWidgetHasDynamicAttributeProperty[] =
    "_adlHasDynamicAttribute";

template <typename ElementType>
bool hasDynamicGraphicAttributes(const ElementType *element)
{
  if (!element) {
    return false;
  }
  if (element->colorMode() != TextColorMode::kStatic) {
    return true;
  }
  const TextVisibilityMode visibilityMode = element->visibilityMode();
  if (visibilityMode != TextVisibilityMode::kStatic) {
    if (visibilityMode != TextVisibilityMode::kCalc
        || !element->visibilityCalc().trimmed().isEmpty()) {
      return true;
    }
  }
  for (int i = 0; i < kCompositeGraphicChannelCount; ++i) {
    if (!element->channel(i).trimmed().isEmpty()) {
      return true;
    }
  }
  return false;
}

bool widgetHasDynamicAttribute(const QWidget *widget)
{
  if (!widget) {
    return false;
  }
  return widget->property(kWidgetHasDynamicAttributeProperty).toBool();
}

bool isControlChildWidget(const QWidget *child)
{
  return dynamic_cast<const TextEntryElement *>(child)
      || dynamic_cast<const SliderElement *>(child)
      || dynamic_cast<const WheelSwitchElement *>(child)
      || dynamic_cast<const ChoiceButtonElement *>(child)
      || dynamic_cast<const MenuElement *>(child)
      || dynamic_cast<const MessageButtonElement *>(child)
      || dynamic_cast<const ShellCommandElement *>(child)
      || dynamic_cast<const RelatedDisplayElement *>(child);
}

bool isMonitorChildWidget(const QWidget *child)
{
  return dynamic_cast<const TextMonitorElement *>(child)
      || dynamic_cast<const MeterElement *>(child)
      || dynamic_cast<const BarMonitorElement *>(child)
      || dynamic_cast<const ScaleMonitorElement *>(child)
      || dynamic_cast<const ByteMonitorElement *>(child)
      || dynamic_cast<const StripChartElement *>(child)
      || dynamic_cast<const CartesianPlotElement *>(child);
}

} // namespace

CompositeElement::CompositeElement(QWidget *parent)
  : QWidget(parent)
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_MouseNoMask, true);
  /* Clear any clipping mask to allow children to extend beyond bounds */
  clearMask();
  /* Ensure no content margins that might offset children */
  setContentsMargins(0, 0, 0, 0);
  designModeVisible_ = QWidget::isVisible();
  runtimeVisible_ = true;
  setExecuteMode(false);
  updateMouseTransparency();
  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();
}

void CompositeElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool CompositeElement::isSelected() const
{
  return selected_;
}

QString CompositeElement::compositeName() const
{
  return compositeName_;
}

void CompositeElement::setCompositeName(const QString &name)
{
  compositeName_ = name;
  if (name.trimmed().isEmpty()) {
    hasExplicitCompositeName_ = false;
  }
}

bool CompositeElement::hasExplicitCompositeName() const
{
  return hasExplicitCompositeName_;
}

void CompositeElement::setHasExplicitCompositeName(bool hasExplicitName)
{
  hasExplicitCompositeName_ = hasExplicitName;
}

QString CompositeElement::compositeFile() const
{
  return compositeFile_;
}

void CompositeElement::setCompositeFile(const QString &filePath)
{
  compositeFile_ = filePath;
}

QColor CompositeElement::foregroundColor() const
{
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return defaultForegroundColor();
}

void CompositeElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  update();
}

QColor CompositeElement::backgroundColor() const
{
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackgroundColor();
}

void CompositeElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  update();
}

TextColorMode CompositeElement::colorMode() const
{
  return colorMode_;
}

void CompositeElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode CompositeElement::visibilityMode() const
{
  return visibilityMode_;
}

void CompositeElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString CompositeElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void CompositeElement::setVisibilityCalc(const QString &calc)
{
  visibilityCalc_ = calc;
}

QString CompositeElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[static_cast<std::size_t>(index)];
}

void CompositeElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  channels_[static_cast<std::size_t>(index)] = value;
  updateMouseTransparency();
}

std::array<QString, 5> CompositeElement::channels() const
{
  return channels_;
}

void CompositeElement::adoptChild(QWidget *child)
{
  if (!child) {
    return;
  }
  if (child->parentWidget() != this) {
    child->setParent(this);
  }
  if (!childWidgets_.contains(child)) {
    childWidgets_.append(QPointer<QWidget>(child));
    child->installEventFilter(this);
  }
  refreshChildStackingOrder();
}

void CompositeElement::expandToFitChildren()
{
  if (childWidgets_.isEmpty()) {
    return;
  }

  /*
   * Calculate the bounding box that encompasses all child widgets,
   * similar to how MEDM handles composite bounds (medmComposite.c).
   */
  int minX = INT_MAX;
  int minY = INT_MAX;
  int maxX = INT_MIN;
  int maxY = INT_MIN;

  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child) {
      continue;
    }

    /* Get child geometry relative to this composite */
    QRect childGeom = child->geometry();
    if (auto *textChild = dynamic_cast<TextElement *>(child)) {
      const QRect textBounds = textChild->visualBoundsRelativeToParent();
      if (textBounds.isValid()) {
        childGeom = textBounds;
      }
    }
    int childX = childGeom.x();
    int childY = childGeom.y();
    int childRight = childX + childGeom.width();
    int childBottom = childY + childGeom.height();

    if (childX < minX) minX = childX;
    if (childY < minY) minY = childY;
    if (childRight > maxX) maxX = childRight;
    if (childBottom > maxY) maxY = childBottom;
  }

  /* Nothing to do if no valid children found */
  if (minX == INT_MAX || minY == INT_MAX) {
    return;
  }

  /*
   * Adjust composite geometry to encompass all children.
   * Store current position to calculate offset.
   */
  QRect currentGeom = geometry();
  int oldX = currentGeom.x();
  int oldY = currentGeom.y();

  /* New composite bounds based on children */
  int newX = oldX + minX;
  int newY = oldY + minY;
  int newWidth = maxX - minX;
  int newHeight = maxY - minY;

  /* Update composite geometry */
  setGeometry(newX, newY, newWidth, newHeight);

  /*
   * Adjust all child positions relative to new composite origin.
   * Since composite moved by (minX, minY), shift children by (-minX, -minY).
   */
  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child) {
      continue;
    }
    QRect childGeom = child->geometry();
    child->setGeometry(childGeom.x() - minX, childGeom.y() - minY,
                      childGeom.width(), childGeom.height());
  }
}

QList<QWidget *> CompositeElement::childWidgets() const
{
  QList<QWidget *> result;
  for (const auto &pointer : childWidgets_) {
    if (QWidget *widget = pointer.data()) {
      result.append(widget);
    }
  }
  return result;
}

void CompositeElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QWidget::isVisible();
  } else {
    QWidget::setVisible(designModeVisible_);
  }

  executeMode_ = execute;
  updateMouseTransparency();

  /* Propagate the execute mode to ALL children FIRST
   * so they update their internal state before we modify visibility. */
  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child || child == this) {
      continue;
    }

    if (auto *elem = dynamic_cast<TextElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<CompositeElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<ChoiceButtonElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<SliderElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<RectangleElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<OvalElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<ArcElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<LineElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<PolylineElement *>(child)) {
      elem->setExecuteMode(execute);
    } else if (auto *elem = dynamic_cast<PolygonElement *>(child)) {
      elem->setExecuteMode(execute);
    }
  }

  applyRuntimeVisibility();
  update();

  refreshChildStackingOrder();
}

void CompositeElement::setChannelConnected(bool connected)
{
  if (channelConnected_ == connected) {
    return;
  }
  const bool wasVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  channelConnected_ = connected;
  applyRuntimeVisibility();
  update();
  const bool nowVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  if (!wasVisible && nowVisible) {
    raiseCompositeHierarchy();
  }
}

bool CompositeElement::isChannelConnected() const
{
  return channelConnected_;
}

void CompositeElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  const bool wasVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  runtimeVisible_ = visible;
  applyRuntimeVisibility();
  update();
  const bool nowVisible = executeMode_ && designModeVisible_ && runtimeVisible_
      && (channelConnected_ || !hasActiveChannel());
  if (!wasVisible && nowVisible) {
    raiseCompositeHierarchy();
  }
}

void CompositeElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  /* In execute mode with a channel defined but not connected, fill with white */
  if (executeMode_ && hasActiveChannel() && !channelConnected_) {
    painter.fillRect(rect(), Qt::white);
    if (selected_) {
      QPen pen(Qt::black);
      pen.setStyle(Qt::DashLine);
      pen.setWidth(1);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    return;
  }

  /* Draw selection border if selected */
  if (selected_) {
    QPen pen(foregroundColor());
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

QColor CompositeElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor CompositeElement::defaultBackgroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

bool CompositeElement::hasActiveChannel() const
{
  for (const auto &channel : channels_) {
    if (!channel.trimmed().isEmpty()) {
      return true;
    }
  }
  return false;
}

void CompositeElement::updateMouseTransparency()
{
  /* In execute mode, always allow mouse events so child widgets can receive
   * them (for cursors, tooltips, interaction). In edit mode, be transparent
   * so clicks select/manipulate the composite itself rather than children. */
  setAttribute(Qt::WA_TransparentForMouseEvents, !executeMode_);
}

void CompositeElement::mousePressEvent(QMouseEvent *event)
{
  // Forward middle button and right-click events to parent window for PV info functionality
  if (executeMode_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

bool CompositeElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  if (!event) {
    return false;
  }
  QWidget *target = window();
  if (!target) {
    return false;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF globalPosF = event->globalPosition();
  const QPoint globalPoint = globalPosF.toPoint();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos, globalPosF,
      event->button(), event->buttons(), event->modifiers());
#else
  const QPoint globalPoint = event->globalPos();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos,
      QPointF(globalPoint), event->button(), event->buttons(),
      event->modifiers());
#endif
  QCoreApplication::sendEvent(target, &forwarded);
  return true;
}

void CompositeElement::applyRuntimeVisibility()
{
  if (!executeMode_) {
    QWidget::setVisible(designModeVisible_);
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(true);
        child->show();
      }
    }
    return;
  }

  const bool hasChannel = hasActiveChannel();
  if (!hasChannel) {
    QWidget::setVisible(designModeVisible_);
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(designModeVisible_);
        if (!designModeVisible_) {
          child->hide();
        }
      }
    }
    return;
  }

  if (!channelConnected_) {
    QWidget::setVisible(designModeVisible_);
    for (const auto &pointer : childWidgets_) {
      if (QWidget *child = pointer.data()) {
        child->setVisible(false);
        child->hide();
      }
    }
    QTimer::singleShot(0, this, [this]() {
      if (!executeMode_ || channelConnected_ || !hasActiveChannel()) {
        return;
      }
      for (const auto &pointer : childWidgets_) {
        if (QWidget *child = pointer.data()) {
          child->hide();
        }
      }
    });
    return;
  }

  const bool show = designModeVisible_ && runtimeVisible_;
  QWidget::setVisible(show);
  for (const auto &pointer : childWidgets_) {
    if (QWidget *child = pointer.data()) {
      child->setVisible(show);
      if (!show) {
        child->hide();
      }
    }
  }
}

void CompositeElement::raiseCompositeHierarchy()
{
  raise();
  refreshChildStackingOrder();
}

/*
 * Child Widget Stacking Order
 * ===========================
 *
 * MEDM renders display elements in ADL (ASCII Display List) declaration order,
 * with later elements drawn on top of earlier ones. QtEDM uses Qt's widget
 * stacking to achieve a similar effect, but must account for Qt's different
 * rendering model.
 *
 * QtEDM classifies child widgets into three categories for stacking purposes:
 *
 * 1. STATIC WIDGETS (raised first, at bottom of stack)
 *    - Graphic elements with no dynamic attributes (no visibility rules,
 *      no color mode changes, no channel connections)
 *    - Composites that contain ONLY static graphic children
 *    - Examples: plain rectangles, static text labels, decorative shapes
 *
 * 2. DYNAMIC WIDGETS (raised second, middle of stack)
 *    - Graphic elements with dynamic attributes (visibility rules like
 *      "if not zero", alarm-sensitive color modes, channel connections)
 *    - Composites that contain ANY dynamic graphic children
 *    - Text elements with channel connections (even without visibility rules)
 *    - Examples: rectangles that show/hide based on PV values, alarm indicators
 *
 * 3. INTERACTIVE WIDGETS (raised last, top of stack)
 *    - Control widgets: text entries, sliders, buttons, menus, related displays
 *    - Monitor widgets: text updates, meters, bar graphs, strip charts
 *    - These must be on top so users can interact with them
 *
 * IMPORTANT: Composites are containers, not controls. A composite's stacking
 * category is determined by its GRAPHIC content, not by whether it contains
 * controls. Controls inside a composite are managed by that composite's own
 * internal stacking order.
 *
 * Example: A composite containing a related display + static rectangle is
 * classified as STATIC (not interactive), because its graphic content (the
 * rectangle) has no dynamic attributes. The related display inside will be
 * raised to the top within that composite's internal stacking.
 *
 * Within each category, widgets maintain their ADL declaration order.
 */

bool CompositeElement::isStaticChildWidget(const QWidget *child) const
{
  if (!child) {
    return false;
  }

  if (const auto *composite = dynamic_cast<const CompositeElement *>(child)) {
    /*
     * A composite is STATIC if it contains NO dynamic graphic children.
     *
     * IMPORTANT: Controls (related displays, buttons, text entries, etc.)
     * inside a composite do NOT affect whether the composite is static or
     * dynamic. Controls are handled by the composite's own internal stacking
     * order - they will be raised to the top within that composite.
     *
     * This allows patterns like:
     *   composite {
     *     related display (invisible)  <- control, ignored for classification
     *     rectangle (clr=57)           <- static graphic
     *   }
     * to be correctly classified as STATIC, so sibling composites with
     * dynamic rectangles will stack on top of it.
     */
    const QList<QWidget *> children = composite->childWidgets();
    for (QWidget *grandChild : children) {
      if (isDynamicGraphicChildWidget(grandChild)) {
        return false;
      }
    }
    return true;
  }

  /* These are the primitive graphic element types that can be static */
  return dynamic_cast<const RectangleElement *>(child)
      || dynamic_cast<const ImageElement *>(child)
      || dynamic_cast<const OvalElement *>(child)
      || dynamic_cast<const ArcElement *>(child)
      || dynamic_cast<const LineElement *>(child)
      || dynamic_cast<const PolylineElement *>(child)
      || dynamic_cast<const PolygonElement *>(child)
      || dynamic_cast<const TextElement *>(child);
}

bool CompositeElement::isDynamicGraphicChildWidget(const QWidget *child) const
{
  if (!child) {
    return false;
  }

  /* Check for the _adlHasDynamicAttribute property set during parsing */
  if (widgetHasDynamicAttribute(child)) {
    return true;
  }

  /*
   * Check primitive graphic elements for dynamic attributes.
   * An element is dynamic if it has:
   *   - Non-static visibility mode (if zero, if not zero, calc)
   *   - Non-static color mode (alarm-sensitive)
   *   - Any channel connections
   */
  if (const auto *rectangle = dynamic_cast<const RectangleElement *>(child)) {
    return hasDynamicGraphicAttributes(rectangle);
  }
  if (const auto *image = dynamic_cast<const ImageElement *>(child)) {
    return hasDynamicGraphicAttributes(image);
  }
  if (const auto *oval = dynamic_cast<const OvalElement *>(child)) {
    return hasDynamicGraphicAttributes(oval);
  }
  if (const auto *arc = dynamic_cast<const ArcElement *>(child)) {
    return hasDynamicGraphicAttributes(arc);
  }
  if (const auto *line = dynamic_cast<const LineElement *>(child)) {
    return hasDynamicGraphicAttributes(line);
  }
  if (const auto *polyline = dynamic_cast<const PolylineElement *>(child)) {
    return hasDynamicGraphicAttributes(polyline);
  }
  if (const auto *polygon = dynamic_cast<const PolygonElement *>(child)) {
    return hasDynamicGraphicAttributes(polygon);
  }
  if (const auto *text = dynamic_cast<const TextElement *>(child)) {
    return hasDynamicGraphicAttributes(text);
  }

  /*
   * Check if the child is a composite with dynamic content.
   *
   * A composite is DYNAMIC if:
   *   1. It has its own dynamic attributes (visibility/color mode), OR
   *   2. It contains ANY dynamic graphic children (recursively)
   *
   * This ensures composites with dynamic content are stacked in the dynamic
   * layer along with sibling dynamic elements, maintaining proper ADL order
   * within that layer.
   *
   * Example of why this matters:
   *   Level 4 composite contains:
   *     - Level 5a: composite with static rectangle (clr=57)    <- STATIC
   *     - Level 5b: composite with dynamic rectangles (clr=16)  <- DYNAMIC
   *     - Level 5c: composite with dynamic rectangles (clr=20)  <- DYNAMIC
   *
   *   Without this check, Level 5b and 5c would fall through to interactive,
   *   breaking the layering. With this check, they're correctly identified
   *   as dynamic and stacked above Level 5a.
   */
  if (const auto *composite = dynamic_cast<const CompositeElement *>(child)) {
    /* Check if composite has its own dynamic attributes */
    if (composite->visibilityMode() != TextVisibilityMode::kStatic
        || composite->colorMode() != TextColorMode::kStatic) {
      return true;
    }
    for (int i = 0; i < 5; ++i) {
      if (!composite->channel(i).trimmed().isEmpty()) {
        return true;
      }
    }
    /* Recursively check if any children are dynamic */
    const QList<QWidget *> children = composite->childWidgets();
    for (QWidget *grandChild : children) {
      if (isDynamicGraphicChildWidget(grandChild)) {
        return true;
      }
    }
  }
  return false;
}

void CompositeElement::refreshChildStackingOrder()
{
  if (childStackingOrderInternallyUpdating_) {
    return;
  }
  childStackingOrderInternallyUpdating_ = true;

  QList<QWidget *> staticWidgets;
  QList<QWidget *> dynamicWidgets;
  QList<QWidget *> interactiveWidgets;

  /*
   * Classify each child widget into one of three stacking categories.
   * See the comment block above isStaticChildWidget() for detailed
   * explanation of the classification logic.
   */
  for (const auto &pointer : childWidgets_) {
    QWidget *child = pointer.data();
    if (!child) {
      continue;
    }

    /* Controls (buttons, text entries, etc.) always go to interactive layer */
    if (isControlChildWidget(child)) {
      interactiveWidgets.append(child);
      continue;
    }

    /* Check for dynamic attributes or monitor widgets */
    if (widgetHasDynamicAttribute(child)
        || isMonitorChildWidget(child)
        || isDynamicGraphicChildWidget(child)) {
      dynamicWidgets.append(child);
    } else if (isStaticChildWidget(child)) {
      staticWidgets.append(child);
    } else {
      /*
       * Fallback: anything not classified goes to interactive layer.
       * This should rarely happen - most widgets should be caught by
       * the checks above. If new widget types are added, they should
       * be explicitly handled in the classification functions.
       */
      interactiveWidgets.append(child);
    }
  }

  /*
   * Apply stacking order by raising widgets in category order.
   * Qt's raise() moves a widget to the top of its siblings' stack.
   * By raising in order (static, dynamic, interactive), we ensure:
   *   - Static widgets are at the bottom
   *   - Dynamic widgets are in the middle (above static)
   *   - Interactive widgets are at the top (always accessible)
   *
   * Within each category, widgets are raised in their original ADL order,
   * so later declarations end up on top of earlier ones.
   */
  for (QWidget *widget : staticWidgets) {
    if (widget) {
      widget->raise();
    }
  }
  for (QWidget *widget : dynamicWidgets) {
    if (widget) {
      widget->raise();
    }
  }
  for (QWidget *widget : interactiveWidgets) {
    if (widget) {
      widget->raise();
    }
  }

  childStackingOrderInternallyUpdating_ = false;
}

void CompositeElement::scheduleChildStackingRefresh()
{
  if (childStackingOrderInternallyUpdating_) {
    return;
  }
  if (childStackingRefreshPending_) {
    return;
  }
  childStackingRefreshPending_ = true;
  QTimer::singleShot(0, this, [this]() {
    childStackingRefreshPending_ = false;
    refreshChildStackingOrder();
  });
}

bool CompositeElement::eventFilter(QObject *watched, QEvent *event)
{
  if (!watched || !event) {
    return QWidget::eventFilter(watched, event);
  }

  if (!childStackingOrderInternallyUpdating_) {
    switch (event->type()) {
    case QEvent::ShowToParent:
    case QEvent::HideToParent:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
      scheduleChildStackingRefresh();
      break;
    default:
      break;
    }
  }

  return QWidget::eventFilter(watched, event);
}
