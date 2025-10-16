#include "object_palette_dialog.h"

#include <QAction>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QSignalBlocker>

#include <algorithm>
#include <utility>

#include "../medm/medmPix25.xpm"
#include "display_window.h"

namespace {

QPixmap createPixmap(const unsigned char *bits, int width, int height)
{
  if (!bits || width <= 0 || height <= 0) {
    return QPixmap();
  }

  const int bytesPerRow = (width + 7) / 8;
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  for (int y = 0; y < height; ++y) {
    const unsigned char *row = bits + y * bytesPerRow;
    for (int x = 0; x < width; ++x) {
      const int byteIndex = x / 8;
      const int bitIndex = x % 8;
      const bool bitSet = row[byteIndex] & (1 << bitIndex);
      if (bitSet) {
        image.setPixelColor(x, y, Qt::black);
      }
    }
  }

  return QPixmap::fromImage(image);
}

}  // namespace

ObjectPaletteDialog::ObjectPaletteDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &buttonFont,
    std::weak_ptr<DisplayState> state, QWidget *parent)
  : QDialog(parent)
  , basePalette_(basePalette)
  , labelFont_(labelFont)
  , buttonFont_(buttonFont)
  , state_(std::move(state))
{
  setObjectName(QStringLiteral("qtedmObjectPalette"));
  setWindowTitle(QStringLiteral("Object Palette"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette_);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, false);
  setSizeGripEnabled(true);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  auto *menuBar = new QMenuBar;
  menuBar->setAutoFillBackground(true);
  menuBar->setPalette(basePalette_);
  menuBar->setFont(labelFont_);

  auto *fileMenu = menuBar->addMenu(QStringLiteral("&File"));
  fileMenu->setFont(labelFont_);
  auto *closeAction = fileMenu->addAction(QStringLiteral("&Close"));
  QObject::connect(closeAction, &QAction::triggered, this, &QDialog::close);

  auto *helpMenu = menuBar->addMenu(QStringLiteral("&Help"));
  helpMenu->setFont(labelFont_);
  auto *helpAction = helpMenu->addAction(QStringLiteral("On &Object Palette"));
  QObject::connect(helpAction, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Select an object creation tool."));
  });
  auto *indexAction = helpMenu->addAction(QStringLiteral("Object &Index"));
  QObject::connect(indexAction, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Displays a list of available MEDM objects."));
  });

  mainLayout->setMenuBar(menuBar);

  auto *contentFrame = new QFrame;
  contentFrame->setFrameShape(QFrame::Panel);
  contentFrame->setFrameShadow(QFrame::Sunken);
  contentFrame->setLineWidth(2);
  contentFrame->setMidLineWidth(1);
  contentFrame->setAutoFillBackground(true);
  contentFrame->setPalette(basePalette_);

  auto *contentLayout = new QVBoxLayout(contentFrame);
  contentLayout->setContentsMargins(6, 6, 6, 6);
  contentLayout->setSpacing(8);

  buttonGroup_ = new QButtonGroup(this);
  buttonGroup_->setExclusive(true);
  QObject::connect(buttonGroup_, &QButtonGroup::idToggled, this,
      [this](int id, bool checked) { handleButtonToggled(id, checked); });

  contentLayout->addWidget(createCategory(QStringLiteral("Graphics"),
      graphicsButtons()));
  contentLayout->addWidget(createCategory(QStringLiteral("Monitor"),
      monitorButtons()));
  contentLayout->addWidget(createCategory(QStringLiteral("Controller"),
      controlButtons()));
  contentLayout->addWidget(createCategory(QStringLiteral("Misc"),
    miscButtons()));

  mainLayout->addWidget(contentFrame);

  auto *messageFrame = new QFrame;
  messageFrame->setFrameShape(QFrame::Panel);
  messageFrame->setFrameShadow(QFrame::Sunken);
  messageFrame->setLineWidth(2);
  messageFrame->setMidLineWidth(1);
  messageFrame->setAutoFillBackground(true);
  messageFrame->setPalette(basePalette_);

  auto *messageLayout = new QHBoxLayout(messageFrame);
  messageLayout->setContentsMargins(8, 4, 8, 4);
  messageLayout->setSpacing(6);

  statusLabel_ = new QLabel(QStringLiteral("Select"));
  statusLabel_->setFont(labelFont_);
  statusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  statusLabel_->setAutoFillBackground(false);
  messageLayout->addWidget(statusLabel_);
  messageLayout->addStretch(1);

  mainLayout->addWidget(messageFrame);

  syncButtonsToState();

  adjustSize();
  setMinimumWidth(sizeHint().width());
}

void ObjectPaletteDialog::showAndRaise()
{
  show();
  raise();
  activateWindow();
}

