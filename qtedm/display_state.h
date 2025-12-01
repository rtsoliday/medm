#pragma once

#include <QList>
#include <QPointer>
#include <QPoint>

#include <functional>
#include <memory>

class DisplayWindow;
class QMainWindow;
class DisplayListDialog;
class FindPvDialog;

struct ClipboardContent
{
  std::function<void(DisplayWindow &, const QPoint &)> paste;
  QPoint nextOffset = QPoint(10, 10);
  bool hasPasted = false;

  bool isValid() const
  {
    return static_cast<bool>(paste);
  }
};

enum class CreateTool {
  kNone,
  kText,
  kTextMonitor,
  kTextEntry,
  kSlider,
  kWheelSwitch,
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
  bool raiseMessageWindow = true;
  QList<QPointer<DisplayWindow>> displays;
  CreateTool createTool = CreateTool::kNone;
  QPointer<QMainWindow> mainWindow;
  QPointer<DisplayListDialog> displayListDialog;
  QPointer<FindPvDialog> findPvDialog;
  QPointer<DisplayWindow> activeDisplay;
  std::shared_ptr<std::function<void()>> updateMenus;
  std::shared_ptr<ClipboardContent> clipboard;
};
