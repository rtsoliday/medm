#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <cstddef>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QDialog>
#include <QAbstractScrollArea>
#include <QList>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QEvent>
#include <QMetaObject>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QPaintEvent>
#include <QPen>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QKeySequence>
#include <QPoint>
#include <QPointF>
#include <QPolygon>
#include <QScreen>
#include <QSize>
#include <QSizePolicy>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QStyleFactory>
#include <QTimer>
#include <QLineEdit>
#include <QRubberBand>
#include <QVBoxLayout>
#include <QWidget>
#include <QVector>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>
#include <utility>
#include <array>
#include <vector>

#include "legacy_fonts.h"
#include "resource_palette_dialog.h"
#include "text_element.h"
#include "rectangle_element.h"
#include "oval_element.h"
#include "arc_element.h"
#include "line_element.h"
#include "polyline_element.h"
#include "polygon_element.h"
#include "color_palette_dialog.h"
#include "display_properties.h"
#include "resources/fonts/adobe_helvetica_24_otb.h"
#include "resources/fonts/adobe_helvetica_bold_24_otb.h"
#include "resources/fonts/adobe_times_18_otb.h"
#include "resources/fonts/misc_fixed_10_otb.h"
#include "resources/fonts/misc_fixed_10x20_otb.h"
#include "resources/fonts/misc_fixed_13_otb.h"
#include "resources/fonts/misc_fixed_7x13_otb.h"
#include "resources/fonts/misc_fixed_7x14_otb.h"
#include "resources/fonts/misc_fixed_8_otb.h"
#include "resources/fonts/misc_fixed_9_otb.h"
#include "resources/fonts/misc_fixed_9x15_otb.h"
#include "resources/fonts/sony_fixed_12x24_otb.h"
#include "resources/fonts/sony_fixed_8x16_otb.h"

