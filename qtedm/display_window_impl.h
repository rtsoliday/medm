#pragma once

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

inline void setUtf8Encoding(QTextStream &stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Utf8);
#else
  stream.setCodec("UTF-8");
#endif
}

class DisplayAreaWidget : public QWidget
{
public:
  explicit DisplayAreaWidget(QWidget *parent = nullptr)
    : QWidget(parent)
  {
    setAutoFillBackground(true);
    gridColor_ = palette().color(QPalette::WindowText);
  }

  void setSelected(bool selected)
  {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    update();
  }

  void setGridOn(bool gridOn)
  {
    if (gridOn_ == gridOn) {
      return;
    }
    gridOn_ = gridOn;
    update();
  }

  bool gridOn() const
  {
    return gridOn_;
  }

  void setGridSpacing(int spacing)
  {
    const int clampedSpacing = std::max(kMinimumGridSpacing, spacing);
    if (gridSpacing_ == clampedSpacing) {
      return;
    }
    gridSpacing_ = clampedSpacing;
    if (gridOn_) {
      update();
    }
  }

  int gridSpacing() const
  {
    return gridSpacing_;
  }

  void setGridColor(const QColor &color)
  {
    if (!color.isValid() || gridColor_ == color) {
      return;
    }
    gridColor_ = color;
    if (gridOn_) {
      update();
    }
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    QWidget::paintEvent(event);

    if (gridOn_ && gridSpacing_ > 0) {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing, false);
      QPen gridPen(gridColor_);
      gridPen.setWidth(1);
      painter.setPen(gridPen);

      const QRect canvas = rect();
      const int width = canvas.width();
      const int height = canvas.height();
      for (int x = 0; x < width; x += gridSpacing_) {
        for (int y = 0; y < height; y += gridSpacing_) {
          painter.drawPoint(canvas.left() + x, canvas.top() + y);
        }
      }
    }

    if (!selected_) {
      return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(Qt::black);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    const QRect borderRect = rect().adjusted(0, 0, -1, -1);
    painter.drawRect(borderRect);
  }

private:
  bool selected_ = false;
  bool gridOn_ = kDefaultGridOn;
  int gridSpacing_ = kDefaultGridSpacing;
  QColor gridColor_ = Qt::black;
};







class DisplayWindow : public QMainWindow
{
public:
  DisplayWindow(const QPalette &displayPalette, const QPalette &uiPalette,
      const QFont &font, const QFont &labelFont,
      std::weak_ptr<DisplayState> state, QWidget *parent = nullptr)
    : QMainWindow(parent)
    , state_(std::move(state))
    , labelFont_(labelFont)
    , resourcePaletteBase_(uiPalette)
  {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("qtedmDisplayWindow"));
    setWindowTitle(QStringLiteral("newDisplay.adl"));
    setFont(font);
    setAutoFillBackground(true);
    setPalette(displayPalette);

    displayArea_ = new DisplayAreaWidget;
    displayArea_->setObjectName(QStringLiteral("displayArea"));
    displayArea_->setAutoFillBackground(true);
    displayArea_->setPalette(displayPalette);
    displayArea_->setBackgroundRole(QPalette::Window);
    displayArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    displayArea_->setMinimumSize(kDefaultDisplayWidth, kDefaultDisplayHeight);
    displayArea_->setGridSpacing(gridSpacing_);
    displayArea_->setGridOn(gridOn_);
    displayArea_->setGridColor(displayPalette.color(QPalette::WindowText));
    setCentralWidget(displayArea_);

    resize(kDefaultDisplayWidth, kDefaultDisplayHeight);
    setFocusPolicy(Qt::StrongFocus);

    auto *cutShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_X), this);
    cutShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(cutShortcut, &QShortcut::activated, this,
        [this]() {
          setAsActiveDisplay();
          cutSelection();
        });
    auto *copyShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(copyShortcut, &QShortcut::activated, this,
        [this]() {
          setAsActiveDisplay();
          copySelection();
        });
    auto *pasteShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_V), this);
    pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(pasteShortcut, &QShortcut::activated, this,
        [this]() {
          setAsActiveDisplay();
          pasteSelection();
        });
    updateDirtyIndicator();
  }

  int gridSpacing() const
  {
    return gridSpacing_;
  }

  void setGridSpacing(int spacing)
  {
    const int clampedSpacing = std::max(kMinimumGridSpacing, spacing);
    if (gridSpacing_ == clampedSpacing) {
      return;
    }
    gridSpacing_ = clampedSpacing;
    if (displayArea_) {
      displayArea_->setGridSpacing(gridSpacing_);
    }
    markDirty();
  }

  bool isGridOn() const
  {
    return gridOn_;
  }

  void setGridOn(bool gridOn)
  {
    if (gridOn_ == gridOn) {
      return;
    }
    gridOn_ = gridOn;
    if (displayArea_) {
      displayArea_->setGridOn(gridOn_);
    }
    markDirty();
  }

  void syncCreateCursor()
  {
    updateCreateCursor();
  }

  void clearSelection()
  {
    clearSelections();
    notifyMenus();
  }

  void selectAllElements()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    clearSelections();

    QList<QWidget *> widgets;
    auto appendVisible = [&](const auto &list) {
      for (auto *element : list) {
        if (element && element->isVisible()) {
          widgets.append(element);
        }
      }
    };

    appendVisible(textElements_);
    appendVisible(textEntryElements_);
    appendVisible(sliderElements_);
    appendVisible(wheelSwitchElements_);
    appendVisible(choiceButtonElements_);
    appendVisible(menuElements_);
    appendVisible(messageButtonElements_);
    appendVisible(shellCommandElements_);
    appendVisible(relatedDisplayElements_);
    appendVisible(textMonitorElements_);
    appendVisible(meterElements_);
    appendVisible(barMonitorElements_);
    appendVisible(scaleMonitorElements_);
    appendVisible(stripChartElements_);
    appendVisible(cartesianPlotElements_);
    appendVisible(byteMonitorElements_);
    appendVisible(rectangleElements_);
    appendVisible(imageElements_);
    appendVisible(ovalElements_);
    appendVisible(arcElements_);
    appendVisible(lineElements_);
    appendVisible(polylineElements_);
    appendVisible(polygonElements_);
    appendVisible(compositeElements_);

    if (widgets.isEmpty()) {
      notifyMenus();
      return;
    }

    if (widgets.size() == 1) {
      selectWidgetForEditing(widgets.front());
    } else {
      multiSelection_.clear();
      for (QWidget *widget : widgets) {
        setWidgetSelectionState(widget, true);
        multiSelection_.append(QPointer<QWidget>(widget));
      }
      showResourcePaletteForMultipleSelection();
    }

    notifyMenus();
  }

  void selectDisplayElement()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    clearSelections();
    if (!ensureResourcePalette()) {
      notifyMenus();
      return;
    }

    for (auto &display : state->displays) {
      if (!display.isNull() && display != this) {
        display->clearSelections();
      }
    }

    setDisplaySelected(true);
    showResourcePaletteForDisplay();
    notifyMenus();
  }

  void cutSelection()
  {
    copySelectionInternal(true);
  }

  void copySelection()
  {
    copySelectionInternal(false);
  }

  void pasteSelection()
  {
    pasteFromClipboard();
  }

  void raiseSelection()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    const QList<QWidget *> widgets = selectedWidgetsInStackOrder(true);
    if (widgets.isEmpty()) {
      return;
    }

    bool reordered = false;
    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      for (auto it = elementStack_.begin(); it != elementStack_.end(); ++it) {
        QWidget *current = it->data();
        if (current == widget) {
          QPointer<QWidget> pointer = *it;
          elementStack_.erase(it);
          elementStack_.append(pointer);
          widget->raise();
          reordered = true;
          break;
        }
      }
    }

    if (reordered) {
      markDirty();
      notifyMenus();
    }
  }

  void lowerSelection()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    const QList<QWidget *> widgets = selectedWidgetsInStackOrder(false);
    if (widgets.isEmpty()) {
      return;
    }

    bool reordered = false;
    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      for (auto it = elementStack_.begin(); it != elementStack_.end(); ++it) {
        QWidget *current = it->data();
        if (current == widget) {
          QPointer<QWidget> pointer = *it;
          elementStack_.erase(it);
          elementStack_.prepend(pointer);
          widget->lower();
          reordered = true;
          break;
        }
      }
    }

    if (reordered) {
      markDirty();
      notifyMenus();
    }
  }

  void groupSelectedElements()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return;
    }

    const QList<QWidget *> orderedSelection = selectedWidgetsInStackOrder(true);
    if (orderedSelection.isEmpty()) {
      return;
    }

    QList<QWidget *> candidates;
    candidates.reserve(orderedSelection.size());
    QSet<QWidget *> candidateSet;
    for (QWidget *widget : orderedSelection) {
      if (!widget || widget == displayArea_) {
        continue;
      }
      if (widget->parentWidget() != displayArea_) {
        return;
      }
      if (candidateSet.contains(widget)) {
        continue;
      }
      candidateSet.insert(widget);
      candidates.append(widget);
    }

    if (candidates.size() < 2) {
      return;
    }

    QRect compositeRect;
    for (QWidget *widget : candidates) {
      const QPoint topLeft = widget->mapTo(displayArea_, QPoint(0, 0));
      const QRect mappedRect(topLeft, widget->size());
      compositeRect = compositeRect.isValid() ? compositeRect.united(mappedRect)
                                              : mappedRect;
    }
    if (!compositeRect.isValid() || compositeRect.isEmpty()) {
      return;
    }

    auto *composite = new CompositeElement(displayArea_);
    composite->setGeometry(compositeRect);
    composite->show();

    int insertIndex = elementStack_.size();
    for (int i = 0; i < elementStack_.size(); ++i) {
      QWidget *current = elementStack_.at(i).data();
      if (current && candidateSet.contains(current)) {
        insertIndex = std::min(insertIndex, i);
      }
    }
    if (insertIndex < 0 || insertIndex > elementStack_.size()) {
      insertIndex = elementStack_.size();
    }

    for (QWidget *widget : candidates) {
      const QPoint currentTopLeft = widget->mapTo(displayArea_, QPoint(0, 0));
      const QPoint relativePos = currentTopLeft - compositeRect.topLeft();
      removeElementFromStack(widget);
      widget->setParent(composite);
      widget->move(relativePos);
      widget->show();
      composite->adoptChild(widget);
    }

    elementStack_.insert(insertIndex, QPointer<QWidget>(composite));
    if (insertIndex == 0) {
      composite->lower();
    } else {
      QWidget *above = nullptr;
      for (int i = insertIndex + 1; i < elementStack_.size(); ++i) {
        QWidget *candidate = elementStack_.at(i).data();
        if (candidate) {
          above = candidate;
          break;
        }
      }
      if (above) {
        composite->stackUnder(above);
      } else {
        composite->raise();
      }
    }

    compositeElements_.append(composite);

    clearSelections();
    selectCompositeElement(composite);
    showResourcePaletteForComposite(composite);
    markDirty();
    refreshResourcePaletteGeometry();
    notifyMenus();
  }

  void ungroupSelectedElements()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return;
    }

    QSet<CompositeElement *> composites;
    for (const auto &pointer : multiSelection_) {
      if (auto *widget = pointer.data()) {
        if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
          composites.insert(composite);
        }
      }
    }
    if (selectedCompositeElement_) {
      composites.insert(selectedCompositeElement_);
    }
    if (composites.isEmpty()) {
      return;
    }

    for (CompositeElement *composite : std::as_const(composites)) {
      if (!composite || composite->parentWidget() != displayArea_) {
        continue;
      }

      int stackIndex = elementStack_.size();
      for (int i = 0; i < elementStack_.size(); ++i) {
        if (elementStack_.at(i).data() == composite) {
          stackIndex = i;
          break;
        }
      }

      const QPoint compositeOrigin = composite->mapTo(displayArea_, QPoint(0, 0));
      const QList<QWidget *> children = composite->childWidgets();

      removeElementFromStack(composite);
      compositeElements_.removeAll(composite);

      int insertIndex = stackIndex;
      for (QWidget *child : children) {
        if (!child) {
          continue;
        }
        const QRect localGeometry = child->geometry();
        const QRect newGeometry(compositeOrigin + localGeometry.topLeft(),
            localGeometry.size());
        child->setParent(displayArea_);
        child->setGeometry(newGeometry);
        child->show();
        elementStack_.insert(insertIndex, QPointer<QWidget>(child));
        ++insertIndex;
      }

      composite->deleteLater();
    }

    clearSelections();
    markDirty();
    refreshResourcePaletteGeometry();
    notifyMenus();
  }

  bool canRaiseSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    return !selectedWidgets().isEmpty();
  }

  bool canLowerSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    return !selectedWidgets().isEmpty();
  }

  bool canGroupSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return false;
    }

    int candidateCount = 0;
    for (QWidget *widget : selectedWidgets()) {
      if (!widget || widget == displayArea_) {
        continue;
      }
      if (widget->parentWidget() != displayArea_) {
        return false;
      }
      ++candidateCount;
      if (candidateCount >= 2) {
        return true;
      }
    }
    return false;
  }

  bool canUngroupSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return false;
    }
    for (QWidget *widget : selectedWidgets()) {
      if (dynamic_cast<CompositeElement *>(widget)) {
        return true;
      }
    }
    return false;
  }

  bool hasCopyableSelection() const
  {
    return hasAnyElementSelection();
  }

  bool canPaste() const
  {
    auto state = state_.lock();
    return state && state->editMode && state->clipboard && state->clipboard->isValid();
  }

  bool save(QWidget *dialogParent = nullptr);
  bool saveAs(QWidget *dialogParent = nullptr);
  bool loadFromFile(const QString &filePath, QString *errorMessage = nullptr);
  QString filePath() const
  {
    return filePath_;
  }

  bool isDirty() const
  {
    return dirty_;
  }

  bool hasFilePath() const
  {
    return !filePath_.isEmpty();
  }

protected:
  void focusInEvent(QFocusEvent *event) override
  {
    QMainWindow::focusInEvent(event);
    setAsActiveDisplay();
  }

  void closeEvent(QCloseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override
  {
    setAsActiveDisplay();
    if (event->button() == Qt::MiddleButton) {
      if (auto state = state_.lock(); state && state->editMode
          && state->createTool == CreateTool::kNone) {
        QWidget *hitWidget = elementAt(event->pos());
        if (!multiSelection_.isEmpty()) {
          if (hitWidget && isWidgetInMultiSelection(hitWidget)) {
            beginMiddleButtonDrag(event->pos());
            event->accept();
            return;
          }
          if (hitWidget && !isWidgetInMultiSelection(hitWidget)) {
            if (selectWidgetForEditing(hitWidget)) {
              beginMiddleButtonDrag(event->pos());
              event->accept();
              return;
            }
          }
          event->accept();
          return;
        }
        QWidget *selectedWidget = currentSelectedWidget();
        if (!selectedWidget && hitWidget) {
          if (selectWidgetForEditing(hitWidget)) {
            selectedWidget = currentSelectedWidget();
          }
        } else if (hitWidget && hitWidget != selectedWidget) {
          if (selectWidgetForEditing(hitWidget)) {
            selectedWidget = currentSelectedWidget();
          }
        }
        if (selectedWidget && hitWidget == selectedWidget) {
          beginMiddleButtonDrag(event->pos());
          event->accept();
          return;
        }
        if (!selectedWidget && !hitWidget) {
          event->accept();
          return;
        }
      }
    }
    if (event->button() == Qt::LeftButton) {
      if (auto state = state_.lock(); state && state->editMode) {
        if (state->createTool == CreateTool::kPolygon) {
          if (displayArea_) {
            const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
            if (displayArea_->rect().contains(areaPos)) {
              if (!polygonCreationActive_) {
                clearSelections();
              }
              handlePolygonClick(areaPos, event->modifiers());
            }
          }
          event->accept();
          return;
        }
        if (state->createTool == CreateTool::kPolyline) {
          if (displayArea_) {
            const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
            if (displayArea_->rect().contains(areaPos)) {
              if (!polylineCreationActive_) {
                clearSelections();
              }
              handlePolylineClick(areaPos, event->modifiers());
            }
          }
          event->accept();
          return;
        }
        if (state->createTool == CreateTool::kText
            || state->createTool == CreateTool::kTextMonitor
            || state->createTool == CreateTool::kTextEntry
            || state->createTool == CreateTool::kSlider
            || state->createTool == CreateTool::kWheelSwitch
            || state->createTool == CreateTool::kChoiceButton
            || state->createTool == CreateTool::kMenu
            || state->createTool == CreateTool::kMessageButton
            || state->createTool == CreateTool::kShellCommand
            || state->createTool == CreateTool::kMeter
            || state->createTool == CreateTool::kBarMonitor
            || state->createTool == CreateTool::kByteMonitor
            || state->createTool == CreateTool::kScaleMonitor
            || state->createTool == CreateTool::kStripChart
            || state->createTool == CreateTool::kCartesianPlot
            || state->createTool == CreateTool::kRectangle
            || state->createTool == CreateTool::kOval
            || state->createTool == CreateTool::kArc
            || state->createTool == CreateTool::kLine
            || state->createTool == CreateTool::kImage
            || state->createTool == CreateTool::kRelatedDisplay) {
          if (displayArea_) {
            const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
            if (displayArea_->rect().contains(areaPos)) {
              clearSelections();
              startCreateRubberBand(areaPos, state->createTool);
            }
          }
          event->accept();
          return;
        }
        if (state->createTool != CreateTool::kNone) {
          event->accept();
          return;
        }
        const QPoint areaPos = displayArea_
            ? displayArea_->mapFrom(this, event->pos())
            : QPoint();
        const bool insideDisplayArea = displayArea_
            && displayArea_->rect().contains(areaPos);

        if (QWidget *widget = elementAt(event->pos())) {
          if (selectWidgetForEditing(widget)) {
            event->accept();
            return;
          }
        }

        if (insideDisplayArea) {
          beginSelectionRubberBandPending(areaPos, event->pos());
          event->accept();
          return;
        }

        handleDisplayBackgroundClick();
        event->accept();
        return;
      }
    }

    if (event->button() == Qt::RightButton) {
      if (auto state = state_.lock(); state && state->editMode) {
  const QPoint globalPos =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      event->globalPosition().toPoint();
#else
      event->globalPos();
#endif
  lastContextMenuGlobalPos_ = globalPos;
  showEditContextMenu(globalPos);
        event->accept();
        return;
      }
    }

    QMainWindow::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (middleButtonDragActive_ && (event->buttons() & Qt::MiddleButton)) {
      updateMiddleButtonDrag(event->pos());
      event->accept();
      return;
    }
    if (polygonCreationActive_) {
      if (auto state = state_.lock(); state && state->editMode && displayArea_
          && state->createTool == CreateTool::kPolygon) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        updatePolygonPreview(areaPos, event->modifiers());
        event->accept();
        return;
      }
    }

    if (polylineCreationActive_) {
      if (auto state = state_.lock(); state && state->editMode && displayArea_
          && state->createTool == CreateTool::kPolyline) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        updatePolylinePreview(areaPos, event->modifiers());
        event->accept();
        return;
      }
    }

    if ((event->buttons() & Qt::LeftButton) && selectionRubberBandPending_) {
      const QPoint delta = event->pos() - selectionRubberBandStartWindowPos_;
      if (delta.manhattanLength() >= QApplication::startDragDistance()) {
        startSelectionRubberBand();
      }
    }

    if (rubberBandActive_) {
      if (auto state = state_.lock(); state && state->editMode && displayArea_) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        if (selectionRubberBandMode_) {
          updateSelectionRubberBand(areaPos);
        } else {
          updateCreateRubberBand(areaPos);
        }
        event->accept();
        return;
      }
    }

    QMainWindow::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::MiddleButton) {
      if (middleButtonDragActive_) {
        finishMiddleButtonDrag(true);
        event->accept();
        return;
      }
    }
    if (event->button() == Qt::LeftButton) {
      if (rubberBandActive_) {
        if (auto state = state_.lock(); state && state->editMode
            && displayArea_) {
          const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
          if (selectionRubberBandMode_) {
            finishSelectionRubberBand(areaPos);
          } else {
            finishCreateRubberBand(areaPos);
          }
          event->accept();
          return;
        }
      } else if (selectionRubberBandPending_) {
        selectionRubberBandPending_ = false;
        if (auto state = state_.lock(); state && state->editMode) {
          handleDisplayBackgroundClick();
        }
        event->accept();
        return;
      }
    }

    QMainWindow::mouseReleaseEvent(event);
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      if (auto state = state_.lock(); state && state->editMode
          && state->createTool == CreateTool::kPolygon && displayArea_) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        if (displayArea_->rect().contains(areaPos)) {
          handlePolygonDoubleClick(areaPos, event->modifiers());
          event->accept();
          return;
        }
      }
      if (auto state = state_.lock(); state && state->editMode
          && state->createTool == CreateTool::kPolyline && displayArea_) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        if (displayArea_->rect().contains(areaPos)) {
          handlePolylineDoubleClick(areaPos, event->modifiers());
          event->accept();
          return;
        }
      }
    }

    QMainWindow::mouseDoubleClickEvent(event);
  }

