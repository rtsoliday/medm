#pragma once

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include <cmath>
#include <type_traits>
#include <cstring>
#include <cstdlib>

#include <QDebug>
#include <QClipboard>
#include <QDrag>
#include <QGuiApplication>
#include <QDateTime>
#include <QCursor>
#include <QFont>
#include <QPen>
#include <QPainter>
#include <QPixmap>
#include <QLabel>
#include <QMimeData>
#include <QPointer>
#include <QList>
#include <QTimer>
#include <QSet>
#include <QStringList>
#include <QEventLoop>
#include <QTextStream>

#ifdef KeyPress
#  undef KeyPress
#endif
#ifdef KeyRelease
#  undef KeyRelease
#endif

#include <cadef.h>
#include <db_access.h>
#include <alarm.h>
#include <epicsTime.h>

#include "channel_access_context.h"
#include "display_properties.h"
#include "statistics_tracker.h"
#include "display_list_dialog.h"
#include "pv_info_dialog.h"
#include "cursor_utils.h"

namespace {
constexpr double kChannelRetryTimeoutSeconds = 1.0;
constexpr double kPvInfoTimeoutSeconds = 1.0;
constexpr qint64 kEpicsEpochOffsetSeconds = 631152000; // 1990-01-01 -> 1970-01-01
}

inline void setUtf8Encoding(QTextStream &stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Utf8);
#else
  stream.setCodec("UTF-8");
#endif
}

inline void setLatin1Encoding(QTextStream &stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  stream.setEncoding(QStringConverter::Latin1);
#else
  stream.setCodec("ISO-8859-1");
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

  void setExecuteMode(bool executeMode)
  {
    if (executeModeActive_ == executeMode) {
      return;
    }
    executeModeActive_ = executeMode;
    update();
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    QWidget::paintEvent(event);

    if (gridOn_ && gridSpacing_ > 0 && !executeModeActive_) {
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
  bool executeModeActive_ = false;
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
  displayArea_->setMinimumSize(kMinimumDisplayWidth, kMinimumDisplayHeight);
    displayArea_->setGridSpacing(gridSpacing_);
    displayArea_->setGridOn(gridOn_);
    displayArea_->setGridColor(displayPalette.color(QPalette::WindowText));
    setCentralWidget(displayArea_);

    undoStack_ = new QUndoStack(this);
    cleanStateSnapshot_ = serializeStateForUndo(filePath_);
    lastCommittedState_ = cleanStateSnapshot_;
    undoStack_->setClean();
    QObject::connect(undoStack_, &QUndoStack::cleanChanged, this,
        [this](bool) {
          updateDirtyFromUndoStack();
        });
    QObject::connect(undoStack_, &QUndoStack::indexChanged, this,
        [this]() {
          notifyMenus();
        });
    QObject::connect(undoStack_, &QUndoStack::canUndoChanged, this,
        [this]() {
          notifyMenus();
        });
    QObject::connect(undoStack_, &QUndoStack::undoTextChanged, this,
        [this]() {
          notifyMenus();
        });

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

  ~DisplayWindow() override
  {
    leaveExecuteMode();
    clearSelections();
    if (executeDragTooltipLabel_) {
      executeDragTooltipLabel_->deleteLater();
      executeDragTooltipLabel_.clear();
    }
    if (undoStack_) {
      undoStack_->disconnect(this);
    }
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
    updateResourcePaletteDisplayControls();
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
    updateResourcePaletteDisplayControls();
  }

  bool isSnapToGridEnabled() const
  {
    return snapToGrid_;
  }

  void setSnapToGrid(bool snap)
  {
    if (snapToGrid_ == snap) {
      return;
    }
    setNextUndoLabel(QStringLiteral("Toggle Snap To Grid"));
    snapToGrid_ = snap;
    markDirty();
    updateResourcePaletteDisplayControls();
  }

  void promptForGridSpacing()
  {
    const int current = gridSpacing();
    bool ok = false;
    const int spacing = QInputDialog::getInt(this,
        QStringLiteral("Grid Spacing"),
        QStringLiteral("Grid Spacing:"), current,
        kMinimumGridSpacing, 4096, 1, &ok);
    if (!ok) {
      return;
    }
    setGridSpacing(spacing);
  }

  void syncCreateCursor()
  {
    updateCreateCursor();
  }

  void setCreateTool(CreateTool tool)
  {
    if (tool == CreateTool::kNone) {
      deactivateCreateTool();
    } else {
      activateCreateTool(tool);
    }
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

  void enterExecuteMode();
  void leaveExecuteMode();

  void handleEditModeChanged(bool editMode)
  {
    if (!editMode) {
      if (!executeModeActive_) {
        enterExecuteMode();
      }
    } else if (executeModeActive_) {
      leaveExecuteMode();
    }
  }

  void findOutliers()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return;
    }

    clearSelections();

    const QSize areaSize = displayArea_->size();
    const QRect visibleRect(QPoint(0, 0), areaSize);

    struct OutlierRecord {
      QWidget *widget = nullptr;
      QRect rect;
      bool total = false;
    };

    QList<OutlierRecord> outliers;

    auto elementTypeName = [](QWidget *widget) -> QString {
      if (!widget) {
        return QStringLiteral("Unknown");
      }
      if (dynamic_cast<TextElement *>(widget)) {
        return QStringLiteral("Text");
      }
      if (dynamic_cast<TextEntryElement *>(widget)) {
        return QStringLiteral("Text Entry");
      }
      if (dynamic_cast<SliderElement *>(widget)) {
        return QStringLiteral("Slider");
      }
      if (dynamic_cast<WheelSwitchElement *>(widget)) {
        return QStringLiteral("Wheel Switch");
      }
      if (dynamic_cast<ChoiceButtonElement *>(widget)) {
        return QStringLiteral("Choice Button");
      }
      if (dynamic_cast<MenuElement *>(widget)) {
        return QStringLiteral("Menu");
      }
      if (dynamic_cast<MessageButtonElement *>(widget)) {
        return QStringLiteral("Message Button");
      }
      if (dynamic_cast<ShellCommandElement *>(widget)) {
        return QStringLiteral("Shell Command");
      }
      if (dynamic_cast<RelatedDisplayElement *>(widget)) {
        return QStringLiteral("Related Display");
      }
      if (dynamic_cast<TextMonitorElement *>(widget)) {
        return QStringLiteral("Text Monitor");
      }
      if (dynamic_cast<MeterElement *>(widget)) {
        return QStringLiteral("Meter");
      }
      if (dynamic_cast<BarMonitorElement *>(widget)) {
        return QStringLiteral("Bar Monitor");
      }
      if (dynamic_cast<ScaleMonitorElement *>(widget)) {
        return QStringLiteral("Scale Monitor");
      }
      if (dynamic_cast<StripChartElement *>(widget)) {
        return QStringLiteral("Strip Chart");
      }
      if (dynamic_cast<CartesianPlotElement *>(widget)) {
        return QStringLiteral("Cartesian Plot");
      }
      if (dynamic_cast<ByteMonitorElement *>(widget)) {
        return QStringLiteral("Byte Monitor");
      }
      if (dynamic_cast<RectangleElement *>(widget)) {
        return QStringLiteral("Rectangle");
      }
      if (dynamic_cast<ImageElement *>(widget)) {
        return QStringLiteral("Image");
      }
      if (dynamic_cast<OvalElement *>(widget)) {
        return QStringLiteral("Oval");
      }
      if (dynamic_cast<ArcElement *>(widget)) {
        return QStringLiteral("Arc");
      }
      if (dynamic_cast<LineElement *>(widget)) {
        return QStringLiteral("Line");
      }
      if (dynamic_cast<PolylineElement *>(widget)) {
        return QStringLiteral("Polyline");
      }
      if (dynamic_cast<PolygonElement *>(widget)) {
        return QStringLiteral("Polygon");
      }
      if (dynamic_cast<CompositeElement *>(widget)) {
        return QStringLiteral("Composite");
      }
      return QString::fromLatin1(widget->metaObject()->className());
    };

    auto considerWidget = [&](QWidget *widget) {
      if (!widget) {
        return;
      }
      const QRect rect = widgetDisplayRect(widget);
      if (!rect.isValid()) {
        return;
      }
      if (!visibleRect.intersects(rect)) {
        outliers.append({widget, rect, true});
        addWidgetToMultiSelection(widget);
        return;
      }
      if (!visibleRect.contains(rect)) {
        outliers.append({widget, rect, false});
        addWidgetToMultiSelection(widget);
      }
    };

    auto considerList = [&](const auto &list) {
      for (auto *element : list) {
        considerWidget(element);
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
    considerList(compositeElements_);

    int partialCount = 0;
    int totalCount = 0;
    for (const OutlierRecord &record : outliers) {
      if (record.total) {
        ++totalCount;
      } else {
        ++partialCount;
      }
    }

    if (!outliers.isEmpty()) {
      updateSelectionAfterMultiChange();
    } else {
      notifyMenus();
    }

    QString summary = QStringLiteral(
        "There are %1 objects partially out of the visible display area.\n"
        "There are %2 objects totally out of the visible display area.");
    summary = summary.arg(partialCount).arg(totalCount);
    if (!outliers.isEmpty()) {
      summary.append(QStringLiteral(
          "\n\nThese %1 objects are currently selected.")
          .arg(outliers.size()));
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("Find Outliers"));
    box.setTextFormat(Qt::PlainText);
    box.setText(summary);
    box.setStandardButtons(QMessageBox::Ok);
    QAbstractButton *listButton = nullptr;
    if (!outliers.isEmpty()) {
      listButton = box.addButton(QStringLiteral("List Them"),
          QMessageBox::ActionRole);
    }
    box.exec();

    if (listButton && box.clickedButton() == listButton) {
      QString detail = QStringLiteral("Outliers:\n"
          "Display width=%1  Display height=%2\n")
          .arg(visibleRect.width())
          .arg(visibleRect.height());
      for (const OutlierRecord &record : outliers) {
        detail.append(QStringLiteral("%1: x=%2 y=%3 width=%4 height=%5 %6\n")
            .arg(elementTypeName(record.widget))
            .arg(record.rect.x())
            .arg(record.rect.y())
            .arg(record.rect.width())
            .arg(record.rect.height())
            .arg(record.total ? QStringLiteral("(total)")
                               : QStringLiteral("(partial)")));
      }
      QMessageBox detailBox(this);
      detailBox.setIcon(QMessageBox::Information);
      detailBox.setWindowTitle(QStringLiteral("Outliers"));
      detailBox.setTextFormat(Qt::PlainText);
      detailBox.setText(detail.trimmed());
      detailBox.addButton(QMessageBox::Ok);
      detailBox.exec();
    }

  }

  void showEditSummaryDialog()
  {
    setAsActiveDisplay();

    const QString summary = QStringLiteral(
        "             EDIT Operations Summary\n"
        "\n"
        "When a create tool is active\n"
        "============================\n"
        "Btn1         Click or drag to place the selected object.\n"
        "Shift-Btn1   (Polygon/Polyline) Constrain segments to 45-degree steps.\n"
        "Double-Btn1  (Polygon/Polyline) Finish drawing.\n"
        "Btn3         Popup edit menu.\n"
        "\n"
        "While selecting and editing\n"
        "===========================\n"
        "Btn1         Select the object under the pointer.\n"
        "Shift-Btn1   Add the object to the current selection.\n"
        "Ctrl-Btn1    Toggle whether the object is selected.\n"
        "Btn1-Drag    Box-select objects in the display.\n"
        "Btn2-Drag    Move the current selection.\n"
        "Ctrl-Btn2    Resize the current selection.\n"
        "Btn3         Popup edit menu.\n"
        "Vertices     Drag polygon/polyline vertices; hold Shift to snap.\n"
        "\n"
        "Keyboard\n"
        "========\n"
        "Arrow        Move selected objects by 1 pixel.\n"
        "Shift-Arrow  Move selected objects by 10 pixels.\n"
        "Ctrl-Arrow   Resize selected objects by 1 pixel.\n"
        "Ctrl-Shift-Arrow Resize selected objects by 10 pixels.\n");

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("Edit Summary"));
    box.setTextFormat(Qt::PlainText);
    box.setText(summary);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
  }

  void refreshDisplayView()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !displayArea_) {
      return;
    }

    if (!state->editMode) {
      displayArea_->update();
      for (const auto &entry : elementStack_) {
        if (QWidget *widget = entry.data()) {
          widget->update();
        }
      }
      return;
    }

    for (const auto &entry : elementStack_) {
      QWidget *widget = entry.data();
      if (!widget) {
        continue;
      }
      if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
        const QList<QWidget *> children = composite->childWidgets();
        for (QWidget *child : children) {
          if (child) {
            child->raise();
            child->update();
          }
        }
      }
      widget->raise();
      widget->update();
    }

    displayArea_->update();
    refreshResourcePaletteGeometry();
    notifyMenus();
  }

  void cutSelection()
  {
    setNextUndoLabel(QStringLiteral("Cut Selection"));
    copySelectionInternal(true);
  }

  void copySelection()
  {
    copySelectionInternal(false);
  }

  void pasteSelection()
  {
    setNextUndoLabel(QStringLiteral("Paste Selection"));
    pasteFromClipboard();
  }

  void triggerCutFromMenu()
  {
    setAsActiveDisplay();
    cutSelection();
  }

  void triggerCopyFromMenu()
  {
    setAsActiveDisplay();
    copySelection();
  }

  void triggerPasteFromMenu()
  {
    setAsActiveDisplay();
    pasteSelection();
  }

  void triggerGroupFromMenu()
  {
    setAsActiveDisplay();
    groupSelectedElements();
  }

  void triggerUngroupFromMenu()
  {
    setAsActiveDisplay();
    ungroupSelectedElements();
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

    setNextUndoLabel(QStringLiteral("Raise Selection"));
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
          /* No need to update elementStackSet_ - same widget is still in stack */
          widget->raise();
          reordered = true;
          break;
        }
      }
    }

    if (reordered) {
      refreshStackingOrder();
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

    setNextUndoLabel(QStringLiteral("Lower Selection"));
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
          /* No need to update elementStackSet_ - same widget is still in stack */
          widget->lower();
          reordered = true;
          break;
        }
      }
    }

    if (reordered) {
      refreshStackingOrder();
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

    setNextUndoLabel(QStringLiteral("Group Elements"));
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

    /* Expand composite bounds to encompass all child widgets */
    composite->expandToFitChildren();

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

    refreshStackingOrder();
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

    setNextUndoLabel(QStringLiteral("Ungroup Elements"));
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
    refreshStackingOrder();
    markDirty();
    refreshResourcePaletteGeometry();
    notifyMenus();
  }

private:
  class DisplaySnapshotCommand : public QUndoCommand
  {
  public:
    DisplaySnapshotCommand(DisplayWindow &window, QByteArray before,
        QByteArray after, QString label)
      : QUndoCommand(std::move(label))
      , window_(window)
      , before_(std::move(before))
      , after_(std::move(after))
    {
    }

    void undo() override
    {
      window_.restoreSerializedState(before_);
      skipFirstRedo_ = false;
    }

    void redo() override
    {
      if (skipFirstRedo_) {
        skipFirstRedo_ = false;
        return;
      }
      window_.restoreSerializedState(after_);
    }

  private:
    DisplayWindow &window_;
    QByteArray before_;
    QByteArray after_;
    bool skipFirstRedo_ = true;
  };

  void setNextUndoLabel(const QString &label);
  QByteArray serializeStateForUndo(const QString &fileNameHint = QString()) const;
  bool restoreSerializedState(const QByteArray &data);
  void updateDirtyFromUndoStack();

  enum class AlignmentMode {
    kLeft,
    kHorizontalCenter,
    kRight,
    kTop,
    kVerticalCenter,
    kBottom,
  };

  enum class OrientationAction {
    kFlipHorizontal,
    kFlipVertical,
    kRotateClockwise,
    kRotateCounterclockwise,
  };

  enum class ResizeDirection {
    kLeft,
    kRight,
    kUp,
    kDown,
  };

  enum class VertexEditMode {
    kNone,
    kPolygon,
    kPolyline,
  };

  static constexpr int kVertexHitRadius = 6;

public:
  void alignSelectionLeft()
  {
    alignSelectionInternal(AlignmentMode::kLeft);
  }

  void alignSelectionHorizontalCenter()
  {
    alignSelectionInternal(AlignmentMode::kHorizontalCenter);
  }

  void alignSelectionRight()
  {
    alignSelectionInternal(AlignmentMode::kRight);
  }

  void alignSelectionTop()
  {
    alignSelectionInternal(AlignmentMode::kTop);
  }

  void alignSelectionVerticalCenter()
  {
    alignSelectionInternal(AlignmentMode::kVerticalCenter);
  }

  void alignSelectionBottom()
  {
    alignSelectionInternal(AlignmentMode::kBottom);
  }

  void centerSelectionHorizontallyInDisplay()
  {
    centerSelectionInDisplayInternal(true, false);
  }

  void centerSelectionVerticallyInDisplay()
  {
    centerSelectionInDisplayInternal(false, true);
  }

  void centerSelectionInDisplayBoth()
  {
    centerSelectionInDisplayInternal(true, true);
  }

  void orientSelectionFlipHorizontal()
  {
    orientSelectionInternal(OrientationAction::kFlipHorizontal);
  }

  void orientSelectionFlipVertical()
  {
    orientSelectionInternal(OrientationAction::kFlipVertical);
  }

  void rotateSelectionClockwise()
  {
    orientSelectionInternal(OrientationAction::kRotateClockwise);
  }

  void rotateSelectionCounterclockwise()
  {
    orientSelectionInternal(OrientationAction::kRotateCounterclockwise);
  }

  void sizeSelectionSameSize()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.isEmpty()) {
      return;
    }

    long long totalWidth = 0;
    long long totalHeight = 0;
    int count = 0;
    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      const QRect rect = widgetDisplayRect(widget);
      totalWidth += rect.width();
      totalHeight += rect.height();
      ++count;
    }

    if (count <= 0) {
      return;
    }

    setNextUndoLabel(QStringLiteral("Same Size"));

    const int targetWidth =
        static_cast<int>((totalWidth + count / 2) / count);
    const int targetHeight =
        static_cast<int>((totalHeight + count / 2) / count);

    bool changed = false;
    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      const QRect currentRect = widgetDisplayRect(widget);
      QRect newRect = currentRect;
      newRect.setSize(QSize(std::max(1, targetWidth),
          std::max(1, targetHeight)));
      newRect = adjustRectToDisplayArea(newRect);
      if (newRect == currentRect) {
        continue;
      }
      setWidgetDisplayRect(widget, newRect);
      changed = true;
    }

    if (changed) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void sizeSelectionTextToContents()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    QList<TextElement *> textWidgets;
    for (QWidget *widget : selectedWidgets()) {
      if (auto *text = dynamic_cast<TextElement *>(widget)) {
        textWidgets.append(text);
      }
    }

    if (textWidgets.isEmpty()) {
      return;
    }

    setNextUndoLabel(QStringLiteral("Size to Contents"));

    bool changed = false;
    for (TextElement *text : std::as_const(textWidgets)) {
      if (!text) {
        continue;
      }
      const QRect currentRect = text->geometry();
      const QString content = text->text();
      const QFontMetrics metrics(text->font());
      const QStringList lines =
          content.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
      int usedWidth = 0;
      for (const QString &line : lines) {
        QString sanitized = line;
        sanitized.remove(QLatin1Char('\r'));
        usedWidth = std::max(usedWidth,
            metrics.horizontalAdvance(sanitized));
      }
      if (lines.isEmpty()) {
        usedWidth = metrics.horizontalAdvance(content);
      }
      const int targetWidth = std::max(1, usedWidth);
      const int originalWidth = currentRect.width();
      int newLeft = currentRect.left();
      const Qt::Alignment alignment = text->textAlignment();
      if (alignment & Qt::AlignHCenter) {
        newLeft += (originalWidth - targetWidth) / 2;
      } else if (alignment & Qt::AlignRight) {
        newLeft += (originalWidth - targetWidth);
      }
      QRect newRect(QPoint(newLeft, currentRect.top()),
          QSize(targetWidth, currentRect.height()));
      newRect = adjustRectToDisplayArea(newRect);
      if (newRect == currentRect) {
        continue;
      }
      setWidgetDisplayRect(text, newRect);
      changed = true;
    }

    if (changed) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void alignSelectionPositionToGrid()
  {
    alignSelectionToGridInternal(false);
  }

  void alignSelectionEdgesToGrid()
  {
    alignSelectionToGridInternal(true);
  }

  void spaceSelectionHorizontal()
  {
    spaceSelectionLinear(Qt::Horizontal);
  }

  void spaceSelectionVertical()
  {
    spaceSelectionLinear(Qt::Vertical);
  }

  void spaceSelection2D()
  {
    spaceSelection2DInternal();
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

  bool canAlignSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    return alignableWidgets().size() >= 2;
  }

  bool canAlignSelectionToGrid() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode || gridSpacing_ <= 0) {
      return false;
    }
    return !alignableWidgets().isEmpty();
  }

  bool canOrientSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    return !alignableWidgets().isEmpty();
  }

  bool canSizeSelectionSameSize() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    return alignableWidgets().size() >= 2;
  }

  bool canSizeSelectionTextToContents() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    for (QWidget *widget : selectedWidgets()) {
      if (dynamic_cast<TextElement *>(widget)) {
        return true;
      }
    }
    return false;
  }

  bool canSpaceSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    return alignableWidgets().size() >= 2;
  }

  bool canSpaceSelection2D() const
  {
    return canSpaceSelection();
  }

  bool canCenterSelection() const
  {
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return false;
    }
    return !alignableWidgets().isEmpty();
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
  bool loadFromFile(const QString &filePath, QString *errorMessage = nullptr,
      const QHash<QString, QString> &macros = {});
  QString filePath() const
  {
    return filePath_;
  }

  const QHash<QString, QString> &macroDefinitions() const
  {
    return macroDefinitions_;
  }

  bool isDirty() const
  {
    return dirty_;
  }

  bool hasFilePath() const
  {
    return !filePath_.isEmpty();
  }

  QUndoStack *undoStack() const
  {
    return undoStack_;
  }