namespace {

QFont loadEmbeddedFont(const unsigned char *data, std::size_t size,
    int pixelSize, QFont::StyleHint styleHint, bool fixedPitch,
    QFont::Weight weight)
{
    const int fontId = QFontDatabase::addApplicationFontFromData(QByteArray(
        reinterpret_cast<const char *>(data), static_cast<int>(size)));

    QFont font;
    if (fontId != -1) {
        const QStringList families = QFontDatabase::applicationFontFamilies(
            fontId);
        if (!families.isEmpty()) {
            font = QFont(families.first());
        }
    }

    if (font.family().isEmpty()) {
        const QFontDatabase::SystemFont fallback =
            styleHint == QFont::TypeWriter ? QFontDatabase::FixedFont
                                           : QFontDatabase::GeneralFont;
        font = QFontDatabase::systemFont(fallback);
    }

    font.setStyleHint(styleHint, QFont::PreferBitmap);
    font.setStyleStrategy(QFont::PreferBitmap);
    font.setFixedPitch(fixedPitch);
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    font.setBold(weight >= QFont::DemiBold);
    return font;
}

void centerWindowOnScreen(QWidget *window);

void showVersionDialog(QWidget *parent, const QFont &titleFont,
    const QFont &bodyFont, const QPalette &palette, bool autoClose = true)
{
    QDialog *dialog = parent ? parent->findChild<QDialog *>(
        QStringLiteral("qtedmVersionDialog"))
                             : nullptr;

    if (!dialog) {
        dialog = new QDialog(parent,
            Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
        dialog->setObjectName(QStringLiteral("qtedmVersionDialog"));
        dialog->setWindowTitle(QStringLiteral("Version"));
        dialog->setModal(false);
        dialog->setAutoFillBackground(true);
        dialog->setBackgroundRole(QPalette::Window);
        dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);

        auto *layout = new QHBoxLayout(dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(16);

        auto *nameFrame = new QFrame(dialog);
        nameFrame->setFrameShape(QFrame::Panel);
        nameFrame->setFrameShadow(QFrame::Raised);
        nameFrame->setLineWidth(2);
        nameFrame->setMidLineWidth(1);
        nameFrame->setAutoFillBackground(true);
        nameFrame->setBackgroundRole(QPalette::Button);
        nameFrame->setPalette(palette);

        auto *nameLayout = new QVBoxLayout(nameFrame);
        nameLayout->setContentsMargins(12, 8, 12, 8);
        nameLayout->setSpacing(0);

        auto *nameLabel = new QLabel(QStringLiteral("QtEDM"), nameFrame);
        QFont nameFont = titleFont;
        nameFont.setPixelSize(nameFont.pixelSize() + 4);
        nameLabel->setFont(nameFont);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLayout->addStretch(1);
        nameLayout->addWidget(nameLabel, 0, Qt::AlignCenter);
        nameLayout->addStretch(1);

        layout->addWidget(nameFrame, 0, Qt::AlignTop);

        auto *infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(8);

        auto *descriptionLabel = new QLabel(
            QStringLiteral("Qt-Based Editor & Display Manager"), dialog);
        descriptionLabel->setFont(titleFont);
        descriptionLabel->setAlignment(Qt::AlignLeft);
        infoLayout->addWidget(descriptionLabel);

        auto *versionLabel = new QLabel(
            QStringLiteral("QtEDM Version 1.0.0  (EPICS 7.0.9.1-DEV)"), dialog);
        versionLabel->setFont(titleFont);
        versionLabel->setAlignment(Qt::AlignLeft);
        infoLayout->addWidget(versionLabel);

        auto *developedLabel = new QLabel(
            QStringLiteral(
                "Developed at Argonne National Laboratory\n"
                "by Robert Soliday"),
            dialog);
        developedLabel->setFont(bodyFont);
        developedLabel->setAlignment(Qt::AlignLeft);
        developedLabel->setWordWrap(false);
        infoLayout->addWidget(developedLabel);

        infoLayout->addStretch(1);

        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch(1);
        auto *okButton = new QPushButton(QStringLiteral("OK"), dialog);
        okButton->setFont(titleFont);
        okButton->setAutoDefault(false);
        okButton->setDefault(false);
        buttonLayout->addWidget(okButton);
        infoLayout->addLayout(buttonLayout);

        layout->addLayout(infoLayout);

        QObject::connect(okButton, &QPushButton::clicked, dialog,
            &QDialog::accept);

        dialog->adjustSize();
        dialog->setFixedSize(dialog->sizeHint());
    }

    dialog->setPalette(palette);
  dialog->adjustSize();
  dialog->setFixedSize(dialog->sizeHint());
  centerWindowOnScreen(dialog);

    if (autoClose) {
        QTimer::singleShot(5000, dialog, &QDialog::accept);
    }

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void positionWindowTopRight(QWidget *window, int rightMargin, int topMargin)
{
  if (!window) {
    return;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect screenGeometry = screen->availableGeometry();
  QSize frameSize = window->frameGeometry().size();
  if (frameSize.isEmpty()) {
    frameSize = window->size();
  }

  const int xOffset = std::max(0, screenGeometry.width() - frameSize.width() - rightMargin);
  const int yOffset = std::max(0, topMargin);
  const int x = screenGeometry.x() + xOffset;
  const int y = screenGeometry.y() + yOffset;

  window->move(x, y);
}

void centerWindowOnScreen(QWidget *window)
{
  if (!window) {
    return;
  }

  QScreen *screen = window->screen();
  if (!screen) {
    if (QWidget *parent = window->parentWidget()) {
      screen = parent->screen();
    }
  }
  if (!screen) {
    screen = QGuiApplication::screenAt(QCursor::pos());
  }
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect screenGeometry = screen->availableGeometry();
  QSize targetSize = window->size();
  if (targetSize.isEmpty()) {
    targetSize = window->sizeHint();
  }

  const int x = screenGeometry.x()
      + std::max(0, (screenGeometry.width() - targetSize.width()) / 2);
  const int y = screenGeometry.y()
      + std::max(0, (screenGeometry.height() - targetSize.height()) / 2);

  window->move(x, y);
}

enum class CreateTool {
  kNone,
  kText,
  kRectangle,
  kOval,
  kArc,
  kPolygon,
  kPolyline,
  kLine,
};

class DisplayWindow;

struct DisplayState {
  bool editMode = true;
  QList<QPointer<DisplayWindow>> displays;
  CreateTool createTool = CreateTool::kNone;
};

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
  }

  void syncCreateCursor()
  {
    updateCreateCursor();
  }

  void clearSelection()
  {
    clearSelections();
  }

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
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
            || state->createTool == CreateTool::kRectangle
            || state->createTool == CreateTool::kOval
            || state->createTool == CreateTool::kArc
            || state->createTool == CreateTool::kLine) {
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
          if (auto *rectangle = dynamic_cast<RectangleElement *>(widget)) {
            selectRectangleElement(rectangle);
            showResourcePaletteForRectangle(rectangle);
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
  std::weak_ptr<DisplayState> state_;
  QFont labelFont_;
  QPalette resourcePaletteBase_;
  QPointer<ResourcePaletteDialog> resourcePalette_;
  DisplayAreaWidget *displayArea_ = nullptr;
  bool displaySelected_ = false;
  bool gridOn_ = kDefaultGridOn;
  int gridSpacing_ = kDefaultGridSpacing;
  QPoint lastContextMenuGlobalPos_;
  QList<TextElement *> textElements_;
  TextElement *selectedTextElement_ = nullptr;
  QList<RectangleElement *> rectangleElements_;
  RectangleElement *selectedRectangle_ = nullptr;
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

  void clearRectangleSelection()
  {
    if (!selectedRectangle_) {
      return;
    }
    selectedRectangle_->setSelected(false);
    selectedRectangle_ = nullptr;
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
    clearRectangleSelection();
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
    clearRectangleSelection();
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
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
        },
        [element]() {
          return element->text();
        },
        [element](const QString &text) {
          element->setText(text.isEmpty() ? QStringLiteral(" ") : text);
        },
        [element]() {
          return element->foregroundColor();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->textAlignment();
        },
        [element](Qt::Alignment alignment) {
          element->setTextAlignment(alignment);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
        },
        std::move(channelGetters), std::move(channelSetters));
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
    }};
    dialog->showForRectangle(
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
        },
        [element]() {
          return element->color();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->fill();
        },
        [element](RectangleFill fill) {
          element->setFill(fill);
        },
        [element]() {
          return element->lineStyle();
        },
        [element](RectangleLineStyle style) {
          element->setLineStyle(style);
        },
        [element]() {
          return element->lineWidth();
        },
        [element](int width) {
          element->setLineWidth(width);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
    }};
    dialog->showForRectangle(
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
        },
        [element]() {
          return element->color();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->fill();
        },
        [element](RectangleFill fill) {
          element->setFill(fill);
        },
        [element]() {
          return element->lineStyle();
        },
        [element](RectangleLineStyle style) {
          element->setLineStyle(style);
        },
        [element]() {
          return element->lineWidth();
        },
        [element](int width) {
          element->setLineWidth(width);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
    }};
    dialog->showForRectangle(
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
        },
        [element]() {
          return element->color();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->fill();
        },
        [element](RectangleFill fill) {
          element->setFill(fill);
        },
        [element]() {
          return element->lineStyle();
        },
        [element](RectangleLineStyle style) {
          element->setLineStyle(style);
        },
        [element]() {
          return element->lineWidth();
        },
        [element](int width) {
          element->setLineWidth(width);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
        },
        std::move(channelGetters), std::move(channelSetters),
        QStringLiteral("Arc"), false,
        [element]() {
          return element->beginAngle();
        },
        [element](int angle) {
          element->setBeginAngle(angle);
        },
        [element]() {
          return element->pathAngle();
        },
        [element](int angle) {
          element->setPathAngle(angle);
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
    }};
    dialog->showForLine(
        [element]() {
          return element->geometry();
        },
        [this, element](const QRect &newGeometry) {
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
        },
        [element]() {
          return element->color();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->lineStyle();
        },
        [element](RectangleLineStyle style) {
          element->setLineStyle(style);
        },
        [element]() {
          return element->lineWidth();
        },
        [element](int width) {
          element->setLineWidth(width);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
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
        },
        [element]() {
          return element->color();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->lineStyle();
        },
        [element](RectangleLineStyle style) {
          element->setLineStyle(style);
        },
        [element]() {
          return element->lineWidth();
        },
        [element](int width) {
          element->setLineWidth(width);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
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
        [element](const QString &value) { element->setChannel(0, value); },
        [element](const QString &value) { element->setChannel(1, value); },
        [element](const QString &value) { element->setChannel(2, value); },
        [element](const QString &value) { element->setChannel(3, value); },
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
        },
        [element]() {
          return element->color();
        },
        [element](const QColor &color) {
          element->setForegroundColor(color);
        },
        [element]() {
          return element->fill();
        },
        [element](RectangleFill fill) {
          element->setFill(fill);
        },
        [element]() {
          return element->lineStyle();
        },
        [element](RectangleLineStyle style) {
          element->setLineStyle(style);
        },
        [element]() {
          return element->lineWidth();
        },
        [element](int width) {
          element->setLineWidth(width);
        },
        [element]() {
          return element->colorMode();
        },
        [element](TextColorMode mode) {
          element->setColorMode(mode);
        },
        [element]() {
          return element->visibilityMode();
        },
        [element](TextVisibilityMode mode) {
          element->setVisibilityMode(mode);
        },
        [element]() {
          return element->visibilityCalc();
        },
        [element](const QString &calc) {
          element->setVisibilityCalc(calc);
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
    clearRectangleSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolygonSelection();
    selectedTextElement_ = element;
    selectedTextElement_->setSelected(true);
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
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolygonSelection();
    selectedRectangle_ = element;
    selectedRectangle_->setSelected(true);
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
    clearRectangleSelection();
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
    clearRectangleSelection();
    clearOvalSelection();
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
    clearRectangleSelection();
    clearOvalSelection();
    clearArcSelection();
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
    clearRectangleSelection();
    clearOvalSelection();
    clearArcSelection();
    clearLineSelection();
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
    clearRectangleSelection();
    clearArcSelection();
    clearLineSelection();
    clearPolylineSelection();
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
            || state->createTool == CreateTool::kRectangle
            || state->createTool == CreateTool::kOval
            || state->createTool == CreateTool::kArc
            || state->createTool == CreateTool::kPolygon
            || state->createTool == CreateTool::kPolyline
            || state->createTool == CreateTool::kLine);
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
    addMenuAction(graphicsMenu, QStringLiteral("Image"));

    auto *monitorsMenu = objectMenu->addMenu(QStringLiteral("Monitors"));
    addMenuAction(monitorsMenu, QStringLiteral("Text Monitor"));
    addMenuAction(monitorsMenu, QStringLiteral("Meter"));
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

class MainWindowController : public QObject
{
public:
  MainWindowController(QMainWindow *mainWindow,
      std::weak_ptr<DisplayState> state)
    : QObject(mainWindow)
    , mainWindow_(mainWindow)
    , state_(std::move(state))
  {
    if (QCoreApplication *core = QCoreApplication::instance()) {
      QObject::connect(core, &QCoreApplication::aboutToQuit, this,
          [this]() { closeAllDisplays(); });
    }
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == mainWindow_ && event->type() == QEvent::Close) {
      closeAllDisplays();
    }
    return QObject::eventFilter(watched, event);
  }

private:
  void closeAllDisplays()
  {
    if (closing_) {
      return;
    }
    closing_ = true;
    if (auto state = state_.lock()) {
      const auto displays = state->displays;
      for (const auto &display : displays) {
        if (!display.isNull()) {
          display->close();
        }
      }
      state->createTool = CreateTool::kNone;
    }
    closing_ = false;
  }

  QPointer<QMainWindow> mainWindow_;
  std::weak_ptr<DisplayState> state_;
  bool closing_ = false;
};

} // namespace

