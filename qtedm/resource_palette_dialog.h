#pragma once

#include <algorithm>
#include <array>
#include <functional>

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QHash>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QString>

#include "cartesian_plot_properties.h"
#include "control_properties.h"
#include "display_common_properties.h"
#include "graphic_properties.h"
#include "heatmap_properties.h"
#include "monitor_properties.h"
#include "strip_chart_properties.h"
#include "text_properties.h"
#include "time_units.h"
#include "waterfall_plot_properties.h"
#include "wave_table_properties.h"

class CartesianAxisDialog;
class CartesianPlotElement;
class ColorPaletteDialog;
class PvLimitsDialog;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QEvent;
class QGridLayout;
class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QScrollArea;
class QScreen;
class QSpinBox;
class QWidget;

class ResourcePaletteDialog : public QDialog
{
public:
  static constexpr int kPvTableRowCount = 8;
  static constexpr int kTableFontSizeMin = 1;
  static constexpr int kTableFontSizeMax = 200;
  ResourcePaletteDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr);


  void showForDisplay(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<int()> gridSpacingGetter,
      std::function<void(int)> gridSpacingSetter,
      std::function<bool()> gridOnGetter,
      std::function<void(bool)> gridOnSetter,
      std::function<bool()> snapToGridGetter,
      std::function<void(bool)> snapToGridSetter);


  void showForMultipleSelection();


  void refreshGeometryFromSelection();


  void refreshDisplayControls();


  void showForText(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> textGetter,
      std::function<void(const QString &)> textSetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<Qt::Alignment()> alignmentGetter,
      std::function<void(Qt::Alignment)> alignmentSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
  std::function<QString()> visibilityCalcGetter,
  std::function<void(const QString &)> visibilityCalcSetter,
  std::array<std::function<QString()>, 5> channelGetters,
  std::array<std::function<void(const QString &)>, 5> channelSetters);


  void showForTextEntry(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForSetpointControl(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> setpointGetter,
      std::function<void(const QString &)> setpointSetter,
      std::function<QString()> readbackGetter,
      std::function<void(const QString &)> readbackSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<SetpointToleranceMode()> toleranceModeGetter,
      std::function<void(SetpointToleranceMode)> toleranceModeSetter,
      std::function<double()> toleranceGetter,
      std::function<void(double)> toleranceSetter,
      std::function<bool()> showReadbackGetter,
      std::function<void(bool)> showReadbackSetter);


  void showForTextArea(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<bool()> readOnlyGetter,
      std::function<void(bool)> readOnlySetter,
      std::function<bool()> wordWrapGetter,
      std::function<void(bool)> wordWrapSetter,
      std::function<TextAreaWrapMode()> wrapModeGetter,
      std::function<void(TextAreaWrapMode)> wrapModeSetter,
      std::function<int()> wrapColumnWidthGetter,
      std::function<void(int)> wrapColumnWidthSetter,
      std::function<bool()> showVerticalScrollBarGetter,
      std::function<void(bool)> showVerticalScrollBarSetter,
      std::function<bool()> showHorizontalScrollBarGetter,
      std::function<void(bool)> showHorizontalScrollBarSetter,
      std::function<TextAreaCommitMode()> commitModeGetter,
      std::function<void(TextAreaCommitMode)> commitModeSetter,
      std::function<bool()> tabInsertsSpacesGetter,
      std::function<void(bool)> tabInsertsSpacesSetter,
      std::function<int()> tabWidthGetter,
      std::function<void(int)> tabWidthSetter,
      std::function<QString()> fontFamilyGetter,
      std::function<void(const QString &)> fontFamilySetter);


  void showForSlider(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<double()> incrementGetter,
      std::function<void(double)> incrementSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForWheelSwitch(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> formatGetter,
      std::function<void(const QString &)> formatSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForChoiceButton(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<ChoiceButtonStacking()> stackingGetter,
      std::function<void(ChoiceButtonStacking)> stackingSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter);


  void showForMenu(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter);


  void showForMessageButton(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::function<QString()> pressGetter,
      std::function<void(const QString &)> pressSetter,
      std::function<QString()> releaseGetter,
      std::function<void(const QString &)> releaseSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter);


  void showForShellCommand(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::array<std::function<QString()>, kShellCommandEntryCount> entryLabelGetters,
      std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryLabelSetters,
      std::array<std::function<QString()>, kShellCommandEntryCount> entryCommandGetters,
      std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryCommandSetters,
      std::array<std::function<QString()>, kShellCommandEntryCount> entryArgsGetters,
      std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryArgsSetters);


  void showForRelatedDisplay(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::function<RelatedDisplayVisual()> visualGetter,
      std::function<void(RelatedDisplayVisual)> visualSetter,
      std::array<std::function<QString()>, kRelatedDisplayEntryCount> entryLabelGetters,
      std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> entryLabelSetters,
      std::array<std::function<QString()>, kRelatedDisplayEntryCount> entryNameGetters,
      std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> entryNameSetters,
      std::array<std::function<QString()>, kRelatedDisplayEntryCount> entryArgsGetters,
      std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> entryArgsSetters,
      std::array<std::function<RelatedDisplayMode()>, kRelatedDisplayEntryCount> entryModeGetters,
      std::array<std::function<void(RelatedDisplayMode)>, kRelatedDisplayEntryCount> entryModeSetters);


  void showForPvTable(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<bool()> showHeadersGetter,
      std::function<void(bool)> showHeadersSetter,
      std::function<int()> fontSizeGetter,
      std::function<void(int)> fontSizeSetter,
      std::function<QString()> columnsGetter,
      std::function<void(const QString &)> columnsSetter,
      std::array<std::function<QString()>, kPvTableRowCount> rowLabelGetters,
      std::array<std::function<void(const QString &)>, kPvTableRowCount> rowLabelSetters,
      std::array<std::function<QString()>, kPvTableRowCount> rowChannelGetters,
      std::array<std::function<void(const QString &)>, kPvTableRowCount> rowChannelSetters);


  void showForWaveTable(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<bool()> showHeadersGetter,
      std::function<void(bool)> showHeadersSetter,
      std::function<int()> fontSizeGetter,
      std::function<void(int)> fontSizeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<WaveTableLayout()> layoutGetter,
      std::function<void(WaveTableLayout)> layoutSetter,
      std::function<int()> columnsGetter,
      std::function<void(int)> columnsSetter,
      std::function<int()> maxElementsGetter,
      std::function<void(int)> maxElementsSetter,
      std::function<int()> indexBaseGetter,
      std::function<void(int)> indexBaseSetter,
      std::function<WaveTableValueFormat()> valueFormatGetter,
      std::function<void(WaveTableValueFormat)> valueFormatSetter,
      std::function<WaveTableCharMode()> charModeGetter,
      std::function<void(WaveTableCharMode)> charModeSetter);


  void showForTextMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<Qt::Alignment()> alignmentGetter,
      std::function<void(Qt::Alignment)> alignmentSetter,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter = {},
      std::function<void(const PvLimits &)> limitsSetter = {});


  void showForMeter(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForBarMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<BarFill()> fillGetter,
      std::function<void(BarFill)> fillSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForThermometer(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QColor()> textGetter,
      std::function<void(const QColor &)> textSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> visibilityChannelGetters,
      std::array<std::function<void(const QString &)>, 4> visibilityChannelSetters,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<bool()> showValueGetter,
      std::function<void(bool)> showValueSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForScaleMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter);


  void showForStripChart(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> xLabelGetter,
      std::function<void(const QString &)> xLabelSetter,
      std::function<QString()> yLabelGetter,
      std::function<void(const QString &)> yLabelSetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<double()> periodGetter,
      std::function<void(double)> periodSetter,
      std::function<TimeUnits()> unitsGetter,
      std::function<void(TimeUnits)> unitsSetter,
      std::array<std::function<QString()>, kStripChartPenCount> channelGetters,
      std::array<std::function<void(const QString &)>, kStripChartPenCount> channelSetters,
      std::array<std::function<QColor()>, kStripChartPenCount> colorGetters,
      std::array<std::function<void(const QColor &)>, kStripChartPenCount> colorSetters,
      std::array<std::function<PvLimits()>, kStripChartPenCount> limitsGetters,
      std::array<std::function<void(const PvLimits &)>, kStripChartPenCount> limitsSetters);


  void showForCartesianPlot(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> xLabelGetter,
      std::function<void(const QString &)> xLabelSetter,
      std::array<std::function<QString()>, 4> yLabelGetters,
      std::array<std::function<void(const QString &)>, 4> yLabelSetters,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<bool()> drawMajorGetter,
      std::function<void(bool)> drawMajorSetter,
      std::function<bool()> drawMinorGetter,
      std::function<void(bool)> drawMinorSetter,
      std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> axisStyleGetters,
      std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> axisStyleSetters,
      std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> axisRangeGetters,
      std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> axisRangeSetters,
      std::array<std::function<double()>, kCartesianAxisCount> axisMinimumGetters,
      std::array<std::function<void(double)>, kCartesianAxisCount> axisMinimumSetters,
      std::array<std::function<double()>, kCartesianAxisCount> axisMaximumGetters,
      std::array<std::function<void(double)>, kCartesianAxisCount> axisMaximumSetters,
      std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> axisTimeFormatGetters,
      std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> axisTimeFormatSetters,
      std::function<CartesianPlotStyle()> styleGetter,
      std::function<void(CartesianPlotStyle)> styleSetter,
      std::function<bool()> eraseOldestGetter,
      std::function<void(bool)> eraseOldestSetter,
      std::function<int()> countGetter,
      std::function<void(int)> countSetter,
      std::function<CartesianPlotEraseMode()> eraseModeGetter,
      std::function<void(CartesianPlotEraseMode)> eraseModeSetter,
      std::function<QString()> triggerGetter,
      std::function<void(const QString &)> triggerSetter,
      std::function<QString()> eraseGetter,
      std::function<void(const QString &)> eraseSetter,
      std::function<QString()> countPvGetter,
      std::function<void(const QString &)> countPvSetter,
      std::array<std::function<QString()>, kCartesianPlotTraceCount> xChannelGetters,
      std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> xChannelSetters,
      std::array<std::function<QString()>, kCartesianPlotTraceCount> yChannelGetters,
      std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> yChannelSetters,
      std::array<std::function<QColor()>, kCartesianPlotTraceCount> colorGetters,
      std::array<std::function<void(const QColor &)>, kCartesianPlotTraceCount> colorSetters,
      std::array<std::function<CartesianPlotYAxis()>, kCartesianPlotTraceCount> axisGetters,
      std::array<std::function<void(CartesianPlotYAxis)>, kCartesianPlotTraceCount> axisSetters,
      std::array<std::function<bool()>, kCartesianPlotTraceCount> sideGetters,
      std::array<std::function<void(bool)>, kCartesianPlotTraceCount> sideSetters);


  void showForByteMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<int()> startBitGetter,
      std::function<void(int)> startBitSetter,
      std::function<int()> endBitGetter,
      std::function<void(int)> endBitSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter);


  void showForLedMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<LedShape()> shapeGetter,
      std::function<void(LedShape)> shapeSetter,
      std::function<bool()> bezelGetter,
      std::function<void(bool)> bezelSetter,
      std::function<QColor()> onColorGetter,
      std::function<void(const QColor &)> onColorSetter,
      std::function<QColor()> offColorGetter,
      std::function<void(const QColor &)> offColorSetter,
      std::function<QColor()> undefinedColorGetter,
      std::function<void(const QColor &)> undefinedColorSetter,
      std::array<std::function<QColor()>, kLedStateCount> stateColorGetters,
      std::array<std::function<void(const QColor &)>, kLedStateCount> stateColorSetters,
      std::function<int()> stateCountGetter,
      std::function<void(int)> stateCountSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> visibilityChannelGetters,
      std::array<std::function<void(const QString &)>, 4> visibilityChannelSetters);


  void showForExpressionChannel(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> variableGetter,
      std::function<void(const QString &)> variableSetter,
      std::function<QString()> calcGetter,
      std::function<void(const QString &)> calcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters,
      std::function<double()> initialValueGetter,
      std::function<void(double)> initialValueSetter,
      std::function<ExpressionChannelEventSignalMode()> eventSignalGetter,
      std::function<void(ExpressionChannelEventSignalMode)> eventSignalSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      const QString &elementLabel = QStringLiteral("Expression Channel"));



  void showForComposite(std::function<QRect()> geometryGetter,
    std::function<void(const QRect &)> geometrySetter,
    std::function<QColor()> foregroundGetter,
    std::function<void(const QColor &)> foregroundSetter,
    std::function<QColor()> backgroundGetter,
    std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> fileGetter,
      std::function<void(const QString &)> fileSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters,
      const QString &elementLabel = QStringLiteral("Composite"));


  void showForRectangle(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> colorGetter,
      std::function<void(const QColor &)> colorSetter,
      std::function<RectangleFill()> fillGetter,
      std::function<void(RectangleFill)> fillSetter,
      std::function<RectangleLineStyle()> lineStyleGetter,
      std::function<void(RectangleLineStyle)> lineStyleSetter,
      std::function<int()> lineWidthGetter,
      std::function<void(int)> lineWidthSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters,
      const QString &elementLabel = QStringLiteral("Rectangle"),
      bool treatAsPolygon = false,
      std::function<int()> arcBeginGetter = {},
      std::function<void(int)> arcBeginSetter = {},
      std::function<int()> arcPathGetter = {},
      std::function<void(int)> arcPathSetter = {});


  void showForImage(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<ImageType()> typeGetter,
      std::function<void(ImageType)> typeSetter,
      std::function<QString()> nameGetter,
      std::function<void(const QString &)> nameSetter,
      std::function<QString()> calcGetter,
      std::function<void(const QString &)> calcSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters);


  void showForHeatmap(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> dataChannelGetter,
      std::function<void(const QString &)> dataChannelSetter,
      std::function<HeatmapDimensionSource()> xSourceGetter,
      std::function<void(HeatmapDimensionSource)> xSourceSetter,
      std::function<HeatmapDimensionSource()> ySourceGetter,
      std::function<void(HeatmapDimensionSource)> ySourceSetter,
      std::function<int()> xDimGetter,
      std::function<void(int)> xDimSetter,
      std::function<int()> yDimGetter,
      std::function<void(int)> yDimSetter,
      std::function<QString()> xDimChannelGetter,
      std::function<void(const QString &)> xDimChannelSetter,
      std::function<QString()> yDimChannelGetter,
      std::function<void(const QString &)> yDimChannelSetter,
      std::function<HeatmapOrder()> orderGetter,
      std::function<void(HeatmapOrder)> orderSetter,
      std::function<HeatmapColorMap()> colorMapGetter,
      std::function<void(HeatmapColorMap)> colorMapSetter,
      std::function<bool()> invertGreyscaleGetter,
      std::function<void(bool)> invertGreyscaleSetter,
      std::function<bool()> showTopProfileGetter,
      std::function<void(bool)> showTopProfileSetter,
      std::function<bool()> showRightProfileGetter,
      std::function<void(bool)> showRightProfileSetter,
      std::function<HeatmapProfileMode()> profileModeGetter,
      std::function<void(HeatmapProfileMode)> profileModeSetter,
      std::function<bool()> preserveAspectRatioGetter,
      std::function<void(bool)> preserveAspectRatioSetter,
      std::function<bool()> flipHorizontalGetter,
      std::function<void(bool)> flipHorizontalSetter,
      std::function<bool()> flipVerticalGetter,
      std::function<void(bool)> flipVerticalSetter,
      std::function<HeatmapRotation()> rotationGetter,
      std::function<void(HeatmapRotation)> rotationSetter);


  void showForWaterfall(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> xLabelGetter,
      std::function<void(const QString &)> xLabelSetter,
      std::function<QString()> yLabelGetter,
      std::function<void(const QString &)> yLabelSetter,
      std::function<QString()> dataChannelGetter,
      std::function<void(const QString &)> dataChannelSetter,
      std::function<QString()> countChannelGetter,
      std::function<void(const QString &)> countChannelSetter,
      std::function<QString()> triggerChannelGetter,
      std::function<void(const QString &)> triggerChannelSetter,
      std::function<QString()> eraseChannelGetter,
      std::function<void(const QString &)> eraseChannelSetter,
      std::function<WaterfallEraseMode()> eraseModeGetter,
      std::function<void(WaterfallEraseMode)> eraseModeSetter,
      std::function<int()> historyCountGetter,
      std::function<void(int)> historyCountSetter,
      std::function<WaterfallScrollDirection()> scrollDirectionGetter,
      std::function<void(WaterfallScrollDirection)> scrollDirectionSetter,
      std::function<HeatmapColorMap()> colorMapGetter,
      std::function<void(HeatmapColorMap)> colorMapSetter,
      std::function<bool()> invertGreyscaleGetter,
      std::function<void(bool)> invertGreyscaleSetter,
      std::function<WaterfallIntensityScale()> intensityScaleGetter,
      std::function<void(WaterfallIntensityScale)> intensityScaleSetter,
      std::function<double()> intensityMinGetter,
      std::function<void(double)> intensityMinSetter,
      std::function<double()> intensityMaxGetter,
      std::function<void(double)> intensityMaxSetter,
      std::function<bool()> showLegendGetter,
      std::function<void(bool)> showLegendSetter,
      std::function<bool()> showGridGetter,
      std::function<void(bool)> showGridSetter,
      std::function<double()> samplePeriodGetter,
      std::function<void(double)> samplePeriodSetter,
      std::function<TimeUnits()> unitsGetter,
      std::function<void(TimeUnits)> unitsSetter);


  void showForLine(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> colorGetter,
      std::function<void(const QColor &)> colorSetter,
      std::function<RectangleLineStyle()> lineStyleGetter,
      std::function<void(RectangleLineStyle)> lineStyleSetter,
      std::function<int()> lineWidthGetter,
      std::function<void(int)> lineWidthSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
    std::array<std::function<QString()>, 4> channelGetters,
    std::array<std::function<void(const QString &)>, 4> channelSetters,
    const QString &elementLabel = QStringLiteral("Line"));


  void clearSelectionState();


  void openCartesianAxisDialogForElement(CartesianPlotElement *element);


protected:
  void closeEvent(QCloseEvent *event) override;


private:
  static constexpr int kPaletteSpacing = 12;
  static constexpr int kTextVisibleChannelCount = 4;
  static constexpr int kTextChannelAIndex = 0;
  static constexpr int kTextChannelBIndex = 1;
  static constexpr int kTextChannelCIndex = 2;
  static constexpr int kTextChannelDIndex = 3;
  enum class SelectionKind {
    kNone,
    kDisplay,
    kExpressionChannel,
    kRectangle,
    kImage,
    kHeatmap,
    kWaterfallPlot,
    kPolygon,
    kComposite,
    kLine,
    kText,
    kTextEntry,
    kSetpointControl,
    kTextArea,
    kSlider,
    kWheelSwitch,
    kChoiceButton,
    kMenu,
    kMessageButton,
    kShellCommand,
    kRelatedDisplay,
    kPvTable,
    kWaveTable,
    kTextMonitor,
    kMeter,
    kBarMonitor,
    kThermometer,
    kScaleMonitor,
    kStripChart,
    kCartesianPlot,
    kByteMonitor,
    kLedMonitor
  };

  enum class LedColorModeChoice {
    kStatic,
    kAlarm,
    kBinary,
    kDiscrete,
  };

  int tableFontSizeValue(int value) const;


  int defaultTableFontSize() const;


  QLineEdit *createLineEdit();


  QPushButton *createColorButton(const QColor &color);


  QPushButton *createActionButton(const QString &text);


  QComboBox *createBooleanComboBox();


  QSpinBox *createFontSizeSpinBox();


  void addRow(QGridLayout *layout, int row, const QString &label,
      QWidget *field);


  bool eventFilter(QObject *object, QEvent *event) override;


  void setColorButtonColor(QPushButton *button, const QColor &color);


  void resetLineEdit(QLineEdit *edit);


  void resetColorButton(QPushButton *button);


  void updateSectionVisibility(SelectionKind kind);


  void commitTextString();


  void revertTextString();


  void commitTextVisibilityCalc();


  void commitTextChannel(int index);


  void updateImageChannelDependentControls();


  void updateHeatmapDimensionControls();


  void updateWaterfallDependentControls();


  void updateLineChannelDependentControls();


  void updateTextChannelDependentControls();


  void updateRectangleChannelDependentControls();


  void updateThermometerChannelDependentControls();


  void refreshLedMonitorColorButtons();


  void applyBinaryLedPreset();


  void updateLedMonitorStateColorControls();


  void updateLedMonitorChannelDependentControls();


  void updateCompositeChannelDependentControls();


  void setFieldEnabled(QWidget *field, bool enabled);


  void updateTextAreaDependentControls();


  void commitTextEntryChannel();


  void commitSetpointControlLabel();


  void commitSetpointControlSetpoint();


  void commitSetpointControlReadback();


  void commitSetpointControlTolerance();


  void commitTextAreaChannel();


  void commitTextAreaWrapColumnWidth();


  void commitTextAreaTabWidth();


  void commitTextAreaFontFamily();


  void commitSliderIncrement();


  void commitSliderChannel();


  void commitWheelSwitchPrecision();


  void commitWheelSwitchFormat();


  void commitWheelSwitchChannel();


  void commitChoiceButtonChannel();


  void commitMenuChannel();


  void commitMessageButtonLabel();


  void commitMessageButtonPressMessage();


  void commitMessageButtonReleaseMessage();


  void commitMessageButtonChannel();


  void commitShellCommandLabel();


  void commitShellCommandEntryLabel(int index);


  void commitShellCommandEntryCommand(int index);


  void commitShellCommandEntryArgs(int index);


  void commitRelatedDisplayLabel();


  void commitRelatedDisplayEntryLabel(int index);


  void commitRelatedDisplayEntryName(int index);


  void commitRelatedDisplayEntryArgs(int index);


  void commitPvTableColumns();


  void commitPvTableFontSize(int value);


  void commitPvTableRowLabel(int index);


  void commitPvTableRowChannel(int index);


  void commitWaveTableFontSize(int value);


  void commitWaveTableChannel();


  void commitWaveTableColumns();


  void commitWaveTableMaxElements();


  void commitTextMonitorChannel();


  void commitMeterChannel();


  void commitBarChannel();


  void commitThermometerChannel();


  void commitThermometerVisibilityCalc();


  void commitThermometerVisibilityChannel(int index);


  void commitScaleChannel();


  void commitStripChartTitle();


  void commitStripChartXLabel();


  void commitStripChartYLabel();


  void commitStripChartPeriod();


  void commitStripChartChannel(int index);


  void commitCartesianTitle();


  void commitCartesianXLabel();


  void commitCartesianYLabel(int index);


  void commitCartesianCount();


  void commitCartesianTrigger();


  void commitCartesianErase();


  void commitCartesianCountPv();


  void commitCartesianTraceXChannel(int index);


  void commitCartesianTraceYChannel(int index);


  void commitByteChannel();


  void commitLedChannel();


  void commitLedVisibilityCalc();


  void commitLedVisibilityChannel(int index);


  void commitLedStateCount(int value);


  void commitExpressionChannelVariable();


  void commitExpressionChannelCalc();


  void commitExpressionChannelChannel(int index);


  void handleStripChartUnitsChanged(int index);


  void handleCartesianStyleChanged(int index);


  void handleCartesianEraseOldestChanged(int index);


  void handleCartesianEraseModeChanged(int index);


  void handleCartesianTraceAxisChanged(int index, int comboIndex);


  void handleCartesianTraceSideChanged(int index, int comboIndex);


  void openStripChartLimitsDialog(int index);


  void updateStripChartPenLimitsFromDialog(int index);


  void commitByteStartBit(int value);


  void commitByteEndBit(int value);


  void updateSliderIncrementEdit();


  void updateWheelSwitchPrecisionEdit();


  void updateTextMonitorLimitsFromDialog();


  void updateTextEntryLimitsFromDialog();


  void updateSetpointControlLimitsFromDialog();


  void updateTextAreaLimitsFromDialog();


  void updateSliderLimitsFromDialog();


  void updateWheelSwitchLimitsFromDialog();


  void updateMeterLimitsFromDialog();


  void updateBarLimitsFromDialog();


  void updateThermometerLimitsFromDialog();


  void updateScaleLimitsFromDialog();


  void updateCartesianAxisButtonState();


  void commitRectangleLineWidth();


  void commitRectangleVisibilityCalc();


  void commitRectangleChannel(int index);


  void commitCompositeFile();


  void commitCompositeVisibilityCalc();


  void commitCompositeChannel(int index);


  void commitImageName();


  void commitImageCalc();


  void commitImageVisibilityCalc();


  void commitHeatmapDataChannel();


  void commitHeatmapTitle();


  void commitHeatmapXDimension();


  void commitHeatmapYDimension();


  void commitHeatmapXDimensionChannel();


  void commitHeatmapYDimensionChannel();


  void commitWaterfallTitle();


  void commitWaterfallXLabel();


  void commitWaterfallYLabel();


  void commitWaterfallDataChannel();


  void commitWaterfallCountChannel();


  void commitWaterfallTriggerChannel();


  void commitWaterfallEraseChannel();


  void commitWaterfallHistoryCount();


  void commitWaterfallIntensityMinimum();


  void commitWaterfallIntensityMaximum();


  void commitWaterfallSamplePeriod();


  void commitImageChannel(int index);


  void commitLineLineWidth();


  void commitLineVisibilityCalc();


  void commitLineChannel(int index);


  Qt::Alignment alignmentFromIndex(int index) const;


  int alignmentToIndex(Qt::Alignment alignment) const;


  TextMonitorFormat textMonitorFormatFromIndex(int index) const;


  int textMonitorFormatToIndex(TextMonitorFormat format) const;


  TextAreaWrapMode textAreaWrapModeFromIndex(int index) const;


  int textAreaWrapModeToIndex(TextAreaWrapMode mode) const;


  TextAreaCommitMode textAreaCommitModeFromIndex(int index) const;


  int textAreaCommitModeToIndex(TextAreaCommitMode mode) const;


  TextColorMode colorModeFromIndex(int index) const;


  int colorModeToIndex(TextColorMode mode) const;


  WaveTableLayout waveTableLayoutFromIndex(int index) const;


  int waveTableLayoutToIndex(WaveTableLayout layout) const;


  WaveTableValueFormat waveTableValueFormatFromIndex(int index) const;


  int waveTableValueFormatToIndex(WaveTableValueFormat format) const;


  WaveTableCharMode waveTableCharModeFromIndex(int index) const;


  int waveTableCharModeToIndex(WaveTableCharMode mode) const;


  LedColorModeChoice ledColorModeChoiceFromIndex(int index) const;


  int ledColorModeChoiceToIndex(LedColorModeChoice choice) const;


  LedColorModeChoice ledColorModeChoiceForState(TextColorMode mode,
      int stateCount) const;


  TextColorMode ledTextColorModeForChoice(LedColorModeChoice choice) const;


  HeatmapDimensionSource heatmapDimensionSourceFromIndex(int index) const;


  int heatmapDimensionSourceToIndex(HeatmapDimensionSource source) const;


  HeatmapOrder heatmapOrderFromIndex(int index) const;


  int heatmapOrderToIndex(HeatmapOrder order) const;


  HeatmapProfileMode heatmapProfileModeFromIndex(int index) const;


  int heatmapProfileModeToIndex(HeatmapProfileMode mode) const;


  bool heatmapBoolFromIndex(int index) const;


  int heatmapBoolToIndex(bool value) const;


  MeterLabel meterLabelFromIndex(int index) const;


  int meterLabelToIndex(MeterLabel label) const;


  BarDirection barDirectionFromIndex(int index) const;


  int barDirectionToIndex(BarDirection direction) const;


  BarDirection scaleDirectionFromIndex(int index) const;


  int scaleDirectionToIndex(BarDirection direction) const;


  BarFill barFillFromIndex(int index) const;


  int barFillToIndex(BarFill fill) const;


  LedShape ledShapeFromIndex(int index) const;


  int ledShapeToIndex(LedShape shape) const;


  TimeUnits timeUnitsFromIndex(int index) const;


  int timeUnitsToIndex(TimeUnits units) const;


  int cartesianPlotStyleToIndex(CartesianPlotStyle style) const;


  CartesianPlotStyle indexToCartesianPlotStyle(int index) const;


  int cartesianEraseModeToIndex(CartesianPlotEraseMode mode) const;


  CartesianPlotEraseMode indexToCartesianPlotEraseMode(int index) const;


  int cartesianAxisToIndex(CartesianPlotYAxis axis) const;


  CartesianPlotYAxis indexToCartesianAxis(int index) const;


  static int degreesToAngle64(int degrees);


  static int angle64ToDegrees(int angle64);


  TextVisibilityMode visibilityModeFromIndex(int index) const;


  int visibilityModeToIndex(TextVisibilityMode mode) const;


  RectangleFill fillFromIndex(int index) const;


  int fillToIndex(RectangleFill fill) const;


  RectangleLineStyle lineStyleFromIndex(int index) const;


  int lineStyleToIndex(RectangleLineStyle style) const;


  ImageType imageTypeFromIndex(int index) const;


  int imageTypeToIndex(ImageType type) const;


  ExpressionChannelEventSignalMode expressionChannelEventSignalFromIndex(
      int index) const;


  int expressionChannelEventSignalToIndex(
      ExpressionChannelEventSignalMode mode) const;


  ChoiceButtonStacking choiceButtonStackingFromIndex(int index) const;


  int choiceButtonStackingToIndex(ChoiceButtonStacking stacking) const;


  RelatedDisplayVisual relatedDisplayVisualFromIndex(int index) const;


  int relatedDisplayVisualToIndex(RelatedDisplayVisual visual) const;


  RelatedDisplayMode relatedDisplayModeFromIndex(int index) const;


  int relatedDisplayModeToIndex(RelatedDisplayMode mode) const;


  void showPaletteWithoutActivating();


  void positionRelativeTo(QWidget *reference);


  QScreen *screenForWidget(const QWidget *widget) const;


  void moveToTopRight(const QRect &area, const QSize &dialogSize);


  void resizeToFitContents(const QRect &available);


  void scheduleDeferredResize(QWidget *reference);


  QFont labelFont_;
  QFont valueFont_;
  QWidget *geometrySection_ = nullptr;
  QWidget *displaySection_ = nullptr;
  QWidget *rectangleSection_ = nullptr;
  QWidget *compositeSection_ = nullptr;
  QWidget *imageSection_ = nullptr;
  QWidget *heatmapSection_ = nullptr;
  QWidget *waterfallSection_ = nullptr;
  QWidget *lineSection_ = nullptr;
  QWidget *textSection_ = nullptr;
  QLineEdit *xEdit_ = nullptr;
  QLineEdit *yEdit_ = nullptr;
  QLineEdit *widthEdit_ = nullptr;
  QLineEdit *heightEdit_ = nullptr;
  QLineEdit *colormapEdit_ = nullptr;
  QLineEdit *gridSpacingEdit_ = nullptr;
  QLineEdit *textStringEdit_ = nullptr;
  QPushButton *textForegroundButton_ = nullptr;
  QComboBox *textAlignmentCombo_ = nullptr;
  QComboBox *textColorModeCombo_ = nullptr;
  QComboBox *textVisibilityCombo_ = nullptr;
  QLineEdit *textVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 5> textChannelEdits_{};
  QLineEdit *heatmapTitleEdit_ = nullptr;
  QLineEdit *heatmapDataChannelEdit_ = nullptr;
  QComboBox *heatmapXSourceCombo_ = nullptr;
  QComboBox *heatmapYSourceCombo_ = nullptr;
  QLineEdit *heatmapXDimEdit_ = nullptr;
  QLineEdit *heatmapYDimEdit_ = nullptr;
  QLineEdit *heatmapXDimChannelEdit_ = nullptr;
  QLineEdit *heatmapYDimChannelEdit_ = nullptr;
  QComboBox *heatmapOrderCombo_ = nullptr;
  QComboBox *heatmapColorMapCombo_ = nullptr;
  QComboBox *heatmapInvertGreyscaleCombo_ = nullptr;
  QComboBox *heatmapPreserveAspectRatioCombo_ = nullptr;
  QComboBox *heatmapFlipHorizontalCombo_ = nullptr;
  QComboBox *heatmapFlipVerticalCombo_ = nullptr;
  QComboBox *heatmapRotationCombo_ = nullptr;
  QComboBox *heatmapProfileModeCombo_ = nullptr;
  QComboBox *heatmapShowTopProfileCombo_ = nullptr;
  QComboBox *heatmapShowRightProfileCombo_ = nullptr;
  QPushButton *waterfallForegroundButton_ = nullptr;
  QPushButton *waterfallBackgroundButton_ = nullptr;
  QLineEdit *waterfallTitleEdit_ = nullptr;
  QLineEdit *waterfallXLabelEdit_ = nullptr;
  QLineEdit *waterfallYLabelEdit_ = nullptr;
  QLineEdit *waterfallDataChannelEdit_ = nullptr;
  QLineEdit *waterfallCountChannelEdit_ = nullptr;
  QLineEdit *waterfallTriggerChannelEdit_ = nullptr;
  QLineEdit *waterfallEraseChannelEdit_ = nullptr;
  QComboBox *waterfallEraseModeCombo_ = nullptr;
  QLineEdit *waterfallHistoryEdit_ = nullptr;
  QComboBox *waterfallScrollDirectionCombo_ = nullptr;
  QComboBox *waterfallColorMapCombo_ = nullptr;
  QComboBox *waterfallInvertGreyscaleCombo_ = nullptr;
  QComboBox *waterfallIntensityScaleCombo_ = nullptr;
  QLineEdit *waterfallIntensityMinEdit_ = nullptr;
  QLineEdit *waterfallIntensityMaxEdit_ = nullptr;
  QComboBox *waterfallShowLegendCombo_ = nullptr;
  QComboBox *waterfallShowGridCombo_ = nullptr;
  QLineEdit *waterfallSamplePeriodEdit_ = nullptr;
  QComboBox *waterfallUnitsCombo_ = nullptr;
  QWidget *pvTableSection_ = nullptr;
  QPushButton *pvTableForegroundButton_ = nullptr;
  QPushButton *pvTableBackgroundButton_ = nullptr;
  QComboBox *pvTableColorModeCombo_ = nullptr;
  QComboBox *pvTableShowHeadersCombo_ = nullptr;
  QSpinBox *pvTableFontSizeSpin_ = nullptr;
  QLineEdit *pvTableColumnsEdit_ = nullptr;
  QWidget *pvTableRowsWidget_ = nullptr;
  std::array<QLineEdit *, kPvTableRowCount> pvTableRowLabelEdits_{};
  std::array<QLineEdit *, kPvTableRowCount> pvTableRowChannelEdits_{};
  QWidget *waveTableSection_ = nullptr;
  QPushButton *waveTableForegroundButton_ = nullptr;
  QPushButton *waveTableBackgroundButton_ = nullptr;
  QComboBox *waveTableColorModeCombo_ = nullptr;
  QComboBox *waveTableShowHeadersCombo_ = nullptr;
  QSpinBox *waveTableFontSizeSpin_ = nullptr;
  QLineEdit *waveTableChannelEdit_ = nullptr;
  QComboBox *waveTableLayoutCombo_ = nullptr;
  QLineEdit *waveTableColumnsEdit_ = nullptr;
  QLineEdit *waveTableMaxElementsEdit_ = nullptr;
  QComboBox *waveTableIndexBaseCombo_ = nullptr;
  QComboBox *waveTableValueFormatCombo_ = nullptr;
  QComboBox *waveTableCharModeCombo_ = nullptr;
  QWidget *textMonitorSection_ = nullptr;
  QPushButton *textMonitorForegroundButton_ = nullptr;
  QPushButton *textMonitorBackgroundButton_ = nullptr;
  QComboBox *textMonitorAlignmentCombo_ = nullptr;
  QComboBox *textMonitorFormatCombo_ = nullptr;
  QComboBox *textMonitorColorModeCombo_ = nullptr;
  QLineEdit *textMonitorChannelEdit_ = nullptr;
  QPushButton *textMonitorPvLimitsButton_ = nullptr;
  QWidget *textEntrySection_ = nullptr;
  QPushButton *textEntryForegroundButton_ = nullptr;
  QPushButton *textEntryBackgroundButton_ = nullptr;
  QComboBox *textEntryFormatCombo_ = nullptr;
  QComboBox *textEntryColorModeCombo_ = nullptr;
  QLineEdit *textEntryChannelEdit_ = nullptr;
  QPushButton *textEntryPvLimitsButton_ = nullptr;
  QWidget *setpointControlSection_ = nullptr;
  QPushButton *setpointControlForegroundButton_ = nullptr;
  QPushButton *setpointControlBackgroundButton_ = nullptr;
  QComboBox *setpointControlFormatCombo_ = nullptr;
  QComboBox *setpointControlColorModeCombo_ = nullptr;
  QLineEdit *setpointControlLabelEdit_ = nullptr;
  QLineEdit *setpointControlSetpointEdit_ = nullptr;
  QLineEdit *setpointControlReadbackEdit_ = nullptr;
  QComboBox *setpointControlToleranceModeCombo_ = nullptr;
  QLineEdit *setpointControlToleranceEdit_ = nullptr;
  QComboBox *setpointControlShowReadbackCombo_ = nullptr;
  QPushButton *setpointControlPvLimitsButton_ = nullptr;
  QWidget *textAreaSection_ = nullptr;
  QPushButton *textAreaForegroundButton_ = nullptr;
  QPushButton *textAreaBackgroundButton_ = nullptr;
  QComboBox *textAreaFormatCombo_ = nullptr;
  QComboBox *textAreaColorModeCombo_ = nullptr;
  QLineEdit *textAreaChannelEdit_ = nullptr;
  QPushButton *textAreaPvLimitsButton_ = nullptr;
  QComboBox *textAreaReadOnlyCombo_ = nullptr;
  QComboBox *textAreaWordWrapCombo_ = nullptr;
  QComboBox *textAreaLineWrapModeCombo_ = nullptr;
  QLineEdit *textAreaWrapColumnWidthEdit_ = nullptr;
  QComboBox *textAreaVerticalScrollBarCombo_ = nullptr;
  QComboBox *textAreaHorizontalScrollBarCombo_ = nullptr;
  QComboBox *textAreaCommitModeCombo_ = nullptr;
  QComboBox *textAreaTabInsertsSpacesCombo_ = nullptr;
  QLineEdit *textAreaTabWidthEdit_ = nullptr;
  QLineEdit *textAreaFontFamilyEdit_ = nullptr;
  QWidget *sliderSection_ = nullptr;
  QPushButton *sliderForegroundButton_ = nullptr;
  QPushButton *sliderBackgroundButton_ = nullptr;
  QComboBox *sliderLabelCombo_ = nullptr;
  QComboBox *sliderColorModeCombo_ = nullptr;
  QComboBox *sliderDirectionCombo_ = nullptr;
  QLineEdit *sliderIncrementEdit_ = nullptr;
  QLineEdit *sliderChannelEdit_ = nullptr;
  QPushButton *sliderPvLimitsButton_ = nullptr;
  QWidget *wheelSwitchSection_ = nullptr;
  QPushButton *wheelSwitchForegroundButton_ = nullptr;
  QPushButton *wheelSwitchBackgroundButton_ = nullptr;
  QComboBox *wheelSwitchColorModeCombo_ = nullptr;
  QLineEdit *wheelSwitchPrecisionEdit_ = nullptr;
  QLineEdit *wheelSwitchFormatEdit_ = nullptr;
  QLineEdit *wheelSwitchChannelEdit_ = nullptr;
  QPushButton *wheelSwitchPvLimitsButton_ = nullptr;
  QWidget *choiceButtonSection_ = nullptr;
  QPushButton *choiceButtonForegroundButton_ = nullptr;
  QPushButton *choiceButtonBackgroundButton_ = nullptr;
  QComboBox *choiceButtonColorModeCombo_ = nullptr;
  QComboBox *choiceButtonStackingCombo_ = nullptr;
  QLineEdit *choiceButtonChannelEdit_ = nullptr;
  QWidget *menuSection_ = nullptr;
  QPushButton *menuForegroundButton_ = nullptr;
  QPushButton *menuBackgroundButton_ = nullptr;
  QComboBox *menuColorModeCombo_ = nullptr;
  QLineEdit *menuChannelEdit_ = nullptr;
  QWidget *messageButtonSection_ = nullptr;
  QPushButton *messageButtonForegroundButton_ = nullptr;
  QPushButton *messageButtonBackgroundButton_ = nullptr;
  QComboBox *messageButtonColorModeCombo_ = nullptr;
  QLineEdit *messageButtonLabelEdit_ = nullptr;
  QLineEdit *messageButtonPressEdit_ = nullptr;
  QLineEdit *messageButtonReleaseEdit_ = nullptr;
  QLineEdit *messageButtonChannelEdit_ = nullptr;
  QWidget *shellCommandSection_ = nullptr;
  QPushButton *shellCommandForegroundButton_ = nullptr;
  QPushButton *shellCommandBackgroundButton_ = nullptr;
  QLineEdit *shellCommandLabelEdit_ = nullptr;
  QWidget *shellCommandEntriesWidget_ = nullptr;
  std::array<QLineEdit *, kShellCommandEntryCount> shellCommandEntryLabelEdits_{};
  std::array<QLineEdit *, kShellCommandEntryCount> shellCommandEntryCommandEdits_{};
  std::array<QLineEdit *, kShellCommandEntryCount> shellCommandEntryArgsEdits_{};
  QWidget *relatedDisplaySection_ = nullptr;
  QPushButton *relatedDisplayForegroundButton_ = nullptr;
  QPushButton *relatedDisplayBackgroundButton_ = nullptr;
  QLineEdit *relatedDisplayLabelEdit_ = nullptr;
  QComboBox *relatedDisplayVisualCombo_ = nullptr;
  QWidget *relatedDisplayEntriesWidget_ = nullptr;
  std::array<QLineEdit *, kRelatedDisplayEntryCount> relatedDisplayEntryLabelEdits_{};
  std::array<QLineEdit *, kRelatedDisplayEntryCount> relatedDisplayEntryNameEdits_{};
  std::array<QLineEdit *, kRelatedDisplayEntryCount> relatedDisplayEntryArgsEdits_{};
  std::array<QComboBox *, kRelatedDisplayEntryCount> relatedDisplayEntryModeCombos_{};
  QWidget *meterSection_ = nullptr;
  QPushButton *meterForegroundButton_ = nullptr;
  QPushButton *meterBackgroundButton_ = nullptr;
  QComboBox *meterLabelCombo_ = nullptr;
  QComboBox *meterColorModeCombo_ = nullptr;
  QLineEdit *meterChannelEdit_ = nullptr;
  QPushButton *meterPvLimitsButton_ = nullptr;
  QWidget *barSection_ = nullptr;
  QPushButton *barForegroundButton_ = nullptr;
  QPushButton *barBackgroundButton_ = nullptr;
  QComboBox *barLabelCombo_ = nullptr;
  QComboBox *barColorModeCombo_ = nullptr;
  QComboBox *barDirectionCombo_ = nullptr;
  QComboBox *barFillCombo_ = nullptr;
  QLineEdit *barChannelEdit_ = nullptr;
  QPushButton *barPvLimitsButton_ = nullptr;
  QWidget *thermometerSection_ = nullptr;
  QPushButton *thermometerForegroundButton_ = nullptr;
  QPushButton *thermometerBackgroundButton_ = nullptr;
  QPushButton *thermometerTextButton_ = nullptr;
  QComboBox *thermometerLabelCombo_ = nullptr;
  QComboBox *thermometerColorModeCombo_ = nullptr;
  QComboBox *thermometerFormatCombo_ = nullptr;
  QComboBox *thermometerShowValueCombo_ = nullptr;
  QComboBox *thermometerVisibilityCombo_ = nullptr;
  QLineEdit *thermometerVisibilityCalcEdit_ = nullptr;
  QLineEdit *thermometerChannelEdit_ = nullptr;
  std::array<QLineEdit *, 4> thermometerVisibilityChannelEdits_{};
  QPushButton *thermometerPvLimitsButton_ = nullptr;
  QWidget *scaleSection_ = nullptr;
  QPushButton *scaleForegroundButton_ = nullptr;
  QPushButton *scaleBackgroundButton_ = nullptr;
  QComboBox *scaleLabelCombo_ = nullptr;
  QComboBox *scaleColorModeCombo_ = nullptr;
  QComboBox *scaleDirectionCombo_ = nullptr;
  QLineEdit *scaleChannelEdit_ = nullptr;
  QPushButton *scalePvLimitsButton_ = nullptr;
  QWidget *stripChartSection_ = nullptr;
  QLineEdit *stripTitleEdit_ = nullptr;
  QLineEdit *stripXLabelEdit_ = nullptr;
  QLineEdit *stripYLabelEdit_ = nullptr;
  QPushButton *stripForegroundButton_ = nullptr;
  QPushButton *stripBackgroundButton_ = nullptr;
  QLineEdit *stripPeriodEdit_ = nullptr;
  QComboBox *stripUnitsCombo_ = nullptr;
  std::array<QPushButton *, kStripChartPenCount> stripPenColorButtons_{};
  std::array<QLineEdit *, kStripChartPenCount> stripPenChannelEdits_{};
  std::array<QPushButton *, kStripChartPenCount> stripPenLimitsButtons_{};
  QWidget *cartesianSection_ = nullptr;
  QLineEdit *cartesianTitleEdit_ = nullptr;
  QLineEdit *cartesianXLabelEdit_ = nullptr;
  std::array<QLineEdit *, 4> cartesianYLabelEdits_{};
  QPushButton *cartesianForegroundButton_ = nullptr;
  QPushButton *cartesianBackgroundButton_ = nullptr;
  QComboBox *cartesianDrawMajorCombo_ = nullptr;
  QComboBox *cartesianDrawMinorCombo_ = nullptr;
  QComboBox *cartesianStyleCombo_ = nullptr;
  QComboBox *cartesianEraseOldestCombo_ = nullptr;
  QLineEdit *cartesianCountEdit_ = nullptr;
  QComboBox *cartesianEraseModeCombo_ = nullptr;
  QLineEdit *cartesianTriggerEdit_ = nullptr;
  QLineEdit *cartesianEraseEdit_ = nullptr;
  QLineEdit *cartesianCountPvEdit_ = nullptr;
  QPushButton *cartesianAxisButton_ = nullptr;
  std::array<QPushButton *, kCartesianPlotTraceCount> cartesianTraceColorButtons_{};
  std::array<QLineEdit *, kCartesianPlotTraceCount> cartesianTraceXEdits_{};
  std::array<QLineEdit *, kCartesianPlotTraceCount> cartesianTraceYEdits_{};
  std::array<QComboBox *, kCartesianPlotTraceCount> cartesianTraceAxisCombos_{};
  std::array<QComboBox *, kCartesianPlotTraceCount> cartesianTraceSideCombos_{};
  QWidget *byteSection_ = nullptr;
  QPushButton *byteForegroundButton_ = nullptr;
  QPushButton *byteBackgroundButton_ = nullptr;
  QComboBox *byteColorModeCombo_ = nullptr;
  QComboBox *byteDirectionCombo_ = nullptr;
  QSpinBox *byteStartBitSpin_ = nullptr;
  QSpinBox *byteEndBitSpin_ = nullptr;
  QLineEdit *byteChannelEdit_ = nullptr;
  QWidget *ledSection_ = nullptr;
  QPushButton *ledForegroundButton_ = nullptr;
  QPushButton *ledBackgroundButton_ = nullptr;
  QPushButton *ledOnColorButton_ = nullptr;
  QPushButton *ledOffColorButton_ = nullptr;
  QPushButton *ledUndefinedColorButton_ = nullptr;
  QComboBox *ledColorModeCombo_ = nullptr;
  QComboBox *ledShapeCombo_ = nullptr;
  QCheckBox *ledBezelCheckBox_ = nullptr;
  QSpinBox *ledStateCountSpin_ = nullptr;
  QWidget *ledStateColorsWidget_ = nullptr;
  std::array<QPushButton *, kLedStateCount> ledStateColorButtons_{};
  QLineEdit *ledChannelEdit_ = nullptr;
  QComboBox *ledVisibilityCombo_ = nullptr;
  QLineEdit *ledVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> ledVisibilityChannelEdits_{};
  QWidget *expressionChannelSection_ = nullptr;
  QPushButton *expressionChannelForegroundButton_ = nullptr;
  QPushButton *expressionChannelBackgroundButton_ = nullptr;
  QLineEdit *expressionChannelVariableEdit_ = nullptr;
  QLineEdit *expressionChannelCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> expressionChannelChannelEdits_{};
  QDoubleSpinBox *expressionChannelInitialValueSpin_ = nullptr;
  QComboBox *expressionChannelEventSignalCombo_ = nullptr;
  QSpinBox *expressionChannelPrecisionSpin_ = nullptr;
  QPushButton *rectangleForegroundButton_ = nullptr;
  QComboBox *rectangleFillCombo_ = nullptr;
  QComboBox *rectangleLineStyleCombo_ = nullptr;
  QLineEdit *rectangleLineWidthEdit_ = nullptr;
  QComboBox *rectangleColorModeCombo_ = nullptr;
  QComboBox *rectangleVisibilityCombo_ = nullptr;
  QLineEdit *rectangleVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> rectangleChannelEdits_{};
  QPushButton *compositeForegroundButton_ = nullptr;
  QPushButton *compositeBackgroundButton_ = nullptr;
  QLineEdit *compositeFileEdit_ = nullptr;
  QComboBox *compositeVisibilityCombo_ = nullptr;
  QLineEdit *compositeVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> compositeChannelEdits_{};
  QComboBox *imageTypeCombo_ = nullptr;
  QLineEdit *imageNameEdit_ = nullptr;
  QLineEdit *imageCalcEdit_ = nullptr;
  QComboBox *imageColorModeCombo_ = nullptr;
  QComboBox *imageVisibilityCombo_ = nullptr;
  QLineEdit *imageVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> imageChannelEdits_{};
  QLabel *arcBeginLabel_ = nullptr;
  QLabel *arcPathLabel_ = nullptr;
  QSpinBox *arcBeginSpin_ = nullptr;
  QSpinBox *arcPathSpin_ = nullptr;
  QPushButton *lineColorButton_ = nullptr;
  QComboBox *lineLineStyleCombo_ = nullptr;
  QLineEdit *lineLineWidthEdit_ = nullptr;
  QComboBox *lineColorModeCombo_ = nullptr;
  QComboBox *lineVisibilityCombo_ = nullptr;
  QLineEdit *lineVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> lineChannelEdits_{};
  QPushButton *foregroundButton_ = nullptr;
  QPushButton *backgroundButton_ = nullptr;
  QComboBox *gridOnCombo_ = nullptr;
  QComboBox *snapToGridCombo_ = nullptr;
  QLabel *elementLabel_ = nullptr;
  QScrollArea *scrollArea_ = nullptr;
  QWidget *entriesWidget_ = nullptr;
  SelectionKind selectionKind_ = SelectionKind::kNone;
  bool rectangleIsArc_ = false;
  enum class GeometryField { kX, kY, kWidth, kHeight };
  void setupGeometryField(QLineEdit *edit, GeometryField field);


  void setupGridSpacingField(QLineEdit *edit);


  void commitGeometryField(GeometryField field);


  void revertLineEdit(QLineEdit *edit);


  void updateGeometryEdits(const QRect &geometry);


  void updateCommittedTexts();


  void commitGridSpacing();


  std::function<QRect()> geometryGetter_;
  std::function<void(const QRect &)> geometrySetter_;
  QRect lastCommittedGeometry_;
  QHash<QLineEdit *, QString> committedTexts_;
  QHash<QWidget *, QLabel *> fieldLabels_;
  ColorPaletteDialog *colorPaletteDialog_ = nullptr;
  PvLimitsDialog *pvLimitsDialog_ = nullptr;
  CartesianAxisDialog *cartesianAxisDialog_ = nullptr;
  QPushButton *activeColorButton_ = nullptr;
  std::function<QColor()> foregroundColorGetter_;
  std::function<void(const QColor &)> foregroundColorSetter_;
  std::function<QColor()> backgroundColorGetter_;
  std::function<void(const QColor &)> backgroundColorSetter_;
  std::function<void(const QColor &)> activeColorSetter_;
  std::function<int()> gridSpacingGetter_;
  std::function<void(int)> gridSpacingSetter_;
  std::function<bool()> gridOnGetter_;
  std::function<void(bool)> gridOnSetter_;
  std::function<bool()> snapToGridGetter_;
  std::function<void(bool)> snapToGridSetter_;
  std::function<QString()> textGetter_;
  std::function<void(const QString &)> textSetter_;
  std::function<QColor()> textForegroundGetter_;
  std::function<void(const QColor &)> textForegroundSetter_;
  std::function<Qt::Alignment()> textAlignmentGetter_;
  std::function<void(Qt::Alignment)> textAlignmentSetter_;
  std::function<TextColorMode()> textColorModeGetter_;
  std::function<void(TextColorMode)> textColorModeSetter_;
  std::function<TextVisibilityMode()> textVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> textVisibilityModeSetter_;
  std::function<QString()> textVisibilityCalcGetter_;
  std::function<void(const QString &)> textVisibilityCalcSetter_;
  std::array<std::function<QString()>, 5> textChannelGetters_{};
  std::array<std::function<void(const QString &)>, 5> textChannelSetters_{};
  std::function<QColor()> textMonitorForegroundGetter_;
  std::function<void(const QColor &)> textMonitorForegroundSetter_;
  std::function<QColor()> textMonitorBackgroundGetter_;
  std::function<void(const QColor &)> textMonitorBackgroundSetter_;
  std::function<Qt::Alignment()> textMonitorAlignmentGetter_;
  std::function<void(Qt::Alignment)> textMonitorAlignmentSetter_;
  std::function<TextMonitorFormat()> textMonitorFormatGetter_;
  std::function<void(TextMonitorFormat)> textMonitorFormatSetter_;
  std::function<int()> textMonitorPrecisionGetter_;
  std::function<void(int)> textMonitorPrecisionSetter_;
  std::function<PvLimitSource()> textMonitorPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> textMonitorPrecisionSourceSetter_;
  std::function<int()> textMonitorPrecisionDefaultGetter_;
  std::function<void(int)> textMonitorPrecisionDefaultSetter_;
  std::function<QColor()> pvTableForegroundGetter_;
  std::function<void(const QColor &)> pvTableForegroundSetter_;
  std::function<QColor()> pvTableBackgroundGetter_;
  std::function<void(const QColor &)> pvTableBackgroundSetter_;
  std::function<TextColorMode()> pvTableColorModeGetter_;
  std::function<void(TextColorMode)> pvTableColorModeSetter_;
  std::function<bool()> pvTableShowHeadersGetter_;
  std::function<void(bool)> pvTableShowHeadersSetter_;
  std::function<int()> pvTableFontSizeGetter_;
  std::function<void(int)> pvTableFontSizeSetter_;
  std::function<QString()> pvTableColumnsGetter_;
  std::function<void(const QString &)> pvTableColumnsSetter_;
  std::array<std::function<QString()>, kPvTableRowCount> pvTableRowLabelGetters_{};
  std::array<std::function<void(const QString &)>, kPvTableRowCount> pvTableRowLabelSetters_{};
  std::array<std::function<QString()>, kPvTableRowCount> pvTableRowChannelGetters_{};
  std::array<std::function<void(const QString &)>, kPvTableRowCount> pvTableRowChannelSetters_{};
  std::function<QColor()> waveTableForegroundGetter_;
  std::function<void(const QColor &)> waveTableForegroundSetter_;
  std::function<QColor()> waveTableBackgroundGetter_;
  std::function<void(const QColor &)> waveTableBackgroundSetter_;
  std::function<TextColorMode()> waveTableColorModeGetter_;
  std::function<void(TextColorMode)> waveTableColorModeSetter_;
  std::function<bool()> waveTableShowHeadersGetter_;
  std::function<void(bool)> waveTableShowHeadersSetter_;
  std::function<int()> waveTableFontSizeGetter_;
  std::function<void(int)> waveTableFontSizeSetter_;
  std::function<QString()> waveTableChannelGetter_;
  std::function<void(const QString &)> waveTableChannelSetter_;
  std::function<WaveTableLayout()> waveTableLayoutGetter_;
  std::function<void(WaveTableLayout)> waveTableLayoutSetter_;
  std::function<int()> waveTableColumnsGetter_;
  std::function<void(int)> waveTableColumnsSetter_;
  std::function<int()> waveTableMaxElementsGetter_;
  std::function<void(int)> waveTableMaxElementsSetter_;
  std::function<int()> waveTableIndexBaseGetter_;
  std::function<void(int)> waveTableIndexBaseSetter_;
  std::function<WaveTableValueFormat()> waveTableValueFormatGetter_;
  std::function<void(WaveTableValueFormat)> waveTableValueFormatSetter_;
  std::function<WaveTableCharMode()> waveTableCharModeGetter_;
  std::function<void(WaveTableCharMode)> waveTableCharModeSetter_;
  std::function<TextColorMode()> textMonitorColorModeGetter_;
  std::function<void(TextColorMode)> textMonitorColorModeSetter_;
  std::function<QString()> textMonitorChannelGetter_;
  std::function<void(const QString &)> textMonitorChannelSetter_;
  std::function<PvLimits()> textMonitorLimitsGetter_;
  std::function<void(const PvLimits &)> textMonitorLimitsSetter_;
  std::function<QColor()> textEntryForegroundGetter_;
  std::function<void(const QColor &)> textEntryForegroundSetter_;
  std::function<QColor()> textEntryBackgroundGetter_;
  std::function<void(const QColor &)> textEntryBackgroundSetter_;
  std::function<TextMonitorFormat()> textEntryFormatGetter_;
  std::function<void(TextMonitorFormat)> textEntryFormatSetter_;
  std::function<int()> textEntryPrecisionGetter_;
  std::function<void(int)> textEntryPrecisionSetter_;
  std::function<PvLimitSource()> textEntryPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> textEntryPrecisionSourceSetter_;
  std::function<int()> textEntryPrecisionDefaultGetter_;
  std::function<void(int)> textEntryPrecisionDefaultSetter_;
  std::function<TextColorMode()> textEntryColorModeGetter_;
  std::function<void(TextColorMode)> textEntryColorModeSetter_;
  std::function<QString()> textEntryChannelGetter_;
  std::function<void(const QString &)> textEntryChannelSetter_;
  std::function<PvLimits()> textEntryLimitsGetter_;
  std::function<void(const PvLimits &)> textEntryLimitsSetter_;
  std::function<QColor()> setpointControlForegroundGetter_;
  std::function<void(const QColor &)> setpointControlForegroundSetter_;
  std::function<QColor()> setpointControlBackgroundGetter_;
  std::function<void(const QColor &)> setpointControlBackgroundSetter_;
  std::function<TextMonitorFormat()> setpointControlFormatGetter_;
  std::function<void(TextMonitorFormat)> setpointControlFormatSetter_;
  std::function<int()> setpointControlPrecisionGetter_;
  std::function<void(int)> setpointControlPrecisionSetter_;
  std::function<PvLimitSource()> setpointControlPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> setpointControlPrecisionSourceSetter_;
  std::function<int()> setpointControlPrecisionDefaultGetter_;
  std::function<void(int)> setpointControlPrecisionDefaultSetter_;
  std::function<TextColorMode()> setpointControlColorModeGetter_;
  std::function<void(TextColorMode)> setpointControlColorModeSetter_;
  std::function<QString()> setpointControlSetpointGetter_;
  std::function<void(const QString &)> setpointControlSetpointSetter_;
  std::function<QString()> setpointControlReadbackGetter_;
  std::function<void(const QString &)> setpointControlReadbackSetter_;
  std::function<QString()> setpointControlLabelGetter_;
  std::function<void(const QString &)> setpointControlLabelSetter_;
  std::function<PvLimits()> setpointControlLimitsGetter_;
  std::function<void(const PvLimits &)> setpointControlLimitsSetter_;
  std::function<SetpointToleranceMode()> setpointControlToleranceModeGetter_;
  std::function<void(SetpointToleranceMode)> setpointControlToleranceModeSetter_;
  std::function<double()> setpointControlToleranceGetter_;
  std::function<void(double)> setpointControlToleranceSetter_;
  std::function<bool()> setpointControlShowReadbackGetter_;
  std::function<void(bool)> setpointControlShowReadbackSetter_;
  std::function<QColor()> textAreaForegroundGetter_;
  std::function<void(const QColor &)> textAreaForegroundSetter_;
  std::function<QColor()> textAreaBackgroundGetter_;
  std::function<void(const QColor &)> textAreaBackgroundSetter_;
  std::function<TextMonitorFormat()> textAreaFormatGetter_;
  std::function<void(TextMonitorFormat)> textAreaFormatSetter_;
  std::function<int()> textAreaPrecisionGetter_;
  std::function<void(int)> textAreaPrecisionSetter_;
  std::function<PvLimitSource()> textAreaPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> textAreaPrecisionSourceSetter_;
  std::function<int()> textAreaPrecisionDefaultGetter_;
  std::function<void(int)> textAreaPrecisionDefaultSetter_;
  std::function<TextColorMode()> textAreaColorModeGetter_;
  std::function<void(TextColorMode)> textAreaColorModeSetter_;
  std::function<QString()> textAreaChannelGetter_;
  std::function<void(const QString &)> textAreaChannelSetter_;
  std::function<PvLimits()> textAreaLimitsGetter_;
  std::function<void(const PvLimits &)> textAreaLimitsSetter_;
  std::function<bool()> textAreaReadOnlyGetter_;
  std::function<void(bool)> textAreaReadOnlySetter_;
  std::function<bool()> textAreaWordWrapGetter_;
  std::function<void(bool)> textAreaWordWrapSetter_;
  std::function<TextAreaWrapMode()> textAreaWrapModeGetter_;
  std::function<void(TextAreaWrapMode)> textAreaWrapModeSetter_;
  std::function<int()> textAreaWrapColumnWidthGetter_;
  std::function<void(int)> textAreaWrapColumnWidthSetter_;
  std::function<bool()> textAreaShowVerticalScrollBarGetter_;
  std::function<void(bool)> textAreaShowVerticalScrollBarSetter_;
  std::function<bool()> textAreaShowHorizontalScrollBarGetter_;
  std::function<void(bool)> textAreaShowHorizontalScrollBarSetter_;
  std::function<TextAreaCommitMode()> textAreaCommitModeGetter_;
  std::function<void(TextAreaCommitMode)> textAreaCommitModeSetter_;
  std::function<bool()> textAreaTabInsertsSpacesGetter_;
  std::function<void(bool)> textAreaTabInsertsSpacesSetter_;
  std::function<int()> textAreaTabWidthGetter_;
  std::function<void(int)> textAreaTabWidthSetter_;
  std::function<QString()> textAreaFontFamilyGetter_;
  std::function<void(const QString &)> textAreaFontFamilySetter_;
  std::function<QColor()> sliderForegroundGetter_;
  std::function<void(const QColor &)> sliderForegroundSetter_;
  std::function<QColor()> sliderBackgroundGetter_;
  std::function<void(const QColor &)> sliderBackgroundSetter_;
  std::function<MeterLabel()> sliderLabelGetter_;
  std::function<void(MeterLabel)> sliderLabelSetter_;
  std::function<TextColorMode()> sliderColorModeGetter_;
  std::function<void(TextColorMode)> sliderColorModeSetter_;
  std::function<BarDirection()> sliderDirectionGetter_;
  std::function<void(BarDirection)> sliderDirectionSetter_;
  std::function<double()> sliderIncrementGetter_;
  std::function<void(double)> sliderIncrementSetter_;
  std::function<QString()> sliderChannelGetter_;
  std::function<void(const QString &)> sliderChannelSetter_;
  std::function<PvLimits()> sliderLimitsGetter_;
  std::function<void(const PvLimits &)> sliderLimitsSetter_;
  std::function<QColor()> wheelSwitchForegroundGetter_;
  std::function<void(const QColor &)> wheelSwitchForegroundSetter_;
  std::function<QColor()> wheelSwitchBackgroundGetter_;
  std::function<void(const QColor &)> wheelSwitchBackgroundSetter_;
  std::function<TextColorMode()> wheelSwitchColorModeGetter_;
  std::function<void(TextColorMode)> wheelSwitchColorModeSetter_;
  std::function<double()> wheelSwitchPrecisionGetter_;
  std::function<void(double)> wheelSwitchPrecisionSetter_;
  std::function<QString()> wheelSwitchFormatGetter_;
  std::function<void(const QString &)> wheelSwitchFormatSetter_;
  std::function<QString()> wheelSwitchChannelGetter_;
  std::function<void(const QString &)> wheelSwitchChannelSetter_;
  std::function<PvLimits()> wheelSwitchLimitsGetter_;
  std::function<void(const PvLimits &)> wheelSwitchLimitsSetter_;
  std::function<QColor()> choiceButtonForegroundGetter_;
  std::function<void(const QColor &)> choiceButtonForegroundSetter_;
  std::function<QColor()> choiceButtonBackgroundGetter_;
  std::function<void(const QColor &)> choiceButtonBackgroundSetter_;
  std::function<TextColorMode()> choiceButtonColorModeGetter_;
  std::function<void(TextColorMode)> choiceButtonColorModeSetter_;
  std::function<ChoiceButtonStacking()> choiceButtonStackingGetter_;
  std::function<void(ChoiceButtonStacking)> choiceButtonStackingSetter_;
  std::function<QString()> choiceButtonChannelGetter_;
  std::function<void(const QString &)> choiceButtonChannelSetter_;
  std::function<QColor()> menuForegroundGetter_;
  std::function<void(const QColor &)> menuForegroundSetter_;
  std::function<QColor()> menuBackgroundGetter_;
  std::function<void(const QColor &)> menuBackgroundSetter_;
  std::function<TextColorMode()> menuColorModeGetter_;
  std::function<void(TextColorMode)> menuColorModeSetter_;
  std::function<QString()> menuChannelGetter_;
  std::function<void(const QString &)> menuChannelSetter_;
  std::function<QColor()> messageButtonForegroundGetter_;
  std::function<void(const QColor &)> messageButtonForegroundSetter_;
  std::function<QColor()> messageButtonBackgroundGetter_;
  std::function<void(const QColor &)> messageButtonBackgroundSetter_;
  std::function<TextColorMode()> messageButtonColorModeGetter_;
  std::function<void(TextColorMode)> messageButtonColorModeSetter_;
  std::function<QString()> messageButtonLabelGetter_;
  std::function<void(const QString &)> messageButtonLabelSetter_;
  std::function<QString()> messageButtonPressGetter_;
  std::function<void(const QString &)> messageButtonPressSetter_;
  std::function<QString()> messageButtonReleaseGetter_;
  std::function<void(const QString &)> messageButtonReleaseSetter_;
  std::function<QString()> messageButtonChannelGetter_;
  std::function<void(const QString &)> messageButtonChannelSetter_;
  std::function<QColor()> shellCommandForegroundGetter_;
  std::function<void(const QColor &)> shellCommandForegroundSetter_;
  std::function<QColor()> shellCommandBackgroundGetter_;
  std::function<void(const QColor &)> shellCommandBackgroundSetter_;
  std::function<QString()> shellCommandLabelGetter_;
  std::function<void(const QString &)> shellCommandLabelSetter_;
  std::array<std::function<QString()>, kShellCommandEntryCount> shellCommandEntryLabelGetters_{};
  std::array<std::function<void(const QString &)>, kShellCommandEntryCount> shellCommandEntryLabelSetters_{};
  std::array<std::function<QString()>, kShellCommandEntryCount> shellCommandEntryCommandGetters_{};
  std::array<std::function<void(const QString &)>, kShellCommandEntryCount> shellCommandEntryCommandSetters_{};
  std::array<std::function<QString()>, kShellCommandEntryCount> shellCommandEntryArgsGetters_{};
  std::array<std::function<void(const QString &)>, kShellCommandEntryCount> shellCommandEntryArgsSetters_{};
  std::function<QColor()> relatedDisplayForegroundGetter_;
  std::function<void(const QColor &)> relatedDisplayForegroundSetter_;
  std::function<QColor()> relatedDisplayBackgroundGetter_;
  std::function<void(const QColor &)> relatedDisplayBackgroundSetter_;
  std::function<QString()> relatedDisplayLabelGetter_;
  std::function<void(const QString &)> relatedDisplayLabelSetter_;
  std::function<RelatedDisplayVisual()> relatedDisplayVisualGetter_;
  std::function<void(RelatedDisplayVisual)> relatedDisplayVisualSetter_;
  std::array<std::function<QString()>, kRelatedDisplayEntryCount> relatedDisplayEntryLabelGetters_{};
  std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> relatedDisplayEntryLabelSetters_{};
  std::array<std::function<QString()>, kRelatedDisplayEntryCount> relatedDisplayEntryNameGetters_{};
  std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> relatedDisplayEntryNameSetters_{};
  std::array<std::function<QString()>, kRelatedDisplayEntryCount> relatedDisplayEntryArgsGetters_{};
  std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> relatedDisplayEntryArgsSetters_{};
  std::array<std::function<RelatedDisplayMode()>, kRelatedDisplayEntryCount> relatedDisplayEntryModeGetters_{};
  std::array<std::function<void(RelatedDisplayMode)>, kRelatedDisplayEntryCount> relatedDisplayEntryModeSetters_{};
  std::function<QColor()> meterForegroundGetter_;
  std::function<void(const QColor &)> meterForegroundSetter_;
  std::function<QColor()> meterBackgroundGetter_;
  std::function<void(const QColor &)> meterBackgroundSetter_;
  std::function<MeterLabel()> meterLabelGetter_;
  std::function<void(MeterLabel)> meterLabelSetter_;
  std::function<TextColorMode()> meterColorModeGetter_;
  std::function<void(TextColorMode)> meterColorModeSetter_;
  std::function<QString()> meterChannelGetter_;
  std::function<void(const QString &)> meterChannelSetter_;
  std::function<PvLimits()> meterLimitsGetter_;
  std::function<void(const PvLimits &)> meterLimitsSetter_;
  std::function<QColor()> barForegroundGetter_;
  std::function<void(const QColor &)> barForegroundSetter_;
  std::function<QColor()> barBackgroundGetter_;
  std::function<void(const QColor &)> barBackgroundSetter_;
  std::function<MeterLabel()> barLabelGetter_;
  std::function<void(MeterLabel)> barLabelSetter_;
  std::function<TextColorMode()> barColorModeGetter_;
  std::function<void(TextColorMode)> barColorModeSetter_;
  std::function<BarDirection()> barDirectionGetter_;
  std::function<void(BarDirection)> barDirectionSetter_;
  std::function<BarFill()> barFillModeGetter_;
  std::function<void(BarFill)> barFillModeSetter_;
  std::function<QString()> barChannelGetter_;
  std::function<void(const QString &)> barChannelSetter_;
  std::function<PvLimits()> barLimitsGetter_;
  std::function<void(const PvLimits &)> barLimitsSetter_;
  std::function<QColor()> thermometerForegroundGetter_;
  std::function<void(const QColor &)> thermometerForegroundSetter_;
  std::function<QColor()> thermometerBackgroundGetter_;
  std::function<void(const QColor &)> thermometerBackgroundSetter_;
  std::function<QColor()> thermometerTextGetter_;
  std::function<void(const QColor &)> thermometerTextSetter_;
  std::function<MeterLabel()> thermometerLabelGetter_;
  std::function<void(MeterLabel)> thermometerLabelSetter_;
  std::function<TextColorMode()> thermometerColorModeGetter_;
  std::function<void(TextColorMode)> thermometerColorModeSetter_;
  std::function<TextVisibilityMode()> thermometerVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> thermometerVisibilityModeSetter_;
  std::function<QString()> thermometerVisibilityCalcGetter_;
  std::function<void(const QString &)> thermometerVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> thermometerVisibilityChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> thermometerVisibilityChannelSetters_{};
  std::function<TextMonitorFormat()> thermometerFormatGetter_;
  std::function<void(TextMonitorFormat)> thermometerFormatSetter_;
  std::function<bool()> thermometerShowValueGetter_;
  std::function<void(bool)> thermometerShowValueSetter_;
  std::function<QString()> thermometerChannelGetter_;
  std::function<void(const QString &)> thermometerChannelSetter_;
  std::function<PvLimits()> thermometerLimitsGetter_;
  std::function<void(const PvLimits &)> thermometerLimitsSetter_;
  std::function<QColor()> scaleForegroundGetter_;
  std::function<void(const QColor &)> scaleForegroundSetter_;
  std::function<QColor()> scaleBackgroundGetter_;
  std::function<void(const QColor &)> scaleBackgroundSetter_;
  std::function<MeterLabel()> scaleLabelGetter_;
  std::function<void(MeterLabel)> scaleLabelSetter_;
  std::function<TextColorMode()> scaleColorModeGetter_;
  std::function<void(TextColorMode)> scaleColorModeSetter_;
  std::function<BarDirection()> scaleDirectionGetter_;
  std::function<void(BarDirection)> scaleDirectionSetter_;
  std::function<QString()> scaleChannelGetter_;
  std::function<void(const QString &)> scaleChannelSetter_;
  std::function<PvLimits()> scaleLimitsGetter_;
  std::function<void(const PvLimits &)> scaleLimitsSetter_;
  std::function<QString()> stripTitleGetter_;
  std::function<void(const QString &)> stripTitleSetter_;
  std::function<QString()> stripXLabelGetter_;
  std::function<void(const QString &)> stripXLabelSetter_;
  std::function<QString()> stripYLabelGetter_;
  std::function<void(const QString &)> stripYLabelSetter_;
  std::function<QColor()> stripForegroundGetter_;
  std::function<void(const QColor &)> stripForegroundSetter_;
  std::function<QColor()> stripBackgroundGetter_;
  std::function<void(const QColor &)> stripBackgroundSetter_;
  std::function<double()> stripPeriodGetter_;
  std::function<void(double)> stripPeriodSetter_;
  std::function<TimeUnits()> stripUnitsGetter_;
  std::function<void(TimeUnits)> stripUnitsSetter_;
  std::array<std::function<QString()>, kStripChartPenCount> stripPenChannelGetters_{};
  std::array<std::function<void(const QString &)>, kStripChartPenCount> stripPenChannelSetters_{};
  std::array<std::function<QColor()>, kStripChartPenCount> stripPenColorGetters_{};
  std::array<std::function<void(const QColor &)>, kStripChartPenCount> stripPenColorSetters_{};
  std::array<std::function<PvLimits()>, kStripChartPenCount> stripPenLimitsGetters_{};
  std::array<std::function<void(const PvLimits &)>, kStripChartPenCount> stripPenLimitsSetters_{};
  std::function<QString()> cartesianTitleGetter_;
  std::function<void(const QString &)> cartesianTitleSetter_;
  std::function<QString()> cartesianXLabelGetter_;
  std::function<void(const QString &)> cartesianXLabelSetter_;
  std::array<std::function<QString()>, 4> cartesianYLabelGetters_{};
  std::array<std::function<void(const QString &)>, 4> cartesianYLabelSetters_{};
  std::function<QColor()> cartesianForegroundGetter_;
  std::function<void(const QColor &)> cartesianForegroundSetter_;
  std::function<QColor()> cartesianBackgroundGetter_;
  std::function<void(const QColor &)> cartesianBackgroundSetter_;
  std::function<bool()> cartesianDrawMajorGetter_;
  std::function<void(bool)> cartesianDrawMajorSetter_;
  std::function<bool()> cartesianDrawMinorGetter_;
  std::function<void(bool)> cartesianDrawMinorSetter_;
  std::function<CartesianPlotStyle()> cartesianStyleGetter_;
  std::function<void(CartesianPlotStyle)> cartesianStyleSetter_;
  std::function<bool()> cartesianEraseOldestGetter_;
  std::function<void(bool)> cartesianEraseOldestSetter_;
  std::function<int()> cartesianCountGetter_;
  std::function<void(int)> cartesianCountSetter_;
  std::function<CartesianPlotEraseMode()> cartesianEraseModeGetter_;
  std::function<void(CartesianPlotEraseMode)> cartesianEraseModeSetter_;
  std::function<QString()> cartesianTriggerGetter_;
  std::function<void(const QString &)> cartesianTriggerSetter_;
  std::function<QString()> cartesianEraseGetter_;
  std::function<void(const QString &)> cartesianEraseSetter_;
  std::function<QString()> cartesianCountPvGetter_;
  std::function<void(const QString &)> cartesianCountPvSetter_;
  std::array<std::function<QString()>, kCartesianPlotTraceCount> cartesianTraceXGetters_{};
  std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> cartesianTraceXSetters_{};
  std::array<std::function<QString()>, kCartesianPlotTraceCount> cartesianTraceYGetters_{};
  std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> cartesianTraceYSetters_{};
  std::array<std::function<QColor()>, kCartesianPlotTraceCount> cartesianTraceColorGetters_{};
  std::array<std::function<void(const QColor &)>, kCartesianPlotTraceCount> cartesianTraceColorSetters_{};
  std::array<std::function<CartesianPlotYAxis()>, kCartesianPlotTraceCount> cartesianTraceAxisGetters_{};
  std::array<std::function<void(CartesianPlotYAxis)>, kCartesianPlotTraceCount> cartesianTraceAxisSetters_{};
  std::array<std::function<bool()>, kCartesianPlotTraceCount> cartesianTraceSideGetters_{};
  std::array<std::function<void(bool)>, kCartesianPlotTraceCount> cartesianTraceSideSetters_{};
  std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> cartesianAxisStyleGetters_{};
  std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> cartesianAxisStyleSetters_{};
  std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> cartesianAxisRangeGetters_{};
  std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> cartesianAxisRangeSetters_{};
  std::array<std::function<double()>, kCartesianAxisCount> cartesianAxisMinimumGetters_{};
  std::array<std::function<void(double)>, kCartesianAxisCount> cartesianAxisMinimumSetters_{};
  std::array<std::function<double()>, kCartesianAxisCount> cartesianAxisMaximumGetters_{};
  std::array<std::function<void(double)>, kCartesianAxisCount> cartesianAxisMaximumSetters_{};
  std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> cartesianAxisTimeFormatGetters_{};
  std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> cartesianAxisTimeFormatSetters_{};
  std::function<QColor()> byteForegroundGetter_;
  std::function<void(const QColor &)> byteForegroundSetter_;
  std::function<QColor()> byteBackgroundGetter_;
  std::function<void(const QColor &)> byteBackgroundSetter_;
  std::function<TextColorMode()> byteColorModeGetter_;
  std::function<void(TextColorMode)> byteColorModeSetter_;
  std::function<BarDirection()> byteDirectionGetter_;
  std::function<void(BarDirection)> byteDirectionSetter_;
  std::function<int()> byteStartBitGetter_;
  std::function<void(int)> byteStartBitSetter_;
  std::function<int()> byteEndBitGetter_;
  std::function<void(int)> byteEndBitSetter_;
  std::function<QString()> byteChannelGetter_;
  std::function<void(const QString &)> byteChannelSetter_;
  std::function<QColor()> ledForegroundGetter_;
  std::function<void(const QColor &)> ledForegroundSetter_;
  std::function<QColor()> ledBackgroundGetter_;
  std::function<void(const QColor &)> ledBackgroundSetter_;
  std::function<TextColorMode()> ledColorModeGetter_;
  std::function<void(TextColorMode)> ledColorModeSetter_;
  std::function<LedShape()> ledShapeGetter_;
  std::function<void(LedShape)> ledShapeSetter_;
  std::function<bool()> ledBezelGetter_;
  std::function<void(bool)> ledBezelSetter_;
  std::function<QColor()> ledOnColorGetter_;
  std::function<void(const QColor &)> ledOnColorSetter_;
  std::function<QColor()> ledOffColorGetter_;
  std::function<void(const QColor &)> ledOffColorSetter_;
  std::function<QColor()> ledUndefinedColorGetter_;
  std::function<void(const QColor &)> ledUndefinedColorSetter_;
  std::array<std::function<QColor()>, kLedStateCount> ledStateColorGetters_{};
  std::array<std::function<void(const QColor &)>, kLedStateCount>
      ledStateColorSetters_{};
  std::function<int()> ledStateCountGetter_;
  std::function<void(int)> ledStateCountSetter_;
  std::function<QString()> ledChannelGetter_;
  std::function<void(const QString &)> ledChannelSetter_;
  std::function<TextVisibilityMode()> ledVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> ledVisibilityModeSetter_;
  std::function<QString()> ledVisibilityCalcGetter_;
  std::function<void(const QString &)> ledVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> ledVisibilityChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4>
      ledVisibilityChannelSetters_{};
  std::function<QColor()> expressionChannelForegroundGetter_;
  std::function<void(const QColor &)> expressionChannelForegroundSetter_;
  std::function<QColor()> expressionChannelBackgroundGetter_;
  std::function<void(const QColor &)> expressionChannelBackgroundSetter_;
  std::function<QString()> expressionChannelVariableGetter_;
  std::function<void(const QString &)> expressionChannelVariableSetter_;
  std::function<QString()> expressionChannelCalcGetter_;
  std::function<void(const QString &)> expressionChannelCalcSetter_;
  std::array<std::function<QString()>, 4> expressionChannelChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4>
      expressionChannelChannelSetters_{};
  std::function<double()> expressionChannelInitialValueGetter_;
  std::function<void(double)> expressionChannelInitialValueSetter_;
  std::function<ExpressionChannelEventSignalMode()>
      expressionChannelEventSignalGetter_;
  std::function<void(ExpressionChannelEventSignalMode)>
      expressionChannelEventSignalSetter_;
  std::function<int()> expressionChannelPrecisionGetter_;
  std::function<void(int)> expressionChannelPrecisionSetter_;
  QString committedTextString_;
  std::function<QColor()> rectangleForegroundGetter_;
  std::function<void(const QColor &)> rectangleForegroundSetter_;
  std::function<RectangleFill()> rectangleFillGetter_;
  std::function<void(RectangleFill)> rectangleFillSetter_;
  std::function<RectangleLineStyle()> rectangleLineStyleGetter_;
  std::function<void(RectangleLineStyle)> rectangleLineStyleSetter_;
  std::function<int()> rectangleLineWidthGetter_;
  std::function<void(int)> rectangleLineWidthSetter_;
  std::function<int()> arcBeginGetter_;
  std::function<void(int)> arcBeginSetter_;
  std::function<int()> arcPathGetter_;
  std::function<void(int)> arcPathSetter_;
  std::function<TextColorMode()> rectangleColorModeGetter_;
  std::function<void(TextColorMode)> rectangleColorModeSetter_;
  std::function<TextVisibilityMode()> rectangleVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> rectangleVisibilityModeSetter_;
  std::function<QString()> rectangleVisibilityCalcGetter_;
  std::function<void(const QString &)> rectangleVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> rectangleChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> rectangleChannelSetters_{};
  std::function<QColor()> compositeForegroundGetter_;
  std::function<void(const QColor &)> compositeForegroundSetter_;
  std::function<QColor()> compositeBackgroundGetter_;
  std::function<void(const QColor &)> compositeBackgroundSetter_;
  std::function<QString()> compositeFileGetter_;
  std::function<void(const QString &)> compositeFileSetter_;
  std::function<TextVisibilityMode()> compositeVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> compositeVisibilityModeSetter_;
  std::function<QString()> compositeVisibilityCalcGetter_;
  std::function<void(const QString &)> compositeVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> compositeChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> compositeChannelSetters_{};
  std::function<ImageType()> imageTypeGetter_;
  std::function<void(ImageType)> imageTypeSetter_;
  std::function<QString()> imageNameGetter_;
  std::function<void(const QString &)> imageNameSetter_;
  std::function<QString()> imageCalcGetter_;
  std::function<void(const QString &)> imageCalcSetter_;
  std::function<TextColorMode()> imageColorModeGetter_;
  std::function<void(TextColorMode)> imageColorModeSetter_;
  std::function<TextVisibilityMode()> imageVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> imageVisibilityModeSetter_;
  std::function<QString()> imageVisibilityCalcGetter_;
  std::function<void(const QString &)> imageVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> imageChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> imageChannelSetters_{};
  std::function<QString()> heatmapTitleGetter_;
  std::function<void(const QString &)> heatmapTitleSetter_;
  std::function<QString()> heatmapDataChannelGetter_;
  std::function<void(const QString &)> heatmapDataChannelSetter_;
  std::function<HeatmapDimensionSource()> heatmapXSourceGetter_;
  std::function<void(HeatmapDimensionSource)> heatmapXSourceSetter_;
  std::function<HeatmapDimensionSource()> heatmapYSourceGetter_;
  std::function<void(HeatmapDimensionSource)> heatmapYSourceSetter_;
  std::function<int()> heatmapXDimensionGetter_;
  std::function<void(int)> heatmapXDimensionSetter_;
  std::function<int()> heatmapYDimensionGetter_;
  std::function<void(int)> heatmapYDimensionSetter_;
  std::function<QString()> heatmapXDimChannelGetter_;
  std::function<void(const QString &)> heatmapXDimChannelSetter_;
  std::function<QString()> heatmapYDimChannelGetter_;
  std::function<void(const QString &)> heatmapYDimChannelSetter_;
  std::function<HeatmapOrder()> heatmapOrderGetter_;
  std::function<void(HeatmapOrder)> heatmapOrderSetter_;
  std::function<HeatmapColorMap()> heatmapColorMapGetter_;
  std::function<void(HeatmapColorMap)> heatmapColorMapSetter_;
  std::function<bool()> heatmapInvertGreyscaleGetter_;
  std::function<void(bool)> heatmapInvertGreyscaleSetter_;
  std::function<bool()> heatmapPreserveAspectRatioGetter_;
  std::function<void(bool)> heatmapPreserveAspectRatioSetter_;
  std::function<bool()> heatmapFlipHorizontalGetter_;
  std::function<void(bool)> heatmapFlipHorizontalSetter_;
  std::function<bool()> heatmapFlipVerticalGetter_;
  std::function<void(bool)> heatmapFlipVerticalSetter_;
  std::function<HeatmapRotation()> heatmapRotationGetter_;
  std::function<void(HeatmapRotation)> heatmapRotationSetter_;
  std::function<bool()> heatmapShowTopProfileGetter_;
  std::function<void(bool)> heatmapShowTopProfileSetter_;
  std::function<bool()> heatmapShowRightProfileGetter_;
  std::function<void(bool)> heatmapShowRightProfileSetter_;
  std::function<HeatmapProfileMode()> heatmapProfileModeGetter_;
  std::function<void(HeatmapProfileMode)> heatmapProfileModeSetter_;
  std::function<QColor()> waterfallForegroundGetter_;
  std::function<void(const QColor &)> waterfallForegroundSetter_;
  std::function<QColor()> waterfallBackgroundGetter_;
  std::function<void(const QColor &)> waterfallBackgroundSetter_;
  std::function<QString()> waterfallTitleGetter_;
  std::function<void(const QString &)> waterfallTitleSetter_;
  std::function<QString()> waterfallXLabelGetter_;
  std::function<void(const QString &)> waterfallXLabelSetter_;
  std::function<QString()> waterfallYLabelGetter_;
  std::function<void(const QString &)> waterfallYLabelSetter_;
  std::function<QString()> waterfallDataChannelGetter_;
  std::function<void(const QString &)> waterfallDataChannelSetter_;
  std::function<QString()> waterfallCountChannelGetter_;
  std::function<void(const QString &)> waterfallCountChannelSetter_;
  std::function<QString()> waterfallTriggerChannelGetter_;
  std::function<void(const QString &)> waterfallTriggerChannelSetter_;
  std::function<QString()> waterfallEraseChannelGetter_;
  std::function<void(const QString &)> waterfallEraseChannelSetter_;
  std::function<WaterfallEraseMode()> waterfallEraseModeGetter_;
  std::function<void(WaterfallEraseMode)> waterfallEraseModeSetter_;
  std::function<int()> waterfallHistoryCountGetter_;
  std::function<void(int)> waterfallHistoryCountSetter_;
  std::function<WaterfallScrollDirection()> waterfallScrollDirectionGetter_;
  std::function<void(WaterfallScrollDirection)>
      waterfallScrollDirectionSetter_;
  std::function<HeatmapColorMap()> waterfallColorMapGetter_;
  std::function<void(HeatmapColorMap)> waterfallColorMapSetter_;
  std::function<bool()> waterfallInvertGreyscaleGetter_;
  std::function<void(bool)> waterfallInvertGreyscaleSetter_;
  std::function<WaterfallIntensityScale()> waterfallIntensityScaleGetter_;
  std::function<void(WaterfallIntensityScale)> waterfallIntensityScaleSetter_;
  std::function<double()> waterfallIntensityMinGetter_;
  std::function<void(double)> waterfallIntensityMinSetter_;
  std::function<double()> waterfallIntensityMaxGetter_;
  std::function<void(double)> waterfallIntensityMaxSetter_;
  std::function<bool()> waterfallShowLegendGetter_;
  std::function<void(bool)> waterfallShowLegendSetter_;
  std::function<bool()> waterfallShowGridGetter_;
  std::function<void(bool)> waterfallShowGridSetter_;
  std::function<double()> waterfallSamplePeriodGetter_;
  std::function<void(double)> waterfallSamplePeriodSetter_;
  std::function<TimeUnits()> waterfallUnitsGetter_;
  std::function<void(TimeUnits)> waterfallUnitsSetter_;
  std::function<QColor()> lineColorGetter_;
  std::function<void(const QColor &)> lineColorSetter_;
  std::function<RectangleLineStyle()> lineLineStyleGetter_;
  std::function<void(RectangleLineStyle)> lineLineStyleSetter_;
  std::function<int()> lineLineWidthGetter_;
  std::function<void(int)> lineLineWidthSetter_;
  std::function<TextColorMode()> lineColorModeGetter_;
  std::function<void(TextColorMode)> lineColorModeSetter_;
  std::function<TextVisibilityMode()> lineVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> lineVisibilityModeSetter_;
  std::function<QString()> lineVisibilityCalcGetter_;
  std::function<void(const QString &)> lineVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> lineChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> lineChannelSetters_{};

  QLineEdit *editForField(GeometryField field) const;


  void openColorPalette(QPushButton *button, const QString &description,
      const std::function<void(const QColor &)> &setter);


  bool positionColorPaletteRelativeToResource();


  void scheduleColorPalettePlacement();


  bool placeColorPaletteRelativeToReference(ColorPaletteDialog *palette,
      const QSize &paletteSize, const QRect &referenceFrame,
      const QRect &available);


  void openTextEntryPvLimitsDialog();


  void openSetpointControlPvLimitsDialog();


  void openTextAreaPvLimitsDialog();


  void openTextMonitorPvLimitsDialog();


  void openMeterPvLimitsDialog();


  void openSliderPvLimitsDialog();


  void openWheelSwitchPvLimitsDialog();


  void openBarMonitorPvLimitsDialog();


  void openThermometerPvLimitsDialog();


  void openScaleMonitorPvLimitsDialog();


  void openCartesianAxisDialog();


  void positionCartesianAxisDialog(CartesianAxisDialog *dialog);


  void positionPvLimitsDialog(PvLimitsDialog *dialog);


  QColor colorFromButton(const QPushButton *button) const;


  QColor currentForegroundColor() const;


  PvLimitsDialog *ensurePvLimitsDialog();


  CartesianAxisDialog *ensureCartesianAxisDialog();


  QColor currentBackgroundColor() const;

};