private:
  bool writeAdlFile(const QString &filePath) const;
  void clearAllElements();
  bool loadDisplaySection(const AdlNode &displayNode);
  TextElement *loadTextElement(const AdlNode &textNode);
  TextMonitorElement *loadTextMonitorElement(const AdlNode &textUpdateNode);
  TextEntryElement *loadTextEntryElement(const AdlNode &textEntryNode);
  SliderElement *loadSliderElement(const AdlNode &valuatorNode);
  WheelSwitchElement *loadWheelSwitchElement(const AdlNode &wheelNode);
  ChoiceButtonElement *loadChoiceButtonElement(const AdlNode &choiceNode);
  MenuElement *loadMenuElement(const AdlNode &menuNode);
  MessageButtonElement *loadMessageButtonElement(const AdlNode &messageNode);
  ShellCommandElement *loadShellCommandElement(const AdlNode &shellNode);
  RelatedDisplayElement *loadRelatedDisplayElement(const AdlNode &relatedNode);
  MeterElement *loadMeterElement(const AdlNode &meterNode);
  BarMonitorElement *loadBarMonitorElement(const AdlNode &barNode);
  ScaleMonitorElement *loadScaleMonitorElement(const AdlNode &indicatorNode);
  CartesianPlotElement *loadCartesianPlotElement(const AdlNode &cartesianNode);
  StripChartElement *loadStripChartElement(const AdlNode &stripNode);
  ByteMonitorElement *loadByteMonitorElement(const AdlNode &byteNode);
  ImageElement *loadImageElement(const AdlNode &imageNode);
  RectangleElement *loadRectangleElement(const AdlNode &rectangleNode);
  OvalElement *loadOvalElement(const AdlNode &ovalNode);
  ArcElement *loadArcElement(const AdlNode &arcNode);
  PolygonElement *loadPolygonElement(const AdlNode &polygonNode);
  PolylineElement *loadPolylineElement(const AdlNode &polylineNode);
  CompositeElement *loadCompositeElement(const AdlNode &compositeNode);
  bool loadElementNode(const AdlNode &node);
  QWidget *effectiveElementParent() const;
  std::optional<AdlNode> widgetToAdlNode(QWidget *widget) const;
  void setObjectGeometry(AdlNode &node, const QRect &rect) const;

  class ElementLoadContextGuard
  {
  public:
    ElementLoadContextGuard(DisplayWindow &window, QWidget *parent,
        const QPoint &offset, bool suppressRegistration,
        CompositeElement *composite)
      : window_(window)
      , previousParent_(window.currentElementParent_)
      , previousOffset_(window.currentElementOffset_)
      , previousSuppress_(window.suppressLoadRegistration_)
      , previousComposite_(window.currentCompositeOwner_)
    {
      window_.currentElementParent_ = parent;
      window_.currentElementOffset_ = offset;
      window_.suppressLoadRegistration_ = suppressRegistration;
      window_.currentCompositeOwner_ = composite;
    }

    ~ElementLoadContextGuard()
    {
      window_.currentElementParent_ = previousParent_;
      window_.currentElementOffset_ = previousOffset_;
      window_.suppressLoadRegistration_ = previousSuppress_;
      window_.currentCompositeOwner_ = previousComposite_;
    }

  private:
    DisplayWindow &window_;
    QWidget *previousParent_ = nullptr;
    QPoint previousOffset_;
    bool previousSuppress_ = false;
    CompositeElement *previousComposite_ = nullptr;
  };
  QRect parseObjectGeometry(const AdlNode &parent) const;
  bool parseAdlPoint(const QString &text, QPoint *point) const;
  QVector<QPoint> parsePolylinePoints(const AdlNode &polylineNode) const;
  void ensureElementInStack(QWidget *element);
  QColor colorForIndex(int index) const;
  QRect widgetDisplayRect(const QWidget *widget) const;
  void setWidgetDisplayRect(QWidget *widget, const QRect &displayRect) const;
  void writeWidgetAdl(QTextStream &stream, QWidget *widget, int indent,
      const std::function<QColor(const QWidget *, const QColor &)> &resolveForeground,
      const std::function<QColor(const QWidget *, const QColor &)> &resolveBackground) const;
  TextColorMode parseTextColorMode(const QString &value) const;
  TextVisibilityMode parseVisibilityMode(const QString &value) const;
  MeterLabel parseMeterLabel(const QString &value) const;
  TimeUnits parseTimeUnits(const QString &value) const;
  CartesianPlotStyle parseCartesianPlotStyle(const QString &value) const;
  CartesianPlotEraseMode parseCartesianEraseMode(const QString &value) const;
  CartesianPlotAxisStyle parseCartesianAxisStyle(const QString &value) const;
  CartesianPlotRangeStyle parseCartesianRangeStyle(const QString &value) const;
  CartesianPlotTimeFormat parseCartesianTimeFormat(const QString &value) const;
  BarDirection parseBarDirection(const QString &value) const;
  BarFill parseBarFill(const QString &value) const;
  ChoiceButtonStacking parseChoiceButtonStacking(const QString &value) const;
  RelatedDisplayVisual parseRelatedDisplayVisual(const QString &value) const;
  RelatedDisplayMode parseRelatedDisplayMode(const QString &value) const;
  void applyChannelProperties(const AdlNode &node,
    const std::function<void(int, const QString &)> &setter,
    int baseChannelIndex, int letterStartIndex) const;
  RectangleFill parseRectangleFill(const QString &value) const;
  RectangleLineStyle parseRectangleLineStyle(const QString &value) const;
  ImageType parseImageType(const QString &value) const;
  TextMonitorFormat parseTextMonitorFormat(const QString &value) const;
  PvLimitSource parseLimitSource(const QString &value) const;
  Qt::Alignment parseAlignment(const QString &value) const;
  void setAsActiveDisplay();
  void markDirty();
  void notifyMenus() const;
  void updateDirtyIndicator();

  std::weak_ptr<DisplayState> state_;
  QFont labelFont_;
  QPalette resourcePaletteBase_;
  QPointer<ResourcePaletteDialog> resourcePalette_;
  DisplayAreaWidget *displayArea_ = nullptr;
  QString filePath_;
  QString currentLoadDirectory_;
  QWidget *currentElementParent_ = nullptr;
  QPoint currentElementOffset_ = QPoint();
  bool suppressLoadRegistration_ = false;
  CompositeElement *currentCompositeOwner_ = nullptr;
  QString colormapName_;
  bool dirty_ = true;
  bool displaySelected_ = false;
  bool gridOn_ = kDefaultGridOn;
  int gridSpacing_ = kDefaultGridSpacing;
  QPoint lastContextMenuGlobalPos_;
  QList<TextElement *> textElements_;
  TextElement *selectedTextElement_ = nullptr;
  QList<TextEntryElement *> textEntryElements_;
  TextEntryElement *selectedTextEntryElement_ = nullptr;
  QList<SliderElement *> sliderElements_;
  SliderElement *selectedSliderElement_ = nullptr;
  QList<WheelSwitchElement *> wheelSwitchElements_;
  WheelSwitchElement *selectedWheelSwitchElement_ = nullptr;
  QList<ChoiceButtonElement *> choiceButtonElements_;
  ChoiceButtonElement *selectedChoiceButtonElement_ = nullptr;
  QList<MenuElement *> menuElements_;
  MenuElement *selectedMenuElement_ = nullptr;
  QList<MessageButtonElement *> messageButtonElements_;
  MessageButtonElement *selectedMessageButtonElement_ = nullptr;
  QList<ShellCommandElement *> shellCommandElements_;
  ShellCommandElement *selectedShellCommandElement_ = nullptr;
  QList<RelatedDisplayElement *> relatedDisplayElements_;
  RelatedDisplayElement *selectedRelatedDisplayElement_ = nullptr;
  QList<TextMonitorElement *> textMonitorElements_;
  TextMonitorElement *selectedTextMonitorElement_ = nullptr;
  QList<MeterElement *> meterElements_;
  MeterElement *selectedMeterElement_ = nullptr;
  QList<BarMonitorElement *> barMonitorElements_;
  BarMonitorElement *selectedBarMonitorElement_ = nullptr;
  QList<ScaleMonitorElement *> scaleMonitorElements_;
  ScaleMonitorElement *selectedScaleMonitorElement_ = nullptr;
  QList<StripChartElement *> stripChartElements_;
  StripChartElement *selectedStripChartElement_ = nullptr;
  QList<CartesianPlotElement *> cartesianPlotElements_;
  CartesianPlotElement *selectedCartesianPlotElement_ = nullptr;
  QList<ByteMonitorElement *> byteMonitorElements_;
  ByteMonitorElement *selectedByteMonitorElement_ = nullptr;
  QList<RectangleElement *> rectangleElements_;
  RectangleElement *selectedRectangle_ = nullptr;
  QList<ImageElement *> imageElements_;
  ImageElement *selectedImage_ = nullptr;
  QList<OvalElement *> ovalElements_;
  OvalElement *selectedOval_ = nullptr;
  QList<ArcElement *> arcElements_;
  ArcElement *selectedArc_ = nullptr;
  QList<LineElement *> lineElements_;
  LineElement *selectedLine_ = nullptr;
  QList<PolylineElement *> polylineElements_;
  PolylineElement *selectedPolyline_ = nullptr;
  QList<PolygonElement *> polygonElements_;
  PolygonElement *selectedPolygon_ = nullptr;
  QList<CompositeElement *> compositeElements_;
  CompositeElement *selectedCompositeElement_ = nullptr;
  bool polygonCreationActive_ = false;
  PolygonElement *activePolygonElement_ = nullptr;
  QVector<QPoint> polygonCreationPoints_;
  bool polylineCreationActive_ = false;
  PolylineElement *activePolylineElement_ = nullptr;
  QVector<QPoint> polylineCreationPoints_;
  QList<QPointer<QWidget>> multiSelection_;
  QList<QPointer<QWidget>> elementStack_;
  QRubberBand *rubberBand_ = nullptr;
  bool rubberBandActive_ = false;
  bool selectionRubberBandPending_ = false;
  bool selectionRubberBandMode_ = false;
  QPoint selectionRubberBandStartWindowPos_;
  QPoint pendingSelectionOrigin_;
  QPoint rubberBandOrigin_;
  CreateTool activeRubberBandTool_ = CreateTool::kNone;
  QList<QPointer<QWidget>> middleButtonDragWidgets_;
  QList<QRect> middleButtonInitialRects_;
  QRect middleButtonBoundingRect_;
  bool middleButtonDragActive_ = false;
  bool middleButtonDragMoved_ = false;
  QPoint middleButtonDragStartAreaPos_;

  void setDisplaySelected(bool selected)
  {
    if (displaySelected_ == selected) {
      return;
    }
    displaySelected_ = selected;
    if (displayArea_) {
      displayArea_->setSelected(selected);
    }
    update();
  }

  void clearDisplaySelection()
  {
    if (!displaySelected_) {
      return;
    }
    setDisplaySelected(false);
  }

  void clearTextSelection()
  {
    if (!selectedTextElement_) {
      return;
    }
    selectedTextElement_->setSelected(false);
    selectedTextElement_ = nullptr;
  }

  void clearTextEntrySelection()
  {
    if (!selectedTextEntryElement_) {
      return;
    }
    selectedTextEntryElement_->setSelected(false);
    selectedTextEntryElement_ = nullptr;
  }

  void clearSliderSelection()
  {
    if (!selectedSliderElement_) {
      return;
    }
    selectedSliderElement_->setSelected(false);
    selectedSliderElement_ = nullptr;
  }

  void clearWheelSwitchSelection()
  {
    if (!selectedWheelSwitchElement_) {
      return;
    }
    selectedWheelSwitchElement_->setSelected(false);
    selectedWheelSwitchElement_ = nullptr;
  }

  void clearChoiceButtonSelection()
  {
    if (!selectedChoiceButtonElement_) {
      return;
    }
    selectedChoiceButtonElement_->setSelected(false);
    selectedChoiceButtonElement_ = nullptr;
  }

  void clearMenuSelection()
  {
    if (!selectedMenuElement_) {
      return;
    }
    selectedMenuElement_->setSelected(false);
    selectedMenuElement_ = nullptr;
  }

  void clearMessageButtonSelection()
  {
    if (!selectedMessageButtonElement_) {
      return;
    }
    selectedMessageButtonElement_->setSelected(false);
    selectedMessageButtonElement_ = nullptr;
  }

  void clearShellCommandSelection()
  {
    if (!selectedShellCommandElement_) {
      return;
    }
    selectedShellCommandElement_->setSelected(false);
    selectedShellCommandElement_ = nullptr;
  }

  void clearRelatedDisplaySelection()
  {
    if (!selectedRelatedDisplayElement_) {
      return;
    }
    selectedRelatedDisplayElement_->setSelected(false);
    selectedRelatedDisplayElement_ = nullptr;
  }

  void clearTextMonitorSelection()
  {
    if (!selectedTextMonitorElement_) {
      return;
    }
    selectedTextMonitorElement_->setSelected(false);
    selectedTextMonitorElement_ = nullptr;
  }

  void clearMeterSelection()
  {
    if (!selectedMeterElement_) {
      return;
    }
    selectedMeterElement_->setSelected(false);
    selectedMeterElement_ = nullptr;
  }

  void clearScaleMonitorSelection()
  {
    if (!selectedScaleMonitorElement_) {
      return;
    }
    selectedScaleMonitorElement_->setSelected(false);
    selectedScaleMonitorElement_ = nullptr;
  }

  void clearStripChartSelection()
  {
    if (!selectedStripChartElement_) {
      return;
    }
    selectedStripChartElement_->setSelected(false);
    selectedStripChartElement_ = nullptr;
  }

  void clearCartesianPlotSelection()
  {
    if (!selectedCartesianPlotElement_) {
      return;
    }
    selectedCartesianPlotElement_->setSelected(false);
    selectedCartesianPlotElement_ = nullptr;
  }

  void clearBarMonitorSelection()
  {
    if (!selectedBarMonitorElement_) {
      return;
    }
    selectedBarMonitorElement_->setSelected(false);
    selectedBarMonitorElement_ = nullptr;
  }

  void clearByteMonitorSelection()
  {
    if (!selectedByteMonitorElement_) {
      return;
    }
    selectedByteMonitorElement_->setSelected(false);
    selectedByteMonitorElement_ = nullptr;
  }

  void clearRectangleSelection()
  {
    if (!selectedRectangle_) {
      return;
    }
    selectedRectangle_->setSelected(false);
    selectedRectangle_ = nullptr;
  }

  void clearImageSelection()
  {
    if (!selectedImage_) {
      return;
    }
    selectedImage_->setSelected(false);
    selectedImage_ = nullptr;
  }

  void clearOvalSelection()
  {
    if (!selectedOval_) {
      return;
    }
    selectedOval_->setSelected(false);
    selectedOval_ = nullptr;
  }

  void clearArcSelection()
  {
    if (!selectedArc_) {
      return;
    }
    selectedArc_->setSelected(false);
    selectedArc_ = nullptr;
  }

  void clearLineSelection()
  {
    if (!selectedLine_) {
      return;
    }
    selectedLine_->setSelected(false);
    selectedLine_ = nullptr;
  }

  void clearPolylineSelection()
  {
    if (!selectedPolyline_) {
      return;
    }
    selectedPolyline_->setSelected(false);
    selectedPolyline_ = nullptr;
  }

  void clearPolygonSelection()
  {
    if (!selectedPolygon_) {
      return;
    }
    selectedPolygon_->setSelected(false);
    selectedPolygon_ = nullptr;
  }

  void clearCompositeSelection()
  {
    if (!selectedCompositeElement_) {
      return;
    }
    selectedCompositeElement_->setSelected(false);
    selectedCompositeElement_ = nullptr;
  }

  void setWidgetSelectionState(QWidget *widget, bool selected)
  {
    if (!widget) {
      return;
    }
    if (auto *text = dynamic_cast<TextElement *>(widget)) {
      text->setSelected(selected);
      return;
    }
    if (auto *textEntry = dynamic_cast<TextEntryElement *>(widget)) {
      textEntry->setSelected(selected);
      return;
    }
    if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      slider->setSelected(selected);
      return;
    }
    if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
      wheel->setSelected(selected);
      return;
    }
    if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
      choice->setSelected(selected);
      return;
    }
    if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
      menu->setSelected(selected);
      return;
    }
    if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
      message->setSelected(selected);
      return;
    }
    if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
      shell->setSelected(selected);
      return;
    }
    if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
      related->setSelected(selected);
      return;
    }
    if (auto *textMonitor = dynamic_cast<TextMonitorElement *>(widget)) {
      textMonitor->setSelected(selected);
      return;
    }
    if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
      meter->setSelected(selected);
      return;
    }
    if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
      bar->setSelected(selected);
      return;
    }
    if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
      scale->setSelected(selected);
      return;
    }
    if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
      strip->setSelected(selected);
      return;
    }
    if (auto *cart = dynamic_cast<CartesianPlotElement *>(widget)) {
      cart->setSelected(selected);
      return;
    }
    if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
      byte->setSelected(selected);
      return;
    }
    if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      rectangle->setSelected(selected);
      return;
    }
    if (auto *image = dynamic_cast<ImageElement *>(widget)) {
      image->setSelected(selected);
      return;
    }
    if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      oval->setSelected(selected);
      return;
    }
    if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      arc->setSelected(selected);
      return;
    }
    if (auto *line = dynamic_cast<LineElement *>(widget)) {
      line->setSelected(selected);
      return;
    }
    if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      polyline->setSelected(selected);
      return;
    }
    if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      polygon->setSelected(selected);
      return;
    }
  }

  void clearMultiSelection()
  {
    if (multiSelection_.isEmpty()) {
      return;
    }
    for (const auto &pointer : multiSelection_) {
      if (QWidget *widget = pointer.data()) {
        setWidgetSelectionState(widget, false);
      }
    }
    multiSelection_.clear();
  }

  bool isWidgetInMultiSelection(QWidget *widget) const
  {
    if (!widget) {
      return false;
    }
    for (const auto &pointer : multiSelection_) {
      if (pointer.data() == widget) {
        return true;
      }
    }
    return false;
  }

  QList<QWidget *> selectedWidgets() const
  {
    QList<QWidget *> widgets;
    QSet<QWidget *> seen;
    auto appendUnique = [&](QWidget *candidate) {
      if (!candidate || seen.contains(candidate)) {
        return;
      }
      seen.insert(candidate);
      widgets.append(candidate);
    };

    for (const auto &pointer : multiSelection_) {
      appendUnique(pointer.data());
    }
    appendUnique(selectedTextElement_);
    appendUnique(selectedTextEntryElement_);
    appendUnique(selectedSliderElement_);
    appendUnique(selectedWheelSwitchElement_);
    appendUnique(selectedChoiceButtonElement_);
    appendUnique(selectedMenuElement_);
    appendUnique(selectedMessageButtonElement_);
    appendUnique(selectedShellCommandElement_);
    appendUnique(selectedRelatedDisplayElement_);
    appendUnique(selectedTextMonitorElement_);
    appendUnique(selectedMeterElement_);
    appendUnique(selectedBarMonitorElement_);
    appendUnique(selectedScaleMonitorElement_);
    appendUnique(selectedStripChartElement_);
    appendUnique(selectedCartesianPlotElement_);
    appendUnique(selectedByteMonitorElement_);
    appendUnique(selectedRectangle_);
    appendUnique(selectedImage_);
    appendUnique(selectedOval_);
    appendUnique(selectedArc_);
    appendUnique(selectedLine_);
    appendUnique(selectedPolyline_);
    appendUnique(selectedPolygon_);
    appendUnique(selectedCompositeElement_);

    return widgets;
  }

  QList<QWidget *> selectedWidgetsInStackOrder(bool ascending) const
  {
    const QList<QWidget *> selection = selectedWidgets();
    if (selection.isEmpty()) {
      return QList<QWidget *>();
    }

    QSet<QWidget *> selectionSet;
    selectionSet.reserve(selection.size());
    for (QWidget *widget : selection) {
      if (widget) {
        selectionSet.insert(widget);
      }
    }

    QList<QWidget *> ordered;
    ordered.reserve(selectionSet.size());
    if (ascending) {
      for (const auto &entry : elementStack_) {
        QWidget *widget = entry.data();
        if (widget && selectionSet.contains(widget)) {
          ordered.append(widget);
          selectionSet.remove(widget);
          if (selectionSet.isEmpty()) {
            break;
          }
        }
      }
    } else {
      for (auto it = elementStack_.crbegin(); it != elementStack_.crend(); ++it) {
        QWidget *widget = it->data();
        if (widget && selectionSet.contains(widget)) {
          ordered.append(widget);
          selectionSet.remove(widget);
          if (selectionSet.isEmpty()) {
            break;
          }
        }
      }
    }

    return ordered;
  }

  void clearSelections()
  {
    cancelMiddleButtonDrag();
    cancelSelectionRubberBand();
    clearMultiSelection();
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    closeResourcePalette();
  }

  template <typename ElementType>
  bool cutSelectedElement(QList<ElementType *> &elements,
      ElementType *&selected)
  {
    if (!selected) {
      return false;
    }

    ElementType *element = selected;
    selected = nullptr;
    element->setSelected(false);
    elements.removeAll(element);
    removeElementFromStack(element);
    element->deleteLater();
    return true;
  }

  bool copySelectionInternal(bool removeOriginal)
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }

    auto prepareClipboard = [&](auto &&fn) {
      if (!state->clipboard) {
        state->clipboard = std::make_shared<ClipboardContent>();
      }
      state->clipboard->paste = std::forward<decltype(fn)>(fn);
      state->clipboard->nextOffset = QPoint(10, 10);
      state->clipboard->hasPasted = false;
      notifyMenus();
    };

    auto finalizeCut = [this]() {
      clearSelections();
      if (displayArea_) {
        displayArea_->update();
      }
      markDirty();
      notifyMenus();
    };

    if (selectedCompositeElement_) {
      CompositeElement *element = selectedCompositeElement_;
  const QRect geometry = widgetDisplayRect(element);
      std::optional<AdlNode> node = widgetToAdlNode(element);
      if (!node) {
        return false;
      }

      prepareClipboard([geometry, node = std::move(*node)](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect desired = geometry.translated(offset);
        desired = target.adjustRectToDisplayArea(desired);
        AdlNode nodeCopy = node;
        target.setObjectGeometry(nodeCopy, desired);
        CompositeElement *newComposite = nullptr;
        {
          ElementLoadContextGuard guard(target, target.displayArea_, QPoint(),
              false, nullptr);
          newComposite = target.loadCompositeElement(nodeCopy);
        }
        if (newComposite) {
          target.selectCompositeElement(newComposite);
          target.showResourcePaletteForComposite(newComposite);
          target.markDirty();
        }
      });

      if (removeOriginal) {
        cutSelectedElement(compositeElements_, selectedCompositeElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedTextElement_) {
      TextElement *element = selectedTextElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QString text = element->text();
      const QColor foreground = element->foregroundColor();
      const Qt::Alignment alignment = element->textAlignment();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
      const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[i] = element->channel(i);
      }
      prepareClipboard([geometry, text, foreground, alignment, colorMode,
                           visibilityMode, visibilityCalc, channels](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        if (rect.height() < kMinimumTextElementHeight) {
          rect.setHeight(kMinimumTextElementHeight);
        }
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new TextElement(target.displayArea_);
        newElement->setFont(target.font());
        newElement->setGeometry(rect);
        newElement->setText(text);
        newElement->setForegroundColor(foreground);
        newElement->setTextAlignment(alignment);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int idx = 0; idx < static_cast<int>(channels.size()); ++idx) {
          newElement->setChannel(idx, channels[idx]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.textElements_.append(newElement);
        target.selectTextElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(textElements_, selectedTextElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedTextEntryElement_) {
      TextEntryElement *element = selectedTextEntryElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const TextMonitorFormat format = element->format();
      const int precision = element->precision();
      const PvLimitSource precisionSource = element->precisionSource();
      const int precisionDefault = element->precisionDefault();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, format,
                           precision, precisionSource, precisionDefault,
                           limits, channel](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new TextEntryElement(target.displayArea_);
        newElement->setFont(target.font());
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setFormat(format);
        newElement->setPrecision(precision);
        newElement->setPrecisionSource(precisionSource);
        newElement->setPrecisionDefault(precisionDefault);
        newElement->setLimits(limits);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.textEntryElements_.append(newElement);
        target.selectTextEntryElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(textEntryElements_, selectedTextEntryElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedSliderElement_) {
      SliderElement *element = selectedSliderElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const MeterLabel label = element->label();
      const BarDirection direction = element->direction();
      const double precision = element->precision();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, label,
                           direction, precision, limits, channel](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new SliderElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setLabel(label);
        newElement->setDirection(direction);
        newElement->setPrecision(precision);
        newElement->setLimits(limits);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.sliderElements_.append(newElement);
        target.selectSliderElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(sliderElements_, selectedSliderElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedWheelSwitchElement_) {
      WheelSwitchElement *element = selectedWheelSwitchElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const double precision = element->precision();
      const QString format = element->format();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, precision,
                           format, limits, channel](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new WheelSwitchElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setPrecision(precision);
        newElement->setFormat(format);
        newElement->setLimits(limits);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.wheelSwitchElements_.append(newElement);
        target.selectWheelSwitchElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(wheelSwitchElements_, selectedWheelSwitchElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedChoiceButtonElement_) {
      ChoiceButtonElement *element = selectedChoiceButtonElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const ChoiceButtonStacking stacking = element->stacking();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, stacking,
                           channel](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new ChoiceButtonElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setStacking(stacking);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.choiceButtonElements_.append(newElement);
        target.selectChoiceButtonElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(choiceButtonElements_, selectedChoiceButtonElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedMenuElement_) {
      MenuElement *element = selectedMenuElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, channel](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new MenuElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.menuElements_.append(newElement);
        target.selectMenuElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(menuElements_, selectedMenuElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedMessageButtonElement_) {
      MessageButtonElement *element = selectedMessageButtonElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const QString label = element->label();
      const QString pressMessage = element->pressMessage();
      const QString releaseMessage = element->releaseMessage();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, label,
                           pressMessage, releaseMessage, channel](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new MessageButtonElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setLabel(label);
        newElement->setPressMessage(pressMessage);
        newElement->setReleaseMessage(releaseMessage);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.messageButtonElements_.append(newElement);
        target.selectMessageButtonElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(messageButtonElements_,
            selectedMessageButtonElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedShellCommandElement_) {
      ShellCommandElement *element = selectedShellCommandElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const QString label = element->label();
      std::array<ShellCommandEntry, kShellCommandEntryCount> entries{};
      for (int i = 0; i < element->entryCount()
          && i < static_cast<int>(entries.size()); ++i) {
        entries[static_cast<std::size_t>(i)] = element->entry(i);
      }
      prepareClipboard([geometry, foreground, background, label, entries](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new ShellCommandElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setLabel(label);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
          newElement->setEntry(i, entries[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.shellCommandElements_.append(newElement);
        target.selectShellCommandElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(shellCommandElements_,
            selectedShellCommandElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedRelatedDisplayElement_) {
      RelatedDisplayElement *element = selectedRelatedDisplayElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const QString label = element->label();
      const RelatedDisplayVisual visual = element->visual();
      std::array<RelatedDisplayEntry, kRelatedDisplayEntryCount> entries{};
      for (int i = 0; i < element->entryCount()
          && i < static_cast<int>(entries.size()); ++i) {
        entries[static_cast<std::size_t>(i)] = element->entry(i);
      }
      prepareClipboard([geometry, foreground, background, label, visual,
                           entries](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new RelatedDisplayElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setLabel(label);
        newElement->setVisual(visual);
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
          newElement->setEntry(i, entries[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.relatedDisplayElements_.append(newElement);
        target.selectRelatedDisplayElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(relatedDisplayElements_,
            selectedRelatedDisplayElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedTextMonitorElement_) {
      TextMonitorElement *element = selectedTextMonitorElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QString text = element->text();
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const Qt::Alignment alignment = element->textAlignment();
      const TextColorMode colorMode = element->colorMode();
      const TextMonitorFormat format = element->format();
      const int precision = element->precision();
      const PvLimitSource precisionSource = element->precisionSource();
      const int precisionDefault = element->precisionDefault();
      const PvLimits limits = element->limits();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      prepareClipboard([geometry, text, foreground, background, alignment,
                           colorMode, format, precision, precisionSource,
                           precisionDefault, limits, channels](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new TextMonitorElement(target.displayArea_);
        newElement->setFont(target.font());
        newElement->setGeometry(rect);
        newElement->setText(text);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setTextAlignment(alignment);
        newElement->setColorMode(colorMode);
        newElement->setFormat(format);
        newElement->setPrecision(precision);
        newElement->setPrecisionSource(precisionSource);
        newElement->setPrecisionDefault(precisionDefault);
        newElement->setLimits(limits);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.textMonitorElements_.append(newElement);
        target.selectTextMonitorElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(textMonitorElements_, selectedTextMonitorElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedMeterElement_) {
      MeterElement *element = selectedMeterElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const MeterLabel label = element->label();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, label,
                           limits, channel](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new MeterElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setLabel(label);
        newElement->setLimits(limits);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.meterElements_.append(newElement);
        target.selectMeterElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(meterElements_, selectedMeterElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedBarMonitorElement_) {
      BarMonitorElement *element = selectedBarMonitorElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const MeterLabel label = element->label();
      const BarDirection direction = element->direction();
      const BarFill fillMode = element->fillMode();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, label,
                           direction, fillMode, limits, channel](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new BarMonitorElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setLabel(label);
        newElement->setDirection(direction);
        newElement->setFillMode(fillMode);
        newElement->setLimits(limits);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.barMonitorElements_.append(newElement);
        target.selectBarMonitorElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(barMonitorElements_, selectedBarMonitorElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedScaleMonitorElement_) {
      ScaleMonitorElement *element = selectedScaleMonitorElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const MeterLabel label = element->label();
      const BarDirection direction = element->direction();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, label,
                           direction, limits, channel](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new ScaleMonitorElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setLabel(label);
        newElement->setDirection(direction);
        newElement->setLimits(limits);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.scaleMonitorElements_.append(newElement);
        target.selectScaleMonitorElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(scaleMonitorElements_, selectedScaleMonitorElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedStripChartElement_) {
      StripChartElement *element = selectedStripChartElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const QString title = element->title();
      const QString xLabel = element->xLabel();
      const QString yLabel = element->yLabel();
      const double period = element->period();
      const TimeUnits units = element->units();
      const int penCount = element->penCount();
      std::array<QString, kStripChartPenCount> channels{};
      std::array<QColor, kStripChartPenCount> penColors{};
      std::array<PvLimits, kStripChartPenCount> penLimits{};
      for (int i = 0; i < penCount && i < kStripChartPenCount; ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
        penColors[static_cast<std::size_t>(i)] = element->penColor(i);
        penLimits[static_cast<std::size_t>(i)] = element->penLimits(i);
      }
      prepareClipboard([geometry, foreground, background, title, xLabel, yLabel,
                           period, units, penCount, channels, penColors,
                           penLimits](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new StripChartElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setTitle(title);
        newElement->setXLabel(xLabel);
        newElement->setYLabel(yLabel);
        newElement->setPeriod(period);
        newElement->setUnits(units);
        for (int i = 0; i < penCount && i < kStripChartPenCount; ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
          newElement->setPenColor(i, penColors[static_cast<std::size_t>(i)]);
          newElement->setPenLimits(i, penLimits[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.stripChartElements_.append(newElement);
        target.selectStripChartElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(stripChartElements_, selectedStripChartElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedCartesianPlotElement_) {
      CartesianPlotElement *element = selectedCartesianPlotElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const QString title = element->title();
      const QString xLabel = element->xLabel();
      std::array<QString, 4> yLabels{};
      for (int i = 0; i < static_cast<int>(yLabels.size()); ++i) {
        yLabels[static_cast<std::size_t>(i)] = element->yLabel(i);
      }
      const CartesianPlotStyle style = element->style();
      const bool eraseOldest = element->eraseOldest();
      const int count = element->count();
      const CartesianPlotEraseMode eraseMode = element->eraseMode();
      const QString triggerChannel = element->triggerChannel();
      const QString eraseChannel = element->eraseChannel();
      const QString countChannel = element->countChannel();
      const int traceCount = element->traceCount();
      std::array<QString, kCartesianPlotTraceCount> traceX{};
      std::array<QString, kCartesianPlotTraceCount> traceY{};
      std::array<QColor, kCartesianPlotTraceCount> traceColors{};
      std::array<CartesianPlotYAxis, kCartesianPlotTraceCount> traceAxes{};
      std::array<bool, kCartesianPlotTraceCount> traceRight{};
      for (int i = 0; i < traceCount && i < kCartesianPlotTraceCount; ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        traceX[idx] = element->traceXChannel(i);
        traceY[idx] = element->traceYChannel(i);
        traceColors[idx] = element->traceColor(i);
        traceAxes[idx] = element->traceYAxis(i);
        traceRight[idx] = element->traceUsesRightAxis(i);
      }
      prepareClipboard([geometry, foreground, background, title, xLabel, yLabels,
                           style, eraseOldest, count, eraseMode, triggerChannel,
                           eraseChannel, countChannel, traceCount, traceX,
                           traceY, traceColors, traceAxes, traceRight](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new CartesianPlotElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setTitle(title);
        newElement->setXLabel(xLabel);
        for (int i = 0; i < static_cast<int>(yLabels.size()); ++i) {
          newElement->setYLabel(i, yLabels[static_cast<std::size_t>(i)]);
        }
        newElement->setStyle(style);
        newElement->setEraseOldest(eraseOldest);
        newElement->setCount(count);
        newElement->setEraseMode(eraseMode);
        newElement->setTriggerChannel(triggerChannel);
        newElement->setEraseChannel(eraseChannel);
        newElement->setCountChannel(countChannel);
        for (int i = 0; i < traceCount && i < kCartesianPlotTraceCount; ++i) {
          const std::size_t idx = static_cast<std::size_t>(i);
          newElement->setTraceXChannel(i, traceX[idx]);
          newElement->setTraceYChannel(i, traceY[idx]);
          newElement->setTraceColor(i, traceColors[idx]);
          newElement->setTraceYAxis(i, traceAxes[idx]);
          newElement->setTraceUsesRightAxis(i, traceRight[idx]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.cartesianPlotElements_.append(newElement);
        target.selectCartesianPlotElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(cartesianPlotElements_,
            selectedCartesianPlotElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedByteMonitorElement_) {
      ByteMonitorElement *element = selectedByteMonitorElement_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor foreground = element->foregroundColor();
      const QColor background = element->backgroundColor();
      const TextColorMode colorMode = element->colorMode();
      const BarDirection direction = element->direction();
      const int startBit = element->startBit();
      const int endBit = element->endBit();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, direction,
                           startBit, endBit, channel](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new ByteMonitorElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setDirection(direction);
        newElement->setStartBit(startBit);
        newElement->setEndBit(endBit);
        newElement->setChannel(channel);
        newElement->show();
        target.ensureElementInStack(newElement);
        target.byteMonitorElements_.append(newElement);
        target.selectByteMonitorElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(byteMonitorElements_, selectedByteMonitorElement_);
        finalizeCut();
      }
      return true;
    }

    if (selectedRectangle_) {
      RectangleElement *element = selectedRectangle_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor color = element->color();
      const RectangleFill fill = element->fill();
      const RectangleLineStyle lineStyle = element->lineStyle();
      const int lineWidth = element->lineWidth();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
      const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      prepareClipboard([geometry, color, fill, lineStyle, lineWidth, colorMode,
                           visibilityMode, visibilityCalc, channels](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new RectangleElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(color);
        newElement->setFill(fill);
        newElement->setLineStyle(lineStyle);
        newElement->setLineWidth(lineWidth);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.rectangleElements_.append(newElement);
        target.selectRectangleElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(rectangleElements_, selectedRectangle_);
        finalizeCut();
      }
      return true;
    }

    if (selectedImage_) {
      ImageElement *element = selectedImage_;
  const QRect geometry = widgetDisplayRect(element);
      const ImageType imageType = element->imageType();
      const QString imageName = element->imageName();
      const QString calc = element->calc();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
  const QString visibilityCalc = element->visibilityCalc();
      const QString baseDirectory = element->baseDirectory();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      prepareClipboard([geometry, imageType, imageName, calc, colorMode,
                           visibilityMode, visibilityCalc, channels,
                           baseDirectory](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new ImageElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setImageType(imageType);
        if (!baseDirectory.isEmpty()) {
          newElement->setBaseDirectory(baseDirectory);
        } else if (!target.filePath_.isEmpty()) {
          newElement->setBaseDirectory(
              QFileInfo(target.filePath_).absolutePath());
        }
        newElement->setImageName(imageName);
        newElement->setCalc(calc);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.imageElements_.append(newElement);
        target.selectImageElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(imageElements_, selectedImage_);
        finalizeCut();
      }
      return true;
    }

    if (selectedOval_) {
      OvalElement *element = selectedOval_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor color = element->color();
      const RectangleFill fill = element->fill();
      const RectangleLineStyle lineStyle = element->lineStyle();
      const int lineWidth = element->lineWidth();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
  const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      prepareClipboard([geometry, color, fill, lineStyle, lineWidth, colorMode,
                           visibilityMode, visibilityCalc, channels](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new OvalElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(color);
        newElement->setFill(fill);
        newElement->setLineStyle(lineStyle);
        newElement->setLineWidth(lineWidth);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.ovalElements_.append(newElement);
        target.selectOvalElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(ovalElements_, selectedOval_);
        finalizeCut();
      }
      return true;
    }

    if (selectedArc_) {
      ArcElement *element = selectedArc_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor color = element->color();
      const RectangleFill fill = element->fill();
      const RectangleLineStyle lineStyle = element->lineStyle();
      const int lineWidth = element->lineWidth();
      const int beginAngle = element->beginAngle();
      const int pathAngle = element->pathAngle();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
  const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      prepareClipboard([geometry, color, fill, lineStyle, lineWidth, beginAngle,
                           pathAngle, colorMode, visibilityMode, visibilityCalc,
                           channels](DisplayWindow &target,
                           const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        auto *newElement = new ArcElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(color);
        newElement->setFill(fill);
        newElement->setLineStyle(lineStyle);
        newElement->setLineWidth(lineWidth);
        newElement->setBeginAngle(beginAngle);
        newElement->setPathAngle(pathAngle);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.arcElements_.append(newElement);
        target.selectArcElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(arcElements_, selectedArc_);
        finalizeCut();
      }
      return true;
    }

    if (selectedLine_) {
      LineElement *element = selectedLine_;
  const QRect geometry = widgetDisplayRect(element);
      const QColor color = element->color();
      const RectangleLineStyle lineStyle = element->lineStyle();
      const int lineWidth = element->lineWidth();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
  const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      const QVector<QPoint> points = element->absolutePoints();
      prepareClipboard([geometry, color, lineStyle, lineWidth, colorMode,
                           visibilityMode, visibilityCalc, channels, points](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = geometry.translated(offset);
        rect = target.adjustRectToDisplayArea(rect);
        QVector<QPoint> translatedPoints = points;
        const QPoint translation = rect.topLeft() - geometry.topLeft();
        for (QPoint &pt : translatedPoints) {
          pt += translation;
        }
        auto *newElement = new LineElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(color);
        newElement->setLineStyle(lineStyle);
        newElement->setLineWidth(lineWidth);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        if (translatedPoints.size() >= 2) {
          newElement->setLocalEndpoints(
              translatedPoints[0] - rect.topLeft(),
              translatedPoints[1] - rect.topLeft());
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.lineElements_.append(newElement);
        target.selectLineElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(lineElements_, selectedLine_);
        finalizeCut();
      }
      return true;
    }

    if (selectedPolyline_) {
      PolylineElement *element = selectedPolyline_;
      const QColor color = element->color();
      const RectangleLineStyle lineStyle = element->lineStyle();
      const int lineWidth = element->lineWidth();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
  const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      const QVector<QPoint> points = element->absolutePoints();
      prepareClipboard([color, lineStyle, lineWidth, colorMode,
                           visibilityMode, visibilityCalc, channels, points](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QVector<QPoint> translated = points;
        for (QPoint &pt : translated) {
          pt += offset;
        }
        auto *newElement = new PolylineElement(target.displayArea_);
        newElement->setAbsolutePoints(translated);
        newElement->setForegroundColor(color);
        newElement->setLineStyle(lineStyle);
        newElement->setLineWidth(lineWidth);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.polylineElements_.append(newElement);
        target.selectPolylineElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(polylineElements_, selectedPolyline_);
        finalizeCut();
      }
      return true;
    }

    if (selectedPolygon_) {
      PolygonElement *element = selectedPolygon_;
      const QColor color = element->color();
      const RectangleFill fill = element->fill();
      const RectangleLineStyle lineStyle = element->lineStyle();
      const int lineWidth = element->lineWidth();
      const TextColorMode colorMode = element->colorMode();
      const TextVisibilityMode visibilityMode = element->visibilityMode();
  const QString visibilityCalc = element->visibilityCalc();
      std::array<QString, 5> channels{};
      for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        channels[static_cast<std::size_t>(i)] = element->channel(i);
      }
      const QVector<QPoint> points = element->absolutePoints();
      prepareClipboard([color, fill, lineStyle, lineWidth, colorMode,
                           visibilityMode, visibilityCalc, channels, points](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QVector<QPoint> translated = points;
        for (QPoint &pt : translated) {
          pt += offset;
        }
        auto *newElement = new PolygonElement(target.displayArea_);
        newElement->setAbsolutePoints(translated);
        newElement->setForegroundColor(color);
        newElement->setFill(fill);
        newElement->setLineStyle(lineStyle);
        newElement->setLineWidth(lineWidth);
        newElement->setColorMode(colorMode);
        newElement->setVisibilityMode(visibilityMode);
        newElement->setVisibilityCalc(visibilityCalc);
        for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
          newElement->setChannel(i, channels[static_cast<std::size_t>(i)]);
        }
        newElement->show();
        target.ensureElementInStack(newElement);
        target.polygonElements_.append(newElement);
        target.selectPolygonElement(newElement);
        target.markDirty();
      });
      if (removeOriginal) {
        cutSelectedElement(polygonElements_, selectedPolygon_);
        finalizeCut();
      }
      return true;
    }

    return false;
  }

  void pasteFromClipboard()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || !state->clipboard
        || !state->clipboard->isValid()) {
      return;
    }

    QPoint offset = state->clipboard->nextOffset;
    state->clipboard->paste(*this, offset);
    state->clipboard->hasPasted = true;
    state->clipboard->nextOffset += QPoint(10, 10);
    notifyMenus();
  }

  bool hasAnyElementSelection() const
  {
    return !multiSelection_.isEmpty() || selectedTextElement_
        || selectedTextEntryElement_
        || selectedSliderElement_ || selectedWheelSwitchElement_
        || selectedChoiceButtonElement_ || selectedMenuElement_
        || selectedMessageButtonElement_ || selectedShellCommandElement_
        || selectedRelatedDisplayElement_ || selectedTextMonitorElement_
        || selectedMeterElement_ || selectedBarMonitorElement_
        || selectedScaleMonitorElement_ || selectedStripChartElement_
        || selectedCartesianPlotElement_ || selectedByteMonitorElement_
        || selectedRectangle_ || selectedImage_ || selectedOval_
    || selectedArc_ || selectedLine_ || selectedPolyline_
    || selectedPolygon_ || selectedCompositeElement_;
  }

  bool hasAnySelection() const
  {
    return displaySelected_ || hasAnyElementSelection();
  }

  bool hasSelectableElements() const
  {
    return !textElements_.isEmpty() || !textEntryElements_.isEmpty()
        || !sliderElements_.isEmpty() || !wheelSwitchElements_.isEmpty()
        || !choiceButtonElements_.isEmpty() || !menuElements_.isEmpty()
        || !messageButtonElements_.isEmpty() || !shellCommandElements_.isEmpty()
        || !relatedDisplayElements_.isEmpty() || !textMonitorElements_.isEmpty()
        || !meterElements_.isEmpty() || !barMonitorElements_.isEmpty()
        || !scaleMonitorElements_.isEmpty() || !stripChartElements_.isEmpty()
        || !cartesianPlotElements_.isEmpty() || !byteMonitorElements_.isEmpty()
        || !rectangleElements_.isEmpty() || !imageElements_.isEmpty()
        || !ovalElements_.isEmpty() || !arcElements_.isEmpty()
        || !lineElements_.isEmpty() || !polylineElements_.isEmpty()
        || !polygonElements_.isEmpty() || !compositeElements_.isEmpty();
  }

  bool canSelectAllElements() const
  {
    auto state = state_.lock();
    return state && state->editMode && hasSelectableElements();
  }

  bool canSelectDisplay() const
  {
    auto state = state_.lock();
    return state && state->editMode;
  }

  void closeResourcePalette()
  {
    if (!resourcePalette_.isNull() && resourcePalette_->isVisible()) {
      resourcePalette_->close();
    }
  }

  void handleResourcePaletteClosed()
  {
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
  }

  ResourcePaletteDialog *ensureResourcePalette()
  {
    if (resourcePalette_.isNull()) {
      resourcePalette_ = new ResourcePaletteDialog(
          resourcePaletteBase_, labelFont_, font(), this);
      QObject::connect(resourcePalette_, &QDialog::finished, this,
          [this](int) {
            handleResourcePaletteClosed();
          });
      QObject::connect(resourcePalette_, &QObject::destroyed, this,
          [this]() {
            resourcePalette_.clear();
            handleResourcePaletteClosed();
          });
      auto *cutShortcut = new QShortcut(
          QKeySequence(Qt::CTRL | Qt::Key_X), resourcePalette_);
      cutShortcut->setContext(Qt::WidgetWithChildrenShortcut);
      QObject::connect(cutShortcut, &QShortcut::activated, this,
          [this]() {
            setAsActiveDisplay();
            cutSelection();
          });
      auto *copyShortcut = new QShortcut(
          QKeySequence(Qt::CTRL | Qt::Key_C), resourcePalette_);
      copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
      QObject::connect(copyShortcut, &QShortcut::activated, this,
          [this]() {
            setAsActiveDisplay();
            copySelection();
          });
      auto *pasteShortcut = new QShortcut(
          QKeySequence(Qt::CTRL | Qt::Key_V), resourcePalette_);
      pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
      QObject::connect(pasteShortcut, &QShortcut::activated, this,
          [this]() {
            setAsActiveDisplay();
            pasteSelection();
          });
    }
    return resourcePalette_;
  }

  void showResourcePaletteForDisplay()
  {
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForDisplay(
        [this]() {
          return geometry();
        },
        [this](const QRect &newGeometry) {
          setGeometry(newGeometry);
          if (auto *widget = centralWidget()) {
            widget->setMinimumSize(newGeometry.size());

            widget->resize(newGeometry.size());
          }
          markDirty();
        },
        [this]() {
          if (auto *widget = centralWidget()) {
            return widget->palette().color(QPalette::WindowText);
          }
          return palette().color(QPalette::WindowText);
        },
        [this](const QColor &color) {
          QPalette windowPalette = palette();
          windowPalette.setColor(QPalette::WindowText, color);
          setPalette(windowPalette);
          if (auto *widget = centralWidget()) {
            QPalette widgetPalette = widget->palette();
            widgetPalette.setColor(QPalette::WindowText, color);
            widget->setPalette(widgetPalette);
            widget->update();
          }
          if (displayArea_) {
            displayArea_->setGridColor(color);
          }
          update();
          markDirty();
        },
        [this]() {
          if (auto *widget = centralWidget()) {
            return widget->palette().color(QPalette::Window);
          }
          return palette().color(QPalette::Window);
        },
        [this](const QColor &color) {
          QPalette windowPalette = palette();
          windowPalette.setColor(QPalette::Window, color);
          setPalette(windowPalette);
          if (auto *widget = centralWidget()) {
            QPalette widgetPalette = widget->palette();
            widgetPalette.setColor(QPalette::Window, color);
            widget->setPalette(widgetPalette);
            widget->update();
          }
          update();
          markDirty();
        },
        [this]() {
          return gridSpacing();
        },
        [this](int spacing) {
          setGridSpacing(spacing);
        },
        [this]() {
          return isGridOn();
        },
        [this](bool gridOn) {
          setGridOn(gridOn);
        });
  }

  void showResourcePaletteForMultipleSelection()
  {
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForMultipleSelection();
  }

  void showResourcePaletteForText(TextElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 5> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
        [element]() { return element->channel(4); },
    }};
    std::array<std::function<void(const QString &)>, 5> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(4, value);
          markDirty();
        },
    }};
    dialog->showForText(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumRectangleSize) {
            adjusted.setWidth(kMinimumRectangleSize);
          }
          if (adjusted.height() < kMinimumTextElementHeight) {
            adjusted.setHeight(kMinimumTextElementHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          if (constrained != widgetDisplayRect(element)) {
            setWidgetDisplayRect(element, constrained);
            markDirty();
          }
        },
        [element]() {
          return element->text();
        },
        [this, element](const QString &text) {
          element->setText(text.isEmpty() ? QStringLiteral(" ") : text);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->textAlignment();
        },
        [this, element](Qt::Alignment alignment) {
          element->setTextAlignment(alignment);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters));
  }

  void showResourcePaletteForTextEntry(TextEntryElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForTextEntry(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->format();
        },
        [this, element](TextMonitorFormat format) {
          element->setFormat(format);
          markDirty();
        },
        [element]() {
          return element->precision();
        },
        [this, element](int precision) {
          element->setPrecision(precision);
          markDirty();
        },
        [element]() {
          return element->precisionSource();
        },
        [this, element](PvLimitSource source) {
          element->setPrecisionSource(source);
          markDirty();
        },
        [element]() {
          return element->precisionDefault();
        },
        [this, element](int precision) {
          element->setPrecisionDefault(precision);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &value) {
          element->setChannel(value);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForSlider(SliderElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForSlider(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumSliderWidth) {
            adjusted.setWidth(kMinimumSliderWidth);
          }
          if (adjusted.height() < kMinimumSliderHeight) {
            adjusted.setHeight(kMinimumSliderHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->label();
        },
        [this, element](MeterLabel label) {
          element->setLabel(label);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->direction();
        },
        [this, element](BarDirection direction) {
          element->setDirection(direction);
          markDirty();
        },
        [element]() {
          return element->precision();
        },
        [this, element](double precision) {
          element->setPrecision(precision);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForWheelSwitch(WheelSwitchElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForWheelSwitch(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumWheelSwitchWidth) {
            adjusted.setWidth(kMinimumWheelSwitchWidth);
          }
          if (adjusted.height() < kMinimumWheelSwitchHeight) {
            adjusted.setHeight(kMinimumWheelSwitchHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->precision();
        },
        [this, element](double precision) {
          element->setPrecision(precision);
          markDirty();
        },
        [element]() {
          return element->format();
        },
        [this, element](const QString &format) {
          element->setFormat(format);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForChoiceButton(ChoiceButtonElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForChoiceButton(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->stacking();
        },
        [this, element](ChoiceButtonStacking stacking) {
          element->setStacking(stacking);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        });
  }

  void showResourcePaletteForMenu(MenuElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForMenu(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        });
  }

  void showResourcePaletteForMessageButton(MessageButtonElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForMessageButton(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->label();
        },
        [this, element](const QString &text) {
          element->setLabel(text);
          markDirty();
        },
        [element]() {
          return element->pressMessage();
        },
        [this, element](const QString &text) {
          element->setPressMessage(text);
          markDirty();
        },
        [element]() {
          return element->releaseMessage();
        },
        [this, element](const QString &text) {
          element->setReleaseMessage(text);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        });
  }

  void showResourcePaletteForShellCommand(ShellCommandElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }

    std::array<std::function<QString()>, kShellCommandEntryCount> entryLabelGetters{};
    std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryLabelSetters{};
    std::array<std::function<QString()>, kShellCommandEntryCount> entryCommandGetters{};
    std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryCommandSetters{};
    std::array<std::function<QString()>, kShellCommandEntryCount> entryArgsGetters{};
    std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryArgsSetters{};

    for (int i = 0; i < kShellCommandEntryCount; ++i) {
      entryLabelGetters[i] = [element, i]() { return element->entryLabel(i); };
      entryLabelSetters[i] = [this, element, i](const QString &value) {
        element->setEntryLabel(i, value);
        markDirty();
      };
      entryCommandGetters[i] = [element, i]() { return element->entryCommand(i); };
      entryCommandSetters[i] = [this, element, i](const QString &value) {
        element->setEntryCommand(i, value);
        markDirty();
      };
      entryArgsGetters[i] = [element, i]() { return element->entryArgs(i); };
      entryArgsSetters[i] = [this, element, i](const QString &value) {
        element->setEntryArgs(i, value);
        markDirty();
      };
    }

    dialog->showForShellCommand(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() { return element->foregroundColor(); },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() { return element->backgroundColor(); },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() { return element->label(); },
        [this, element](const QString &text) {
          element->setLabel(text);
          markDirty();
        },
        std::move(entryLabelGetters), std::move(entryLabelSetters),
        std::move(entryCommandGetters), std::move(entryCommandSetters),
        std::move(entryArgsGetters), std::move(entryArgsSetters));
  }

  void showResourcePaletteForRelatedDisplay(RelatedDisplayElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }

    std::array<std::function<QString()>, kRelatedDisplayEntryCount> labelGetters{};
    std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> labelSetters{};
    std::array<std::function<QString()>, kRelatedDisplayEntryCount> nameGetters{};
    std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> nameSetters{};
    std::array<std::function<QString()>, kRelatedDisplayEntryCount> argsGetters{};
    std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> argsSetters{};
    std::array<std::function<RelatedDisplayMode()>, kRelatedDisplayEntryCount> modeGetters{};
    std::array<std::function<void(RelatedDisplayMode)>, kRelatedDisplayEntryCount> modeSetters{};

    for (int i = 0; i < kRelatedDisplayEntryCount; ++i) {
      labelGetters[i] = [element, i]() { return element->entryLabel(i); };
      labelSetters[i] = [this, element, i](const QString &value) {
        element->setEntryLabel(i, value);
        markDirty();
      };
      nameGetters[i] = [element, i]() { return element->entryName(i); };
      nameSetters[i] = [this, element, i](const QString &value) {
        element->setEntryName(i, value);
        markDirty();
      };
      argsGetters[i] = [element, i]() { return element->entryArgs(i); };
      argsSetters[i] = [this, element, i](const QString &value) {
        element->setEntryArgs(i, value);
        markDirty();
      };
      modeGetters[i] = [element, i]() { return element->entryMode(i); };
      modeSetters[i] = [this, element, i](RelatedDisplayMode mode) {
        element->setEntryMode(i, mode);
        markDirty();
      };
    }

    dialog->showForRelatedDisplay(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() { return element->foregroundColor(); },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() { return element->backgroundColor(); },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() { return element->label(); },
        [this, element](const QString &text) {
          element->setLabel(text);
          markDirty();
        },
        [element]() { return element->visual(); },
        [this, element](RelatedDisplayVisual visual) {
          element->setVisual(visual);
          markDirty();
        },
        std::move(labelGetters), std::move(labelSetters), std::move(nameGetters),
        std::move(nameSetters), std::move(argsGetters), std::move(argsSetters),
        std::move(modeGetters), std::move(modeSetters));
  }

  void showResourcePaletteForTextMonitor(TextMonitorElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForTextMonitor(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->textAlignment();
        },
        [this, element](Qt::Alignment alignment) {
          element->setTextAlignment(alignment);
          markDirty();
        },
        [element]() {
          return element->format();
        },
        [this, element](TextMonitorFormat format) {
          element->setFormat(format);
          markDirty();
        },
        [element]() {
          return element->precision();
        },
        [this, element](int precision) {
          element->setPrecision(precision);
          markDirty();
        },
        [element]() {
          return element->precisionSource();
        },
        [this, element](PvLimitSource source) {
          element->setPrecisionSource(source);
          markDirty();
        },
        [element]() {
          return element->precisionDefault();
        },
        [this, element](int precision) {
          element->setPrecisionDefault(precision);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->channel(0);
        },
        [this, element](const QString &value) {
          element->setChannel(0, value);
          element->setText(value);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForMeter(MeterElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForMeter(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumMeterSize) {
            adjusted.setWidth(kMinimumMeterSize);
          }
          if (adjusted.height() < kMinimumMeterSize) {
            adjusted.setHeight(kMinimumMeterSize);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->label();
        },
        [this, element](MeterLabel label) {
          element->setLabel(label);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForStripChart(StripChartElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }

    std::array<std::function<QString()>, kStripChartPenCount> channelGetters{};
    std::array<std::function<void(const QString &)>, kStripChartPenCount> channelSetters{};
    std::array<std::function<QColor()>, kStripChartPenCount> colorGetters{};
    std::array<std::function<void(const QColor &)>, kStripChartPenCount> colorSetters{};
    std::array<std::function<PvLimits()>, kStripChartPenCount> limitsGetters{};
    std::array<std::function<void(const PvLimits &)>, kStripChartPenCount> limitsSetters{};
    for (int i = 0; i < kStripChartPenCount; ++i) {
      channelGetters[i] = [element, i]() {
        return element->channel(i);
      };
      channelSetters[i] = [this, element, i](const QString &channel) {
        element->setChannel(i, channel);
        markDirty();
      };
      colorGetters[i] = [element, i]() {
        return element->penColor(i);
      };
      colorSetters[i] = [this, element, i](const QColor &color) {
        element->setPenColor(i, color);
        markDirty();
      };
      limitsGetters[i] = [element, i]() {
        return element->penLimits(i);
      };
      limitsSetters[i] = [this, element, i](const PvLimits &limits) {
        element->setPenLimits(i, limits);
        markDirty();
      };
    }

    dialog->showForStripChart(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumStripChartWidth) {
            adjusted.setWidth(kMinimumStripChartWidth);
          }
          if (adjusted.height() < kMinimumStripChartHeight) {
            adjusted.setHeight(kMinimumStripChartHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->title();
        },
        [this, element](const QString &title) {
          element->setTitle(title);
          markDirty();
        },
        [element]() {
          return element->xLabel();
        },
        [this, element](const QString &label) {
          element->setXLabel(label);
          markDirty();
        },
        [element]() {
          return element->yLabel();
        },
        [this, element](const QString &label) {
          element->setYLabel(label);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->period();
        },
        [this, element](double period) {
          element->setPeriod(period);
          markDirty();
        },
        [element]() {
          return element->units();
        },
        [this, element](TimeUnits units) {
          element->setUnits(units);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters),
        std::move(colorGetters), std::move(colorSetters),
        std::move(limitsGetters), std::move(limitsSetters));
  }

  void showResourcePaletteForCartesianPlot(CartesianPlotElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }

    std::array<std::function<QString()>, 4> yLabelGetters{};
    std::array<std::function<void(const QString &)>, 4> yLabelSetters{};
    for (int i = 0; i < 4; ++i) {
      yLabelGetters[i] = [element, i]() { return element->yLabel(i); };
      yLabelSetters[i] = [this, element, i](const QString &label) {
        element->setYLabel(i, label);
        markDirty();
      };
    }

    std::array<std::function<QString()>, kCartesianPlotTraceCount> xChannelGetters{};
    std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> xChannelSetters{};
    std::array<std::function<QString()>, kCartesianPlotTraceCount> yChannelGetters{};
    std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> yChannelSetters{};
    std::array<std::function<QColor()>, kCartesianPlotTraceCount> colorGetters{};
    std::array<std::function<void(const QColor &)>, kCartesianPlotTraceCount> colorSetters{};
    std::array<std::function<CartesianPlotYAxis()>, kCartesianPlotTraceCount> axisGetters{};
    std::array<std::function<void(CartesianPlotYAxis)>, kCartesianPlotTraceCount> axisSetters{};
    std::array<std::function<bool()>, kCartesianPlotTraceCount> sideGetters{};
    std::array<std::function<void(bool)>, kCartesianPlotTraceCount> sideSetters{};

    std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> axisStyleGetters{};
    std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> axisStyleSetters{};
    std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> axisRangeGetters{};
    std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> axisRangeSetters{};
    std::array<std::function<double()>, kCartesianAxisCount> axisMinimumGetters{};
    std::array<std::function<void(double)>, kCartesianAxisCount> axisMinimumSetters{};
    std::array<std::function<double()>, kCartesianAxisCount> axisMaximumGetters{};
    std::array<std::function<void(double)>, kCartesianAxisCount> axisMaximumSetters{};
    std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> axisTimeFormatGetters{};
    std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> axisTimeFormatSetters{};

    for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
      xChannelGetters[i] = [element, i]() { return element->traceXChannel(i); };
      xChannelSetters[i] = [this, element, i](const QString &channel) {
        element->setTraceXChannel(i, channel);
        markDirty();
      };
      yChannelGetters[i] = [element, i]() { return element->traceYChannel(i); };
      yChannelSetters[i] = [this, element, i](const QString &channel) {
        element->setTraceYChannel(i, channel);
        markDirty();
      };
      colorGetters[i] = [element, i]() { return element->traceColor(i); };
      colorSetters[i] = [this, element, i](const QColor &color) {
        element->setTraceColor(i, color);
        markDirty();
      };
      axisGetters[i] = [element, i]() { return element->traceYAxis(i); };
      axisSetters[i] = [this, element, i](CartesianPlotYAxis axis) {
        element->setTraceYAxis(i, axis);
        markDirty();
      };
      sideGetters[i] = [element, i]() { return element->traceUsesRightAxis(i); };
      sideSetters[i] = [this, element, i](bool usesRight) {
        element->setTraceUsesRightAxis(i, usesRight);
        markDirty();
      };
    }

    for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
      axisStyleGetters[axis] = [element, axis]() { return element->axisStyle(axis); };
      axisStyleSetters[axis] = [this, element, axis](CartesianPlotAxisStyle style) {
        element->setAxisStyle(axis, style);
        markDirty();
      };
      axisRangeGetters[axis] = [element, axis]() { return element->axisRangeStyle(axis); };
      axisRangeSetters[axis] = [this, element, axis](CartesianPlotRangeStyle style) {
        element->setAxisRangeStyle(axis, style);
        markDirty();
      };
      axisMinimumGetters[axis] = [element, axis]() { return element->axisMinimum(axis); };
      axisMinimumSetters[axis] = [this, element, axis](double value) {
        element->setAxisMinimum(axis, value);
        markDirty();
      };
      axisMaximumGetters[axis] = [element, axis]() { return element->axisMaximum(axis); };
      axisMaximumSetters[axis] = [this, element, axis](double value) {
        element->setAxisMaximum(axis, value);
        markDirty();
      };
      axisTimeFormatGetters[axis] = [element, axis]() { return element->axisTimeFormat(axis); };
      axisTimeFormatSetters[axis] = [this, element, axis](CartesianPlotTimeFormat format) {
        element->setAxisTimeFormat(axis, format);
        markDirty();
      };
    }

    dialog->showForCartesianPlot(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumCartesianPlotWidth) {
            adjusted.setWidth(kMinimumCartesianPlotWidth);
          }
          if (adjusted.height() < kMinimumCartesianPlotHeight) {
            adjusted.setHeight(kMinimumCartesianPlotHeight);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->title();
        },
        [this, element](const QString &title) {
          element->setTitle(title);
          markDirty();
        },
        [element]() {
          return element->xLabel();
        },
        [this, element](const QString &label) {
          element->setXLabel(label);
          markDirty();
        },
        std::move(yLabelGetters), std::move(yLabelSetters),
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        std::move(axisStyleGetters), std::move(axisStyleSetters),
        std::move(axisRangeGetters), std::move(axisRangeSetters),
        std::move(axisMinimumGetters), std::move(axisMinimumSetters),
        std::move(axisMaximumGetters), std::move(axisMaximumSetters),
        std::move(axisTimeFormatGetters), std::move(axisTimeFormatSetters),
        [element]() {
          return element->style();
        },
        [this, element](CartesianPlotStyle style) {
          element->setStyle(style);
          markDirty();
        },
        [element]() {
          return element->eraseOldest();
        },
        [this, element](bool eraseOldest) {
          element->setEraseOldest(eraseOldest);
          markDirty();
        },
        [element]() {
          return element->count();
        },
        [this, element](int count) {
          element->setCount(count);
          markDirty();
        },
        [element]() {
          return element->eraseMode();
        },
        [this, element](CartesianPlotEraseMode mode) {
          element->setEraseMode(mode);
          markDirty();
        },
        [element]() {
          return element->triggerChannel();
        },
        [this, element](const QString &channel) {
          element->setTriggerChannel(channel);
          markDirty();
        },
        [element]() {
          return element->eraseChannel();
        },
        [this, element](const QString &channel) {
          element->setEraseChannel(channel);
          markDirty();
        },
        [element]() {
          return element->countChannel();
        },
        [this, element](const QString &channel) {
          element->setCountChannel(channel);
          markDirty();
        },
        std::move(xChannelGetters), std::move(xChannelSetters),
        std::move(yChannelGetters), std::move(yChannelSetters),
        std::move(colorGetters), std::move(colorSetters),
        std::move(axisGetters), std::move(axisSetters),
        std::move(sideGetters), std::move(sideSetters));
  }

  void showResourcePaletteForBar(BarMonitorElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForBarMonitor(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumBarSize) {
            adjusted.setWidth(kMinimumBarSize);
          }
          if (adjusted.height() < kMinimumBarSize) {
            adjusted.setHeight(kMinimumBarSize);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->label();
        },
        [this, element](MeterLabel label) {
          element->setLabel(label);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->direction();
        },
        [this, element](BarDirection direction) {
          element->setDirection(direction);
          markDirty();
        },
        [element]() {
          return element->fillMode();
        },
        [this, element](BarFill mode) {
          element->setFillMode(mode);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForScale(ScaleMonitorElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForScaleMonitor(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumScaleSize) {
            adjusted.setWidth(kMinimumScaleSize);
          }
          if (adjusted.height() < kMinimumScaleSize) {
            adjusted.setHeight(kMinimumScaleSize);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->label();
        },
        [this, element](MeterLabel label) {
          element->setLabel(label);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->direction();
        },
        [this, element](BarDirection direction) {
          element->setDirection(direction);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        },
        [element]() {
          return element->limits();
        },
        [this, element](const PvLimits &limits) {
          element->setLimits(limits);
          markDirty();
        });
  }

  void showResourcePaletteForByte(ByteMonitorElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    dialog->showForByteMonitor(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumByteSize) {
            adjusted.setWidth(kMinimumByteSize);
          }
          if (adjusted.height() < kMinimumByteSize) {
            adjusted.setHeight(kMinimumByteSize);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->direction();
        },
        [this, element](BarDirection direction) {
          element->setDirection(direction);
          markDirty();
        },
        [element]() {
          return element->startBit();
        },
        [this, element](int bit) {
          element->setStartBit(bit);
          markDirty();
        },
        [element]() {
          return element->endBit();
        },
        [this, element](int bit) {
          element->setEndBit(bit);
          markDirty();
        },
        [element]() {
          return element->channel();
        },
        [this, element](const QString &channel) {
          element->setChannel(channel);
          markDirty();
        });
  }

  void showResourcePaletteForComposite(CompositeElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForComposite(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < 1) {
            adjusted.setWidth(1);
          }
          if (adjusted.height() < 1) {
            adjusted.setHeight(1);
          }
          const QRect constrained = adjustRectToDisplayArea(adjusted);
          if (constrained != widgetDisplayRect(element)) {
            setWidgetDisplayRect(element, constrained);
            markDirty();
          }
        },
        [element]() {
          return element->foregroundColor();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->backgroundColor();
        },
        [this, element](const QColor &color) {
          element->setBackgroundColor(color);
          markDirty();
        },
        [element]() {
          return element->compositeFile();
        },
        [this, element](const QString &filePath) {
          element->setCompositeFile(filePath);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        channelGetters,
        channelSetters,
        element->compositeName().isEmpty() ? QStringLiteral("Composite")
                                           : element->compositeName());
  }

  void showResourcePaletteForRectangle(RectangleElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForRectangle(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          const QRect constrained = adjustRectToDisplayArea(newGeometry);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->color();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->fill();
        },
        [this, element](RectangleFill fill) {
          element->setFill(fill);
          markDirty();
        },
        [element]() {
          return element->lineStyle();
        },
        [this, element](RectangleLineStyle style) {
          element->setLineStyle(style);
          markDirty();
        },
        [element]() {
          return element->lineWidth();
        },
        [this, element](int width) {
          element->setLineWidth(width);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters));
  }

  void showResourcePaletteForImage(ImageElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForImage(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          const QRect constrained = adjustRectToDisplayArea(newGeometry);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->imageType();
        },
        [this, element](ImageType type) {
          element->setImageType(type);
          markDirty();
        },
        [element]() {
          return element->imageName();
        },
        [this, element](const QString &name) {
          element->setImageName(name);
          markDirty();
        },
        [element]() {
          return element->calc();
        },
        [this, element](const QString &calc) {
          element->setCalc(calc);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters));
  }

  void showResourcePaletteForOval(OvalElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForRectangle(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          const QRect constrained = adjustRectToDisplayArea(newGeometry);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->color();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->fill();
        },
        [this, element](RectangleFill fill) {
          element->setFill(fill);
          markDirty();
        },
        [element]() {
          return element->lineStyle();
        },
        [this, element](RectangleLineStyle style) {
          element->setLineStyle(style);
          markDirty();
        },
        [element]() {
          return element->lineWidth();
        },
        [this, element](int width) {
          element->setLineWidth(width);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters),
        QStringLiteral("Oval"));
  }

  void showResourcePaletteForArc(ArcElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForRectangle(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          const QRect constrained = adjustRectToDisplayArea(newGeometry);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->color();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->fill();
        },
        [this, element](RectangleFill fill) {
          element->setFill(fill);
          markDirty();
        },
        [element]() {
          return element->lineStyle();
        },
        [this, element](RectangleLineStyle style) {
          element->setLineStyle(style);
          markDirty();
        },
        [element]() {
          return element->lineWidth();
        },
        [this, element](int width) {
          element->setLineWidth(width);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters),
        QStringLiteral("Arc"), false,
        [element]() {
          return element->beginAngle();
        },
        [this, element](int angle) {
          element->setBeginAngle(angle);
          markDirty();
        },
        [element]() {
          return element->pathAngle();
        },
        [this, element](int angle) {
          element->setPathAngle(angle);
          markDirty();
        });
  }

  void showResourcePaletteForLine(LineElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForLine(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          const QRect constrained = adjustRectToDisplayArea(newGeometry);
          setWidgetDisplayRect(element, constrained);
          markDirty();
        },
        [element]() {
          return element->color();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->lineStyle();
        },
        [this, element](RectangleLineStyle style) {
          element->setLineStyle(style);
          markDirty();
        },
        [element]() {
          return element->lineWidth();
        },
        [this, element](int width) {
          element->setLineWidth(width);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters));
  }

  void showResourcePaletteForPolyline(PolylineElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    const int pointCount = element->absolutePoints().size();
    const QString label = pointCount == 2 ? QStringLiteral("Line")
                                          : QStringLiteral("Polyline");
    dialog->showForLine(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = adjustRectToDisplayArea(newGeometry);
          if (adjusted.width() < 1) {
            adjusted.setWidth(1);
          }
          if (adjusted.height() < 1) {
            adjusted.setHeight(1);
          }
          setWidgetDisplayRect(element, adjusted);
          element->update();
          markDirty();
        },
        [element]() {
          return element->color();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->lineStyle();
        },
        [this, element](RectangleLineStyle style) {
          element->setLineStyle(style);
          markDirty();
        },
        [element]() {
          return element->lineWidth();
        },
        [this, element](int width) {
          element->setLineWidth(width);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters), label);
  }

  void showResourcePaletteForPolygon(PolygonElement *element)
  {
    if (!element) {
      return;
    }
    ResourcePaletteDialog *dialog = ensureResourcePalette();
    if (!dialog) {
      return;
    }
    std::array<std::function<QString()>, 4> channelGetters{{
        [element]() { return element->channel(0); },
        [element]() { return element->channel(1); },
        [element]() { return element->channel(2); },
        [element]() { return element->channel(3); },
    }};
    std::array<std::function<void(const QString &)>, 4> channelSetters{{
        [this, element](const QString &value) {
          element->setChannel(0, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(1, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(2, value);
          markDirty();
        },
        [this, element](const QString &value) {
          element->setChannel(3, value);
          markDirty();
        },
    }};
    dialog->showForRectangle(
        [this, element]() {
          return widgetDisplayRect(element);
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = adjustRectToDisplayArea(newGeometry);
          if (adjusted.width() < 1) {
            adjusted.setWidth(1);
          }
          if (adjusted.height() < 1) {
            adjusted.setHeight(1);
          }
          setWidgetDisplayRect(element, adjusted);
          element->update();
          markDirty();
        },
        [element]() {
          return element->color();
        },
        [this, element](const QColor &color) {
          element->setForegroundColor(color);
          markDirty();
        },
        [element]() {
          return element->fill();
        },
        [this, element](RectangleFill fill) {
          element->setFill(fill);
          markDirty();
        },
        [element]() {
          return element->lineStyle();
        },
        [this, element](RectangleLineStyle style) {
          element->setLineStyle(style);
          markDirty();
        },
        [element]() {
          return element->lineWidth();
        },
        [this, element](int width) {
          element->setLineWidth(width);
          markDirty();
        },
        [element]() {
          return element->colorMode();
        },
        [this, element](TextColorMode mode) {
          element->setColorMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityMode();
        },
        [this, element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
          markDirty();
        },
        [element]() {
          return element->visibilityCalc();
        },
        [this, element](const QString &calc) {
          element->setVisibilityCalc(calc);
          markDirty();
        },
        std::move(channelGetters), std::move(channelSetters),
        QStringLiteral("Polygon"), true);
  }

  QWidget *elementAt(const QPoint &windowPos) const
  {
    if (!displayArea_) {
      return nullptr;
    }
    const QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    if (!displayArea_->rect().contains(areaPos)) {
      return nullptr;
    }
    for (auto it = elementStack_.crbegin(); it != elementStack_.crend(); ++it) {
      QWidget *widget = it->data();
      if (!widget) {
        continue;
      }
      if (!widget->geometry().contains(areaPos)) {
        continue;
      }
      if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
        if (!polyline->containsGlobalPoint(areaPos)) {
          continue;
        }
      }
      if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
        if (!polygon->containsGlobalPoint(areaPos)) {
          continue;
        }
      }
      return widget;
    }
    return nullptr;
  }

  void bringElementToFront(QWidget *element)
  {
    if (!element) {
      return;
    }
    for (auto it = elementStack_.begin(); it != elementStack_.end(); ++it) {
      QWidget *current = it->data();
      if (current == element) {
        QPointer<QWidget> pointer = *it;
        elementStack_.erase(it);
        elementStack_.append(pointer);
        element->raise();
        return;
      }
    }
    elementStack_.append(QPointer<QWidget>(element));
    element->raise();
  }

  void removeElementFromStack(QWidget *element)
  {
    if (!element) {
      return;
    }
    for (auto it = elementStack_.begin(); it != elementStack_.end();) {
      QWidget *current = it->data();
      if (!current || current == element) {
        it = elementStack_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void selectTextElement(TextElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedTextElement_) {
      selectedTextElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedTextElement_ = element;
    selectedTextElement_->setSelected(true);
  }

  void selectTextEntryElement(TextEntryElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedTextEntryElement_) {
      selectedTextEntryElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedTextEntryElement_ = element;
    selectedTextEntryElement_->setSelected(true);
  }

  void selectSliderElement(SliderElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedSliderElement_) {
      selectedSliderElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedSliderElement_ = element;
    selectedSliderElement_->setSelected(true);
  }

  void selectWheelSwitchElement(WheelSwitchElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedWheelSwitchElement_) {
      selectedWheelSwitchElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedWheelSwitchElement_ = element;
    selectedWheelSwitchElement_->setSelected(true);
  }

  void selectChoiceButtonElement(ChoiceButtonElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedChoiceButtonElement_) {
      selectedChoiceButtonElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedChoiceButtonElement_ = element;
    selectedChoiceButtonElement_->setSelected(true);
  }

  void selectMenuElement(MenuElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedMenuElement_) {
      selectedMenuElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedMenuElement_ = element;
    selectedMenuElement_->setSelected(true);
  }

  void selectMessageButtonElement(MessageButtonElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedMessageButtonElement_) {
      selectedMessageButtonElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedMessageButtonElement_ = element;
    selectedMessageButtonElement_->setSelected(true);
  }

  void selectShellCommandElement(ShellCommandElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedShellCommandElement_) {
      selectedShellCommandElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedShellCommandElement_ = element;
    selectedShellCommandElement_->setSelected(true);
  }

  void selectRelatedDisplayElement(RelatedDisplayElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedRelatedDisplayElement_) {
      selectedRelatedDisplayElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedRelatedDisplayElement_ = element;
    selectedRelatedDisplayElement_->setSelected(true);
  }

  void selectTextMonitorElement(TextMonitorElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedTextMonitorElement_) {
      selectedTextMonitorElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedTextMonitorElement_ = element;
    selectedTextMonitorElement_->setSelected(true);
  }

  void selectMeterElement(MeterElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedMeterElement_) {
      selectedMeterElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedMeterElement_ = element;
    selectedMeterElement_->setSelected(true);
  }

  void selectScaleMonitorElement(ScaleMonitorElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedScaleMonitorElement_) {
      selectedScaleMonitorElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedScaleMonitorElement_ = element;
    selectedScaleMonitorElement_->setSelected(true);
  }

  void selectStripChartElement(StripChartElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedStripChartElement_) {
      selectedStripChartElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedStripChartElement_ = element;
    selectedStripChartElement_->setSelected(true);
  }

  void selectCartesianPlotElement(CartesianPlotElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedCartesianPlotElement_) {
      selectedCartesianPlotElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedCartesianPlotElement_ = element;
    selectedCartesianPlotElement_->setSelected(true);
  }

  void selectBarMonitorElement(BarMonitorElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedBarMonitorElement_) {
      selectedBarMonitorElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedBarMonitorElement_ = element;
    selectedBarMonitorElement_->setSelected(true);
  }

  void selectByteMonitorElement(ByteMonitorElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedByteMonitorElement_) {
      selectedByteMonitorElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedByteMonitorElement_ = element;
    selectedByteMonitorElement_->setSelected(true);
  }

  void selectRectangleElement(RectangleElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedRectangle_) {
      selectedRectangle_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedRectangle_ = element;
    selectedRectangle_->setSelected(true);
  }

  void selectImageElement(ImageElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedImage_) {
      selectedImage_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedImage_ = element;
    selectedImage_->setSelected(true);
  }

  void selectOvalElement(OvalElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedOval_) {
      selectedOval_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedOval_ = element;
    selectedOval_->setSelected(true);
  }

  void selectArcElement(ArcElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedArc_) {
      selectedArc_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearOvalSelection();
    clearImageSelection();
    clearLineSelection();
    clearPolygonSelection();
    clearPolylineSelection();
    clearCompositeSelection();
    selectedArc_ = element;
    selectedArc_->setSelected(true);
  }

  void selectLineElement(LineElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedLine_) {
      selectedLine_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearOvalSelection();
    clearArcSelection();
    clearImageSelection();
    clearPolygonSelection();
    clearPolylineSelection();
    clearCompositeSelection();
    selectedLine_ = element;
    selectedLine_->setSelected(true);
  }

  void selectPolylineElement(PolylineElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedPolyline_) {
      selectedPolyline_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearImageSelection();
    clearPolygonSelection();
    clearCompositeSelection();
    selectedPolyline_ = element;
    selectedPolyline_->setSelected(true);
  }

  void selectPolygonElement(PolygonElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedPolygon_) {
      selectedPolygon_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearImageSelection();
    clearOvalSelection();
    clearCompositeSelection();
    selectedPolygon_ = element;
    selectedPolygon_->setSelected(true);
  }

  void selectCompositeElement(CompositeElement *element)
  {
    if (!element) {
      return;
    }
    if (selectedCompositeElement_) {
      selectedCompositeElement_->setSelected(false);
    }
    clearDisplaySelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearRectangleSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    clearImageSelection();
    clearOvalSelection();
    selectedCompositeElement_ = element;
    selectedCompositeElement_->setSelected(true);
  }

  QWidget *currentSelectedWidget() const
  {
    for (const auto &pointer : multiSelection_) {
      if (QWidget *widget = pointer.data()) {
        return widget;
      }
    }
    if (selectedTextElement_) {
      return selectedTextElement_;
    }
    if (selectedTextEntryElement_) {
      return selectedTextEntryElement_;
    }
    if (selectedSliderElement_) {
      return selectedSliderElement_;
    }
    if (selectedWheelSwitchElement_) {
      return selectedWheelSwitchElement_;
    }
    if (selectedChoiceButtonElement_) {
      return selectedChoiceButtonElement_;
    }
    if (selectedMenuElement_) {
      return selectedMenuElement_;
    }
    if (selectedMessageButtonElement_) {
      return selectedMessageButtonElement_;
    }
    if (selectedShellCommandElement_) {
      return selectedShellCommandElement_;
    }
    if (selectedRelatedDisplayElement_) {
      return selectedRelatedDisplayElement_;
    }
    if (selectedTextMonitorElement_) {
      return selectedTextMonitorElement_;
    }
    if (selectedMeterElement_) {
      return selectedMeterElement_;
    }
    if (selectedBarMonitorElement_) {
      return selectedBarMonitorElement_;
    }
    if (selectedScaleMonitorElement_) {
      return selectedScaleMonitorElement_;
    }
    if (selectedStripChartElement_) {
      return selectedStripChartElement_;
    }
    if (selectedCartesianPlotElement_) {
      return selectedCartesianPlotElement_;
    }
    if (selectedByteMonitorElement_) {
      return selectedByteMonitorElement_;
    }
    if (selectedRectangle_) {
      return selectedRectangle_;
    }
    if (selectedImage_) {
      return selectedImage_;
    }
    if (selectedOval_) {
      return selectedOval_;
    }
    if (selectedArc_) {
      return selectedArc_;
    }
    if (selectedLine_) {
      return selectedLine_;
    }
    if (selectedPolyline_) {
      return selectedPolyline_;
    }
    if (selectedPolygon_) {
      return selectedPolygon_;
    }
    if (selectedCompositeElement_) {
      return selectedCompositeElement_;
    }
    return nullptr;
  }

  bool selectWidgetForEditing(QWidget *widget)
  {
    if (!widget) {
      return false;
    }
    clearMultiSelection();
    if (auto *text = dynamic_cast<TextElement *>(widget)) {
      selectTextElement(text);
      showResourcePaletteForText(text);
      return true;
    }
    if (auto *textEntry = dynamic_cast<TextEntryElement *>(widget)) {
      selectTextEntryElement(textEntry);
      showResourcePaletteForTextEntry(textEntry);
      return true;
    }
    if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      selectSliderElement(slider);
      showResourcePaletteForSlider(slider);
      return true;
    }
    if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
      selectWheelSwitchElement(wheel);
      showResourcePaletteForWheelSwitch(wheel);
      return true;
    }
    if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
      selectChoiceButtonElement(choice);
      showResourcePaletteForChoiceButton(choice);
      return true;
    }
    if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
      selectMenuElement(menu);
      showResourcePaletteForMenu(menu);
      return true;
    }
    if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
      selectMessageButtonElement(message);
      showResourcePaletteForMessageButton(message);
      return true;
    }
    if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
      selectShellCommandElement(shell);
      showResourcePaletteForShellCommand(shell);
      return true;
    }
    if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
      selectRelatedDisplayElement(related);
      showResourcePaletteForRelatedDisplay(related);
      return true;
    }
    if (auto *textMonitor = dynamic_cast<TextMonitorElement *>(widget)) {
      selectTextMonitorElement(textMonitor);
      showResourcePaletteForTextMonitor(textMonitor);
      return true;
    }
    if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
      selectMeterElement(meter);
      showResourcePaletteForMeter(meter);
      return true;
    }
    if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
      selectScaleMonitorElement(scale);
      showResourcePaletteForScale(scale);
      return true;
    }
    if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
      selectStripChartElement(strip);
      showResourcePaletteForStripChart(strip);
      return true;
    }
    if (auto *cart = dynamic_cast<CartesianPlotElement *>(widget)) {
      selectCartesianPlotElement(cart);
      showResourcePaletteForCartesianPlot(cart);
      return true;
    }
    if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
      selectBarMonitorElement(bar);
      showResourcePaletteForBar(bar);
      return true;
    }
    if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
      selectByteMonitorElement(byte);
      showResourcePaletteForByte(byte);
      return true;
    }
    if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      selectRectangleElement(rectangle);
      showResourcePaletteForRectangle(rectangle);
      return true;
    }
    if (auto *image = dynamic_cast<ImageElement *>(widget)) {
      selectImageElement(image);
      showResourcePaletteForImage(image);
      return true;
    }
    if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      selectOvalElement(oval);
      showResourcePaletteForOval(oval);
      return true;
    }
    if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      selectArcElement(arc);
      showResourcePaletteForArc(arc);
      return true;
    }
    if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      selectPolylineElement(polyline);
      showResourcePaletteForPolyline(polyline);
      return true;
    }
    if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      selectPolygonElement(polygon);
      showResourcePaletteForPolygon(polygon);
      return true;
    }
    if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
      selectCompositeElement(composite);
      showResourcePaletteForComposite(composite);
      return true;
    }
    if (auto *line = dynamic_cast<LineElement *>(widget)) {
      selectLineElement(line);
      showResourcePaletteForLine(line);
      return true;
    }
    return false;
  }

  void beginMiddleButtonDrag(const QPoint &windowPos)
  {
    finishMiddleButtonDrag(false);
    if (!displayArea_) {
      return;
    }
    QList<QPointer<QWidget>> dragWidgets;
    if (!multiSelection_.isEmpty()) {
      dragWidgets = multiSelection_;
    } else {
      if (QWidget *selected = currentSelectedWidget()) {
        dragWidgets.append(QPointer<QWidget>(selected));
      }
    }
    middleButtonDragWidgets_.clear();
    middleButtonInitialRects_.clear();
    middleButtonBoundingRect_ = QRect();
    for (const auto &pointer : dragWidgets) {
      QWidget *widget = pointer.data();
      if (!widget) {
        continue;
      }
      middleButtonDragWidgets_.append(QPointer<QWidget>(widget));
      const QRect rect = widget->geometry();
      middleButtonInitialRects_.append(rect);
      if (!middleButtonBoundingRect_.isValid()) {
        middleButtonBoundingRect_ = rect;
      } else {
        middleButtonBoundingRect_ = middleButtonBoundingRect_.united(rect);
      }
    }
    if (middleButtonDragWidgets_.isEmpty()) {
      return;
    }
    middleButtonDragStartAreaPos_ = displayArea_->mapFrom(this, windowPos);
    middleButtonDragActive_ = true;
    middleButtonDragMoved_ = false;
  }

  void updateMiddleButtonDrag(const QPoint &windowPos)
  {
    if (!middleButtonDragActive_ || middleButtonDragWidgets_.isEmpty()
        || !displayArea_) {
      return;
    }
    const QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    const QPoint offset = areaPos - middleButtonDragStartAreaPos_;
    const QPoint clamped =
        clampOffsetToDisplayArea(middleButtonBoundingRect_, offset);
    bool anyMoved = false;
    const int widgetCount = middleButtonDragWidgets_.size();
    for (int i = 0; i < widgetCount; ++i) {
      QWidget *widget = middleButtonDragWidgets_.at(i).data();
      if (!widget) {
        continue;
      }
      const QRect initialRect = middleButtonInitialRects_.value(i);
      const QPoint newTopLeft = initialRect.topLeft() + clamped;
      if (widget->pos() == newTopLeft) {
        continue;
      }
      widget->move(newTopLeft);
      widget->update();
      anyMoved = true;
    }
    if (anyMoved) {
      middleButtonDragMoved_ = true;
    }
  }

  void finishMiddleButtonDrag(bool applyChanges)
  {
    const bool wasActive = middleButtonDragActive_;
    const bool moved = middleButtonDragMoved_;
    middleButtonDragActive_ = false;
    middleButtonDragMoved_ = false;
    middleButtonInitialRects_.clear();
    middleButtonDragWidgets_.clear();
    middleButtonBoundingRect_ = QRect();
    if (applyChanges && wasActive && moved) {
      markDirty();
      refreshResourcePaletteGeometry();
    }
  }

  void cancelMiddleButtonDrag()
  {
    finishMiddleButtonDrag(false);
  }

  void refreshResourcePaletteGeometry()
  {
    if (resourcePalette_.isNull()) {
      return;
    }
    resourcePalette_->refreshGeometryFromSelection();
  }

  QPoint clampOffsetToDisplayArea(const QRect &rect,
      const QPoint &offset) const
  {
    if (!displayArea_) {
      return offset;
    }
    const QRect areaRect = displayArea_->rect();
    const int maxLeft = std::max(areaRect.left(),
        areaRect.right() - rect.width() + 1);
    const int maxTop = std::max(areaRect.top(),
        areaRect.bottom() - rect.height() + 1);
    const int clampedLeft = std::clamp(rect.left() + offset.x(),
        areaRect.left(), maxLeft);
    const int clampedTop = std::clamp(rect.top() + offset.y(),
        areaRect.top(), maxTop);
    return QPoint(clampedLeft - rect.left(), clampedTop - rect.top());
  }

  void cancelSelectionRubberBand()
  {
    selectionRubberBandPending_ = false;
    if (!selectionRubberBandMode_) {
      return;
    }
    selectionRubberBandMode_ = false;
    rubberBandActive_ = false;
    if (rubberBand_) {
      rubberBand_->hide();
    }
  }

  void beginSelectionRubberBandPending(const QPoint &areaPos,
      const QPoint &windowPos)
  {
    selectionRubberBandPending_ = true;
    pendingSelectionOrigin_ = clampToDisplayArea(areaPos);
    selectionRubberBandStartWindowPos_ = windowPos;
  }

  void startSelectionRubberBand()
  {
    if (!displayArea_) {
      selectionRubberBandPending_ = false;
      return;
    }
    selectionRubberBandPending_ = false;
    selectionRubberBandMode_ = true;
    rubberBandActive_ = true;
    rubberBandOrigin_ = pendingSelectionOrigin_;
    ensureRubberBand();
    if (rubberBand_) {
      rubberBand_->setGeometry(QRect(rubberBandOrigin_, QSize(1, 1)));
      rubberBand_->show();
    }
  }

  void updateSelectionRubberBand(const QPoint &areaPos)
  {
    if (!selectionRubberBandMode_ || !rubberBand_) {
      return;
    }
    const QPoint clamped = clampToDisplayArea(areaPos);
    rubberBand_->setGeometry(QRect(rubberBandOrigin_, clamped).normalized());
  }

  void finishSelectionRubberBand(const QPoint &areaPos)
  {
    selectionRubberBandPending_ = false;
    if (!selectionRubberBandMode_) {
      return;
    }
    selectionRubberBandMode_ = false;
    rubberBandActive_ = false;
    if (rubberBand_) {
      rubberBand_->hide();
    }
    if (!displayArea_) {
      return;
    }
    const QPoint clamped = clampToDisplayArea(areaPos);
    QRect rect = QRect(rubberBandOrigin_, clamped).normalized();
    applySelectionRect(rect);
  }

  void applySelectionRect(const QRect &rect)
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      clearSelections();
      return;
    }

    clearSelections();

    QList<QWidget *> matched;
    auto considerList = [&](const auto &list) {
      for (auto *element : list) {
        if (!element || !element->isVisible()) {
          continue;
        }
        if (rect.contains(element->geometry())) {
          matched.append(element);
        }
      }
    };

    considerList(textElements_);
    considerList(textEntryElements_);
    considerList(sliderElements_);
    considerList(wheelSwitchElements_);
    considerList(choiceButtonElements_);
    considerList(menuElements_);
    considerList(messageButtonElements_);
    considerList(shellCommandElements_);
    considerList(relatedDisplayElements_);
    considerList(textMonitorElements_);
    considerList(meterElements_);
    considerList(barMonitorElements_);
    considerList(scaleMonitorElements_);
    considerList(stripChartElements_);
    considerList(cartesianPlotElements_);
    considerList(byteMonitorElements_);
    considerList(rectangleElements_);
    considerList(imageElements_);
    considerList(ovalElements_);
    considerList(arcElements_);
    considerList(lineElements_);
    considerList(polylineElements_);
    considerList(polygonElements_);

    if (matched.isEmpty()) {
      return;
    }

    if (matched.size() == 1) {
      selectWidgetForEditing(matched.front());
      return;
    }

    multiSelection_.clear();
    for (QWidget *widget : matched) {
      setWidgetSelectionState(widget, true);
      multiSelection_.append(QPointer<QWidget>(widget));
    }
    showResourcePaletteForMultipleSelection();
  }

  void handleDisplayBackgroundClick()
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }
    clearMultiSelection();
    clearRectangleSelection();
    clearOvalSelection();
    clearTextSelection();
    clearTextEntrySelection();
    clearSliderSelection();
    clearWheelSwitchSelection();
    clearChoiceButtonSelection();
    clearMenuSelection();
    clearMessageButtonSelection();
    clearShellCommandSelection();
    clearRelatedDisplaySelection();
    clearTextMonitorSelection();
    clearMeterSelection();
    clearScaleMonitorSelection();
    clearStripChartSelection();
    clearCartesianPlotSelection();
    clearBarMonitorSelection();
    clearByteMonitorSelection();
    clearImageSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();

    if (displaySelected_) {
      clearDisplaySelection();
      closeResourcePalette();
      return;
    }

    if (ensureResourcePalette()) {
      for (auto &display : state->displays) {
        if (!display.isNull() && display != this) {
          display->clearSelections();
        }
      }
      setDisplaySelected(true);
      showResourcePaletteForDisplay();
    }
  }

  void startCreateRubberBand(const QPoint &areaPos, CreateTool tool)
  {
    selectionRubberBandPending_ = false;
    selectionRubberBandMode_ = false;
    rubberBandActive_ = true;
    activeRubberBandTool_ = tool;
    rubberBandOrigin_ = clampToDisplayArea(areaPos);
    ensureRubberBand();
    if (rubberBand_) {
      rubberBand_->setGeometry(QRect(rubberBandOrigin_, QSize(1, 1)));
      rubberBand_->show();
    }
  }

  void updateCreateRubberBand(const QPoint &areaPos)
  {
    if (!rubberBandActive_ || !rubberBand_) {
      return;
    }
    const QPoint clamped = clampToDisplayArea(areaPos);
    rubberBand_->setGeometry(QRect(rubberBandOrigin_, clamped).normalized());
  }

  void finishCreateRubberBand(const QPoint &areaPos)
  {
    if (!rubberBandActive_) {
      return;
    }
    rubberBandActive_ = false;
    selectionRubberBandMode_ = false;
    CreateTool tool = activeRubberBandTool_;
    activeRubberBandTool_ = CreateTool::kNone;
    if (rubberBand_) {
      rubberBand_->hide();
    }
    if (!displayArea_) {
      return;
    }
    const QPoint clamped = clampToDisplayArea(areaPos);
    QRect rect = QRect(rubberBandOrigin_, clamped).normalized();
    switch (tool) {
    case CreateTool::kText:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createTextElement(rect);
      break;
    case CreateTool::kTextMonitor:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createTextMonitorElement(rect);
      break;
    case CreateTool::kTextEntry:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createTextEntryElement(rect);
      break;
    case CreateTool::kSlider:
      if (rect.width() < kMinimumSliderWidth) {
        rect.setWidth(kMinimumSliderWidth);
      }
      if (rect.height() < kMinimumSliderHeight) {
        rect.setHeight(kMinimumSliderHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createSliderElement(rect);
      break;
    case CreateTool::kWheelSwitch:
      if (rect.width() < kMinimumWheelSwitchWidth) {
        rect.setWidth(kMinimumWheelSwitchWidth);
      }
      if (rect.height() < kMinimumWheelSwitchHeight) {
        rect.setHeight(kMinimumWheelSwitchHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createWheelSwitchElement(rect);
      break;
    case CreateTool::kChoiceButton:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createChoiceButtonElement(rect);
      break;
    case CreateTool::kMenu:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createMenuElement(rect);
      break;
    case CreateTool::kMessageButton:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createMessageButtonElement(rect);
      break;
    case CreateTool::kShellCommand:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createShellCommandElement(rect);
      break;
    case CreateTool::kMeter:
      if (rect.width() < kMinimumMeterSize) {
        rect.setWidth(kMinimumMeterSize);
      }
      if (rect.height() < kMinimumMeterSize) {
        rect.setHeight(kMinimumMeterSize);
      }
      rect = adjustRectToDisplayArea(rect);
      createMeterElement(rect);
      break;
    case CreateTool::kBarMonitor:
      if (rect.width() < kMinimumBarSize) {
        rect.setWidth(kMinimumBarSize);
      }
      if (rect.height() < kMinimumBarSize) {
        rect.setHeight(kMinimumBarSize);
      }
      rect = adjustRectToDisplayArea(rect);
      createBarMonitorElement(rect);
      break;
    case CreateTool::kByteMonitor:
      if (rect.width() < kMinimumByteSize) {
        rect.setWidth(kMinimumByteSize);
      }
      if (rect.height() < kMinimumByteSize) {
        rect.setHeight(kMinimumByteSize);
      }
      rect = adjustRectToDisplayArea(rect);
      createByteMonitorElement(rect);
      break;
    case CreateTool::kScaleMonitor:
      if (rect.width() < kMinimumScaleSize) {
        rect.setWidth(kMinimumScaleSize);
      }
      if (rect.height() < kMinimumScaleSize) {
        rect.setHeight(kMinimumScaleSize);
      }
      rect = adjustRectToDisplayArea(rect);
      createScaleMonitorElement(rect);
      break;
    case CreateTool::kStripChart:
      if (rect.width() < kMinimumStripChartWidth) {
        rect.setWidth(kMinimumStripChartWidth);
      }
      if (rect.height() < kMinimumStripChartHeight) {
        rect.setHeight(kMinimumStripChartHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createStripChartElement(rect);
      break;
    case CreateTool::kCartesianPlot:
      if (rect.width() < kMinimumCartesianPlotWidth) {
        rect.setWidth(kMinimumCartesianPlotWidth);
      }
      if (rect.height() < kMinimumCartesianPlotHeight) {
        rect.setHeight(kMinimumCartesianPlotHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createCartesianPlotElement(rect);
      break;
    case CreateTool::kRectangle:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = adjustRectToDisplayArea(rect);
      createRectangleElement(rect);
      break;
    case CreateTool::kOval:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = adjustRectToDisplayArea(rect);
      createOvalElement(rect);
      break;
    case CreateTool::kArc:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = adjustRectToDisplayArea(rect);
      createArcElement(rect);
      break;
    case CreateTool::kLine:
      createLineElement(rubberBandOrigin_, clamped);
      break;
    case CreateTool::kImage:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = adjustRectToDisplayArea(rect);
      createImageElement(rect);
      break;
    case CreateTool::kRelatedDisplay:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = adjustRectToDisplayArea(rect);
      createRelatedDisplayElement(rect);
      break;
    default:
      break;
    }
  }

  void handlePolygonClick(const QPoint &areaPos, Qt::KeyboardModifiers modifiers)
  {
    if (!displayArea_) {
      return;
    }

    const QPoint point = polygonCreationActive_
        ? adjustedPolygonPoint(areaPos, modifiers)
        : clampToDisplayArea(areaPos);

    if (!polygonCreationActive_) {
      polygonCreationActive_ = true;
      polygonCreationPoints_.clear();
      polygonCreationPoints_.append(point);
      if (activePolygonElement_) {
        removeElementFromStack(activePolygonElement_);
        activePolygonElement_->deleteLater();
      }
      activePolygonElement_ = new PolygonElement(displayArea_);
      activePolygonElement_->show();
      bringElementToFront(activePolygonElement_);
      QVector<QPoint> preview{point, point};
      activePolygonElement_->setAbsolutePoints(preview);
      return;
    }

    if (polygonCreationPoints_.isEmpty()
        || polygonCreationPoints_.last() != point) {
      polygonCreationPoints_.append(point);
    }
    updatePolygonPreview(point, modifiers);
  }

  void handlePolygonDoubleClick(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    if (!polygonCreationActive_) {
      return;
    }

    const QPoint point = adjustedPolygonPoint(areaPos, modifiers);
    if (polygonCreationPoints_.isEmpty()
        || polygonCreationPoints_.last() != point) {
      polygonCreationPoints_.append(point);
    }
    finalizePolygonCreation();
  }

  void updatePolygonPreview(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    if (!polygonCreationActive_ || !activePolygonElement_) {
      return;
    }

    const QPoint previewPoint = adjustedPolygonPoint(areaPos, modifiers);
    QVector<QPoint> preview = polygonCreationPoints_;
    if (preview.isEmpty()) {
      preview.append(previewPoint);
      preview.append(previewPoint);
    } else {
      preview.append(previewPoint);
    }
    activePolygonElement_->setAbsolutePoints(preview);
    bringElementToFront(activePolygonElement_);
    activePolygonElement_->update();
  }

  void handlePolylineClick(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    if (!displayArea_) {
      return;
    }

    const QPoint point = polylineCreationActive_
        ? adjustedPolylinePoint(areaPos, modifiers)
        : clampToDisplayArea(areaPos);

    if (!polylineCreationActive_) {
      polylineCreationActive_ = true;
      polylineCreationPoints_.clear();
      polylineCreationPoints_.append(point);
      if (activePolylineElement_) {
        removeElementFromStack(activePolylineElement_);
        activePolylineElement_->deleteLater();
      }
      activePolylineElement_ = new PolylineElement(displayArea_);
      activePolylineElement_->show();
      bringElementToFront(activePolylineElement_);
      QVector<QPoint> preview{point, point};
      activePolylineElement_->setAbsolutePoints(preview);
      return;
    }

    if (polylineCreationPoints_.isEmpty()
        || polylineCreationPoints_.last() != point) {
      polylineCreationPoints_.append(point);
    }
    updatePolylinePreview(point, modifiers);
  }

  void handlePolylineDoubleClick(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    if (!polylineCreationActive_) {
      return;
    }

    const QPoint point = adjustedPolylinePoint(areaPos, modifiers);
    if (polylineCreationPoints_.isEmpty()
        || polylineCreationPoints_.last() != point) {
      polylineCreationPoints_.append(point);
    }
    finalizePolylineCreation();
  }

  void updatePolylinePreview(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    if (!polylineCreationActive_ || !activePolylineElement_) {
      return;
    }

    const QPoint previewPoint = adjustedPolylinePoint(areaPos, modifiers);
    QVector<QPoint> preview = polylineCreationPoints_;
    if (preview.isEmpty()) {
      preview.append(previewPoint);
    } else {
      preview.append(previewPoint);
    }
    activePolylineElement_->setAbsolutePoints(preview);
    bringElementToFront(activePolylineElement_);
    activePolylineElement_->update();
  }

  void finalizePolygonCreation()
  {
    if (!polygonCreationActive_ || !activePolygonElement_) {
      cancelPolygonCreation();
      return;
    }

    QVector<QPoint> finalPoints = polygonCreationPoints_;
    if (finalPoints.size() < 3) {
      cancelPolygonCreation();
      return;
    }
    if (finalPoints.first() != finalPoints.last()) {
      finalPoints.append(finalPoints.first());
    }
    activePolygonElement_->setAbsolutePoints(finalPoints);
    PolygonElement *element = activePolygonElement_;
    polygonCreationActive_ = false;
    polygonCreationPoints_.clear();
    activePolygonElement_ = nullptr;
    polygonElements_.append(element);
    selectPolygonElement(element);
    showResourcePaletteForPolygon(element);
    deactivateCreateTool();
    markDirty();
  }

  void finalizePolylineCreation()
  {
    if (!polylineCreationActive_ || !activePolylineElement_) {
      cancelPolylineCreation();
      return;
    }

    QVector<QPoint> finalPoints = polylineCreationPoints_;
    if (finalPoints.size() < 2) {
      cancelPolylineCreation();
      return;
    }

    activePolylineElement_->setAbsolutePoints(finalPoints);
    PolylineElement *element = activePolylineElement_;
    polylineCreationActive_ = false;
    polylineCreationPoints_.clear();
    activePolylineElement_ = nullptr;
    polylineElements_.append(element);
    selectPolylineElement(element);
    showResourcePaletteForPolyline(element);
    deactivateCreateTool();
    markDirty();
  }

  void cancelPolygonCreation()
  {
    if (!polygonCreationActive_ && !activePolygonElement_) {
      polygonCreationPoints_.clear();
      return;
    }

    polygonCreationActive_ = false;
    polygonCreationPoints_.clear();
    if (activePolygonElement_) {
      removeElementFromStack(activePolygonElement_);
      activePolygonElement_->deleteLater();
      activePolygonElement_ = nullptr;
    }
  }

  void cancelPolylineCreation()
  {
    if (!polylineCreationActive_ && !activePolylineElement_) {
      polylineCreationPoints_.clear();
      return;
    }

    polylineCreationActive_ = false;
    polylineCreationPoints_.clear();
    if (activePolylineElement_) {
      removeElementFromStack(activePolylineElement_);
      activePolylineElement_->deleteLater();
      activePolylineElement_ = nullptr;
    }
  }

  QPoint adjustedPolygonPoint(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    return adjustedPathPoint(polygonCreationPoints_, areaPos, modifiers);
  }

  QPoint adjustedPolylinePoint(const QPoint &areaPos,
      Qt::KeyboardModifiers modifiers)
  {
    return adjustedPathPoint(polylineCreationPoints_, areaPos, modifiers);
  }

  QPoint adjustedPathPoint(const QVector<QPoint> &points,
      const QPoint &areaPos, Qt::KeyboardModifiers modifiers)
  {
    QPoint clamped = clampToDisplayArea(areaPos);
    if (!(modifiers & Qt::ShiftModifier) || points.isEmpty()) {
      return clamped;
    }

    const QPoint &reference = points.last();
    const int dx = clamped.x() - reference.x();
    const int dy = clamped.y() - reference.y();
    if (dx == 0 && dy == 0) {
      return clamped;
    }

    constexpr double kPi = 3.14159265358979323846;
    double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle < 0.0) {
      angle += 2.0 * kPi;
    }
    const double step = kPi / 4.0;
    const int index = static_cast<int>(std::round(angle / step));
    const double snapped = index * step;
    const double length = std::sqrt(static_cast<double>(dx * dx + dy * dy));
    const int x = reference.x()
        + static_cast<int>(std::round(std::cos(snapped) * length));
    const int y = reference.y()
        + static_cast<int>(std::round(std::sin(snapped) * length));
    return clampToDisplayArea(QPoint(x, y));
  }

  void createTextElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.height() < kMinimumTextElementHeight) {
      target.setHeight(kMinimumTextElementHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new TextElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->setText(QStringLiteral("Text"));
    element->show();
    ensureElementInStack(element);
    textElements_.append(element);
    selectTextElement(element);
    showResourcePaletteForText(element);
    deactivateCreateTool();
    markDirty();
  }

  void createTextMonitorElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = adjustRectToDisplayArea(rect);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new TextMonitorElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->setText(element->channel(0));
    element->show();
    ensureElementInStack(element);
    textMonitorElements_.append(element);
    selectTextMonitorElement(element);
    showResourcePaletteForTextMonitor(element);
    deactivateCreateTool();
    markDirty();
  }

  void createTextEntryElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumTextWidth) {
      target.setWidth(kMinimumTextWidth);
    }
    if (target.height() < kMinimumTextHeight) {
      target.setHeight(kMinimumTextHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new TextEntryElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    textEntryElements_.append(element);
    selectTextEntryElement(element);
    showResourcePaletteForTextEntry(element);
    deactivateCreateTool();
    markDirty();
  }

  void createSliderElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumSliderWidth) {
      target.setWidth(kMinimumSliderWidth);
    }
    if (target.height() < kMinimumSliderHeight) {
      target.setHeight(kMinimumSliderHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new SliderElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    sliderElements_.append(element);
    selectSliderElement(element);
    showResourcePaletteForSlider(element);
    deactivateCreateTool();
    markDirty();
  }

  void createWheelSwitchElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumWheelSwitchWidth) {
      target.setWidth(kMinimumWheelSwitchWidth);
    }
    if (target.height() < kMinimumWheelSwitchHeight) {
      target.setHeight(kMinimumWheelSwitchHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new WheelSwitchElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    wheelSwitchElements_.append(element);
    selectWheelSwitchElement(element);
    showResourcePaletteForWheelSwitch(element);
    deactivateCreateTool();
    markDirty();
  }

  void createChoiceButtonElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumTextWidth) {
      target.setWidth(kMinimumTextWidth);
    }
    if (target.height() < kMinimumTextHeight) {
      target.setHeight(kMinimumTextHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new ChoiceButtonElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    choiceButtonElements_.append(element);
    selectChoiceButtonElement(element);
    showResourcePaletteForChoiceButton(element);
    deactivateCreateTool();
    markDirty();
  }

  void createMenuElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumTextWidth) {
      target.setWidth(kMinimumTextWidth);
    }
    if (target.height() < kMinimumTextHeight) {
      target.setHeight(kMinimumTextHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new MenuElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    menuElements_.append(element);
    selectMenuElement(element);
    showResourcePaletteForMenu(element);
    deactivateCreateTool();
    markDirty();
  }

  void createMessageButtonElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumTextWidth) {
      target.setWidth(kMinimumTextWidth);
    }
    if (target.height() < kMinimumTextHeight) {
      target.setHeight(kMinimumTextHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new MessageButtonElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    messageButtonElements_.append(element);
    selectMessageButtonElement(element);
    showResourcePaletteForMessageButton(element);
    deactivateCreateTool();
    markDirty();
  }

  void createShellCommandElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumTextWidth) {
      target.setWidth(kMinimumTextWidth);
    }
    if (target.height() < kMinimumTextHeight) {
      target.setHeight(kMinimumTextHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new ShellCommandElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->setLabel(QStringLiteral("Shell Command"));
    element->show();
    ensureElementInStack(element);
    shellCommandElements_.append(element);
    selectShellCommandElement(element);
    showResourcePaletteForShellCommand(element);
    deactivateCreateTool();
    markDirty();
  }

  void createRelatedDisplayElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumTextWidth) {
      target.setWidth(kMinimumTextWidth);
    }
    if (target.height() < kMinimumTextHeight) {
      target.setHeight(kMinimumTextHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new RelatedDisplayElement(displayArea_);
    element->setFont(font());
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    relatedDisplayElements_.append(element);
    selectRelatedDisplayElement(element);
    showResourcePaletteForRelatedDisplay(element);
    deactivateCreateTool();
    markDirty();
  }

  void createMeterElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumMeterSize) {
      target.setWidth(kMinimumMeterSize);
    }
    if (target.height() < kMinimumMeterSize) {
      target.setHeight(kMinimumMeterSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new MeterElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    meterElements_.append(element);
    selectMeterElement(element);
    showResourcePaletteForMeter(element);
    deactivateCreateTool();
    markDirty();
  }

  void createBarMonitorElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumBarSize) {
      target.setWidth(kMinimumBarSize);
    }
    if (target.height() < kMinimumBarSize) {
      target.setHeight(kMinimumBarSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new BarMonitorElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    barMonitorElements_.append(element);
    selectBarMonitorElement(element);
    showResourcePaletteForBar(element);
    deactivateCreateTool();
    markDirty();
  }

  void createScaleMonitorElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumScaleSize) {
      target.setWidth(kMinimumScaleSize);
    }
    if (target.height() < kMinimumScaleSize) {
      target.setHeight(kMinimumScaleSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new ScaleMonitorElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    scaleMonitorElements_.append(element);
    selectScaleMonitorElement(element);
    showResourcePaletteForScale(element);
    deactivateCreateTool();
    markDirty();
  }

  void createStripChartElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumStripChartWidth) {
      target.setWidth(kMinimumStripChartWidth);
    }
    if (target.height() < kMinimumStripChartHeight) {
      target.setHeight(kMinimumStripChartHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new StripChartElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    stripChartElements_.append(element);
    selectStripChartElement(element);
    showResourcePaletteForStripChart(element);
    deactivateCreateTool();
    markDirty();
  }

  void createCartesianPlotElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumCartesianPlotWidth) {
      target.setWidth(kMinimumCartesianPlotWidth);
    }
    if (target.height() < kMinimumCartesianPlotHeight) {
      target.setHeight(kMinimumCartesianPlotHeight);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new CartesianPlotElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    cartesianPlotElements_.append(element);
    selectCartesianPlotElement(element);
    showResourcePaletteForCartesianPlot(element);
    deactivateCreateTool();
    markDirty();
  }

  void createByteMonitorElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumByteSize) {
      target.setWidth(kMinimumByteSize);
    }
    if (target.height() < kMinimumByteSize) {
      target.setHeight(kMinimumByteSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new ByteMonitorElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    byteMonitorElements_.append(element);
    selectByteMonitorElement(element);
    showResourcePaletteForByte(element);
    deactivateCreateTool();
    markDirty();
  }

  void createRectangleElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumRectangleSize) {
      target.setWidth(kMinimumRectangleSize);
    }
    if (target.height() < kMinimumRectangleSize) {
      target.setHeight(kMinimumRectangleSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new RectangleElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    rectangleElements_.append(element);
    selectRectangleElement(element);
    showResourcePaletteForRectangle(element);
    deactivateCreateTool();
    markDirty();
  }

  void createImageElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumRectangleSize) {
      target.setWidth(kMinimumRectangleSize);
    }
    if (target.height() < kMinimumRectangleSize) {
      target.setHeight(kMinimumRectangleSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new ImageElement(displayArea_);
    element->setGeometry(target);
    if (!filePath_.isEmpty()) {
      element->setBaseDirectory(QFileInfo(filePath_).absolutePath());
    }
    element->show();
    ensureElementInStack(element);
    imageElements_.append(element);
    selectImageElement(element);
    showResourcePaletteForImage(element);
    deactivateCreateTool();
    markDirty();
  }

  void createOvalElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumRectangleSize) {
      target.setWidth(kMinimumRectangleSize);
    }
    if (target.height() < kMinimumRectangleSize) {
      target.setHeight(kMinimumRectangleSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new OvalElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    ovalElements_.append(element);
    selectOvalElement(element);
    showResourcePaletteForOval(element);
    deactivateCreateTool();
    markDirty();
  }

  void createArcElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    QRect target = rect;
    if (target.width() < kMinimumRectangleSize) {
      target.setWidth(kMinimumRectangleSize);
    }
    if (target.height() < kMinimumRectangleSize) {
      target.setHeight(kMinimumRectangleSize);
    }
    target = adjustRectToDisplayArea(target);
    if (target.width() <= 0 || target.height() <= 0) {
      return;
    }
    auto *element = new ArcElement(displayArea_);
    element->setGeometry(target);
    element->show();
    ensureElementInStack(element);
    arcElements_.append(element);
    selectArcElement(element);
    showResourcePaletteForArc(element);
    deactivateCreateTool();
    markDirty();
  }

  void createLineElement(const QPoint &startPoint, const QPoint &endPoint)
  {
    if (!displayArea_) {
      return;
    }
    QPoint clampedStart = clampToDisplayArea(startPoint);
    QPoint clampedEnd = clampToDisplayArea(endPoint);
    QRect rect(clampedStart, clampedEnd);
    rect = rect.normalized();
    if (rect.width() < 1) {
      rect.setWidth(1);
    }
    if (rect.height() < 1) {
      rect.setHeight(1);
    }
    rect = adjustRectToDisplayArea(rect);

    auto clampLocalPoint = [](const QPoint &point, const QSize &size) {
      const int maxX = std::max(0, size.width() - 1);
      const int maxY = std::max(0, size.height() - 1);
      const int x = std::clamp(point.x(), 0, maxX);
      const int y = std::clamp(point.y(), 0, maxY);
      return QPoint(x, y);
    };

    QPoint localStart = clampLocalPoint(clampedStart - rect.topLeft(), rect.size());
    QPoint localEnd = clampLocalPoint(clampedEnd - rect.topLeft(), rect.size());

    auto *element = new LineElement(displayArea_);
    element->setGeometry(rect);
    element->setLocalEndpoints(localStart, localEnd);
    element->show();
    ensureElementInStack(element);
    lineElements_.append(element);
    selectLineElement(element);
    showResourcePaletteForLine(element);
    deactivateCreateTool();
    markDirty();
  }

  void ensureRubberBand()
  {
    if (!rubberBand_) {
      rubberBand_ = new QRubberBand(QRubberBand::Rectangle, displayArea_);
    }
  }

  QPoint clampToDisplayArea(const QPoint &areaPos) const
  {
    if (!displayArea_) {
      return areaPos;
    }
    const QRect areaRect = displayArea_->rect();
    const int x = std::clamp(areaPos.x(), areaRect.left(), areaRect.right());
    const int y = std::clamp(areaPos.y(), areaRect.top(), areaRect.bottom());
    return QPoint(x, y);
  }

  QRect adjustRectToDisplayArea(const QRect &rect) const
  {
    if (!displayArea_) {
      return rect;
    }
    const QRect areaRect = displayArea_->rect();
    int width = std::min(rect.width(), areaRect.width());
    int height = std::min(rect.height(), areaRect.height());
    int x = std::clamp(rect.x(), areaRect.left(),
        areaRect.right() - width + 1);
    int y = std::clamp(rect.y(), areaRect.top(),
        areaRect.bottom() - height + 1);
    return QRect(QPoint(x, y), QSize(width, height));
  }

  void updateCreateCursor()
  {
    auto state = state_.lock();
    const bool crossCursorActive = state
        && (state->createTool == CreateTool::kText
            || state->createTool == CreateTool::kTextMonitor
            || state->createTool == CreateTool::kTextEntry
            || state->createTool == CreateTool::kSlider
            || state->createTool == CreateTool::kWheelSwitch
            || state->createTool == CreateTool::kChoiceButton
            || state->createTool == CreateTool::kMenu
            || state->createTool == CreateTool::kMessageButton
            || state->createTool == CreateTool::kShellCommand
            || state->createTool == CreateTool::kMeter
            || state->createTool == CreateTool::kBarMonitor
            || state->createTool == CreateTool::kByteMonitor
            || state->createTool == CreateTool::kScaleMonitor
            || state->createTool == CreateTool::kStripChart
            || state->createTool == CreateTool::kCartesianPlot
            || state->createTool == CreateTool::kRectangle
            || state->createTool == CreateTool::kOval
            || state->createTool == CreateTool::kArc
            || state->createTool == CreateTool::kPolygon
            || state->createTool == CreateTool::kPolyline
            || state->createTool == CreateTool::kLine
            || state->createTool == CreateTool::kImage
            || state->createTool == CreateTool::kRelatedDisplay);
    if (displayArea_) {
      if (crossCursorActive) {
        displayArea_->setCursor(Qt::CrossCursor);
      } else {
        displayArea_->unsetCursor();
      }
    }
    if (crossCursorActive) {
      setCursor(Qt::CrossCursor);
    } else {
      unsetCursor();
    }
  }

  void activateCreateTool(CreateTool tool)
  {
    if (auto state = state_.lock(); state && state->editMode) {
      for (auto &display : state->displays) {
        if (!display.isNull()) {
          display->cancelPolygonCreation();
          display->cancelPolylineCreation();
          display->clearSelections();
        }
      }
      state->createTool = tool;
      for (auto &display : state->displays) {
        if (!display.isNull()) {
          display->updateCreateCursor();
        }
      }
      rubberBandActive_ = false;
      activeRubberBandTool_ = CreateTool::kNone;
      selectionRubberBandPending_ = false;
      selectionRubberBandMode_ = false;
      if (rubberBand_) {
        rubberBand_->hide();
      }
    }
  }

  void deactivateCreateTool()
  {
    if (auto state = state_.lock(); state
        && state->createTool != CreateTool::kNone) {
      state->createTool = CreateTool::kNone;
      for (auto &display : state->displays) {
        if (!display.isNull()) {
          display->cancelPolygonCreation();
          display->cancelPolylineCreation();
          display->updateCreateCursor();
        }
      }
    }
    rubberBandActive_ = false;
    activeRubberBandTool_ = CreateTool::kNone;
    selectionRubberBandPending_ = false;
    selectionRubberBandMode_ = false;
    cancelPolygonCreation();
    cancelPolylineCreation();
    if (rubberBand_) {
      rubberBand_->hide();
    }
  }

  void showEditContextMenu(const QPoint &globalPos)
  {
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("editModeContextMenu"));
    menu.setSeparatorsCollapsible(false);

    auto addMenuAction = [](QMenu *target, const QString &text,
        const QKeySequence &shortcut = QKeySequence()) {
      QAction *action = target->addAction(text);
      if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
        action->setShortcutVisibleInContextMenu(true);
      }
      return action;
    };

    auto *objectMenu = menu.addMenu(QStringLiteral("Object"));

    auto *graphicsMenu = objectMenu->addMenu(QStringLiteral("Graphics"));
    auto *textAction =
        addMenuAction(graphicsMenu, QStringLiteral("Text"));
    QObject::connect(textAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kText);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *rectangleAction =
        addMenuAction(graphicsMenu, QStringLiteral("Rectangle"));
    QObject::connect(rectangleAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kRectangle);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *lineAction =
        addMenuAction(graphicsMenu, QStringLiteral("Line"));
    QObject::connect(lineAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kLine);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *polygonAction =
        addMenuAction(graphicsMenu, QStringLiteral("Polygon"));
    QObject::connect(polygonAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kPolygon);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *polylineAction =
        addMenuAction(graphicsMenu, QStringLiteral("Polyline"));
    QObject::connect(polylineAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kPolyline);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *ovalAction = addMenuAction(graphicsMenu, QStringLiteral("Oval"));
    QObject::connect(ovalAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kOval);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *arcAction = addMenuAction(graphicsMenu, QStringLiteral("Arc"));
    QObject::connect(arcAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kArc);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *imageAction =
        addMenuAction(graphicsMenu, QStringLiteral("Image"));
    QObject::connect(imageAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kImage);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });

    auto *monitorsMenu = objectMenu->addMenu(QStringLiteral("Monitors"));
    auto *textMonitorAction =
        addMenuAction(monitorsMenu, QStringLiteral("Text Monitor"));
    QObject::connect(textMonitorAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kTextMonitor);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *meterAction = addMenuAction(monitorsMenu, QStringLiteral("Meter"));
    QObject::connect(meterAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kMeter);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *barAction = addMenuAction(monitorsMenu, QStringLiteral("Bar Monitor"));
    QObject::connect(barAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kBarMonitor);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *byteAction = addMenuAction(monitorsMenu, QStringLiteral("Byte Monitor"));
    QObject::connect(byteAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kByteMonitor);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *scaleAction =
        addMenuAction(monitorsMenu, QStringLiteral("Scale Monitor"));
    QObject::connect(scaleAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kScaleMonitor);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *stripChartAction =
        addMenuAction(monitorsMenu, QStringLiteral("Strip Chart"));
    QObject::connect(stripChartAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kStripChart);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *cartesianAction =
        addMenuAction(monitorsMenu, QStringLiteral("Cartesian Plot"));
    QObject::connect(cartesianAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kCartesianPlot);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });

    auto *controllersMenu = objectMenu->addMenu(QStringLiteral("Controllers"));
    auto *textEntryAction =
        addMenuAction(controllersMenu, QStringLiteral("Text Entry"));
    QObject::connect(textEntryAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kTextEntry);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *choiceButtonAction =
        addMenuAction(controllersMenu, QStringLiteral("Choice Button"));
    QObject::connect(choiceButtonAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kChoiceButton);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *menuControllerAction =
        addMenuAction(controllersMenu, QStringLiteral("Menu"));
    QObject::connect(menuControllerAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kMenu);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *sliderAction =
        addMenuAction(controllersMenu, QStringLiteral("Slider"));
    QObject::connect(sliderAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kSlider);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *messageButtonAction =
        addMenuAction(controllersMenu, QStringLiteral("Message Button"));
    QObject::connect(messageButtonAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kMessageButton);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *relatedDisplayAction =
        addMenuAction(controllersMenu, QStringLiteral("Related Display"));
    QObject::connect(relatedDisplayAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kRelatedDisplay);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *shellCommandAction =
        addMenuAction(controllersMenu, QStringLiteral("Shell Command"));
    QObject::connect(shellCommandAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kShellCommand);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });
    auto *wheelSwitchAction =
        addMenuAction(controllersMenu, QStringLiteral("Wheel Switch"));
    QObject::connect(wheelSwitchAction, &QAction::triggered, this, [this]() {
      activateCreateTool(CreateTool::kWheelSwitch);
      if (!lastContextMenuGlobalPos_.isNull()) {
        QCursor::setPos(lastContextMenuGlobalPos_);
      }
    });

    addMenuAction(&menu, QStringLiteral("Undo"));

    menu.addSeparator();
    auto *cutAction = addMenuAction(&menu, QStringLiteral("Cut"),
        QKeySequence(Qt::CTRL | Qt::Key_X));
    QObject::connect(cutAction, &QAction::triggered, this, [this]() {
      setAsActiveDisplay();
      cutSelection();
    });
    auto *copyAction = addMenuAction(&menu, QStringLiteral("Copy"),
        QKeySequence(Qt::CTRL | Qt::Key_C));
    copyAction->setEnabled(hasAnyElementSelection());
    QObject::connect(copyAction, &QAction::triggered, this, [this]() {
      setAsActiveDisplay();
      copySelection();
    });
    auto *pasteAction = addMenuAction(&menu, QStringLiteral("Paste"),
        QKeySequence(Qt::CTRL | Qt::Key_V));
    pasteAction->setEnabled(canPaste());
    QObject::connect(pasteAction, &QAction::triggered, this, [this]() {
      setAsActiveDisplay();
      pasteSelection();
    });

    menu.addSeparator();
    auto *raiseAction = addMenuAction(&menu, QStringLiteral("Raise"));
    raiseAction->setEnabled(canRaiseSelection());
    QObject::connect(raiseAction, &QAction::triggered, this, [this]() {
      raiseSelection();
    });
    auto *lowerAction = addMenuAction(&menu, QStringLiteral("Lower"));
    lowerAction->setEnabled(canLowerSelection());
    QObject::connect(lowerAction, &QAction::triggered, this, [this]() {
      lowerSelection();
    });

    menu.addSeparator();
    auto *groupAction = addMenuAction(&menu, QStringLiteral("Group"));
    groupAction->setEnabled(canGroupSelection());
    QObject::connect(groupAction, &QAction::triggered, this, [this]() {
      groupSelectedElements();
    });
    auto *ungroupAction = addMenuAction(&menu, QStringLiteral("Ungroup"));
    ungroupAction->setEnabled(canUngroupSelection());
    QObject::connect(ungroupAction, &QAction::triggered, this, [this]() {
      ungroupSelectedElements();
    });

    menu.addSeparator();
    auto *alignMenu = menu.addMenu(QStringLiteral("Align"));
    addMenuAction(alignMenu, QStringLiteral("Left"));
    addMenuAction(alignMenu, QStringLiteral("Horizontal Center"));
    addMenuAction(alignMenu, QStringLiteral("Right"));
    addMenuAction(alignMenu, QStringLiteral("Top"));
    addMenuAction(alignMenu, QStringLiteral("Vertical Center"));
    addMenuAction(alignMenu, QStringLiteral("Bottom"));
    addMenuAction(alignMenu, QStringLiteral("Position to Grid"));
    addMenuAction(alignMenu, QStringLiteral("Edges to Grid"));

    auto *spaceMenu = menu.addMenu(QStringLiteral("Space Evenly"));
    addMenuAction(spaceMenu, QStringLiteral("Horizontal"));
    addMenuAction(spaceMenu, QStringLiteral("Vertical"));
    addMenuAction(spaceMenu, QStringLiteral("2-D"));

    auto *centerMenu = menu.addMenu(QStringLiteral("Center"));
    addMenuAction(centerMenu, QStringLiteral("Horizontally in Display"));
    addMenuAction(centerMenu, QStringLiteral("Vertically in Display"));
    addMenuAction(centerMenu, QStringLiteral("Both"));

    auto *orientMenu = menu.addMenu(QStringLiteral("Orient"));
    addMenuAction(orientMenu, QStringLiteral("Flip Horizontally"));
    addMenuAction(orientMenu, QStringLiteral("Flip Vertically"));
    addMenuAction(orientMenu, QStringLiteral("Rotate Clockwise"));
    addMenuAction(orientMenu, QStringLiteral("Rotate Counterclockwise"));

    auto *sizeMenu = menu.addMenu(QStringLiteral("Size"));
    addMenuAction(sizeMenu, QStringLiteral("Same Size"));
    addMenuAction(sizeMenu, QStringLiteral("Text to Contents"));

    auto *gridMenu = menu.addMenu(QStringLiteral("Grid"));
    addMenuAction(gridMenu, QStringLiteral("Toggle Show Grid"));
    addMenuAction(gridMenu, QStringLiteral("Toggle Snap To Grid"));
    addMenuAction(gridMenu, QStringLiteral("Grid Spacing..."));

    menu.addSeparator();
    auto *unselectAction = addMenuAction(&menu, QStringLiteral("Unselect"));
    unselectAction->setEnabled(hasAnySelection());
    QObject::connect(unselectAction, &QAction::triggered, this, [this]() {
      setAsActiveDisplay();
      clearSelection();
    });
    auto *selectAllAction = addMenuAction(&menu, QStringLiteral("Select All"));
    selectAllAction->setEnabled(canSelectAllElements());
    QObject::connect(selectAllAction, &QAction::triggered, this, [this]() {
      selectAllElements();
    });
    auto *selectDisplayAction =
        addMenuAction(&menu, QStringLiteral("Select Display"));
    selectDisplayAction->setEnabled(canSelectDisplay());
    QObject::connect(selectDisplayAction, &QAction::triggered, this, [this]() {
      selectDisplayElement();
    });

    menu.addSeparator();
    addMenuAction(&menu, QStringLiteral("Find Outliers"));
    addMenuAction(&menu, QStringLiteral("Refresh"));
    addMenuAction(&menu, QStringLiteral("Edit Summary..."));

    menu.exec(globalPos);
  }
};

inline void DisplayWindow::closeEvent(QCloseEvent *event)
{
  if (dirty_) {
    QString baseTitle = windowTitle();
    if (baseTitle.endsWith(QLatin1Char('*'))) {
      baseTitle.chop(1);
    }
    if (baseTitle.isEmpty()) {
      baseTitle = QStringLiteral("this display");
    }
    const QMessageBox::StandardButton choice = QMessageBox::warning(this,
        QStringLiteral("Close Display"),
        QStringLiteral("Save changes to %1?").arg(baseTitle),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Save) {
      if (!save(this)) {
        event->ignore();
        return;
      }
    } else if (choice == QMessageBox::Cancel) {
      event->ignore();
      return;
    }
  }
  QMainWindow::closeEvent(event);
  if (!event->isAccepted()) {
    return;
  }
  if (auto state = state_.lock()) {
    if (state->activeDisplay == this) {
      state->activeDisplay = nullptr;
    }
  }
  notifyMenus();
}

inline bool DisplayWindow::save(QWidget *dialogParent)
{
  QWidget *parent = dialogParent ? dialogParent : this;
  if (filePath_.isEmpty()) {
    return saveAs(parent);
  }
  if (!writeAdlFile(filePath_)) {
    QMessageBox::critical(parent, QStringLiteral("Save Display"),
        QStringLiteral("Failed to save display to:\n%1").arg(filePath_));
    return false;
  }
  dirty_ = false;
  setWindowTitle(QFileInfo(filePath_).fileName());
  updateDirtyIndicator();
  notifyMenus();
  return true;
}

inline bool DisplayWindow::saveAs(QWidget *dialogParent)
{
  QWidget *parent = dialogParent ? dialogParent : this;
  QString initialPath = filePath_;
  if (initialPath.isEmpty()) {
    QString baseName = windowTitle();
    if (baseName.endsWith(QLatin1Char('*'))) {
      baseName.chop(1);
      baseName = baseName.trimmed();
    }
    if (baseName.isEmpty()) {
      baseName = QStringLiteral("untitled.adl");
    } else if (!baseName.endsWith(QStringLiteral(".adl"), Qt::CaseInsensitive)) {
      baseName.append(QStringLiteral(".adl"));
    }
    initialPath = baseName;
  }

  QFileDialog dialog(parent, QStringLiteral("Save Display"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setNameFilter(QStringLiteral("MEDM Display Files (*.adl)"));
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  dialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);
  dialog.setModal(true);
  dialog.setWindowModality(Qt::ApplicationModal);
  dialog.setDefaultSuffix(QStringLiteral("adl"));

  const QFileInfo initialInfo(initialPath);
  if (initialInfo.exists() || !initialPath.isEmpty()) {
    dialog.setDirectory(initialInfo.absolutePath());
    dialog.selectFile(initialInfo.filePath());
  }

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  const QString selected = dialog.selectedFiles().value(0);
  if (selected.isEmpty()) {
    return false;
  }

  QString normalized = selected;
  if (!normalized.endsWith(QStringLiteral(".adl"), Qt::CaseInsensitive)) {
    normalized.append(QStringLiteral(".adl"));
  }

  if (!writeAdlFile(normalized)) {
    QMessageBox::critical(parent, QStringLiteral("Save Display"),
        QStringLiteral("Failed to save display to:\n%1").arg(normalized));
    return false;
  }

  filePath_ = QFileInfo(normalized).absoluteFilePath();
  setWindowTitle(QFileInfo(filePath_).fileName());
  dirty_ = false;
  updateDirtyIndicator();
  notifyMenus();
  return true;
}

inline bool DisplayWindow::loadFromFile(const QString &filePath,
    QString *errorMessage)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to open %1").arg(filePath);
    }
    return false;
  }

  QTextStream stream(&file);
  setUtf8Encoding(stream);
  const QString contents = stream.readAll();

  std::optional<AdlNode> document = AdlParser::parse(contents, errorMessage);
  if (!document) {
    return false;
  }

  clearAllElements();

  const QString previousLoadDirectory = currentLoadDirectory_;
  currentLoadDirectory_ = QFileInfo(filePath).absolutePath();

  bool displayLoaded = false;
  bool elementLoaded = false;
  for (const auto &child : document->children) {
    if (child.name.compare(QStringLiteral("display"), Qt::CaseInsensitive)
        == 0) {
      displayLoaded = loadDisplaySection(child) || displayLoaded;
      continue;
    }
    if (loadElementNode(child)) {
      elementLoaded = true;
      continue;
    }
  }

  filePath_ = QFileInfo(filePath).absoluteFilePath();
  setWindowTitle(QFileInfo(filePath_).fileName());

  dirty_ = false;
  updateDirtyIndicator();
  notifyMenus();
  if (displayArea_) {
    displayArea_->update();
  }
  update();
  if (auto state = state_.lock()) {
    state->createTool = CreateTool::kNone;
  }
  currentLoadDirectory_ = previousLoadDirectory;
  return displayLoaded || elementLoaded;
}

inline bool DisplayWindow::writeAdlFile(const QString &filePath) const
{
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  setUtf8Encoding(stream);

  auto resolveColor = [](const QWidget *widget, const QColor &candidate,
      QPalette::ColorRole role) {
    if (candidate.isValid()) {
      return candidate;
    }
    const QWidget *current = widget;
    while (current) {
      const QColor fromPalette = current->palette().color(role);
      if (fromPalette.isValid()) {
        return fromPalette;
      }
      current = current->parentWidget();
    }
    if (qApp) {
      const QColor appColor = qApp->palette().color(role);
      if (appColor.isValid()) {
        return appColor;
      }
    }
    return role == QPalette::WindowText ? QColor(Qt::black)
                                        : QColor(Qt::white);
  };

  auto resolvedForegroundColor = [&resolveColor](const QWidget *widget,
      const QColor &candidate) {
    return resolveColor(widget, candidate, QPalette::WindowText);
  };

  auto resolvedBackgroundColor = [&resolveColor](const QWidget *widget,
      const QColor &candidate) {
    return resolveColor(widget, candidate, QPalette::Window);
  };

  const QFileInfo info(filePath);
  QString fileName = info.filePath();
  if (info.isAbsolute()) {
    fileName = info.absoluteFilePath();
  }
  if (fileName.isEmpty()) {
    fileName = info.fileName();
  }
  fileName = QDir::cleanPath(fileName);
  AdlWriter::writeIndentedLine(stream, 0, QString());
  stream << "file {";
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("name=\"%1\"").arg(AdlWriter::escapeAdlString(fileName)));
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("version=%1")
          .arg(QStringLiteral("%1")
                   .arg(AdlWriter::kMedmVersionNumber, 6, 10, QLatin1Char('0'))));
  AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));

  const int displayWidth = displayArea_ ? displayArea_->width() : width();
  const int displayHeight = displayArea_ ? displayArea_->height() : height();
  const QRect displayRect(geometry().x(), geometry().y(), displayWidth,
      displayHeight);

  AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("display {"));
  AdlWriter::writeObjectSection(stream, 1, displayRect);
  const QColor foreground = displayArea_
      ? displayArea_->palette().color(QPalette::WindowText)
      : palette().color(QPalette::WindowText);
  const QColor background = displayArea_
      ? displayArea_->palette().color(QPalette::Window)
      : palette().color(QPalette::Window);
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("clr=%1").arg(AdlWriter::medmColorIndex(foreground)));
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("bclr=%1").arg(AdlWriter::medmColorIndex(background)));
  const QString cmapName = colormapName_.trimmed();
  const bool cmapDefault = cmapName.isEmpty()
      || cmapName.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0;
  if (cmapDefault) {
    AdlWriter::writeIndentedLine(stream, 1, QStringLiteral("cmap=\"\""));
  } else {
    AdlWriter::writeIndentedLine(stream, 1,
        QStringLiteral("cmap=\"%1\"").arg(AdlWriter::escapeAdlString(cmapName)));
  }
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("gridSpacing=%1").arg(gridSpacing_));
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("gridOn=%1").arg(gridOn_ ? 1 : 0));
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("snapToGrid=%1").arg(kDefaultSnapToGrid ? 1 : 0));
  AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));

  AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("\"color map\" {"));
  const auto &colors = MedmColors::palette();
  AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("ncolors=%1").arg(static_cast<int>(colors.size())));
  AdlWriter::writeIndentedLine(stream, 1, QStringLiteral("colors {"));
  for (const QColor &color : colors) {
    const int value = (color.red() << 16) | (color.green() << 8)
        | color.blue();
    AdlWriter::writeIndentedLine(stream, 2,
        QStringLiteral("%1,")
            .arg(QString::number(value, 16).rightJustified(6, QLatin1Char('0'))
                     .toLower()));
  }
  AdlWriter::writeIndentedLine(stream, 1, QStringLiteral("}"));
  AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));

  for (const auto &entry : elementStack_) {
    QWidget *widget = entry.data();
    if (!widget) {
      continue;
    }

    if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("composite {"));
      AdlWriter::writeObjectSection(stream, 1, widgetDisplayRect(composite));
      const QString compositeName = composite->compositeName().trimmed();
      if (!compositeName.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("\"composite name\"=\"%1\"")
                .arg(AdlWriter::escapeAdlString(compositeName)));
      }
      const QString compositeFile = composite->compositeFile().trimmed();
      if (!compositeFile.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("\"composite file\"=\"%1\"")
                .arg(AdlWriter::escapeAdlString(compositeFile)));
      } else {
        const QList<QWidget *> children = composite->childWidgets();
        if (!children.isEmpty()) {
          AdlWriter::writeIndentedLine(stream, 1,
              QStringLiteral("children {"));
          for (QWidget *child : children) {
            writeWidgetAdl(stream, child, 2, resolvedForegroundColor,
                resolvedBackgroundColor);
          }
          AdlWriter::writeIndentedLine(stream, 1, QStringLiteral("}"));
        }
      }
      AdlWriter::writeDynamicAttributeSection(stream, 1,
          composite->colorMode(), composite->visibilityMode(),
          composite->visibilityCalc(), composite->channels());
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *text = dynamic_cast<TextElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("text {"));
      AdlWriter::writeObjectSection(stream, 1, text->geometry());
    const QColor textForeground = resolvedForegroundColor(text,
      text->foregroundColor());
      AdlWriter::writeBasicAttributeSection(stream, 1,
      AdlWriter::medmColorIndex(textForeground),
          RectangleLineStyle::kSolid, RectangleFill::kSolid, 0);
      AdlWriter::writeDynamicAttributeSection(stream, 1, text->colorMode(),
          text->visibilityMode(), text->visibilityCalc(),
          AdlWriter::collectChannels(text));
      const QString content = text->text();
      if (!content.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("textix=\"%1\"")
                .arg(AdlWriter::escapeAdlString(content)));
      }
      const Qt::Alignment horizontal = text->textAlignment()
          & Qt::AlignHorizontal_Mask;
      if (horizontal != Qt::AlignLeft) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("align=\"%1\"")
                .arg(AdlWriter::alignmentString(text->textAlignment())));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *entry = dynamic_cast<TextEntryElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"text entry\" {"));
      AdlWriter::writeObjectSection(stream, 1, entry->geometry());
    const QColor entryForeground = resolvedForegroundColor(entry,
      entry->foregroundColor());
    const QColor entryBackground = resolvedBackgroundColor(entry,
      entry->backgroundColor());
      AdlWriter::writeControlSection(stream, 1, entry->channel(),
      AdlWriter::medmColorIndex(entryForeground),
      AdlWriter::medmColorIndex(entryBackground));
      if (entry->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(entry->colorMode())));
      }
      if (entry->format() != TextMonitorFormat::kDecimal) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("format=\"%1\"")
                .arg(AdlWriter::textMonitorFormatString(entry->format())));
      }
      AdlWriter::writeLimitsSection(stream, 1, entry->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("valuator {"));
      AdlWriter::writeObjectSection(stream, 1, slider->geometry());
    const QColor sliderForeground = resolvedForegroundColor(slider,
      slider->foregroundColor());
    const QColor sliderBackground = resolvedBackgroundColor(slider,
      slider->backgroundColor());
      AdlWriter::writeControlSection(stream, 1, slider->channel(),
      AdlWriter::medmColorIndex(sliderForeground),
      AdlWriter::medmColorIndex(sliderBackground));
      if (slider->label() != MeterLabel::kNone) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::meterLabelString(slider->label())));
      }
      if (slider->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(slider->colorMode())));
      }
      if (slider->direction() != BarDirection::kRight) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("direction=\"%1\"")
                .arg(AdlWriter::barDirectionString(slider->direction())));
      }
      if (std::abs(slider->precision() - 1.0) > 1e-9) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("dPrecision=%1")
                .arg(QString::number(slider->precision(), 'g', 6)));
      }
      AdlWriter::writeLimitsSection(stream, 1, slider->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"wheel switch\" {"));
      AdlWriter::writeObjectSection(stream, 1, wheel->geometry());
    const QColor wheelForeground = resolvedForegroundColor(wheel,
      wheel->foregroundColor());
    const QColor wheelBackground = resolvedBackgroundColor(wheel,
      wheel->backgroundColor());
      AdlWriter::writeControlSection(stream, 1, wheel->channel(),
      AdlWriter::medmColorIndex(wheelForeground),
      AdlWriter::medmColorIndex(wheelBackground));
      if (wheel->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(wheel->colorMode())));
      }
      const QString wheelFormat = wheel->format().trimmed();
      if (!wheelFormat.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("format=\"%1\"")
                .arg(AdlWriter::escapeAdlString(wheelFormat)));
      }
      AdlWriter::writeLimitsSection(stream, 1, wheel->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"choice button\" {"));
      AdlWriter::writeObjectSection(stream, 1, choice->geometry());
    const QColor choiceForeground = resolvedForegroundColor(choice,
      choice->foregroundColor());
    const QColor choiceBackground = resolvedBackgroundColor(choice,
      choice->backgroundColor());
      AdlWriter::writeControlSection(stream, 1, choice->channel(),
      AdlWriter::medmColorIndex(choiceForeground),
      AdlWriter::medmColorIndex(choiceBackground));
      if (choice->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(choice->colorMode())));
      }
      if (choice->stacking() != ChoiceButtonStacking::kRow) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("stacking=\"%1\"")
                .arg(AdlWriter::choiceButtonStackingString(choice->stacking())));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("menu {"));
      AdlWriter::writeObjectSection(stream, 1, menu->geometry());
    const QColor menuForeground = resolvedForegroundColor(menu,
      menu->foregroundColor());
    const QColor menuBackground = resolvedBackgroundColor(menu,
      menu->backgroundColor());
      AdlWriter::writeControlSection(stream, 1, menu->channel(),
      AdlWriter::medmColorIndex(menuForeground),
      AdlWriter::medmColorIndex(menuBackground));
      if (menu->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(menu->colorMode())));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"message button\" {"));
      AdlWriter::writeObjectSection(stream, 1, message->geometry());
    const QColor messageForeground = resolvedForegroundColor(message,
      message->foregroundColor());
    const QColor messageBackground = resolvedBackgroundColor(message,
      message->backgroundColor());
      AdlWriter::writeControlSection(stream, 1, message->channel(),
      AdlWriter::medmColorIndex(messageForeground),
      AdlWriter::medmColorIndex(messageBackground));
      if (message->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(message->colorMode())));
      }
      const QString label = message->label().trimmed();
      if (!label.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::escapeAdlString(label)));
      }
      const QString press = message->pressMessage().trimmed();
      if (!press.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("press_msg=\"%1\"")
                .arg(AdlWriter::escapeAdlString(press)));
      }
      const QString release = message->releaseMessage().trimmed();
      if (!release.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("release_msg=\"%1\"")
                .arg(AdlWriter::escapeAdlString(release)));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"shell command\" {"));
      AdlWriter::writeObjectSection(stream, 1, shell->geometry());
      for (int i = 0; i < shell->entryCount(); ++i) {
        const QString entryLabel = shell->entryLabel(i);
        const QString entryCommand = shell->entryCommand(i);
        const QString entryArgs = shell->entryArgs(i);
        const bool labelEmpty = entryLabel.trimmed().isEmpty();
        const bool commandEmpty = entryCommand.trimmed().isEmpty();
        const bool argsEmpty = entryArgs.trimmed().isEmpty();
        if (labelEmpty && commandEmpty && argsEmpty) {
          continue;
        }
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("command[%1] {").arg(i));
        if (!labelEmpty) {
          AdlWriter::writeIndentedLine(stream, 2,
              QStringLiteral("label=\"%1\"")
                  .arg(AdlWriter::escapeAdlString(entryLabel)));
        }
        if (!commandEmpty) {
          AdlWriter::writeIndentedLine(stream, 2,
              QStringLiteral("name=\"%1\"")
                  .arg(AdlWriter::escapeAdlString(entryCommand)));
        }
        if (!argsEmpty) {
          AdlWriter::writeIndentedLine(stream, 2,
              QStringLiteral("args=\"%1\"")
                  .arg(AdlWriter::escapeAdlString(entryArgs)));
        }
        AdlWriter::writeIndentedLine(stream, 1, QStringLiteral("}"));
      }
    const QColor shellForeground = resolvedForegroundColor(shell,
      shell->foregroundColor());
    const QColor shellBackground = resolvedBackgroundColor(shell,
      shell->backgroundColor());
    AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("clr=%1")
        .arg(AdlWriter::medmColorIndex(shellForeground)));
    AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("bclr=%1")
        .arg(AdlWriter::medmColorIndex(shellBackground)));
      const QString shellLabel = shell->label();
      if (!shellLabel.trimmed().isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::escapeAdlString(shellLabel)));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"related display\" {"));
      AdlWriter::writeObjectSection(stream, 1, related->geometry());
      for (int i = 0; i < related->entryCount(); ++i) {
        RelatedDisplayEntry entry = related->entry(i);
        if (entry.label.trimmed().isEmpty() && entry.name.trimmed().isEmpty()
            && entry.args.trimmed().isEmpty()) {
          continue;
        }
        AdlWriter::writeRelatedDisplayEntry(stream, 1, i, entry);
      }
    const QColor relatedForeground = resolvedForegroundColor(related,
      related->foregroundColor());
    const QColor relatedBackground = resolvedBackgroundColor(related,
      related->backgroundColor());
    AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("clr=%1")
        .arg(AdlWriter::medmColorIndex(relatedForeground)));
    AdlWriter::writeIndentedLine(stream, 1,
      QStringLiteral("bclr=%1")
        .arg(AdlWriter::medmColorIndex(relatedBackground)));
      const QString label = related->label();
      if (!label.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::escapeAdlString(label)));
      }
      if (related->visual() != RelatedDisplayVisual::kMenu) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("visual=\"%1\"")
                .arg(AdlWriter::relatedDisplayVisualString(related->visual())));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("meter {"));
      AdlWriter::writeObjectSection(stream, 1, meter->geometry());
    const QColor meterForeground = resolvedForegroundColor(meter,
      meter->foregroundColor());
    const QColor meterBackground = resolvedBackgroundColor(meter,
      meter->backgroundColor());
      AdlWriter::writeMonitorSection(stream, 1, meter->channel(),
      AdlWriter::medmColorIndex(meterForeground),
      AdlWriter::medmColorIndex(meterBackground));
      if (meter->label() != MeterLabel::kNone) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::meterLabelString(meter->label())));
      }
      if (meter->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(meter->colorMode())));
      }
  AdlWriter::writeLimitsSection(stream, 1, meter->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("bar {"));
      AdlWriter::writeObjectSection(stream, 1, bar->geometry());
    const QColor barForeground = resolvedForegroundColor(bar,
      bar->foregroundColor());
    const QColor barBackground = resolvedBackgroundColor(bar,
      bar->backgroundColor());
      AdlWriter::writeMonitorSection(stream, 1, bar->channel(),
      AdlWriter::medmColorIndex(barForeground),
      AdlWriter::medmColorIndex(barBackground));
      if (bar->label() != MeterLabel::kNone) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::meterLabelString(bar->label())));
      }
      if (bar->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(bar->colorMode())));
      }
      if (bar->direction() != BarDirection::kRight) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("direction=\"%1\"")
                .arg(AdlWriter::barDirectionString(bar->direction())));
      }
      if (bar->fillMode() != BarFill::kFromEdge) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("fillmod=\"%1\"")
                .arg(AdlWriter::barFillModeString(bar->fillMode())));
      }
      AdlWriter::writeLimitsSection(stream, 1, bar->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("indicator {"));
      AdlWriter::writeObjectSection(stream, 1, scale->geometry());
    const QColor scaleForeground = resolvedForegroundColor(scale,
      scale->foregroundColor());
    const QColor scaleBackground = resolvedBackgroundColor(scale,
      scale->backgroundColor());
      AdlWriter::writeMonitorSection(stream, 1, scale->channel(),
      AdlWriter::medmColorIndex(scaleForeground),
      AdlWriter::medmColorIndex(scaleBackground));
      if (scale->label() != MeterLabel::kNone) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::meterLabelString(scale->label())));
      }
      if (scale->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(scale->colorMode())));
      }
      if (scale->direction() != BarDirection::kRight) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("direction=\"%1\"")
                .arg(AdlWriter::barDirectionString(scale->direction())));
      }
      AdlWriter::writeLimitsSection(stream, 1, scale->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("byte {"));
      AdlWriter::writeObjectSection(stream, 1, byte->geometry());
    const QColor byteForeground = resolvedForegroundColor(byte,
      byte->foregroundColor());
    const QColor byteBackground = resolvedBackgroundColor(byte,
      byte->backgroundColor());
      AdlWriter::writeMonitorSection(stream, 1, byte->channel(),
      AdlWriter::medmColorIndex(byteForeground),
      AdlWriter::medmColorIndex(byteBackground));
      if (byte->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(byte->colorMode())));
      }
      if (byte->direction() != BarDirection::kRight) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("direction=\"%1\"")
                .arg(AdlWriter::barDirectionString(byte->direction())));
      }
      if (byte->startBit() != 15) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("sbit=%1").arg(byte->startBit()));
      }
      if (byte->endBit() != 0) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("ebit=%1").arg(byte->endBit()));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *monitor = dynamic_cast<TextMonitorElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"text update\" {"));
      AdlWriter::writeObjectSection(stream, 1, monitor->geometry());
    const QColor monitorForeground = resolvedForegroundColor(monitor,
      monitor->foregroundColor());
    const QColor monitorBackground = resolvedBackgroundColor(monitor,
      monitor->backgroundColor());
      AdlWriter::writeMonitorSection(stream, 1, monitor->channel(0),
      AdlWriter::medmColorIndex(monitorForeground),
      AdlWriter::medmColorIndex(monitorBackground));
      if (monitor->colorMode() != TextColorMode::kStatic) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("clrmod=\"%1\"")
                .arg(AdlWriter::colorModeString(monitor->colorMode())));
      }
      const Qt::Alignment monitorHorizontal = monitor->textAlignment()
          & Qt::AlignHorizontal_Mask;
      if (monitorHorizontal != Qt::AlignLeft) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("align=\"%1\"")
                .arg(AdlWriter::alignmentString(monitor->textAlignment())));
      }
      if (monitor->format() != TextMonitorFormat::kDecimal) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("format=\"%1\"")
                .arg(AdlWriter::textMonitorFormatString(monitor->format())));
      }
  AdlWriter::writeLimitsSection(stream, 1, monitor->limits(), true);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"strip chart\" {"));
      AdlWriter::writeObjectSection(stream, 1, strip->geometry());
      std::array<QString, 4> stripYLabels{};
      stripYLabels[0] = strip->yLabel();
    const QColor stripForeground = resolvedForegroundColor(strip,
      strip->foregroundColor());
    const QColor stripBackground = resolvedBackgroundColor(strip,
      strip->backgroundColor());
      AdlWriter::writePlotcom(stream, 1, strip->title(), strip->xLabel(),
      stripYLabels, AdlWriter::medmColorIndex(stripForeground),
      AdlWriter::medmColorIndex(stripBackground));
      const double period = strip->period();
      if (period > 0.0
          && std::abs(period - kDefaultStripChartPeriod) > 1e-6) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("period=%1")
                .arg(QString::number(period, 'f', 6)));
      }
      if (strip->units() != TimeUnits::kSeconds) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("units=\"%1\"")
                .arg(AdlWriter::timeUnitsString(strip->units())));
      }
      for (int i = 0; i < strip->penCount(); ++i) {
        const QString channel = strip->channel(i);
        const QColor penColor = strip->penColor(i);
        const PvLimits limits = strip->penLimits(i);
        AdlWriter::writeStripChartPenSection(stream, 1, i, channel,
            AdlWriter::medmColorIndex(penColor), limits);
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *cartesian = dynamic_cast<CartesianPlotElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"cartesian plot\" {"));
      AdlWriter::writeObjectSection(stream, 1, cartesian->geometry());
      std::array<QString, 4> yLabels{};
      for (int i = 0; i < static_cast<int>(yLabels.size()); ++i) {
        yLabels[i] = cartesian->yLabel(i);
      }
      const QColor cartesianForeground = resolvedForegroundColor(cartesian,
          cartesian->foregroundColor());
      const QColor cartesianBackground = resolvedBackgroundColor(cartesian,
          cartesian->backgroundColor());
      AdlWriter::writePlotcom(stream, 1, cartesian->title(),
          cartesian->xLabel(), yLabels,
          AdlWriter::medmColorIndex(cartesianForeground),
          AdlWriter::medmColorIndex(cartesianBackground));
      if (cartesian->style() != CartesianPlotStyle::kPoint) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("style=\"%1\"")
                .arg(AdlWriter::cartesianPlotStyleString(cartesian->style())));
      }
      if (cartesian->eraseOldest()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("erase_oldest=\"%1\"")
                .arg(AdlWriter::cartesianEraseOldestString(
                    cartesian->eraseOldest())));
      }
      if (cartesian->count() > 1) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("count=\"%1\"")
                .arg(QString::number(cartesian->count())));
      }
      auto axisIndexFor = [](CartesianPlotYAxis axis) {
        switch (axis) {
        case CartesianPlotYAxis::kY2:
          return 1;
        case CartesianPlotYAxis::kY3:
          return 2;
        case CartesianPlotYAxis::kY4:
          return 3;
        case CartesianPlotYAxis::kY1:
        default:
          return 0;
        }
      };
      for (int i = 0; i < cartesian->traceCount(); ++i) {
        const QString xChannel = cartesian->traceXChannel(i);
        const QString yChannel = cartesian->traceYChannel(i);
        const int colorIndex = AdlWriter::medmColorIndex(
            cartesian->traceColor(i));
        const int axisIndex = axisIndexFor(cartesian->traceYAxis(i));
        const bool usesRightAxis = cartesian->traceUsesRightAxis(i);
        AdlWriter::writeCartesianTraceSection(stream, 1, i, xChannel,
            yChannel, colorIndex, axisIndex, usesRightAxis);
      }
      for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
        const bool includeTimeFormat = axis == 0;
        AdlWriter::writeCartesianAxisSection(stream, 1, axis,
            cartesian->axisStyle(axis), cartesian->axisRangeStyle(axis),
            cartesian->axisMinimum(axis), cartesian->axisMaximum(axis),
            cartesian->axisTimeFormat(axis), includeTimeFormat);
      }
      const QString trigger = cartesian->triggerChannel().trimmed();
      if (!trigger.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("trigger=\"%1\"")
                .arg(AdlWriter::escapeAdlString(trigger)));
      }
      const QString erase = cartesian->eraseChannel().trimmed();
      if (!erase.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("erase=\"%1\"")
                .arg(AdlWriter::escapeAdlString(erase)));
      }
      const QString countPv = cartesian->countChannel().trimmed();
      if (!countPv.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("countPvName=\"%1\"")
                .arg(AdlWriter::escapeAdlString(countPv)));
      }
      if (cartesian->eraseMode() != CartesianPlotEraseMode::kIfNotZero) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("eraseMode=\"%1\"")
                .arg(AdlWriter::cartesianEraseModeString(
                    cartesian->eraseMode())));
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("rectangle {"));
      AdlWriter::writeObjectSection(stream, 1, rectangle->geometry());
    AdlWriter::writeBasicAttributeSection(stream, 1,
      AdlWriter::medmColorIndex(rectangle->color()),
      rectangle->lineStyle(), rectangle->fill(), rectangle->lineWidth(),
      true);
    const auto rectangleChannels = AdlWriter::channelsForMedmFourValues(
      AdlWriter::collectChannels(rectangle));
    // MEDM stores rectangle channels as chan, chanB, chanC, chanD.
    AdlWriter::writeDynamicAttributeSection(stream, 1, rectangle->colorMode(),
      rectangle->visibilityMode(), rectangle->visibilityCalc(),
      rectangleChannels);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *image = dynamic_cast<ImageElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("image {"));
      AdlWriter::writeObjectSection(stream, 1, image->geometry());
      AdlWriter::writeIndentedLine(stream, 1,
          QStringLiteral("type=\"%1\"")
              .arg(AdlWriter::imageTypeString(image->imageType())));
      const QString imageName = image->imageName();
      if (!imageName.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("\"image name\"=\"%1\"")
                .arg(AdlWriter::escapeAdlString(imageName)));
      }
      const QString imageCalc = image->calc();
      if (!imageCalc.trimmed().isEmpty()) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("calc=\"%1\"")
                .arg(AdlWriter::escapeAdlString(imageCalc)));
      }
    const auto imageChannels = AdlWriter::channelsForMedmFourValues(
      AdlWriter::collectChannels(image));
    AdlWriter::writeDynamicAttributeSection(stream, 1, image->colorMode(),
      image->visibilityMode(), image->visibilityCalc(), imageChannels);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("oval {"));
      AdlWriter::writeObjectSection(stream, 1, oval->geometry());
      AdlWriter::writeBasicAttributeSection(stream, 1, AdlWriter::medmColorIndex(oval->color()),
          oval->lineStyle(), oval->fill(), oval->lineWidth());
    const auto ovalChannels = AdlWriter::channelsForMedmFourValues(
      AdlWriter::collectChannels(oval));
    // MEDM stores oval channels as chan, chanB, chanC, chanD.
      AdlWriter::writeDynamicAttributeSection(stream, 1, oval->colorMode(),
      oval->visibilityMode(), oval->visibilityCalc(), ovalChannels);
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("arc {"));
      AdlWriter::writeObjectSection(stream, 1, arc->geometry());
      AdlWriter::writeBasicAttributeSection(stream, 1, AdlWriter::medmColorIndex(arc->color()),
          arc->lineStyle(), arc->fill(), arc->lineWidth());
      const auto arcChannels = AdlWriter::channelsForMedmFourValues(
          AdlWriter::collectChannels(arc));
      // MEDM stores arc channels as chan, chanB, chanC, chanD.
      AdlWriter::writeDynamicAttributeSection(stream, 1, arc->colorMode(),
          arc->visibilityMode(), arc->visibilityCalc(), arcChannels);
      AdlWriter::writeIndentedLine(stream, 1,
          QStringLiteral("begin=%1").arg(arc->beginAngle()));
      AdlWriter::writeIndentedLine(stream, 1,
          QStringLiteral("path=%1").arg(arc->pathAngle()));
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *line = dynamic_cast<LineElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("polyline {"));
      AdlWriter::writeObjectSection(stream, 1, line->geometry());
    AdlWriter::writeBasicAttributeSection(stream, 1, AdlWriter::medmColorIndex(line->color()),
      line->lineStyle(), RectangleFill::kSolid, line->lineWidth(), true);
    const auto lineChannels = AdlWriter::channelsForMedmFourValues(
      AdlWriter::collectChannels(line));
    AdlWriter::writeDynamicAttributeSection(stream, 1, line->colorMode(),
      line->visibilityMode(), line->visibilityCalc(), lineChannels);
      const QVector<QPoint> points = line->absolutePoints();
      if (points.size() >= 2) {
        AdlWriter::writePointsSection(stream, 1, points);
      }
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("polyline {"));
      AdlWriter::writeObjectSection(stream, 1, polyline->geometry());
    AdlWriter::writeBasicAttributeSection(stream, 1,
      AdlWriter::medmColorIndex(polyline->color()), polyline->lineStyle(),
      RectangleFill::kSolid, polyline->lineWidth(), true);
    const auto polylineChannels = AdlWriter::channelsForMedmFourValues(
      AdlWriter::collectChannels(polyline));
    AdlWriter::writeDynamicAttributeSection(stream, 1,
      polyline->colorMode(), polyline->visibilityMode(),
      polyline->visibilityCalc(), polylineChannels);
      AdlWriter::writePointsSection(stream, 1, polyline->absolutePoints());
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("polygon {"));
      AdlWriter::writeObjectSection(stream, 1, polygon->geometry());
      AdlWriter::writeBasicAttributeSection(stream, 1,
          AdlWriter::medmColorIndex(polygon->color()), polygon->lineStyle(),
          polygon->fill(), polygon->lineWidth());
    const auto polygonChannels = AdlWriter::channelsForMedmFourValues(
      AdlWriter::collectChannels(polygon));
    AdlWriter::writeDynamicAttributeSection(stream, 1, polygon->colorMode(),
      polygon->visibilityMode(), polygon->visibilityCalc(),
      polygonChannels);
      AdlWriter::writePointsSection(stream, 1, polygon->absolutePoints());
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }
  }

  stream << '\n';
  if (!file.commit()) {
    return false;
  }
  return true;
}

