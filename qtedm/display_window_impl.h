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

  bool save(QWidget *dialogParent = nullptr);
  bool saveAs(QWidget *dialogParent = nullptr);
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
            || state->createTool == CreateTool::kMeter
            || state->createTool == CreateTool::kRectangle
            || state->createTool == CreateTool::kOval
            || state->createTool == CreateTool::kArc
            || state->createTool == CreateTool::kLine
            || state->createTool == CreateTool::kImage) {
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
  QString colormapName_;
  bool dirty_ = true;
  bool displaySelected_ = false;
  bool gridOn_ = kDefaultGridOn;
  int gridSpacing_ = kDefaultGridSpacing;
  QPoint lastContextMenuGlobalPos_;
  QList<TextElement *> textElements_;
  TextElement *selectedTextElement_ = nullptr;
  QList<TextMonitorElement *> textMonitorElements_;
  TextMonitorElement *selectedTextMonitorElement_ = nullptr;
  QList<MeterElement *> meterElements_;
  MeterElement *selectedMeterElement_ = nullptr;
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
    clearTextMonitorSelection();
    clearMeterSelection();
    clearRectangleSelection();
    clearImageSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
    clearPolygonSelection();
    closeResourcePalette();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    dialog->showForText(
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          QRect adjusted = newGeometry;
          if (adjusted.width() < kMinimumRectangleSize) {
            adjusted.setWidth(kMinimumRectangleSize);
          }
          if (adjusted.height() < kMinimumRectangleSize) {
            adjusted.setHeight(kMinimumRectangleSize);
          }
          element->setGeometry(adjustRectToDisplayArea(adjusted));
          markDirty();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearMeterSelection();
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
    clearTextMonitorSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    clearTextMonitorSelection();
    clearMeterSelection();
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
    QRect target = adjustRectToDisplayArea(rect);
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
            || state->createTool == CreateTool::kMeter
            || state->createTool == CreateTool::kRectangle
            || state->createTool == CreateTool::kOval
            || state->createTool == CreateTool::kArc
            || state->createTool == CreateTool::kPolygon
            || state->createTool == CreateTool::kPolyline
            || state->createTool == CreateTool::kLine
            || state->createTool == CreateTool::kImage);
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
    addMenuAction(monitorsMenu, QStringLiteral("Bar Monitor"));
    addMenuAction(monitorsMenu, QStringLiteral("Byte Monitor"));
    addMenuAction(monitorsMenu, QStringLiteral("Scale Monitor"));
    addMenuAction(monitorsMenu, QStringLiteral("Strip Chart"));
    addMenuAction(monitorsMenu, QStringLiteral("Cartesian Plot"));

    auto *controllersMenu = objectMenu->addMenu(QStringLiteral("Controllers"));
    addMenuAction(controllersMenu, QStringLiteral("Text Entry"));
    addMenuAction(controllersMenu, QStringLiteral("Choice Button"));
    addMenuAction(controllersMenu, QStringLiteral("Menu"));
    addMenuAction(controllersMenu, QStringLiteral("Slider"));
    addMenuAction(controllersMenu, QStringLiteral("Message Button"));
    addMenuAction(controllersMenu, QStringLiteral("Related Display"));
    addMenuAction(controllersMenu, QStringLiteral("Shell Command"));
    addMenuAction(controllersMenu, QStringLiteral("Wheel Switch"));

    addMenuAction(&menu, QStringLiteral("Undo"));

    menu.addSeparator();
    addMenuAction(&menu, QStringLiteral("Cut"), QKeySequence(QStringLiteral("Shift+Del")));
    addMenuAction(&menu, QStringLiteral("Copy"), QKeySequence(QStringLiteral("Ctrl+Ins")));
    addMenuAction(&menu, QStringLiteral("Paste"), QKeySequence(QStringLiteral("Shift+Ins")));

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

inline bool DisplayWindow::writeAdlFile(const QString &filePath) const
{
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setCodec("UTF-8");

  const QFileInfo info(filePath);
  QString fileName = info.filePath();
  if (info.isAbsolute()) {
    fileName = info.absoluteFilePath();
  }
  if (fileName.isEmpty()) {
    fileName = info.fileName();
  }
  fileName = QDir::cleanPath(fileName);
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
      AdlWriter::writeBasicAttributeSection(stream, 1,
          AdlWriter::medmColorIndex(text->foregroundColor()),
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

    if (auto *monitor = dynamic_cast<TextMonitorElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0,
          QStringLiteral("\"text update\" {"));
      AdlWriter::writeObjectSection(stream, 1, monitor->geometry());
      AdlWriter::writeMonitorSection(stream, 1, monitor->channel(0),
          AdlWriter::medmColorIndex(monitor->foregroundColor()),
          AdlWriter::medmColorIndex(monitor->backgroundColor()));
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
      AdlWriter::writeLimitsSection(stream, 1, monitor->limits());
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("rectangle {"));
      AdlWriter::writeObjectSection(stream, 1, rectangle->geometry());
      AdlWriter::writeBasicAttributeSection(stream, 1,
          AdlWriter::medmColorIndex(rectangle->color()), rectangle->lineStyle(),
          rectangle->fill(), rectangle->lineWidth());
      AdlWriter::writeDynamicAttributeSection(stream, 1, rectangle->colorMode(),
          rectangle->visibilityMode(), rectangle->visibilityCalc(),
          AdlWriter::collectChannels(rectangle));
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
      AdlWriter::writeDynamicAttributeSection(stream, 1, image->colorMode(),
          image->visibilityMode(), image->visibilityCalc(),
          AdlWriter::collectChannels(image));
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("oval {"));
      AdlWriter::writeObjectSection(stream, 1, oval->geometry());
      AdlWriter::writeBasicAttributeSection(stream, 1, AdlWriter::medmColorIndex(oval->color()),
          oval->lineStyle(), oval->fill(), oval->lineWidth());
      AdlWriter::writeDynamicAttributeSection(stream, 1, oval->colorMode(),
          oval->visibilityMode(), oval->visibilityCalc(),
          AdlWriter::collectChannels(oval));
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("}"));
      continue;
    }

    if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      AdlWriter::writeIndentedLine(stream, 0, QStringLiteral("arc {"));
      AdlWriter::writeObjectSection(stream, 1, arc->geometry());
      AdlWriter::writeBasicAttributeSection(stream, 1, AdlWriter::medmColorIndex(arc->color()),
          arc->lineStyle(), arc->fill(), arc->lineWidth());
      AdlWriter::writeDynamicAttributeSection(stream, 1, arc->colorMode(),
          arc->visibilityMode(), arc->visibilityCalc(),
          AdlWriter::collectChannels(arc));
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
          line->lineStyle(), RectangleFill::kSolid, line->lineWidth());
      AdlWriter::writeDynamicAttributeSection(stream, 1, line->colorMode(),
          line->visibilityMode(), line->visibilityCalc(),
          AdlWriter::collectChannels(line));
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
          RectangleFill::kSolid, polyline->lineWidth());
      AdlWriter::writeDynamicAttributeSection(stream, 1, polyline->colorMode(),
          polyline->visibilityMode(), polyline->visibilityCalc(),
          AdlWriter::collectChannels(polyline));
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
      AdlWriter::writeDynamicAttributeSection(stream, 1, polygon->colorMode(),
          polygon->visibilityMode(), polygon->visibilityCalc(),
          AdlWriter::collectChannels(polygon));
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
