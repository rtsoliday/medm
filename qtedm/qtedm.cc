#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPointer>
#include <QRadioButton>
#include <QSizePolicy>
#include <QMessageBox>
#include <QStyleFactory>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <memory>

#include "display_properties.h"
#include "display_state.h"
#include "display_list_dialog.h"
#include "display_window.h"
#include "legacy_fonts.h"
#include "main_window_controller.h"
#include "window_utils.h"

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  // High-DPI is on by default in Qt6
#else
  // Opt-in for sensible DPI scaling on Qt5
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

  QApplication app(argc, argv);

  if (auto *fusionStyle =
          QStyleFactory::create(QStringLiteral("Fusion"))) {
    app.setStyle(fusionStyle);
  }

  const QFont fixed10Font = LegacyFonts::fontOrDefault(
      QStringLiteral("widgetDM_10"),
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
  app.setFont(fixed10Font);

  const QFont fixed13Font = LegacyFonts::fontOrDefault(
      QStringLiteral("miscFixed13"), fixed10Font);

  QMainWindow win;
  win.setObjectName("QtedmMainWindow");
  win.setWindowTitle("QtEDM");

  const QColor backgroundColor(0xb0, 0xc3, 0xca);
  const QColor highlightColor = backgroundColor.lighter(120);
  const QColor midHighlightColor = backgroundColor.lighter(108);
  const QColor shadowColor = backgroundColor.darker(120);
  const QColor midShadowColor = backgroundColor.darker(140);
  const QColor disabledTextColor(0x64, 0x64, 0x64);
  QPalette palette = win.palette();
  palette.setColor(QPalette::Window, backgroundColor);
  palette.setColor(QPalette::Base, backgroundColor);
  palette.setColor(QPalette::AlternateBase, backgroundColor);
  palette.setColor(QPalette::Button, backgroundColor);
  palette.setColor(QPalette::WindowText, Qt::black);
  palette.setColor(QPalette::ButtonText, Qt::black);
  palette.setColor(QPalette::Light, highlightColor);
  palette.setColor(QPalette::Midlight, midHighlightColor);
  palette.setColor(QPalette::Dark, shadowColor);
  palette.setColor(QPalette::Mid, midShadowColor);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledTextColor);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledTextColor);
  palette.setColor(QPalette::Disabled, QPalette::Text, disabledTextColor);
  palette.setColor(QPalette::Disabled, QPalette::Button, backgroundColor);
  win.setPalette(palette);

  auto *menuBar = win.menuBar();
  menuBar->setAutoFillBackground(true);
  menuBar->setPalette(palette);
  menuBar->setFont(fixed13Font);

  auto *fileMenu = menuBar->addMenu("&File");
  fileMenu->setFont(fixed13Font);
  auto *newAct = fileMenu->addAction("&New");
  newAct->setShortcut(QKeySequence::New);
  auto *openAct = fileMenu->addAction("&Open...");
  openAct->setShortcut(QKeySequence::Open);
  auto *saveAct = fileMenu->addAction("&Save");
  saveAct->setShortcut(QKeySequence::Save);
  auto *saveAsAct = fileMenu->addAction("Save &As...");
  auto *closeAct = fileMenu->addAction("&Close");
  fileMenu->addSeparator();
  fileMenu->addAction("Print Set&up...");
  fileMenu->addAction("&Print");
  fileMenu->addSeparator();
  auto *exitAct = fileMenu->addAction("E&xit");
  exitAct->setShortcut(QKeySequence::Quit);
  QObject::connect(exitAct, &QAction::triggered, &app, &QApplication::quit);
  saveAct->setEnabled(false);
  saveAsAct->setEnabled(false);
  closeAct->setEnabled(false);

  auto *editMenu = menuBar->addMenu("&Edit");
  editMenu->setFont(fixed13Font);
  auto *undoAct = editMenu->addAction("&Undo");
  undoAct->setShortcut(QKeySequence::Undo);
  undoAct->setEnabled(false);
  editMenu->addSeparator();
  auto *cutAct = editMenu->addAction("Cu&t");
  cutAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
  auto *copyAct = editMenu->addAction("&Copy");
  copyAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
  auto *pasteAct = editMenu->addAction("&Paste");
  pasteAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
  editMenu->addSeparator();
  auto *raiseAct = editMenu->addAction("&Raise");
  auto *lowerAct = editMenu->addAction("&Lower");
  editMenu->addSeparator();
  auto *groupAct = editMenu->addAction("&Group");
  auto *ungroupAct = editMenu->addAction("&Ungroup");
  editMenu->addSeparator();
  auto *alignMenu = editMenu->addMenu("&Align");
  alignMenu->setFont(fixed13Font);
  auto *alignLeftAct = alignMenu->addAction("&Left");
  auto *alignHorizontalCenterAct = alignMenu->addAction("&Horizontal Center");
  auto *alignRightAct = alignMenu->addAction("&Right");
  auto *alignTopAct = alignMenu->addAction("&Top");
  auto *alignVerticalCenterAct = alignMenu->addAction("&Vertical Center");
  auto *alignBottomAct = alignMenu->addAction("&Bottom");
  auto *positionToGridAct = alignMenu->addAction("Position to &Grid");
  auto *edgesToGridAct = alignMenu->addAction("Ed&ges to Grid");

  auto *spaceMenu = editMenu->addMenu("Space &Evenly");
  spaceMenu->setFont(fixed13Font);
  auto *spaceHorizontalAct = spaceMenu->addAction("&Horizontal");
  auto *spaceVerticalAct = spaceMenu->addAction("&Vertical");
  auto *space2DAct = spaceMenu->addAction("&2-D");

  auto *centerMenu = editMenu->addMenu("&Center");
  centerMenu->setFont(fixed13Font);
  auto *centerHorizontalAct =
      centerMenu->addAction("&Horizontally in Display");
  auto *centerVerticalAct =
      centerMenu->addAction("&Vertically in Display");
  auto *centerBothAct = centerMenu->addAction("&Both");

  auto *orientMenu = editMenu->addMenu("&Orient");
  orientMenu->setFont(fixed13Font);
  auto *flipHorizontalAct = orientMenu->addAction("Flip &Horizontally");
  auto *flipVerticalAct = orientMenu->addAction("Flip &Vertically");
  auto *rotateClockwiseAct = orientMenu->addAction("Rotate &Clockwise");
  auto *rotateCounterclockwiseAct =
      orientMenu->addAction("Rotate &Counterclockwise");

  auto *sizeMenu = editMenu->addMenu("&Size");
  sizeMenu->setFont(fixed13Font);
  auto *sameSizeAct = sizeMenu->addAction("&Same Size");
  auto *textToContentsAct = sizeMenu->addAction("Text to &Contents");

  auto *gridMenu = editMenu->addMenu("&Grid");
  gridMenu->setFont(fixed13Font);
  auto *toggleGridAct = gridMenu->addAction("Toggle Show &Grid");
  auto *toggleSnapAct = gridMenu->addAction("Toggle &Snap To Grid");
  auto *gridSpacingAct = gridMenu->addAction("Grid &Spacing...");
  toggleGridAct->setCheckable(true);
  toggleSnapAct->setCheckable(true);

  editMenu->addSeparator();
  auto *unselectAct = editMenu->addAction("U&nselect");
  auto *selectAllAct = editMenu->addAction("Select &All");
  auto *selectDisplayAct = editMenu->addAction("Select &Display");
  editMenu->addSeparator();
  auto *findOutliersAct = editMenu->addAction("Find &Outliers");
  auto *refreshAct = editMenu->addAction("&Refresh");
  auto *editSummaryAct = editMenu->addAction("Edit &Summary...");

  editMenu->setEnabled(false);
  editMenu->menuAction()->setEnabled(false);

  auto *viewMenu = menuBar->addMenu("&View");
  viewMenu->setFont(fixed13Font);
  viewMenu->addAction("&Message Window");
  viewMenu->addAction("&Statistics Window");
  auto *viewDisplayListAct = viewMenu->addAction("&Display List");

  auto *palettesMenu = menuBar->addMenu("&Palettes");
  palettesMenu->setFont(fixed13Font);
  palettesMenu->addAction("&Object");
  palettesMenu->addAction("&Resource");
  palettesMenu->addAction("&Color");
  palettesMenu->setEnabled(false);
  palettesMenu->menuAction()->setEnabled(false);

  auto *helpMenu = menuBar->addMenu("&Help");
  helpMenu->setFont(fixed13Font);
  helpMenu->addAction("&Overview");
  helpMenu->addAction("&Contents");
  helpMenu->addAction("Object &Index");
  helpMenu->addAction("&Editing");
  helpMenu->addAction("&New Features");
  helpMenu->addAction("Technical &Support");
  helpMenu->addAction("On &Help");
  auto *onVersionAct = helpMenu->addAction("On &Version");
  QObject::connect(onVersionAct, &QAction::triggered, &win,
      [&win, &fixed13Font, &fixed10Font, &palette]() {
        showVersionDialog(&win, fixed13Font, fixed10Font, palette, false);
      });

  auto *central = new QWidget;
  central->setObjectName("mainBB");
  central->setAutoFillBackground(true);
  central->setPalette(palette);
  central->setBackgroundRole(QPalette::Window);

  auto *layout = new QVBoxLayout;
  layout->setContentsMargins(10, 8, 10, 10);
  layout->setSpacing(10);

  auto *modePanel = new QFrame;
  modePanel->setFrameShape(QFrame::Panel);
  modePanel->setFrameShadow(QFrame::Sunken);
  modePanel->setLineWidth(2);
  modePanel->setMidLineWidth(1);
  modePanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  modePanel->setAutoFillBackground(true);
  modePanel->setPalette(palette);
  modePanel->setBackgroundRole(QPalette::Button);

  auto *panelLayout = new QVBoxLayout(modePanel);
  panelLayout->setContentsMargins(12, 8, 12, 12);
  panelLayout->setSpacing(6);

  auto *modeBox = new QGroupBox("Mode");
  modeBox->setFont(fixed13Font);
  modeBox->setAutoFillBackground(true);
  modeBox->setPalette(palette);
  modeBox->setBackgroundRole(QPalette::Window);
  modeBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  modeBox->setStyleSheet(
      "QGroupBox { border: 2px groove palette(mid); margin-top: 0.8em;"
      " padding: 6px 12px 8px 12px; }"
      " QGroupBox::title { subcontrol-origin: margin; left: 10px;"
      " padding: 0 4px; }");

  auto *modeLayout = new QHBoxLayout;
  modeLayout->setContentsMargins(12, 8, 12, 8);
  modeLayout->setSpacing(14);
  auto *editModeButton = new QRadioButton("Edit");
  auto *executeModeButton = new QRadioButton("Execute");
  editModeButton->setFont(fixed13Font);
  executeModeButton->setFont(fixed13Font);
  editModeButton->setChecked(true);
  modeLayout->addWidget(editModeButton);
  modeLayout->addWidget(executeModeButton);
  modeBox->setLayout(modeLayout);

  auto state = std::make_shared<DisplayState>();
  auto *mainWindowController = new MainWindowController(&win,
      std::weak_ptr<DisplayState>(state));
  win.installEventFilter(mainWindowController);
  auto updateMenus = std::make_shared<std::function<void()>>();
  state->updateMenus = updateMenus;
  auto *displayListDialog = new DisplayListDialog(palette, fixed13Font,
      std::weak_ptr<DisplayState>(state), &win);

  QObject::connect(viewDisplayListAct, &QAction::triggered, displayListDialog,
      [displayListDialog]() {
        displayListDialog->showAndRaise();
      });

  QObject::connect(saveAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->save();
        }
      });
  QObject::connect(undoAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          if (auto *stack = active->undoStack()) {
            stack->undo();
          }
        }
      });
  QObject::connect(cutAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerCutFromMenu();
        }
      });
  QObject::connect(copyAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerCopyFromMenu();
        }
      });
  QObject::connect(pasteAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerPasteFromMenu();
        }
      });
  QObject::connect(raiseAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->raiseSelection();
        }
      });
  QObject::connect(lowerAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->lowerSelection();
        }
      });
  QObject::connect(groupAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerGroupFromMenu();
        }
      });
  QObject::connect(ungroupAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->triggerUngroupFromMenu();
        }
      });
  QObject::connect(alignLeftAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionLeft();
        }
      });
  QObject::connect(alignHorizontalCenterAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionHorizontalCenter();
        }
      });
  QObject::connect(alignRightAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionRight();
        }
      });
  QObject::connect(alignTopAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionTop();
        }
      });
  QObject::connect(alignVerticalCenterAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionVerticalCenter();
        }
      });
  QObject::connect(alignBottomAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionBottom();
        }
      });
  QObject::connect(positionToGridAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionPositionToGrid();
        }
      });
  QObject::connect(edgesToGridAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->alignSelectionEdgesToGrid();
        }
      });
  QObject::connect(spaceHorizontalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->spaceSelectionHorizontal();
        }
      });
  QObject::connect(spaceVerticalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->spaceSelectionVertical();
        }
      });
  QObject::connect(space2DAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->spaceSelection2D();
        }
      });
  QObject::connect(centerHorizontalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->centerSelectionHorizontallyInDisplay();
        }
      });
  QObject::connect(centerVerticalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->centerSelectionVerticallyInDisplay();
        }
      });
  QObject::connect(centerBothAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->centerSelectionInDisplayBoth();
        }
      });
  QObject::connect(flipHorizontalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->orientSelectionFlipHorizontal();
        }
      });
  QObject::connect(flipVerticalAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->orientSelectionFlipVertical();
        }
      });
  QObject::connect(rotateClockwiseAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->rotateSelectionClockwise();
        }
      });
  QObject::connect(rotateCounterclockwiseAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->rotateSelectionCounterclockwise();
        }
      });
  QObject::connect(sameSizeAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->sizeSelectionSameSize();
        }
      });
  QObject::connect(textToContentsAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->sizeSelectionTextToContents();
        }
      });
  QObject::connect(toggleGridAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->setGridOn(!active->isGridOn());
        }
      });
  QObject::connect(toggleSnapAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->setSnapToGrid(!active->isSnapToGridEnabled());
        }
      });
  QObject::connect(gridSpacingAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->promptForGridSpacing();
        }
      });
  QObject::connect(unselectAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->clearSelection();
        }
      });
  QObject::connect(selectAllAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->selectAllElements();
        }
      });
  QObject::connect(selectDisplayAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->selectDisplayElement();
        }
      });
  QObject::connect(findOutliersAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->findOutliers();
        }
      });
  QObject::connect(refreshAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->refreshDisplayView();
        }
      });
  QObject::connect(editSummaryAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->showEditSummaryDialog();
        }
      });
  QObject::connect(saveAsAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->saveAs();
        }
      });
  QObject::connect(closeAct, &QAction::triggered, &win,
      [state]() {
        if (auto active = state->activeDisplay.data()) {
          active->close();
        }
      });

  QPalette displayPalette = palette;
  const QColor displayBackgroundColor(0xbb, 0xbb, 0xbb);
  displayPalette.setColor(QPalette::Window, displayBackgroundColor);
  displayPalette.setColor(QPalette::Base, displayBackgroundColor);
  displayPalette.setColor(QPalette::AlternateBase, displayBackgroundColor);
  displayPalette.setColor(QPalette::Button, displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::Window,
      displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::Base,
      displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::AlternateBase,
      displayBackgroundColor);
  displayPalette.setColor(QPalette::Disabled, QPalette::Button,
      displayBackgroundColor);

  *updateMenus = [state, editMenu, palettesMenu, newAct, saveAct, saveAsAct,
    closeAct, undoAct, cutAct, copyAct, pasteAct, raiseAct, lowerAct, groupAct,
    ungroupAct, alignLeftAct, alignHorizontalCenterAct, alignRightAct,
    alignTopAct, alignVerticalCenterAct, alignBottomAct, positionToGridAct,
    edgesToGridAct, spaceHorizontalAct, spaceVerticalAct, space2DAct,
    centerHorizontalAct, centerVerticalAct, centerBothAct,
    flipHorizontalAct, flipVerticalAct, rotateClockwiseAct,
    rotateCounterclockwiseAct, sameSizeAct, textToContentsAct, toggleGridAct, toggleSnapAct,
    gridSpacingAct, unselectAct, selectAllAct, selectDisplayAct,
    findOutliersAct, refreshAct, editSummaryAct, displayListDialog]() {
    auto &displays = state->displays;
    for (auto it = displays.begin(); it != displays.end();) {
      if (it->isNull()) {
        it = displays.erase(it);
      } else {
        ++it;
      }
    }

    if (!state->activeDisplay.isNull()) {
      bool found = false;
      for (const auto &display : displays) {
        if (display == state->activeDisplay) {
          found = true;
          break;
        }
      }
      if (!found) {
        state->activeDisplay.clear();
      }
    }

    DisplayWindow *active = state->activeDisplay.data();
    if (!active) {
      for (auto it = displays.crbegin(); it != displays.crend(); ++it) {
        if (!it->isNull()) {
          active = it->data();
          state->activeDisplay = active;
          break;
        }
      }
    }

    const bool hasDisplay = !displays.isEmpty();
    const bool enableEditing = hasDisplay && state->editMode;
    const bool canEditActive = enableEditing && active;

    editMenu->setEnabled(enableEditing);
    editMenu->menuAction()->setEnabled(enableEditing);
    palettesMenu->setEnabled(enableEditing);
    palettesMenu->menuAction()->setEnabled(enableEditing);
    newAct->setEnabled(state->editMode);
    saveAct->setEnabled(canEditActive && active->isDirty());
    saveAsAct->setEnabled(canEditActive);
    closeAct->setEnabled(active);
    QString undoTextLabel = QStringLiteral("&Undo");
    bool enableUndo = false;
    if (canEditActive && active) {
      if (auto *stack = active->undoStack()) {
        if (stack->canUndo()) {
          enableUndo = true;
          const QString stackText = stack->undoText();
          if (!stackText.isEmpty()) {
            undoTextLabel = QStringLiteral("&Undo %1").arg(stackText);
          }
        }
      }
    }
    undoAct->setEnabled(enableUndo);
    undoAct->setText(undoTextLabel);
    const bool hasSelection = canEditActive && active
        && active->hasCopyableSelection();
    cutAct->setEnabled(hasSelection);
    copyAct->setEnabled(hasSelection);
    const bool canPaste = canEditActive && active && active->canPaste();
    pasteAct->setEnabled(canPaste);
    const bool canRaise = canEditActive && active && active->canRaiseSelection();
    const bool canLower = canEditActive && active && active->canLowerSelection();
    raiseAct->setEnabled(canRaise);
    lowerAct->setEnabled(canLower);
    const bool canGroup = canEditActive && active && active->canGroupSelection();
    toggleGridAct->setEnabled(canEditActive);
    toggleGridAct->setChecked(canEditActive && active && active->isGridOn());
    toggleSnapAct->setEnabled(canEditActive);
    toggleSnapAct->setChecked(canEditActive && active && active->isSnapToGridEnabled());
    gridSpacingAct->setEnabled(canEditActive);
    const bool canUngroup = canEditActive && active && active->canUngroupSelection();
    groupAct->setEnabled(canGroup);
    ungroupAct->setEnabled(canUngroup);
    const bool canAlign = canEditActive && active && active->canAlignSelection();
    alignLeftAct->setEnabled(canAlign);
    alignHorizontalCenterAct->setEnabled(canAlign);
    alignRightAct->setEnabled(canAlign);
    alignTopAct->setEnabled(canAlign);
    alignVerticalCenterAct->setEnabled(canAlign);
    alignBottomAct->setEnabled(canAlign);
    const bool canAlignToGrid = canEditActive && active
        && active->canAlignSelectionToGrid();
    positionToGridAct->setEnabled(canAlignToGrid);
    edgesToGridAct->setEnabled(canAlignToGrid);
    const bool canSpace = canEditActive && active
        && active->canSpaceSelection();
    const bool canSpace2D = canEditActive && active
        && active->canSpaceSelection2D();
    spaceHorizontalAct->setEnabled(canSpace);
    spaceVerticalAct->setEnabled(canSpace);
    space2DAct->setEnabled(canSpace2D);
    const bool canCenter = canEditActive && active
        && active->canCenterSelection();
    centerHorizontalAct->setEnabled(canCenter);
    centerVerticalAct->setEnabled(canCenter);
    centerBothAct->setEnabled(canCenter);
    const bool canOrient = canEditActive && active
        && active->canOrientSelection();
    flipHorizontalAct->setEnabled(canOrient);
    flipVerticalAct->setEnabled(canOrient);
    rotateClockwiseAct->setEnabled(canOrient);
    rotateCounterclockwiseAct->setEnabled(canOrient);
    const bool canSizeSame = canEditActive && active
        && active->canSizeSelectionSameSize();
    const bool canSizeContents = canEditActive && active
        && active->canSizeSelectionTextToContents();
    sameSizeAct->setEnabled(canSizeSame);
    textToContentsAct->setEnabled(canSizeContents);
    const bool canOperateSelection = canEditActive && active;
    unselectAct->setEnabled(canOperateSelection);
    selectAllAct->setEnabled(canOperateSelection);
    selectDisplayAct->setEnabled(canOperateSelection);
    findOutliersAct->setEnabled(canOperateSelection);
    refreshAct->setEnabled(canOperateSelection);
    editSummaryAct->setEnabled(canOperateSelection);

    if (displayListDialog) {
      displayListDialog->handleStateChanged();
    }
  };

  auto registerDisplayWindow = [state, updateMenus, &win](
      DisplayWindow *displayWin) {
    state->displays.append(displayWin);
    displayWin->syncCreateCursor();

    QObject::connect(displayWin, &QObject::destroyed, &win,
        [state, updateMenus,
            displayPtr = QPointer<DisplayWindow>(displayWin)]() {
          if (state) {
            if (state->activeDisplay == displayPtr) {
              state->activeDisplay.clear();
            }
            bool hasLiveDisplay = false;
            for (auto &display : state->displays) {
              if (!display.isNull()) {
                hasLiveDisplay = true;
                break;
              }
            }
            if (!hasLiveDisplay) {
              state->createTool = CreateTool::kNone;
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
  };

  QObject::connect(newAct, &QAction::triggered, &win,
      [state, displayPalette, &win, fixed10Font, &palette, fixed13Font,
          registerDisplayWindow]() {
        if (!state->editMode) {
          return;
        }

        auto *displayWin = new DisplayWindow(displayPalette, palette,
            fixed10Font, fixed13Font, std::weak_ptr<DisplayState>(state));
        registerDisplayWindow(displayWin);
      });

  QObject::connect(openAct, &QAction::triggered, &win,
      [state, &win, displayPalette, &palette, fixed10Font, fixed13Font,
          registerDisplayWindow]() {
        if (!state->editMode) {
          return;
        }

        static QString lastDirectory;
        QFileDialog dialog(&win, QStringLiteral("Open Display"));
        //dialog.setOption(QFileDialog::DontUseNativeDialog, true); // Disabled because it causes an unitialized value 
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilters({
            QStringLiteral("MEDM Display Files (*.adl)"),
            QStringLiteral("All Files (*)")});
        dialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);
        dialog.setModal(true);
        dialog.setWindowModality(Qt::ApplicationModal);
        if (!lastDirectory.isEmpty()) {
          dialog.setDirectory(lastDirectory);
        }

        if (dialog.exec() != QDialog::Accepted) {
          return;
        }

        const QString selected = dialog.selectedFiles().value(0);
        if (selected.isEmpty()) {
          return;
        }

        lastDirectory = QFileInfo(selected).absolutePath();

        QString errorMessage;
        auto *displayWin = new DisplayWindow(displayPalette, palette,
            fixed10Font, fixed13Font, std::weak_ptr<DisplayState>(state));
        if (!displayWin->loadFromFile(selected, &errorMessage)) {
          const QString message = errorMessage.isEmpty()
              ? QStringLiteral("Failed to open display:\n%1").arg(selected)
              : errorMessage;
          QMessageBox::critical(&win, QStringLiteral("Open Display"),
              message);
          delete displayWin;
          return;
        }

        registerDisplayWindow(displayWin);
      });

  QObject::connect(editModeButton, &QRadioButton::toggled, &win,
      [state, updateMenus](bool checked) {
        state->editMode = checked;
        if (!checked) {
          state->createTool = CreateTool::kNone;
          for (auto &display : state->displays) {
            if (!display.isNull()) {
              display->clearSelection();
              display->syncCreateCursor();
            }
          }
        } else {
          for (auto &display : state->displays) {
            if (!display.isNull()) {
              display->syncCreateCursor();
            }
          }
        }
        if (updateMenus && *updateMenus) {
          (*updateMenus)();
        }
      });

  if (updateMenus && *updateMenus) {
    (*updateMenus)();
  }

  panelLayout->addWidget(modeBox);

  layout->addWidget(modePanel, 0, Qt::AlignLeft);
  layout->addStretch();

  central->setLayout(layout);
  win.setCentralWidget(central);

  win.adjustSize();
  win.setFixedSize(win.sizeHint());
  win.show();
  positionWindowTopRight(&win, kMainWindowRightMargin, kMainWindowTopMargin);
  QTimer::singleShot(0, &win,
      [&, rightMargin = kMainWindowRightMargin,
          topMargin = kMainWindowTopMargin]() {
        positionWindowTopRight(&win, rightMargin, topMargin);
      });
  return app.exec();
}
