#pragma once

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

        if (QWidget *widget = elementAt(event->pos())) {
          if (auto *text = dynamic_cast<TextElement *>(widget)) {
            selectTextElement(text);
            showResourcePaletteForText(text);
            event->accept();
            return;
          }
          if (auto *textEntry = dynamic_cast<TextEntryElement *>(widget)) {
            selectTextEntryElement(textEntry);
            showResourcePaletteForTextEntry(textEntry);
            event->accept();
            return;
          }
          if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
            selectSliderElement(slider);
            showResourcePaletteForSlider(slider);
            event->accept();
            return;
          }
          if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
            selectWheelSwitchElement(wheel);
            showResourcePaletteForWheelSwitch(wheel);
            event->accept();
            return;
          }
          if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
            selectChoiceButtonElement(choice);
            showResourcePaletteForChoiceButton(choice);
            event->accept();
            return;
          }
          if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
            selectMenuElement(menu);
            showResourcePaletteForMenu(menu);
            event->accept();
            return;
          }
          if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
            selectMessageButtonElement(message);
            showResourcePaletteForMessageButton(message);
            event->accept();
            return;
          }
          if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
            selectShellCommandElement(shell);
            showResourcePaletteForShellCommand(shell);
            event->accept();
            return;
          }
          if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
            selectRelatedDisplayElement(related);
            showResourcePaletteForRelatedDisplay(related);
            event->accept();
            return;
          }
          if (auto *textMonitor = dynamic_cast<TextMonitorElement *>(widget)) {
            selectTextMonitorElement(textMonitor);
            showResourcePaletteForTextMonitor(textMonitor);
            event->accept();
            return;
          }
          if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
            selectMeterElement(meter);
            showResourcePaletteForMeter(meter);
            event->accept();
            return;
          }
          if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
            selectScaleMonitorElement(scale);
            showResourcePaletteForScale(scale);
            event->accept();
            return;
          }
          if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
            selectStripChartElement(strip);
            showResourcePaletteForStripChart(strip);
            event->accept();
            return;
          }
          if (auto *cart = dynamic_cast<CartesianPlotElement *>(widget)) {
            selectCartesianPlotElement(cart);
            showResourcePaletteForCartesianPlot(cart);
            event->accept();
            return;
          }
          if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
            selectBarMonitorElement(bar);
            showResourcePaletteForBar(bar);
            event->accept();
            return;
          }
          if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
            selectByteMonitorElement(byte);
            showResourcePaletteForByte(byte);
            event->accept();
            return;
          }
          if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
            selectRectangleElement(rectangle);
            showResourcePaletteForRectangle(rectangle);
            event->accept();
            return;
          }
          if (auto *image = dynamic_cast<ImageElement *>(widget)) {
            selectImageElement(image);
            showResourcePaletteForImage(image);
            event->accept();
            return;
          }
          if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
            selectOvalElement(oval);
            showResourcePaletteForOval(oval);
            event->accept();
            return;
          }
          if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
            selectArcElement(arc);
            showResourcePaletteForArc(arc);
            event->accept();
            return;
          }
          if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
            selectPolylineElement(polyline);
            showResourcePaletteForPolyline(polyline);
            event->accept();
            return;
          }
          if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
            selectPolygonElement(polygon);
            showResourcePaletteForPolygon(polygon);
            event->accept();
            return;
          }
          if (auto *line = dynamic_cast<LineElement *>(widget)) {
            selectLineElement(line);
            showResourcePaletteForLine(line);
            event->accept();
            return;
          }
        }

        clearRectangleSelection();
        clearOvalSelection();
        clearTextSelection();
        clearTextMonitorSelection();
        clearMeterSelection();
        clearBarMonitorSelection();
        clearByteMonitorSelection();
        clearLineSelection();

        if (displaySelected_) {
          clearDisplaySelection();
          closeResourcePalette();
          event->accept();
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
        event->accept();
        return;
      }
    }

    if (event->button() == Qt::RightButton) {
      if (auto state = state_.lock(); state && state->editMode) {
        lastContextMenuGlobalPos_ = event->globalPos();
        showEditContextMenu(event->globalPos());
        event->accept();
        return;
      }
    }

    QMainWindow::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
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

    if (rubberBandActive_) {
      if (auto state = state_.lock(); state && state->editMode && displayArea_) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        updateCreateRubberBand(areaPos);
        event->accept();
        return;
      }
    }

    QMainWindow::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      if (rubberBandActive_) {
        if (auto state = state_.lock(); state && state->editMode
            && displayArea_) {
          const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
          finishCreateRubberBand(areaPos);
          event->accept();
          return;
        }
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
  void loadTextElement(const AdlNode &textNode);
  void loadTextMonitorElement(const AdlNode &textUpdateNode);
  void loadMeterElement(const AdlNode &meterNode);
  void loadImageElement(const AdlNode &imageNode);
  void loadRectangleElement(const AdlNode &rectangleNode);
  void loadOvalElement(const AdlNode &ovalNode);
  void loadArcElement(const AdlNode &arcNode);
  void loadPolygonElement(const AdlNode &polygonNode);
  void loadPolylineElement(const AdlNode &polylineNode);
  QRect parseObjectGeometry(const AdlNode &parent) const;
  bool parseAdlPoint(const QString &text, QPoint *point) const;
  QVector<QPoint> parsePolylinePoints(const AdlNode &polylineNode) const;
  void ensureElementInStack(QWidget *element);
  QColor colorForIndex(int index) const;
  TextColorMode parseTextColorMode(const QString &value) const;
  TextVisibilityMode parseVisibilityMode(const QString &value) const;
  MeterLabel parseMeterLabel(const QString &value) const;
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
  bool polygonCreationActive_ = false;
  PolygonElement *activePolygonElement_ = nullptr;
  QVector<QPoint> polygonCreationPoints_;
  bool polylineCreationActive_ = false;
  PolylineElement *activePolylineElement_ = nullptr;
  QVector<QPoint> polylineCreationPoints_;
  QList<QPointer<QWidget>> elementStack_;
  QRubberBand *rubberBand_ = nullptr;
  bool rubberBandActive_ = false;
  QPoint rubberBandOrigin_;
  CreateTool activeRubberBandTool_ = CreateTool::kNone;

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

  void clearSelections()
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

    if (selectedTextElement_) {
      TextElement *element = selectedTextElement_;
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
      const QRect geometry = element->geometry();
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
    return selectedTextElement_ || selectedTextEntryElement_
        || selectedSliderElement_ || selectedWheelSwitchElement_
        || selectedChoiceButtonElement_ || selectedMenuElement_
        || selectedMessageButtonElement_ || selectedShellCommandElement_
        || selectedRelatedDisplayElement_ || selectedTextMonitorElement_
        || selectedMeterElement_ || selectedBarMonitorElement_
        || selectedScaleMonitorElement_ || selectedStripChartElement_
        || selectedCartesianPlotElement_ || selectedByteMonitorElement_
        || selectedRectangle_ || selectedImage_ || selectedOval_
        || selectedArc_ || selectedLine_ || selectedPolyline_
        || selectedPolygon_;
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
        [element]() {
          return element->geometry();
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
          if (constrained != element->geometry()) {
            element->setGeometry(constrained);
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumSliderWidth) {
            adjusted.setWidth(kMinimumSliderWidth);
          }
          if (adjusted.height() < kMinimumSliderHeight) {
            adjusted.setHeight(kMinimumSliderHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumWheelSwitchWidth) {
            adjusted.setWidth(kMinimumWheelSwitchWidth);
          }
          if (adjusted.height() < kMinimumWheelSwitchHeight) {
            adjusted.setHeight(kMinimumWheelSwitchHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() { return element->geometry(); },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() { return element->geometry(); },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumTextWidth) {
            adjusted.setWidth(kMinimumTextWidth);
          }
          if (adjusted.height() < kMinimumTextHeight) {
            adjusted.setHeight(kMinimumTextHeight);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumMeterSize) {
            adjusted.setWidth(kMinimumMeterSize);
          }
          if (adjusted.height() < kMinimumMeterSize) {
            adjusted.setHeight(kMinimumMeterSize);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumStripChartWidth) {
            adjusted.setWidth(kMinimumStripChartWidth);
          }
          if (adjusted.height() < kMinimumStripChartHeight) {
            adjusted.setHeight(kMinimumStripChartHeight);
          }
          adjusted = adjustRectToDisplayArea(adjusted);
          element->setGeometry(adjusted);
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

    dialog->showForCartesianPlot(
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumCartesianPlotWidth) {
            adjusted.setWidth(kMinimumCartesianPlotWidth);
          }
          if (adjusted.height() < kMinimumCartesianPlotHeight) {
            adjusted.setHeight(kMinimumCartesianPlotHeight);
          }
          adjusted = adjustRectToDisplayArea(adjusted);
          element->setGeometry(adjusted);
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumBarSize) {
            adjusted.setWidth(kMinimumBarSize);
          }
          if (adjusted.height() < kMinimumBarSize) {
            adjusted.setHeight(kMinimumBarSize);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumScaleSize) {
            adjusted.setWidth(kMinimumScaleSize);
          }
          if (adjusted.height() < kMinimumScaleSize) {
            adjusted.setHeight(kMinimumScaleSize);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumByteSize) {
            adjusted.setWidth(kMinimumByteSize);
          }
          if (adjusted.height() < kMinimumByteSize) {
            adjusted.setHeight(kMinimumByteSize);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = adjustRectToDisplayArea(newGeometry);
          if (adjusted.width() < 1) {
            adjusted.setWidth(1);
          }
          if (adjusted.height() < 1) {
            adjusted.setHeight(1);
          }
          element->setGeometry(adjusted);
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
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = adjustRectToDisplayArea(newGeometry);
          if (adjusted.width() < 1) {
            adjusted.setWidth(1);
          }
          if (adjusted.height() < 1) {
            adjusted.setHeight(1);
          }
          element->setGeometry(adjusted);
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
    selectedTextElement_ = element;
    selectedTextElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedTextEntryElement_ = element;
    selectedTextEntryElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedSliderElement_ = element;
    selectedSliderElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedWheelSwitchElement_ = element;
    selectedWheelSwitchElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedChoiceButtonElement_ = element;
    selectedChoiceButtonElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedMenuElement_ = element;
    selectedMenuElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedMessageButtonElement_ = element;
    selectedMessageButtonElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedShellCommandElement_ = element;
    selectedShellCommandElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedRelatedDisplayElement_ = element;
    selectedRelatedDisplayElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedTextMonitorElement_ = element;
    selectedTextMonitorElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedMeterElement_ = element;
    selectedMeterElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedScaleMonitorElement_ = element;
    selectedScaleMonitorElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedStripChartElement_ = element;
    selectedStripChartElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedCartesianPlotElement_ = element;
    selectedCartesianPlotElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedBarMonitorElement_ = element;
    selectedBarMonitorElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedByteMonitorElement_ = element;
    selectedByteMonitorElement_->setSelected(true);
    bringElementToFront(element);
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
    selectedRectangle_ = element;
    selectedRectangle_->setSelected(true);
    bringElementToFront(element);
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
    selectedImage_ = element;
    selectedImage_->setSelected(true);
    bringElementToFront(element);
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
    selectedOval_ = element;
    selectedOval_->setSelected(true);
    bringElementToFront(element);
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
    selectedArc_ = element;
    selectedArc_->setSelected(true);
    bringElementToFront(element);
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
    selectedLine_ = element;
    selectedLine_->setSelected(true);
    bringElementToFront(element);
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
    selectedPolyline_ = element;
    selectedPolyline_->setSelected(true);
    bringElementToFront(element);
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
    selectedPolygon_ = element;
    selectedPolygon_->setSelected(true);
    bringElementToFront(element);
  }

  void startCreateRubberBand(const QPoint &areaPos, CreateTool tool)
  {
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
    addMenuAction(&menu, QStringLiteral("Raise"));
    addMenuAction(&menu, QStringLiteral("Lower"));

    menu.addSeparator();
    addMenuAction(&menu, QStringLiteral("Group"));
    addMenuAction(&menu, QStringLiteral("Ungroup"));

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
    addMenuAction(&menu, QStringLiteral("Unselect"));
    addMenuAction(&menu, QStringLiteral("Select All"));
    addMenuAction(&menu, QStringLiteral("Select Display"));

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
  stream.setCodec("UTF-8");
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
    if (child.name.compare(QStringLiteral("text"), Qt::CaseInsensitive)
        == 0) {
      loadTextElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("text update"), Qt::CaseInsensitive)
        == 0
        || child.name.compare(QStringLiteral("text monitor"), Qt::CaseInsensitive)
            == 0) {
      loadTextMonitorElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("meter"), Qt::CaseInsensitive)
        == 0) {
      loadMeterElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("image"), Qt::CaseInsensitive)
        == 0) {
      loadImageElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("rectangle"), Qt::CaseInsensitive)
        == 0) {
      loadRectangleElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("oval"), Qt::CaseInsensitive)
        == 0) {
      loadOvalElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("arc"), Qt::CaseInsensitive)
        == 0) {
      loadArcElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("polygon"), Qt::CaseInsensitive)
        == 0) {
      loadPolygonElement(child);
      elementLoaded = true;
      continue;
    }
    if (child.name.compare(QStringLiteral("polyline"), Qt::CaseInsensitive)
        == 0
        || child.name.compare(QStringLiteral("line"), Qt::CaseInsensitive)
            == 0) {
      loadPolylineElement(child);
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
  stream.setCodec("UTF-8");

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
      AdlWriter::writeLimitsSection(stream, 1, entry->limits());
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"valuator\" {"));
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
      AdlWriter::writeLimitsSection(stream, 1, slider->limits());
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
      AdlWriter::writeLimitsSection(stream, 1, wheel->limits());
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
      AdlWriter::writeLimitsSection(stream, 1, bar->limits());
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
      AdlWriter::writeLimitsSection(stream, 1, scale->limits());
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

