#pragma once

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QBuffer>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QPolygon>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QRadioButton>
#include <QRect>
#include <QResizeEvent>
#include <QRubberBand>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QShortcut>
#include <QSet>
#include <QSizePolicy>
#include <QStyleFactory>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QUndoCommand>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "adl_parser.h"
#include "adl_writer.h"
#include "arc_element.h"
#include "arc_runtime.h"
#include "cartesian_plot_element.h"
#include "cartesian_plot_runtime.h"
#include "byte_monitor_element.h"
#include "byte_monitor_runtime.h"
#include "led_monitor_element.h"
#include "led_monitor_runtime.h"
#include "bar_monitor_element.h"
#include "bar_monitor_runtime.h"
#include "thermometer_element.h"
#include "thermometer_runtime.h"
#include "scale_monitor_runtime.h"
#include "slider_element.h"
#include "slider_runtime.h"
#include "soft_pv_registry.h"
#include "wheel_switch_element.h"
#include "wheel_switch_runtime.h"
#include "scale_monitor_element.h"
#include "cartesian_plot_properties.h"
#include "control_properties.h"
#include "display_common_properties.h"
#include "graphic_properties.h"
#include "heatmap_properties.h"
#include "monitor_properties.h"
#include "text_properties.h"
#include "time_units.h"
#include "waterfall_plot_properties.h"
#include "display_state.h"
#include "expression_channel_element.h"
#include "expression_channel_runtime.h"
#include "image_element.h"
#include "image_runtime.h"
#include "heatmap_element.h"
#include "heatmap_runtime.h"
#include "waterfall_plot_element.h"
#include "waterfall_plot_runtime.h"
#include "meter_element.h"
#include "meter_runtime.h"
#include "line_element.h"
#include "line_runtime.h"
#include "medm_colors.h"
#include "oval_element.h"
#include "oval_runtime.h"
#include "strip_chart_element.h"
#include "strip_chart_runtime.h"
#include "polygon_element.h"
#include "polyline_element.h"
#include "polyline_runtime.h"
#include "polygon_runtime.h"
#include "rectangle_element.h"
#include "rectangle_runtime.h"
#include "composite_element.h"
#include "composite_runtime.h"
#include "resource_palette_dialog.h"
#include "text_element.h"
#include "text_runtime.h"
#include "choice_button_element.h"
#include "choice_button_runtime.h"
#include "menu_element.h"
#include "menu_runtime.h"
#include "message_button_element.h"
#include "message_button_runtime.h"
#include "shell_command_element.h"
#include "related_display_element.h"
#include "text_entry_element.h"
#include "text_entry_runtime.h"
#include "setpoint_control_element.h"
#include "setpoint_control_runtime.h"
#include "text_area_element.h"
#include "text_area_runtime.h"
#include "text_monitor_element.h"
#include "text_monitor_runtime.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include <cmath>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <cstring>
#include <cstdlib>

#include <QDebug>
#include <QClipboard>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QDateTime>
#include <QCursor>
#include <QFont>
#include <QPageSetupDialog>
#include <QPen>
#include <QPainter>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrinter>
#include <QScreen>
#include <QSize>
#include <QLabel>
#include <QMimeData>
#include <QPointer>
#include <QList>
#include <QTimer>
#include <QSet>
#include <QStringList>
#include <QEventLoop>
#include <QTextStream>
#include <QUrl>
#include <QVariant>
#include <QSvgGenerator>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonObject>

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

#include "pv_table_element.h"
#include "pv_table_runtime.h"
#include "wave_table_element.h"
#include "wave_table_runtime.h"
#include "statistics_tracker.h"
#include "display_list_dialog.h"
#include "find_pv_dialog.h"
#include "pv_protocol.h"
#include "pv_info_dialog.h"
#include "pv_limits_dialog.h"
#include "pva_info_snapshot.h"
#include "runtime_utils.h"
#include "strip_chart_data_dialog.h"
#include "cursor_utils.h"