inline void DisplayWindow::writeWidgetAdl(QTextStream &stream, QWidget *widget,
    int indent,
    const std::function<QColor(const QWidget *, const QColor &)> &resolveForeground,
    const std::function<QColor(const QWidget *, const QColor &)> &resolveBackground) const
{
  if (!widget) {
    return;
  }

  const int level = indent;
  const int next = indent + 1;

  if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("composite {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(composite));
    const QString compositeName = composite->compositeName().trimmed();
    if (!compositeName.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("\"composite name\"=\"%1\"")
              .arg(AdlWriter::escapeAdlString(compositeName)));
    }
    const QString compositeFile = composite->compositeFile().trimmed();
    if (!compositeFile.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("\"composite file\"=\"%1\"")
              .arg(AdlWriter::escapeAdlString(compositeFile)));
    } else {
      const QList<QWidget *> children = composite->childWidgets();
      if (!children.isEmpty()) {
        AdlWriter::writeIndentedLine(stream, next,
            QStringLiteral("children {"));
        for (QWidget *child : children) {
          writeWidgetAdl(stream, child, next + 1, resolveForeground,
              resolveBackground);
        }
        AdlWriter::writeIndentedLine(stream, next, QStringLiteral("}"));
      }
    }
    AdlWriter::writeDynamicAttributeSection(stream, next,
        composite->colorMode(), composite->visibilityMode(),
        composite->visibilityCalc(), composite->channels());
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *text = dynamic_cast<TextElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("text {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(text));
    const QColor textForeground = resolveForeground(text,
        text->foregroundColor());
    AdlWriter::writeBasicAttributeSection(stream, next,
        AdlWriter::medmColorIndex(textForeground),
        RectangleLineStyle::kSolid, RectangleFill::kSolid, 0);
    AdlWriter::writeDynamicAttributeSection(stream, next,
        text->colorMode(), text->visibilityMode(), text->visibilityCalc(),
        AdlWriter::collectChannels(text));
    const QString content = text->text();
    if (!content.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("textix=\"%1\"")
              .arg(AdlWriter::escapeAdlString(content)));
    }
    const Qt::Alignment horizontal = text->textAlignment()
        & Qt::AlignHorizontal_Mask;
    if (horizontal != Qt::AlignLeft) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("align=\"%1\"")
              .arg(AdlWriter::alignmentString(text->textAlignment())));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *entry = dynamic_cast<TextEntryElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"text entry\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(entry));
    const QColor entryForeground = resolveForeground(entry,
        entry->foregroundColor());
    const QColor entryBackground = resolveBackground(entry,
        entry->backgroundColor());
    AdlWriter::writeControlSection(stream, next, entry->channel(),
        AdlWriter::medmColorIndex(entryForeground),
        AdlWriter::medmColorIndex(entryBackground));
    if (entry->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(entry->colorMode())));
    }
    if (entry->format() != TextMonitorFormat::kDecimal) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("format=\"%1\"")
              .arg(AdlWriter::textMonitorFormatString(entry->format())));
    }
    AdlWriter::writeLimitsSection(stream, next, entry->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("valuator {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(slider));
    const QColor sliderForeground = resolveForeground(slider,
        slider->foregroundColor());
    const QColor sliderBackground = resolveBackground(slider,
        slider->backgroundColor());
    AdlWriter::writeControlSection(stream, next, slider->channel(),
        AdlWriter::medmColorIndex(sliderForeground),
        AdlWriter::medmColorIndex(sliderBackground));
    if (slider->label() != MeterLabel::kNone) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::meterLabelString(slider->label())));
    }
    if (slider->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(slider->colorMode())));
    }
    if (slider->direction() != BarDirection::kRight) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("direction=\"%1\"")
              .arg(AdlWriter::barDirectionString(slider->direction())));
    }
    if (std::abs(slider->precision() - 1.0) > 1e-9) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("dPrecision=%1")
              .arg(QString::number(slider->precision(), 'g', 6)));
    }
    AdlWriter::writeLimitsSection(stream, next, slider->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"wheel switch\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(wheel));
    const QColor wheelForeground = resolveForeground(wheel,
        wheel->foregroundColor());
    const QColor wheelBackground = resolveBackground(wheel,
        wheel->backgroundColor());
    AdlWriter::writeControlSection(stream, next, wheel->channel(),
        AdlWriter::medmColorIndex(wheelForeground),
        AdlWriter::medmColorIndex(wheelBackground));
    if (wheel->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(wheel->colorMode())));
    }
    const QString wheelFormat = wheel->format().trimmed();
    if (!wheelFormat.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("format=\"%1\"")
              .arg(AdlWriter::escapeAdlString(wheelFormat)));
    }
    AdlWriter::writeLimitsSection(stream, next, wheel->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"choice button\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(choice));
    const QColor choiceForeground = resolveForeground(choice,
        choice->foregroundColor());
    const QColor choiceBackground = resolveBackground(choice,
        choice->backgroundColor());
    AdlWriter::writeControlSection(stream, next, choice->channel(),
        AdlWriter::medmColorIndex(choiceForeground),
        AdlWriter::medmColorIndex(choiceBackground));
    if (choice->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(choice->colorMode())));
    }
    if (choice->stacking() != ChoiceButtonStacking::kRow) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("stacking=\"%1\"")
              .arg(AdlWriter::choiceButtonStackingString(choice->stacking())));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("menu {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(menu));
    const QColor menuForeground = resolveForeground(menu,
        menu->foregroundColor());
    const QColor menuBackground = resolveBackground(menu,
        menu->backgroundColor());
    AdlWriter::writeControlSection(stream, next, menu->channel(),
        AdlWriter::medmColorIndex(menuForeground),
        AdlWriter::medmColorIndex(menuBackground));
    if (menu->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(menu->colorMode())));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"message button\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(message));
    const QColor messageForeground = resolveForeground(message,
        message->foregroundColor());
    const QColor messageBackground = resolveBackground(message,
        message->backgroundColor());
    AdlWriter::writeControlSection(stream, next, message->channel(),
        AdlWriter::medmColorIndex(messageForeground),
        AdlWriter::medmColorIndex(messageBackground));
    if (message->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(message->colorMode())));
    }
    const QString label = message->label().trimmed();
    if (!label.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::escapeAdlString(label)));
    }
    const QString press = message->pressMessage().trimmed();
    if (!press.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("press_msg=\"%1\"")
              .arg(AdlWriter::escapeAdlString(press)));
    }
    const QString release = message->releaseMessage().trimmed();
    if (!release.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("release_msg=\"%1\"")
              .arg(AdlWriter::escapeAdlString(release)));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"shell command\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(shell));
    const int commandIndent = next + 1;
    for (int i = 0; i < shell->entryCount(); ++i) {
      const QString entryLabel = shell->entryLabel(i);
      const QString entryCommand = shell->entryCommand(i);
      const QString entryArgs = shell->entryArgs(i);
      const bool labelEmpty = entryLabel.trimmed().isEmpty();
      const bool commandEmpty = entryCommand.trimmed().isEmpty();
      const bool argsEmpty = entryArgs.trimmed().isEmpty();
      if (labelEmpty && commandEmpty && argsEmpty) {
        continue;
      }
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("command[%1] {").arg(i));
      if (!labelEmpty) {
        AdlWriter::writeIndentedLine(stream, commandIndent,
            QStringLiteral("label=\"%1\"")
                .arg(AdlWriter::escapeAdlString(entryLabel)));
      }
      if (!commandEmpty) {
        AdlWriter::writeIndentedLine(stream, commandIndent,
            QStringLiteral("name=\"%1\"")
                .arg(AdlWriter::escapeAdlString(entryCommand)));
      }
      if (!argsEmpty) {
        AdlWriter::writeIndentedLine(stream, commandIndent,
            QStringLiteral("args=\"%1\"")
                .arg(AdlWriter::escapeAdlString(entryArgs)));
      }
      AdlWriter::writeIndentedLine(stream, next, QStringLiteral("}"));
    }
    const QColor shellForeground = resolveForeground(shell,
        shell->foregroundColor());
    const QColor shellBackground = resolveBackground(shell,
        shell->backgroundColor());
    AdlWriter::writeIndentedLine(stream, next,
        QStringLiteral("clr=%1")
            .arg(AdlWriter::medmColorIndex(shellForeground)));
    AdlWriter::writeIndentedLine(stream, next,
        QStringLiteral("bclr=%1")
            .arg(AdlWriter::medmColorIndex(shellBackground)));
    const QString shellLabel = shell->label();
    if (!shellLabel.trimmed().isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::escapeAdlString(shellLabel)));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"related display\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(related));
    for (int i = 0; i < related->entryCount(); ++i) {
      RelatedDisplayEntry entry = related->entry(i);
      if (entry.label.trimmed().isEmpty() && entry.name.trimmed().isEmpty()
          && entry.args.trimmed().isEmpty()) {
        continue;
      }
      AdlWriter::writeRelatedDisplayEntry(stream, next, i, entry);
    }
    const QColor relatedForeground = resolveForeground(related,
        related->foregroundColor());
    const QColor relatedBackground = resolveBackground(related,
        related->backgroundColor());
    AdlWriter::writeIndentedLine(stream, next,
        QStringLiteral("clr=%1")
            .arg(AdlWriter::medmColorIndex(relatedForeground)));
    AdlWriter::writeIndentedLine(stream, next,
        QStringLiteral("bclr=%1")
            .arg(AdlWriter::medmColorIndex(relatedBackground)));
    const QString label = related->label();
    if (!label.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::escapeAdlString(label)));
    }
    if (related->visual() != RelatedDisplayVisual::kMenu) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("visual=\"%1\"")
              .arg(AdlWriter::relatedDisplayVisualString(related->visual())));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("meter {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(meter));
    const QColor meterForeground = resolveForeground(meter,
        meter->foregroundColor());
    const QColor meterBackground = resolveBackground(meter,
        meter->backgroundColor());
    AdlWriter::writeMonitorSection(stream, next, meter->channel(),
        AdlWriter::medmColorIndex(meterForeground),
        AdlWriter::medmColorIndex(meterBackground));
    if (meter->label() != MeterLabel::kNone) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::meterLabelString(meter->label())));
    }
    if (meter->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(meter->colorMode())));
    }
    AdlWriter::writeLimitsSection(stream, next, meter->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("bar {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(bar));
    const QColor barForeground = resolveForeground(bar,
        bar->foregroundColor());
    const QColor barBackground = resolveBackground(bar,
        bar->backgroundColor());
    AdlWriter::writeMonitorSection(stream, next, bar->channel(),
        AdlWriter::medmColorIndex(barForeground),
        AdlWriter::medmColorIndex(barBackground));
    if (bar->label() != MeterLabel::kNone) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::meterLabelString(bar->label())));
    }
    if (bar->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(bar->colorMode())));
    }
    if (bar->direction() != BarDirection::kRight) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("direction=\"%1\"")
              .arg(AdlWriter::barDirectionString(bar->direction())));
    }
    if (bar->fillMode() != BarFill::kFromEdge) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("fillmod=\"%1\"")
              .arg(AdlWriter::barFillModeString(bar->fillMode())));
    }
    AdlWriter::writeLimitsSection(stream, next, bar->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("indicator {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(scale));
    const QColor scaleForeground = resolveForeground(scale,
        scale->foregroundColor());
    const QColor scaleBackground = resolveBackground(scale,
        scale->backgroundColor());
    AdlWriter::writeMonitorSection(stream, next, scale->channel(),
        AdlWriter::medmColorIndex(scaleForeground),
        AdlWriter::medmColorIndex(scaleBackground));
    if (scale->label() != MeterLabel::kNone) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("label=\"%1\"")
              .arg(AdlWriter::meterLabelString(scale->label())));
    }
    if (scale->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(scale->colorMode())));
    }
    if (scale->direction() != BarDirection::kRight) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("direction=\"%1\"")
              .arg(AdlWriter::barDirectionString(scale->direction())));
    }
    AdlWriter::writeLimitsSection(stream, next, scale->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("byte {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(byte));
    const QColor byteForeground = resolveForeground(byte,
        byte->foregroundColor());
    const QColor byteBackground = resolveBackground(byte,
        byte->backgroundColor());
    AdlWriter::writeMonitorSection(stream, next, byte->channel(),
        AdlWriter::medmColorIndex(byteForeground),
        AdlWriter::medmColorIndex(byteBackground));
    if (byte->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(byte->colorMode())));
    }
    if (byte->direction() != BarDirection::kRight) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("direction=\"%1\"")
              .arg(AdlWriter::barDirectionString(byte->direction())));
    }
    if (byte->startBit() != 15) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("sbit=%1").arg(byte->startBit()));
    }
    if (byte->endBit() != 0) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("ebit=%1").arg(byte->endBit()));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *monitor = dynamic_cast<TextMonitorElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"text update\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(monitor));
    const QColor monitorForeground = resolveForeground(monitor,
        monitor->foregroundColor());
    const QColor monitorBackground = resolveBackground(monitor,
        monitor->backgroundColor());
    AdlWriter::writeMonitorSection(stream, next, monitor->channel(0),
        AdlWriter::medmColorIndex(monitorForeground),
        AdlWriter::medmColorIndex(monitorBackground));
    if (monitor->colorMode() != TextColorMode::kStatic) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("clrmod=\"%1\"")
              .arg(AdlWriter::colorModeString(monitor->colorMode())));
    }
    const Qt::Alignment horizontal = monitor->textAlignment()
        & Qt::AlignHorizontal_Mask;
    if (horizontal != Qt::AlignLeft) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("align=\"%1\"")
              .arg(AdlWriter::alignmentString(monitor->textAlignment())));
    }
    if (monitor->format() != TextMonitorFormat::kDecimal) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("format=\"%1\"")
              .arg(AdlWriter::textMonitorFormatString(monitor->format())));
    }
    AdlWriter::writeLimitsSection(stream, next, monitor->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"strip chart\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(strip));
    std::array<QString, 4> yLabels{};
    yLabels[0] = strip->yLabel();
    const QColor stripForeground = resolveForeground(strip,
        strip->foregroundColor());
    const QColor stripBackground = resolveBackground(strip,
        strip->backgroundColor());
    AdlWriter::writePlotcom(stream, next, strip->title(), strip->xLabel(),
        yLabels, AdlWriter::medmColorIndex(stripForeground),
        AdlWriter::medmColorIndex(stripBackground));
    const double period = strip->period();
    if (period > 0.0 && std::abs(period - kDefaultStripChartPeriod) > 1e-6) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("period=%1")
              .arg(QString::number(period, 'f', 6)));
    }
    if (strip->units() != TimeUnits::kSeconds) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("units=\"%1\"")
              .arg(AdlWriter::timeUnitsString(strip->units())));
    }
    for (int i = 0; i < strip->penCount(); ++i) {
      const QString channel = strip->channel(i);
      const QColor penColor = strip->penColor(i);
      const PvLimits limits = strip->penLimits(i);
      AdlWriter::writeStripChartPenSection(stream, next, i, channel,
          AdlWriter::medmColorIndex(penColor), limits);
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *cartesian = dynamic_cast<CartesianPlotElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"cartesian plot\" {"));
    AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(cartesian));
    std::array<QString, 4> yLabels{};
    for (int i = 0; i < static_cast<int>(yLabels.size()); ++i) {
      yLabels[i] = cartesian->yLabel(i);
    }
    const QColor cartesianForeground = resolveForeground(cartesian,
        cartesian->foregroundColor());
    const QColor cartesianBackground = resolveBackground(cartesian,
        cartesian->backgroundColor());
    AdlWriter::writePlotcom(stream, next, cartesian->title(),
        cartesian->xLabel(), yLabels,
        AdlWriter::medmColorIndex(cartesianForeground),
        AdlWriter::medmColorIndex(cartesianBackground));
    if (cartesian->style() != CartesianPlotStyle::kPoint) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("style=\"%1\"")
              .arg(AdlWriter::cartesianPlotStyleString(cartesian->style())));
    }
    if (cartesian->eraseOldest()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("erase_oldest=\"%1\"")
              .arg(AdlWriter::cartesianEraseOldestString(
                  cartesian->eraseOldest())));
    }
    if (cartesian->count() > 1) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("count=\"%1\"")
              .arg(QString::number(cartesian->count())));
    }
    auto axisIndexFor = [](CartesianPlotYAxis axis) {
      switch (axis) {
      case CartesianPlotYAxis::kY2:
        return 1;
      case CartesianPlotYAxis::kY3:
        return 2;
      case CartesianPlotYAxis::kY4:
        return 3;
      case CartesianPlotYAxis::kY1:
      default:
        return 0;
      }
    };
    for (int i = 0; i < cartesian->traceCount(); ++i) {
      const QString xChannel = cartesian->traceXChannel(i);
      const QString yChannel = cartesian->traceYChannel(i);
      const int colorIndex = AdlWriter::medmColorIndex(
          cartesian->traceColor(i));
      const int axisIndex = axisIndexFor(cartesian->traceYAxis(i));
      const bool usesRightAxis = cartesian->traceUsesRightAxis(i);
      AdlWriter::writeCartesianTraceSection(stream, next, i, xChannel,
          yChannel, colorIndex, axisIndex, usesRightAxis);
    }
    for (int axis = 0; axis < kCartesianAxisCount; ++axis) {
      const bool includeTimeFormat = axis == 0;
      AdlWriter::writeCartesianAxisSection(stream, next, axis,
          cartesian->axisStyle(axis), cartesian->axisRangeStyle(axis),
          cartesian->axisMinimum(axis), cartesian->axisMaximum(axis),
          cartesian->axisTimeFormat(axis), includeTimeFormat);
    }
    const QString trigger = cartesian->triggerChannel().trimmed();
    if (!trigger.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("trigger=\"%1\"")
              .arg(AdlWriter::escapeAdlString(trigger)));
    }
    const QString erase = cartesian->eraseChannel().trimmed();
    if (!erase.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("erase=\"%1\"")
              .arg(AdlWriter::escapeAdlString(erase)));
    }
    const QString countPv = cartesian->countChannel().trimmed();
    if (!countPv.isEmpty()) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("countPvName=\"%1\"")
              .arg(AdlWriter::escapeAdlString(countPv)));
    }
    if (cartesian->eraseMode() != CartesianPlotEraseMode::kIfNotZero) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("eraseMode=\"%1\"")
              .arg(AdlWriter::cartesianEraseModeString(
                  cartesian->eraseMode())));
    }
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level,
    QStringLiteral("rectangle {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(rectangle));
  AdlWriter::writeBasicAttributeSection(stream, next,
    AdlWriter::medmColorIndex(rectangle->color()),
    rectangle->lineStyle(), rectangle->fill(), rectangle->lineWidth(),
    true);
  const auto rectangleChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(rectangle));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    rectangle->colorMode(), rectangle->visibilityMode(),
    rectangle->visibilityCalc(), rectangleChannels);
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }

  if (auto *image = dynamic_cast<ImageElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("image {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(image));
  AdlWriter::writeIndentedLine(stream, next,
    QStringLiteral("type=\"%1\"")
      .arg(AdlWriter::imageTypeString(image->imageType())));
  const QString imageName = image->imageName();
  if (!imageName.isEmpty()) {
    AdlWriter::writeIndentedLine(stream, next,
      QStringLiteral("\"image name\"=\"%1\"")
        .arg(AdlWriter::escapeAdlString(imageName)));
  }
  const QString imageCalc = image->calc();
  if (!imageCalc.trimmed().isEmpty()) {
    AdlWriter::writeIndentedLine(stream, next,
      QStringLiteral("calc=\"%1\"")
        .arg(AdlWriter::escapeAdlString(imageCalc)));
  }
  const auto imageChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(image));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    image->colorMode(), image->visibilityMode(), image->visibilityCalc(),
    imageChannels);
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }

  if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("oval {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(oval));
  AdlWriter::writeBasicAttributeSection(stream, next,
    AdlWriter::medmColorIndex(oval->color()), oval->lineStyle(),
    oval->fill(), oval->lineWidth());
  const auto ovalChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(oval));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    oval->colorMode(), oval->visibilityMode(), oval->visibilityCalc(),
    ovalChannels);
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }

  if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("arc {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(arc));
  AdlWriter::writeBasicAttributeSection(stream, next,
    AdlWriter::medmColorIndex(arc->color()), arc->lineStyle(),
    arc->fill(), arc->lineWidth());
  const auto arcChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(arc));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    arc->colorMode(), arc->visibilityMode(), arc->visibilityCalc(),
    arcChannels);
  AdlWriter::writeIndentedLine(stream, next,
    QStringLiteral("begin=%1").arg(arc->beginAngle()));
  AdlWriter::writeIndentedLine(stream, next,
    QStringLiteral("path=%1").arg(arc->pathAngle()));
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }

  if (auto *line = dynamic_cast<LineElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level,
    QStringLiteral("polyline {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(line));
  AdlWriter::writeBasicAttributeSection(stream, next,
    AdlWriter::medmColorIndex(line->color()), line->lineStyle(),
    RectangleFill::kSolid, line->lineWidth(), true);
  const auto lineChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(line));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    line->colorMode(), line->visibilityMode(),
    line->visibilityCalc(), lineChannels);
  const QVector<QPoint> points = line->absolutePoints();
  if (points.size() >= 2) {
    AdlWriter::writePointsSection(stream, next, points);
  }
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }

  if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level,
    QStringLiteral("polyline {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(polyline));
  AdlWriter::writeBasicAttributeSection(stream, next,
    AdlWriter::medmColorIndex(polyline->color()),
    polyline->lineStyle(), RectangleFill::kSolid,
    polyline->lineWidth(), true);
  const auto polylineChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(polyline));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    polyline->colorMode(), polyline->visibilityMode(),
    polyline->visibilityCalc(), polylineChannels);
  AdlWriter::writePointsSection(stream, next, polyline->absolutePoints());
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }

  if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
  AdlWriter::writeIndentedLine(stream, level,
    QStringLiteral("polygon {"));
  AdlWriter::writeObjectSection(stream, next, widgetDisplayRect(polygon));
  AdlWriter::writeBasicAttributeSection(stream, next,
    AdlWriter::medmColorIndex(polygon->color()), polygon->lineStyle(),
    polygon->fill(), polygon->lineWidth());
  const auto polygonChannels = AdlWriter::channelsForMedmFourValues(
    AdlWriter::collectChannels(polygon));
  AdlWriter::writeDynamicAttributeSection(stream, next,
    polygon->colorMode(), polygon->visibilityMode(),
    polygon->visibilityCalc(), polygonChannels);
  AdlWriter::writePointsSection(stream, next, polygon->absolutePoints());
  AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
  return;
  }
}