QWidget *ObjectPaletteDialog::createCategory(const QString &title,
    const std::vector<ButtonDefinition> &buttons)
{
  auto *container = new QWidget;
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  auto *label = new QLabel(title);
  label->setFont(labelFont_);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setAutoFillBackground(false);
  layout->addWidget(label);

  auto *gridWidget = new QWidget;
  gridWidget->setAutoFillBackground(true);
  gridWidget->setPalette(basePalette_);

  auto *gridLayout = new QGridLayout(gridWidget);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setHorizontalSpacing(6);
  gridLayout->setVerticalSpacing(6);

  const int columns = 4;
  for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
    QToolButton *button = createToolButton(buttons[i]);
    gridLayout->addWidget(button, i / columns, i % columns, Qt::AlignLeft);
  }

  layout->addWidget(gridWidget);
  return container;
}

QToolButton *ObjectPaletteDialog::createToolButton(
    const ButtonDefinition &definition)
{
  auto *button = new QToolButton;
  button->setCheckable(true);
  button->setAutoExclusive(false);
  button->setAutoFillBackground(true);
  button->setPalette(basePalette_);
  button->setFont(buttonFont_);
  button->setFocusPolicy(Qt::NoFocus);
  button->setToolTip(definition.label);
  button->setIcon(QIcon(createPixmap(definition.bits,
      definition.width, definition.height)));
  button->setIconSize(QSize(definition.width, definition.height));
  button->setFixedSize(definition.width + 8, definition.height + 8);

  const int id = nextButtonId_++;
  buttonGroup_->addButton(button, id);
  buttonDescriptions_.insert(id, definition.label);
  buttonTools_.insert(id, definition.tool);
  button->installEventFilter(this);

  if (definition.label.compare(QStringLiteral("Select"),
      Qt::CaseInsensitive) == 0) {
    selectButton_ = button;
  }

  return button;
}

bool ObjectPaletteDialog::eventFilter(QObject *watched, QEvent *event)
{
  auto *button = qobject_cast<QAbstractButton *>(watched);
  if (!button || buttonGroup_ == nullptr) {
    return QDialog::eventFilter(watched, event);
  }

  const int id = buttonGroup_->id(button);
  if (id < 0) {
    return QDialog::eventFilter(watched, event);
  }
  switch (event->type()) {
  case QEvent::Enter:
  case QEvent::HoverEnter: {
    const QString description = buttonDescriptions_.value(id);
    if (!description.isEmpty()) {
      updateStatusLabel(description);
      QToolTip::showText(QCursor::pos(), description, button);
    }
    break;
  }
  case QEvent::Leave:
  case QEvent::HoverLeave: {
    const int checkedId = buttonGroup_->checkedId();
    if (checkedId >= 0) {
      updateStatusLabel(buttonDescriptions_.value(checkedId));
    } else {
      updateStatusLabel(QString());
    }
    QToolTip::hideText();
    break;
  }
  default:
    break;
  }

  return QDialog::eventFilter(watched, event);
}

void ObjectPaletteDialog::handleButtonToggled(int id, bool checked)
{
  if (!checked) {
    return;
  }
  applyCreateToolSelection(id);
}

void ObjectPaletteDialog::refreshSelectionFromState()
{
  syncButtonsToState();
}

void ObjectPaletteDialog::updateStatusLabel(const QString &description)
{
  if (!statusLabel_) {
    return;
  }

  if (description.isEmpty()) {
    statusLabel_->setText(QStringLiteral("Select"));
  } else {
    statusLabel_->setText(description);
  }
}

void ObjectPaletteDialog::applyCreateToolSelection(int id)
{
  updateStatusLabel(buttonDescriptions_.value(id));
  const auto tool = buttonTools_.value(id, CreateTool::kNone);
  if (auto state = state_.lock()) {
    bool stateChanged = false;
    bool handledByDisplay = false;
    if (tool == CreateTool::kNone) {
      for (auto &display : state->displays) {
        if (!display.isNull()) {
          display->setCreateTool(CreateTool::kNone);
          handledByDisplay = true;
          break;
        }
      }
      if (!handledByDisplay && state->createTool != CreateTool::kNone) {
        state->createTool = CreateTool::kNone;
        stateChanged = true;
      }
    } else {
      DisplayWindow *target = state->activeDisplay.data();
      if (!target) {
        for (auto &display : state->displays) {
          if (!display.isNull()) {
            target = display.data();
            break;
          }
        }
      }
      if (target) {
        target->setCreateTool(tool);
        handledByDisplay = true;
      } else {
        if (state->createTool != tool) {
          state->createTool = tool;
          stateChanged = true;
        }
      }
    }
    if (!handledByDisplay && stateChanged
        && state->updateMenus && *state->updateMenus) {
      (*state->updateMenus)();
    }
  }
}