inline void DisplayWindow::ensureElementInStack(QWidget *element)
{
  if (!element) {
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

inline void DisplayWindow::loadTextElement(const AdlNode &textNode)
{
  if (!displayArea_) {
    return;
  }
  QRect geometry = parseObjectGeometry(textNode);
  if (geometry.height() < kMinimumTextElementHeight) {
    geometry.setHeight(kMinimumTextElementHeight);
  }
  auto *element = new TextElement(displayArea_);
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

  element->show();
  element->setSelected(false);
  textElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadTextMonitorElement(
    const AdlNode &textUpdateNode)
{
  if (!displayArea_) {
    return;
  }

  QRect geometry = parseObjectGeometry(textUpdateNode);
  auto *element = new TextMonitorElement(displayArea_);
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

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
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

  element->show();
  element->setSelected(false);
  textMonitorElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadMeterElement(const AdlNode &meterNode)
{
  if (!displayArea_) {
    return;
  }

  QRect geometry = parseObjectGeometry(meterNode);
  if (geometry.width() < kMinimumMeterSize) {
    geometry.setWidth(kMinimumMeterSize);
  }
  if (geometry.height() < kMinimumMeterSize) {
    geometry.setHeight(kMinimumMeterSize);
  }

  auto *element = new MeterElement(displayArea_);
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

    if (const AdlProperty *prop = ::findProperty(*limitsNode,
            QStringLiteral("loprSrc"))) {
      limits.lowSource = parseLimitSource(prop->value);
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

  element->show();
  element->setSelected(false);
  meterElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadImageElement(const AdlNode &imageNode)
{
  if (!displayArea_) {
    return;
  }

  QRect geometry = parseObjectGeometry(imageNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }

  auto *element = new ImageElement(displayArea_);
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

  element->show();
  element->setSelected(false);
  imageElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadRectangleElement(const AdlNode &rectangleNode)
{
  if (!displayArea_) {
    return;
  }

  QRect geometry = parseObjectGeometry(rectangleNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }

  auto *element = new RectangleElement(displayArea_);
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

  element->show();
  element->setSelected(false);
  rectangleElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadOvalElement(const AdlNode &ovalNode)
{
  if (!displayArea_) {
    return;
  }

  QRect geometry = parseObjectGeometry(ovalNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }

  auto *element = new OvalElement(displayArea_);
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

  element->show();
  element->setSelected(false);
  ovalElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadArcElement(const AdlNode &arcNode)
{
  if (!displayArea_) {
    return;
  }

  QRect geometry = parseObjectGeometry(arcNode);
  if (geometry.width() < kMinimumRectangleSize) {
    geometry.setWidth(kMinimumRectangleSize);
  }
  if (geometry.height() < kMinimumRectangleSize) {
    geometry.setHeight(kMinimumRectangleSize);
  }

  auto *element = new ArcElement(displayArea_);
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

  element->show();
  element->setSelected(false);
  arcElements_.append(element);
  ensureElementInStack(element);
}

inline void DisplayWindow::loadPolygonElement(const AdlNode &polygonNode)
{
  if (!displayArea_) {
    return;
  }

  QVector<QPoint> points = parsePolylinePoints(polygonNode);
  if (points.size() < 3) {
    return;
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

  auto *element = new PolygonElement(displayArea_);
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
  element->show();
  element->setSelected(false);
  polygonElements_.append(element);
  ensureElementInStack(element);
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

inline void DisplayWindow::loadPolylineElement(
    const AdlNode &polylineNode)
{
  if (!displayArea_) {
    return;
  }

  QVector<QPoint> points = parsePolylinePoints(polylineNode);
  if (points.size() < 2) {
    return;
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
    auto *element = new LineElement(displayArea_);
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
    element->show();
    element->setSelected(false);
    lineElements_.append(element);
    ensureElementInStack(element);
    return;
  }

  auto *element = new PolylineElement(displayArea_);
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
  element->show();
  element->setSelected(false);
  polylineElements_.append(element);
  ensureElementInStack(element);
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