inline void DisplayWindow::clearAllElements()
{
  clearSelections();
  auto clearList = [this](auto &list) {
    for (auto *element : list) {
      if (element) {
        removeElementFromStack(element);
        element->deleteLater();
      }
    }
    list.clear();
  };
  clearList(textElements_);
  clearList(textEntryElements_);
  clearList(sliderElements_);
  clearList(wheelSwitchElements_);
  clearList(choiceButtonElements_);
  clearList(menuElements_);
  clearList(messageButtonElements_);
  clearList(shellCommandElements_);
  clearList(relatedDisplayElements_);
  clearList(textMonitorElements_);
  clearList(meterElements_);
  clearList(barMonitorElements_);
  clearList(scaleMonitorElements_);
  clearList(stripChartElements_);
  clearList(cartesianPlotElements_);
  clearList(byteMonitorElements_);
  clearList(rectangleElements_);
  clearList(imageElements_);
  clearList(ovalElements_);
  clearList(arcElements_);
  clearList(lineElements_);
  clearList(polylineElements_);
  clearList(polygonElements_);
  clearList(compositeElements_);
  elementStack_.clear();
  polygonCreationActive_ = false;
  polygonCreationPoints_.clear();
  activePolygonElement_ = nullptr;
  polylineCreationActive_ = false;
  polylineCreationPoints_.clear();
  activePolylineElement_ = nullptr;
  colormapName_.clear();
  gridOn_ = kDefaultGridOn;
  gridSpacing_ = kDefaultGridSpacing;
  displaySelected_ = false;
  if (displayArea_) {
    displayArea_->setSelected(false);
    displayArea_->setGridOn(gridOn_);
    displayArea_->setGridSpacing(gridSpacing_);
  }
  currentLoadDirectory_.clear();
}