void ObjectPaletteDialog::syncButtonsToState()
{
  if (!buttonGroup_) {
    return;
  }
  QSignalBlocker blocker(buttonGroup_);
  if (auto state = state_.lock()) {
    const CreateTool currentTool = state->createTool;
    int selectId = -1;
    for (auto button : buttonGroup_->buttons()) {
      const int buttonId = buttonGroup_->id(button);
      const CreateTool toolForButton = buttonTools_.value(buttonId,
          CreateTool::kNone);
      if (toolForButton == currentTool) {
        button->setChecked(true);
        updateStatusLabel(buttonDescriptions_.value(buttonId));
        return;
      }
      if (button == selectButton_) {
        selectId = buttonId;
      }
    }
    if (selectId >= 0) {
      if (auto *button = buttonGroup_->button(selectId)) {
        button->setChecked(true);
        updateStatusLabel(buttonDescriptions_.value(selectId));
      }
    }
  } else if (selectButton_) {
    selectButton_->setChecked(true);
    const int id = buttonGroup_->id(selectButton_);
    if (id >= 0) {
      updateStatusLabel(buttonDescriptions_.value(id));
    }
  }
}

std::vector<ObjectPaletteDialog::ButtonDefinition>
ObjectPaletteDialog::graphicsButtons()
{
  return {
      {QStringLiteral("Rectangle"), rectangle25_bits, rectangle25_width,
          rectangle25_height, CreateTool::kRectangle},
      {QStringLiteral("Oval"), oval25_bits, oval25_width, oval25_height,
          CreateTool::kOval},
      {QStringLiteral("Arc"), arc25_bits, arc25_width, arc25_height,
          CreateTool::kArc},
      {QStringLiteral("Text"), text25_bits, text25_width, text25_height,
          CreateTool::kText},
      {QStringLiteral("Polyline"), polyline25_bits, polyline25_width,
          polyline25_height, CreateTool::kPolyline},
      {QStringLiteral("Line"), line25_bits, line25_width, line25_height,
          CreateTool::kLine},
      {QStringLiteral("Polygon"), polygon25_bits, polygon25_width,
          polygon25_height, CreateTool::kPolygon},
      {QStringLiteral("Image"), image25_bits, image25_width,
          image25_height, CreateTool::kImage},
  };
}

std::vector<ObjectPaletteDialog::ButtonDefinition>
ObjectPaletteDialog::monitorButtons()
{
  return {
      {QStringLiteral("Meter"), meter25_bits, meter25_width, meter25_height,
          CreateTool::kMeter},
      {QStringLiteral("Bar Monitor"), bar25_bits, bar25_width,
          bar25_height, CreateTool::kBarMonitor},
      {QStringLiteral("Strip Chart"), stripChart25_bits, stripChart25_width,
          stripChart25_height, CreateTool::kStripChart},
      {QStringLiteral("Text Monitor"), textUpdate25_bits,
          textUpdate25_width, textUpdate25_height, CreateTool::kTextMonitor},
      {QStringLiteral("Scale Monitor"), indicator25_bits,
          indicator25_width, indicator25_height, CreateTool::kScaleMonitor},
      {QStringLiteral("Cartesian Plot"), cartesianPlot25_bits,
          cartesianPlot25_width, cartesianPlot25_height,
          CreateTool::kCartesianPlot},
      {QStringLiteral("Byte Monitor"), byte25_bits, byte25_width,
          byte25_height, CreateTool::kByteMonitor},
  };
}

std::vector<ObjectPaletteDialog::ButtonDefinition>
ObjectPaletteDialog::controlButtons()
{
  return {
      {QStringLiteral("Choice Button"), choiceButton25_bits,
          choiceButton25_width, choiceButton25_height,
          CreateTool::kChoiceButton},
      {QStringLiteral("Text Entry"), textEntry25_bits, textEntry25_width,
          textEntry25_height, CreateTool::kTextEntry},
      {QStringLiteral("Message Button"), messageButton25_bits,
          messageButton25_width, messageButton25_height,
          CreateTool::kMessageButton},
      {QStringLiteral("Menu"), menu25_bits, menu25_width, menu25_height,
          CreateTool::kMenu},
      {QStringLiteral("Slider"), valuator25_bits, valuator25_width,
          valuator25_height, CreateTool::kSlider},
      {QStringLiteral("Related Display"), relatedDisplay25_bits,
          relatedDisplay25_width, relatedDisplay25_height,
          CreateTool::kRelatedDisplay},
      {QStringLiteral("Shell Command"), shellCommand25_bits,
          shellCommand25_width, shellCommand25_height,
          CreateTool::kShellCommand},
      {QStringLiteral("Wheel Switch"), wheelSwitch25_bits,
          wheelSwitch25_width, wheelSwitch25_height,
          CreateTool::kWheelSwitch},
  };
}

std::vector<ObjectPaletteDialog::ButtonDefinition>
ObjectPaletteDialog::miscButtons()
{
  return {
      {QStringLiteral("Select"), select25_bits, select25_width,
          select25_height, CreateTool::kNone},
  };
}