namespace LegacyFonts {

const QHash<QString, QFont> &all()
{
    static const QHash<QString, QFont> fonts = [] {
        struct FontSpec {
            const char *key;
            const unsigned char *data;
            std::size_t size;
            int pixelSize;
            QFont::StyleHint styleHint;
            bool fixedPitch;
            QFont::Weight weight;
        };

        const FontSpec fontSpecs[] = {
            {"miscFixed8", kMiscFixed8FontData, kMiscFixed8FontSize, 8,
                QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed9", kMiscFixed9FontData, kMiscFixed9FontSize, 9,
                QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed10", kMiscFixed10FontData, kMiscFixed10FontSize, 10,
                QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed13", kMiscFixed13FontData, kMiscFixed13FontSize, 13,
                QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed7x13", kMiscFixed7x13FontData, kMiscFixed7x13FontSize,
                13, QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed7x14", kMiscFixed7x14FontData, kMiscFixed7x14FontSize,
                14, QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed9x15", kMiscFixed9x15FontData, kMiscFixed9x15FontSize,
                15, QFont::TypeWriter, true, QFont::Normal},
            {"sonyFixed8x16", kSonyFixed8x16FontData, kSonyFixed8x16FontSize,
                16, QFont::TypeWriter, true, QFont::Normal},
            {"miscFixed10x20", kMiscFixed10x20FontData,
                kMiscFixed10x20FontSize, 20, QFont::TypeWriter, true,
                QFont::Normal},
            {"sonyFixed12x24", kSonyFixed12x24FontData,
                kSonyFixed12x24FontSize, 24, QFont::TypeWriter, true,
                QFont::Normal},
            {"adobeTimes18", kAdobeTimes18FontData, kAdobeTimes18FontSize, 25,
                QFont::Serif, false, QFont::Normal},
            {"adobeHelvetica24", kAdobeHelvetica24FontData,
                kAdobeHelvetica24FontSize, 34, QFont::SansSerif, false,
                QFont::Normal},
            {"adobeHelveticaBold24", kAdobeHelveticaBold24FontData,
                kAdobeHelveticaBold24FontSize, 34, QFont::SansSerif, false,
                QFont::Bold},
        };

        QHash<QString, QFont> fonts;
        for (const FontSpec &spec : fontSpecs) {
            fonts.insert(QString::fromLatin1(spec.key), loadEmbeddedFont(
                spec.data, spec.size, spec.pixelSize, spec.styleHint,
                spec.fixedPitch, spec.weight));
        }

        struct FontAlias {
            const char *alias;
            const char *key;
        };

        const FontAlias fontAliases[] = {
            {"widgetDM_4", "miscFixed8"},
            {"widgetDM_6", "miscFixed8"},
            {"widgetDM_8", "miscFixed9"},
            {"widgetDM_10", "miscFixed10"},
            {"widgetDM_12", "miscFixed7x13"},
            {"widgetDM_14", "miscFixed7x14"},
            {"widgetDM_16", "miscFixed9x15"},
            {"widgetDM_18", "sonyFixed8x16"},
            {"widgetDM_20", "miscFixed10x20"},
            {"widgetDM_22", "sonyFixed12x24"},
            {"widgetDM_24", "sonyFixed12x24"},
            {"widgetDM_30", "adobeTimes18"},
            {"widgetDM_36", "adobeHelvetica24"},
            {"widgetDM_40", "adobeHelveticaBold24"},
            {"widgetDM_48", "adobeHelveticaBold24"},
            {"widgetDM_60", "adobeHelveticaBold24"},
        };

        for (const FontAlias &alias : fontAliases) {
            const QString key = QString::fromLatin1(alias.key);
            const QFont font = fonts.value(key);
            if (!font.family().isEmpty()) {
                fonts.insert(QString::fromLatin1(alias.alias), font);
            }
        }

        return fonts;
    }();

    return fonts;
}

QFont font(const QString &key)
{
    return all().value(key);
}

QFont fontOrDefault(const QString &key, const QFont &fallback)
{
    const QHash<QString, QFont> &fonts = all();
    if (fonts.contains(key)) {
        return fonts.value(key);
    }
    return fallback;
}

} // namespace LegacyFonts