inline bool DisplayWindow::loadDisplaySection(const AdlNode &displayNode)
{
  QRect geometry = parseObjectGeometry(displayNode);
  if (displayArea_) {
    if (geometry.width() > 0 && geometry.height() > 0) {
      displayArea_->setMinimumSize(geometry.width(), geometry.height());
      displayArea_->resize(geometry.size());
      const QSize current = size();
      const int extraWidth = current.width() - displayArea_->width();
      const int extraHeight = current.height() - displayArea_->height();
      resize(geometry.width() + extraWidth, geometry.height() + extraHeight);
    }
    const QString clrStr = propertyValue(displayNode, QStringLiteral("clr"));
    const QString bclrStr = propertyValue(displayNode, QStringLiteral("bclr"));
    QPalette areaPalette = displayArea_->palette();
    bool ok = false;
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      areaPalette.setColor(QPalette::WindowText, colorForIndex(clrIndex));
    }
    ok = false;
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      QColor background = colorForIndex(bclrIndex);
      areaPalette.setColor(QPalette::Window, background);
      areaPalette.setColor(QPalette::Base, background);
    }
    displayArea_->setPalette(areaPalette);
  }

  const QString cmap = propertyValue(displayNode, QStringLiteral("cmap"));
  colormapName_ = cmap;

  const QString gridSpacingStr = propertyValue(displayNode,
      QStringLiteral("gridSpacing"));
  bool ok = false;
  int spacing = gridSpacingStr.toInt(&ok);
  if (ok) {
    gridSpacing_ = std::max(kMinimumGridSpacing, spacing);
    if (displayArea_) {
      displayArea_->setGridSpacing(gridSpacing_);
    }
  }
  const QString gridOnStr = propertyValue(displayNode,
      QStringLiteral("gridOn"));
  int gridOnValue = gridOnStr.toInt(&ok);
  if (ok) {
    gridOn_ = gridOnValue != 0;
    if (displayArea_) {
      displayArea_->setGridOn(gridOn_);
    }
  }
  return true;
}