namespace {
constexpr double kChannelRetryTimeoutSeconds = 1.0;
constexpr double kPvInfoTimeoutSeconds = 1.0;
constexpr qint64 kEpicsEpochOffsetSeconds = 631152000; // 1990-01-01 -> 1970-01-01
constexpr unsigned long kPvInfoArrayFetchLimit = 100000;
constexpr int kPvInfoArrayPreviewCount = 5;
constexpr int kResizeDebounceMs = 80;
constexpr char kWidgetHasDynamicAttributeProperty[] =
    "_adlHasDynamicAttribute";
constexpr char kOriginalAdlGeometryProperty[] = "_adlOriginalGeometry";
constexpr char kAdlGeometryEditedProperty[] = "_adlGeometryEdited";

template <typename ElementType, int ChannelCount = 5>
bool hasConfiguredLayeringChannel(const ElementType *element)
{
  if (!element) {
    return false;
  }
  for (int i = 0; i < ChannelCount; ++i) {
    if (!element->channel(i).isEmpty()) {
      return true;
    }
  }
  return false;
}
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

// Compute "nice" axis limits for a range [xmin, xmax] with n divisions.
// This replicates the linear_scale algorithm from medm/medmStripChart.c
// used to convert legacy delay+units to period values.
inline void linearScale(double xmin, double xmax, int n,
                        double *xminp, double *xmaxp, double *dist)
{
  static const double vint[4] = { 1.0, 2.0, 5.0, 10.0 };
  static const double sqr[3] = { 1.414214, 3.162278, 7.071068 };
  const double del = 0.0000002; // account for round-off errors

  if (!(xmin <= xmax && n > 0)) {
    return;
  }

  // Provide 10% spread if graph is parallel to an axis
  if (xmax == xmin) {
    xmax *= 1.05;
    xmin *= 0.95;
  }

  const double fn = static_cast<double>(n);

  // Find approximate interval size
  const double a = (xmax - xmin) / fn;
  const double al = std::log10(a);
  int nal = static_cast<int>(al);
  if (a < 1.0) {
    nal -= 1;
  }

  // Scale 'a' into variable 'b' between 1 and 10
  const double b = a / std::pow(10.0, static_cast<double>(nal));

  // Find the closest permissible value for b
  int i = 4;
  for (int j = 1; j < 4; ++j) {
    if (b < sqr[j - 1]) {
      i = j;
      break;
    }
  }

  // Compute the interval size
  *dist = vint[i - 1] * std::pow(10.0, static_cast<double>(nal));

  const double fm1 = xmin / (*dist);
  int m1 = static_cast<int>(fm1);
  if (fm1 < 0.0) {
    m1 -= 1;
  }
  if (std::abs(static_cast<double>(m1) + 1.0 - fm1) < del) {
    m1 += 1;
  }

  // New min limit
  *xminp = (*dist) * static_cast<double>(m1);

  const double fm2 = xmax / (*dist);
  int m2 = static_cast<int>(fm2) + 1;
  if (fm2 < -1.0) {
    m2 -= 1;
  }
  if (std::abs(fm2 + 1.0 - static_cast<double>(m2)) < del) {
    m2 -= 1;
  }

  // New max limit
  *xmaxp = (*dist) * static_cast<double>(m2);

  // Adjust limits to account for round-off if necessary
  if (*xminp > xmin) {
    *xminp = xmin;
  }
  if (*xmaxp < xmax) {
    *xmaxp = xmax;
  }
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

class HiddenButtonMarkerWidget : public QWidget
{
public:
  explicit HiddenButtonMarkerWidget(QWidget *parent = nullptr)
    : QWidget(parent)
    , animationPhase_(0)
  {
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    animationTimer_ = new QTimer(this);
    animationTimer_->setInterval(200);
    QObject::connect(animationTimer_, &QTimer::timeout, this, [this]() {
      animationPhase_ = (animationPhase_ + 1) % 4;
      update();
    });
    animationTimer_->start();
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QPen pen;
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    pen.setDashOffset(animationPhase_ * 2);

    if ((animationPhase_ % 2) == 0) {
      pen.setColor(Qt::white);
    } else {
      pen.setColor(Qt::black);
    }

    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    QRect borderRect = rect().adjusted(1, 1, -1, -1);
    painter.drawRect(borderRect);
  }

private:
  QTimer *animationTimer_ = nullptr;
  int animationPhase_ = 0;
};

class DisplayWindow : public QMainWindow
{

  friend class DisplayStackingEventFilter;
public:
  class DisplayStackingEventFilter : public QObject
  {
  public:
    explicit DisplayStackingEventFilter(DisplayWindow *owner)
      : QObject(owner)
      , owner_(owner)
    {
    }

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
      if (!owner_) {
        return QObject::eventFilter(watched, event);
      }
      switch (event->type()) {
      case QEvent::ShowToParent:
      case QEvent::HideToParent:
      case QEvent::ParentChange:
      case QEvent::ZOrderChange:
        owner_->requestStackingOrderRefresh();
        break;
      default:
        break;
      }
      return QObject::eventFilter(watched, event);
    }

  private:
    DisplayWindow *owner_ = nullptr;
  };
  /* Format a display window title with QtEDM prefix for visual distinction.
   * When macros are provided, they are appended in brackets after the filename
   * to help operators distinguish multiple instances of the same display
   * opened with different macro substitutions. */
  static QString formatDisplayTitle(const QString &filename,
      const QHash<QString, QString> &macros = {});

  DisplayWindow(const QPalette &displayPalette, const QPalette &uiPalette,
      const QFont &font, const QFont &labelFont,
      std::weak_ptr<DisplayState> state, QWidget *parent = nullptr);

  ~DisplayWindow() override;

  int gridSpacing() const;

  void setGridSpacing(int spacing);

  bool isGridOn() const;

  void setGridOn(bool gridOn);

  bool isSnapToGridEnabled() const;

  void setSnapToGrid(bool snap);

  void promptForGridSpacing();

  void syncCreateCursor();

  void setCreateTool(CreateTool tool);

  void clearSelection();

  void selectAllElements();

  void selectDisplayElement();

  void enterExecuteMode();
  void leaveExecuteMode();
  void triggerUndo();

  void triggerRedo();

  void handleEditModeChanged(bool editMode);

  void findOutliers();

  void showEditSummaryDialog();

  void refreshDisplayView();

  void toggleHiddenButtonMarkers();

  void destroyHiddenButtonMarkers();

  void createHiddenButtonMarkers();

  void collectHiddenButtons(const QList<RelatedDisplayElement *> &elements, QList<RelatedDisplayElement *> &result);

  void collectHiddenButtonsInComposite(CompositeElement *composite, QList<RelatedDisplayElement *> &result);

  QWidget *createHiddenButtonMarker(RelatedDisplayElement *element);

  void cutSelection();

  void copySelection();

  void pasteSelection();

  void triggerCutFromMenu();

  void triggerCopyFromMenu();

  void triggerPasteFromMenu();

  void triggerGroupFromMenu();

  void triggerUngroupFromMenu();

  void raiseSelection();

  void lowerSelection();

  void groupSelectedElements();

  void ungroupSelectedElements();

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
  void alignSelectionLeft();

  void alignSelectionHorizontalCenter();

  void alignSelectionRight();

  void alignSelectionTop();

  void alignSelectionVerticalCenter();

  void alignSelectionBottom();

  void centerSelectionHorizontallyInDisplay();

  void centerSelectionVerticallyInDisplay();

  void centerSelectionInDisplayBoth();

  void orientSelectionFlipHorizontal();

  void orientSelectionFlipVertical();

  void rotateSelectionClockwise();

  void rotateSelectionCounterclockwise();

  void sizeSelectionSameSize();

  void sizeSelectionTextToContents();

  void alignSelectionPositionToGrid();

  void alignSelectionEdgesToGrid();

  void spaceSelectionHorizontal();

  void spaceSelectionVertical();

  void spaceSelection2D();

  bool canRaiseSelection() const;

  bool canLowerSelection() const;

  bool canGroupSelection() const;

  bool canUngroupSelection() const;

  bool canAlignSelection() const;

  bool canAlignSelectionToGrid() const;

  bool canOrientSelection() const;

  bool canSizeSelectionSameSize() const;

  bool canSizeSelectionTextToContents() const;

  bool canSpaceSelection() const;

  bool canSpaceSelection2D() const;

  bool canCenterSelection() const;

  bool hasCopyableSelection() const;

  bool canPaste() const;

  bool save(QWidget *dialogParent = nullptr);
  bool saveAs(QWidget *dialogParent = nullptr);
  bool saveToPath(const QString &filePath) const;
  bool saveScreenshotToPath(const QString &filePath) const;
  bool isTestAutomationReady() const;

  void showPrintSetup();

  void printDisplay();

  void exportDisplayImage();

  void exportStripChartData(StripChartElement *stripChart);

  void exportCartesianPlotData(CartesianPlotElement *plot);

  void exportWaterfallPlotImage(WaterfallPlotElement *plot);

  void exportWaterfallPlotData(WaterfallPlotElement *plot);

  bool loadFromFile(const QString &filePath, QString *errorMessage = nullptr,
      const QHash<QString, QString> &macros = {});
  QString filePath() const;

  QJsonObject testStateObject() const;

  const QHash<QString, QString> &macroDefinitions() const;

  bool isDirty() const;

  bool hasFilePath() const;

  QUndoStack *undoStack() const;

  bool isPvInfoPickingActive() const;

  bool isPvLimitsPickingActive() const;

protected:
  void focusInEvent(QFocusEvent *event) override;

  void keyPressEvent(QKeyEvent *event) override;

  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

  void mouseMoveEvent(QMouseEvent *event) override;

  void mouseReleaseEvent(QMouseEvent *event) override;

  void mouseDoubleClickEvent(QMouseEvent *event) override;

  void dragEnterEvent(QDragEnterEvent *event) override;

  void dropEvent(QDropEvent *event) override;

private:
  bool writeAdlFile(const QString &filePath) const;
  void clearAllElements();
  void applyExecuteResizeScale(int newWidth, int newHeight);
  void scaleAllElements(int oldWidth, int oldHeight, int newWidth, int newHeight);
  void handleDroppedAdlFiles(const QStringList &filePaths);
  QString convertLegacyAdlFormat(const QString &adlText, int fileVersion) const;
  bool loadDisplaySection(const AdlNode &displayNode);
  TextElement *loadTextElement(const AdlNode &textNode);
  TextMonitorElement *loadTextMonitorElement(const AdlNode &textUpdateNode);
  PvTableElement *loadPvTableElement(const AdlNode &pvTableNode);
  WaveTableElement *loadWaveTableElement(const AdlNode &waveTableNode);
  TextEntryElement *loadTextEntryElement(const AdlNode &textEntryNode);
  SetpointControlElement *loadSetpointControlElement(
      const AdlNode &setpointControlNode);
  TextAreaElement *loadTextAreaElement(const AdlNode &textAreaNode);
  SliderElement *loadSliderElement(const AdlNode &valuatorNode);
  WheelSwitchElement *loadWheelSwitchElement(const AdlNode &wheelNode);
  ChoiceButtonElement *loadChoiceButtonElement(const AdlNode &choiceNode);
  MenuElement *loadMenuElement(const AdlNode &menuNode);
  MessageButtonElement *loadMessageButtonElement(const AdlNode &messageNode);
  ShellCommandElement *loadShellCommandElement(const AdlNode &shellNode);
  RelatedDisplayElement *loadRelatedDisplayElement(const AdlNode &relatedNode);
  MeterElement *loadMeterElement(const AdlNode &meterNode);
  BarMonitorElement *loadBarMonitorElement(const AdlNode &barNode);
  ThermometerElement *loadThermometerElement(const AdlNode &thermometerNode);
  ScaleMonitorElement *loadScaleMonitorElement(const AdlNode &indicatorNode);
  CartesianPlotElement *loadCartesianPlotElement(const AdlNode &cartesianNode);
  StripChartElement *loadStripChartElement(const AdlNode &stripNode);
  ByteMonitorElement *loadByteMonitorElement(const AdlNode &byteNode);
  LedMonitorElement *loadLedMonitorElement(const AdlNode &ledNode);
  ExpressionChannelElement *loadExpressionChannelElement(
      const AdlNode &expressionChannelNode);
  ImageElement *loadImageElement(const AdlNode &imageNode);
  HeatmapElement *loadHeatmapElement(const AdlNode &heatmapNode);
  WaterfallPlotElement *loadWaterfallPlotElement(
      const AdlNode &waterfallNode);
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
  void insertElementInStack(int insertIndex, QWidget *element);
  void ensureElementInStack(QWidget *element);
  void refreshStackingOrder();
  void requestStackingOrderRefresh();
  void markWidgetHasDynamicAttribute(QWidget *widget) const;
  bool widgetHasDynamicAttribute(const QWidget *widget) const;
  void recordWidgetOriginalGeometry(QWidget *widget,
      const QRect &geometry) const;
  void markWidgetGeometryEdited(QWidget *widget) const;
  bool widgetGeometryEdited(const QWidget *widget) const;
  QRect widgetGeometryForSerialization(const QWidget *widget) const;
  bool isControlWidget(const QWidget *widget) const;
  bool isMedmWidgetBackedWidget(const QWidget *widget) const;
  bool needsGraphicDynamicLayer(const QWidget *widget) const;
  bool isStaticGraphicWidget(const QWidget *widget) const;
  QColor colorForIndex(int index) const;
  QRect widgetDisplayRect(const QWidget *widget) const;
  void setWidgetDisplayRect(QWidget *widget, const QRect &displayRect) const;
  QRect absoluteGeometryForWidget(const QWidget *widget,
      const QRect &localGeometry) const;
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
  LedShape parseLedShape(const QString &value) const;
  ChoiceButtonStacking parseChoiceButtonStacking(const QString &value) const;
  RelatedDisplayVisual parseRelatedDisplayVisual(const QString &value) const;
  RelatedDisplayMode parseRelatedDisplayMode(const QString &value) const;
  TextAreaWrapMode parseTextAreaWrapMode(const QString &value) const;
  TextAreaCommitMode parseTextAreaCommitMode(const QString &value) const;
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
  HeatmapDimensionSource parseHeatmapDimensionSource(const QString &value) const;
  HeatmapOrder parseHeatmapOrder(const QString &value) const;
  HeatmapRotation parseHeatmapRotation(const QString &value) const;
  HeatmapColorMap parseHeatmapColorMap(const QString &value) const;
  HeatmapProfileMode parseHeatmapProfileMode(const QString &value) const;
  bool parseHeatmapBool(const QString &value) const;
  WaterfallScrollDirection parseWaterfallScrollDirection(
      const QString &value) const;
  WaterfallIntensityScale parseWaterfallIntensityScale(
      const QString &value) const;
  WaterfallEraseMode parseWaterfallEraseMode(const QString &value) const;
  TextMonitorFormat parseTextMonitorFormat(const QString &value) const;
  PvLimitSource parseLimitSource(const QString &value) const;
  Qt::Alignment parseAlignment(const QString &value) const;
  ExpressionChannelEventSignalMode parseExpressionChannelEventSignalMode(
      const QString &value) const;
  QString expressionChannelEventSignalModeString(
      ExpressionChannelEventSignalMode mode) const;
  void setAsActiveDisplay();
  void markDirty();
  void notifyMenus() const;
  void updateDirtyIndicator();

  std::weak_ptr<DisplayState> state_;
  QFont labelFont_;
  QPalette resourcePaletteBase_;
  ResourcePaletteDialog *resourcePalette_ = nullptr;
  DisplayAreaWidget *displayArea_ = nullptr;
  std::unique_ptr<QPrinter> printer_;
  QString filePath_;
  QString currentLoadDirectory_;
  bool loadingLegacyAdl_ = false;
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
  bool preserveNextLoadPosition_ = false;
  QPoint preservedLoadPosition_;
  QPoint originalDisplayPosition_;
  bool hasOriginalDisplayX_ = false;
  bool hasOriginalDisplayY_ = false;
  bool displaySelected_ = false;
  bool gridOn_ = kDefaultGridOn;
  bool snapToGrid_ = kDefaultSnapToGrid;
  int gridSpacing_ = kDefaultGridSpacing;
  int originalDisplayWidth_ = 0;
  int originalDisplayHeight_ = 0;
  bool resizeScalingEnabled_ = true;
  QTimer *resizeDebounceTimer_ = nullptr;
  QSize pendingResizeAreaSize_ = QSize(0, 0);
  QSize lastScaledAreaSize_ = QSize(0, 0);
  QPoint lastContextMenuGlobalPos_;
  struct ExecuteMenuEntry {
    QString label;
    QString command;
  };

  struct PvInfoChannelRef {
    QString name;
    chid channelId = nullptr;
  };

  struct PvInfoContent {
    QString text;
    QVector<PvInfoArrayData> arrays;
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
    QString units;
    bool hasUnits = false;
    QStringList states;
    bool hasStates = false;
    bool hasArrayData = false;
    PvInfoArrayData arrayData;
    QString arrayMessage;
    bool producedByExpressionChannel = false;
    QString expressionCalc;
    QStringList expressionChannels;
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
  QPointer<PvLimitsDialog> pvLimitsDialog_;
  bool pvLimitsPickingActive_ = false;
  bool pvLimitsCursorInitialized_ = false;
  bool pvLimitsCursorActive_ = false;
  QCursor pvLimitsCursor_;
  QPointer<StripChartDataDialog> stripChartDataDialog_;
  QList<TextElement *> textElements_;
  TextElement *selectedTextElement_ = nullptr;
  QHash<TextElement *, TextRuntime *> textRuntimes_;
  QList<TextEntryElement *> textEntryElements_;
  TextEntryElement *selectedTextEntryElement_ = nullptr;
  QHash<TextEntryElement *, TextEntryRuntime *> textEntryRuntimes_;
  QList<SetpointControlElement *> setpointControlElements_;
  SetpointControlElement *selectedSetpointControl_ = nullptr;
  QHash<SetpointControlElement *, SetpointControlRuntime *>
      setpointControlRuntimes_;
  QList<TextAreaElement *> textAreaElements_;
  TextAreaElement *selectedTextAreaElement_ = nullptr;
  QHash<TextAreaElement *, TextAreaRuntime *> textAreaRuntimes_;
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
  QList<PvTableElement *> pvTableElements_;
  QHash<PvTableElement *, PvTableRuntime *> pvTableRuntimes_;
  PvTableElement *selectedPvTable_ = nullptr;
  QList<WaveTableElement *> waveTableElements_;
  QHash<WaveTableElement *, WaveTableRuntime *> waveTableRuntimes_;
  WaveTableElement *selectedWaveTable_ = nullptr;
  QList<MeterElement *> meterElements_;
  QHash<MeterElement *, MeterRuntime *> meterRuntimes_;
  MeterElement *selectedMeterElement_ = nullptr;
  QList<BarMonitorElement *> barMonitorElements_;
  QHash<BarMonitorElement *, BarMonitorRuntime *> barMonitorRuntimes_;
  BarMonitorElement *selectedBarMonitorElement_ = nullptr;
  QList<ThermometerElement *> thermometerElements_;
  QHash<ThermometerElement *, ThermometerRuntime *> thermometerRuntimes_;
  ThermometerElement *selectedThermometerElement_ = nullptr;
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
  QList<LedMonitorElement *> ledMonitorElements_;
  QHash<LedMonitorElement *, LedMonitorRuntime *> ledMonitorRuntimes_;
  LedMonitorElement *selectedLedMonitorElement_ = nullptr;
  QList<ExpressionChannelElement *> expressionChannelElements_;
  QHash<ExpressionChannelElement *, ExpressionChannelRuntime *>
      expressionChannelRuntimes_;
  ExpressionChannelElement *selectedExpressionChannel_ = nullptr;
  QList<RectangleElement *> rectangleElements_;
  QHash<RectangleElement *, RectangleRuntime *> rectangleRuntimes_;
  RectangleElement *selectedRectangle_ = nullptr;
  QList<ImageElement *> imageElements_;
  QHash<ImageElement *, ImageRuntime *> imageRuntimes_;
  ImageElement *selectedImage_ = nullptr;
  QList<HeatmapElement *> heatmapElements_;
  QHash<HeatmapElement *, HeatmapRuntime *> heatmapRuntimes_;
  HeatmapElement *selectedHeatmap_ = nullptr;
  QList<WaterfallPlotElement *> waterfallPlotElements_;
  QHash<WaterfallPlotElement *, WaterfallPlotRuntime *>
      waterfallPlotRuntimes_;
  WaterfallPlotElement *selectedWaterfallPlot_ = nullptr;
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
  QHash<const QWidget *, QRect> originalAdlGeometries_;
  QHash<const QWidget *, QVector<QPoint>> originalPolylinePoints_;
  bool stackingRefreshPending_ = false;
  bool stackingOrderInternallyUpdating_ = false;
  DisplayStackingEventFilter *stackingEventFilter_ = nullptr;
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
  QList<QWidget *> hiddenButtonMarkers_;
  bool hiddenButtonMarkersVisible_ = false;


  void setDisplaySelected(bool selected);

  void clearDisplaySelection();

  void clearTextSelection();

  void clearTextEntrySelection();

  void clearSetpointControlSelection();

  void clearTextAreaSelection();

  void clearSliderSelection();

  void clearWheelSwitchSelection();

  void clearChoiceButtonSelection();

  void clearMenuSelection();

  void clearMessageButtonSelection();

  void clearShellCommandSelection();

  void clearRelatedDisplaySelection();

  void clearTextMonitorSelection();

  void clearPvTableSelection();

  void clearWaveTableSelection();

  void clearMeterSelection();

  void clearScaleMonitorSelection();

  void clearStripChartSelection();

  void clearCartesianPlotSelection();

  void clearBarMonitorSelection();

  void clearThermometerSelection();

  void clearByteMonitorSelection();

  void clearLedMonitorSelection();

  void clearExpressionChannelSelection();

  void clearRectangleSelection();

  void clearImageSelection();

  void clearHeatmapSelection();

  void clearWaterfallPlotSelection();

  void clearOvalSelection();

  void clearArcSelection();

  void clearLineSelection();

  void clearPolylineSelection();

  void clearPolygonSelection();

  void clearCompositeSelection();

  void setWidgetSelectionState(QWidget *widget, bool selected);

  void flashWidget(QWidget *widget);

  void clearMultiSelection();

  bool isWidgetInMultiSelection(QWidget *widget) const;

  void detachSingleSelectionForWidget(QWidget *widget);

  void addWidgetToMultiSelection(QWidget *widget);

  void removeWidgetFromMultiSelection(QWidget *widget);

  void removeWidgetFromSelection(QWidget *widget);

  void pruneMultiSelection();

  void updateSelectionAfterMultiChange();

  QList<QWidget *> selectedWidgets() const;

  QList<QWidget *> alignableWidgets() const;

  QList<QWidget *> selectedWidgetsInStackOrder(bool ascending) const;

  void clearSelections();

  void removeTextRuntime(TextElement *element);
  void removeCompositeRuntime(CompositeElement *element);
  void removeSliderRuntime(SliderElement *element);
  void removeChoiceButtonRuntime(ChoiceButtonElement *element);
  void removeMenuRuntime(MenuElement *element);
  void removeMessageButtonRuntime(MessageButtonElement *element);
  void removeArcRuntime(ArcElement *element);
  void removeOvalRuntime(OvalElement *element);
  void removeLineRuntime(LineElement *element);
  void removeRectangleRuntime(RectangleElement *element);
  void removeImageRuntime(ImageElement *element);
  void removeHeatmapRuntime(HeatmapElement *element);
  void removeWaterfallPlotRuntime(WaterfallPlotElement *element);
  void removePolylineRuntime(PolylineElement *element);
  void removePolygonRuntime(PolygonElement *element);
  void removeMeterRuntime(MeterElement *element);
  void removeBarMonitorRuntime(BarMonitorElement *element);
  void removeThermometerRuntime(ThermometerElement *element);
  void removeScaleMonitorRuntime(ScaleMonitorElement *element);
  void removeStripChartRuntime(StripChartElement *element);
  void removeCartesianPlotRuntime(CartesianPlotElement *element);
  void removeByteMonitorRuntime(ByteMonitorElement *element);
  void removeLedMonitorRuntime(LedMonitorElement *element);
  void removeExpressionChannelRuntime(ExpressionChannelElement *element);
  void removeWheelSwitchRuntime(WheelSwitchElement *element);
  void removeTextMonitorRuntime(TextMonitorElement *element);
  void removePvTableRuntime(PvTableElement *element);
  void removeWaveTableRuntime(WaveTableElement *element);
  void removeTextEntryRuntime(TextEntryElement *element);
  void removeSetpointControlRuntime(SetpointControlElement *element);
  void removeTextAreaRuntime(TextAreaElement *element);

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
    } else if constexpr (std::is_same_v<ElementType, SetpointControlElement>) {
      removeSetpointControlRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, TextAreaElement>) {
      removeTextAreaRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, SliderElement>) {
      removeSliderRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, WheelSwitchElement>) {
      removeWheelSwitchRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, PvTableElement>) {
      removePvTableRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, WaveTableElement>) {
      removeWaveTableRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, MeterElement>) {
      removeMeterRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, BarMonitorElement>) {
      removeBarMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ThermometerElement>) {
      removeThermometerRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ScaleMonitorElement>) {
      removeScaleMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, StripChartElement>) {
      removeStripChartRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, CartesianPlotElement>) {
      removeCartesianPlotRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ByteMonitorElement>) {
      removeByteMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, LedMonitorElement>) {
      removeLedMonitorRuntime(element);
    } else if constexpr (std::is_same_v<ElementType, ExpressionChannelElement>) {
      removeExpressionChannelRuntime(element);
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

  bool copySelectionInternal(bool removeOriginal);

  void pasteFromClipboard();

  bool hasAnyElementSelection() const;

  bool hasAnySelection() const;

  bool hasSelectableElements() const;

  bool canSelectAllElements() const;

  bool canSelectDisplay() const;

  void closeResourcePalette();

  void handleResourcePaletteClosed();

  ResourcePaletteDialog *ensureResourcePalette();

  void showResourcePaletteForDisplay();

  void showResourcePaletteForMultipleSelection();

  void showResourcePaletteForText(TextElement *element);

  void showResourcePaletteForTextEntry(TextEntryElement *element);

  void showResourcePaletteForSetpointControl(SetpointControlElement *element);

  void showResourcePaletteForTextArea(TextAreaElement *element);

  void showResourcePaletteForSlider(SliderElement *element);

  void showResourcePaletteForWheelSwitch(WheelSwitchElement *element);

  void showResourcePaletteForChoiceButton(ChoiceButtonElement *element);

  void showResourcePaletteForMenu(MenuElement *element);

  void showResourcePaletteForMessageButton(MessageButtonElement *element);

  void showResourcePaletteForShellCommand(ShellCommandElement *element);

  void showResourcePaletteForRelatedDisplay(RelatedDisplayElement *element);

  void showResourcePaletteForPvTable(PvTableElement *element);

  void showResourcePaletteForWaveTable(WaveTableElement *element);

  void showResourcePaletteForTextMonitor(TextMonitorElement *element);

  void showResourcePaletteForMeter(MeterElement *element);

  void showResourcePaletteForStripChart(StripChartElement *element);

  void showResourcePaletteForCartesianPlot(CartesianPlotElement *element);

  void showResourcePaletteForBar(BarMonitorElement *element);

  void showResourcePaletteForThermometer(ThermometerElement *element);

  void showResourcePaletteForScale(ScaleMonitorElement *element);

  void showResourcePaletteForByte(ByteMonitorElement *element);

  void showResourcePaletteForLedMonitor(LedMonitorElement *element);

  void showResourcePaletteForComposite(CompositeElement *element);

  void showResourcePaletteForRectangle(RectangleElement *element);

  void showResourcePaletteForExpressionChannel( ExpressionChannelElement *element);

  void showResourcePaletteForImage(ImageElement *element);

  void showResourcePaletteForHeatmap(HeatmapElement *element);

  void showResourcePaletteForWaterfall(WaterfallPlotElement *element);

  void showResourcePaletteForOval(OvalElement *element);

  void showResourcePaletteForArc(ArcElement *element);

  void showResourcePaletteForLine(LineElement *element);

  void showResourcePaletteForPolyline(PolylineElement *element);

  void showResourcePaletteForPolygon(PolygonElement *element);

  enum class CompositeHitMode
  {
    kAuto,
    kRejectChildren,
    kIncludeChildren,
  };

  QWidget *elementAt(const QPoint &windowPos, CompositeHitMode mode = CompositeHitMode::kAuto) const;

  QStringList channelsForWidget(QWidget *widget) const;

public:
  /* Returns all widgets that may have PV channels associated with them */ QList<QWidget *> findPvWidgets() const;

  bool hasPvaChannels() const;

  /* Select a single widget and scroll to make it visible */ void selectAndScrollToWidget(QWidget *widget);

  /* Select multiple widgets */ void selectWidgets(const QList<QWidget *> &widgets);

private:
  void startPvInfoPickMode();

  void cancelPvInfoPickMode();

  void completePvInfoPick(const QPoint &windowPos);

  void showPvInfoContent(const PvInfoContent &content);

  void startPvLimitsPickMode();

  void cancelPvLimitsPickMode();

  void completePvLimitsPick(const QPoint &windowPos);

  void showPvLimitsForWidget(QWidget *widget);

  void showStripChartDataDialog(StripChartElement *element);

  PvLimitsDialog *ensurePvLimitsDialog();

  QCursor createPvLimitsCursor() const;

  QCursor createPvInfoCursor() const;

  QString buildPvInfoBackgroundText() const;

  QString pvInfoElementLabel(QWidget *widget) const;

  QVector<PvInfoChannelRef> gatherPvInfoChannels(QWidget *widget) const;

  PvInfoContent buildPvInfoContent(QWidget *widget) const;

  QString pvInfoTypeName(chtype fieldType) const;

  QString formatPvInfoNumericValue(double value, int precision = -1) const;

  QString decodePvInfoCharArray(const QByteArray &bytes) const;

  QString compactPvInfoTextPreview(QString text) const;

  QString formatPvInfoArrayElement(const PvInfoArrayData &array, int index) const;

  QString formatPvInfoArrayPreview(const PvInfoArrayData &array) const;

  QString formatPvInfoArrayStats(const QVector<double> &values, int precision = -1) const;

  void applyPvInfoArrayMetadata(PvInfoArrayData &array, const PvInfoChannelDetails &details) const;

  QString formatPvInfoSection(const PvInfoChannelDetails &details) const;

  QString formatPvInfoTimestamp(const epicsTimeStamp &stamp) const;

  QString alarmSeverityString(short severity) const;

  QString pvInfoRelatedFieldName(const QString &channelName, const QString &fieldSuffix) const;

  QString fetchPvInfoRelatedField(const QString &channelName, const QString &fieldSuffix) const;

  bool populateSoftPvInfoDetails(const QString &channelName,
      const SoftPvInfoSnapshot &snapshot,
      PvInfoChannelDetails &details) const;

  bool fetchCaPvInfoArray(chid channelId, PvInfoChannelDetails &details) const;

  bool populatePvInfoDetails(const QString &channelName, chid existingChid, PvInfoChannelDetails &details) const;

  bool prepareExecuteChannelDrag(const QPoint &windowPos);

  void startExecuteChannelDrag();

  void cancelExecuteChannelDrag();

  QLabel *ensureExecuteDragTooltipLabel();

  void updateExecuteDragTooltip(const QPoint &windowPos);

  void hideExecuteDragTooltip();

  void bringElementToFront(QWidget *element);

  void removeElementFromStack(QWidget *element);

  void selectTextElement(TextElement *element);

  void selectTextEntryElement(TextEntryElement *element);

  void selectSetpointControlElement(SetpointControlElement *element);

  void selectTextAreaElement(TextAreaElement *element);

  void selectSliderElement(SliderElement *element);

  void selectWheelSwitchElement(WheelSwitchElement *element);

  void selectChoiceButtonElement(ChoiceButtonElement *element);

  void selectMenuElement(MenuElement *element);

  void selectMessageButtonElement(MessageButtonElement *element);

  void selectShellCommandElement(ShellCommandElement *element);

  void selectRelatedDisplayElement(RelatedDisplayElement *element);

  void selectTextMonitorElement(TextMonitorElement *element);

  void selectPvTableElement(PvTableElement *element);

  void selectWaveTableElement(WaveTableElement *element);

  void selectMeterElement(MeterElement *element);

  void selectScaleMonitorElement(ScaleMonitorElement *element);

  void selectStripChartElement(StripChartElement *element);

  void selectCartesianPlotElement(CartesianPlotElement *element);

  void selectBarMonitorElement(BarMonitorElement *element);

  void selectThermometerElement(ThermometerElement *element);

  void selectByteMonitorElement(ByteMonitorElement *element);

  void selectLedMonitorElement(LedMonitorElement *element);

  void selectRectangleElement(RectangleElement *element);

  void selectExpressionChannelElement(ExpressionChannelElement *element);

  void selectImageElement(ImageElement *element);

  void selectHeatmapElement(HeatmapElement *element);

  void selectWaterfallPlotElement(WaterfallPlotElement *element);

  void selectOvalElement(OvalElement *element);

  void selectArcElement(ArcElement *element);

  void selectLineElement(LineElement *element);

  void selectPolylineElement(PolylineElement *element);

  void selectPolygonElement(PolygonElement *element);

  void selectCompositeElement(CompositeElement *element);

  QWidget *currentSelectedWidget() const;

  bool selectWidgetForEditing(QWidget *widget);

  bool handleMultiSelectionClick(QWidget *widget, Qt::KeyboardModifiers modifiers);

  void beginMiddleButtonDrag(const QPoint &windowPos);

  void updateMiddleButtonDrag(const QPoint &windowPos);

  void finishMiddleButtonDrag(bool applyChanges);

  void beginMiddleButtonResize(const QPoint &windowPos);

  void updateMiddleButtonResize(const QPoint &windowPos);

  void collectCompositeResizeInfo(CompositeElement *composite);

  bool applyCompositeResize(CompositeElement *composite, const QRect &targetRect);

  int widgetHierarchyDepth(QWidget *widget) const;

  void finishMiddleButtonResize(bool applyChanges);

  void cancelMiddleButtonDrag();

  void alignSelectionInternal(AlignmentMode mode);

  void orientSelectionInternal(OrientationAction action);

  QRect orientedRect(const QRect &rect, OrientationAction action, int centerX, int centerY) const;

  QPoint orientedPoint(const QPoint &point, OrientationAction action, int centerX, int centerY) const;

  bool orientGenericWidget(QWidget *widget, OrientationAction action, int centerX, int centerY);

  bool orientLineElement(LineElement *line, OrientationAction action, int centerX, int centerY);

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

  bool orientArcElement(ArcElement *arc, OrientationAction action, int centerX, int centerY);

  bool applyOrientationToWidget(QWidget *widget, OrientationAction action,
      int centerX, int centerY, QSet<QWidget *> *orientedWidgets);

  void centerSelectionInDisplayInternal(bool horizontal, bool vertical);

  void alignSelectionToGridInternal(bool edges);

  void spaceSelectionLinear(Qt::Orientation orientation);

  void spaceSelection2DInternal();

  bool handleEditArrowKey(QKeyEvent *event);

  bool resizeSelectionBy(ResizeDirection direction, int amount);

  bool moveSelectionBy(const QPoint &delta);

  static int snapCoordinateToGrid(int value, int spacing);

  void refreshResourcePaletteGeometry();

  void updateResourcePaletteDisplayControls();

  QPoint clampOffsetToDisplayArea(const QRect &rect, const QPoint &offset) const;

  void cancelSelectionRubberBand();

  void beginSelectionRubberBandPending(const QPoint &areaPos, const QPoint &windowPos);

  void startSelectionRubberBand();

  void updateSelectionRubberBand(const QPoint &areaPos);

  void finishSelectionRubberBand(const QPoint &areaPos);

  void applySelectionRect(const QRect &rect);

  void handleDisplayBackgroundClick();

  void startCreateRubberBand(const QPoint &areaPos, CreateTool tool);

  void updateCreateRubberBand(const QPoint &areaPos);

  void finishCreateRubberBand(const QPoint &areaPos);

  void handlePolygonClick(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  void handlePolygonDoubleClick(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  void updatePolygonPreview(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  void handlePolylineClick(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  void handlePolylineDoubleClick(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  void updatePolylinePreview(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  void finalizePolygonCreation();

  void finalizePolylineCreation();

  void cancelPolygonCreation();

  void cancelPolylineCreation();

  QPoint adjustedPolygonPoint(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  QPoint adjustedPolylinePoint(const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  QPoint adjustedPathPoint(const QVector<QPoint> &points, const QPoint &areaPos, Qt::KeyboardModifiers modifiers);

  bool beginVertexEdit(const QPoint &windowPos, Qt::KeyboardModifiers modifiers);

  bool beginPolygonVertexEdit(const QPoint &areaPos);

  bool beginPolylineVertexEdit(const QPoint &areaPos);

  void startPolygonVertexEdit(PolygonElement *polygon, int index, const QVector<QPoint> &points);

  void startPolylineVertexEdit(PolylineElement *polyline, int index, const QVector<QPoint> &points);

  int hitTestVertex(const QVector<QPoint> &points, const QPoint &areaPos) const;

  int canonicalPolygonVertexIndex(const QVector<QPoint> &points, int index) const;

  int previousVertexIndex(const QVector<QPoint> &points, int index, bool closed) const;

  QPoint adjustedVertexPoint(const QVector<QPoint> &points, int index,
      const QPoint &areaPos, Qt::KeyboardModifiers modifiers,
      bool closed) const;

  void updateVertexEdit(const QPoint &windowPos, Qt::KeyboardModifiers modifiers);

  void restoreInitialVertexPoints();

  void resetVertexEditState();

  void finishActiveVertexEdit(bool applyChanges);

  void finishActiveVertexEditFor(QWidget *widget, bool applyChanges);

  void createTextElement(const QRect &rect);

  void createTextMonitorElement(const QRect &rect);


  void createPvTableElement(const QRect &rect);

  void createWaveTableElement(const QRect &rect);

  void createTextEntryElement(const QRect &rect);

  void createSetpointControlElement(const QRect &rect);

  void createTextAreaElement(const QRect &rect);

  void createSliderElement(const QRect &rect);

  void createWheelSwitchElement(const QRect &rect);

  void createChoiceButtonElement(const QRect &rect);

  void createMenuElement(const QRect &rect);

  void createMessageButtonElement(const QRect &rect);

  void createShellCommandElement(const QRect &rect);

  void createRelatedDisplayElement(const QRect &rect);

  void createMeterElement(const QRect &rect);

  void createBarMonitorElement(const QRect &rect);

  void createThermometerElement(const QRect &rect);

  void createScaleMonitorElement(const QRect &rect);

  void createStripChartElement(const QRect &rect);

  void createCartesianPlotElement(const QRect &rect);

  void createByteMonitorElement(const QRect &rect);

  void createLedMonitorElement(const QRect &rect);

  void createExpressionChannelElement(const QRect &rect);

  void createRectangleElement(const QRect &rect);

  void createImageElement(const QRect &rect);

  void createHeatmapElement(const QRect &rect);

  void createWaterfallPlotElement(const QRect &rect);

  void createOvalElement(const QRect &rect);

  void createArcElement(const QRect &rect);

  void createLineElement(const QPoint &startPoint, const QPoint &endPoint);

  void ensureRubberBand();

  QPoint clampToDisplayArea(const QPoint &areaPos) const;

  QRect adjustRectToDisplayArea(const QRect &rect) const;

  QPoint snapPointToGrid(const QPoint &point) const;

  QPoint snapTopLeftToGrid(const QPoint &topLeft, const QSize &size) const;

  QPoint snapOffsetToGrid(const QRect &rect, const QPoint &offset) const;

  QRect snapRectOriginToGrid(const QRect &rect) const;

  QRect translateRectForPaste(const QRect &rect, const QPoint &offset) const;

  QVector<QPoint> translatePointsForPaste(const QVector<QPoint> &points, const QPoint &offset) const;

  void updateCreateCursor();

  void activateCreateTool(CreateTool tool);

  void deactivateCreateTool();

  void ensureExecuteContextMenuEntriesLoaded();

  bool showExecuteSliderDialogForRightClick(const QPoint &globalPos);

  void showExecuteContextMenu(const QPoint &globalPos);

  void focusMainWindow() const;

  void showDisplayListDialog() const;

  void showFindPvDialog() const;

  PvInfoDialog *ensurePvInfoDialog();

  bool attemptChannelRetry(const QString &channelName, QString &retriedChannel) const;

  bool retryFirstUnconnectedChannel(QString &retriedChannel);

  void retryChannelConnections();

  void showEditContextMenu(const QPoint &globalPos);
};