// Entry point
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

    // Load the packaged bitmap fonts so every widget matches the legacy MEDM
    // appearance.  Fall back to the system fixed font if the embedded data
    // cannot be registered for some reason.
    const QFont fixed10Font = LegacyFonts::fontOrDefault(
        QStringLiteral("widgetDM_10"),
        QFontDatabase::systemFont(QFontDatabase::FixedFont));
    app.setFont(fixed10Font);

    const QFont fixed13Font = LegacyFonts::fontOrDefault(
        QStringLiteral("miscFixed13"), fixed10Font);

    QMainWindow win;
    win.setObjectName("QtedmMainWindow");
    win.setWindowTitle("QtEDM");

    // Match the teal Motif background used by the legacy MEDM main window.
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
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
        disabledTextColor);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
        disabledTextColor);
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
    auto *saveAllAct = fileMenu->addAction("Save &All");
    saveAllAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
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
    saveAllAct->setEnabled(false);
    saveAsAct->setEnabled(false);
    closeAct->setEnabled(false);
    QObject::connect(closeAct, &QAction::triggered, &win, &QWidget::close);

    auto *editMenu = menuBar->addMenu("&Edit");
    editMenu->setFont(fixed13Font);
    editMenu->addAction("&Undo");
    editMenu->addSeparator();
    editMenu->addAction("Cu&t");
    editMenu->addAction("&Copy");
    editMenu->addAction("&Paste");
    editMenu->addSeparator();
    editMenu->addAction("&Raise");
    editMenu->addAction("&Lower");
    editMenu->addSeparator();
    editMenu->addAction("&Group");
    editMenu->addAction("&Ungroup");
    editMenu->addSeparator();
    auto *alignMenu = editMenu->addMenu("&Align");
    alignMenu->setFont(fixed13Font);
    alignMenu->addAction("&Left");
    alignMenu->addAction("&Horizontal Center");
    alignMenu->addAction("&Right");
    alignMenu->addAction("&Top");
    alignMenu->addAction("&Vertical Center");
    alignMenu->addAction("&Bottom");
    alignMenu->addAction("Position to &Grid");
    alignMenu->addAction("Ed&ges to Grid");

    auto *spaceMenu = editMenu->addMenu("Space &Evenly");
    spaceMenu->setFont(fixed13Font);
    spaceMenu->addAction("&Horizontal");
    spaceMenu->addAction("&Vertical");
    spaceMenu->addAction("&2-D");

    auto *centerMenu = editMenu->addMenu("&Center");
    centerMenu->setFont(fixed13Font);
    centerMenu->addAction("&Horizontally in Display");
    centerMenu->addAction("&Vertically in Display");
    centerMenu->addAction("&Both");

    auto *orientMenu = editMenu->addMenu("&Orient");
    orientMenu->setFont(fixed13Font);
    orientMenu->addAction("Flip &Horizontally");
    orientMenu->addAction("Flip &Vertically");
    orientMenu->addAction("Rotate &Clockwise");
    orientMenu->addAction("Rotate &Counterclockwise");

    auto *sizeMenu = editMenu->addMenu("&Size");
    sizeMenu->setFont(fixed13Font);
    sizeMenu->addAction("&Same Size");
    sizeMenu->addAction("Text to &Contents");

    auto *gridMenu = editMenu->addMenu("&Grid");
    gridMenu->setFont(fixed13Font);
    gridMenu->addAction("Toggle Show &Grid");
    gridMenu->addAction("Toggle &Snap To Grid");
    gridMenu->addAction("Grid &Spacing...");

    editMenu->addSeparator();
    editMenu->addAction("U&nselect");
    editMenu->addAction("Select &All");
    editMenu->addAction("Select &Display");
    editMenu->addSeparator();
    editMenu->addAction("Find &Outliers");
    editMenu->addAction("&Refresh");
    editMenu->addAction("Edit &Summary...");

    editMenu->setEnabled(false);
    editMenu->menuAction()->setEnabled(false);

    auto *viewMenu = menuBar->addMenu("&View");
    viewMenu->setFont(fixed13Font);
    viewMenu->addAction("&Message Window");
    viewMenu->addAction("&Statistics Window");
    viewMenu->addAction("&Display List");

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

    QPalette displayPalette = palette;
    // Match MEDM default display background (colormap index 4).
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

    *updateMenus = [state, editMenu, palettesMenu, newAct]() {
      auto &displays = state->displays;
      for (auto it = displays.begin(); it != displays.end();) {
        if (it->isNull()) {
          it = displays.erase(it);
        } else {
          ++it;
        }
      }

      const bool hasDisplay = !displays.isEmpty();
      const bool enableEditing = hasDisplay && state->editMode;

      editMenu->setEnabled(enableEditing);
      editMenu->menuAction()->setEnabled(enableEditing);
      palettesMenu->setEnabled(enableEditing);
      palettesMenu->menuAction()->setEnabled(enableEditing);
      newAct->setEnabled(state->editMode);
    };

    QObject::connect(newAct, &QAction::triggered, &win,
        [state, displayPalette, updateMenus, &win, fixed10Font, &palette,
            fixed13Font]() {
          if (!state->editMode) {
            return;
          }

          auto *displayWin = new DisplayWindow(displayPalette, palette,
              fixed10Font, fixed13Font, std::weak_ptr<DisplayState>(state));
          state->displays.append(displayWin);
          displayWin->syncCreateCursor();

          QObject::connect(displayWin, &QObject::destroyed, &win,
              [state, updateMenus]() {
                if (state) {
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

    showVersionDialog(&win, fixed13Font, fixed10Font, palette);

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