inline QColor DisplayWindow::colorForIndex(int index) const
{
  const auto &palette = MedmColors::palette();
  if (index >= 0 && index < static_cast<int>(palette.size())) {
    return palette[static_cast<std::size_t>(index)];
  }
  return QColor(Qt::black);
}

inline TextColorMode DisplayWindow::parseTextColorMode(
    const QString &value) const
{
  if (value.compare(QStringLiteral("alarm"), Qt::CaseInsensitive) == 0) {
    return TextColorMode::kAlarm;
  }
  if (value.compare(QStringLiteral("discrete"), Qt::CaseInsensitive) == 0) {
    return TextColorMode::kDiscrete;
  }
  return TextColorMode::kStatic;
}

inline TextVisibilityMode DisplayWindow::parseVisibilityMode(
    const QString &value) const
{
  if (value.compare(QStringLiteral("if not zero"), Qt::CaseInsensitive)
      == 0) {
    return TextVisibilityMode::kIfNotZero;
  }
  if (value.compare(QStringLiteral("if zero"), Qt::CaseInsensitive) == 0) {
    return TextVisibilityMode::kIfZero;
  }
  if (value.compare(QStringLiteral("calc"), Qt::CaseInsensitive) == 0) {
    return TextVisibilityMode::kCalc;
  }
  return TextVisibilityMode::kStatic;
}

inline MeterLabel DisplayWindow::parseMeterLabel(const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized.isEmpty()) {
    return MeterLabel::kOutline;
  }
  if (normalized == QStringLiteral("none")
      || normalized == QStringLiteral("no label")
      || normalized == QStringLiteral("no-label")
      || normalized == QStringLiteral("no_label")) {
    return MeterLabel::kNone;
  }
  if (normalized == QStringLiteral("no decorations")
      || normalized == QStringLiteral("no-decorations")
      || normalized == QStringLiteral("no_decorations")) {
    return MeterLabel::kNoDecorations;
  }
  if (normalized == QStringLiteral("limits")) {
    return MeterLabel::kLimits;
  }
  if (normalized == QStringLiteral("channel")) {
    return MeterLabel::kChannel;
  }
  if (normalized == QStringLiteral("outline")) {
    return MeterLabel::kOutline;
  }
  return MeterLabel::kOutline;
}

inline TimeUnits DisplayWindow::parseTimeUnits(const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("minute")
      || normalized == QStringLiteral("minutes")) {
    return TimeUnits::kMinutes;
  }
  if (normalized == QStringLiteral("milli-second")
      || normalized == QStringLiteral("milli second")
      || normalized == QStringLiteral("millisecond")
      || normalized == QStringLiteral("milliseconds")) {
    return TimeUnits::kMilliseconds;
  }
  return TimeUnits::kSeconds;
}

inline CartesianPlotStyle DisplayWindow::parseCartesianPlotStyle(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("point")
      || normalized == QStringLiteral("point plot")) {
    return CartesianPlotStyle::kPoint;
  }
  if (normalized == QStringLiteral("step")) {
    return CartesianPlotStyle::kStep;
  }
  if (normalized == QStringLiteral("fill under")
      || normalized == QStringLiteral("fill-under")) {
    return CartesianPlotStyle::kFillUnder;
  }
  if (normalized == QStringLiteral("line")
      || normalized == QStringLiteral("line plot")) {
    return CartesianPlotStyle::kLine;
  }
  return CartesianPlotStyle::kLine;
}

inline CartesianPlotAxisStyle DisplayWindow::parseCartesianAxisStyle(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("log10")) {
    return CartesianPlotAxisStyle::kLog10;
  }
  if (normalized == QStringLiteral("time")) {
    return CartesianPlotAxisStyle::kTime;
  }
  return CartesianPlotAxisStyle::kLinear;
}

inline CartesianPlotRangeStyle DisplayWindow::parseCartesianRangeStyle(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("user-specified")
      || normalized == QStringLiteral("user specified")) {
    return CartesianPlotRangeStyle::kUserSpecified;
  }
  if (normalized == QStringLiteral("auto-scale")
      || normalized == QStringLiteral("auto scale")) {
    return CartesianPlotRangeStyle::kAutoScale;
  }
  if (normalized == QStringLiteral("from channel")
      || normalized == QStringLiteral("channel")) {
    return CartesianPlotRangeStyle::kChannel;
  }
  return CartesianPlotRangeStyle::kChannel;
}

inline CartesianPlotTimeFormat DisplayWindow::parseCartesianTimeFormat(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("hh:mm")) {
    return CartesianPlotTimeFormat::kHhMm;
  }
  if (normalized == QStringLiteral("hh:00")) {
    return CartesianPlotTimeFormat::kHh00;
  }
  if (normalized == QStringLiteral("mmm dd yyyy")) {
    return CartesianPlotTimeFormat::kMonthDayYear;
  }
  if (normalized == QStringLiteral("mmm dd")) {
    return CartesianPlotTimeFormat::kMonthDay;
  }
  if (normalized == QStringLiteral("mmm dd hh:00")) {
    return CartesianPlotTimeFormat::kMonthDayHour00;
  }
  if (normalized == QStringLiteral("wd hh:00")
      || normalized == QStringLiteral("weekday hh:00")) {
    return CartesianPlotTimeFormat::kWeekdayHour00;
  }
  return CartesianPlotTimeFormat::kHhMmSs;
}

inline CartesianPlotEraseMode DisplayWindow::parseCartesianEraseMode(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("if zero")) {
    return CartesianPlotEraseMode::kIfZero;
  }
  if (normalized == QStringLiteral("if not zero")) {
    return CartesianPlotEraseMode::kIfNotZero;
  }
  return CartesianPlotEraseMode::kIfNotZero;
}

inline BarDirection DisplayWindow::parseBarDirection(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("up")
      || normalized == QStringLiteral("top")) {
    return BarDirection::kUp;
  }
  if (normalized == QStringLiteral("down")
      || normalized == QStringLiteral("bottom")) {
    return BarDirection::kDown;
  }
  if (normalized == QStringLiteral("left")) {
    return BarDirection::kLeft;
  }
  if (normalized == QStringLiteral("right")) {
    return BarDirection::kRight;
  }
  return BarDirection::kRight;
}

inline BarFill DisplayWindow::parseBarFill(const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("from center")
      || normalized == QStringLiteral("from-center")
      || normalized == QStringLiteral("from_center")
      || normalized == QStringLiteral("center")) {
    return BarFill::kFromCenter;
  }
  return BarFill::kFromEdge;
}