protected:
  void focusInEvent(QFocusEvent *event) override
  {
    QMainWindow::focusInEvent(event);
    setAsActiveDisplay();
  }

  void keyPressEvent(QKeyEvent *event) override
  {
    setAsActiveDisplay();
    if (pvInfoPickingActive_) {
      if (event->key() == Qt::Key_Escape) {
        cancelPvInfoPickMode();
        event->accept();
        return;
      }
    }
    if (handleEditArrowKey(event)) {
      event->accept();
      return;
    }
    QMainWindow::keyPressEvent(event);
  }

  void closeEvent(QCloseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override
  {
    setAsActiveDisplay();
    if (pvInfoPickingActive_) {
      if (event->button() == Qt::LeftButton) {
        completePvInfoPick(event->pos());
        event->accept();
        return;
      }
      if (event->button() == Qt::RightButton
          || event->button() == Qt::MiddleButton) {
        cancelPvInfoPickMode();
      }
    }
    if (event->button() == Qt::MiddleButton) {
      if (auto state = state_.lock()) {
        if (!state->editMode) {
          if (prepareExecuteChannelDrag(event->pos())) {
            event->accept();
            return;
          }
        } else if (state->createTool == CreateTool::kNone) {
          const bool control =
              event->modifiers().testFlag(Qt::ControlModifier);
          QWidget *hitWidget =
              elementAt(event->pos(), CompositeHitMode::kRejectChildren);
          if (control) {
            if (!multiSelection_.isEmpty()) {
              if (hitWidget && isWidgetInMultiSelection(hitWidget)) {
                beginMiddleButtonResize(event->pos());
                event->accept();
                return;
              }
              if (hitWidget && !isWidgetInMultiSelection(hitWidget)) {
                if (selectWidgetForEditing(hitWidget)) {
                  beginMiddleButtonResize(event->pos());
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
              beginMiddleButtonResize(event->pos());
              event->accept();
              return;
            }
            if (!selectedWidget && !hitWidget) {
              event->accept();
              return;
            }
          } else {
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
        if (beginVertexEdit(event->pos(), event->modifiers())) {
          event->accept();
          return;
        }
        const QPoint areaPos = displayArea_
            ? displayArea_->mapFrom(this, event->pos())
            : QPoint();
        const bool insideDisplayArea = displayArea_
            && displayArea_->rect().contains(areaPos);

        if (QWidget *widget =
            elementAt(event->pos(), CompositeHitMode::kRejectChildren)) {
          const Qt::KeyboardModifiers mods = event->modifiers();
          if (mods.testFlag(Qt::ShiftModifier)
              || mods.testFlag(Qt::ControlModifier)) {
            if (handleMultiSelectionClick(widget, mods)) {
              event->accept();
              return;
            }
          } else {
            if (selectWidgetForEditing(widget)) {
              event->accept();
              return;
            }
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
      if (auto state = state_.lock()) {
        const QPoint globalPos =
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            event->globalPosition().toPoint();
#else
            event->globalPos();
#endif
        lastContextMenuGlobalPos_ = globalPos;
        if (state->editMode) {
          showEditContextMenu(globalPos);
        } else {
          showExecuteContextMenu(globalPos);
        }
        event->accept();
        return;
      }
    }

    QMainWindow::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (executeDragPending_) {
      if (event->buttons() & Qt::MiddleButton) {
        updateExecuteDragTooltip(event->pos());
        if (!executeDragStarted_
            && event->modifiers().testFlag(Qt::ControlModifier)) {
          const QPoint delta =
              event->pos() - executeDragStartWindowPos_;
          if (delta.manhattanLength() >= QApplication::startDragDistance()) {
            executeDragStarted_ = true;
            startExecuteChannelDrag();
            event->accept();
            return;
          }
        }
        event->accept();
        return;
      }
      cancelExecuteChannelDrag();
    }
    if (middleButtonResizeActive_ && (event->buttons() & Qt::MiddleButton)) {
      updateMiddleButtonResize(event->pos());
      event->accept();
      return;
    }
    if (middleButtonDragActive_ && (event->buttons() & Qt::MiddleButton)) {
      updateMiddleButtonDrag(event->pos());
      event->accept();
      return;
    }
    if ((event->buttons() & Qt::LeftButton)
        && vertexEditMode_ != VertexEditMode::kNone) {
      updateVertexEdit(event->pos(), event->modifiers());
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
      if (executeDragPending_) {
        cancelExecuteChannelDrag();
        event->accept();
        return;
      }
      if (middleButtonResizeActive_) {
        finishMiddleButtonResize(true);
        event->accept();
        return;
      }
      if (middleButtonDragActive_) {
        finishMiddleButtonDrag(true);
        event->accept();
        return;
      }
    }
    if (event->button() == Qt::LeftButton) {
      if (vertexEditMode_ != VertexEditMode::kNone) {
        finishActiveVertexEdit(true);
        event->accept();
        return;
      }
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
  QString convertLegacyAdlFormat(const QString &adlText, int fileVersion) const;
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
  void refreshStackingOrder();
  bool isStaticGraphicWidget(const QWidget *widget) const;
  QColor colorForIndex(int index) const;
  QRect widgetDisplayRect(const QWidget *widget) const;
  void setWidgetDisplayRect(QWidget *widget, const QRect &displayRect) const;
  static QString applyMacroSubstitutions(const QString &input,
      const QHash<QString, QString> &macros);
  void writeWidgetAdl(QTextStream &stream, QWidget *widget, int indent,
      const std::function<QColor(const QWidget *, const QColor &)> &resolveForeground,
      const std::function<QColor(const QWidget *, const QColor &)> &resolveBackground) const;
  void writeAdlToStream(QTextStream &stream, const QString &fileNameHint) const;
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
  AdlNode applyPendingBasicAttribute(const AdlNode &node) const;
  AdlNode applyPendingDynamicAttribute(const AdlNode &node) const;
  void connectShellCommandElement(ShellCommandElement *element);
  void handleShellCommandActivation(ShellCommandElement *element,
      int entryIndex, Qt::KeyboardModifiers modifiers);
  bool buildShellCommandString(const QString &templateString,
      QString *result);
  bool promptForShellCommandInput(const QString &defaultCommand,
      QString *result);
  QStringList promptForShellCommandPvNames();
  QString shellCommandDisplayPath() const;
  QString shellCommandDisplayTitle() const;
  void runShellCommand(const QString &command);
  void connectRelatedDisplayElement(RelatedDisplayElement *element);
  void handleRelatedDisplayActivation(RelatedDisplayElement *element,
      int entryIndex, Qt::KeyboardModifiers modifiers);
  QString resolveRelatedDisplayFile(const QString &fileName) const;
  QStringList buildDisplaySearchPaths() const;
  QHash<QString, QString> parseMacroDefinitionString(const QString &macroString) const;
  void registerDisplayWindow(DisplayWindow *displayWin,
      bool delayExecuteMode = false);
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
  ResourcePaletteDialog *resourcePalette_ = nullptr;
  DisplayAreaWidget *displayArea_ = nullptr;
  QString filePath_;
  QString currentLoadDirectory_;
  QUndoStack *undoStack_ = nullptr;
  QByteArray lastCommittedState_;
  QString pendingUndoLabel_;
  bool suppressUndoCapture_ = false;
  bool restoringState_ = false;
  QByteArray cleanStateSnapshot_;
  QWidget *currentElementParent_ = nullptr;
  QPoint currentElementOffset_ = QPoint();
  bool suppressLoadRegistration_ = false;
  CompositeElement *currentCompositeOwner_ = nullptr;
  QString colormapName_;
  QHash<QString, QString> macroDefinitions_;
  std::optional<AdlNode> pendingBasicAttribute_;
  std::optional<AdlNode> pendingDynamicAttribute_;
  bool dirty_ = true;
  bool executeModeActive_ = false;
  bool displaySelected_ = false;
  bool gridOn_ = kDefaultGridOn;
  bool snapToGrid_ = kDefaultSnapToGrid;
  int gridSpacing_ = kDefaultGridSpacing;
  QPoint lastContextMenuGlobalPos_;
  struct ExecuteMenuEntry {
    QString label;
    QString command;
  };

  struct PvInfoChannelRef {
    QString name;
    chid channelId = nullptr;
  };

  struct PvInfoChannelDetails {
    QString name;
    QString desc;
    QString recordType;
    chtype fieldType = -1;
    unsigned long elementCount = 0;
    bool readAccess = false;
    bool writeAccess = false;
    QString host;
    QString value;
    bool hasValue = false;
    epicsTimeStamp timestamp{};
    bool hasTimestamp = false;
    short severity = 0;
    short status = 0;
    double hopr = 0.0;
    double lopr = 0.0;
    bool hasLimits = false;
    int precision = -1;
   bool hasPrecision = false;
   QStringList states;
   bool hasStates = false;
    QString error;
  };
  bool executeContextMenuInitialized_ = false;
  bool executeCascadeAvailable_ = false;
  QVector<ExecuteMenuEntry> executeMenuEntries_;
  QPointer<PvInfoDialog> pvInfoDialog_;
  bool pvInfoPickingActive_ = false;
  bool pvInfoCursorInitialized_ = false;
  bool pvInfoCursorActive_ = false;
  QCursor pvInfoCursor_;
  QList<TextElement *> textElements_;
  TextElement *selectedTextElement_ = nullptr;
  QHash<TextElement *, TextRuntime *> textRuntimes_;
  QList<TextEntryElement *> textEntryElements_;
  TextEntryElement *selectedTextEntryElement_ = nullptr;
  QHash<TextEntryElement *, TextEntryRuntime *> textEntryRuntimes_;
  QList<SliderElement *> sliderElements_;
  SliderElement *selectedSliderElement_ = nullptr;
  QHash<SliderElement *, SliderRuntime *> sliderRuntimes_;
  QList<WheelSwitchElement *> wheelSwitchElements_;
  WheelSwitchElement *selectedWheelSwitchElement_ = nullptr;
  QHash<WheelSwitchElement *, WheelSwitchRuntime *> wheelSwitchRuntimes_;
  QList<ChoiceButtonElement *> choiceButtonElements_;
  ChoiceButtonElement *selectedChoiceButtonElement_ = nullptr;
  QHash<ChoiceButtonElement *, ChoiceButtonRuntime *> choiceButtonRuntimes_;
  QList<MenuElement *> menuElements_;
  QHash<MenuElement *, MenuRuntime *> menuRuntimes_;
  MenuElement *selectedMenuElement_ = nullptr;
  QList<MessageButtonElement *> messageButtonElements_;
  QHash<MessageButtonElement *, MessageButtonRuntime *> messageButtonRuntimes_;
  MessageButtonElement *selectedMessageButtonElement_ = nullptr;
  QList<ShellCommandElement *> shellCommandElements_;
  ShellCommandElement *selectedShellCommandElement_ = nullptr;
  QList<RelatedDisplayElement *> relatedDisplayElements_;
  RelatedDisplayElement *selectedRelatedDisplayElement_ = nullptr;
  QList<TextMonitorElement *> textMonitorElements_;
  QHash<TextMonitorElement *, TextMonitorRuntime *> textMonitorRuntimes_;
  TextMonitorElement *selectedTextMonitorElement_ = nullptr;
  QList<MeterElement *> meterElements_;
  QHash<MeterElement *, MeterRuntime *> meterRuntimes_;
  MeterElement *selectedMeterElement_ = nullptr;
  QList<BarMonitorElement *> barMonitorElements_;
  QHash<BarMonitorElement *, BarMonitorRuntime *> barMonitorRuntimes_;
  BarMonitorElement *selectedBarMonitorElement_ = nullptr;
  QList<ScaleMonitorElement *> scaleMonitorElements_;
  QHash<ScaleMonitorElement *, ScaleMonitorRuntime *> scaleMonitorRuntimes_;
  ScaleMonitorElement *selectedScaleMonitorElement_ = nullptr;
  QList<StripChartElement *> stripChartElements_;
  QHash<StripChartElement *, StripChartRuntime *> stripChartRuntimes_;
  StripChartElement *selectedStripChartElement_ = nullptr;
  QList<CartesianPlotElement *> cartesianPlotElements_;
  QHash<CartesianPlotElement *, CartesianPlotRuntime *> cartesianPlotRuntimes_;
  CartesianPlotElement *selectedCartesianPlotElement_ = nullptr;
  QList<ByteMonitorElement *> byteMonitorElements_;
  QHash<ByteMonitorElement *, ByteMonitorRuntime *> byteMonitorRuntimes_;
  ByteMonitorElement *selectedByteMonitorElement_ = nullptr;
  QList<RectangleElement *> rectangleElements_;
  QHash<RectangleElement *, RectangleRuntime *> rectangleRuntimes_;
  RectangleElement *selectedRectangle_ = nullptr;
  QList<ImageElement *> imageElements_;
  QHash<ImageElement *, ImageRuntime *> imageRuntimes_;
  ImageElement *selectedImage_ = nullptr;
  QList<OvalElement *> ovalElements_;
  QHash<OvalElement *, OvalRuntime *> ovalRuntimes_;
  OvalElement *selectedOval_ = nullptr;
  QList<ArcElement *> arcElements_;
  QHash<ArcElement *, ArcRuntime *> arcRuntimes_;
  ArcElement *selectedArc_ = nullptr;
  QList<LineElement *> lineElements_;
  QHash<LineElement *, LineRuntime *> lineRuntimes_;
  LineElement *selectedLine_ = nullptr;
  QList<PolylineElement *> polylineElements_;
  QHash<PolylineElement *, PolylineRuntime *> polylineRuntimes_;
  QHash<PolygonElement *, PolygonRuntime *> polygonRuntimes_;
  PolylineElement *selectedPolyline_ = nullptr;
  QList<PolygonElement *> polygonElements_;
  PolygonElement *selectedPolygon_ = nullptr;
  QList<CompositeElement *> compositeElements_;
  QHash<CompositeElement *, CompositeRuntime *> compositeRuntimes_;
  CompositeElement *selectedCompositeElement_ = nullptr;
  bool polygonCreationActive_ = false;
  PolygonElement *activePolygonElement_ = nullptr;
  QVector<QPoint> polygonCreationPoints_;
  bool polylineCreationActive_ = false;
  PolylineElement *activePolylineElement_ = nullptr;
  QVector<QPoint> polylineCreationPoints_;
  QList<QPointer<QWidget>> multiSelection_;
  QList<QPointer<QWidget>> elementStack_;
  QSet<QWidget *> elementStackSet_;  /* Fast O(1) duplicate checking for elementStack_ */
  struct CompositeResizeChildInfo {
    QWidget *widget;
    QRect initialRect;
  };
  struct CompositeResizeInfo {
    QRect initialRect;
    QVector<CompositeResizeChildInfo> children;
  };
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
  QStringList executeDragChannels_;
  bool executeDragPending_ = false;
  bool executeDragStarted_ = false;
  QPoint executeDragStartWindowPos_;
  QString executeDragTooltipText_;
  bool executeDragTooltipVisible_ = false;
  QPointer<QLabel> executeDragTooltipLabel_;
  QList<QPointer<QWidget>> middleButtonResizeWidgets_;
  QList<QRect> middleButtonResizeInitialRects_;
  QPoint middleButtonResizeStartAreaPos_;
  bool middleButtonResizeActive_ = false;
  bool middleButtonResizeMoved_ = false;
  QHash<CompositeElement *, CompositeResizeInfo> middleButtonResizeCompositeInfo_;
  QSet<QWidget *> middleButtonResizeCompositeUpdated_;
  VertexEditMode vertexEditMode_ = VertexEditMode::kNone;
  PolygonElement *vertexEditPolygon_ = nullptr;
  PolylineElement *vertexEditPolyline_ = nullptr;
  int vertexEditIndex_ = -1;
  QVector<QPoint> vertexEditInitialPoints_;
  QVector<QPoint> vertexEditCurrentPoints_;
  bool vertexEditMoved_ = false;

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
    finishActiveVertexEditFor(selectedPolyline_, true);
    selectedPolyline_->setSelected(false);
    selectedPolyline_ = nullptr;
  }

  void clearPolygonSelection()
  {
    if (!selectedPolygon_) {
      return;
    }
    finishActiveVertexEditFor(selectedPolygon_, true);
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

  void detachSingleSelectionForWidget(QWidget *widget)
  {
    if (!widget) {
      return;
    }
    if (auto *text = dynamic_cast<TextElement *>(widget)) {
      if (selectedTextElement_ == text) {
        selectedTextElement_ = nullptr;
      }
      return;
    }
    if (auto *textEntry = dynamic_cast<TextEntryElement *>(widget)) {
      if (selectedTextEntryElement_ == textEntry) {
        selectedTextEntryElement_ = nullptr;
      }
      return;
    }
    if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      if (selectedSliderElement_ == slider) {
        selectedSliderElement_ = nullptr;
      }
      return;
    }
    if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
      if (selectedWheelSwitchElement_ == wheel) {
        selectedWheelSwitchElement_ = nullptr;
      }
      return;
    }
    if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
      if (selectedChoiceButtonElement_ == choice) {
        selectedChoiceButtonElement_ = nullptr;
      }
      return;
    }
    if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
      if (selectedMenuElement_ == menu) {
        selectedMenuElement_ = nullptr;
      }
      return;
    }
    if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
      if (selectedMessageButtonElement_ == message) {
        selectedMessageButtonElement_ = nullptr;
      }
      return;
    }
    if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
      if (selectedShellCommandElement_ == shell) {
        selectedShellCommandElement_ = nullptr;
      }
      return;
    }
    if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
      if (selectedRelatedDisplayElement_ == related) {
        selectedRelatedDisplayElement_ = nullptr;
      }
      return;
    }
    if (auto *textMonitor = dynamic_cast<TextMonitorElement *>(widget)) {
      if (selectedTextMonitorElement_ == textMonitor) {
        selectedTextMonitorElement_ = nullptr;
      }
      return;
    }
    if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
      if (selectedMeterElement_ == meter) {
        selectedMeterElement_ = nullptr;
      }
      return;
    }
    if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
      if (selectedBarMonitorElement_ == bar) {
        selectedBarMonitorElement_ = nullptr;
      }
      return;
    }
    if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
      if (selectedScaleMonitorElement_ == scale) {
        selectedScaleMonitorElement_ = nullptr;
      }
      return;
    }
    if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
      if (selectedStripChartElement_ == strip) {
        selectedStripChartElement_ = nullptr;
      }
      return;
    }
    if (auto *cart = dynamic_cast<CartesianPlotElement *>(widget)) {
      if (selectedCartesianPlotElement_ == cart) {
        selectedCartesianPlotElement_ = nullptr;
      }
      return;
    }
    if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
      if (selectedByteMonitorElement_ == byte) {
        selectedByteMonitorElement_ = nullptr;
      }
      return;
    }
    if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      if (selectedRectangle_ == rectangle) {
        selectedRectangle_ = nullptr;
      }
      return;
    }
    if (auto *image = dynamic_cast<ImageElement *>(widget)) {
      if (selectedImage_ == image) {
        selectedImage_ = nullptr;
      }
      return;
    }
    if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      if (selectedOval_ == oval) {
        selectedOval_ = nullptr;
      }
      return;
    }
    if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      if (selectedArc_ == arc) {
        selectedArc_ = nullptr;
      }
      return;
    }
    if (auto *line = dynamic_cast<LineElement *>(widget)) {
      if (selectedLine_ == line) {
        selectedLine_ = nullptr;
      }
      return;
    }
    if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      if (selectedPolyline_ == polyline) {
        selectedPolyline_ = nullptr;
      }
      return;
    }
    if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      if (selectedPolygon_ == polygon) {
        selectedPolygon_ = nullptr;
      }
      return;
    }
    if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
      if (selectedCompositeElement_ == composite) {
        selectedCompositeElement_ = nullptr;
      }
      return;
    }
  }

  void addWidgetToMultiSelection(QWidget *widget)
  {
    if (!widget) {
      return;
    }
    if (isWidgetInMultiSelection(widget)) {
      setWidgetSelectionState(widget, true);
      detachSingleSelectionForWidget(widget);
      return;
    }
    detachSingleSelectionForWidget(widget);
    setWidgetSelectionState(widget, true);
    multiSelection_.append(QPointer<QWidget>(widget));
  }

  void removeWidgetFromMultiSelection(QWidget *widget)
  {
    if (!widget) {
      return;
    }
    for (auto it = multiSelection_.begin(); it != multiSelection_.end();) {
      QWidget *current = it->data();
      if (!current || current == widget) {
        it = multiSelection_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void removeWidgetFromSelection(QWidget *widget)
  {
    if (!widget) {
      return;
    }
    removeWidgetFromMultiSelection(widget);
    if (auto *text = dynamic_cast<TextElement *>(widget)) {
      if (selectedTextElement_ == text) {
        clearTextSelection();
        return;
      }
    }
    if (auto *textEntry = dynamic_cast<TextEntryElement *>(widget)) {
      if (selectedTextEntryElement_ == textEntry) {
        clearTextEntrySelection();
        return;
      }
    }
    if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      if (selectedSliderElement_ == slider) {
        clearSliderSelection();
        return;
      }
    }
    if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
      if (selectedWheelSwitchElement_ == wheel) {
        clearWheelSwitchSelection();
        return;
      }
    }
    if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
      if (selectedChoiceButtonElement_ == choice) {
        clearChoiceButtonSelection();
        return;
      }
    }
    if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
      if (selectedMenuElement_ == menu) {
        clearMenuSelection();
        return;
      }
    }
    if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
      if (selectedMessageButtonElement_ == message) {
        clearMessageButtonSelection();
        return;
      }
    }
    if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
      if (selectedShellCommandElement_ == shell) {
        clearShellCommandSelection();
        return;
      }
    }
    if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
      if (selectedRelatedDisplayElement_ == related) {
        clearRelatedDisplaySelection();
        return;
      }
    }
    if (auto *textMonitor = dynamic_cast<TextMonitorElement *>(widget)) {
      if (selectedTextMonitorElement_ == textMonitor) {
        clearTextMonitorSelection();
        return;
      }
    }
    if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
      if (selectedMeterElement_ == meter) {
        clearMeterSelection();
        return;
      }
    }
    if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
      if (selectedBarMonitorElement_ == bar) {
        clearBarMonitorSelection();
        return;
      }
    }
    if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
      if (selectedScaleMonitorElement_ == scale) {
        clearScaleMonitorSelection();
        return;
      }
    }
    if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
      if (selectedStripChartElement_ == strip) {
        clearStripChartSelection();
        return;
      }
    }
    if (auto *cart = dynamic_cast<CartesianPlotElement *>(widget)) {
      if (selectedCartesianPlotElement_ == cart) {
        clearCartesianPlotSelection();
        return;
      }
    }
    if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
      if (selectedByteMonitorElement_ == byte) {
        clearByteMonitorSelection();
        return;
      }
    }
    if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      if (selectedRectangle_ == rectangle) {
        clearRectangleSelection();
        return;
      }
    }
    if (auto *image = dynamic_cast<ImageElement *>(widget)) {
      if (selectedImage_ == image) {
        clearImageSelection();
        return;
      }
    }
    if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      if (selectedOval_ == oval) {
        clearOvalSelection();
        return;
      }
    }
    if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      if (selectedArc_ == arc) {
        clearArcSelection();
        return;
      }
    }
    if (auto *line = dynamic_cast<LineElement *>(widget)) {
      if (selectedLine_ == line) {
        clearLineSelection();
        return;
      }
    }
    if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      if (selectedPolyline_ == polyline) {
        clearPolylineSelection();
        return;
      }
    }
    if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      if (selectedPolygon_ == polygon) {
        clearPolygonSelection();
        return;
      }
    }
    if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
      if (selectedCompositeElement_ == composite) {
        clearCompositeSelection();
        return;
      }
    }
    setWidgetSelectionState(widget, false);
  }

  void pruneMultiSelection()
  {
    for (auto it = multiSelection_.begin(); it != multiSelection_.end();) {
      if (it->isNull()) {
        it = multiSelection_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void updateSelectionAfterMultiChange()
  {
    pruneMultiSelection();
    QList<QWidget *> widgets;
    for (const auto &pointer : multiSelection_) {
      QWidget *widget = pointer.data();
      if (widget) {
        widgets.append(widget);
      }
    }
    if (widgets.isEmpty()) {
      if (!currentSelectedWidget() && !displaySelected_) {
        closeResourcePalette();
      }
      notifyMenus();
      return;
    }
    if (widgets.size() == 1) {
      QWidget *single = widgets.front();
      multiSelection_.clear();
      setWidgetSelectionState(single, true);
      selectWidgetForEditing(single);
      notifyMenus();
      return;
    }
    showResourcePaletteForMultipleSelection();
    notifyMenus();
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

  QList<QWidget *> alignableWidgets() const
  {
    QList<QWidget *> widgets;
    for (QWidget *widget : selectedWidgets()) {
      if (!widget || widget == displayArea_) {
        continue;
      }
      widgets.append(widget);
    }
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
    notifyMenus();
  }

  void removeTextRuntime(TextElement *element);
  void removeSliderRuntime(SliderElement *element);
  void removeChoiceButtonRuntime(ChoiceButtonElement *element);
  void removeMenuRuntime(MenuElement *element);
  void removeMessageButtonRuntime(MessageButtonElement *element);
  void removeArcRuntime(ArcElement *element);
  void removeOvalRuntime(OvalElement *element);
  void removeLineRuntime(LineElement *element);
  void removeRectangleRuntime(RectangleElement *element);
  void removeImageRuntime(ImageElement *element);
  void removePolylineRuntime(PolylineElement *element);
  void removePolygonRuntime(PolygonElement *element);
  void removeMeterRuntime(MeterElement *element);
  void removeBarMonitorRuntime(BarMonitorElement *element);
  void removeScaleMonitorRuntime(ScaleMonitorElement *element);
  void removeStripChartRuntime(StripChartElement *element);
  void removeCartesianPlotRuntime(CartesianPlotElement *element);
  void removeByteMonitorRuntime(ByteMonitorElement *element);
  void removeWheelSwitchRuntime(WheelSwitchElement *element);
  void removeTextEntryRuntime(TextEntryElement *element);

  template <typename ElementType>
  bool cutSelectedElement(QList<ElementType *> &elements,
      ElementType *&selected)
  {
    if (!selected) {
      return false;
    }

    ElementType *element = selected;
    finishActiveVertexEditFor(element, true);
    selected = nullptr;
    element->setSelected(false);
    elements.removeAll(element);
    removeElementFromStack(element);
    if constexpr (std::is_same_v<ElementType, TextElement>) {
      removeTextRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, TextEntryElement>) {
      removeTextEntryRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, SliderElement>) {
      removeSliderRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, WheelSwitchElement>) {
      removeWheelSwitchRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, MeterElement>) {
      removeMeterRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, BarMonitorElement>) {
      removeBarMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ScaleMonitorElement>) {
      removeScaleMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, StripChartElement>) {
      removeStripChartRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, CartesianPlotElement>) {
      removeCartesianPlotRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ByteMonitorElement>) {
      removeByteMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ChoiceButtonElement>) {
      removeChoiceButtonRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, MenuElement>) {
      removeMenuRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, MessageButtonElement>) {
      removeMessageButtonRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, LineElement>) {
      removeLineRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, RectangleElement>) {
      removeRectangleRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ArcElement>) {
      removeArcRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, OvalElement>) {
      removeOvalRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, PolylineElement>) {
      removePolylineRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, PolygonElement>) {
      removePolygonRuntime(element);
    }
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
        QRect desired = target.translateRectForPaste(geometry, offset);
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
        rect = target.snapRectOriginToGrid(rect);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.textRuntimes_.contains(newElement)) {
            auto *runtime = new TextRuntime(newElement);
            target.textRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
      const double increment = element->increment();
      const PvLimits limits = element->limits();
      const QString channel = element->channel();
      prepareClipboard([geometry, foreground, background, colorMode, label,
                           direction, increment, limits, channel](
                           DisplayWindow &target, const QPoint &offset) {
        if (!target.displayArea_) {
          return;
        }
        QRect rect = target.translateRectForPaste(geometry, offset);
        auto *newElement = new SliderElement(target.displayArea_);
        newElement->setGeometry(rect);
        newElement->setForegroundColor(foreground);
        newElement->setBackgroundColor(background);
        newElement->setColorMode(colorMode);
        newElement->setLabel(label);
        newElement->setDirection(direction);
        newElement->setIncrement(increment);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        target.connectShellCommandElement(newElement);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
      target.connectRelatedDisplayElement(newElement);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.rectangleRuntimes_.contains(newElement)) {
            auto *runtime = new RectangleRuntime(newElement);
            target.rectangleRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.imageRuntimes_.contains(newElement)) {
            auto *runtime = new ImageRuntime(newElement);
            target.imageRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.ovalRuntimes_.contains(newElement)) {
            auto *runtime = new OvalRuntime(newElement);
            target.ovalRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.arcRuntimes_.contains(newElement)) {
            auto *runtime = new ArcRuntime(newElement);
            target.arcRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QRect rect = target.translateRectForPaste(geometry, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.lineRuntimes_.contains(newElement)) {
            auto *runtime = new LineRuntime(newElement);
            target.lineRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QVector<QPoint> translated =
            target.translatePointsForPaste(points, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.polylineRuntimes_.contains(newElement)) {
            auto *runtime = new PolylineRuntime(newElement);
            target.polylineRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
        QVector<QPoint> translated =
            target.translatePointsForPaste(points, offset);
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
        if (target.executeModeActive_) {
          newElement->setExecuteMode(true);
          if (!target.polygonRuntimes_.contains(newElement)) {
            auto *runtime = new PolygonRuntime(newElement);
            target.polygonRuntimes_.insert(newElement, runtime);
            runtime->start();
          }
        }
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
    if (resourcePalette_ && resourcePalette_->isVisible()) {
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
    if (!resourcePalette_) {
      resourcePalette_ = new ResourcePaletteDialog(
          resourcePaletteBase_, labelFont_, font(), this);
      QObject::connect(resourcePalette_, &QDialog::finished, this,
          [this](int) {
            handleResourcePaletteClosed();
          });
      QObject::connect(resourcePalette_, &QObject::destroyed, this,
          [this]() {
            resourcePalette_ = nullptr;
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
        },
        [this]() {
          return isSnapToGridEnabled();
        },
        [this](bool snap) {
          setSnapToGrid(snap);
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
          return element->increment();
        },
        [this, element](double increment) {
          element->setIncrement(increment);
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

  enum class CompositeHitMode
  {
    kAuto,
    kRejectChildren,
    kIncludeChildren,
  };

  QWidget *elementAt(const QPoint &windowPos,
      CompositeHitMode mode = CompositeHitMode::kAuto) const
  {
    if (!displayArea_) {
      return nullptr;
    }
    const QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    if (!displayArea_->rect().contains(areaPos)) {
      return nullptr;
    }
    bool includeCompositeChildren = true;
    switch (mode) {
    case CompositeHitMode::kRejectChildren:
      includeCompositeChildren = false;
      break;
    case CompositeHitMode::kIncludeChildren:
      includeCompositeChildren = true;
      break;
    case CompositeHitMode::kAuto:
    default:
      includeCompositeChildren = executeModeActive_;
      break;
    }
    auto hitTest = [&](QWidget *candidate, const auto &self) -> QWidget * {
      if (!candidate) {
        return nullptr;
      }

      const QPoint topLeft = candidate->mapTo(displayArea_, QPoint(0, 0));
      const QRect globalRect(topLeft, candidate->size());
      if (!globalRect.contains(areaPos)) {
        return nullptr;
      }

      if (includeCompositeChildren) {
        if (auto *composite = dynamic_cast<CompositeElement *>(candidate)) {
          const QList<QWidget *> children = composite->childWidgets();
          for (auto it = children.crbegin(); it != children.crend(); ++it) {
            QWidget *child = *it;
            if (!child) {
              continue;
            }
            if (QWidget *hit = self(child, self)) {
              return hit;
            }
          }
          if (executeModeActive_) {
            const bool compositeHasChannels =
                !channelsForWidget(composite).isEmpty();
            const bool compositeHasDynamicVisibility =
                composite->visibilityMode() != TextVisibilityMode::kStatic
                || !composite->visibilityCalc().isEmpty();
            const bool compositeHasDynamicColor =
                composite->colorMode() != TextColorMode::kStatic;
            if (!compositeHasChannels
                && !compositeHasDynamicVisibility
                && !compositeHasDynamicColor) {
              return nullptr;
            }
          }
          return candidate;
        }
      }

      if (auto *polyline = dynamic_cast<PolylineElement *>(candidate)) {
        if (!polyline->containsGlobalPoint(areaPos)) {
          return nullptr;
        }
      } else if (auto *polygon = dynamic_cast<PolygonElement *>(candidate)) {
        if (!polygon->containsGlobalPoint(areaPos)) {
          return nullptr;
        }
      }

      /* Skip purely decorative graphic elements (static rectangles, ovals,
       * etc. with no dynamic channels) to allow hit testing to pass through
       * to underlying widgets with EPICS channels in Execute mode. In Edit
       * mode, all widgets must be selectable. */
      if (executeModeActive_
          && candidate->testAttribute(Qt::WA_TransparentForMouseEvents)) {
        if (auto *rect = dynamic_cast<RectangleElement *>(candidate)) {
          if (rect->colorMode() == TextColorMode::kStatic
              && rect->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *oval = dynamic_cast<OvalElement *>(candidate)) {
          if (oval->colorMode() == TextColorMode::kStatic
              && oval->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *arc = dynamic_cast<ArcElement *>(candidate)) {
          if (arc->colorMode() == TextColorMode::kStatic
              && arc->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *line = dynamic_cast<LineElement *>(candidate)) {
          if (line->colorMode() == TextColorMode::kStatic
              && line->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *polyline = dynamic_cast<PolylineElement *>(candidate)) {
          if (polyline->colorMode() == TextColorMode::kStatic
              && polyline->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *polygon = dynamic_cast<PolygonElement *>(candidate)) {
          if (polygon->colorMode() == TextColorMode::kStatic
              && polygon->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *image = dynamic_cast<ImageElement *>(candidate)) {
          if (image->colorMode() == TextColorMode::kStatic
              && image->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        } else if (auto *text = dynamic_cast<TextElement *>(candidate)) {
          if (text->colorMode() == TextColorMode::kStatic
              && text->visibilityMode() == TextVisibilityMode::kStatic) {
            return nullptr;
          }
        }
      }
      return candidate;
    };

    for (auto it = elementStack_.crbegin(); it != elementStack_.crend(); ++it) {
      QWidget *widget = it->data();
      if (QWidget *hit = hitTest(widget, hitTest)) {
        return hit;
      }
    }
    return nullptr;
  }

  QStringList channelsForWidget(QWidget *widget) const
  {
    QStringList channels;
    if (!widget) {
      return channels;
    }

    auto appendChannel = [&](const QString &channel) {
      const QString trimmed = channel.trimmed();
      if (trimmed.isEmpty()) {
        return;
      }
      if (!channels.contains(trimmed)) {
        channels.append(trimmed);
      }
    };

    auto appendChannelArray = [&](const auto &array) {
      for (const QString &value : array) {
        appendChannel(value);
      }
    };

    if (auto *element = dynamic_cast<TextElement *>(widget)) {
      if (element->colorMode() != TextColorMode::kStatic
          || element->visibilityMode() != TextVisibilityMode::kStatic) {
        appendChannelArray(AdlWriter::collectChannels(element));
      }
    } else if (auto *element = dynamic_cast<TextMonitorElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<TextEntryElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<SliderElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<WheelSwitchElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<ChoiceButtonElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<MenuElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<MessageButtonElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<MeterElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<BarMonitorElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<ScaleMonitorElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<ByteMonitorElement *>(widget)) {
      appendChannel(element->channel());
    } else if (auto *element = dynamic_cast<RectangleElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<ImageElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<OvalElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<ArcElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<LineElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<PolylineElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<PolygonElement *>(widget)) {
      appendChannelArray(AdlWriter::collectChannels(element));
    } else if (auto *element = dynamic_cast<CompositeElement *>(widget)) {
      appendChannelArray(element->channels());
    } else if (auto *element = dynamic_cast<StripChartElement *>(widget)) {
      const int penCount = element->penCount();
      for (int i = 0; i < penCount; ++i) {
        appendChannel(element->channel(i));
      }
    } else if (auto *element = dynamic_cast<CartesianPlotElement *>(widget)) {
      appendChannel(element->triggerChannel());
      appendChannel(element->eraseChannel());
      appendChannel(element->countChannel());
      const int traceCount = element->traceCount();
      for (int i = 0; i < traceCount; ++i) {
        appendChannel(element->traceXChannel(i));
        appendChannel(element->traceYChannel(i));
      }
    }

    return channels;
  }

  void startPvInfoPickMode()
  {
    if (!executeModeActive_) {
      return;
    }
    if (pvInfoPickingActive_) {
      cancelPvInfoPickMode();
    }
    if (!pvInfoCursorInitialized_) {
      pvInfoCursor_ = createPvInfoCursor();
      pvInfoCursorInitialized_ = true;
    }
    QGuiApplication::setOverrideCursor(pvInfoCursor_);
    pvInfoCursorActive_ = true;
    pvInfoPickingActive_ = true;
  }

  void cancelPvInfoPickMode()
  {
    if (!pvInfoPickingActive_) {
      return;
    }
    pvInfoPickingActive_ = false;
    if (pvInfoCursorActive_) {
      QGuiApplication::restoreOverrideCursor();
      pvInfoCursorActive_ = false;
    }
  }

  void completePvInfoPick(const QPoint &windowPos)
  {
    if (!pvInfoPickingActive_) {
      return;
    }
    QWidget *widget = elementAt(windowPos);
    const QPoint globalPos = mapToGlobal(windowPos);
    lastContextMenuGlobalPos_ = globalPos;

    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitialized();

    QString content;
    if (!context.isInitialized()) {
      content = QStringLiteral(
          "           PV Information\n\nChannel Access is not available.\n");
    } else if (widget) {
      content = buildPvInfoText(widget);
    } else {
      content = buildPvInfoBackgroundText();
    }

    cancelPvInfoPickMode();
    showPvInfoContent(content);
  }

  void showPvInfoContent(const QString &text)
  {
    PvInfoDialog *dialog = ensurePvInfoDialog();
    if (!dialog) {
      return;
    }
    dialog->setContent(text);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
  }

  QCursor createPvInfoCursor() const
  {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPoint(6, 16), 4, 4);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(12, 4, 18, 24),
        Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("PV"));
    painter.end();

    return QCursor(pixmap, 4, 16);
  }

  QString buildPvInfoBackgroundText() const
  {
    QString result;
    QTextStream stream(&result);
    setUtf8Encoding(stream);

    const QString timestamp =
        QDateTime::currentDateTime().toString(
            QStringLiteral("ddd MMM dd, yyyy HH:mm:ss"));

    stream << "           PV Information\n\n";
    stream << "Object: Display Background\n";
    stream << timestamp << "\n\n";
    stream << "No process variables are associated with this location.\n\n";

    return result;
  }

  QString pvInfoElementLabel(QWidget *widget) const
  {
    if (!widget) {
      return QStringLiteral("Unknown");
    }
    if (dynamic_cast<TextElement *>(widget)) {
      return QStringLiteral("Text");
    }
    if (dynamic_cast<TextMonitorElement *>(widget)) {
      return QStringLiteral("Text Monitor");
    }
    if (dynamic_cast<TextEntryElement *>(widget)) {
      return QStringLiteral("Text Entry");
    }
    if (dynamic_cast<SliderElement *>(widget)) {
      return QStringLiteral("Slider");
    }
    if (dynamic_cast<WheelSwitchElement *>(widget)) {
      return QStringLiteral("Wheel Switch");
    }
    if (dynamic_cast<ChoiceButtonElement *>(widget)) {
      return QStringLiteral("Choice Button");
    }
    if (dynamic_cast<MenuElement *>(widget)) {
      return QStringLiteral("Menu");
    }
    if (dynamic_cast<MessageButtonElement *>(widget)) {
      return QStringLiteral("Message Button");
    }
    if (dynamic_cast<ShellCommandElement *>(widget)) {
      return QStringLiteral("Shell Command");
    }
    if (dynamic_cast<RelatedDisplayElement *>(widget)) {
      return QStringLiteral("Related Display");
    }
    if (dynamic_cast<MeterElement *>(widget)) {
      return QStringLiteral("Meter");
    }
    if (dynamic_cast<BarMonitorElement *>(widget)) {
      return QStringLiteral("Bar Monitor");
    }
    if (dynamic_cast<ScaleMonitorElement *>(widget)) {
      return QStringLiteral("Scale Monitor");
    }
    if (dynamic_cast<ByteMonitorElement *>(widget)) {
      return QStringLiteral("Byte Monitor");
    }
    if (dynamic_cast<StripChartElement *>(widget)) {
      return QStringLiteral("Strip Chart");
    }
    if (dynamic_cast<CartesianPlotElement *>(widget)) {
      return QStringLiteral("Cartesian Plot");
    }
    if (dynamic_cast<RectangleElement *>(widget)) {
      return QStringLiteral("Rectangle");
    }
    if (dynamic_cast<ImageElement *>(widget)) {
      return QStringLiteral("Image");
    }
    if (dynamic_cast<OvalElement *>(widget)) {
      return QStringLiteral("Oval");
    }
    if (dynamic_cast<ArcElement *>(widget)) {
      return QStringLiteral("Arc");
    }
    if (dynamic_cast<LineElement *>(widget)) {
      return QStringLiteral("Line");
    }
    if (dynamic_cast<PolylineElement *>(widget)) {
      return QStringLiteral("Polyline");
    }
    if (dynamic_cast<PolygonElement *>(widget)) {
      return QStringLiteral("Polygon");
    }
    if (dynamic_cast<CompositeElement *>(widget)) {
      return QStringLiteral("Composite");
    }
    return QStringLiteral("Unknown");
  }

  QVector<PvInfoChannelRef> gatherPvInfoChannels(QWidget *widget) const
  {
    QVector<PvInfoChannelRef> refs;
    if (!widget) {
      return refs;
    }

    auto addRef = [&](const QString &channel, chid channelId) {
      const QString trimmed = channel.trimmed();
      if (trimmed.isEmpty()) {
        return;
      }
      PvInfoChannelRef ref;
      ref.name = trimmed;
      ref.channelId = channelId;
      refs.append(ref);
    };

    if (auto *element = dynamic_cast<TextElement *>(widget)) {
      if (auto *runtime = textRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<TextMonitorElement *>(widget)) {
      if (auto *runtime = textMonitorRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<TextEntryElement *>(widget)) {
      if (auto *runtime = textEntryRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<SliderElement *>(widget)) {
      if (auto *runtime = sliderRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<WheelSwitchElement *>(widget)) {
      if (auto *runtime = wheelSwitchRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<ChoiceButtonElement *>(widget)) {
      if (auto *runtime = choiceButtonRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<MenuElement *>(widget)) {
      if (auto *runtime = menuRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<MessageButtonElement *>(widget)) {
      if (auto *runtime = messageButtonRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<MeterElement *>(widget)) {
      if (auto *runtime = meterRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<BarMonitorElement *>(widget)) {
      if (auto *runtime = barMonitorRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<ScaleMonitorElement *>(widget)) {
      if (auto *runtime = scaleMonitorRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<ByteMonitorElement *>(widget)) {
      if (auto *runtime = byteMonitorRuntimes_.value(element, nullptr)) {
        addRef(runtime->channelName_, runtime->channelId_);
      } else {
        addRef(element->channel(), nullptr);
      }
    } else if (auto *element = dynamic_cast<RectangleElement *>(widget)) {
      if (auto *runtime = rectangleRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<ImageElement *>(widget)) {
      if (auto *runtime = imageRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<OvalElement *>(widget)) {
      if (auto *runtime = ovalRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<ArcElement *>(widget)) {
      if (auto *runtime = arcRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<LineElement *>(widget)) {
      if (auto *runtime = lineRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<PolylineElement *>(widget)) {
      if (auto *runtime = polylineRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<PolygonElement *>(widget)) {
      if (auto *runtime = polygonRuntimes_.value(element, nullptr)) {
        for (const auto &channel : runtime->channels_) {
          addRef(channel.name, channel.channelId);
        }
      } else {
        const auto rawChannels = AdlWriter::collectChannels(element);
        for (const QString &channel : rawChannels) {
          addRef(channel, nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<CompositeElement *>(widget)) {
      const auto rawChannels = element->channels();
      for (const QString &channel : rawChannels) {
        addRef(channel, nullptr);
      }
    } else if (auto *element = dynamic_cast<StripChartElement *>(widget)) {
      if (auto *runtime = stripChartRuntimes_.value(element, nullptr)) {
        for (const auto &pen : runtime->pens_) {
          addRef(pen.channelName, pen.channelId);
        }
      } else {
        const int penCount = element->penCount();
        for (int i = 0; i < penCount; ++i) {
          addRef(element->channel(i), nullptr);
        }
      }
    } else if (auto *element = dynamic_cast<CartesianPlotElement *>(widget)) {
      if (auto *runtime = cartesianPlotRuntimes_.value(element, nullptr)) {
        for (const auto &trace : runtime->traces_) {
          addRef(trace.x.name, trace.x.channelId);
          addRef(trace.y.name, trace.y.channelId);
        }
        addRef(runtime->triggerChannel_.name, runtime->triggerChannel_.channelId);
        addRef(runtime->eraseChannel_.name, runtime->eraseChannel_.channelId);
        addRef(runtime->countChannel_.name, runtime->countChannel_.channelId);
      } else {
        addRef(element->triggerChannel(), nullptr);
        addRef(element->eraseChannel(), nullptr);
        addRef(element->countChannel(), nullptr);
        const int traceCount = element->traceCount();
        for (int i = 0; i < traceCount; ++i) {
          addRef(element->traceXChannel(i), nullptr);
          addRef(element->traceYChannel(i), nullptr);
        }
      }
    }

    return refs;
  }

  QString buildPvInfoText(QWidget *widget) const
  {
    QString result;
    QTextStream stream(&result);
    setUtf8Encoding(stream);

    const QString objectLabel = pvInfoElementLabel(widget);
    const QString timestamp =
        QDateTime::currentDateTime().toString(
            QStringLiteral("ddd MMM dd, yyyy HH:mm:ss"));

    stream << "           PV Information\n\n";
    stream << "Object: " << objectLabel << '\n';
    stream << timestamp << "\n\n";

    const QVector<PvInfoChannelRef> refs = gatherPvInfoChannels(widget);
    QSet<QString> seen;
    bool addedSection = false;

    for (const auto &ref : refs) {
      const QString channel = ref.name;
      if (channel.isEmpty() || seen.contains(channel)) {
        continue;
      }
      seen.insert(channel);
      PvInfoChannelDetails details;
      if (populatePvInfoDetails(channel, ref.channelId, details)) {
        stream << formatPvInfoSection(details);
      } else {
        stream << channel << '\n';
        stream << "======================================\n";
        if (!details.error.isEmpty()) {
          stream << "Error: " << details.error << '\n';
        } else {
          stream << "Error: Unable to retrieve channel data.\n";
        }
        stream << '\n';
      }
      addedSection = true;
    }

    if (!addedSection) {
      stream << "No process variables are associated with this object.\n\n";
    }

    return result;
  }

  QString formatPvInfoSection(const PvInfoChannelDetails &details) const
  {
    QString result;
    QTextStream stream(&result);
    setUtf8Encoding(stream);

    const char *typeName = nullptr;
    if (details.fieldType >= 0) {
      typeName = dbf_type_to_text(details.fieldType);
    }

    QString access;
    if (details.readAccess) {
      access.append(QLatin1Char('R'));
    }
    if (details.writeAccess) {
      access.append(QLatin1Char('W'));
    }

    const QString descValue = details.desc.isEmpty()
        ? QStringLiteral("Not Available")
        : details.desc;
    const QString rtypValue = details.recordType.isEmpty()
        ? QStringLiteral("Not Available")
        : details.recordType;

    stream << details.name << '\n';
    stream << "======================================\n";
    stream << "DESC: " << descValue << '\n';
    stream << "RTYP: " << rtypValue << '\n';
    stream << "TYPE: "
           << (typeName ? QString::fromLatin1(typeName)
                        : QStringLiteral("Unknown"))
           << '\n';
    stream << "COUNT: " << details.elementCount << '\n';
    stream << "ACCESS: " << access << '\n';
    stream << "HOST: "
           << (details.host.isEmpty() ? QStringLiteral("Unknown")
                                      : details.host)
           << '\n';

    const QString valueLabel = (details.elementCount > 1)
        ? QStringLiteral("FIRST VALUE")
        : QStringLiteral("VALUE");
    const QString valueText = details.hasValue
        ? details.value
        : (details.value.isEmpty() ? QStringLiteral("Not Available")
                                   : details.value);
    stream << valueLabel << ": " << valueText << '\n';

    if (details.hasTimestamp) {
      stream << "STAMP: " << formatPvInfoTimestamp(details.timestamp) << '\n';
    } else {
      stream << "STAMP: Not Available\n";
    }

    stream << "ALARM: " << alarmSeverityString(details.severity) << '\n';

    auto formatDouble = [](double value) {
      return QString::number(value, 'g', 12);
    };

    if (details.fieldType == DBF_ENUM && details.hasStates) {
      stream << '\n';
      stream << "STATES: " << details.states.size() << '\n';
      for (int i = 0; i < details.states.size(); ++i) {
        QString stateLine = QStringLiteral("STATE %1: %2")
            .arg(i, 2, 10, QLatin1Char(' '))
            .arg(details.states.at(i));
        stream << stateLine << '\n';
      }
      stream << '\n';
    } else if ((details.fieldType == DBF_CHAR ||
                details.fieldType == DBF_SHORT ||
                details.fieldType == DBF_LONG) && details.hasLimits) {
      stream << '\n';
      stream << "HOPR: " << formatDouble(details.hopr)
             << "  LOPR: " << formatDouble(details.lopr) << '\n';
      stream << '\n';
    } else if ((details.fieldType == DBF_FLOAT ||
                details.fieldType == DBF_DOUBLE) && details.hasLimits) {
      stream << '\n';
      if (details.hasPrecision) {
        stream << "PRECISION: " << details.precision << '\n';
      } else {
        stream << "PRECISION: Not Available\n";
      }
      stream << "HOPR: " << formatDouble(details.hopr)
             << "  LOPR: " << formatDouble(details.lopr) << '\n';
      stream << '\n';
    } else {
      stream << '\n';
    }

    return result;
  }

  QString formatPvInfoTimestamp(const epicsTimeStamp &stamp) const
  {
    const qint64 seconds = static_cast<qint64>(stamp.secPastEpoch)
        + kEpicsEpochOffsetSeconds;
    const qint64 msecs = seconds * 1000
        + static_cast<qint64>(stamp.nsec) / 1000000;
    const int fractional =
        static_cast<int>((stamp.nsec % 1000000000) / 1000000);
    QDateTime dateTime =
        QDateTime::fromMSecsSinceEpoch(msecs, Qt::LocalTime);
    const QString base =
        dateTime.toString(QStringLiteral("ddd MMM dd, yyyy HH:mm:ss"));
    const QString fraction =
        QStringLiteral("%1").arg(fractional, 3, 10, QLatin1Char('0'));
    return QStringLiteral("%1.%2").arg(base, fraction);
  }

  QString alarmSeverityString(short severity) const
  {
    switch (severity) {
    case NO_ALARM:
      return QStringLiteral("NO");
    case MINOR_ALARM:
      return QStringLiteral("MINOR");
    case MAJOR_ALARM:
      return QStringLiteral("MAJOR");
    case INVALID_ALARM:
      return QStringLiteral("INVALID");
    default:
      return QStringLiteral("Unknown (%1)").arg(severity);
    }
  }

  QString pvInfoRelatedFieldName(const QString &channelName,
      const QString &fieldSuffix) const
  {
    const int dotIndex = channelName.indexOf(QLatin1Char('.'));
    if (dotIndex >= 0) {
      return channelName.left(dotIndex) + fieldSuffix;
    }
    return channelName + fieldSuffix;
  }

  QString fetchPvInfoRelatedField(const QString &channelName,
      const QString &fieldSuffix) const
  {
    const QString fieldName =
        pvInfoRelatedFieldName(channelName, fieldSuffix).trimmed();
    if (fieldName.isEmpty()) {
      return QString();
    }

    QByteArray fieldBytes = fieldName.toLatin1();
    if (fieldBytes.isEmpty()) {
      return QString();
    }

    chid fieldId = nullptr;
    int status = ca_create_channel(fieldBytes.constData(), nullptr, nullptr,
        CA_PRIORITY_DEFAULT, &fieldId);
    if (status != ECA_NORMAL) {
      return QString();
    }

    struct ChannelCleanup {
      chid id = nullptr;
      bool owned = false;
      ~ChannelCleanup()
      {
        if (owned && id) {
          ca_clear_channel(id);
        }
      }
    } cleanup{fieldId, true};

    status = ca_pend_io(kPvInfoTimeoutSeconds);
    if (status != ECA_NORMAL || ca_state(fieldId) != cs_conn) {
      return QString();
    }

    dbr_string_t value{};
    status = ca_array_get(DBR_STRING, 1, fieldId, &value);
    if (status != ECA_NORMAL) {
      return QString();
    }
    status = ca_pend_io(kPvInfoTimeoutSeconds);
    if (status != ECA_NORMAL) {
      return QString();
    }

    return QString::fromLatin1(value);
  }

  bool populatePvInfoDetails(const QString &channelName, chid existingChid,
      PvInfoChannelDetails &details) const
  {
    details = PvInfoChannelDetails{};
    details.name = channelName;

    const QString trimmed = channelName.trimmed();
    if (trimmed.isEmpty()) {
      details.error = QStringLiteral("Channel name is empty.");
      return false;
    }

    chid channelId = nullptr;
    bool createdChannel = false;
    if (existingChid && ca_state(existingChid) == cs_conn) {
      channelId = existingChid;
    } else {
      QByteArray nameBytes = trimmed.toLatin1();
      if (nameBytes.isEmpty()) {
        details.error =
            QStringLiteral("Channel name cannot be converted to ASCII.");
        return false;
      }
      int status = ca_create_channel(nameBytes.constData(), nullptr, nullptr,
          CA_PRIORITY_DEFAULT, &channelId);
      if (status != ECA_NORMAL) {
        details.error = QStringLiteral("ca_create_channel failed: %1")
            .arg(QString::fromLatin1(ca_message(status)));
        return false;
      }
      createdChannel = true;
      status = ca_pend_io(kPvInfoTimeoutSeconds);
      if (status != ECA_NORMAL) {
        details.error = QStringLiteral("Timeout waiting for channel connection (%1)")
            .arg(QString::fromLatin1(ca_message(status)));
        ca_clear_channel(channelId);
        return false;
      }
      if (ca_state(channelId) != cs_conn) {
        details.error = QStringLiteral("Channel is not connected.");
        ca_clear_channel(channelId);
        return false;
      }
    }

    struct ChannelCleanup {
      chid id = nullptr;
      bool owned = false;
      ~ChannelCleanup()
      {
        if (owned && id) {
          ca_clear_channel(id);
        }
      }
    } cleanup{channelId, createdChannel};

    details.fieldType = ca_field_type(channelId);
    details.elementCount = static_cast<unsigned long>(ca_element_count(channelId));
    details.readAccess = ca_read_access(channelId);
    details.writeAccess = ca_write_access(channelId);
    if (const char *host = ca_host_name(channelId)) {
      details.host = QString::fromLatin1(host);
    }

    dbr_time_string timeValue{};
    int status = ca_array_get(DBR_TIME_STRING, 1, channelId, &timeValue);
    if (status == ECA_NORMAL) {
      status = ca_pend_io(kPvInfoTimeoutSeconds);
      if (status == ECA_NORMAL) {
        details.value = QString::fromLatin1(timeValue.value);
        details.hasValue = true;
        details.timestamp = timeValue.stamp;
        details.hasTimestamp = true;
        details.severity = timeValue.severity;
        details.status = timeValue.status;
      } else {
        details.value = QStringLiteral("Unavailable (%1)")
            .arg(QString::fromLatin1(ca_message(status)));
      }
    } else {
      details.value = QStringLiteral("Unavailable (%1)")
          .arg(QString::fromLatin1(ca_message(status)));
    }

    switch (details.fieldType) {
    case DBF_ENUM: {
      dbr_ctrl_enum ctrl{};
      status = ca_array_get(DBR_CTRL_ENUM, 1, channelId, &ctrl);
      if (status == ECA_NORMAL) {
        status = ca_pend_io(kPvInfoTimeoutSeconds);
        if (status == ECA_NORMAL) {
          for (unsigned short i = 0; i < ctrl.no_str; ++i) {
            details.states.append(QString::fromLatin1(ctrl.strs[i]));
          }
          details.hasStates = !details.states.isEmpty();
        }
      }
      break;
    }
    case DBF_CHAR:
    case DBF_SHORT:
    case DBF_LONG:
    case DBF_FLOAT:
    case DBF_DOUBLE: {
      dbr_ctrl_double ctrl{};
      status = ca_array_get(DBR_CTRL_DOUBLE, 1, channelId, &ctrl);
      if (status == ECA_NORMAL) {
        status = ca_pend_io(kPvInfoTimeoutSeconds);
        if (status == ECA_NORMAL) {
          details.hopr = ctrl.upper_ctrl_limit;
          details.lopr = ctrl.lower_ctrl_limit;
          details.hasLimits = true;
          details.precision = ctrl.precision;
          details.hasPrecision = (ctrl.precision >= 0);
        }
      }
      break;
    }
    default:
      break;
    }

    details.desc = fetchPvInfoRelatedField(trimmed, QStringLiteral(".DESC"));
    details.recordType = fetchPvInfoRelatedField(trimmed, QStringLiteral(".RTYP"));
    details.error.clear();
    return true;
  }

  bool prepareExecuteChannelDrag(const QPoint &windowPos)
  {
    if (!executeModeActive_) {
      return false;
    }

    QWidget *widget = elementAt(windowPos);
    if (!widget) {
      return false;
    }

    const QStringList channels = channelsForWidget(widget);
    if (channels.isEmpty()) {
      return false;
    }

    const QString text = channels.join(QStringLiteral(" "));
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
      clipboard->setText(text, QClipboard::Clipboard);
      clipboard->setText(text, QClipboard::Selection);
    }

    executeDragChannels_ = channels;
    executeDragStartWindowPos_ = windowPos;
    executeDragPending_ = true;
    executeDragStarted_ = false;
    executeDragTooltipText_ = text;
    updateExecuteDragTooltip(windowPos);
    return true;
  }

  void startExecuteChannelDrag()
  {
    hideExecuteDragTooltip();
    if (executeDragChannels_.isEmpty()) {
      return;
    }

    const QString text = executeDragChannels_.join(QStringLiteral(" "));
    auto *mimeData = new QMimeData;
    mimeData->setText(text);
    QDrag drag(this);
    drag.setMimeData(mimeData);
    drag.exec(Qt::CopyAction);
    cancelExecuteChannelDrag();
  }

  void cancelExecuteChannelDrag()
  {
    executeDragPending_ = false;
    executeDragStarted_ = false;
    executeDragChannels_.clear();
    hideExecuteDragTooltip();
  }

  QLabel *ensureExecuteDragTooltipLabel()
  {
    if (!executeDragTooltipLabel_) {
      auto *label = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
      label->setWindowFlag(Qt::WindowStaysOnTopHint);
      label->setAttribute(Qt::WA_ShowWithoutActivating);
      label->setAttribute(Qt::WA_TransparentForMouseEvents);
      label->setFocusPolicy(Qt::NoFocus);
      label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      label->setMargin(6);
      label->setWordWrap(false);
      label->setTextFormat(Qt::PlainText);
      QFont font = label->font();
      const qreal pointSizeF = font.pointSizeF();
      if (pointSizeF > 0.0) {
        font.setPointSizeF(pointSizeF + 2.0);
      } else {
        const int pointSize = font.pointSize();
        font.setPointSize(pointSize > 0 ? pointSize + 2 : 12);
      }
      label->setFont(font);
      executeDragTooltipLabel_ = label;
    }
    return executeDragTooltipLabel_;
  }

  void updateExecuteDragTooltip(const QPoint &windowPos)
  {
    if (executeDragTooltipText_.isEmpty()) {
      hideExecuteDragTooltip();
      return;
    }
    QLabel *tooltip = ensureExecuteDragTooltipLabel();
    if (!tooltip) {
      return;
    }
    tooltip->setText(executeDragTooltipText_);
    tooltip->adjustSize();
    const QPoint globalPos = mapToGlobal(windowPos + QPoint(16, 20));
    tooltip->move(globalPos);
    tooltip->raise();
    if (!tooltip->isVisible()) {
      tooltip->show();
    }
    executeDragTooltipVisible_ = true;
  }

  void hideExecuteDragTooltip()
  {
    if (executeDragTooltipLabel_) {
      executeDragTooltipLabel_->hide();
      executeDragTooltipLabel_->clear();
    }
    executeDragTooltipVisible_ = false;
    executeDragTooltipText_.clear();
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
        /* No need to update elementStackSet_ - same widget is still in stack */
        refreshStackingOrder();
        return;
      }
    }
    elementStack_.append(QPointer<QWidget>(element));
    refreshStackingOrder();
  }

  void removeElementFromStack(QWidget *element)
  {
    if (!element) {
      return;
    }
    bool removed = false;
    for (auto it = elementStack_.begin(); it != elementStack_.end();) {
      QWidget *current = it->data();
      if (!current || current == element) {
        if (current) {
          elementStackSet_.remove(current);
        }
        it = elementStack_.erase(it);
        removed = true;
      } else {
        ++it;
      }
    }
    if (removed) {
      refreshStackingOrder();
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
    bool handled = false;
    if (auto *text = dynamic_cast<TextElement *>(widget)) {
      selectTextElement(text);
      showResourcePaletteForText(text);
      handled = true;
    } else if (auto *textEntry = dynamic_cast<TextEntryElement *>(widget)) {
      selectTextEntryElement(textEntry);
      showResourcePaletteForTextEntry(textEntry);
      handled = true;
    } else if (auto *slider = dynamic_cast<SliderElement *>(widget)) {
      selectSliderElement(slider);
      showResourcePaletteForSlider(slider);
      handled = true;
    } else if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
      selectWheelSwitchElement(wheel);
      showResourcePaletteForWheelSwitch(wheel);
      handled = true;
    } else if (auto *choice = dynamic_cast<ChoiceButtonElement *>(widget)) {
      selectChoiceButtonElement(choice);
      showResourcePaletteForChoiceButton(choice);
      handled = true;
    } else if (auto *menu = dynamic_cast<MenuElement *>(widget)) {
      selectMenuElement(menu);
      showResourcePaletteForMenu(menu);
      handled = true;
    } else if (auto *message = dynamic_cast<MessageButtonElement *>(widget)) {
      selectMessageButtonElement(message);
      showResourcePaletteForMessageButton(message);
      handled = true;
    } else if (auto *shell = dynamic_cast<ShellCommandElement *>(widget)) {
      selectShellCommandElement(shell);
      showResourcePaletteForShellCommand(shell);
      handled = true;
    } else if (auto *related = dynamic_cast<RelatedDisplayElement *>(widget)) {
      selectRelatedDisplayElement(related);
      showResourcePaletteForRelatedDisplay(related);
      handled = true;
    } else if (auto *textMonitor = dynamic_cast<TextMonitorElement *>(widget)) {
      selectTextMonitorElement(textMonitor);
      showResourcePaletteForTextMonitor(textMonitor);
      handled = true;
    } else if (auto *meter = dynamic_cast<MeterElement *>(widget)) {
      selectMeterElement(meter);
      showResourcePaletteForMeter(meter);
      handled = true;
    } else if (auto *scale = dynamic_cast<ScaleMonitorElement *>(widget)) {
      selectScaleMonitorElement(scale);
      showResourcePaletteForScale(scale);
      handled = true;
    } else if (auto *strip = dynamic_cast<StripChartElement *>(widget)) {
      selectStripChartElement(strip);
      showResourcePaletteForStripChart(strip);
      handled = true;
    } else if (auto *cart = dynamic_cast<CartesianPlotElement *>(widget)) {
      selectCartesianPlotElement(cart);
      showResourcePaletteForCartesianPlot(cart);
      handled = true;
    } else if (auto *bar = dynamic_cast<BarMonitorElement *>(widget)) {
      selectBarMonitorElement(bar);
      showResourcePaletteForBar(bar);
      handled = true;
    } else if (auto *byte = dynamic_cast<ByteMonitorElement *>(widget)) {
      selectByteMonitorElement(byte);
      showResourcePaletteForByte(byte);
      handled = true;
    } else if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
      selectRectangleElement(rectangle);
      showResourcePaletteForRectangle(rectangle);
      handled = true;
    } else if (auto *image = dynamic_cast<ImageElement *>(widget)) {
      selectImageElement(image);
      showResourcePaletteForImage(image);
      handled = true;
    } else if (auto *oval = dynamic_cast<OvalElement *>(widget)) {
      selectOvalElement(oval);
      showResourcePaletteForOval(oval);
      handled = true;
    } else if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      selectArcElement(arc);
      showResourcePaletteForArc(arc);
      handled = true;
    } else if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      selectPolylineElement(polyline);
      showResourcePaletteForPolyline(polyline);
      handled = true;
    } else if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      selectPolygonElement(polygon);
      showResourcePaletteForPolygon(polygon);
      handled = true;
    } else if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
      selectCompositeElement(composite);
      showResourcePaletteForComposite(composite);
      handled = true;
    } else if (auto *line = dynamic_cast<LineElement *>(widget)) {
      selectLineElement(line);
      showResourcePaletteForLine(line);
      handled = true;
    }
    notifyMenus();
    return handled;
  }

  bool handleMultiSelectionClick(QWidget *widget,
      Qt::KeyboardModifiers modifiers)
  {
    if (!widget) {
      return false;
    }
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return false;
    }
    clearDisplaySelection();
    pruneMultiSelection();
    const bool toggle = modifiers.testFlag(Qt::ControlModifier);
    const QList<QWidget *> currentSelection = selectedWidgets();
    const bool currentlySelected = currentSelection.contains(widget);

    if (toggle && currentlySelected && multiSelection_.isEmpty()) {
      removeWidgetFromSelection(widget);
      if (selectedWidgets().isEmpty() && !displaySelected_) {
        closeResourcePalette();
      }
      notifyMenus();
      return true;
    }

    if (toggle && currentlySelected) {
      removeWidgetFromSelection(widget);
      updateSelectionAfterMultiChange();
      return true;
    }

    if (multiSelection_.isEmpty()) {
      if (QWidget *primary = currentSelectedWidget()) {
        addWidgetToMultiSelection(primary);
      }
    }

    addWidgetToMultiSelection(widget);
    updateSelectionAfterMultiChange();
    return true;
  }

  void beginMiddleButtonDrag(const QPoint &windowPos)
  {
    finishMiddleButtonDrag(false);
    finishMiddleButtonResize(false);
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
    QPoint clamped =
        clampOffsetToDisplayArea(middleButtonBoundingRect_, offset);
    clamped = snapOffsetToGrid(middleButtonBoundingRect_, clamped);
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
      setNextUndoLabel(QStringLiteral("Move Selection"));
      markDirty();
      refreshResourcePaletteGeometry();
    }
  }

  void beginMiddleButtonResize(const QPoint &windowPos)
  {
    finishMiddleButtonResize(false);
    finishMiddleButtonDrag(false);
    if (!displayArea_) {
      return;
    }
    QList<QWidget *> resizeWidgets;
    if (!multiSelection_.isEmpty()) {
      for (const auto &pointer : multiSelection_) {
        if (QWidget *widget = pointer.data()) {
          resizeWidgets.append(widget);
        }
      }
    } else {
      if (QWidget *selected = currentSelectedWidget()) {
        resizeWidgets.append(selected);
      }
    }
    if (resizeWidgets.isEmpty()) {
      return;
    }
    std::sort(resizeWidgets.begin(), resizeWidgets.end(),
        [this](QWidget *lhs, QWidget *rhs) {
          return widgetHierarchyDepth(lhs) < widgetHierarchyDepth(rhs);
        });
    middleButtonResizeWidgets_.clear();
    middleButtonResizeInitialRects_.clear();
    middleButtonResizeCompositeInfo_.clear();
    middleButtonResizeCompositeUpdated_.clear();
    for (QWidget *widget : std::as_const(resizeWidgets)) {
      if (!widget) {
        continue;
      }
      const QRect rect = widgetDisplayRect(widget);
      if (!rect.isValid()) {
        continue;
      }
      middleButtonResizeWidgets_.append(QPointer<QWidget>(widget));
      middleButtonResizeInitialRects_.append(rect);
      if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
        collectCompositeResizeInfo(composite);
      }
    }
    if (middleButtonResizeWidgets_.isEmpty()) {
      middleButtonResizeCompositeInfo_.clear();
      return;
    }
    QPoint startPos = displayArea_->mapFrom(this, windowPos);
    startPos = clampToDisplayArea(startPos);
    if (snapToGrid_ && gridSpacing_ > 0) {
      startPos = snapPointToGrid(startPos);
    }
    middleButtonResizeStartAreaPos_ = startPos;
    middleButtonResizeActive_ = true;
    middleButtonResizeMoved_ = false;
  }

  void updateMiddleButtonResize(const QPoint &windowPos)
  {
    if (!middleButtonResizeActive_ || middleButtonResizeWidgets_.isEmpty()
        || !displayArea_) {
      return;
    }
    QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    areaPos = clampToDisplayArea(areaPos);
    if (snapToGrid_ && gridSpacing_ > 0) {
      areaPos = snapPointToGrid(areaPos);
    }
    const QPoint offset = areaPos - middleButtonResizeStartAreaPos_;
    bool anyResized = false;
    middleButtonResizeCompositeUpdated_.clear();
    const int widgetCount = middleButtonResizeWidgets_.size();
    for (int i = 0; i < widgetCount; ++i) {
      QWidget *widget = middleButtonResizeWidgets_.at(i).data();
      if (!widget) {
        continue;
      }
      if (middleButtonResizeCompositeUpdated_.contains(widget)) {
        widget->update();
        continue;
      }
      const QRect initialRect = middleButtonResizeInitialRects_.value(i);
      if (!initialRect.isValid()) {
        continue;
      }
      const int newWidth = std::max(initialRect.width() + offset.x(), 1);
      const int newHeight = std::max(initialRect.height() + offset.y(), 1);
      QRect targetRect(initialRect.topLeft(), QSize(newWidth, newHeight));
      targetRect = adjustRectToDisplayArea(targetRect);
      const QRect currentRect = widgetDisplayRect(widget);
      bool widgetChanged = false;
      if (targetRect != currentRect) {
        setWidgetDisplayRect(widget, targetRect);
        widget->update();
        widgetChanged = true;
      } else {
        widget->update();
      }
      if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
        if (applyCompositeResize(composite, targetRect)) {
          widgetChanged = true;
        }
      }
      if (widgetChanged) {
        anyResized = true;
      }
    }
    if (anyResized) {
      middleButtonResizeMoved_ = true;
    }
  }

  void collectCompositeResizeInfo(CompositeElement *composite)
  {
    if (!composite || middleButtonResizeCompositeInfo_.contains(composite)) {
      return;
    }
    CompositeResizeInfo info;
    info.initialRect = composite->geometry();
    if (!info.initialRect.isValid()) {
      middleButtonResizeCompositeInfo_.insert(composite, info);
      return;
    }
    const QList<QWidget *> children = composite->childWidgets();
    info.children.reserve(children.size());
    for (QWidget *child : children) {
      if (!child) {
        continue;
      }
      CompositeResizeChildInfo childInfo;
      childInfo.widget = child;
      childInfo.initialRect = widgetDisplayRect(child);
      info.children.append(childInfo);
    }
    middleButtonResizeCompositeInfo_.insert(composite, info);
    for (QWidget *child : children) {
      if (auto *childComposite = dynamic_cast<CompositeElement *>(child)) {
        collectCompositeResizeInfo(childComposite);
      }
    }
  }

  bool applyCompositeResize(CompositeElement *composite,
      const QRect &targetRect)
  {
    auto it = middleButtonResizeCompositeInfo_.find(composite);
    if (it == middleButtonResizeCompositeInfo_.end()) {
      return false;
    }
    const CompositeResizeInfo &info = it.value();
    const QRect &initialRect = info.initialRect;
    if (!initialRect.isValid()) {
      return false;
    }
    const double scaleX = initialRect.width() > 0
        ? static_cast<double>(targetRect.width()) / initialRect.width()
        : 1.0;
    const double scaleY = initialRect.height() > 0
        ? static_cast<double>(targetRect.height()) / initialRect.height()
        : 1.0;
    bool changed = false;
    for (const CompositeResizeChildInfo &childInfo : info.children) {
      QWidget *child = childInfo.widget;
      if (!child) {
        continue;
      }
      const QRect childInitialRect = childInfo.initialRect;
      if (!childInitialRect.isValid()) {
        continue;
      }
      const double relativeLeft =
          static_cast<double>(childInitialRect.x() - initialRect.x());
      const double relativeTop =
          static_cast<double>(childInitialRect.y() - initialRect.y());
      const int newLeft = targetRect.x()
          + static_cast<int>(std::round(relativeLeft * scaleX));
      const int newTop = targetRect.y()
          + static_cast<int>(std::round(relativeTop * scaleY));
      const int newWidth = std::max(1,
          static_cast<int>(std::round(childInitialRect.width() * scaleX)));
      const int newHeight = std::max(1,
          static_cast<int>(std::round(childInitialRect.height() * scaleY)));
      QRect newRect(QPoint(newLeft, newTop), QSize(newWidth, newHeight));
      const QRect currentRect = widgetDisplayRect(child);
      if (newRect != currentRect) {
        setWidgetDisplayRect(child, newRect);
        changed = true;
      }
      child->update();
      middleButtonResizeCompositeUpdated_.insert(child);
      if (auto *childComposite = dynamic_cast<CompositeElement *>(child)) {
        if (applyCompositeResize(childComposite, newRect)) {
          changed = true;
        }
      }
    }
    return changed;
  }

  int widgetHierarchyDepth(QWidget *widget) const
  {
    int depth = 0;
    while (widget) {
      widget = widget->parentWidget();
      ++depth;
      if (widget == displayArea_) {
        break;
      }
    }
    return depth;
  }

  void finishMiddleButtonResize(bool applyChanges)
  {
    const bool wasActive = middleButtonResizeActive_;
    const bool resized = middleButtonResizeMoved_;
    middleButtonResizeActive_ = false;
    middleButtonResizeMoved_ = false;
    middleButtonResizeWidgets_.clear();
    middleButtonResizeInitialRects_.clear();
    middleButtonResizeCompositeInfo_.clear();
    middleButtonResizeCompositeUpdated_.clear();
    if (applyChanges && wasActive && resized) {
      setNextUndoLabel(QStringLiteral("Resize Selection"));
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void cancelMiddleButtonDrag()
  {
    finishMiddleButtonDrag(false);
    finishMiddleButtonResize(false);
  }

  void alignSelectionInternal(AlignmentMode mode)
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.size() < 2) {
      return;
    }

    QString label;
    switch (mode) {
    case AlignmentMode::kLeft:
      label = QStringLiteral("Align Left");
      break;
    case AlignmentMode::kHorizontalCenter:
      label = QStringLiteral("Align Horizontal Center");
      break;
    case AlignmentMode::kRight:
      label = QStringLiteral("Align Right");
      break;
    case AlignmentMode::kTop:
      label = QStringLiteral("Align Top");
      break;
    case AlignmentMode::kVerticalCenter:
      label = QStringLiteral("Align Vertical Center");
      break;
    case AlignmentMode::kBottom:
      label = QStringLiteral("Align Bottom");
      break;
    }
    if (label.isEmpty()) {
      label = QStringLiteral("Align Selection");
    }
    setNextUndoLabel(label);

    int minLeft = std::numeric_limits<int>::max();
    int minTop = std::numeric_limits<int>::max();
    int maxRight = std::numeric_limits<int>::min();
    int maxBottom = std::numeric_limits<int>::min();

    QVector<QRect> geometries;
    geometries.reserve(widgets.size());
    for (QWidget *widget : widgets) {
      const QRect rect = widgetDisplayRect(widget);
      geometries.append(rect);
      minLeft = std::min(minLeft, rect.left());
      minTop = std::min(minTop, rect.top());
      maxRight = std::max(maxRight, rect.left() + rect.width());
      maxBottom = std::max(maxBottom, rect.top() + rect.height());
    }

    const int centerX = (minLeft + maxRight) / 2;
    const int centerY = (minTop + maxBottom) / 2;

    bool changed = false;
    for (int i = 0; i < widgets.size(); ++i) {
      QWidget *widget = widgets.at(i);
      QRect rect = geometries.at(i);
      int targetLeft = rect.left();
      int targetTop = rect.top();

      switch (mode) {
      case AlignmentMode::kLeft:
        targetLeft = minLeft;
        break;
      case AlignmentMode::kHorizontalCenter:
        targetLeft = centerX - rect.width() / 2;
        break;
      case AlignmentMode::kRight:
        targetLeft = maxRight - rect.width();
        break;
      case AlignmentMode::kTop:
        targetTop = minTop;
        break;
      case AlignmentMode::kVerticalCenter:
        targetTop = centerY - rect.height() / 2;
        break;
      case AlignmentMode::kBottom:
        targetTop = maxBottom - rect.height();
        break;
      }

      if (targetLeft != rect.left() || targetTop != rect.top()) {
        rect.moveTo(targetLeft, targetTop);
        setWidgetDisplayRect(widget, rect);
        changed = true;
      }
    }

    if (changed) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void orientSelectionInternal(OrientationAction action)
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.isEmpty()) {
      return;
    }

    QString label;
    switch (action) {
    case OrientationAction::kFlipHorizontal:
      label = QStringLiteral("Flip Horizontal");
      break;
    case OrientationAction::kFlipVertical:
      label = QStringLiteral("Flip Vertical");
      break;
    case OrientationAction::kRotateClockwise:
      label = QStringLiteral("Rotate Clockwise");
      break;
    case OrientationAction::kRotateCounterclockwise:
      label = QStringLiteral("Rotate Counterclockwise");
      break;
    }
    if (label.isEmpty()) {
      label = QStringLiteral("Orient Selection");
    }
    setNextUndoLabel(label);

    int minLeft = std::numeric_limits<int>::max();
    int minTop = std::numeric_limits<int>::max();
    int maxRight = std::numeric_limits<int>::min();
    int maxBottom = std::numeric_limits<int>::min();

    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      const QRect rect = widgetDisplayRect(widget);
      minLeft = std::min(minLeft, rect.left());
      minTop = std::min(minTop, rect.top());
      maxRight = std::max(maxRight, rect.left() + rect.width());
      maxBottom = std::max(maxBottom, rect.top() + rect.height());
    }

    if (minLeft == std::numeric_limits<int>::max()
        || minTop == std::numeric_limits<int>::max()
        || maxRight == std::numeric_limits<int>::min()
        || maxBottom == std::numeric_limits<int>::min()) {
      return;
    }

    const int centerX = (minLeft + maxRight) / 2;
    const int centerY = (minTop + maxBottom) / 2;

    bool anyChanged = false;
    QSet<QWidget *> orientedWidgets;
    orientedWidgets.reserve(widgets.size());

    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      if (orientedWidgets.contains(widget)) {
        continue;
      }
      if (applyOrientationToWidget(widget, action, centerX, centerY,
              &orientedWidgets)) {
        anyChanged = true;
      }
    }

    if (anyChanged) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  QRect orientedRect(const QRect &rect, OrientationAction action, int centerX,
      int centerY) const
  {
    const int left = rect.left();
    const int top = rect.top();
    const int width = rect.width();
    const int height = rect.height();

    int newLeft = left;
    int newTop = top;
    int newWidth = width;
    int newHeight = height;

    switch (action) {
    case OrientationAction::kFlipHorizontal:
      newLeft = 2 * centerX - left - width;
      break;
    case OrientationAction::kFlipVertical:
      newTop = 2 * centerY - top - height;
      break;
    case OrientationAction::kRotateClockwise:
      newLeft = centerX - top + centerY - height;
      newTop = centerY + left - centerX;
      newWidth = height;
      newHeight = width;
      break;
    case OrientationAction::kRotateCounterclockwise:
      newLeft = centerX + top - centerY;
      newTop = centerY - left + centerX - width;
      newWidth = height;
      newHeight = width;
      break;
    }

    newLeft = std::max(0, newLeft);
    newTop = std::max(0, newTop);
    newWidth = std::max(1, newWidth);
    newHeight = std::max(1, newHeight);

    return QRect(QPoint(newLeft, newTop), QSize(newWidth, newHeight));
  }

  QPoint orientedPoint(const QPoint &point, OrientationAction action,
      int centerX, int centerY) const
  {
    int x = point.x();
    int y = point.y();

    switch (action) {
    case OrientationAction::kFlipHorizontal:
      x = 2 * centerX - x;
      break;
    case OrientationAction::kFlipVertical:
      y = 2 * centerY - y;
      break;
    case OrientationAction::kRotateClockwise: {
      const int newX = centerX - y + centerY;
      const int newY = centerY + x - centerX;
      x = newX;
      y = newY;
      break;
    }
    case OrientationAction::kRotateCounterclockwise: {
      const int newX = centerX + y - centerY;
      const int newY = centerY - x + centerX;
      x = newX;
      y = newY;
      break;
    }
    }

    x = std::max(0, x);
    y = std::max(0, y);
    return QPoint(x, y);
  }

  bool orientGenericWidget(QWidget *widget, OrientationAction action,
      int centerX, int centerY)
  {
    if (!widget) {
      return false;
    }
    const QRect currentRect = widgetDisplayRect(widget);
    const QRect targetRect =
        orientedRect(currentRect, action, centerX, centerY);
    if (targetRect == currentRect) {
      return false;
    }
    setWidgetDisplayRect(widget, targetRect);
    widget->update();
    return true;
  }

  bool orientLineElement(LineElement *line, OrientationAction action,
      int centerX, int centerY)
  {
    if (!line || !displayArea_) {
      return false;
    }

    const QVector<QPoint> parentPoints = line->absolutePoints();
    if (parentPoints.size() < 2) {
      return false;
    }

    QVector<QPoint> displayPoints;
    displayPoints.reserve(parentPoints.size());
    const QPoint topLeftInParent = line->geometry().topLeft();
    for (const QPoint &point : parentPoints) {
      const QPoint localPoint = point - topLeftInParent;
      displayPoints.append(line->mapTo(displayArea_, localPoint));
    }

    QVector<QPoint> orientedPoints;
    orientedPoints.reserve(displayPoints.size());
    for (const QPoint &point : displayPoints) {
      orientedPoints.append(orientedPoint(point, action, centerX, centerY));
    }

    if (displayPoints == orientedPoints) {
      return false;
    }

    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();
    for (const QPoint &point : orientedPoints) {
      minX = std::min(minX, point.x());
      minY = std::min(minY, point.y());
      maxX = std::max(maxX, point.x());
      maxY = std::max(maxY, point.y());
    }

    if (minX == std::numeric_limits<int>::max()
        || minY == std::numeric_limits<int>::max()
        || maxX == std::numeric_limits<int>::min()
        || maxY == std::numeric_limits<int>::min()) {
      return false;
    }

    const int width = std::max(1, maxX - minX + 1);
    const int height = std::max(1, maxY - minY + 1);
    const QRect targetRect(QPoint(minX, minY), QSize(width, height));
    setWidgetDisplayRect(line, targetRect);

    const QPoint startLocal = orientedPoints.first() - targetRect.topLeft();
    const QPoint endLocal = orientedPoints.last() - targetRect.topLeft();
    line->setLocalEndpoints(startLocal, endLocal);
    line->update();
    return true;
  }

  template <typename Element>
  bool orientElementWithAbsolutePoints(Element *element,
      OrientationAction action, int centerX, int centerY)
  {
    if (!element || !displayArea_) {
      return false;
    }

    const QVector<QPoint> parentPoints = element->absolutePoints();
    if (parentPoints.isEmpty()) {
      return false;
    }

    QVector<QPoint> displayPoints;
    displayPoints.reserve(parentPoints.size());
    const QPoint topLeftInParent = element->geometry().topLeft();
    for (const QPoint &point : parentPoints) {
      const QPoint localPoint = point - topLeftInParent;
      displayPoints.append(element->mapTo(displayArea_, localPoint));
    }

    QVector<QPoint> orientedPoints;
    orientedPoints.reserve(displayPoints.size());
    for (const QPoint &point : displayPoints) {
      orientedPoints.append(orientedPoint(point, action, centerX, centerY));
    }

    if (displayPoints == orientedPoints) {
      return false;
    }

    QWidget *parent = element->parentWidget();
    QVector<QPoint> parentCoordinates;
    parentCoordinates.reserve(orientedPoints.size());
    for (const QPoint &point : orientedPoints) {
      if (parent) {
        parentCoordinates.append(parent->mapFrom(displayArea_, point));
      } else {
        parentCoordinates.append(point);
      }
    }

    element->setAbsolutePoints(parentCoordinates);
    element->update();
    return true;
  }

  bool orientArcElement(ArcElement *arc, OrientationAction action,
      int centerX, int centerY)
  {
    if (!arc) {
      return false;
    }

    constexpr int kFullCircle = 360 * 64;
    constexpr int kHalfCircle = 180 * 64;
    constexpr int kQuarterCircle = 90 * 64;

    int begin = arc->beginAngle();
    const int path = arc->pathAngle();

    switch (action) {
    case OrientationAction::kFlipHorizontal:
      begin = kHalfCircle - begin;
      begin -= path;
      break;
    case OrientationAction::kFlipVertical:
      begin = -begin;
      begin -= path;
      break;
    case OrientationAction::kRotateClockwise:
      begin -= kQuarterCircle;
      break;
    case OrientationAction::kRotateCounterclockwise:
      begin += kQuarterCircle;
      break;
    }

    while (begin >= kFullCircle) {
      begin -= kFullCircle;
    }
    while (begin < 0) {
      begin += kFullCircle;
    }

    const bool angleChanged = begin != arc->beginAngle();
    if (angleChanged) {
      arc->setBeginAngle(begin);
    }

    const bool rectChanged =
        orientGenericWidget(arc, action, centerX, centerY);
    return angleChanged || rectChanged;
  }

  bool applyOrientationToWidget(QWidget *widget, OrientationAction action,
      int centerX, int centerY, QSet<QWidget *> *orientedWidgets)
  {
    if (!widget) {
      return false;
    }

    bool changed = false;
    if (auto *composite = dynamic_cast<CompositeElement *>(widget)) {
      for (QWidget *child : composite->childWidgets()) {
        if (!child) {
          continue;
        }
        changed |= applyOrientationToWidget(child, action, centerX, centerY,
            orientedWidgets);
      }
      changed |= orientGenericWidget(composite, action, centerX, centerY);
    } else if (auto *line = dynamic_cast<LineElement *>(widget)) {
      changed |= orientLineElement(line, action, centerX, centerY);
    } else if (auto *polyline = dynamic_cast<PolylineElement *>(widget)) {
      changed |= orientElementWithAbsolutePoints(polyline, action, centerX,
          centerY);
    } else if (auto *polygon = dynamic_cast<PolygonElement *>(widget)) {
      changed |= orientElementWithAbsolutePoints(polygon, action, centerX,
          centerY);
    } else if (auto *arc = dynamic_cast<ArcElement *>(widget)) {
      changed |= orientArcElement(arc, action, centerX, centerY);
    } else {
      changed |= orientGenericWidget(widget, action, centerX, centerY);
    }

    if (orientedWidgets) {
      orientedWidgets->insert(widget);
    }
    return changed;
  }

  void centerSelectionInDisplayInternal(bool horizontal, bool vertical)
  {
    if (!horizontal && !vertical) {
      return;
    }
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || !displayArea_) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.isEmpty()) {
      return;
    }

    QString label;
    if (horizontal && vertical) {
      label = QStringLiteral("Center Both");
    } else if (horizontal) {
      label = QStringLiteral("Center Horizontally");
    } else if (vertical) {
      label = QStringLiteral("Center Vertically");
    } else {
      label = QStringLiteral("Center Selection");
    }
    setNextUndoLabel(label);

    QRect selectionBounds;
    bool hasBounds = false;
    QVector<QRect> geometries;
    geometries.reserve(widgets.size());

    for (QWidget *widget : widgets) {
      const QRect rect = widgetDisplayRect(widget);
      geometries.append(rect);
      if (!hasBounds) {
        selectionBounds = rect;
        hasBounds = true;
      } else {
        selectionBounds = selectionBounds.united(rect);
      }
    }

    if (!hasBounds) {
      return;
    }

    const QRect displayRect = displayArea_->rect();
    const QPoint displayCenter = displayRect.center();
    const QPoint selectionCenter = selectionBounds.center();

    const int deltaX = horizontal ? (displayCenter.x() - selectionCenter.x()) : 0;
    const int deltaY = vertical ? (displayCenter.y() - selectionCenter.y()) : 0;

    if (deltaX == 0 && deltaY == 0) {
      return;
    }

    bool changed = false;
    for (int i = 0; i < widgets.size(); ++i) {
      QWidget *widget = widgets.at(i);
      QRect rect = geometries.at(i);
      rect.translate(deltaX, deltaY);
      if (rect != geometries.at(i)) {
        setWidgetDisplayRect(widget, rect);
        changed = true;
      }
    }

    if (changed) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void alignSelectionToGridInternal(bool edges)
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || gridSpacing_ <= 0) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.isEmpty()) {
      return;
    }

    bool anyChanged = false;
    const int spacing = gridSpacing_;

    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      QRect rect = widgetDisplayRect(widget);
      if (!rect.isValid()) {
        continue;
      }

      bool widgetChanged = false;
      const QPoint originalTopLeft = rect.topLeft();
      const QPoint snappedTopLeft(
          snapCoordinateToGrid(originalTopLeft.x(), spacing),
          snapCoordinateToGrid(originalTopLeft.y(), spacing));
      if (snappedTopLeft != originalTopLeft) {
        rect.moveTopLeft(snappedTopLeft);
        widgetChanged = true;
      }

      if (edges) {
        const QPoint currentBottomRight = rect.bottomRight();
        const QPoint snappedBottomRight(
            snapCoordinateToGrid(currentBottomRight.x(), spacing),
            snapCoordinateToGrid(currentBottomRight.y(), spacing));
        int newWidth = snappedBottomRight.x() - rect.left() + 1;
        int newHeight = snappedBottomRight.y() - rect.top() + 1;
        if (newWidth < 1) {
          newWidth = 1;
        }
        if (newHeight < 1) {
          newHeight = 1;
        }
        if (newWidth != rect.width() || newHeight != rect.height()) {
          rect.setSize(QSize(newWidth, newHeight));
          widgetChanged = true;
        }
      }

      if (widgetChanged) {
        setWidgetDisplayRect(widget, rect);
        anyChanged = true;
      }
    }

    if (anyChanged) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void spaceSelectionLinear(Qt::Orientation orientation)
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.size() < 2) {
      return;
    }

    struct Entry {
      QWidget *widget = nullptr;
      QRect rect;
    };

    QVector<Entry> entries;
    entries.reserve(widgets.size());
    for (QWidget *widget : widgets) {
      const QRect rect = widgetDisplayRect(widget);
      if (!rect.isValid()) {
        continue;
      }
      entries.append({widget, rect});
    }

    if (entries.size() < 2) {
      return;
    }

    std::sort(entries.begin(), entries.end(),
        [orientation](const Entry &lhs, const Entry &rhs) {
          if (orientation == Qt::Horizontal) {
            if (lhs.rect.left() == rhs.rect.left()) {
              return lhs.rect.top() < rhs.rect.top();
            }
            return lhs.rect.left() < rhs.rect.left();
          }
          if (lhs.rect.top() == rhs.rect.top()) {
            return lhs.rect.left() < rhs.rect.left();
          }
          return lhs.rect.top() < rhs.rect.top();
        });

    const int spacing = std::max(0, gridSpacing_);
    bool anyChanged = false;
    bool firstEntry = true;
    int nextCoordinate = 0;

    for (auto &entry : entries) {
      QRect rect = entry.rect;
      bool entryChanged = false;

      if (firstEntry) {
        nextCoordinate = orientation == Qt::Horizontal ? rect.left()
                                                       : rect.top();
        firstEntry = false;
      } else {
        const int target = nextCoordinate;
        if (orientation == Qt::Horizontal) {
          if (rect.left() != target) {
            rect.moveLeft(target);
            entryChanged = true;
          }
        } else {
          if (rect.top() != target) {
            rect.moveTop(target);
            entryChanged = true;
          }
        }
      }

      if (entryChanged) {
        setWidgetDisplayRect(entry.widget, rect);
        entry.rect = rect;
        anyChanged = true;
      }

      if (orientation == Qt::Horizontal) {
        nextCoordinate = rect.left() + rect.width() + spacing;
      } else {
        nextCoordinate = rect.top() + rect.height() + spacing;
      }
    }

    if (anyChanged) {
      setNextUndoLabel(orientation == Qt::Horizontal
          ? QStringLiteral("Space Horizontally")
          : QStringLiteral("Space Vertically"));
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  void spaceSelection2DInternal()
  {
    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode) {
      return;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.size() < 2) {
      return;
    }

    struct Entry {
      QWidget *widget = nullptr;
      QRect rect;
    };

    QVector<Entry> entries;
    entries.reserve(widgets.size());
    for (QWidget *widget : widgets) {
      const QRect rect = widgetDisplayRect(widget);
      if (!rect.isValid()) {
        continue;
      }
      entries.append({widget, rect});
    }

    if (entries.size() < 2) {
      return;
    }

    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxY = std::numeric_limits<int>::min();
    int totalHeight = 0;
    int elementCount = 0;
    int maxRowCount = 1;

    for (int i = 0; i < entries.size(); ++i) {
      const QRect &rect = entries.at(i).rect;
      minX = std::min(minX, rect.left());
      minY = std::min(minY, rect.top());
      maxY = std::max(maxY, rect.top() + rect.height());
      totalHeight += rect.height();
      ++elementCount;

      const int xLeft = rect.left();
      const int xRight = rect.left() + rect.width();
      int overlapping = 0;
      for (const Entry &other : std::as_const(entries)) {
        const int xCenter =
            other.rect.left() + other.rect.width() / 2;
        if (xCenter >= xLeft && xCenter <= xRight) {
          ++overlapping;
        }
      }
      if (overlapping > maxRowCount) {
        maxRowCount = overlapping;
      }
    }

    if (elementCount < 1 || minX == std::numeric_limits<int>::max()
        || minY == std::numeric_limits<int>::max()) {
      return;
    }

    const int spacing = std::max(0, gridSpacing_);
    const int averageHeight =
        std::max(1, (totalHeight + elementCount / 2) / elementCount);
    const int deltaYRaw = maxY - minY;
    int deltaY = maxRowCount > 0
        ? ((deltaYRaw + maxRowCount - 1) / maxRowCount)
        : deltaYRaw;
    if (deltaY <= 0) {
      deltaY = averageHeight;
    }
    if (deltaY <= 0) {
      deltaY = 1;
    }
    if (maxRowCount <= 0) {
      maxRowCount = 1;
    }

    QVector<QVector<int>> rows(maxRowCount);
    for (int index = 0; index < entries.size(); ++index) {
      const QRect &rect = entries.at(index).rect;
      const int centerY = rect.top() + rect.height() / 2;
      int rowIndex = 0;
      if (deltaY > 0) {
        rowIndex = (centerY - minY) / deltaY;
      }
      if (rowIndex < 0) {
        rowIndex = 0;
      } else if (rowIndex >= maxRowCount) {
        rowIndex = maxRowCount - 1;
      }
      rows[rowIndex].append(index);
    }

    bool anyChanged = false;
    const int rowStep = std::max(1, averageHeight + spacing);

    for (int row = 0; row < rows.size(); ++row) {
      auto &indices = rows[row];
      if (indices.isEmpty()) {
        continue;
      }

      std::sort(indices.begin(), indices.end(),
          [&entries](int lhs, int rhs) {
            const QRect &leftRect = entries.at(lhs).rect;
            const QRect &rightRect = entries.at(rhs).rect;
            if (leftRect.left() == rightRect.left()) {
              return leftRect.top() < rightRect.top();
            }
            return leftRect.left() < rightRect.left();
          });

      int currentX = minX;
      const int targetTop = minY + row * rowStep;

      for (int idx : std::as_const(indices)) {
        auto &entry = entries[idx];
        QRect rect = entry.rect;
        bool entryChanged = false;

        if (rect.left() != currentX) {
          rect.moveLeft(currentX);
          entryChanged = true;
        }
        if (rect.top() != targetTop) {
          rect.moveTop(targetTop);
          entryChanged = true;
        }

        if (entryChanged) {
          setWidgetDisplayRect(entry.widget, rect);
          entry.rect = rect;
          anyChanged = true;
        }

        currentX = rect.left() + rect.width() + spacing;
      }
    }

    if (anyChanged) {
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
  }

  bool handleEditArrowKey(QKeyEvent *event)
  {
    if (!event) {
      return false;
    }
    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);

    QPoint delta;
    switch (event->key()) {
    case Qt::Key_Left:
      if (control) {
        const int amount = shift ? 10 : 1;
        return resizeSelectionBy(ResizeDirection::kLeft, amount);
      }
      delta = QPoint(-1, 0);
      break;
    case Qt::Key_Right:
      if (control) {
        const int amount = shift ? 10 : 1;
        return resizeSelectionBy(ResizeDirection::kRight, amount);
      }
      delta = QPoint(1, 0);
      break;
    case Qt::Key_Up:
      if (control) {
        const int amount = shift ? 10 : 1;
        return resizeSelectionBy(ResizeDirection::kUp, amount);
      }
      delta = QPoint(0, -1);
      break;
    case Qt::Key_Down:
      if (control) {
        const int amount = shift ? 10 : 1;
        return resizeSelectionBy(ResizeDirection::kDown, amount);
      }
      delta = QPoint(0, 1);
      break;
    default:
      return false;
    }

    if (shift) {
      delta *= 10;
    }

    return moveSelectionBy(delta);
  }

  bool resizeSelectionBy(ResizeDirection direction, int amount)
  {
    if (amount <= 0) {
      return false;
    }

    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || state->createTool != CreateTool::kNone) {
      return false;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.isEmpty()) {
      return false;
    }

    bool anyProcessed = false;
    bool anyChanged = false;

    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      const QRect currentRect = widgetDisplayRect(widget);
      if (!currentRect.isValid()) {
        continue;
      }

      anyProcessed = true;
      QRect targetRect = currentRect;

      switch (direction) {
      case ResizeDirection::kLeft:
        targetRect.setWidth(std::max(currentRect.width() - amount, 1));
        break;
      case ResizeDirection::kRight:
        targetRect.setWidth(currentRect.width() + amount);
        break;
      case ResizeDirection::kUp:
        targetRect.setHeight(std::max(currentRect.height() - amount, 1));
        break;
      case ResizeDirection::kDown:
        targetRect.setHeight(currentRect.height() + amount);
        break;
      }

      targetRect = adjustRectToDisplayArea(targetRect);
      if (targetRect == currentRect) {
        continue;
      }

      setWidgetDisplayRect(widget, targetRect);
      widget->update();
      anyChanged = true;
    }

    if (!anyProcessed) {
      return false;
    }

    if (anyChanged) {
      setNextUndoLabel(QStringLiteral("Space Evenly"));
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }
    return true;
  }

  bool moveSelectionBy(const QPoint &delta)
  {
    if (delta.isNull()) {
      return false;
    }

    setAsActiveDisplay();
    auto state = state_.lock();
    if (!state || !state->editMode || state->createTool != CreateTool::kNone) {
      return false;
    }

    const QList<QWidget *> widgets = alignableWidgets();
    if (widgets.isEmpty()) {
      return false;
    }

    QRect boundingRect;
    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      const QRect rect = widgetDisplayRect(widget);
      if (!rect.isValid()) {
        continue;
      }
      if (!boundingRect.isValid()) {
        boundingRect = rect;
      } else {
        boundingRect = boundingRect.united(rect);
      }
    }

    if (!boundingRect.isValid()) {
      return false;
    }

    const QPoint effectiveDelta =
        clampOffsetToDisplayArea(boundingRect, delta);
    if (effectiveDelta.isNull()) {
      return false;
    }

    bool anyMoved = false;
    for (QWidget *widget : widgets) {
      if (!widget) {
        continue;
      }
      const QRect currentRect = widgetDisplayRect(widget);
      const QRect targetRect = currentRect.translated(effectiveDelta);
      if (currentRect.topLeft() == targetRect.topLeft()) {
        continue;
      }
      setWidgetDisplayRect(widget, targetRect);
      widget->update();
      anyMoved = true;
    }

    if (!anyMoved) {
      return false;
    }

    markDirty();
    refreshResourcePaletteGeometry();
    notifyMenus();
    return true;
  }

  static int snapCoordinateToGrid(int value, int spacing)
  {
    if (spacing <= 0) {
      return value;
    }
    const int base = (value / spacing) * spacing;
    int offset = value - base;
    if (offset > spacing / 2) {
      offset -= spacing;
    }
    return value - offset;
  }

  void refreshResourcePaletteGeometry()
  {
    if (!resourcePalette_) {
      return;
    }
    resourcePalette_->refreshGeometryFromSelection();
  }

  void updateResourcePaletteDisplayControls()
  {
    if (!resourcePalette_) {
      return;
    }
    resourcePalette_->refreshDisplayControls();
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
    updateSelectionAfterMultiChange();
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
    rubberBandOrigin_ = snapPointToGrid(rubberBandOrigin_);
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
    QPoint clamped = clampToDisplayArea(areaPos);
    clamped = snapPointToGrid(clamped);
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
    QPoint clamped = clampToDisplayArea(areaPos);
    clamped = snapPointToGrid(clamped);
    QRect rect = QRect(rubberBandOrigin_, clamped).normalized();
    switch (tool) {
    case CreateTool::kText:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createTextElement(rect);
      break;
    case CreateTool::kTextMonitor:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createTextMonitorElement(rect);
      break;
    case CreateTool::kTextEntry:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createTextEntryElement(rect);
      break;
    case CreateTool::kSlider:
      if (rect.width() < kMinimumSliderWidth) {
        rect.setWidth(kMinimumSliderWidth);
      }
      if (rect.height() < kMinimumSliderHeight) {
        rect.setHeight(kMinimumSliderHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createSliderElement(rect);
      break;
    case CreateTool::kWheelSwitch:
      if (rect.width() < kMinimumWheelSwitchWidth) {
        rect.setWidth(kMinimumWheelSwitchWidth);
      }
      if (rect.height() < kMinimumWheelSwitchHeight) {
        rect.setHeight(kMinimumWheelSwitchHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createWheelSwitchElement(rect);
      break;
    case CreateTool::kChoiceButton:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createChoiceButtonElement(rect);
      break;
    case CreateTool::kMenu:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createMenuElement(rect);
      break;
    case CreateTool::kMessageButton:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createMessageButtonElement(rect);
      break;
    case CreateTool::kShellCommand:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createShellCommandElement(rect);
      break;
    case CreateTool::kMeter:
      if (rect.width() < kMinimumMeterSize) {
        rect.setWidth(kMinimumMeterSize);
      }
      if (rect.height() < kMinimumMeterSize) {
        rect.setHeight(kMinimumMeterSize);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createMeterElement(rect);
      break;
    case CreateTool::kBarMonitor:
      if (rect.width() < kMinimumBarSize) {
        rect.setWidth(kMinimumBarSize);
      }
      if (rect.height() < kMinimumBarSize) {
        rect.setHeight(kMinimumBarSize);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createBarMonitorElement(rect);
      break;
    case CreateTool::kByteMonitor:
      if (rect.width() < kMinimumByteSize) {
        rect.setWidth(kMinimumByteSize);
      }
      if (rect.height() < kMinimumByteSize) {
        rect.setHeight(kMinimumByteSize);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createByteMonitorElement(rect);
      break;
    case CreateTool::kScaleMonitor:
      if (rect.width() < kMinimumScaleSize) {
        rect.setWidth(kMinimumScaleSize);
      }
      if (rect.height() < kMinimumScaleSize) {
        rect.setHeight(kMinimumScaleSize);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createScaleMonitorElement(rect);
      break;
    case CreateTool::kStripChart:
      if (rect.width() < kMinimumStripChartWidth) {
        rect.setWidth(kMinimumStripChartWidth);
      }
      if (rect.height() < kMinimumStripChartHeight) {
        rect.setHeight(kMinimumStripChartHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createStripChartElement(rect);
      break;
    case CreateTool::kCartesianPlot:
      if (rect.width() < kMinimumCartesianPlotWidth) {
        rect.setWidth(kMinimumCartesianPlotWidth);
      }
      if (rect.height() < kMinimumCartesianPlotHeight) {
        rect.setHeight(kMinimumCartesianPlotHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createCartesianPlotElement(rect);
      break;
    case CreateTool::kRectangle:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createRectangleElement(rect);
      break;
    case CreateTool::kOval:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createOvalElement(rect);
      break;
    case CreateTool::kArc:
      if (rect.width() <= 0) {
        rect.setWidth(1);
      }
      if (rect.height() <= 0) {
        rect.setHeight(1);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
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
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
      createImageElement(rect);
      break;
    case CreateTool::kRelatedDisplay:
      if (rect.width() < kMinimumTextWidth) {
        rect.setWidth(kMinimumTextWidth);
      }
      if (rect.height() < kMinimumTextHeight) {
        rect.setHeight(kMinimumTextHeight);
      }
      rect = snapRectOriginToGrid(adjustRectToDisplayArea(rect));
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
        : snapPointToGrid(clampToDisplayArea(areaPos));

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
        : snapPointToGrid(clampToDisplayArea(areaPos));

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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!polygonRuntimes_.contains(element)) {
        auto *runtime = new PolygonRuntime(element);
        polygonRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!polylineRuntimes_.contains(element)) {
        auto *runtime = new PolylineRuntime(element);
        polylineRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
      return snapPointToGrid(clamped);
    }

    const QPoint &reference = points.last();
    const int dx = clamped.x() - reference.x();
    const int dy = clamped.y() - reference.y();
    if (dx == 0 && dy == 0) {
      return snapPointToGrid(clamped);
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
    return snapPointToGrid(clampToDisplayArea(QPoint(x, y)));
  }

  bool beginVertexEdit(const QPoint &windowPos, Qt::KeyboardModifiers modifiers)
  {
    Q_UNUSED(modifiers);
    if (!displayArea_ || vertexEditMode_ != VertexEditMode::kNone) {
      return false;
    }

    auto state = state_.lock();
    if (!state || !state->editMode || state->createTool != CreateTool::kNone) {
      return false;
    }

    if (!multiSelection_.isEmpty()) {
      return false;
    }

    const QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    if (!displayArea_->rect().contains(areaPos)) {
      return false;
    }

    if (selectedPolygon_ && beginPolygonVertexEdit(areaPos)) {
      return true;
    }

    if (selectedPolyline_ && beginPolylineVertexEdit(areaPos)) {
      return true;
    }

    return false;
  }

  bool beginPolygonVertexEdit(const QPoint &areaPos)
  {
    PolygonElement *polygon = selectedPolygon_;
    if (!polygon) {
      return false;
    }

    const QVector<QPoint> points = polygon->absolutePoints();
    if (points.size() < 2) {
      return false;
    }

    const int hitIndex = hitTestVertex(points, areaPos);
    if (hitIndex < 0) {
      return false;
    }

    const int index = canonicalPolygonVertexIndex(points, hitIndex);
    startPolygonVertexEdit(polygon, index, points);
    return true;
  }

  bool beginPolylineVertexEdit(const QPoint &areaPos)
  {
    PolylineElement *polyline = selectedPolyline_;
    if (!polyline) {
      return false;
    }

    const QVector<QPoint> points = polyline->absolutePoints();
    if (points.size() < 2) {
      return false;
    }

    const int hitIndex = hitTestVertex(points, areaPos);
    if (hitIndex < 0) {
      return false;
    }

    startPolylineVertexEdit(polyline, hitIndex, points);
    return true;
  }

  void startPolygonVertexEdit(PolygonElement *polygon, int index,
      const QVector<QPoint> &points)
  {
    if (!polygon || points.isEmpty()) {
      return;
    }

    const int maxIndex = std::max(0, static_cast<int>(points.size()) - 1);
    vertexEditMode_ = VertexEditMode::kPolygon;
    vertexEditPolygon_ = polygon;
    vertexEditPolyline_ = nullptr;
    vertexEditIndex_ = std::clamp(index, 0, maxIndex);
    vertexEditInitialPoints_ = points;
    vertexEditCurrentPoints_ = points;
    vertexEditMoved_ = false;
  }

  void startPolylineVertexEdit(PolylineElement *polyline, int index,
      const QVector<QPoint> &points)
  {
    if (!polyline || points.isEmpty()) {
      return;
    }

    const int maxIndex = std::max(0, static_cast<int>(points.size()) - 1);
    vertexEditMode_ = VertexEditMode::kPolyline;
    vertexEditPolyline_ = polyline;
    vertexEditPolygon_ = nullptr;
    vertexEditIndex_ = std::clamp(index, 0, maxIndex);
    vertexEditInitialPoints_ = points;
    vertexEditCurrentPoints_ = points;
    vertexEditMoved_ = false;
  }

  int hitTestVertex(const QVector<QPoint> &points,
      const QPoint &areaPos) const
  {
    if (points.isEmpty()) {
      return -1;
    }
    const int radiusSquared = kVertexHitRadius * kVertexHitRadius;
    for (int i = 0; i < points.size(); ++i) {
      const QPoint delta = points.at(i) - areaPos;
      if (delta.x() * delta.x() + delta.y() * delta.y() <= radiusSquared) {
        return i;
      }
    }
    return -1;
  }

  int canonicalPolygonVertexIndex(const QVector<QPoint> &points,
      int index) const
  {
    if (points.size() >= 2 && index == points.size() - 1
        && points.first() == points.last()) {
      return 0;
    }
    return index;
  }

  int previousVertexIndex(const QVector<QPoint> &points, int index,
      bool closed) const
  {
    if (points.isEmpty()) {
      return -1;
    }
    if (index > 0) {
      return index - 1;
    }
    if (closed) {
      return points.size() >= 2 ? points.size() - 2 : 0;
    }
    return points.size() - 1;
  }

  QPoint adjustedVertexPoint(const QVector<QPoint> &points, int index,
      const QPoint &areaPos, Qt::KeyboardModifiers modifiers,
      bool closed) const
  {
    QPoint clamped = clampToDisplayArea(areaPos);
    if (!(modifiers & Qt::ShiftModifier) || points.isEmpty()
        || index < 0 || index >= points.size()) {
      return snapPointToGrid(clamped);
    }

    const int referenceIndex = previousVertexIndex(points, index, closed);
    if (referenceIndex < 0 || referenceIndex >= points.size()) {
      return snapPointToGrid(clamped);
    }

    const QPoint &reference = points.at(referenceIndex);
    const int dx = clamped.x() - reference.x();
    const int dy = clamped.y() - reference.y();
    if (dx == 0 && dy == 0) {
      return snapPointToGrid(clamped);
    }

    constexpr double kPi = 3.14159265358979323846;
    double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle < 0.0) {
      angle += 2.0 * kPi;
    }
    const double step = kPi / 4.0;
    const double snapped = std::round(angle / step) * step;
    const double length = std::sqrt(static_cast<double>(dx * dx + dy * dy));
    const int x = reference.x()
        + static_cast<int>(std::round(std::cos(snapped) * length));
    const int y = reference.y()
        + static_cast<int>(std::round(std::sin(snapped) * length));
    return snapPointToGrid(clampToDisplayArea(QPoint(x, y)));
  }

  void updateVertexEdit(const QPoint &windowPos,
      Qt::KeyboardModifiers modifiers)
  {
    if (vertexEditMode_ == VertexEditMode::kNone || !displayArea_
        || vertexEditIndex_ < 0) {
      return;
    }

    if (vertexEditCurrentPoints_.isEmpty()
        || vertexEditIndex_ >= vertexEditCurrentPoints_.size()) {
      return;
    }

    const QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    const bool closed = vertexEditMode_ == VertexEditMode::kPolygon;
    const QPoint newPoint = adjustedVertexPoint(vertexEditCurrentPoints_,
        vertexEditIndex_, areaPos, modifiers, closed);
    if (vertexEditCurrentPoints_.value(vertexEditIndex_) == newPoint) {
      return;
    }

    QVector<QPoint> updated = vertexEditCurrentPoints_;
    updated[vertexEditIndex_] = newPoint;

    if (vertexEditMode_ == VertexEditMode::kPolygon && !updated.isEmpty()) {
      const int lastIndex = updated.size() - 1;
      if (vertexEditIndex_ == 0 && lastIndex > 0) {
        updated[lastIndex] = newPoint;
      } else if (vertexEditIndex_ == lastIndex && lastIndex > 0) {
        updated[0] = newPoint;
      }
    }

    switch (vertexEditMode_) {
    case VertexEditMode::kPolygon:
      if (vertexEditPolygon_) {
        vertexEditPolygon_->setAbsolutePoints(updated);
        vertexEditCurrentPoints_ = vertexEditPolygon_->absolutePoints();
      }
      break;
    case VertexEditMode::kPolyline:
      if (vertexEditPolyline_) {
        vertexEditPolyline_->setAbsolutePoints(updated);
        vertexEditCurrentPoints_ = vertexEditPolyline_->absolutePoints();
      }
      break;
    case VertexEditMode::kNone:
      break;
    }

    vertexEditMoved_ = true;
  }

  void restoreInitialVertexPoints()
  {
    switch (vertexEditMode_) {
    case VertexEditMode::kPolygon:
      if (vertexEditPolygon_ && !vertexEditInitialPoints_.isEmpty()) {
        vertexEditPolygon_->setAbsolutePoints(vertexEditInitialPoints_);
        vertexEditCurrentPoints_ = vertexEditPolygon_->absolutePoints();
      }
      break;
    case VertexEditMode::kPolyline:
      if (vertexEditPolyline_ && !vertexEditInitialPoints_.isEmpty()) {
        vertexEditPolyline_->setAbsolutePoints(vertexEditInitialPoints_);
        vertexEditCurrentPoints_ = vertexEditPolyline_->absolutePoints();
      }
      break;
    case VertexEditMode::kNone:
      break;
    }
  }

  void resetVertexEditState()
  {
    vertexEditMode_ = VertexEditMode::kNone;
    vertexEditPolygon_ = nullptr;
    vertexEditPolyline_ = nullptr;
    vertexEditIndex_ = -1;
    vertexEditInitialPoints_.clear();
    vertexEditCurrentPoints_.clear();
    vertexEditMoved_ = false;
  }

  void finishActiveVertexEdit(bool applyChanges)
  {
    if (vertexEditMode_ == VertexEditMode::kNone) {
      return;
    }

    if (!applyChanges) {
      restoreInitialVertexPoints();
    }

    if (applyChanges && vertexEditMoved_) {
      setNextUndoLabel(QStringLiteral("Edit Vertices"));
      markDirty();
      refreshResourcePaletteGeometry();
      notifyMenus();
    }

    resetVertexEditState();
  }

  void finishActiveVertexEditFor(QWidget *widget, bool applyChanges)
  {
    if (!widget) {
      return;
    }
    if ((vertexEditMode_ == VertexEditMode::kPolygon
            && widget == vertexEditPolygon_)
        || (vertexEditMode_ == VertexEditMode::kPolyline
            && widget == vertexEditPolyline_)) {
      finishActiveVertexEdit(applyChanges);
    }
  }

  void createTextElement(const QRect &rect)
  {
    if (!displayArea_) {
      return;
    }
    setNextUndoLabel(QStringLiteral("Create Text"));
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!textRuntimes_.contains(element)) {
        auto *runtime = new TextRuntime(element);
        textRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
    setNextUndoLabel(QStringLiteral("Create Text Monitor"));
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
    setNextUndoLabel(QStringLiteral("Create Text Entry"));
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
    setNextUndoLabel(QStringLiteral("Create Slider"));
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
    setNextUndoLabel(QStringLiteral("Create Wheel Switch"));
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
    setNextUndoLabel(QStringLiteral("Create Choice Button"));
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
    setNextUndoLabel(QStringLiteral("Create Menu"));
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
    setNextUndoLabel(QStringLiteral("Create Message Button"));
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
    setNextUndoLabel(QStringLiteral("Create Shell Command"));
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
    connectShellCommandElement(element);
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
    setNextUndoLabel(QStringLiteral("Create Related Display"));
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
    connectRelatedDisplayElement(element);
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
    setNextUndoLabel(QStringLiteral("Create Meter"));
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
    setNextUndoLabel(QStringLiteral("Create Bar Monitor"));
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
    setNextUndoLabel(QStringLiteral("Create Scale Monitor"));
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
    setNextUndoLabel(QStringLiteral("Create Strip Chart"));
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
    setNextUndoLabel(QStringLiteral("Create Cartesian Plot"));
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
    setNextUndoLabel(QStringLiteral("Create Byte Monitor"));
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
    setNextUndoLabel(QStringLiteral("Create Rectangle"));
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!rectangleRuntimes_.contains(element)) {
        auto *runtime = new RectangleRuntime(element);
        rectangleRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
    setNextUndoLabel(QStringLiteral("Create Image"));
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!imageRuntimes_.contains(element)) {
        auto *runtime = new ImageRuntime(element);
        imageRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
    setNextUndoLabel(QStringLiteral("Create Oval"));
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!ovalRuntimes_.contains(element)) {
        auto *runtime = new OvalRuntime(element);
        ovalRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
    setNextUndoLabel(QStringLiteral("Create Arc"));
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!arcRuntimes_.contains(element)) {
        auto *runtime = new ArcRuntime(element);
        arcRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!lineRuntimes_.contains(element)) {
        auto *runtime = new LineRuntime(element);
        lineRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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

  QPoint snapPointToGrid(const QPoint &point) const
  {
    if (!snapToGrid_ || gridSpacing_ <= 0 || !displayArea_) {
      return point;
    }
    const QRect areaRect = displayArea_->rect();
    const int spacing = gridSpacing_;
    const int minX = areaRect.left();
    const int minY = areaRect.top();
    const int maxX = areaRect.right();
    const int maxY = areaRect.bottom();
    auto snapCoord = [&](int value, int minCoord, int maxCoord) {
      if (maxCoord < minCoord) {
        return minCoord;
      }
      const double units = static_cast<double>(value - minCoord) / spacing;
      int roundedUnits = static_cast<int>(std::round(units));
      const int maxUnits = static_cast<int>(
          std::floor(static_cast<double>(maxCoord - minCoord) / spacing));
      roundedUnits = std::clamp(roundedUnits, 0, maxUnits);
      return minCoord + roundedUnits * spacing;
    };
    const int clampedX = std::clamp(point.x(), minX, maxX);
    const int clampedY = std::clamp(point.y(), minY, maxY);
    return QPoint(
        snapCoord(clampedX, minX, maxX),
        snapCoord(clampedY, minY, maxY));
  }

  QPoint snapTopLeftToGrid(const QPoint &topLeft, const QSize &size) const
  {
    if (!snapToGrid_ || gridSpacing_ <= 0 || !displayArea_) {
      return topLeft;
    }
    const QRect areaRect = displayArea_->rect();
    const int spacing = gridSpacing_;
    const int minX = areaRect.left();
    const int minY = areaRect.top();
    const int maxX = areaRect.right() - size.width() + 1;
    const int maxY = areaRect.bottom() - size.height() + 1;
    auto snapCoord = [&](int value, int minCoord, int maxCoord) {
      if (maxCoord < minCoord) {
        return minCoord;
      }
      const double units = static_cast<double>(value - minCoord) / spacing;
      int roundedUnits = static_cast<int>(std::round(units));
      const int maxUnits = static_cast<int>(
          std::floor(static_cast<double>(maxCoord - minCoord) / spacing));
      roundedUnits = std::clamp(roundedUnits, 0, maxUnits);
      return minCoord + roundedUnits * spacing;
    };
    const int clampedX = std::clamp(topLeft.x(), minX,
        std::max(minX, maxX));
    const int clampedY = std::clamp(topLeft.y(), minY,
        std::max(minY, maxY));
    return QPoint(
        snapCoord(clampedX, minX, maxX),
        snapCoord(clampedY, minY, maxY));
  }

  QPoint snapOffsetToGrid(const QRect &rect,
      const QPoint &offset) const
  {
    if (!snapToGrid_ || gridSpacing_ <= 0 || !displayArea_) {
      return offset;
    }
    const QPoint base = rect.topLeft();
    const QPoint desired = base + offset;
    const QPoint snapped = snapTopLeftToGrid(desired, rect.size());
    return snapped - base;
  }

  QRect snapRectOriginToGrid(const QRect &rect) const
  {
    if (!snapToGrid_ || gridSpacing_ <= 0 || !displayArea_) {
      return rect;
    }
    QRect adjusted = rect;
    adjusted.moveTopLeft(snapTopLeftToGrid(adjusted.topLeft(),
        adjusted.size()));
    return adjusted;
  }

  QRect translateRectForPaste(const QRect &rect,
      const QPoint &offset) const
  {
    QRect translated = rect.translated(offset);
    translated = adjustRectToDisplayArea(translated);
    return snapRectOriginToGrid(translated);
  }

  QVector<QPoint> translatePointsForPaste(const QVector<QPoint> &points,
      const QPoint &offset) const
  {
    QVector<QPoint> translated = points;
    if (translated.isEmpty()) {
      return translated;
    }
    for (QPoint &pt : translated) {
      pt += offset;
    }
    if (!snapToGrid_ || gridSpacing_ <= 0 || !displayArea_) {
      return translated;
    }
    QRect bounding(translated.first(), translated.first());
    bounding = bounding.normalized();
    for (const QPoint &pt : translated) {
      bounding = bounding.united(QRect(pt, pt));
    }
    const QPoint snappedTopLeft =
        snapTopLeftToGrid(bounding.topLeft(), bounding.size());
    const QPoint delta = snappedTopLeft - bounding.topLeft();
    for (QPoint &pt : translated) {
      pt += delta;
    }
    return translated;
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
        displayArea_->setCursor(CursorUtils::crossCursor());
      } else {
        displayArea_->unsetCursor();
      }
    }
    if (crossCursorActive) {
      setCursor(CursorUtils::crossCursor());
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
      notifyMenus();
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
    notifyMenus();
  }

  void ensureExecuteContextMenuEntriesLoaded()
  {
    if (executeContextMenuInitialized_) {
      return;
    }

    executeContextMenuInitialized_ = true;
    executeCascadeAvailable_ = false;
    executeMenuEntries_.clear();

    const QByteArray rawList = qgetenv("MEDM_EXEC_LIST");
    if (rawList.isEmpty()) {
      return;
    }

    const QString list = QString::fromLocal8Bit(rawList.constData());
    QString currentItem;
    QVector<QString> items;
    items.reserve(list.count(QLatin1Char(':')) + 1);

    for (int i = 0; i < list.size(); ++i) {
      const QChar ch = list.at(i);
      if (ch == QLatin1Char(':')) {
        if ((i + 1) < list.size() && list.at(i + 1) == QLatin1Char('\\')) {
          currentItem.append(ch);
          continue;
        }
        if (!currentItem.isEmpty()) {
          items.append(currentItem);
          currentItem.clear();
        } else {
          currentItem.clear();
        }
        continue;
      }
      currentItem.append(ch);
    }

    if (!currentItem.isEmpty()) {
      items.append(currentItem);
    }

    for (const QString &item : items) {
      const int separator = item.indexOf(QLatin1Char(';'));
      if (separator <= 0 || separator >= item.size() - 1) {
        continue;
      }
      const QString label = item.left(separator).trimmed();
      const QString command = item.mid(separator + 1).trimmed();
      if (label.isEmpty() || command.isEmpty()) {
        continue;
      }
      executeMenuEntries_.push_back({label, command});
    }

    executeCascadeAvailable_ = !executeMenuEntries_.isEmpty();
  }

  void showExecuteContextMenu(const QPoint &globalPos)
  {
    ensureExecuteContextMenuEntriesLoaded();

    QMenu menu(this);
    menu.setObjectName(QStringLiteral("executeModeContextMenu"));
    menu.setSeparatorsCollapsible(false);

    // Mirror MEDM execute popup menu; actions will be wired up later.
    menu.addAction(QStringLiteral("Print"));
    QAction *closeAction = menu.addAction(QStringLiteral("Close"));
    QObject::connect(closeAction, &QAction::triggered, this,
        [this]() {
          setAsActiveDisplay();
          close();
        });
    QAction *pvInfoAction = menu.addAction(QStringLiteral("PV Info"));
    QObject::connect(pvInfoAction, &QAction::triggered, this,
        [this]() {
          setAsActiveDisplay();
          startPvInfoPickMode();
        });
    menu.addAction(QStringLiteral("PV Limits"));
    QAction *mainWindowAction =
      menu.addAction(QStringLiteral("QtEDM Main Window"));
    QObject::connect(mainWindowAction, &QAction::triggered, this,
        [this]() {
          focusMainWindow();
        });
    QAction *displayListAction =
      menu.addAction(QStringLiteral("Display List"));
    QObject::connect(displayListAction, &QAction::triggered, this,
        [this]() {
          showDisplayListDialog();
        });
    menu.addAction(QStringLiteral("Toggle Hidden Button Markers"));
    QAction *refreshAction =
      menu.addAction(QStringLiteral("Refresh"));
    QObject::connect(refreshAction, &QAction::triggered, this,
        [this]() {
          refreshDisplayView();
        });
    QAction *retryAction =
      menu.addAction(QStringLiteral("Retry Connections"));
    QObject::connect(retryAction, &QAction::triggered, this,
        [this]() {
          retryChannelConnections();
        });

    if (executeCascadeAvailable_) {
      QMenu *executeMenu = menu.addMenu(QStringLiteral("Execute"));
      for (const ExecuteMenuEntry &entry : executeMenuEntries_) {
        QAction *action = executeMenu->addAction(entry.label);
        action->setData(entry.command);
      }
    }

    menu.exec(globalPos);
  }

  void focusMainWindow() const
  {
    if (auto state = state_.lock()) {
      if (auto *window = state->mainWindow.data()) {
        if (window->isMinimized()) {
          window->showNormal();
        } else {
          window->show();
        }
        window->raise();
        window->activateWindow();
        window->setFocus(Qt::OtherFocusReason);
      }
    }
  }

  void showDisplayListDialog() const
  {
    if (auto state = state_.lock()) {
      if (auto *dialog = state->displayListDialog.data()) {
        dialog->showAndRaise();
      }
    }
  }

  PvInfoDialog *ensurePvInfoDialog()
  {
    if (!pvInfoDialog_) {
      auto *dialog = new PvInfoDialog(palette(), labelFont_, font(), this);
      pvInfoDialog_ = dialog;
    }
    return pvInfoDialog_.data();
  }

  bool attemptChannelRetry(const QString &channelName,
      QString &retriedChannel) const
  {
    const QString trimmed = channelName.trimmed();
    if (trimmed.isEmpty()) {
      return false;
    }

    QByteArray channelBytes = trimmed.toLatin1();
    if (channelBytes.isEmpty()) {
      return false;
    }

    chid retryChid = nullptr;
    int status = ca_search_and_connect(channelBytes.constData(), &retryChid,
        nullptr, nullptr);
    if (status != ECA_NORMAL) {
      qWarning() << "Retry Connections: ca_search_and_connect failed for"
                 << trimmed << ':' << ca_message(status);
      return false;
    }

    int pendStatus = ca_pend_io(kChannelRetryTimeoutSeconds);
    if (pendStatus != ECA_NORMAL) {
      qWarning() << "Retry Connections: ca_pend_io timed out for"
                 << trimmed << ':' << ca_message(pendStatus);
    }

    if (retryChid) {
      int clearStatus = ca_clear_channel(retryChid);
      if (clearStatus != ECA_NORMAL) {
        qWarning() << "Retry Connections: ca_clear_channel failed for"
                   << trimmed << ':' << ca_message(clearStatus);
      }
    }

    retriedChannel = trimmed;
    return true;
  }

  bool retryFirstUnconnectedChannel(QString &retriedChannel)
  {
    if (!executeModeActive_) {
      return false;
    }

    auto attemptWithAccessor = [&](const QString &name, bool connected) {
      if (name.trimmed().isEmpty() || connected) {
        return false;
      }
      return attemptChannelRetry(name, retriedChannel);
    };

    auto attemptRuntimeChannels = [&](const auto &runtimeMap) {
      for (auto it = runtimeMap.cbegin(); it != runtimeMap.cend(); ++it) {
        auto *runtime = it.value();
        if (!runtime || !runtime->started_) {
          continue;
        }
        for (const auto &channel : runtime->channels_) {
          if (attemptWithAccessor(channel.name, channel.connected)) {
            return true;
          }
        }
      }
      return false;
    };

    if (attemptRuntimeChannels(textRuntimes_) ||
        attemptRuntimeChannels(rectangleRuntimes_) ||
        attemptRuntimeChannels(imageRuntimes_) ||
        attemptRuntimeChannels(ovalRuntimes_) ||
        attemptRuntimeChannels(arcRuntimes_) ||
        attemptRuntimeChannels(lineRuntimes_) ||
        attemptRuntimeChannels(polylineRuntimes_) ||
        attemptRuntimeChannels(polygonRuntimes_)) {
      return true;
    }

    auto attemptSingleChannelRuntime = [&](const auto &runtimeMap) {
      for (auto it = runtimeMap.cbegin(); it != runtimeMap.cend(); ++it) {
        auto *runtime = it.value();
        if (!runtime || !runtime->started_) {
          continue;
        }
        if (attemptWithAccessor(runtime->channelName_, runtime->connected_)) {
          return true;
        }
      }
      return false;
    };

    if (attemptSingleChannelRuntime(textEntryRuntimes_) ||
        attemptSingleChannelRuntime(sliderRuntimes_) ||
        attemptSingleChannelRuntime(wheelSwitchRuntimes_) ||
        attemptSingleChannelRuntime(choiceButtonRuntimes_) ||
        attemptSingleChannelRuntime(menuRuntimes_) ||
        attemptSingleChannelRuntime(messageButtonRuntimes_) ||
        attemptSingleChannelRuntime(textMonitorRuntimes_) ||
        attemptSingleChannelRuntime(meterRuntimes_) ||
        attemptSingleChannelRuntime(barMonitorRuntimes_) ||
        attemptSingleChannelRuntime(scaleMonitorRuntimes_) ||
        attemptSingleChannelRuntime(byteMonitorRuntimes_)) {
      return true;
    }

    for (auto it = stripChartRuntimes_.cbegin();
         it != stripChartRuntimes_.cend(); ++it) {
      auto *runtime = it.value();
      if (!runtime || !runtime->started_) {
        continue;
      }
      for (const auto &pen : runtime->pens_) {
        if (attemptWithAccessor(pen.channelName, pen.connected)) {
          return true;
        }
      }
    }

    for (auto it = cartesianPlotRuntimes_.cbegin();
         it != cartesianPlotRuntimes_.cend(); ++it) {
      auto *runtime = it.value();
      if (!runtime || !runtime->started_) {
        continue;
      }
      for (const auto &trace : runtime->traces_) {
        if (attemptWithAccessor(trace.x.name, trace.x.connected) ||
            attemptWithAccessor(trace.y.name, trace.y.connected)) {
          return true;
        }
      }
      if (attemptWithAccessor(runtime->triggerChannel_.name,
              runtime->triggerChannel_.connected) ||
          attemptWithAccessor(runtime->eraseChannel_.name,
              runtime->eraseChannel_.connected) ||
          attemptWithAccessor(runtime->countChannel_.name,
              runtime->countChannel_.connected)) {
        return true;
      }
    }

    return false;
  }

  void retryChannelConnections()
  {
    ChannelAccessContext &context = ChannelAccessContext::instance();
    context.ensureInitialized();
    if (!context.isInitialized()) {
      qWarning() << "Retry Connections: Channel Access context unavailable";
      return;
    }

    const auto channelCounts = StatisticsTracker::instance().channelCounts();
    const int totalChannels = channelCounts.first;
    const int connectedChannels = channelCounts.second;

    QString retriedChannel;
    DisplayWindow *targetDisplay = nullptr;

    if (retryFirstUnconnectedChannel(retriedChannel)) {
      targetDisplay = this;
    } else if (auto state = state_.lock()) {
      const auto displays = state->displays;
      for (const auto &displayPtr : displays) {
        DisplayWindow *display = displayPtr.data();
        if (!display || display == this || !display->executeModeActive_) {
          continue;
        }
        if (display->retryFirstUnconnectedChannel(retriedChannel)) {
          targetDisplay = display;
          break;
        }
      }
    }

    if (!targetDisplay) {
      if (totalChannels > 0 && totalChannels == connectedChannels) {
        QApplication::beep();
        qInfo() << "Retry Connections: all channels are connected";
      } else if (totalChannels == 0) {
        qInfo() << "Retry Connections: no active channels to retry";
      } else {
        qWarning() << "Retry Connections: no unresolved channels found for retry";
      }
      return;
    }

    qInfo() << "Retry Connections: retried search for" << retriedChannel;

    if (targetDisplay == this) {
      refreshDisplayView();
    } else if (targetDisplay->displayArea_) {
      targetDisplay->displayArea_->update();
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

    QString undoLabel = QStringLiteral("Undo");
    bool canUndo = false;
    if (undoStack_) {
      if (undoStack_->canUndo()) {
        canUndo = true;
        const QString stackText = undoStack_->undoText();
        if (!stackText.isEmpty()) {
          undoLabel = QStringLiteral("Undo %1").arg(stackText);
        }
      }
    }
    auto *undoAction =
        addMenuAction(&menu, undoLabel, QKeySequence::Undo);
    undoAction->setEnabled(canUndo);
    QObject::connect(undoAction, &QAction::triggered, this, [this]() {
      if (!undoStack_) {
        return;
      }
      setAsActiveDisplay();
      if (undoStack_->canUndo()) {
        undoStack_->undo();
      }
    });

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
    auto *alignLeftAction =
        addMenuAction(alignMenu, QStringLiteral("Left"));
    alignLeftAction->setEnabled(canAlignSelection());
    QObject::connect(alignLeftAction, &QAction::triggered, this, [this]() {
      alignSelectionLeft();
    });
    auto *alignHorizontalCenterAction =
        addMenuAction(alignMenu, QStringLiteral("Horizontal Center"));
    alignHorizontalCenterAction->setEnabled(canAlignSelection());
    QObject::connect(alignHorizontalCenterAction, &QAction::triggered, this,
        [this]() {
          alignSelectionHorizontalCenter();
        });
    auto *alignRightAction =
        addMenuAction(alignMenu, QStringLiteral("Right"));
    alignRightAction->setEnabled(canAlignSelection());
    QObject::connect(alignRightAction, &QAction::triggered, this, [this]() {
      alignSelectionRight();
    });
    auto *alignTopAction = addMenuAction(alignMenu, QStringLiteral("Top"));
    alignTopAction->setEnabled(canAlignSelection());
    QObject::connect(alignTopAction, &QAction::triggered, this, [this]() {
      alignSelectionTop();
    });
    auto *alignVerticalCenterAction =
        addMenuAction(alignMenu, QStringLiteral("Vertical Center"));
    alignVerticalCenterAction->setEnabled(canAlignSelection());
    QObject::connect(alignVerticalCenterAction, &QAction::triggered, this,
        [this]() {
          alignSelectionVerticalCenter();
        });
    auto *alignBottomAction =
        addMenuAction(alignMenu, QStringLiteral("Bottom"));
    alignBottomAction->setEnabled(canAlignSelection());
    QObject::connect(alignBottomAction, &QAction::triggered, this, [this]() {
      alignSelectionBottom();
    });
    auto *positionToGridAction =
        addMenuAction(alignMenu, QStringLiteral("Position to Grid"));
    positionToGridAction->setEnabled(canAlignSelectionToGrid());
    QObject::connect(positionToGridAction, &QAction::triggered, this,
        [this]() {
          alignSelectionPositionToGrid();
        });
    auto *edgesToGridAction =
        addMenuAction(alignMenu, QStringLiteral("Edges to Grid"));
    edgesToGridAction->setEnabled(canAlignSelectionToGrid());
    QObject::connect(edgesToGridAction, &QAction::triggered, this, [this]() {
      alignSelectionEdgesToGrid();
    });

    auto *spaceMenu = menu.addMenu(QStringLiteral("Space Evenly"));
    auto *spaceHorizontalAction =
        addMenuAction(spaceMenu, QStringLiteral("Horizontal"));
    spaceHorizontalAction->setEnabled(canSpaceSelection());
    QObject::connect(spaceHorizontalAction, &QAction::triggered, this,
        [this]() {
          spaceSelectionHorizontal();
        });
    auto *spaceVerticalAction =
        addMenuAction(spaceMenu, QStringLiteral("Vertical"));
    spaceVerticalAction->setEnabled(canSpaceSelection());
    QObject::connect(spaceVerticalAction, &QAction::triggered, this, [this]() {
      spaceSelectionVertical();
    });
    auto *space2DAction =
        addMenuAction(spaceMenu, QStringLiteral("2-D"));
    space2DAction->setEnabled(canSpaceSelection2D());
    QObject::connect(space2DAction, &QAction::triggered, this, [this]() {
      spaceSelection2D();
    });

    auto *centerMenu = menu.addMenu(QStringLiteral("Center"));
    auto *centerHorizontalAction =
        addMenuAction(centerMenu, QStringLiteral("Horizontally in Display"));
    centerHorizontalAction->setEnabled(canCenterSelection());
    QObject::connect(centerHorizontalAction, &QAction::triggered, this,
        [this]() {
          centerSelectionHorizontallyInDisplay();
        });
    auto *centerVerticalAction =
        addMenuAction(centerMenu, QStringLiteral("Vertically in Display"));
    centerVerticalAction->setEnabled(canCenterSelection());
    QObject::connect(centerVerticalAction, &QAction::triggered, this,
        [this]() {
          centerSelectionVerticallyInDisplay();
        });
    auto *centerBothAction =
        addMenuAction(centerMenu, QStringLiteral("Both"));
    centerBothAction->setEnabled(canCenterSelection());
    QObject::connect(centerBothAction, &QAction::triggered, this, [this]() {
      centerSelectionInDisplayBoth();
    });

    auto *orientMenu = menu.addMenu(QStringLiteral("Orient"));
    auto *flipHorizontalAction =
        addMenuAction(orientMenu, QStringLiteral("Flip Horizontally"));
    flipHorizontalAction->setEnabled(canOrientSelection());
    QObject::connect(flipHorizontalAction, &QAction::triggered, this,
        [this]() {
          orientSelectionFlipHorizontal();
        });
    auto *flipVerticalAction =
        addMenuAction(orientMenu, QStringLiteral("Flip Vertically"));
    flipVerticalAction->setEnabled(canOrientSelection());
    QObject::connect(flipVerticalAction, &QAction::triggered, this,
        [this]() {
          orientSelectionFlipVertical();
        });
    auto *rotateClockwiseAction =
        addMenuAction(orientMenu, QStringLiteral("Rotate Clockwise"));
    rotateClockwiseAction->setEnabled(canOrientSelection());
    QObject::connect(rotateClockwiseAction, &QAction::triggered, this,
        [this]() {
          rotateSelectionClockwise();
        });
    auto *rotateCounterclockwiseAction =
        addMenuAction(orientMenu, QStringLiteral("Rotate Counterclockwise"));
    rotateCounterclockwiseAction->setEnabled(canOrientSelection());
    QObject::connect(rotateCounterclockwiseAction, &QAction::triggered, this,
        [this]() {
          rotateSelectionCounterclockwise();
        });

    auto *sizeMenu = menu.addMenu(QStringLiteral("Size"));
    auto *sameSizeAction =
        addMenuAction(sizeMenu, QStringLiteral("Same Size"));
    sameSizeAction->setEnabled(canSizeSelectionSameSize());
    QObject::connect(sameSizeAction, &QAction::triggered, this, [this]() {
      sizeSelectionSameSize();
    });
    auto *textToContentsAction =
        addMenuAction(sizeMenu, QStringLiteral("Text to Contents"));
    textToContentsAction->setEnabled(canSizeSelectionTextToContents());
    QObject::connect(textToContentsAction, &QAction::triggered, this,
        [this]() {
          sizeSelectionTextToContents();
        });

    auto *gridMenu = menu.addMenu(QStringLiteral("Grid"));
    auto *toggleShowGridAction =
        addMenuAction(gridMenu, QStringLiteral("Toggle Show Grid"));
    QObject::connect(toggleShowGridAction, &QAction::triggered, this,
        [this]() {
          setAsActiveDisplay();
          setGridOn(!isGridOn());
        });
    auto *toggleSnapAction =
        addMenuAction(gridMenu, QStringLiteral("Toggle Snap To Grid"));
    QObject::connect(toggleSnapAction, &QAction::triggered, this, [this]() {
      setAsActiveDisplay();
      setSnapToGrid(!isSnapToGridEnabled());
    });
    auto *gridSpacingAction =
        addMenuAction(gridMenu, QStringLiteral("Grid Spacing..."));
    QObject::connect(gridSpacingAction, &QAction::triggered, this, [this]() {
      setAsActiveDisplay();
      promptForGridSpacing();
    });

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
    auto *findOutliersAction =
        addMenuAction(&menu, QStringLiteral("Find Outliers"));
    QObject::connect(findOutliersAction, &QAction::triggered, this,
        [this]() {
      findOutliers();
    });
    auto *refreshAction = addMenuAction(&menu, QStringLiteral("Refresh"));
    QObject::connect(refreshAction, &QAction::triggered, this, [this]() {
      refreshDisplayView();
    });
    auto *editSummaryAction =
        addMenuAction(&menu, QStringLiteral("Edit Summary..."));
    QObject::connect(editSummaryAction, &QAction::triggered, this, [this]() {
      showEditSummaryDialog();
    });

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
  cleanStateSnapshot_ = serializeStateForUndo(filePath_);
  lastCommittedState_ = cleanStateSnapshot_;
  if (undoStack_) {
    const bool previousSuppress = suppressUndoCapture_;
    suppressUndoCapture_ = true;
    undoStack_->setClean();
    suppressUndoCapture_ = previousSuppress;
  }
  updateDirtyFromUndoStack();
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
  cleanStateSnapshot_ = serializeStateForUndo(filePath_);
  lastCommittedState_ = cleanStateSnapshot_;
  if (undoStack_) {
    const bool previousSuppress = suppressUndoCapture_;
    suppressUndoCapture_ = true;
    undoStack_->setClean();
    suppressUndoCapture_ = previousSuppress;
  }
  updateDirtyFromUndoStack();
  notifyMenus();
  return true;
}

inline bool DisplayWindow::loadFromFile(const QString &filePath,
    QString *errorMessage, const QHash<QString, QString> &macros)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to open %1").arg(filePath);
    }
    return false;
  }

  QTextStream stream(&file);
  setLatin1Encoding(stream);
  const QString contents = stream.readAll();

  /* Detect file version */
  int fileVersion = 30122; /* Default to current version */
  QRegularExpression versionPattern(QStringLiteral(R"(version\s*=\s*(\d+))"));
  QRegularExpressionMatch versionMatch = versionPattern.match(contents);
  if (versionMatch.hasMatch()) {
    bool ok = false;
    int parsedVersion = versionMatch.captured(1).toInt(&ok);
    if (ok) {
      fileVersion = parsedVersion;
    }
  }

  /* Convert legacy format if needed */
  QString adlContent = contents;
  if (fileVersion < 20200) {
    adlContent = convertLegacyAdlFormat(contents, fileVersion);
    /* Debug: write converted content to /tmp/qtedm_converted.adl */
    QFile debugFile(QStringLiteral("/tmp/qtedm_converted.adl"));
    if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream debugStream(&debugFile);
      debugStream << adlContent;
      debugFile.close();
    }
  }

  const QString processedContents = applyMacroSubstitutions(adlContent, macros);

  //qWarning() << "Parsing ADL file with" << processedContents.size() << "characters...";
  std::optional<AdlNode> document = AdlParser::parse(processedContents, errorMessage);
  if (!document) {
    return false;
  }
  //qWarning() << "ADL parsing complete, loading" << document->children.size() << "top-level nodes...";

  clearAllElements();

  /* Set restoringState_ to defer per-element stacking order updates during bulk load */
  restoringState_ = true;

  const QString previousLoadDirectory = currentLoadDirectory_;
  currentLoadDirectory_ = QFileInfo(filePath).absolutePath();

  pendingBasicAttribute_ = std::nullopt;
  pendingDynamicAttribute_ = std::nullopt;
  bool displayLoaded = false;
  bool elementLoaded = false;
  int elementCount = 0;
  for (const auto &child : document->children) {
    if (child.name.compare(QStringLiteral("display"), Qt::CaseInsensitive)
        == 0) {
      displayLoaded = loadDisplaySection(child) || displayLoaded;
      continue;
    }
    /* Check for standalone "basic attribute" node */
    if (child.name.compare(QStringLiteral("basic attribute"),
        Qt::CaseInsensitive) == 0) {
      pendingBasicAttribute_ = child;
      continue;
    }
    /* Check for standalone "dynamic attribute" node */
    if (child.name.compare(QStringLiteral("dynamic attribute"),
        Qt::CaseInsensitive) == 0) {
      pendingDynamicAttribute_ = child;
      continue;
    }
    if (loadElementNode(child)) {
      elementLoaded = true;
      elementCount++;
      if (elementCount % 1000 == 0) {
        //qWarning() << "Loaded" << elementCount << "elements...";
      }
      continue;
    }
  }
  //qWarning() << "Finished loading" << elementCount << "elements";
  pendingBasicAttribute_ = std::nullopt;
  pendingDynamicAttribute_ = std::nullopt;

  /* Re-enable stacking order updates and refresh to ensure static graphics
   * are below interactive widgets regardless of ADL file ordering */
  restoringState_ = false;
  refreshStackingOrder();

  filePath_ = QFileInfo(filePath).absoluteFilePath();
  setWindowTitle(QFileInfo(filePath_).fileName());
  macroDefinitions_ = macros;

  dirty_ = false;
  if (undoStack_) {
    const bool previousSuppress = suppressUndoCapture_;
    suppressUndoCapture_ = true;
    undoStack_->clear();
    undoStack_->setClean();
    suppressUndoCapture_ = previousSuppress;
  }
  /* Defer undo snapshot for large files to avoid blocking the UI during load.
   * For files with many elements (>1000), create the snapshot asynchronously
   * using a zero-delay timer so the UI can display the loaded file first. */
  const int totalElements = elementStack_.size();
  if (totalElements < 1000) {
    cleanStateSnapshot_ = serializeStateForUndo(filePath_);
    lastCommittedState_ = cleanStateSnapshot_;
  } else {
    /* Use a timer to defer the snapshot creation until the event loop runs */
    QTimer::singleShot(0, this, [this, filePath]() {
      cleanStateSnapshot_ = serializeStateForUndo(filePath);
      lastCommittedState_ = cleanStateSnapshot_;
      //qWarning() << "Created deferred undo snapshot for file with" << elementStack_.size() << "elements";
    });
  }
  updateDirtyFromUndoStack();
  /* Defer UI updates for large files to avoid blocking during initial load */
  if (totalElements < 1000) {
    if (displayArea_) {
      displayArea_->update();
    }
    update();
  } else {
    /* Queue UI updates to run after the event loop processes */
    QTimer::singleShot(0, this, [this]() {
      if (displayArea_) {
        displayArea_->update();
      }
      update();
    });
  }
  if (auto state = state_.lock()) {
    state->createTool = CreateTool::kNone;
  }
  currentLoadDirectory_ = previousLoadDirectory;
  notifyMenus();
  return displayLoaded || elementLoaded;
}

inline QString DisplayWindow::applyMacroSubstitutions(const QString &input,
    const QHash<QString, QString> &macros)
{
  if (macros.isEmpty()) {
    return input;
  }

  static constexpr int kMaxIterations = 10;
  QRegularExpression pattern(QStringLiteral(R"(\$\(([^)]+)\))"));
  QString current = input;
  for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
    QString result;
    result.reserve(current.size());
    int lastIndex = 0;
    QRegularExpressionMatchIterator it = pattern.globalMatch(current);
    bool replaced = false;
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      result.append(current.mid(lastIndex, match.capturedStart() - lastIndex));
      const QString name = match.captured(1);
      const auto macroIt = macros.constFind(name);
      if (macroIt != macros.constEnd()) {
        result.append(*macroIt);
        replaced = true;
      } else {
        result.append(match.captured(0));
      }
      lastIndex = match.capturedEnd();
    }
    result.append(current.mid(lastIndex));
    if (!replaced) {
      return current;
    }
    current = result;
  }
  return current;
}

inline QString DisplayWindow::convertLegacyAdlFormat(const QString &adlText,
    int fileVersion) const
{
  /*
   * Convert old format (< 20200) to new format.
   * Old: "basic attribute" { attr { props } } THEN widget { object{...} }
   * New: widget { object{...} "basic attribute" { props } }
   *
   * Strategy from studying tests/legacy.adl:
   * 1. Standalone attribute blocks appear before widgets they apply to
   * 2. Extract content from inside attr{} blocks
   * 3. When we see a widget, buffer it entirely (tracking braces)
   * 4. Inject pending attributes after the widget's object{} sub-block closes
   */
  if (fileVersion >= 20200) {
    return adlText;
  }

  QStringList lines = adlText.split(QChar('\n'));
  QStringList result;
  QStringList pendingBasicAttr;    /* Properties from attr{} in basic attribute */
  QStringList pendingDynamicAttr;  /* Content from attr{} in dynamic attribute */
  
  enum State {
    Normal,
    InBasicAttr,         /* Inside "basic attribute" { ... } */
    InBasicAttrInner,    /* Inside attr { ... } within basic attribute */
    InDynamicAttr,       /* Inside "dynamic attribute" { ... } */
    InDynamicAttrInner,  /* Inside attr { ... } within dynamic attribute */
    InWidget,            /* Inside widget { ... } - buffering until we can inject */
    InWidgetObject       /* Inside object { ... } within widget */
  };
  State state = Normal;
  int braceDepth = 0;
  QStringList widgetBuffer;  /* Buffer lines of current widget */
  int widgetBraceDepth = 0;
  int objectBraceDepth = 0;

  for (const QString &line : lines) {
    QString trimmed = line.trimmed();
    
    switch (state) {
    case Normal:
      /* Detect standalone basic attribute block */
      if (trimmed.startsWith(QStringLiteral("\"basic attribute\""))) {
        state = InBasicAttr;
        braceDepth = 0;
        pendingBasicAttr.clear();
        if (trimmed.contains(QChar('{'))) {
          braceDepth++;
        }
        continue;
      }
      /* Detect standalone dynamic attribute block */
      if (trimmed.startsWith(QStringLiteral("\"dynamic attribute\""))) {
        state = InDynamicAttr;
        braceDepth = 0;
        pendingDynamicAttr.clear();
        if (trimmed.contains(QChar('{'))) {
          braceDepth++;
        }
        continue;
      }
      /* Detect widget that should receive pending attributes */
      {
        const bool isWidget = trimmed.startsWith(QStringLiteral("rectangle"))
            || trimmed.startsWith(QStringLiteral("oval"))
            || trimmed.startsWith(QStringLiteral("arc"))
            || trimmed.startsWith(QStringLiteral("polygon"))
            || trimmed.startsWith(QStringLiteral("polyline"))
            || (trimmed.startsWith(QStringLiteral("text")) && !trimmed.startsWith(QStringLiteral("textix")));
        
        if (isWidget && trimmed.endsWith(QChar('{'))) {
          /* Start buffering this widget */
          state = InWidget;
          widgetBuffer.clear();
          widgetBuffer.append(line);
          widgetBraceDepth = 1;
          continue;
        }
      }
      /* Normal line */
      result.append(line);
      break;

    case InBasicAttr:
      if (trimmed.startsWith(QStringLiteral("attr")) && trimmed.contains(QChar('{'))) {
        /* Enter attr{} block */
        state = InBasicAttrInner;
        braceDepth = 1;  /* Reset to count attr{} braces */
        continue;
      }
      /* Track other braces in basic attribute block */
      if (trimmed.contains(QChar('{'))) {
        braceDepth++;
      }
      if (trimmed.contains(QChar('}'))) {
        braceDepth--;
        if (braceDepth == 0) {
          state = Normal;
        }
      }
      break;

    case InBasicAttrInner:
      /* Count braces to detect end of attr{} */
      if (trimmed.contains(QChar('{'))) {
        braceDepth++;
      }
      if (trimmed.contains(QChar('}'))) {
        braceDepth--;
        if (braceDepth == 0) {
          /* End of attr{} - back to basic attribute level */
          state = InBasicAttr;
          braceDepth = 1;  /* Reset for basic attribute brace counting */
          continue;
        }
      }
      /* Save property lines */
      if (!trimmed.isEmpty() && braceDepth > 0) {
        pendingBasicAttr.append(QStringLiteral("\t\t") + trimmed);
      }
      break;

    case InDynamicAttr:
      if (trimmed.startsWith(QStringLiteral("attr")) && trimmed.contains(QChar('{'))) {
        state = InDynamicAttrInner;
        braceDepth = 1;
        continue;
      }
      if (trimmed.contains(QChar('{'))) {
        braceDepth++;
      }
      if (trimmed.contains(QChar('}'))) {
        braceDepth--;
        if (braceDepth == 0) {
          state = Normal;
        }
      }
      break;

    case InDynamicAttrInner:
      if (trimmed.contains(QChar('{'))) {
        braceDepth++;
      }
      if (trimmed.contains(QChar('}'))) {
        braceDepth--;
        if (braceDepth == 0) {
          state = InDynamicAttr;
          braceDepth = 1;
          continue;
        }
      }
      if (!trimmed.isEmpty() && braceDepth > 0) {
        pendingDynamicAttr.append(QStringLiteral("\t\t") + trimmed);
      }
      break;

    case InWidget:
      widgetBuffer.append(line);
      /* Detect object{} sub-block */
      if (trimmed.startsWith(QStringLiteral("object")) && trimmed.contains(QChar('{'))) {
        state = InWidgetObject;
        objectBraceDepth = 1;
        continue;
      }
      /* Track widget braces */
      if (trimmed.contains(QChar('{'))) {
        widgetBraceDepth++;
      }
      if (trimmed.contains(QChar('}'))) {
        widgetBraceDepth--;
        if (widgetBraceDepth == 0) {
          /* Widget complete - output with attributes injected */
          result.append(widgetBuffer);
          state = Normal;
          pendingBasicAttr.clear();
          pendingDynamicAttr.clear();
        }
      }
      break;

    case InWidgetObject:
      widgetBuffer.append(line);
      if (trimmed.contains(QChar('{'))) {
        objectBraceDepth++;
      }
      if (trimmed.contains(QChar('}'))) {
        objectBraceDepth--;
        if (objectBraceDepth == 0) {
          /* object{} closed - inject attributes now */
          if (!pendingBasicAttr.isEmpty()) {
            widgetBuffer.append(QStringLiteral("\t\"basic attribute\" {"));
            widgetBuffer.append(pendingBasicAttr);
            widgetBuffer.append(QStringLiteral("\t}"));
          }
          if (!pendingDynamicAttr.isEmpty()) {
            widgetBuffer.append(QStringLiteral("\t\"dynamic attribute\" {"));
            widgetBuffer.append(pendingDynamicAttr);
            widgetBuffer.append(QStringLiteral("\t}"));
          }
          state = InWidget;  /* Back to widget level */
        }
      }
      break;
    }
  }

  QString converted = result.join(QChar('\n'));
  
  /* In legacy format, monitor blocks used "rdbk" instead of "chan" */
  /* Handle both tabs and spaces for indentation */
  QRegularExpression rdbkPattern(QStringLiteral("\\n(\\s+)rdbk="));
  converted.replace(rdbkPattern, QStringLiteral("\n\\1chan="));
  
  /* Debug: write converted text to file */
  QFile debugFile(QStringLiteral("/tmp/qtedm_converted.adl"));
  if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&debugFile);
    out << converted;
    debugFile.close();
  }

  return converted;
}

inline void DisplayWindow::writeAdlToStream(QTextStream &stream, const QString &fileNameHint) const
{
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

  const QString hint = fileNameHint.isEmpty() ? filePath_ : fileNameHint;
  const QFileInfo info(hint);
  QString fileName = info.filePath();
  if (info.isAbsolute()) {
    fileName = info.absoluteFilePath();
  }
  if (fileName.isEmpty()) {
    fileName = info.fileName();
    if (fileName.isEmpty()) {
      fileName = QStringLiteral("display.adl");
    }
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
  // Capture the decorated window position so the saved ADL restores accurately.
  const QRect frameRect = frameGeometry();
  const QRect displayRect(frameRect.x(), frameRect.y(), displayWidth,
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
      QStringLiteral("snapToGrid=%1").arg(snapToGrid_ ? 1 : 0));
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
      AdlWriter::writeObjectSection(stream, 1, composite->geometry());
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
      if (std::abs(slider->increment() - 1.0) > 1e-9) {
        AdlWriter::writeIndentedLine(stream, 1,
            QStringLiteral("dPrecision=%1")
                .arg(QString::number(slider->increment(), 'g', 6)));
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
}

inline bool DisplayWindow::writeAdlFile(const QString &filePath) const
{
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }

  {
    QTextStream stream(&file);
    setLatin1Encoding(stream);
    writeAdlToStream(stream, filePath);
    stream.flush();
  }

  if (!file.commit()) {
    return false;
  }
  return true;
}


inline QByteArray DisplayWindow::serializeStateForUndo(const QString &fileNameHint) const
{
  QByteArray buffer;
  QBuffer device(&buffer);
  if (!device.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return buffer;
  }
  QTextStream stream(&device);
  setUtf8Encoding(stream);
  const QString hint = fileNameHint.isEmpty() ? filePath_ : fileNameHint;
  writeAdlToStream(stream, hint);
  stream.flush();
  device.close();
  return buffer;
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
    AdlWriter::writeObjectSection(stream, next, composite->geometry());
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
    AdlWriter::writeObjectSection(stream, next, text->geometry());
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
    AdlWriter::writeObjectSection(stream, next, entry->geometry());
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
    AdlWriter::writeObjectSection(stream, next, slider->geometry());
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
    if (std::abs(slider->increment() - 1.0) > 1e-9) {
      AdlWriter::writeIndentedLine(stream, next,
          QStringLiteral("dPrecision=%1")
              .arg(QString::number(slider->increment(), 'g', 6)));
    }
    AdlWriter::writeLimitsSection(stream, next, slider->limits(), true);
    AdlWriter::writeIndentedLine(stream, level, QStringLiteral("}"));
    return;
  }

  if (auto *wheel = dynamic_cast<WheelSwitchElement *>(widget)) {
    AdlWriter::writeIndentedLine(stream, level,
        QStringLiteral("\"wheel switch\" {"));
    AdlWriter::writeObjectSection(stream, next, wheel->geometry());
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
    AdlWriter::writeObjectSection(stream, next, choice->geometry());
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
    AdlWriter::writeObjectSection(stream, next, menu->geometry());
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
    AdlWriter::writeObjectSection(stream, next, message->geometry());
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
    AdlWriter::writeObjectSection(stream, next, shell->geometry());
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
    AdlWriter::writeObjectSection(stream, next, related->geometry());
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
    AdlWriter::writeObjectSection(stream, next, meter->geometry());
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
    AdlWriter::writeObjectSection(stream, next, bar->geometry());
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
    AdlWriter::writeObjectSection(stream, next, scale->geometry());
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
    AdlWriter::writeObjectSection(stream, next, byte->geometry());
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
    AdlWriter::writeObjectSection(stream, next, monitor->geometry());
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
    AdlWriter::writeObjectSection(stream, next, strip->geometry());
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
    AdlWriter::writeObjectSection(stream, next, cartesian->geometry());
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
  AdlWriter::writeObjectSection(stream, next, rectangle->geometry());
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
  AdlWriter::writeObjectSection(stream, next, image->geometry());
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
  AdlWriter::writeObjectSection(stream, next, oval->geometry());
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
  AdlWriter::writeObjectSection(stream, next, arc->geometry());
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
  AdlWriter::writeObjectSection(stream, next, line->geometry());
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
  AdlWriter::writeObjectSection(stream, next, polyline->geometry());
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
  AdlWriter::writeObjectSection(stream, next, polygon->geometry());
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
    using ElementType = typename std::decay_t<decltype(list)>::value_type;
    for (auto *element : list) {
      if (element) {
        if constexpr (std::is_same_v<ElementType, TextElement *>) {
          removeTextRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, TextEntryElement *>) {
          removeTextEntryRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, SliderElement *>) {
          removeSliderRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, WheelSwitchElement *>) {
          removeWheelSwitchRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, MeterElement *>) {
          removeMeterRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, BarMonitorElement *>) {
          removeBarMonitorRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, ScaleMonitorElement *>) {
          removeScaleMonitorRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, StripChartElement *>) {
          removeStripChartRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, CartesianPlotElement *>) {
          removeCartesianPlotRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, ByteMonitorElement *>) {
          removeByteMonitorRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, ChoiceButtonElement *>) {
          removeChoiceButtonRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, MenuElement *>) {
          removeMenuRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, MessageButtonElement *>) {
          removeMessageButtonRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, LineElement *>) {
          removeLineRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, RectangleElement *>) {
          removeRectangleRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, ImageElement *>) {
          removeImageRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, ArcElement *>) {
          removeArcRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, OvalElement *>) {
          removeOvalRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, PolylineElement *>) {
          removePolylineRuntime(element);
        } else if constexpr (std::is_same_v<ElementType, PolygonElement *>) {
          removePolygonRuntime(element);
        }
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
  elementStackSet_.clear();
  polygonCreationActive_ = false;
  polygonCreationPoints_.clear();
  activePolygonElement_ = nullptr;
  polylineCreationActive_ = false;
  polylineCreationPoints_.clear();
  activePolylineElement_ = nullptr;
  colormapName_.clear();
  if (!restoringState_) {
    macroDefinitions_.clear();
  }
  gridOn_ = kDefaultGridOn;
  snapToGrid_ = kDefaultSnapToGrid;
  gridSpacing_ = kDefaultGridSpacing;
  displaySelected_ = false;
  if (displayArea_) {
    displayArea_->setSelected(false);
    displayArea_->setGridOn(gridOn_);
    displayArea_->setGridSpacing(gridSpacing_);
    displayArea_->setMinimumSize(kMinimumDisplayWidth, kMinimumDisplayHeight);
  }
  currentLoadDirectory_.clear();
  updateResourcePaletteDisplayControls();
}

inline bool DisplayWindow::loadDisplaySection(const AdlNode &displayNode)
{
  QRect geometry = parseObjectGeometry(displayNode);
  const AdlNode *objectNode = ::findChild(displayNode, QStringLiteral("object"));
  const bool hasWidthProperty = objectNode
      && (::findProperty(*objectNode, QStringLiteral("width")) != nullptr);
  const bool hasHeightProperty = objectNode
      && (::findProperty(*objectNode, QStringLiteral("height")) != nullptr);
  const bool hasXProperty = objectNode
      && (::findProperty(*objectNode, QStringLiteral("x")) != nullptr);
  const bool hasYProperty = objectNode
      && (::findProperty(*objectNode, QStringLiteral("y")) != nullptr);

  if (displayArea_) {
    const QSize currentWindowSize = size();
    const QSize currentAreaSize = displayArea_->size();
    const int extraWidth = currentWindowSize.width() - currentAreaSize.width();
    const int extraHeight = currentWindowSize.height() - currentAreaSize.height();
    const int targetWidth = std::max(hasWidthProperty ? geometry.width()
                                                     : currentAreaSize.width(),
        kMinimumDisplayWidth);
    const int targetHeight = std::max(hasHeightProperty ? geometry.height()
                                                       : currentAreaSize.height(),
        kMinimumDisplayHeight);
    displayArea_->setMinimumSize(targetWidth, targetHeight);
    displayArea_->resize(targetWidth, targetHeight);
    if (hasWidthProperty || hasHeightProperty) {
      resize(targetWidth + extraWidth, targetHeight + extraHeight);
    }
    QPoint targetPos = pos();
    if (hasXProperty) {
      targetPos.setX(geometry.x());
    }
    if (hasYProperty) {
      targetPos.setY(geometry.y());
    }
    if (hasXProperty || hasYProperty) {
      // Preserve unspecified axes so partial geometry in the ADL stays intact.
      move(targetPos);
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
  const QString snapStr = propertyValue(displayNode,
      QStringLiteral("snapToGrid"));
  int snapValue = snapStr.toInt(&ok);
  if (ok) {
    snapToGrid_ = snapValue != 0;
  }
  updateResourcePaletteDisplayControls();
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
  /* Old ADL format (pre-version 20200) nests channels in attr → param.
   * Modern format has channels directly as properties.
   * Check for old format first. */
  const AdlNode *attrNode = ::findChild(node, QStringLiteral("attr"));
  const AdlNode *paramNode = attrNode != nullptr
      ? ::findChild(*attrNode, QStringLiteral("param"))
      : nullptr;
  const AdlNode &propSource = paramNode != nullptr ? *paramNode : node;

  for (const auto &prop : propSource.properties) {
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

inline AdlNode DisplayWindow::applyPendingBasicAttribute(
    const AdlNode &node) const
{
  if (!pendingBasicAttribute_) {
    return node;
  }
  /* Check if this node already has a "basic attribute" child */
  if (::findChild(node, QStringLiteral("basic attribute"))) {
    return node;
  }
  /* Create a copy and add the pending basic attribute as a child */
  AdlNode merged = node;
  
  /* In old-format ADL files, standalone "basic attribute" blocks have an 
   * "attr" child containing the actual properties. We need to unwrap this
   * and create a proper "basic attribute" node with direct properties. */
  if (const AdlNode *attrChild = ::findChild(*pendingBasicAttribute_,
          QStringLiteral("attr"))) {
    /* Create a new basic attribute node with properties from attr child */
    AdlNode basicAttr;
    basicAttr.name = QStringLiteral("basic attribute");
    basicAttr.properties = attrChild->properties;
    basicAttr.children = attrChild->children;
    merged.children.append(basicAttr);
  } else {
    /* No attr child, use the pending attribute as-is */
    merged.children.append(*pendingBasicAttribute_);
  }
  
  return merged;
}

inline AdlNode DisplayWindow::applyPendingDynamicAttribute(
    const AdlNode &node) const
{
  if (!pendingDynamicAttribute_) {
    return node;
  }
  /* Check if this node already has a "dynamic attribute" child */
  if (::findChild(node, QStringLiteral("dynamic attribute"))) {
    return node;
  }
  /* Create a copy and add the pending dynamic attribute as a child */
  AdlNode merged = node;
  merged.children.append(*pendingDynamicAttribute_);
  return merged;
}

inline QStringList DisplayWindow::buildDisplaySearchPaths() const
{
  QStringList searchPaths;
  const QByteArray env = qgetenv("EPICS_DISPLAY_PATH");
  if (!env.isEmpty()) {
    const QStringList parts = QString::fromLocal8Bit(env).split(
        QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
      const QString trimmed = part.trimmed();
      if (!trimmed.isEmpty()) {
        searchPaths.push_back(trimmed);
      }
    }
  }
  return searchPaths;
}

inline QHash<QString, QString> DisplayWindow::parseMacroDefinitionString(
    const QString &macroString) const
{
  QHash<QString, QString> macros;
  if (macroString.trimmed().isEmpty()) {
    return macros;
  }
  const QStringList entries = macroString.split(QLatin1Char(','),
      Qt::KeepEmptyParts);
  for (const QString &entry : entries) {
    const QString trimmedEntry = entry.trimmed();
    if (trimmedEntry.isEmpty()) {
      continue;
    }
    const int equalsIndex = trimmedEntry.indexOf(QLatin1Char('='));
    if (equalsIndex <= 0) {
      continue;
    }
    const QString name = trimmedEntry.left(equalsIndex).trimmed();
    const QString value = trimmedEntry.mid(equalsIndex + 1).trimmed();
    if (name.isEmpty()) {
      continue;
    }
    macros.insert(name, value);
  }
  return macros;
}

inline QString DisplayWindow::resolveRelatedDisplayFile(
    const QString &fileName) const
{
  const QString trimmed = fileName.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }

  QFileInfo directInfo(trimmed);
  if (directInfo.exists() && directInfo.isFile()) {
    return directInfo.absoluteFilePath();
  }

  if (!currentLoadDirectory_.isEmpty()) {
    QFileInfo fromCurrent(QDir(currentLoadDirectory_), trimmed);
    if (fromCurrent.exists() && fromCurrent.isFile()) {
      return fromCurrent.absoluteFilePath();
    }
  }

  if (!filePath_.isEmpty()) {
    QFileInfo relative(QFileInfo(filePath_).absolutePath(), trimmed);
    if (relative.exists() && relative.isFile()) {
      return relative.absoluteFilePath();
    }
  }

  QFileInfo cwdRelative(QDir::current(), trimmed);
  if (cwdRelative.exists() && cwdRelative.isFile()) {
    return cwdRelative.absoluteFilePath();
  }

  const QStringList searchPaths = buildDisplaySearchPaths();
  for (const QString &base : searchPaths) {
    QFileInfo candidate(QDir(base), trimmed);
    if (candidate.exists() && candidate.isFile()) {
      return candidate.absoluteFilePath();
    }
  }

  return QString();
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
  /* Use set for O(1) duplicate checking instead of O(n) linear search */
  if (elementStackSet_.contains(element)) {
    return;
  }
  elementStack_.append(QPointer<QWidget>(element));
  elementStackSet_.insert(element);
  /* Defer stacking order refresh during bulk loading to avoid O(n²) behavior.
   * refreshStackingOrder() will be called once after load completes. */
  if (!restoringState_) {
    refreshStackingOrder();
  }
}

inline bool DisplayWindow::isStaticGraphicWidget(const QWidget *widget) const
{
  if (!widget) {
    return false;
  }
  
  /* Check if this is a CompositeElement with only static children */
  if (const auto *composite = dynamic_cast<const CompositeElement *>(widget)) {
    QList<QWidget *> children = composite->childWidgets();
    if (children.isEmpty()) {
      /* Empty composite is treated as static */
      return true;
    }
    /* Composite is static only if ALL children are static graphics */
    for (QWidget *child : children) {
      if (!isStaticGraphicWidget(child)) {
        return false;
      }
    }
    return true;
  }
  
  /* Regular static graphics (rectangles, ovals, text, etc.) */
  return dynamic_cast<const RectangleElement *>(widget)
    || dynamic_cast<const ImageElement *>(widget)
    || dynamic_cast<const OvalElement *>(widget)
    || dynamic_cast<const ArcElement *>(widget)
    || dynamic_cast<const LineElement *>(widget)
    || dynamic_cast<const PolylineElement *>(widget)
    || dynamic_cast<const PolygonElement *>(widget)
    || dynamic_cast<const TextElement *>(widget);
}

inline void DisplayWindow::refreshStackingOrder()
{
  QList<QWidget *> staticWidgets;
  QList<QWidget *> interactiveWidgets;

  for (auto it = elementStack_.begin(); it != elementStack_.end();) {
    QWidget *widget = it->data();
    if (!widget) {
      it = elementStack_.erase(it);
      /* No widget to remove from set - already null */
      continue;
    }
    if (isStaticGraphicWidget(widget)) {
      staticWidgets.append(widget);
    } else {
      interactiveWidgets.append(widget);
    }
    ++it;
  }

  /* Raise static widgets first (rectangles, ovals, etc. with no PVs) */
  for (QWidget *widget : staticWidgets) {
    widget->raise();
  }
  /* Then raise interactive widgets (composites, monitors, controls) on top */
  for (QWidget *widget : interactiveWidgets) {
    widget->raise();
  }
}

inline TextElement *DisplayWindow::loadTextElement(const AdlNode &textNode)
{
  const AdlNode withBasic = applyPendingBasicAttribute(textNode);
  const AdlNode effectiveNode = applyPendingDynamicAttribute(withBasic);
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }
  QRect geometry = parseObjectGeometry(effectiveNode);
  geometry.translate(currentElementOffset_);
  if (geometry.height() < kMinimumTextElementHeight) {
    geometry.setHeight(kMinimumTextElementHeight);
  }
  auto *element = new TextElement(parent);
  element->setFont(font());
  element->setGeometry(geometry);
  const QString content = propertyValue(effectiveNode, QStringLiteral("textix"));
  if (!content.isEmpty()) {
    element->setText(content);
  }
  const QString alignValue = propertyValue(effectiveNode, QStringLiteral("align"));
  if (!alignValue.isEmpty()) {
    element->setTextAlignment(parseAlignment(alignValue));
  }

  if (const AdlNode *basic = ::findChild(effectiveNode,
          QStringLiteral("basic attribute"))) {
    bool ok = false;
    const QString clrStr = propertyValue(*basic, QStringLiteral("clr"));
    int clrIndex = clrStr.toInt(&ok);
    if (ok) {
      element->setForegroundColor(colorForIndex(clrIndex));
    }
  }

  if (const AdlNode *dyn = ::findChild(effectiveNode,
          QStringLiteral("dynamic attribute"))) {
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorMode = propertyValue(modeSource, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }
    const QString visibility = propertyValue(modeSource, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }
    const QString calc = propertyValue(modeSource, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }
    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 1);
  }

  applyChannelProperties(effectiveNode,
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!textRuntimes_.contains(element)) {
      auto *runtime = new TextRuntime(element);
      textRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
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
    /* Try "chan" first (modern format), then "rdbk" (old format) */
    QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (channel.isEmpty()) {
      channel = propertyValue(*monitor, QStringLiteral("rdbk"));
    }
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

  const QString incrementValue = propertyValue(valuatorNode,
      QStringLiteral("dPrecision"));
  if (!incrementValue.isEmpty()) {
    bool ok = false;
    const double increment = incrementValue.toDouble(&ok);
    if (ok) {
      element->setIncrement(increment);
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!menuRuntimes_.contains(element)) {
      auto *runtime = new MenuRuntime(element);
      menuRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
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
    QString channel = propertyValue(*control, QStringLiteral("chan"));
    /* Old format uses "ctrl" instead of "chan" */
    if (channel.isEmpty()) {
      channel = propertyValue(*control, QStringLiteral("ctrl"));
    }
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!messageButtonRuntimes_.contains(element)) {
      auto *runtime = new MessageButtonRuntime(element);
      messageButtonRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
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
  connectShellCommandElement(element);
  return element;
}

inline void DisplayWindow::connectShellCommandElement(
    ShellCommandElement *element)
{
  if (!element) {
    return;
  }
  element->setActivationCallback(
      [this, element](int index, Qt::KeyboardModifiers modifiers) {
        handleShellCommandActivation(element, index, modifiers);
      });
  element->setExecuteMode(executeModeActive_);
}

inline void DisplayWindow::handleShellCommandActivation(
    ShellCommandElement *element, int entryIndex,
    Qt::KeyboardModifiers modifiers)
{
  Q_UNUSED(modifiers);
  if (!element || !executeModeActive_) {
    return;
  }
  if (entryIndex < 0 || entryIndex >= element->entryCount()) {
    return;
  }

  ShellCommandEntry entry = element->entry(entryIndex);
  QString commandTemplate = entry.command.trimmed();
  if (commandTemplate.isEmpty()) {
    return;
  }

  const QString args = entry.args.trimmed();
  if (!args.isEmpty()) {
    commandTemplate.append(QLatin1Char(' '));
    commandTemplate.append(args);
  }

  commandTemplate =
      applyMacroSubstitutions(commandTemplate, macroDefinitions_);

  QString resolved;
  if (!buildShellCommandString(commandTemplate, &resolved)) {
    return;
  }

  if (resolved.trimmed().isEmpty()) {
    return;
  }

  runShellCommand(resolved);
}

inline bool DisplayWindow::buildShellCommandString(
    const QString &templateString, QString *result)
{
  if (!result) {
    return false;
  }

  QString current = templateString;
  const int ampPromptIndex = current.indexOf(QStringLiteral("&?"));
  if (ampPromptIndex >= 0) {
    QString defaultCommand = current.left(ampPromptIndex);
    if (!defaultCommand.endsWith(QLatin1Char('&'))) {
      defaultCommand.append(QLatin1Char('&'));
    }
    QString userCommand;
    if (!promptForShellCommandInput(defaultCommand, &userCommand)) {
      return false;
    }
    current = userCommand;
  } else {
    const int promptIndex = current.indexOf(QLatin1Char('?'));
    if (promptIndex >= 0) {
      QString defaultCommand = current.left(promptIndex);
      if (!defaultCommand.endsWith(QLatin1Char('&'))) {
        defaultCommand.append(QLatin1Char('&'));
      }
      QString userCommand;
      if (!promptForShellCommandInput(defaultCommand, &userCommand)) {
        return false;
      }
      current = userCommand;
    }
  }

  QString output;
  output.reserve(current.size());

  for (int i = 0; i < current.size(); ++i) {
    const QChar ch = current.at(i);
    if (ch != QLatin1Char('&')) {
      output.append(ch);
      continue;
    }
    if (i + 1 >= current.size()) {
      output.append(ch);
      continue;
    }
    const QChar token = current.at(i + 1);
    if (token == QLatin1Char('P')) {
      QStringList pvNames = promptForShellCommandPvNames();
      if (pvNames.isEmpty()) {
        return false;
      }
      output.append(pvNames.join(QStringLiteral(" ")));
      ++i;
      continue;
    }
    if (token == QLatin1Char('A')) {
      output.append(shellCommandDisplayPath());
      ++i;
      continue;
    }
    if (token == QLatin1Char('T')) {
      output.append(shellCommandDisplayTitle());
      ++i;
      continue;
    }
    if (token == QLatin1Char('X')) {
      output.append(QString::number(static_cast<qulonglong>(winId())));
      ++i;
      continue;
    }
    if (token == QLatin1Char('?')) {
      QString defaultCommand = output;
      if (!defaultCommand.endsWith(QLatin1Char('&'))) {
        defaultCommand.append(QLatin1Char('&'));
      }
      QString userCommand;
      if (!promptForShellCommandInput(defaultCommand, &userCommand)) {
        return false;
      }
      *result = userCommand;
      return true;
    }
    output.append(ch);
  }

  *result = output;
  return true;
}

inline bool DisplayWindow::promptForShellCommandInput(
    const QString &defaultCommand, QString *result)
{
  bool ok = false;
  const QString text = QInputDialog::getText(this,
      QStringLiteral("Shell Command"), QStringLiteral("Command:"),
      QLineEdit::Normal, defaultCommand, &ok);
  if (!ok) {
    return false;
  }
  if (result) {
    *result = text;
  }
  return true;
}

inline QStringList DisplayWindow::promptForShellCommandPvNames()
{
  if (!displayArea_) {
    return QStringList();
  }

  class ShellCommandPvPicker : public QObject
  {
  public:
    ShellCommandPvPicker(DisplayWindow *window, QStringList &channels,
        bool &cancelled, QEventLoop &loop)
      : QObject(window)
      , window_(window)
      , channels_(channels)
      , cancelled_(cancelled)
      , loop_(loop)
    {
    }

  protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
      Q_UNUSED(object);
      if (!window_) {
        return QObject::eventFilter(object, event);
      }
      if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
          const QPoint globalPos = mouseEvent->globalPosition().toPoint();
#else
          const QPoint globalPos = mouseEvent->globalPos();
#endif
          const QPoint windowPos = window_->mapFromGlobal(globalPos);
          QWidget *target = window_->elementAt(windowPos);
          channels_ = window_->channelsForWidget(target);
          if (!target) {
            cancelled_ = true;
            QMessageBox::warning(window_, QStringLiteral("Shell Command"),
                QStringLiteral("Not on an object with a process variable."));
          } else if (channels_.isEmpty()) {
            cancelled_ = true;
            QMessageBox::warning(window_, QStringLiteral("Shell Command"),
                QStringLiteral("No process variables associated with the selected object."));
          } else {
            cancelled_ = false;
          }
          loop_.quit();
          mouseEvent->accept();
          return true;
        }
        cancelled_ = true;
        loop_.quit();
        mouseEvent->accept();
        return true;
      }
      if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
          cancelled_ = true;
          loop_.quit();
          keyEvent->accept();
          return true;
        }
      }
      return QObject::eventFilter(object, event);
    }

  private:
    DisplayWindow *window_ = nullptr;
    QStringList &channels_;
    bool &cancelled_;
    QEventLoop &loop_;
  };

  QEventLoop loop;
  QStringList channels;
  bool cancelled = true;

  ShellCommandPvPicker picker(this, channels, cancelled, loop);
  displayArea_->installEventFilter(&picker);
  QMetaObject::Connection destroyedConnection =
      QObject::connect(displayArea_, &QObject::destroyed,
          &loop, &QEventLoop::quit);

  if (!pvInfoCursorInitialized_) {
    pvInfoCursor_ = createPvInfoCursor();
    pvInfoCursorInitialized_ = true;
  }
  QGuiApplication::setOverrideCursor(pvInfoCursor_);
  loop.exec();
  QGuiApplication::restoreOverrideCursor();

  QObject::disconnect(destroyedConnection);
  displayArea_->removeEventFilter(&picker);

  if (cancelled || channels.isEmpty()) {
    return QStringList();
  }
  return channels;
}

inline QString DisplayWindow::shellCommandDisplayPath() const
{
  if (!filePath_.isEmpty()) {
    return filePath_;
  }
  return windowTitle();
}

inline QString DisplayWindow::shellCommandDisplayTitle() const
{
  if (!filePath_.isEmpty()) {
    return QFileInfo(filePath_).fileName();
  }
  return windowTitle();
}

inline void DisplayWindow::runShellCommand(const QString &command)
{
  const QString trimmed = command.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  const QByteArray encoded = trimmed.toLocal8Bit();
  int status = std::system(encoded.constData());
  if (status == -1) {
    qWarning() << "Failed to start shell command:" << trimmed;
  } else if (status != 0) {
    qWarning() << "Shell command exited with status" << status
               << ":" << trimmed;
  }
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
  connectRelatedDisplayElement(element);
  return element;
}

inline void DisplayWindow::connectRelatedDisplayElement(
    RelatedDisplayElement *element)
{
  if (!element) {
    return;
  }
  element->setActivationCallback(
      [this, element](int index, Qt::KeyboardModifiers modifiers) {
        handleRelatedDisplayActivation(element, index, modifiers);
      });
  element->setExecuteMode(executeModeActive_);
}

inline void DisplayWindow::handleRelatedDisplayActivation(
    RelatedDisplayElement *element, int entryIndex,
    Qt::KeyboardModifiers modifiers)
{
  if (!element || !executeModeActive_) {
    return;
  }
  if (entryIndex < 0 || entryIndex >= element->entryCount()) {
    return;
  }

  RelatedDisplayEntry entry = element->entry(entryIndex);
  const QString fileName = entry.name.trimmed();
  if (fileName.isEmpty()) {
    return;
  }

  const bool replaceRequested = modifiers.testFlag(Qt::ControlModifier);
  const bool replace = replaceRequested
      || entry.mode == RelatedDisplayMode::kReplace;

  const QString resolved = resolveRelatedDisplayFile(fileName);
  if (resolved.isEmpty()) {
    const QString message = QStringLiteral("Cannot open related display:\n  %1\nCheck EPICS_DISPLAY_PATH")
        .arg(fileName);
    QMessageBox::critical(this, QStringLiteral("Related Display"), message);
    return;
  }

  const QString substitutedArgs = applyMacroSubstitutions(entry.args,
      macroDefinitions_);
  QHash<QString, QString> macros =
      parseMacroDefinitionString(substitutedArgs);

  if (replace) {
    QString errorMessage;
    if (!loadFromFile(resolved, &errorMessage, macros)) {
      const QString message = errorMessage.isEmpty()
          ? QStringLiteral("Failed to open display:\n%1").arg(resolved)
          : errorMessage;
      QMessageBox::critical(this, QStringLiteral("Related Display"),
          message);
    }
    return;
  }

  auto state = state_.lock();
  if (!state) {
    return;
  }

  /* Check if a display with the same file path and macros is already open.
   * If found, bring it forward instead of creating a duplicate. */
  for (const auto &displayPtr : state->displays) {
    if (displayPtr.isNull()) {
      continue;
    }
    DisplayWindow *existingWindow = displayPtr.data();
    if (!existingWindow || existingWindow == this) {
      continue;
    }
    /* Compare file paths and macro definitions */
    if (existingWindow->filePath() == resolved
        && existingWindow->macroDefinitions() == macros) {
      /* Found an exact match - bring it forward */
      if (existingWindow->isMinimized()) {
        existingWindow->showNormal();
      } else {
        existingWindow->show();
      }
      existingWindow->raise();
      existingWindow->activateWindow();
      return;
    }
  }

  auto *newWindow = new DisplayWindow(palette(), resourcePaletteBase_,
      font(), labelFont_, state_);
  QString errorMessage;
  if (!newWindow->loadFromFile(resolved, &errorMessage, macros)) {
    const QString message = errorMessage.isEmpty()
        ? QStringLiteral("Failed to open display:\n%1").arg(resolved)
        : errorMessage;
    QMessageBox::critical(this, QStringLiteral("Related Display"),
        message);
    delete newWindow;
    return;
  }

  registerDisplayWindow(newWindow, true);
}

inline void DisplayWindow::registerDisplayWindow(DisplayWindow *displayWin,
    bool delayExecuteMode)
{
  if (!displayWin) {
    return;
  }
  auto state = state_.lock();
  if (!state) {
    return;
  }

  state->displays.append(displayWin);
  displayWin->syncCreateCursor();

  const bool postponeExecute = delayExecuteMode && !state->editMode;
  if (!postponeExecute) {
    displayWin->handleEditModeChanged(state->editMode);
  }

  QObject *context = state->mainWindow
      ? static_cast<QObject *>(state->mainWindow.data())
      : static_cast<QObject *>(this);
  const auto updateMenus = state->updateMenus;
  QPointer<DisplayWindow> displayPtr(displayWin);

  QObject::connect(displayWin, &QObject::destroyed, context,
      [stateWeak = state_, updateMenus, displayPtr]() {
        if (auto locked = stateWeak.lock()) {
          if (locked->activeDisplay == displayPtr) {
            locked->activeDisplay.clear();
          }
          bool hasLiveDisplay = false;
          for (auto &display : locked->displays) {
            if (!display.isNull()) {
              hasLiveDisplay = true;
              break;
            }
          }
          if (!hasLiveDisplay) {
            locked->createTool = CreateTool::kNone;
          }
        }
        if (updateMenus && *updateMenus) {
          (*updateMenus)();
        }
      });

  displayWin->show();
  displayWin->raise();
  displayWin->activateWindow();

  if (updateMenus && *updateMenus) {
    (*updateMenus)();
  }

  if (postponeExecute) {
    QPointer<DisplayWindow> delayed(displayWin);
    QTimer::singleShot(0, displayWin,
        [stateWeak = state_, delayed]() {
          if (auto locked = stateWeak.lock()) {
            if (!locked->editMode) {
              if (DisplayWindow *window = delayed.data()) {
                window->handleEditModeChanged(false);
              }
            }
          }
        });
  }
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
    QString channel = propertyValue(*control, QStringLiteral("chan"));
    /* Old format uses "ctrl" instead of "chan" */
    if (channel.isEmpty()) {
      channel = propertyValue(*control, QStringLiteral("ctrl"));
    }
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!choiceButtonRuntimes_.contains(element)) {
      auto *runtime = new ChoiceButtonRuntime(element);
      choiceButtonRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
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
    /* Try "chan" first (modern format), then "rdbk" (old format) */
    QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (channel.isEmpty()) {
      channel = propertyValue(*monitor, QStringLiteral("rdbk"));
    }
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
    /* Try "chan" first (modern format), then "rdbk" (old format) */
    QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (channel.isEmpty()) {
      channel = propertyValue(*monitor, QStringLiteral("rdbk"));
    }
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
    /* Try "chan" first (modern format), then "rdbk" (old format) */
    QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (channel.isEmpty()) {
      channel = propertyValue(*monitor, QStringLiteral("rdbk"));
    }
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
    // Handle legacy "delay" format - medm converts delay to period based on units.
    // For SECONDS: period = delay * 60
    // For MINUTES: period = delay
    // For MILLISECONDS: period = delay
    const QString delayStr = propertyValue(stripNode, QStringLiteral("delay"));
    const QString unitsStr = propertyValue(stripNode, QStringLiteral("units"));
    periodValue = delayStr.toDouble(&ok);
    if (ok) {
      // Apply medm's delay-to-period conversion based on units
      const TimeUnits units = parseTimeUnits(unitsStr);
      if (units == TimeUnits::kSeconds) {
        periodValue *= 60.0;  // medm multiplies by 60 for seconds
      }
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
    /* Try "chan" first (modern format), then "rdbk" (old format) */
    QString channel = propertyValue(*monitor, QStringLiteral("chan"));
    if (channel.isEmpty()) {
      channel = propertyValue(*monitor, QStringLiteral("rdbk"));
    }
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
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorMode = propertyValue(modeSource, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(modeSource, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString visCalc = propertyValue(modeSource, QStringLiteral("calc"));
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!imageRuntimes_.contains(element)) {
      auto *runtime = new ImageRuntime(element);
      imageRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  return element;
}

inline RectangleElement *DisplayWindow::loadRectangleElement(
    const AdlNode &rectangleNode)
{
  const AdlNode withBasic = applyPendingBasicAttribute(rectangleNode);
  const AdlNode effectiveNode = applyPendingDynamicAttribute(withBasic);
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(effectiveNode);
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

  if (const AdlNode *basic = ::findChild(effectiveNode,
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
    const int adlWidth = ok ? widthValue.toInt() : 0;
    element->setAdlLineWidth(adlWidth);
    element->setLineWidth(width);
    
    /* Expand geometry by 1 pixel if adlLineWidth is 0 to prevent clipping */
    if (adlWidth == 0) {
      QRect expandedGeometry = element->geometry();
      expandedGeometry.setWidth(expandedGeometry.width() + 1);
      expandedGeometry.setHeight(expandedGeometry.height() + 1);
      element->setGeometry(expandedGeometry);
    }
  }

  if (const AdlNode *dyn = ::findChild(effectiveNode,
          QStringLiteral("dynamic attribute"))) {
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorMode = propertyValue(modeSource, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(modeSource, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString calc = propertyValue(modeSource, QStringLiteral("calc"));
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!rectangleRuntimes_.contains(element)) {
      auto *runtime = new RectangleRuntime(element);
      rectangleRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  return element;
}

inline OvalElement *DisplayWindow::loadOvalElement(const AdlNode &ovalNode)
{
  const AdlNode withBasic = applyPendingBasicAttribute(ovalNode);
  const AdlNode effectiveNode = applyPendingDynamicAttribute(withBasic);
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(effectiveNode);
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

  if (const AdlNode *basic = ::findChild(effectiveNode,
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

  if (const AdlNode *dyn = ::findChild(effectiveNode,
          QStringLiteral("dynamic attribute"))) {
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorMode = propertyValue(modeSource, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(modeSource, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString calc = propertyValue(modeSource, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }

    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 0);
  }

  applyChannelProperties(effectiveNode,
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!ovalRuntimes_.contains(element)) {
      auto *runtime = new OvalRuntime(element);
      ovalRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  return element;
}

inline ArcElement *DisplayWindow::loadArcElement(const AdlNode &arcNode)
{
  const AdlNode withBasic = applyPendingBasicAttribute(arcNode);
  const AdlNode effectiveNode = applyPendingDynamicAttribute(withBasic);
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QRect geometry = parseObjectGeometry(effectiveNode);
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

  if (const AdlNode *basic = ::findChild(effectiveNode,
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

  if (const AdlNode *dyn = ::findChild(effectiveNode,
          QStringLiteral("dynamic attribute"))) {
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorMode = propertyValue(modeSource, QStringLiteral("clr"));
    if (!colorMode.isEmpty()) {
      element->setColorMode(parseTextColorMode(colorMode));
    }

    const QString visibility = propertyValue(modeSource, QStringLiteral("vis"));
    if (!visibility.isEmpty()) {
      element->setVisibilityMode(parseVisibilityMode(visibility));
    }

    const QString calc = propertyValue(modeSource, QStringLiteral("calc"));
    if (!calc.isEmpty()) {
      element->setVisibilityCalc(calc);
    }

    applyChannelProperties(*dyn,
        [element](int index, const QString &value) {
          element->setChannel(index, value);
        },
        0, 0);
  }

  applyChannelProperties(effectiveNode,
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!arcRuntimes_.contains(element)) {
      auto *runtime = new ArcRuntime(element);
      arcRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  return element;
}

inline PolygonElement *DisplayWindow::loadPolygonElement(
    const AdlNode &polygonNode)
{
  const AdlNode withBasic = applyPendingBasicAttribute(polygonNode);
  const AdlNode effectiveNode = applyPendingDynamicAttribute(withBasic);
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QVector<QPoint> points = parsePolylinePoints(effectiveNode);
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

  if (const AdlNode *basic = ::findChild(effectiveNode,
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

  if (const AdlNode *dyn = ::findChild(effectiveNode,
          QStringLiteral("dynamic attribute"))) {
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorModeValue = propertyValue(modeSource,
        QStringLiteral("clr"));
    if (!colorModeValue.isEmpty()) {
      colorMode = parseTextColorMode(colorModeValue);
    }

    const QString visibilityValue = propertyValue(modeSource,
        QStringLiteral("vis"));
    if (!visibilityValue.isEmpty()) {
      visibilityMode = parseVisibilityMode(visibilityValue);
    }

    const QString calcValue = propertyValue(modeSource, QStringLiteral("calc"));
    if (!calcValue.isEmpty()) {
      visibilityCalc = calcValue.trimmed();
    }

    applyChannelProperties(*dyn, channelSetter, 0, 0);
  }

  applyChannelProperties(effectiveNode, channelSetter, 0, 0);

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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!polygonRuntimes_.contains(element)) {
      auto *runtime = new PolygonRuntime(element);
      polygonRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
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
  const AdlNode withBasic = applyPendingBasicAttribute(polylineNode);
  const AdlNode effectiveNode = applyPendingDynamicAttribute(withBasic);
  QWidget *parent = effectiveElementParent();
  if (!parent) {
    return nullptr;
  }

  QVector<QPoint> points = parsePolylinePoints(effectiveNode);
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

  if (const AdlNode *basic = ::findChild(effectiveNode,
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

  if (const AdlNode *dyn = ::findChild(effectiveNode,
          QStringLiteral("dynamic attribute"))) {
    /* Old ADL format (pre-version 20200) nests mode properties in attr → mod.
     * Modern format has mode properties directly in dynamic attribute. */
    const AdlNode *attrNode = ::findChild(*dyn, QStringLiteral("attr"));
    const AdlNode *modNode = attrNode != nullptr
        ? ::findChild(*attrNode, QStringLiteral("mod"))
        : nullptr;
    const AdlNode &modeSource = modNode != nullptr ? *modNode : *dyn;

    const QString colorModeValue = propertyValue(modeSource,
        QStringLiteral("clr"));
    if (!colorModeValue.isEmpty()) {
      colorMode = parseTextColorMode(colorModeValue);
    }

    const QString visibilityValue = propertyValue(modeSource,
        QStringLiteral("vis"));
    if (!visibilityValue.isEmpty()) {
      visibilityMode = parseVisibilityMode(visibilityValue);
    }

    const QString calcValue = propertyValue(modeSource, QStringLiteral("calc"));
    if (!calcValue.isEmpty()) {
      visibilityCalc = calcValue.trimmed();
    }

    applyChannelProperties(*dyn, channelSetter, 0, 0);
  }

  applyChannelProperties(effectiveNode, channelSetter, 0, 0);

  QPolygon polygon(points);
  QRect geometry = polygon.boundingRect();
  
  /* Expand geometry to accommodate line width, matching MEDM behavior */
  const int halfWidth = lineWidth / 2;
  geometry.adjust(-halfWidth, -halfWidth, halfWidth, halfWidth);
  
  if (geometry.width() <= 0) {
    geometry.setWidth(std::max(1, lineWidth));
  }
  if (geometry.height() <= 0) {
    geometry.setHeight(std::max(1, lineWidth));
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
    if (executeModeActive_) {
      element->setExecuteMode(true);
      if (!lineRuntimes_.contains(element)) {
        auto *runtime = new LineRuntime(element);
        lineRuntimes_.insert(element, runtime);
        runtime->start();
      }
    }
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
  if (executeModeActive_) {
    element->setExecuteMode(true);
    if (!polylineRuntimes_.contains(element)) {
      auto *runtime = new PolylineRuntime(element);
      polylineRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
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

  /* Children in ADL files keep absolute coordinates; translate them
     into the composite's local space while parsing. */
  const QPoint childOffset = currentElementOffset_ - geometry.topLeft();

  const QString trimmedFile = compositeFile.trimmed();
  if (trimmedFile.isEmpty()) {
    if (const AdlNode *childrenNode = ::findChild(compositeNode,
            QStringLiteral("children"))) {
      ElementLoadContextGuard guard(*this, composite, childOffset, true,
          composite);
      for (const auto &child : childrenNode->children) {
        loadElementNode(child);
      }
    }
  } else {
    /* Parse composite file string: "filename;macro1=value1,macro2=value2" */
    QString fileName = trimmedFile;
    QString macroString;
    const int semicolonIndex = trimmedFile.indexOf(QLatin1Char(';'));
    if (semicolonIndex >= 0) {
      fileName = trimmedFile.left(semicolonIndex).trimmed();
      macroString = trimmedFile.mid(semicolonIndex + 1).trimmed();
    }

    /* Resolve the composite file path */
    const QString resolvedPath = resolveRelatedDisplayFile(fileName);
    if (resolvedPath.isEmpty()) {
      qWarning() << "CompositeElement: Cannot resolve composite file:"
                 << fileName;
    } else {
      /* Parse macro definitions */
      QHash<QString, QString> compositeMacros = parseMacroDefinitionString(macroString);

      /* Merge with existing macros - composite macros take precedence */
      QHash<QString, QString> mergedMacros = macroDefinitions_;
      for (auto it = compositeMacros.constBegin(); it != compositeMacros.constEnd(); ++it) {
        mergedMacros.insert(it.key(), it.value());
      }

      /* Load the composite file */
      QFile file(resolvedPath);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "CompositeElement: Cannot open composite file:"
                   << resolvedPath;
      } else {
        QTextStream stream(&file);
        setLatin1Encoding(stream);
        const QString contents = stream.readAll();

        /* Detect file version */
        int fileVersion = 30122;
        QRegularExpression versionPattern(QStringLiteral(R"(version\s*=\s*(\d+))"));
        QRegularExpressionMatch versionMatch = versionPattern.match(contents);
        if (versionMatch.hasMatch()) {
          bool ok = false;
          int parsedVersion = versionMatch.captured(1).toInt(&ok);
          if (ok) {
            fileVersion = parsedVersion;
          }
        }

        /* Convert legacy format if needed */
        QString adlContent = contents;
        if (fileVersion < 20200) {
          adlContent = convertLegacyAdlFormat(contents, fileVersion);
        }

        /* Apply macro substitutions */
        const QString processedContents = applyMacroSubstitutions(adlContent, mergedMacros);

        QString errorMessage;
        std::optional<AdlNode> document = AdlParser::parse(processedContents, &errorMessage);
        if (!document) {
          qWarning() << "CompositeElement: Failed to parse composite file:"
                     << resolvedPath << "-" << errorMessage;
        } else {
          /* Save current load directory for nested composites */
          const QString previousLoadDir = currentLoadDirectory_;
          currentLoadDirectory_ = QFileInfo(resolvedPath).absolutePath();

          /* Children in composite files have coordinates relative to the
             composite file's display origin. We need to load them at (0,0)
             offset, then the composite will be repositioned to fit contents. */
          ElementLoadContextGuard guard(*this, composite, QPoint(-2, -4), true,
              composite);
          for (const auto &child : document->children) {
            /* Skip file, display, and color map blocks */
            const QString childName = child.name.trimmed().toLower();
            if (childName == QStringLiteral("file")
                || childName == QStringLiteral("display")
                || childName == QStringLiteral("color map")
                || childName == QStringLiteral("<<color map>>")) {
              continue;
            }
            loadElementNode(child);
          }

          /* Restore load directory */
          currentLoadDirectory_ = previousLoadDir;
        }
      }
    }
  }

  /* Expand composite bounds to encompass all child widgets */
  composite->expandToFitChildren();

  return composite;
}

inline bool DisplayWindow::loadElementNode(const AdlNode &node)
{
  const QString name = node.name.trimmed().toLower();

  bool loaded = false;

  if (name == QStringLiteral("text")) {
    loaded = loadTextElement(node) != nullptr;
  } else if (name == QStringLiteral("text update")
      || name == QStringLiteral("text monitor")) {
    loaded = loadTextMonitorElement(node) != nullptr;
  } else if (name == QStringLiteral("text entry")) {
    loaded = loadTextEntryElement(node) != nullptr;
  } else if (name == QStringLiteral("valuator")) {
    loaded = loadSliderElement(node) != nullptr;
  } else if (name == QStringLiteral("wheel switch")) {
    loaded = loadWheelSwitchElement(node) != nullptr;
  } else if (name == QStringLiteral("choice button")) {
    loaded = loadChoiceButtonElement(node) != nullptr;
  } else if (name == QStringLiteral("menu")) {
    loaded = loadMenuElement(node) != nullptr;
  } else if (name == QStringLiteral("message button")) {
    loaded = loadMessageButtonElement(node) != nullptr;
  } else if (name == QStringLiteral("shell command")) {
    loaded = loadShellCommandElement(node) != nullptr;
  } else if (name == QStringLiteral("related display")) {
    loaded = loadRelatedDisplayElement(node) != nullptr;
  } else if (name == QStringLiteral("meter")) {
    loaded = loadMeterElement(node) != nullptr;
  } else if (name == QStringLiteral("bar")) {
    loaded = loadBarMonitorElement(node) != nullptr;
  } else if (name == QStringLiteral("indicator")) {
    loaded = loadScaleMonitorElement(node) != nullptr;
  } else if (name == QStringLiteral("cartesian plot")) {
    loaded = loadCartesianPlotElement(node) != nullptr;
  } else if (name == QStringLiteral("strip chart")) {
    loaded = loadStripChartElement(node) != nullptr;
  } else if (name == QStringLiteral("byte")) {
    loaded = loadByteMonitorElement(node) != nullptr;
  } else if (name == QStringLiteral("image")) {
    loaded = loadImageElement(node) != nullptr;
  } else if (name == QStringLiteral("rectangle")) {
    loaded = loadRectangleElement(node) != nullptr;
  } else if (name == QStringLiteral("oval")) {
    loaded = loadOvalElement(node) != nullptr;
  } else if (name == QStringLiteral("arc")) {
    loaded = loadArcElement(node) != nullptr;
  } else if (name == QStringLiteral("polygon")) {
    loaded = loadPolygonElement(node) != nullptr;
  } else if (name == QStringLiteral("polyline") || name == QStringLiteral("line")) {
    loadPolylineElement(node);
    loaded = true;
  } else if (name == QStringLiteral("composite")) {
    loaded = loadCompositeElement(node) != nullptr;
  }

  /* Clear pending basic attribute after loading applicable widgets */
  if (loaded && (name == QStringLiteral("rectangle")
      || name == QStringLiteral("oval")
      || name == QStringLiteral("arc")
      || name == QStringLiteral("text")
      || name == QStringLiteral("polyline")
      || name == QStringLiteral("line")
      || name == QStringLiteral("polygon"))) {
    pendingBasicAttribute_ = std::nullopt;
    pendingDynamicAttribute_ = std::nullopt;
  }

  return loaded;
}
inline void DisplayWindow::setNextUndoLabel(const QString &label)
{
  pendingUndoLabel_ = label;
}

inline void DisplayWindow::updateDirtyFromUndoStack()
{
  const bool previous = dirty_;
  if (undoStack_) {
    dirty_ = !undoStack_->isClean();
  }
  updateDirtyIndicator();
  if (dirty_ != previous) {
    notifyMenus();
  }
}

inline bool DisplayWindow::restoreSerializedState(const QByteArray &data)
{
  if (data.isEmpty()) {
    return false;
  }

  const QString previousFilePath = filePath_;
  const QString previousTitle = windowTitle();
  const QString previousLoadDirectory = currentLoadDirectory_;

  suppressUndoCapture_ = true;
  restoringState_ = true;

  const QString textData = QString::fromLatin1(data);
  std::optional<AdlNode> document = AdlParser::parse(textData, nullptr);
  if (!document) {
    restoringState_ = false;
    suppressUndoCapture_ = false;
    return false;
  }

  clearAllElements();

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

  filePath_ = previousFilePath;
  if (!filePath_.isEmpty()) {
    setWindowTitle(QFileInfo(filePath_).fileName());
  } else if (!previousTitle.isEmpty()) {
    setWindowTitle(previousTitle);
  }
  currentLoadDirectory_ = previousLoadDirectory;

  if (displayArea_) {
    displayArea_->update();
  }
  update();
  refreshResourcePaletteGeometry();
  if (auto state = state_.lock()) {
    state->createTool = CreateTool::kNone;
  }

  lastCommittedState_ = serializeStateForUndo(filePath_);

  restoringState_ = false;
  suppressUndoCapture_ = false;

  updateDirtyFromUndoStack();
  notifyMenus();
  return displayLoaded || elementLoaded;
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
  if (restoringState_) {
    return;
  }
  const bool wasDirty = dirty_;
  dirty_ = true;

  if (!suppressUndoCapture_ && undoStack_) {
    const QByteArray before = lastCommittedState_;
    const QByteArray after = serializeStateForUndo(filePath_);
    if (before != after) {
      QString label = pendingUndoLabel_.trimmed();
      if (label.isEmpty()) {
        label = QStringLiteral("Modify Display");
      }
      auto *command = new DisplaySnapshotCommand(*this, before, after, label);
      suppressUndoCapture_ = true;
      undoStack_->push(command);
      suppressUndoCapture_ = false;
      lastCommittedState_ = after;
      pendingUndoLabel_.clear();
      updateDirtyFromUndoStack();
    } else {
      pendingUndoLabel_.clear();
    }
  }

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

inline void DisplayWindow::enterExecuteMode()
{
  if (executeModeActive_) {
    return;
  }
  executeModeActive_ = true;
  if (displayArea_) {
    displayArea_->setExecuteMode(true);
  }
  for (CompositeElement *element : compositeElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!compositeRuntimes_.contains(element)) {
      auto *runtime = new CompositeRuntime(element);
      compositeRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (TextElement *element : textElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!textRuntimes_.contains(element)) {
      auto *runtime = new TextRuntime(element);
      textRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (TextEntryElement *element : textEntryElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!textEntryRuntimes_.contains(element)) {
      auto *runtime = new TextEntryRuntime(element);
      textEntryRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (RectangleElement *element : rectangleElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!rectangleRuntimes_.contains(element)) {
      auto *runtime = new RectangleRuntime(element);
      rectangleRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (ImageElement *element : imageElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!imageRuntimes_.contains(element)) {
      auto *runtime = new ImageRuntime(element);
      imageRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (OvalElement *element : ovalElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!ovalRuntimes_.contains(element)) {
      auto *runtime = new OvalRuntime(element);
      ovalRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (ArcElement *element : arcElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!arcRuntimes_.contains(element)) {
      auto *runtime = new ArcRuntime(element);
      arcRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (LineElement *element : lineElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!lineRuntimes_.contains(element)) {
      auto *runtime = new LineRuntime(element);
      lineRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (PolylineElement *element : polylineElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!polylineRuntimes_.contains(element)) {
      auto *runtime = new PolylineRuntime(element);
      polylineRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (PolygonElement *element : polygonElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!polygonRuntimes_.contains(element)) {
      auto *runtime = new PolygonRuntime(element);
      polygonRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (MeterElement *element : meterElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!meterRuntimes_.contains(element)) {
      auto *runtime = new MeterRuntime(element);
      meterRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (ScaleMonitorElement *element : scaleMonitorElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!scaleMonitorRuntimes_.contains(element)) {
      auto *runtime = new ScaleMonitorRuntime(element);
      scaleMonitorRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (StripChartElement *element : stripChartElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!stripChartRuntimes_.contains(element)) {
      auto *runtime = new StripChartRuntime(element);
      stripChartRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (CartesianPlotElement *element : cartesianPlotElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!cartesianPlotRuntimes_.contains(element)) {
      auto *runtime = new CartesianPlotRuntime(element);
      cartesianPlotRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (BarMonitorElement *element : barMonitorElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!barMonitorRuntimes_.contains(element)) {
      auto *runtime = new BarMonitorRuntime(element);
      barMonitorRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (ByteMonitorElement *element : byteMonitorElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!byteMonitorRuntimes_.contains(element)) {
      auto *runtime = new ByteMonitorRuntime(element);
      byteMonitorRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (SliderElement *element : sliderElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!sliderRuntimes_.contains(element)) {
      auto *runtime = new SliderRuntime(element);
      sliderRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (WheelSwitchElement *element : wheelSwitchElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!wheelSwitchRuntimes_.contains(element)) {
      auto *runtime = new WheelSwitchRuntime(element);
      wheelSwitchRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (TextMonitorElement *element : textMonitorElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!textMonitorRuntimes_.contains(element)) {
      auto *runtime = new TextMonitorRuntime(element);
      textMonitorRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (ChoiceButtonElement *element : choiceButtonElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!choiceButtonRuntimes_.contains(element)) {
      auto *runtime = new ChoiceButtonRuntime(element);
      choiceButtonRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (MenuElement *element : menuElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!menuRuntimes_.contains(element)) {
      auto *runtime = new MenuRuntime(element);
      menuRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (MessageButtonElement *element : messageButtonElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
    if (!messageButtonRuntimes_.contains(element)) {
      auto *runtime = new MessageButtonRuntime(element);
      messageButtonRuntimes_.insert(element, runtime);
      runtime->start();
    }
  }
  for (ShellCommandElement *element : shellCommandElements_) {
    if (!element) {
      continue;
    }
    connectShellCommandElement(element);
  }
  for (RelatedDisplayElement *element : relatedDisplayElements_) {
    if (!element) {
      continue;
    }
    element->setExecuteMode(true);
  }

  /* Mimic MEDM's behavior: raise dynamic widgets above static widgets
   * MEDM draws static graphics to drawingAreaPixmap and dynamic widgets
   * to updatePixmap (layered on top). In Qt, we achieve the same effect
   * by calling raise() on widgets that need channels. */
  for (auto it = rectangleRuntimes_.constBegin();
      it != rectangleRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = imageRuntimes_.constBegin();
      it != imageRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = ovalRuntimes_.constBegin();
      it != ovalRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = arcRuntimes_.constBegin();
      it != arcRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = lineRuntimes_.constBegin();
      it != lineRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = polylineRuntimes_.constBegin();
      it != polylineRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = polygonRuntimes_.constBegin();
      it != polygonRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = textRuntimes_.constBegin();
      it != textRuntimes_.constEnd(); ++it) {
    if (auto *runtime = it.value()) {
      if (runtime->channelsNeeded_) {
        it.key()->raise();
      }
    }
  }
  for (auto it = textMonitorRuntimes_.constBegin();
      it != textMonitorRuntimes_.constEnd(); ++it) {
    /* TextMonitor always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = meterRuntimes_.constBegin();
      it != meterRuntimes_.constEnd(); ++it) {
    /* Meter always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = scaleMonitorRuntimes_.constBegin();
      it != scaleMonitorRuntimes_.constEnd(); ++it) {
    /* ScaleMonitor always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = barMonitorRuntimes_.constBegin();
      it != barMonitorRuntimes_.constEnd(); ++it) {
    /* BarMonitor always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = byteMonitorRuntimes_.constBegin();
      it != byteMonitorRuntimes_.constEnd(); ++it) {
    /* ByteMonitor always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = textEntryRuntimes_.constBegin();
      it != textEntryRuntimes_.constEnd(); ++it) {
    /* TextEntry always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = sliderRuntimes_.constBegin();
      it != sliderRuntimes_.constEnd(); ++it) {
    /* Slider always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = wheelSwitchRuntimes_.constBegin();
      it != wheelSwitchRuntimes_.constEnd(); ++it) {
    /* WheelSwitch always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = choiceButtonRuntimes_.constBegin();
      it != choiceButtonRuntimes_.constEnd(); ++it) {
    /* ChoiceButton always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = menuRuntimes_.constBegin();
      it != menuRuntimes_.constEnd(); ++it) {
    /* Menu always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = messageButtonRuntimes_.constBegin();
      it != messageButtonRuntimes_.constEnd(); ++it) {
    /* MessageButton always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = stripChartRuntimes_.constBegin();
      it != stripChartRuntimes_.constEnd(); ++it) {
    /* StripChart always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }
  for (auto it = cartesianPlotRuntimes_.constBegin();
      it != cartesianPlotRuntimes_.constEnd(); ++it) {
    /* CartesianPlot always needs channels, so always raise */
    if (it.key()) {
      it.key()->raise();
    }
  }

  /* ShellCommand and RelatedDisplay elements do not own runtimes, but in
   * MEDM they are realized as widgets that sit above the static graphics.
   * Explicitly raise them after all channel-driven widgets so they mimic the
   * same layering semantics. */
  for (ShellCommandElement *element : shellCommandElements_) {
    if (element) {
      element->raise();
    }
  }
  for (RelatedDisplayElement *element : relatedDisplayElements_) {
    if (element) {
      element->raise();
    }
  }
}

inline void DisplayWindow::leaveExecuteMode()
{
  if (!executeModeActive_) {
    return;
  }
  executeModeActive_ = false;
  if (displayArea_) {
    displayArea_->setExecuteMode(false);
  }
  cancelPvInfoPickMode();
  if (pvInfoDialog_) {
    pvInfoDialog_->hide();
  }
  cancelExecuteChannelDrag();
  for (auto it = compositeRuntimes_.begin(); it != compositeRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  compositeRuntimes_.clear();
  for (CompositeElement *element : compositeElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = textRuntimes_.begin(); it != textRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  textRuntimes_.clear();
  for (TextElement *element : textElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = textEntryRuntimes_.begin(); it != textEntryRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  textEntryRuntimes_.clear();
  for (TextEntryElement *element : textEntryElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = rectangleRuntimes_.begin(); it != rectangleRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  rectangleRuntimes_.clear();
  for (RectangleElement *element : rectangleElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = imageRuntimes_.begin(); it != imageRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  imageRuntimes_.clear();
  for (ImageElement *element : imageElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = ovalRuntimes_.begin(); it != ovalRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  ovalRuntimes_.clear();
  for (OvalElement *element : ovalElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = arcRuntimes_.begin(); it != arcRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  arcRuntimes_.clear();
  for (ArcElement *element : arcElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = lineRuntimes_.begin(); it != lineRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  lineRuntimes_.clear();
  for (LineElement *element : lineElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = polylineRuntimes_.begin(); it != polylineRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  polylineRuntimes_.clear();
  for (PolylineElement *element : polylineElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = polygonRuntimes_.begin(); it != polygonRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  polygonRuntimes_.clear();
  for (PolygonElement *element : polygonElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = meterRuntimes_.begin(); it != meterRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  meterRuntimes_.clear();
  for (MeterElement *element : meterElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = scaleMonitorRuntimes_.begin();
      it != scaleMonitorRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  scaleMonitorRuntimes_.clear();
  for (ScaleMonitorElement *element : scaleMonitorElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = stripChartRuntimes_.begin();
      it != stripChartRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  stripChartRuntimes_.clear();
  for (StripChartElement *element : stripChartElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = cartesianPlotRuntimes_.begin();
      it != cartesianPlotRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  cartesianPlotRuntimes_.clear();
  for (CartesianPlotElement *element : cartesianPlotElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = barMonitorRuntimes_.begin(); it != barMonitorRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  barMonitorRuntimes_.clear();
  for (BarMonitorElement *element : barMonitorElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = byteMonitorRuntimes_.begin(); it != byteMonitorRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  byteMonitorRuntimes_.clear();
  for (ByteMonitorElement *element : byteMonitorElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = sliderRuntimes_.begin(); it != sliderRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  sliderRuntimes_.clear();
  for (SliderElement *element : sliderElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = wheelSwitchRuntimes_.begin(); it != wheelSwitchRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  wheelSwitchRuntimes_.clear();
  for (WheelSwitchElement *element : wheelSwitchElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = textMonitorRuntimes_.begin(); it != textMonitorRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  textMonitorRuntimes_.clear();
  for (TextMonitorElement *element : textMonitorElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = choiceButtonRuntimes_.begin(); it != choiceButtonRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  choiceButtonRuntimes_.clear();
  for (ChoiceButtonElement *element : choiceButtonElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = menuRuntimes_.begin(); it != menuRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  menuRuntimes_.clear();
  for (MenuElement *element : menuElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (auto it = messageButtonRuntimes_.begin(); it != messageButtonRuntimes_.end(); ++it) {
    if (auto *runtime = it.value()) {
      runtime->stop();
      runtime->deleteLater();
    }
  }
  messageButtonRuntimes_.clear();
  for (MessageButtonElement *element : messageButtonElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (ShellCommandElement *element : shellCommandElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
  for (RelatedDisplayElement *element : relatedDisplayElements_) {
    if (element) {
      element->setExecuteMode(false);
    }
  }
}

inline void DisplayWindow::removeTextRuntime(TextElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = textRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeTextEntryRuntime(TextEntryElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = textEntryRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeArcRuntime(ArcElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = arcRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeOvalRuntime(OvalElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = ovalRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeLineRuntime(LineElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = lineRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeRectangleRuntime(RectangleElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = rectangleRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeImageRuntime(ImageElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = imageRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removePolylineRuntime(PolylineElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = polylineRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removePolygonRuntime(PolygonElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = polygonRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeMeterRuntime(MeterElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = meterRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeBarMonitorRuntime(BarMonitorElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = barMonitorRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeScaleMonitorRuntime(ScaleMonitorElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = scaleMonitorRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeStripChartRuntime(StripChartElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = stripChartRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeCartesianPlotRuntime(CartesianPlotElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = cartesianPlotRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeByteMonitorRuntime(ByteMonitorElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = byteMonitorRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeSliderRuntime(SliderElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = sliderRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeWheelSwitchRuntime(WheelSwitchElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = wheelSwitchRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeChoiceButtonRuntime(ChoiceButtonElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = choiceButtonRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeMenuRuntime(MenuElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = menuRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
  }
}

inline void DisplayWindow::removeMessageButtonRuntime(MessageButtonElement *element)
{
  if (!element) {
    return;
  }
  if (auto *runtime = messageButtonRuntimes_.take(element)) {
    runtime->stop();
    runtime->deleteLater();
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
