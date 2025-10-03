#pragma once

#include <QList>
#include <QPointer>

#include <functional>
#include <memory>

class DisplayWindow;

enum class CreateTool {
  kNone,
  kText,
  kTextMonitor,
  kTextEntry,
  kSlider,
  kChoiceButton,
  kMenu,
  kMessageButton,
  kShellCommand,
  kRelatedDisplay,
  kMeter,
  kBarMonitor,
  kByteMonitor,
  kScaleMonitor,
  kStripChart,
  kCartesianPlot,
  kRectangle,
  kOval,
  kArc,
  kPolygon,
  kPolyline,
  kLine,
  kImage,
};

struct DisplayState {
  bool editMode = true;
  QList<QPointer<DisplayWindow>> displays;
  CreateTool createTool = CreateTool::kNone;
  QPointer<DisplayWindow> activeDisplay;
  std::shared_ptr<std::function<void()>> updateMenus;
};