inline ChoiceButtonStacking DisplayWindow::parseChoiceButtonStacking(
    const QString &value) const
{
  QString normalized = value.trimmed().toLower();
  normalized.replace(QChar('-'), QChar(' '));
  normalized.replace(QChar('_'), QChar(' '));
  const QString simplified = normalized.simplified();
  const QString compact = QString(normalized).remove(QChar(' '));

  if (simplified == QStringLiteral("column")
      || compact == QStringLiteral("column")
      || simplified == QStringLiteral("columns")) {
    return ChoiceButtonStacking::kColumn;
  }
  if (simplified == QStringLiteral("row column")
      || compact == QStringLiteral("rowcolumn")
      || simplified == QStringLiteral("row columns")) {
    return ChoiceButtonStacking::kRowColumn;
  }
  return ChoiceButtonStacking::kRow;
}

inline RelatedDisplayVisual DisplayWindow::parseRelatedDisplayVisual(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("a row of buttons")
      || normalized == QStringLiteral("row of buttons")
      || normalized == QStringLiteral("row")
      || normalized == QStringLiteral("row buttons")) {
    return RelatedDisplayVisual::kRowOfButtons;
  }
  if (normalized == QStringLiteral("a column of buttons")
      || normalized == QStringLiteral("column of buttons")
      || normalized == QStringLiteral("column")
      || normalized == QStringLiteral("column buttons")) {
    return RelatedDisplayVisual::kColumnOfButtons;
  }
  if (normalized == QStringLiteral("invisible")
      || normalized == QStringLiteral("hidden")
      || normalized == QStringLiteral("hidden button")
      || normalized == QStringLiteral("invisible button")) {
    return RelatedDisplayVisual::kHiddenButton;
  }
  return RelatedDisplayVisual::kMenu;
}

inline RelatedDisplayMode DisplayWindow::parseRelatedDisplayMode(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized.contains(QStringLiteral("replace"))) {
    return RelatedDisplayMode::kReplace;
  }
  return RelatedDisplayMode::kAdd;
}

inline void DisplayWindow::applyChannelProperties(const AdlNode &node,
    const std::function<void(int, const QString &)> &setter,
    int baseChannelIndex, int letterStartIndex) const
{
  for (const auto &prop : node.properties) {
    const QString key = prop.key.trimmed();
    if (key.isEmpty()) {
      continue;
    }

    if (key.compare(QStringLiteral("chan"), Qt::CaseInsensitive) == 0) {
      const QString value = prop.value;
      if (!value.isEmpty() && baseChannelIndex >= 0
          && baseChannelIndex < 5) {
        setter(baseChannelIndex, value);
      }
      continue;
    }

    if (key.length() <= 4
        || key.left(4).compare(QStringLiteral("chan"), Qt::CaseInsensitive)
            != 0) {
      continue;
    }

    const QString suffix = key.mid(4);
    if (suffix.isEmpty()) {
      continue;
    }

    int index = -1;
    if (suffix.size() == 1) {
      const QChar suffixChar = suffix.at(0);
      if (suffixChar.isLetter()) {
        index = letterStartIndex
            + suffixChar.toUpper().unicode() - QChar('A').unicode();
      } else if (suffixChar.isDigit()) {
        const int digit = suffixChar.digitValue();
        if (digit > 0) {
          index = letterStartIndex + digit - 1;
        }
      }
    }
    if (index < 0) {
      bool ok = false;
      const int numeric = suffix.toInt(&ok);
      if (ok && numeric > 0) {
        index = letterStartIndex + numeric - 1;
      }
    }

    if (index >= 0 && index < 5) {
      const QString value = prop.value;
      if (!value.isEmpty()) {
        setter(index, value);
      }
    }
  }
}

inline RectangleFill DisplayWindow::parseRectangleFill(const QString &value) const
{
  if (value.compare(QStringLiteral("outline"), Qt::CaseInsensitive) == 0) {
    return RectangleFill::kOutline;
  }
  return RectangleFill::kSolid;
}

inline RectangleLineStyle DisplayWindow::parseRectangleLineStyle(
    const QString &value) const
{
  if (value.compare(QStringLiteral("dash"), Qt::CaseInsensitive) == 0) {
    return RectangleLineStyle::kDash;
  }
  return RectangleLineStyle::kSolid;
}

inline ImageType DisplayWindow::parseImageType(const QString &value) const
{
  const QString normalized = value.trimmed();
  if (normalized.compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0) {
    return ImageType::kGif;
  }
  if (normalized.compare(QStringLiteral("tiff"), Qt::CaseInsensitive)
      == 0) {
    return ImageType::kTiff;
  }
  if (normalized.compare(QStringLiteral("no image"), Qt::CaseInsensitive)
          == 0
      || normalized.compare(QStringLiteral("none"), Qt::CaseInsensitive)
          == 0
      || normalized.isEmpty()) {
    return ImageType::kNone;
  }
  return ImageType::kNone;
}

inline TextMonitorFormat DisplayWindow::parseTextMonitorFormat(
    const QString &value) const
{
  const QString normalized = value.trimmed();
  if (normalized.compare(QStringLiteral("decimal"), Qt::CaseInsensitive)
      == 0) {
    return TextMonitorFormat::kDecimal;
  }
  if (normalized.compare(QStringLiteral("exponential"), Qt::CaseInsensitive)
      == 0) {
    return TextMonitorFormat::kExponential;
  }
  if (normalized.compare(QStringLiteral("engineering"), Qt::CaseInsensitive)
      == 0
      || normalized.compare(QStringLiteral("engr. notation"),
             Qt::CaseInsensitive)
          == 0
      || normalized.compare(QStringLiteral("engr notation"),
             Qt::CaseInsensitive)
          == 0) {
    return TextMonitorFormat::kEngineering;
  }
  if (normalized.compare(QStringLiteral("compact"), Qt::CaseInsensitive)
      == 0) {
    return TextMonitorFormat::kCompact;
  }
  if (normalized.compare(QStringLiteral("truncated"), Qt::CaseInsensitive)
      == 0) {
    return TextMonitorFormat::kTruncated;
  }
  if (normalized.compare(QStringLiteral("hexadecimal"), Qt::CaseInsensitive)
      == 0) {
    return TextMonitorFormat::kHexadecimal;
  }
  if (normalized.compare(QStringLiteral("octal"), Qt::CaseInsensitive) == 0) {
    return TextMonitorFormat::kOctal;
  }
  if (normalized.compare(QStringLiteral("string"), Qt::CaseInsensitive) == 0) {
    return TextMonitorFormat::kString;
  }
  if (normalized.compare(QStringLiteral("sexagesimal"), Qt::CaseInsensitive)
      == 0) {
    return TextMonitorFormat::kSexagesimal;
  }
  if (normalized.compare(QStringLiteral("sexagesimal hms"),
          Qt::CaseInsensitive)
          == 0
      || normalized.compare(QStringLiteral("sexagesimal-hms"),
             Qt::CaseInsensitive)
          == 0) {
    return TextMonitorFormat::kSexagesimalHms;
  }
  if (normalized.compare(QStringLiteral("sexagesimal dms"),
          Qt::CaseInsensitive)
          == 0
      || normalized.compare(QStringLiteral("sexagesimal-dms"),
             Qt::CaseInsensitive)
          == 0) {
    return TextMonitorFormat::kSexagesimalDms;
  }
  return TextMonitorFormat::kDecimal;
}

inline PvLimitSource DisplayWindow::parseLimitSource(
    const QString &value) const
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("default")) {
    return PvLimitSource::kDefault;
  }
  if (normalized == QStringLiteral("user")
    || normalized == QStringLiteral("user specified")
    || normalized == QStringLiteral("user-specified")
    || normalized == QStringLiteral("user_specified")) {
    return PvLimitSource::kUser;
  }
  return PvLimitSource::kChannel;
}

inline Qt::Alignment DisplayWindow::parseAlignment(
    const QString &value) const
{
  if (value.compare(QStringLiteral("horiz. centered"), Qt::CaseInsensitive)
      == 0) {
    return Qt::AlignHCenter | Qt::AlignTop;
  }
  if (value.compare(QStringLiteral("horiz. right"), Qt::CaseInsensitive)
      == 0) {
    return Qt::AlignRight | Qt::AlignTop;
  }
  if (value.compare(QStringLiteral("center"), Qt::CaseInsensitive) == 0) {
    return Qt::AlignHCenter | Qt::AlignTop;
  }
  if (value.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0) {
    return Qt::AlignRight | Qt::AlignTop;
  }
  return Qt::AlignLeft | Qt::AlignTop;
}

inline QRect DisplayWindow::parseObjectGeometry(const AdlNode &parent) const
{
  const AdlNode *objectNode = ::findChild(parent, QStringLiteral("object"));
  if (!objectNode) {
    return QRect();
  }
  bool ok = false;
  int x = propertyValue(*objectNode, QStringLiteral("x")).toInt(&ok);
  if (!ok) {
    x = 0;
  }
  ok = false;
  int y = propertyValue(*objectNode, QStringLiteral("y")).toInt(&ok);
  if (!ok) {
    y = 0;
  }
  ok = false;
  int width = propertyValue(*objectNode, QStringLiteral("width")).toInt(&ok);
  if (!ok) {
    width = kMinimumTextWidth;
  }
  ok = false;
  int height = propertyValue(*objectNode, QStringLiteral("height")).toInt(&ok);
  if (!ok) {
    height = kMinimumTextHeight;
  }
  return QRect(x, y, width, height);
}

inline void DisplayWindow::setObjectGeometry(AdlNode &node,
    const QRect &rect) const
{
  auto setProperty = [](QList<AdlProperty> &properties, const QString &key,
                          int value) {
    for (auto &prop : properties) {
      if (prop.key.compare(key, Qt::CaseInsensitive) == 0) {
        prop.value = QString::number(value);
        return;
      }
    }
    properties.append({key, QString::number(value)});
  };

  for (auto &child : node.children) {
    if (child.name.compare(QStringLiteral("object"), Qt::CaseInsensitive)
        == 0) {
      setProperty(child.properties, QStringLiteral("x"), rect.x());
      setProperty(child.properties, QStringLiteral("y"), rect.y());
      setProperty(child.properties, QStringLiteral("width"), rect.width());
      setProperty(child.properties, QStringLiteral("height"), rect.height());
      return;
    }
  }

  AdlNode objectNode;
  objectNode.name = QStringLiteral("object");
  objectNode.properties.append(
      {QStringLiteral("x"), QString::number(rect.x())});
  objectNode.properties.append(
      {QStringLiteral("y"), QString::number(rect.y())});
  objectNode.properties.append(
      {QStringLiteral("width"), QString::number(rect.width())});
  objectNode.properties.append(
      {QStringLiteral("height"), QString::number(rect.height())});
  node.children.append(std::move(objectNode));
}

inline QRect DisplayWindow::widgetDisplayRect(const QWidget *widget) const
{
  if (!widget) {
    return QRect();
  }
  const QRect widgetGeometry = widget->geometry();
  if (!displayArea_) {
    return widgetGeometry;
  }
  const QPoint topLeftInDisplay =
      displayArea_->mapFromGlobal(widget->mapToGlobal(QPoint(0, 0)));
  return QRect(topLeftInDisplay, widgetGeometry.size());
}

inline void DisplayWindow::setWidgetDisplayRect(QWidget *widget,
    const QRect &displayRect) const
{
  if (!widget) {
    return;
  }
  if (!displayArea_) {
    widget->setGeometry(displayRect);
    return;
  }
  QPoint localTopLeft = displayRect.topLeft();
  if (QWidget *parent = widget->parentWidget()) {
    const QPoint parentTopLeftInDisplay =
        displayArea_->mapFromGlobal(parent->mapToGlobal(QPoint(0, 0)));
    localTopLeft -= parentTopLeftInDisplay;
  }
  widget->setGeometry(QRect(localTopLeft, displayRect.size()));
}

inline std::optional<AdlNode> DisplayWindow::widgetToAdlNode(QWidget *widget) const
{
  if (!widget) {
    return std::nullopt;
  }

  QString buffer;
  QTextStream stream(&buffer);
  setUtf8Encoding(stream);

  auto resolveColor = [](const QWidget *source, const QColor &candidate,
                          QPalette::ColorRole role) {
    if (candidate.isValid()) {
      return candidate;
    }
    const QWidget *current = source;
    while (current) {
      const QColor fromPalette = current->palette().color(role);
      if (fromPalette.isValid()) {
        return fromPalette;
      }
      current = current->parentWidget();
    }
    if (qApp) {
      const QColor appColor = qApp->palette().color(role);
      if (appColor.isValid()) {
        return appColor;
      }
    }
    return role == QPalette::WindowText ? QColor(Qt::black)
                                        : QColor(Qt::white);
  };

  auto resolvedForeground = [&resolveColor](const QWidget *source,
      const QColor &candidate) {
    return resolveColor(source, candidate, QPalette::WindowText);
  };

  auto resolvedBackground = [&resolveColor](const QWidget *source,
      const QColor &candidate) {
    return resolveColor(source, candidate, QPalette::Window);
  };

  writeWidgetAdl(stream, widget, 0, resolvedForeground, resolvedBackground);

  std::optional<AdlNode> root = AdlParser::parse(buffer, nullptr);
  if (!root || root->children.isEmpty()) {
    return std::nullopt;
  }
  return root->children.first();
}

inline QWidget *DisplayWindow::effectiveElementParent() const
{
  if (currentElementParent_) {
    return currentElementParent_;
  }
  return displayArea_;
}

inline void DisplayWindow::ensureElementInStack(QWidget *element)
{
  if (!element || suppressLoadRegistration_) {
    return;
  }
  for (const auto &entry : elementStack_) {
    if (entry.data() == element) {
      return;
    }
  }
  elementStack_.append(QPointer<QWidget>(element));
  element->raise();
}

