#pragma once

#include <array>

#include <QColor>
#include <QEvent>
#include <QList>
#include <QPointer>
#include <QWidget>

#include "text_properties.h"

class RelatedDisplayElement;

class CompositeElement : public QWidget
{
public:
  explicit CompositeElement(QWidget *parent = nullptr);
  ~CompositeElement() override;

  void setSelected(bool selected);
  bool isSelected() const;

  QString compositeName() const;
  void setCompositeName(const QString &name);
  bool hasExplicitCompositeName() const;
  void setHasExplicitCompositeName(bool hasExplicitName);

  QString compositeFile() const;
  void setCompositeFile(const QString &filePath);
  /* The size of the bounding box of a composite file's objects, which
   * MEDM makes the composite's object; empty for other composites. */
  QSize fileObjectSize() const;
  void setFileObjectSize(const QSize &size);

  QColor foregroundColor() const;
  void setForegroundColor(const QColor &color);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor &color);

  TextColorMode colorMode() const;
  void setColorMode(TextColorMode mode);

  TextVisibilityMode visibilityMode() const;
  void setVisibilityMode(TextVisibilityMode mode);

  QString visibilityCalc() const;
  void setVisibilityCalc(const QString &calc);

  QString channel(int index) const;
  void setChannel(int index, const QString &value);

  std::array<QString, 5> channels() const;

  void adoptChild(QWidget *child);
  QList<QWidget *> childWidgets() const;

  void setExecuteMode(bool execute);
  
  void setChannelConnected(bool connected);
  bool isChannelConnected() const;
  void setRuntimeVisible(bool visible);
  void setVisible(bool visible) override;
  
  /* keepCurrentBounds grows the composite around its children without
   * shrinking it below its current rectangle, which stays the area painted
   * white while the channel is disconnected. */
  void expandToFitChildren(bool keepCurrentBounds = false);

  /* MEDM draws the children of a composite with a channel from the
   * composite's update task, whose GC clips to the composite's object, so
   * its static graphics, and those of composites without a channel inside
   * it, show only within that object. Widgets and children with their own
   * channel draw through their own update tasks and are not clipped. This
   * masks those static graphics to the object after a growing fit in
   * execute mode, and removes the masks otherwise. */
  void clipStaticGraphicsToObject();

  /* MEDM draws the static graphics of a composite on the display's
   * background with the other static graphics, in display order, and its
   * dynamic graphics over them. These move the given children to a sibling
   * that keeps the composite's geometry, for the display to stack with its
   * static graphics, and back. The stipples of the given hidden buttons,
   * which MEDM draws there too, are painted on that sibling, in their place
   * in display order, while the buttons stay to take clicks. */
  QWidget *splitStaticChildren(const QList<QWidget *> &children,
      const QList<RelatedDisplayElement *> &hiddenButtons);
  void mergeStaticChildren();
  QWidget *staticChildLayer() const;

protected:
  void paintEvent(QPaintEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QColor defaultForegroundColor() const;
  QColor defaultBackgroundColor() const;
  bool forwardMouseEventToParent(QMouseEvent *event) const;
  bool hasActiveChannel() const;
  void updateMouseTransparency();
  void applyRuntimeVisibility();
  void raiseCompositeHierarchy();
  void scheduleChildStackingRefresh();
  bool isMedmWidgetBackedChildWidget(const QWidget *child) const;
  bool isStaticChildWidget(const QWidget *child) const;
  bool isDynamicGraphicChildWidget(const QWidget *child) const;
  void clipStaticGraphicsOf(const CompositeElement *composite);
  void refreshChildStackingOrder();

  bool selected_ = false;
  QString compositeName_;
  QString compositeFile_;
  QColor foregroundColor_;
  QColor backgroundColor_;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 5> channels_{};
  QList<QPointer<QWidget>> childWidgets_;
  bool executeMode_ = false;
  bool channelConnected_ = false;
  bool runtimeVisible_ = true;
  bool designModeVisible_ = true;
  bool childStackingRefreshPending_ = false;
  bool childStackingOrderInternallyUpdating_ = false;
  /* The order the last restack applied and the child list it left. */
  QList<QWidget *> appliedChildStackingOrder_;
  QObjectList appliedChildStackingChildren_;
  bool hasExplicitCompositeName_ = false;
  /* The ADL object within the composite after a growing fit, or null. */
  QRect objectRect_;
  QSize fileObjectSize_;
  /* Static graphics masked to objectRect_ by clipStaticGraphicsToObject(). */
  QList<QPointer<QWidget>> clippedStaticGraphics_;
  QPointer<QWidget> staticChildLayer_;
  /* The static children and hidden button stipples on staticChildLayer_,
   * in display order. */
  QList<QPointer<QWidget>> staticLayerOrder_;
};