inline TextElement *DisplayWindow::loadTextElement(const AdlNode &textNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }
  QRect geometry = parseObjectGeometry(textNode);
  geometry.translate(currentElementOffset_);
  if (geometry.height() < kMinimumTextElementHeight) {
    geometry.setHeight(kMinimumTextElementHeight);
  }
  auto *element = new TextElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);
  const QString content = propertyValue(textNode, QStringLiteral("textix"));
  if (!content.isEmpty()) {
    element->setText(content);
  }
  const QString alignValue = propertyValue(textNode, QStringLiteral("align"));
  if (!alignValue.isEmpty()) {
    element->setTextAlignment(parseAlignment(alignValue));
  }

  if (const AdlNode *basic = ::findChild(textNode,
          QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }
  }

  if (const AdlNode *dyn = ::findChild(textNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorMode = propertyValue(*dyn, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }
    const QString visibility = propertyValue(*dyn, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }
    const QString calc = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }
    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 1);
  }

  applyChannelProperties(textNode,
      [element](int index, const QString &value) {
        element->setChannel(index, value);
      },
      0, 1);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  textElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline TextMonitorElement *DisplayWindow::loadTextMonitorElement(
    const AdlNode &textUpdateNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(textUpdateNode);
  geometry.translate(currentElementOffset_);
  auto *element = new TextMonitorElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  const QString alignValue = propertyValue(textUpdateNode,
      QStringLiteral("align"));
  if (!alignValue.isEmpty()) {
    element->setTextAlignment(parseAlignment(alignValue));
  }

  const QString formatValue = propertyValue(textUpdateNode,
      QStringLiteral("format"));
  if (!formatValue.isEmpty()) {
    element->setFormat(parseTextMonitorFormat(formatValue));
  }

  const QString colorModeValue = propertyValue(textUpdateNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  if (const AdlNode *monitor = ::findChild(textUpdateNode,
          QStringLiteral("monitor"))) {
    const QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (!channel.isEmpty()) {
      element->setChannel(0, channel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*monitor, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*monitor, QStringLiteral("bclr"));
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  if (const AdlNode *limitsNode = ::findChild(textUpdateNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    }

    element->setLimits(limits);
  }

  if (element->text().isEmpty()) {
    element->setText(element->channel(0));
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  textMonitorElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline TextEntryElement *DisplayWindow::loadTextEntryElement(
    const AdlNode &textEntryNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(textEntryNode);
  geometry.translate(currentElementOffset_);
  auto *element = new TextEntryElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  const QString formatValue = propertyValue(textEntryNode,
      QStringLiteral("format"));
  if (!formatValue.isEmpty()) {
    element->setFormat(parseTextMonitorFormat(formatValue));
  }

  const QString colorModeValue = propertyValue(textEntryNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  if (const AdlNode *control = ::findChild(textEntryNode,
          QStringLiteral("control"))) {
    const QString channel = propertyValue(*control, QStringLiteral("chan"));
    if (!channel.trimmed().isEmpty()) {
      element->setChannel(channel.trimmed());
    }

    bool ok = false;
    const QString clrStr = propertyValue(*control, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*control, QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  if (const AdlNode *limitsNode = ::findChild(textEntryNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    }

    element->setLimits(limits);
  }

  auto channelSetter = [element](int, const QString &value) {
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty()) {
      element->setChannel(trimmed);
    }
  };

  applyChannelProperties(textEntryNode, channelSetter, 0, 1);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  textEntryElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline SliderElement *DisplayWindow::loadSliderElement(const AdlNode &valuatorNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(valuatorNode);
  if (geometry.width() < kMinimumSliderWidth) {
    geometry.setWidth(kMinimumSliderWidth);
  }
  if (geometry.height() < kMinimumSliderHeight) {
    geometry.setHeight(kMinimumSliderHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new SliderElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *control = ::findChild(valuatorNode,
          QStringLiteral("control"))) {
    const QString channel = propertyValue(*control, QStringLiteral("chan"));
    const QString trimmedChannel = channel.trimmed();
    if (!trimmedChannel.isEmpty()) {
      element->setChannel(trimmedChannel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*control, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*control, QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  const QString labelValue = propertyValue(valuatorNode, QStringLiteral("label"));
  const QString trimmedLabel = labelValue.trimmed();
  if (trimmedLabel.isEmpty()) {
    element->setLabel(MeterLabel::kNone);
  } else {
    element->setLabel(parseMeterLabel(trimmedLabel));
  }

  const QString colorModeValue = propertyValue(valuatorNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString directionValue = propertyValue(valuatorNode,
      QStringLiteral("direction"));
  if (!directionValue.isEmpty()) {
    element->setDirection(parseBarDirection(directionValue));
  }

  const QString precisionValue = propertyValue(valuatorNode,
      QStringLiteral("dPrecision"));
  if (!precisionValue.isEmpty()) {
    bool ok = false;
    const double precision = precisionValue.toDouble(&ok);
    if (ok) {
      element->setPrecision(precision);
    }
  }

  if (const AdlNode *limitsNode = ::findChild(valuatorNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    }

    element->setLimits(limits);
  }

  auto channelSetter = [element](int index, const QString &value) {
    if (index != 0) {
      return;
    }
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty()) {
      element->setChannel(trimmed);
    }
  };

  applyChannelProperties(valuatorNode, channelSetter, 0, 1);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  sliderElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline WheelSwitchElement *DisplayWindow::loadWheelSwitchElement(const AdlNode &wheelNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(wheelNode);
  if (geometry.width() < kMinimumWheelSwitchWidth) {
    geometry.setWidth(kMinimumWheelSwitchWidth);
  }
  if (geometry.height() < kMinimumWheelSwitchHeight) {
    geometry.setHeight(kMinimumWheelSwitchHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new WheelSwitchElement(parent);
  element->setGeometry(geometry);

  const QString colorModeValue = propertyValue(wheelNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString formatValue = propertyValue(wheelNode,
      QStringLiteral("format"));
  const QString trimmedFormat = formatValue.trimmed();
  if (!trimmedFormat.isEmpty()) {
    element->setFormat(trimmedFormat);
  }

  if (const AdlNode *control = ::findChild(wheelNode,
          QStringLiteral("control"))) {
    const QString channel = propertyValue(*control,
        QStringLiteral("chan"));
    const QString trimmedChannel = channel.trimmed();
    if (!trimmedChannel.isEmpty()) {
      element->setChannel(trimmedChannel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*control, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*control, QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  if (const AdlNode *limitsNode = ::findChild(wheelNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;
    bool hasPrecisionValue = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
        hasPrecisionValue = true;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
        hasPrecisionValue = true;
      }
    }

    element->setLimits(limits);
    if (hasPrecisionValue) {
      element->setPrecision(static_cast<double>(limits.precisionDefault));
    }
  }

  auto channelSetter = [element](int index, const QString &value) {
    if (index != 0) {
      return;
    }
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty()) {
      element->setChannel(trimmed);
    }
  };

  applyChannelProperties(wheelNode, channelSetter, 0, 1);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  wheelSwitchElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline MenuElement *DisplayWindow::loadMenuElement(const AdlNode &menuNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(menuNode);
  if (geometry.width() < kMinimumTextWidth) {
    geometry.setWidth(kMinimumTextWidth);
  }
  if (geometry.height() < kMinimumTextHeight) {
    geometry.setHeight(kMinimumTextHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new MenuElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  const QString colorModeValue = propertyValue(menuNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  if (const AdlNode *control = ::findChild(menuNode,
          QStringLiteral("control"))) {
    const QString channel = propertyValue(*control, QStringLiteral("chan"));
    const QString trimmedChannel = channel.trimmed();
    if (!trimmedChannel.isEmpty()) {
      element->setChannel(trimmedChannel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*control, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*control, QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  auto channelSetter = [element](int, const QString &value) {
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty()) {
      element->setChannel(trimmed);
    }
  };

  applyChannelProperties(menuNode, channelSetter, 0, 1);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  menuElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline MessageButtonElement *DisplayWindow::loadMessageButtonElement(
    const AdlNode &messageNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(messageNode);
  if (geometry.width() < kMinimumTextWidth) {
    geometry.setWidth(kMinimumTextWidth);
  }
  if (geometry.height() < kMinimumTextHeight) {
    geometry.setHeight(kMinimumTextHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new MessageButtonElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  const QString colorModeValue = propertyValue(messageNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString labelValue = propertyValue(messageNode,
      QStringLiteral("label"));
  const QString trimmedLabel = labelValue.trimmed();
  if (!trimmedLabel.isEmpty()) {
    element->setLabel(trimmedLabel);
  }

  const QString pressValue = propertyValue(messageNode,
      QStringLiteral("press_msg"));
  const QString trimmedPress = pressValue.trimmed();
  if (!trimmedPress.isEmpty()) {
    element->setPressMessage(trimmedPress);
  }

  const QString releaseValue = propertyValue(messageNode,
      QStringLiteral("release_msg"));
  const QString trimmedRelease = releaseValue.trimmed();
  if (!trimmedRelease.isEmpty()) {
    element->setReleaseMessage(trimmedRelease);
  }

  if (const AdlNode *control = ::findChild(messageNode,
          QStringLiteral("control"))) {
    const QString channel = propertyValue(*control, QStringLiteral("chan"));
    const QString trimmedChannel = channel.trimmed();
    if (!trimmedChannel.isEmpty()) {
      element->setChannel(trimmedChannel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*control, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*control, QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  messageButtonElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline ShellCommandElement *DisplayWindow::loadShellCommandElement(
    const AdlNode &shellNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(shellNode);
  if (geometry.width() < kMinimumTextWidth) {
    geometry.setWidth(kMinimumTextWidth);
  }
  if (geometry.height() < kMinimumTextHeight) {
    geometry.setHeight(kMinimumTextHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new ShellCommandElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  bool ok = false;
  const QString clrStr = propertyValue(shellNode, QStringLiteral("clr"));
  const int clrIndex = clrStr.toInt(&ok);
  if (ok) {
    element->setForegroundColor(colorForIndex(clrIndex));
  }

  ok = false;
  const QString bclrStr = propertyValue(shellNode, QStringLiteral("bclr"));
  const int bclrIndex = bclrStr.toInt(&ok);
  if (ok) {
    element->setBackgroundColor(colorForIndex(bclrIndex));
  }

  if (const AdlProperty *labelProp = findProperty(shellNode,
          QStringLiteral("label"))) {
    element->setLabel(labelProp->value);
  }

  for (const auto &child : shellNode.children) {
    if (!child.name.startsWith(QStringLiteral("command["), Qt::CaseInsensitive)) {
      continue;
    }
    const int openIndex = child.name.indexOf(QLatin1Char('['));
    const int closeIndex = child.name.indexOf(QLatin1Char(']'), openIndex + 1);
    if (openIndex < 0 || closeIndex <= openIndex + 1) {
      continue;
    }
    bool indexOk = false;
    const int entryIndex = child.name.mid(openIndex + 1,
        closeIndex - openIndex - 1).toInt(&indexOk);
    if (!indexOk || entryIndex < 0 || entryIndex >= element->entryCount()) {
      continue;
    }

    ShellCommandEntry entry = element->entry(entryIndex);
    if (const AdlProperty *entryLabel = findProperty(child,
            QStringLiteral("label"))) {
      entry.label = entryLabel->value;
    }
    if (const AdlProperty *entryCommand = findProperty(child,
            QStringLiteral("name"))) {
      entry.command = entryCommand->value;
    }
    if (const AdlProperty *entryArgs = findProperty(child,
            QStringLiteral("args"))) {
      entry.args = entryArgs->value;
    }
    element->setEntry(entryIndex, entry);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  shellCommandElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline RelatedDisplayElement *DisplayWindow::loadRelatedDisplayElement(
    const AdlNode &relatedNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(relatedNode);
  if (geometry.width() < kMinimumTextWidth) {
    geometry.setWidth(kMinimumTextWidth);
  }
  if (geometry.height() < kMinimumTextHeight) {
    geometry.setHeight(kMinimumTextHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new RelatedDisplayElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  bool ok = false;
  const QString clrValue = propertyValue(relatedNode, QStringLiteral("clr"));
  const int clrIndex = clrValue.toInt(&ok);
  if (ok) {
    element->setForegroundColor(colorForIndex(clrIndex));
  }

  ok = false;
  const QString bclrValue = propertyValue(relatedNode,
      QStringLiteral("bclr"));
  const int bclrIndex = bclrValue.toInt(&ok);
  if (ok) {
    element->setBackgroundColor(colorForIndex(bclrIndex));
  }

  const QString labelValue = propertyValue(relatedNode,
      QStringLiteral("label"));
  const QString trimmedLabel = labelValue.trimmed();
  if (!trimmedLabel.isEmpty()) {
    element->setLabel(trimmedLabel);
  }

  const QString visualValue = propertyValue(relatedNode,
      QStringLiteral("visual"));
  if (!visualValue.trimmed().isEmpty()) {
    element->setVisual(parseRelatedDisplayVisual(visualValue));
  }

  for (const auto &child : relatedNode.children) {
    const QString childName = child.name.trimmed();
    if (!childName.startsWith(QStringLiteral("display"),
            Qt::CaseInsensitive)) {
      continue;
    }

    const int openIndex = childName.indexOf(QLatin1Char('['));
    const int closeIndex = childName.indexOf(QLatin1Char(']'), openIndex + 1);
    if (openIndex < 0 || closeIndex <= openIndex + 1) {
      continue;
    }

    bool indexOk = false;
    const int entryIndex = childName.mid(openIndex + 1,
        closeIndex - openIndex - 1).toInt(&indexOk);
    if (!indexOk || entryIndex < 0
        || entryIndex >= kRelatedDisplayEntryCount) {
      continue;
    }

    RelatedDisplayEntry entry = element->entry(entryIndex);

    const QString entryLabel = propertyValue(child,
        QStringLiteral("label")).trimmed();
    if (!entryLabel.isEmpty()) {
      entry.label = entryLabel;
    }

    const QString entryName = propertyValue(child, QStringLiteral("name"))
        .trimmed();
    if (!entryName.isEmpty()) {
      entry.name = entryName;
    }

    const QString entryArgs = propertyValue(child, QStringLiteral("args"))
        .trimmed();
    if (!entryArgs.isEmpty()) {
      entry.args = entryArgs;
    }

    const QString policyValue = propertyValue(child,
        QStringLiteral("policy")).trimmed();
    if (!policyValue.isEmpty()) {
      entry.mode = parseRelatedDisplayMode(policyValue);
    }

    element->setEntry(entryIndex, entry);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  relatedDisplayElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline ChoiceButtonElement *DisplayWindow::loadChoiceButtonElement(
    const AdlNode &choiceNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(choiceNode);
  if (geometry.width() < kMinimumTextWidth) {
    geometry.setWidth(kMinimumTextWidth);
  }
  if (geometry.height() < kMinimumTextHeight) {
    geometry.setHeight(kMinimumTextHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new ChoiceButtonElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);

  const QString colorModeValue = propertyValue(choiceNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString stackingValue = propertyValue(choiceNode,
      QStringLiteral("stacking"));
  if (!stackingValue.isEmpty()) {
    element->setStacking(parseChoiceButtonStacking(stackingValue));
  }

  if (const AdlNode *control = ::findChild(choiceNode,
          QStringLiteral("control"))) {
    const QString channel = propertyValue(*control,
        QStringLiteral("chan"));
    if (!channel.trimmed().isEmpty()) {
      element->setChannel(channel.trimmed());
    }

    bool ok = false;
    const QString clrStr = propertyValue(*control, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*control,
        QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  choiceButtonElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline MeterElement *DisplayWindow::loadMeterElement(const AdlNode &meterNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(meterNode);
  if (geometry.width() < kMinimumMeterSize) {
    geometry.setWidth(kMinimumMeterSize);
  }
  if (geometry.height() < kMinimumMeterSize) {
    geometry.setHeight(kMinimumMeterSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new MeterElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *monitor = ::findChild(meterNode,
          QStringLiteral("monitor"))) {
    const QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (!channel.isEmpty()) {
      element->setChannel(channel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*monitor, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*monitor, QStringLiteral("bclr"));
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  const QString labelValue = propertyValue(meterNode, QStringLiteral("label"));
  const QString trimmedLabel = labelValue.trimmed();
  if (trimmedLabel.isEmpty()) {
    element->setLabel(MeterLabel::kNone);
  } else {
    element->setLabel(parseMeterLabel(trimmedLabel));
  }

  const QString colorModeValue = propertyValue(meterNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  if (const AdlNode *limitsNode = ::findChild(meterNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    }

    element->setLimits(limits);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  meterElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline BarMonitorElement *DisplayWindow::loadBarMonitorElement(const AdlNode &barNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(barNode);
  if (geometry.width() < kMinimumBarSize) {
    geometry.setWidth(kMinimumBarSize);
  }
  if (geometry.height() < kMinimumBarSize) {
    geometry.setHeight(kMinimumBarSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new BarMonitorElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *monitor = ::findChild(barNode,
          QStringLiteral("monitor"))) {
    const QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (!channel.isEmpty()) {
      element->setChannel(channel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*monitor, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*monitor, QStringLiteral("bclr"));
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  const QString labelValue = propertyValue(barNode, QStringLiteral("label"));
  if (!labelValue.trimmed().isEmpty()) {
    element->setLabel(parseMeterLabel(labelValue));
  }

  const QString colorModeValue = propertyValue(barNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString directionValue = propertyValue(barNode,
      QStringLiteral("direction"));
  if (!directionValue.isEmpty()) {
    element->setDirection(parseBarDirection(directionValue));
  }

  const QString fillModeValue = propertyValue(barNode,
      QStringLiteral("fillmod"));
  if (!fillModeValue.isEmpty()) {
    element->setFillMode(parseBarFill(fillModeValue));
  }

  if (const AdlNode *limitsNode = ::findChild(barNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    }

    element->setLimits(limits);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  barMonitorElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline ScaleMonitorElement *DisplayWindow::loadScaleMonitorElement(
    const AdlNode &indicatorNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(indicatorNode);
  if (geometry.width() < kMinimumScaleSize) {
    geometry.setWidth(kMinimumScaleSize);
  }
  if (geometry.height() < kMinimumScaleSize) {
    geometry.setHeight(kMinimumScaleSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new ScaleMonitorElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *monitor = ::findChild(indicatorNode,
          QStringLiteral("monitor"))) {
    const QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (!channel.isEmpty()) {
      element->setChannel(channel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*monitor, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*monitor, QStringLiteral("bclr"));
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  const QString labelValue = propertyValue(indicatorNode,
      QStringLiteral("label"));
  const QString trimmedLabel = labelValue.trimmed();
  if (trimmedLabel.isEmpty()) {
    element->setLabel(MeterLabel::kNone);
  } else {
    element->setLabel(parseMeterLabel(trimmedLabel));
  }

  const QString colorModeValue = propertyValue(indicatorNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString directionValue = propertyValue(indicatorNode,
      QStringLiteral("direction"));
  if (!directionValue.isEmpty()) {
    element->setDirection(parseBarDirection(directionValue));
  }

  if (const AdlNode *limitsNode = ::findChild(indicatorNode,
          QStringLiteral("limits"))) {
    PvLimits limits = element->limits();

    bool hasLowSource = false;
    bool hasHighSource = false;
    bool hasPrecisionSource = false;

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
      hasLowSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("lopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("loprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.lowDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hoprSrc"))) {
      limits.highSource = parseLimitSource(prop->value);
      hasHighSource = true;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("hopr"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("hoprDefault"))) {
      bool ok = false;
      const double value = prop->value.toDouble(&ok);
      if (ok) {
        limits.highDefault = value;
      }
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("precSrc"))) {
      limits.precisionSource = parseLimitSource(prop->value);
      hasPrecisionSource = true;
    }

    if (!hasLowSource) {
      limits.lowSource = PvLimitSource::kChannel;
    }
    if (!hasHighSource) {
      limits.highSource = PvLimitSource::kChannel;
    }
    if (!hasPrecisionSource) {
      limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                   QStringLiteral("precDefault"))) {
      bool ok = false;
      const int value = prop->value.toInt(&ok);
      if (ok) {
        limits.precisionDefault = value;
      }
    }

    element->setLimits(limits);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  scaleMonitorElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline CartesianPlotElement *DisplayWindow::loadCartesianPlotElement(
    const AdlNode &cartesianNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(cartesianNode);
  if (geometry.width() < kMinimumCartesianPlotWidth) {
    geometry.setWidth(kMinimumCartesianPlotWidth);
  }
  if (geometry.height() < kMinimumCartesianPlotHeight) {
    geometry.setHeight(kMinimumCartesianPlotHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new CartesianPlotElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *plotcom = ::findChild(cartesianNode,
          QStringLiteral("plotcom"))) {
    const QString title = propertyValue(*plotcom, QStringLiteral("title"));
    if (!title.trimmed().isEmpty()) {
      element->setTitle(title.trimmed());
    }

    const QString xLabel = propertyValue(*plotcom,
        QStringLiteral("xlabel"));
    if (!xLabel.trimmed().isEmpty()) {
      element->setXLabel(xLabel.trimmed());
    }

    const QString yLabel = propertyValue(*plotcom, QStringLiteral("ylabel"));
    if (!yLabel.trimmed().isEmpty()) {
      element->setYLabel(0, yLabel.trimmed());
    }

    const QString y2Label = propertyValue(*plotcom,
        QStringLiteral("y2label"));
    if (!y2Label.trimmed().isEmpty()) {
      element->setYLabel(1, y2Label.trimmed());
    }

    const QString y3Label = propertyValue(*plotcom,
        QStringLiteral("y3label"));
    if (!y3Label.trimmed().isEmpty()) {
      element->setYLabel(2, y3Label.trimmed());
    }

    const QString y4Label = propertyValue(*plotcom,
        QStringLiteral("y4label"));
    if (!y4Label.trimmed().isEmpty()) {
      element->setYLabel(3, y4Label.trimmed());
    }

    bool ok = false;
    const QString clrStr = propertyValue(*plotcom, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*plotcom, QStringLiteral("bclr"));
    const int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  element->setStyle(CartesianPlotStyle::kPoint);

  const QString styleValue = propertyValue(cartesianNode,
      QStringLiteral("style"));
  if (!styleValue.trimmed().isEmpty()) {
    element->setStyle(parseCartesianPlotStyle(styleValue));
  }

  const QString eraseOldestValue = propertyValue(cartesianNode,
      QStringLiteral("erase_oldest"));
  if (!eraseOldestValue.trimmed().isEmpty()) {
    const QString normalized = eraseOldestValue.trimmed().toLower();
    const bool eraseOldest = normalized == QStringLiteral("on")
        || normalized == QStringLiteral("plot last n pts");
    element->setEraseOldest(eraseOldest);
  }

  QString countChannel;
  const QString countValue = propertyValue(cartesianNode,
      QStringLiteral("count")).trimmed();
  if (!countValue.isEmpty()) {
    bool ok = false;
    const int countInt = countValue.toInt(&ok);
    if (ok) {
      element->setCount(countInt);
    }
    countChannel = countValue;
  }

  const QString countPvValue = propertyValue(cartesianNode,
      QStringLiteral("countPvName")).trimmed();
  if (!countPvValue.isEmpty()) {
    bool ok = false;
    const int countInt = countPvValue.toInt(&ok);
    if (ok) {
      element->setCount(countInt);
    }
    countChannel = countPvValue;
  }

  if (!countChannel.isEmpty()) {
    element->setCountChannel(countChannel);
  }

  const QString triggerValue = propertyValue(cartesianNode,
      QStringLiteral("trigger")).trimmed();
  if (!triggerValue.isEmpty()) {
    element->setTriggerChannel(triggerValue);
  }

  const QString eraseChannelValue = propertyValue(cartesianNode,
      QStringLiteral("erase")).trimmed();
  if (!eraseChannelValue.isEmpty()) {
    element->setEraseChannel(eraseChannelValue);
  }

  const QString countChannelValue = propertyValue(cartesianNode,
      QStringLiteral("countChannel")).trimmed();
  if (!countChannelValue.isEmpty()) {
    element->setCountChannel(countChannelValue);
  }

  const QString eraseModeValue = propertyValue(cartesianNode,
      QStringLiteral("eraseMode"));
  if (!eraseModeValue.trimmed().isEmpty()) {
    element->setEraseMode(parseCartesianEraseMode(eraseModeValue));
  }

  auto traceIndexForName = [](const QString &name) {
    int start = name.indexOf(QLatin1Char('['));
    int end = name.indexOf(QLatin1Char(']'), start + 1);
    if (start >= 0 && end > start) {
      bool ok = false;
      const int value = name.mid(start + 1, end - start - 1).toInt(&ok);
      if (ok) {
        return value;
      }
    }
    for (int i = 0; i < name.size(); ++i) {
      const int digit = name.at(i).digitValue();
      if (digit >= 0) {
        int value = digit;
        int j = i + 1;
        while (j < name.size()) {
          const int nextDigit = name.at(j).digitValue();
          if (nextDigit < 0) {
            break;
          }
          value = value * 10 + nextDigit;
          ++j;
        }
        return value;
      }
    }
    return -1;
  };

  auto axisForIndex = [](int index) {
    switch (index) {
    case 1:
      return CartesianPlotYAxis::kY2;
    case 2:
      return CartesianPlotYAxis::kY3;
    case 3:
      return CartesianPlotYAxis::kY4;
    default:
      return CartesianPlotYAxis::kY1;
    }
  };

  for (const auto &child : cartesianNode.children) {
    if (!child.name.startsWith(QStringLiteral("trace"), Qt::CaseInsensitive)) {
      continue;
    }

    const int traceIndex = traceIndexForName(child.name);
    if (traceIndex < 0 || traceIndex >= element->traceCount()) {
      continue;
    }

    const QString xdata = propertyValue(child, QStringLiteral("xdata"));
    if (!xdata.trimmed().isEmpty()) {
      element->setTraceXChannel(traceIndex, xdata.trimmed());
    }

    const QString ydata = propertyValue(child, QStringLiteral("ydata"));
    if (!ydata.trimmed().isEmpty()) {
      element->setTraceYChannel(traceIndex, ydata.trimmed());
    }

    bool ok = false;
    const QString clrStr = propertyValue(child, QStringLiteral("data_clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setTraceColor(traceIndex, colorForIndex(clrIndex));
    }

    ok = false;
    const QString axisStr = propertyValue(child, QStringLiteral("yaxis"));
    const int axisIndex = axisStr.toInt(&ok);
    if (ok) {
      element->setTraceYAxis(traceIndex, axisForIndex(axisIndex));
    }

    ok = false;
    const QString sideStr = propertyValue(child, QStringLiteral("yside"));
    const int sideIndex = sideStr.toInt(&ok);
    if (ok) {
      element->setTraceUsesRightAxis(traceIndex, sideIndex == 1);
    }
  }

  auto parseAxisNode = [this, element](const AdlNode &axisNode, int axisIndex) {
    const QString axisStyleValue = propertyValue(axisNode,
        QStringLiteral("axisStyle"));
    if (!axisStyleValue.trimmed().isEmpty()) {
      element->setAxisStyle(axisIndex,
          parseCartesianAxisStyle(axisStyleValue));
    }

    const QString rangeStyleValue = propertyValue(axisNode,
        QStringLiteral("rangeStyle"));
    if (!rangeStyleValue.trimmed().isEmpty()) {
      element->setAxisRangeStyle(axisIndex,
          parseCartesianRangeStyle(rangeStyleValue));
    }

    const QString minValue = propertyValue(axisNode,
        QStringLiteral("minRange")).trimmed();
    if (!minValue.isEmpty()) {
      bool ok = false;
      const double minNumber = minValue.toDouble(&ok);
      if (ok) {
        element->setAxisMinimum(axisIndex, minNumber);
      }
    }

    const QString maxValue = propertyValue(axisNode,
        QStringLiteral("maxRange")).trimmed();
    if (!maxValue.isEmpty()) {
      bool ok = false;
      const double maxNumber = maxValue.toDouble(&ok);
      if (ok) {
        element->setAxisMaximum(axisIndex, maxNumber);
      }
    }

    if (axisIndex == 0) {
      const QString timeFormatValue = propertyValue(axisNode,
          QStringLiteral("timeFormat")).trimmed();
      if (!timeFormatValue.isEmpty()) {
        element->setAxisTimeFormat(0,
            parseCartesianTimeFormat(timeFormatValue));
      }
    }
  };

  if (const AdlNode *xAxisNode = ::findChild(cartesianNode,
          QStringLiteral("x_axis"))) {
    parseAxisNode(*xAxisNode, 0);
  }
  if (const AdlNode *y1AxisNode = ::findChild(cartesianNode,
          QStringLiteral("y1_axis"))) {
    parseAxisNode(*y1AxisNode, 1);
  }
  if (const AdlNode *y2AxisNode = ::findChild(cartesianNode,
          QStringLiteral("y2_axis"))) {
    parseAxisNode(*y2AxisNode, 2);
  }
  if (const AdlNode *y3AxisNode = ::findChild(cartesianNode,
          QStringLiteral("y3_axis"))) {
    parseAxisNode(*y3AxisNode, 3);
  }
  if (const AdlNode *y4AxisNode = ::findChild(cartesianNode,
          QStringLiteral("y4_axis"))) {
    parseAxisNode(*y4AxisNode, 4);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  cartesianPlotElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline StripChartElement *DisplayWindow::loadStripChartElement(const AdlNode &stripNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(stripNode);
  if (geometry.width() < kMinimumStripChartWidth) {
    geometry.setWidth(kMinimumStripChartWidth);
  }
  if (geometry.height() < kMinimumStripChartHeight) {
    geometry.setHeight(kMinimumStripChartHeight);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new StripChartElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *plotcom = ::findChild(stripNode,
          QStringLiteral("plotcom"))) {
    const QString title = propertyValue(*plotcom, QStringLiteral("title"));
    if (!title.trimmed().isEmpty()) {
      element->setTitle(title.trimmed());
    }

    const QString xLabel = propertyValue(*plotcom, QStringLiteral("xlabel"));
    if (!xLabel.trimmed().isEmpty()) {
      element->setXLabel(xLabel.trimmed());
    }

    const QString yLabel = propertyValue(*plotcom, QStringLiteral("ylabel"));
    if (!yLabel.trimmed().isEmpty()) {
      element->setYLabel(yLabel.trimmed());
    }

    bool ok = false;
    const QString clrStr = propertyValue(*plotcom, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*plotcom, QStringLiteral("bclr"));
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  bool ok = false;
  const QString periodStr = propertyValue(stripNode, QStringLiteral("period"));
  double periodValue = periodStr.toDouble(&ok);
  if (ok) {
    element->setPeriod(periodValue);
  } else {
    const QString delayStr = propertyValue(stripNode, QStringLiteral("delay"));
    periodValue = delayStr.toDouble(&ok);
    if (ok) {
      element->setPeriod(periodValue);
    }
  }

  const QString unitsStr = propertyValue(stripNode, QStringLiteral("units"));
  if (!unitsStr.trimmed().isEmpty()) {
    element->setUnits(parseTimeUnits(unitsStr));
  }

  auto extractPenIndex = [](const QString &name) {
    for (int i = 0; i < name.size(); ++i) {
      const int digit = name.at(i).digitValue();
      if (digit >= 0) {
        int value = digit;
        int j = i + 1;
        while (j < name.size()) {
          const int nextDigit = name.at(j).digitValue();
          if (nextDigit < 0) {
            break;
          }
          value = value * 10 + nextDigit;
          ++j;
        }
        return value;
      }
    }
    return -1;
  };

  int nextSequentialPen = 0;
  for (const auto &child : stripNode.children) {
    if (!child.name.startsWith(QStringLiteral("pen"), Qt::CaseInsensitive)) {
      continue;
    }

    int penIndex = extractPenIndex(child.name);
    if (penIndex < 0) {
      penIndex = nextSequentialPen;
      ++nextSequentialPen;
    } else {
      if (penIndex >= kStripChartPenCount) {
        continue;
      }
      nextSequentialPen = std::max(nextSequentialPen, penIndex + 1);
    }

    if (penIndex < 0 || penIndex >= kStripChartPenCount) {
      continue;
    }

    const QString channel = propertyValue(child, QStringLiteral("chan"));
    if (!channel.trimmed().isEmpty()) {
      element->setChannel(penIndex, channel.trimmed());
    }

    bool colorOk = false;
    const QString penColorStr = propertyValue(child, QStringLiteral("clr"));
    const int penColorIndex = penColorStr.toInt(&colorOk);
    if (colorOk) {
      element->setPenColor(penIndex, colorForIndex(penColorIndex));
    }

    if (const AdlNode *limitsNode = ::findChild(child,
            QStringLiteral("limits"))) {
      PvLimits limits = element->penLimits(penIndex);

      bool hasLowSource = false;
      bool hasHighSource = false;
      bool hasPrecisionSource = false;

      if (const AdlProperty *prop = ::findProperty(*limitsNode,
              QStringLiteral("loprSrc"))) {
        limits.lowSource = parseLimitSource(prop->value);
        hasLowSource = true;
      }

      if (const AdlProperty *prop = ::findProperty(*limitsNode,
              QStringLiteral("lopr"))) {
        bool limitOk = false;
        const double value = prop->value.toDouble(&limitOk);
        if (limitOk) {
          limits.lowDefault = value;
        }
      } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                     QStringLiteral("loprDefault"))) {
        bool limitOk = false;
        const double value = prop->value.toDouble(&limitOk);
        if (limitOk) {
          limits.lowDefault = value;
        }
      }

      if (const AdlProperty *prop = ::findProperty(*limitsNode,
              QStringLiteral("hoprSrc"))) {
        limits.highSource = parseLimitSource(prop->value);
        hasHighSource = true;
      }

      if (const AdlProperty *prop = ::findProperty(*limitsNode,
              QStringLiteral("hopr"))) {
        bool limitOk = false;
        const double value = prop->value.toDouble(&limitOk);
        if (limitOk) {
          limits.highDefault = value;
        }
      } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                     QStringLiteral("hoprDefault"))) {
        bool limitOk = false;
        const double value = prop->value.toDouble(&limitOk);
        if (limitOk) {
          limits.highDefault = value;
        }
      }

      if (const AdlProperty *prop = ::findProperty(*limitsNode,
              QStringLiteral("precSrc"))) {
        limits.precisionSource = parseLimitSource(prop->value);
        hasPrecisionSource = true;
      }

      if (!hasLowSource) {
        limits.lowSource = PvLimitSource::kChannel;
      }
      if (!hasHighSource) {
        limits.highSource = PvLimitSource::kChannel;
      }
      if (!hasPrecisionSource) {
        limits.precisionSource = PvLimitSource::kChannel;
    }

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("prec"))) {
      bool limitOk = false;
        const int value = prop->value.toInt(&limitOk);
        if (limitOk) {
          limits.precisionDefault = value;
        }
      } else if (const AdlProperty *prop = ::findProperty(*limitsNode,
                     QStringLiteral("precDefault"))) {
        bool limitOk = false;
        const int value = prop->value.toInt(&limitOk);
        if (limitOk) {
          limits.precisionDefault = value;
        }
      }

      element->setPenLimits(penIndex, limits);
    }
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  stripChartElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline ByteMonitorElement *DisplayWindow::loadByteMonitorElement(
    const AdlNode &byteNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(byteNode);
  if (geometry.width() < kMinimumByteSize) {
    geometry.setWidth(kMinimumByteSize);
  }
  if (geometry.height() < kMinimumByteSize) {
    geometry.setHeight(kMinimumByteSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new ByteMonitorElement(parent);
  element->setGeometry(geometry);

  if (const AdlNode *monitor = ::findChild(byteNode,
          QStringLiteral("monitor"))) {
    const QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (!channel.isEmpty()) {
      element->setChannel(channel);
    }

    bool ok = false;
    const QString clrStr = propertyValue(*monitor, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    ok = false;
    const QString bclrStr = propertyValue(*monitor, QStringLiteral("bclr"));
    int bclrIndex = bclrStr.toInt(&ok);
    if (ok) {
      element->setBackgroundColor(colorForIndex(bclrIndex));
    }
  }

  const QString colorModeValue = propertyValue(byteNode,
      QStringLiteral("clrmod"));
  if (!colorModeValue.isEmpty()) {
    element->setColorMode(parseTextColorMode(colorModeValue));
  }

  const QString directionValue = propertyValue(byteNode,
      QStringLiteral("direction"));
  if (!directionValue.isEmpty()) {
    element->setDirection(parseBarDirection(directionValue));
  }

  bool ok = false;
  const QString startBitValue = propertyValue(byteNode,
      QStringLiteral("sbit"));
  const int startBit = startBitValue.toInt(&ok);
  if (ok) {
    element->setStartBit(startBit);
  }

  ok = false;
  const QString endBitValue = propertyValue(byteNode, QStringLiteral("ebit"));
  const int endBit = endBitValue.toInt(&ok);
  if (ok) {
    element->setEndBit(endBit);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  byteMonitorElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline ImageElement *DisplayWindow::loadImageElement(
    const AdlNode &imageNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(imageNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new ImageElement(parent);
  element->setGeometry(geometry);

  const QString typeValue = propertyValue(imageNode, QStringLiteral("type"));
  if (!typeValue.isEmpty()) {
    element->setImageType(parseImageType(typeValue));
  }

  if (!currentLoadDirectory_.isEmpty()) {
    element->setBaseDirectory(currentLoadDirectory_);
  } else if (!filePath_.isEmpty()) {
    element->setBaseDirectory(QFileInfo(filePath_).absolutePath());
  }

  const QString nameValue = propertyValue(imageNode,
      QStringLiteral("image name"));
  if (!nameValue.isEmpty()) {
    element->setImageName(nameValue);
  }

  const QString calcValue = propertyValue(imageNode, QStringLiteral("calc"));
  if (!calcValue.trimmed().isEmpty()) {
    element->setCalc(calcValue);
  }

  auto imageChannelSetter = [element](int index, const QString &value) {
    int mappedIndex = -1;
    if (index == 0) {
      mappedIndex = 0;
    } else if (index == 1) {
      mappedIndex = 1;
    } else if (index >= 2) {
      mappedIndex = index - 1;
    }
    if (mappedIndex >= 0) {
      element->setChannel(mappedIndex, value);
    }
  };

  if (const AdlNode *dyn = ::findChild(imageNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorMode = propertyValue(*dyn, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(*dyn, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString visCalc = propertyValue(*dyn, QStringLiteral("calc"));
    if (!visCalc.isEmpty()) {
      element->setVisibilityCalc(visCalc);
    }

    applyChannelProperties(*dyn,
        imageChannelSetter,
        0, 1);
  }

  applyChannelProperties(imageNode,
      imageChannelSetter,
      0, 1);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  imageElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline RectangleElement *DisplayWindow::loadRectangleElement(
    const AdlNode &rectangleNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(rectangleNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new RectangleElement(parent);
  element->setFill(RectangleFill::kSolid);
  element->setGeometry(geometry);

  if (const AdlNode *basic = ::findChild(rectangleNode,
          QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    const QString styleValue = propertyValue(*basic, QStringLiteral("style"));
    if (!styleValue.isEmpty()) {
      element->setLineStyle(parseRectangleLineStyle(styleValue));
    }

    const QString fillValue = propertyValue(*basic, QStringLiteral("fill"),
        QStringLiteral("solid"));
    element->setFill(parseRectangleFill(fillValue));

    ok = false;
    const QString widthValue = propertyValue(*basic, QStringLiteral("width"));
    int width = widthValue.toInt(&ok);
    if (!ok || width <= 0) {
      width = 1;
    }
    element->setLineWidth(width);
  }

  if (const AdlNode *dyn = ::findChild(rectangleNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorMode = propertyValue(*dyn, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(*dyn, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString calc = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }
    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 0);
  }

  applyChannelProperties(rectangleNode,
      [element](int index, const QString &value) {
        element->setChannel(index, value);
      },
      0, 0);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  rectangleElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline OvalElement *DisplayWindow::loadOvalElement(const AdlNode &ovalNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(ovalNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new OvalElement(parent);
  element->setGeometry(geometry);
  element->setFill(RectangleFill::kSolid);

  if (const AdlNode *basic = ::findChild(ovalNode,
          QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    const QString styleValue = propertyValue(*basic,
        QStringLiteral("style"));
    if (!styleValue.isEmpty()) {
      element->setLineStyle(parseRectangleLineStyle(styleValue));
    }

    const QString fillValue = propertyValue(*basic,
        QStringLiteral("fill"));
    if (!fillValue.isEmpty()) {
      element->setFill(parseRectangleFill(fillValue));
    }

    const QString widthValue = propertyValue(*basic,
        QStringLiteral("width"));
    if (!widthValue.isEmpty()) {
      int width = widthValue.toInt(&ok);
      if (!ok || width <= 0) {
        width = 1;
      }
      element->setLineWidth(width);
    }
  }

  if (const AdlNode *dyn = ::findChild(ovalNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorMode = propertyValue(*dyn, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(*dyn, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString calc = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }

    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 0);
  }

  applyChannelProperties(ovalNode,
      [element](int index, const QString &value) {
        element->setChannel(index, value);
      },
      0, 0);

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  ovalElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline ArcElement *DisplayWindow::loadArcElement(const AdlNode &arcNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(arcNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }
  geometry.translate(currentElementOffset_);

  auto *element = new ArcElement(parent);
  element->setGeometry(geometry);

  bool fillSpecified = false;

  if (const AdlNode *basic = ::findChild(arcNode,
    QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }

    const QString styleValue = propertyValue(*basic,
        QStringLiteral("style"));
    if (!styleValue.isEmpty()) {
      element->setLineStyle(parseRectangleLineStyle(styleValue));
    }

    const QString fillValue = propertyValue(*basic,
        QStringLiteral("fill"));
    if (!fillValue.isEmpty()) {
      element->setFill(parseRectangleFill(fillValue));
      fillSpecified = true;
    }

    const QString widthValue = propertyValue(*basic,
        QStringLiteral("width"));
    if (!widthValue.isEmpty()) {
      int width = widthValue.toInt(&ok);
      if (!ok || width <= 0) {
        width = 1;
      }
      element->setLineWidth(width);
    }
  }

  if (!fillSpecified) {
    element->setFill(RectangleFill::kSolid);
  }

  if (const AdlNode *dyn = ::findChild(arcNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorMode = propertyValue(*dyn, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(*dyn, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString calc = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }

    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 0);
  }

  applyChannelProperties(arcNode,
      [element](int index, const QString &value) {
        element->setChannel(index, value);
      },
      0, 0);

  bool ok = false;
  const QString beginValue = propertyValue(arcNode, QStringLiteral("begin"));
  int beginAngle = beginValue.toInt(&ok);
  if (ok) {
    element->setBeginAngle(beginAngle);
  }

  ok = false;
  const QString pathValue = propertyValue(arcNode, QStringLiteral("path"));
  int pathAngle = pathValue.toInt(&ok);
  if (ok) {
   element->setPathAngle(pathAngle);
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }

  element->show();
  element->setSelected(false);
  arcElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline PolygonElement *DisplayWindow::loadPolygonElement(
    const AdlNode &polygonNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QVector<QPoint> points = parsePolylinePoints(polygonNode);
  if (points.size() < 3) {
    return nullptr;
  }

  if (!currentElementOffset_.isNull()) {
    for (QPoint &point : points) {
      point += currentElementOffset_;
    }
  }

  QColor color = colorForIndex(14);
  RectangleLineStyle lineStyle = RectangleLineStyle::kSolid;
  RectangleFill fill = RectangleFill::kSolid;
  int lineWidth = 1;

  if (const AdlNode *basic = ::findChild(polygonNode,
          QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      color = colorForIndex(clrIndex);
    }

    const QString styleValue = propertyValue(*basic,
        QStringLiteral("style"));
    if (!styleValue.isEmpty()) {
      lineStyle = parseRectangleLineStyle(styleValue);
    }

    const QString fillValue = propertyValue(*basic, QStringLiteral("fill"));
    if (!fillValue.isEmpty()) {
      fill = parseRectangleFill(fillValue);
    }

    const QString widthValue = propertyValue(*basic,
        QStringLiteral("width"));
    if (!widthValue.isEmpty()) {
      int widthCandidate = widthValue.toInt(&ok);
      if (ok) {
        lineWidth = std::max(1, widthCandidate);
      }
    }
  }

  TextColorMode colorMode = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode = TextVisibilityMode::kStatic;
  QString visibilityCalc;

  std::array<QString, 5> channels{};
  auto channelSetter = [&channels](int index, const QString &value) {
    if (index >= 0 && index < static_cast<int>(channels.size())) {
      channels[static_cast<std::size_t>(index)] = value;
    }
  };

  if (const AdlNode *dyn = ::findChild(polygonNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorModeValue = propertyValue(*dyn,
        QStringLiteral("clr"));
    if (!colorModeValue.isEmpty()) {
      colorMode = parseTextColorMode(colorModeValue);
    }

    const QString visibilityValue = propertyValue(*dyn,
        QStringLiteral("vis"));
    if (!visibilityValue.isEmpty()) {
      visibilityMode = parseVisibilityMode(visibilityValue);
    }

    const QString calcValue = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calcValue.isEmpty()) {
      visibilityCalc = calcValue.trimmed();
    }

    applyChannelProperties(*dyn, channelSetter, 0, 0);
  }

  applyChannelProperties(polygonNode, channelSetter, 0, 0);

  auto *element = new PolygonElement(parent);
  element->setForegroundColor(color);
  element->setFill(fill);
  element->setLineStyle(lineStyle);
  element->setLineWidth(lineWidth);
  element->setColorMode(colorMode);
  element->setVisibilityMode(visibilityMode);
  element->setVisibilityCalc(visibilityCalc);
  for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
    const QString &channel = channels[static_cast<std::size_t>(i)];
    if (!channel.isEmpty()) {
      element->setChannel(i, channel);
    }
  }
  element->setAbsolutePoints(points);
  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }
  element->show();
  element->setSelected(false);
  polygonElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline bool DisplayWindow::parseAdlPoint(const QString &text, QPoint *point) const
{
  if (!point) {
    return false;
  }
  QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return false;
  }
  if (trimmed.endsWith(QChar(','))) {
    trimmed.chop(1);
    trimmed = trimmed.trimmed();
  }
  if (trimmed.startsWith(QChar('(')) && trimmed.endsWith(QChar(')'))) {
    trimmed = trimmed.mid(1, trimmed.size() - 2);
  }
  const QStringList parts = trimmed.split(QChar(','), Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return false;
  }
  bool okX = false;
  bool okY = false;
  const int x = parts.at(0).trimmed().toInt(&okX);
  const int y = parts.at(1).trimmed().toInt(&okY);
  if (!okX || !okY) {
    return false;
  }
  *point = QPoint(x, y);
  return true;
}

inline QVector<QPoint> DisplayWindow::parsePolylinePoints(
    const AdlNode &polylineNode) const
{
  QVector<QPoint> points;
  const AdlNode *pointsNode = ::findChild(polylineNode,
      QStringLiteral("points"));
  if (!pointsNode) {
    return points;
  }

  QStringList tokens;
  tokens.reserve(pointsNode->properties.size()
      + pointsNode->children.size());
  for (const auto &prop : pointsNode->properties) {
    tokens.append(prop.value);
  }
  for (const auto &child : pointsNode->children) {
    if (!child.properties.isEmpty()) {
      tokens.append(child.properties.first().value);
    } else if (!child.name.isEmpty()) {
      tokens.append(child.name);
    }
  }

  if (tokens.isEmpty()) {
    return points;
  }

  const QString aggregate = tokens.join(QStringLiteral(" "));
  int searchPos = 0;
  while (true) {
    const int start = aggregate.indexOf(QChar('('), searchPos);
    if (start < 0) {
      break;
    }
    const int end = aggregate.indexOf(QChar(')'), start + 1);
    if (end < 0) {
      break;
    }
    QString inside = aggregate.mid(start + 1, end - start - 1).trimmed();
    inside.replace(QLatin1Char(','), QLatin1Char(' '));
    if (!inside.isEmpty()) {
      const QStringList parts = inside.split(QLatin1Char(' '),
          Qt::SkipEmptyParts);
      if (parts.size() >= 2) {
        bool okX = false;
        bool okY = false;
        const int x = parts.at(0).toInt(&okX);
        const int y = parts.at(1).toInt(&okY);
        if (okX && okY) {
          points.append(QPoint(x, y));
        }
      }
    }
    searchPos = end + 1;
  }
  return points;
}

inline PolylineElement *DisplayWindow::loadPolylineElement(
    const AdlNode &polylineNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QVector<QPoint> points = parsePolylinePoints(polylineNode);
  if (points.size() < 2) {
    return nullptr;
  }

  if (!currentElementOffset_.isNull()) {
    for (QPoint &point : points) {
      point += currentElementOffset_;
    }
  }

  QColor color = colorForIndex(14);
  RectangleLineStyle lineStyle = RectangleLineStyle::kSolid;
  int lineWidth = 1;

  if (const AdlNode *basic = ::findChild(polylineNode,
          QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    const int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      color = colorForIndex(clrIndex);
    }

    const QString styleValue = propertyValue(*basic,
        QStringLiteral("style"));
    if (!styleValue.isEmpty()) {
      lineStyle = parseRectangleLineStyle(styleValue);
    }

    const QString widthValue = propertyValue(*basic,
        QStringLiteral("width"));
    if (!widthValue.isEmpty()) {
      int widthCandidate = widthValue.toInt(&ok);
      if (ok) {
        lineWidth = std::max(1, widthCandidate);
      }
    }
  }

  TextColorMode colorMode = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode = TextVisibilityMode::kStatic;
  QString visibilityCalc;

  std::array<QString, 5> channels{};
  auto channelSetter = [&channels](int index, const QString &value) {
    if (index >= 0 && index < static_cast<int>(channels.size())) {
      channels[static_cast<std::size_t>(index)] = value;
    }
  };

  if (const AdlNode *dyn = ::findChild(polylineNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorModeValue = propertyValue(*dyn,
        QStringLiteral("clr"));
    if (!colorModeValue.isEmpty()) {
      colorMode = parseTextColorMode(colorModeValue);
    }

    const QString visibilityValue = propertyValue(*dyn,
        QStringLiteral("vis"));
    if (!visibilityValue.isEmpty()) {
      visibilityMode = parseVisibilityMode(visibilityValue);
    }

    const QString calcValue = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calcValue.isEmpty()) {
      visibilityCalc = calcValue.trimmed();
    }

    applyChannelProperties(*dyn, channelSetter, 0, 0);
  }

  applyChannelProperties(polylineNode, channelSetter, 0, 0);

  QPolygon polygon(points);
  QRect geometry = polygon.boundingRect();
  if (geometry.width() <= 0) {
    geometry.setWidth(1);
  }
  if (geometry.height() <= 0) {
    geometry.setHeight(1);
  }

  if (points.size() == 2) {
    auto *element = new LineElement(parent);
    element->setGeometry(geometry);
    element->setForegroundColor(color);
    element->setLineStyle(lineStyle);
    element->setLineWidth(lineWidth);
    element->setColorMode(colorMode);
    element->setVisibilityMode(visibilityMode);
    element->setVisibilityCalc(visibilityCalc);
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
      const QString &channel = channels[static_cast<std::size_t>(i)];
      if (!channel.isEmpty()) {
        element->setChannel(i, channel);
      }
    }
    const QPoint localStart = points.first() - geometry.topLeft();
    const QPoint localEnd = points.last() - geometry.topLeft();
    element->setLocalEndpoints(localStart, localEnd);
    if (currentCompositeOwner_) {
      currentCompositeOwner_->adoptChild(element);
    }
    element->show();
    element->setSelected(false);
    lineElements_.append(element);
    ensureElementInStack(element);
    return nullptr;
  }

  auto *element = new PolylineElement(parent);
  element->setForegroundColor(color);
  element->setLineStyle(lineStyle);
  element->setLineWidth(lineWidth);
  element->setColorMode(colorMode);
  element->setVisibilityMode(visibilityMode);
  element->setVisibilityCalc(visibilityCalc);
  for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
    const QString &channel = channels[static_cast<std::size_t>(i)];
    if (!channel.isEmpty()) {
      element->setChannel(i, channel);
    }
  }
  element->setAbsolutePoints(points);
  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(element);
  }
  element->show();
  element->setSelected(false);
  polylineElements_.append(element);
  ensureElementInStack(element);
  return element;
}

inline CompositeElement *DisplayWindow::loadCompositeElement(
    const AdlNode &compositeNode)
{
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(compositeNode);
  geometry.translate(currentElementOffset_);

  auto *composite = new CompositeElement(parent);
  composite->setGeometry(geometry);

  const QString compositeName = propertyValue(compositeNode,
      QStringLiteral("composite name"));
  if (!compositeName.trimmed().isEmpty()) {
    composite->setCompositeName(compositeName.trimmed());
  }

  const QString compositeFile = propertyValue(compositeNode,
      QStringLiteral("composite file"));
  if (!compositeFile.trimmed().isEmpty()) {
    composite->setCompositeFile(compositeFile.trimmed());
  }

  TextColorMode colorMode = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode = TextVisibilityMode::kStatic;
  QString visibilityCalc;
  std::array<QString, 5> channels{};
  auto channelSetter = [&channels](int index, const QString &value) {
    if (index >= 0 && index < static_cast<int>(channels.size())) {
      channels[static_cast<std::size_t>(index)] = value;
    }
  };

  if (const AdlNode *dyn = ::findChild(compositeNode,
          QStringLiteral("dynamic attribute"))) {
    const QString colorModeValue = propertyValue(*dyn,
        QStringLiteral("clr"));
    if (!colorModeValue.isEmpty()) {
      colorMode = parseTextColorMode(colorModeValue);
    }

    const QString visibilityValue = propertyValue(*dyn,
        QStringLiteral("vis"));
    if (!visibilityValue.isEmpty()) {
      visibilityMode = parseVisibilityMode(visibilityValue);
    }

    const QString calcValue = propertyValue(*dyn, QStringLiteral("calc"));
    if (!calcValue.isEmpty()) {
      visibilityCalc = calcValue.trimmed();
    }

    applyChannelProperties(*dyn, channelSetter, 0, 0);
  }

  applyChannelProperties(compositeNode, channelSetter, 0, 0);

  composite->setColorMode(colorMode);
  composite->setVisibilityMode(visibilityMode);
  composite->setVisibilityCalc(visibilityCalc);
  for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
    const QString &channel = channels[static_cast<std::size_t>(i)];
    if (!channel.isEmpty()) {
      composite->setChannel(i, channel);
    }
  }

  if (currentCompositeOwner_) {
    currentCompositeOwner_->adoptChild(composite);
  }

  compositeElements_.append(composite);
  composite->show();
  composite->setSelected(false);
  ensureElementInStack(composite);

  const QString trimmedFile = compositeFile.trimmed();
  if (trimmedFile.isEmpty()) {
    if (const AdlNode *childrenNode = ::findChild(compositeNode,
            QStringLiteral("children"))) {
      ElementLoadContextGuard guard(*this, composite, QPoint(), true,
          composite);
      for (const auto &child : childrenNode->children) {
        loadElementNode(child);
      }
    }
  }

  return composite;
}

inline bool DisplayWindow::loadElementNode(const AdlNode &node)
{
  const QString name = node.name.trimmed().toLower();

  if (name == QStringLiteral("text")) {
    return loadTextElement(node) != nullptr;
  }
  if (name == QStringLiteral("text update")
      || name == QStringLiteral("text monitor")) {
    return loadTextMonitorElement(node) != nullptr;
  }
  if (name == QStringLiteral("text entry")) {
    return loadTextEntryElement(node) != nullptr;
  }
  if (name == QStringLiteral("valuator")) {
    return loadSliderElement(node) != nullptr;
  }
  if (name == QStringLiteral("wheel switch")) {
    return loadWheelSwitchElement(node) != nullptr;
  }
  if (name == QStringLiteral("choice button")) {
    return loadChoiceButtonElement(node) != nullptr;
  }
  if (name == QStringLiteral("menu")) {
    return loadMenuElement(node) != nullptr;
  }
  if (name == QStringLiteral("message button")) {
    return loadMessageButtonElement(node) != nullptr;
  }
  if (name == QStringLiteral("shell command")) {
    return loadShellCommandElement(node) != nullptr;
  }
  if (name == QStringLiteral("related display")) {
    return loadRelatedDisplayElement(node) != nullptr;
  }
  if (name == QStringLiteral("meter")) {
    return loadMeterElement(node) != nullptr;
  }
  if (name == QStringLiteral("bar")) {
    return loadBarMonitorElement(node) != nullptr;
  }
  if (name == QStringLiteral("indicator")) {
    return loadScaleMonitorElement(node) != nullptr;
  }
  if (name == QStringLiteral("cartesian plot")) {
    return loadCartesianPlotElement(node) != nullptr;
  }
  if (name == QStringLiteral("strip chart")) {
    return loadStripChartElement(node) != nullptr;
  }
  if (name == QStringLiteral("byte")) {
    return loadByteMonitorElement(node) != nullptr;
  }
  if (name == QStringLiteral("image")) {
    return loadImageElement(node) != nullptr;
  }
  if (name == QStringLiteral("rectangle")) {
    return loadRectangleElement(node) != nullptr;
  }
  if (name == QStringLiteral("oval")) {
    return loadOvalElement(node) != nullptr;
  }
  if (name == QStringLiteral("arc")) {
    return loadArcElement(node) != nullptr;
  }
  if (name == QStringLiteral("polygon")) {
    return loadPolygonElement(node) != nullptr;
  }
  if (name == QStringLiteral("polyline") || name == QStringLiteral("line")) {
    loadPolylineElement(node);
    return true;
  }
  if (name == QStringLiteral("composite")) {
    return loadCompositeElement(node) != nullptr;
  }
  return false;
}

inline void DisplayWindow::setAsActiveDisplay()
{
  if (auto state = state_.lock()) {
    if (state->activeDisplay != this) {
      state->activeDisplay = this;
      notifyMenus();
    }
  }
}

inline void DisplayWindow::markDirty()
{
  const bool wasDirty = dirty_;
  dirty_ = true;
  updateDirtyIndicator();
  if (!wasDirty) {
    notifyMenus();
  }
}

inline void DisplayWindow::notifyMenus() const
{
  if (auto state = state_.lock()) {
    if (state->updateMenus && *state->updateMenus) {
      (*state->updateMenus)();
    }
  }
}

inline void DisplayWindow::updateDirtyIndicator()
{
  QString title = windowTitle();
  const bool hasIndicator = title.endsWith(QLatin1Char('*'));
  if (dirty_) {
    if (!hasIndicator) {
      setWindowTitle(title + QLatin1Char('*'));
    }
  } else if (hasIndicator) {
    title.chop(1);
    setWindowTitle(title);
  }
}
