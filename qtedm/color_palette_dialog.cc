#include "color_palette_dialog.h"

#include <array>

#include <QAction>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "medm_colors.h"

ColorPaletteDialog::ColorPaletteDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &valueFont, QWidget *parent)
  : QDialog(parent)
  , labelFont_(labelFont)
  , valueFont_(valueFont)
{
  setObjectName(QStringLiteral("qtedmColorPalette"));
  setWindowTitle(QStringLiteral("Color Palette"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, false);
  setSizeGripEnabled(true);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  auto *menuBar = new QMenuBar;
  menuBar->setAutoFillBackground(true);
  menuBar->setPalette(basePalette);
  menuBar->setFont(labelFont_);

  auto *fileMenu = menuBar->addMenu(QStringLiteral("&File"));
  fileMenu->setFont(labelFont_);
  auto *closeAction = fileMenu->addAction(QStringLiteral("&Close"));
  QObject::connect(closeAction, &QAction::triggered, this, &QDialog::close);

  auto *helpMenu = menuBar->addMenu(QStringLiteral("&Help"));
  helpMenu->setFont(labelFont_);
  auto *helpAction = helpMenu->addAction(QStringLiteral("On &Color Palette"));
  QObject::connect(helpAction, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Select a color to apply to the current resource."));
  });

  mainLayout->setMenuBar(menuBar);

  auto *contentFrame = new QFrame;
  contentFrame->setFrameShape(QFrame::Panel);
  contentFrame->setFrameShadow(QFrame::Sunken);
  contentFrame->setLineWidth(2);
  contentFrame->setMidLineWidth(1);
  contentFrame->setAutoFillBackground(true);
  contentFrame->setPalette(basePalette);

  auto *gridLayout = new QGridLayout(contentFrame);
  gridLayout->setContentsMargins(6, 6, 6, 6);
  gridLayout->setHorizontalSpacing(4);
  gridLayout->setVerticalSpacing(4);

  const auto &colors = MedmColors::palette();
  for (int index = 0; index < static_cast<int>(colors.size()); ++index) {
    const int row = index % kColorRows;
    const int column = index / kColorRows;

    auto *button = new QPushButton;
    button->setFont(valueFont_);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setCheckable(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(32, 24);
    button->setText(QString());
    configureButtonColor(button, colors[index]);

    gridLayout->addWidget(button, row, column);
    colorButtons_.push_back(button);

    QObject::connect(button, &QPushButton::clicked, this,
        [this, index]() { handleColorClicked(index); });
  }

  mainLayout->addWidget(contentFrame);

  auto *messageFrame = new QFrame;
  messageFrame->setFrameShape(QFrame::Panel);
  messageFrame->setFrameShadow(QFrame::Sunken);
  messageFrame->setLineWidth(2);
  messageFrame->setMidLineWidth(1);
  messageFrame->setAutoFillBackground(true);
  messageFrame->setPalette(basePalette);

  auto *messageLayout = new QVBoxLayout(messageFrame);
  messageLayout->setContentsMargins(8, 4, 8, 4);
  messageLayout->setSpacing(2);

  messageLabel_ = new QLabel(QStringLiteral("Select color"));
  messageLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  messageLabel_->setFont(labelFont_);
  messageLabel_->setAutoFillBackground(false);
  messageLayout->addWidget(messageLabel_);

  auto *separator = new QFrame;
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Plain);
  separator->setLineWidth(1);
  messageLayout->addWidget(separator);

  mainLayout->addWidget(messageFrame);

  adjustSize();
  setMinimumWidth(sizeHint().width());
  updateMessageLabel();
}

void ColorPaletteDialog::setColorSelectedCallback(
    std::function<void(const QColor &)> callback)
{
  colorSelectedCallback_ = std::move(callback);
}

void ColorPaletteDialog::setCurrentColor(
    const QColor &color, const QString &description)
{
  currentColor_ = color;
  description_ = description;
  updateSelection();
  updateMessageLabel();
}

const std::array<QColor, 65> &ColorPaletteDialog::paletteColors()
{
  return MedmColors::palette();
}

void ColorPaletteDialog::configureButtonColor(
    QPushButton *button, const QColor &color)
{
  button->setAutoFillBackground(true);
  QPalette buttonPalette = button->palette();
  buttonPalette.setColor(QPalette::Button, color);
  buttonPalette.setColor(QPalette::Window, color);
  buttonPalette.setColor(QPalette::Base, color);
  buttonPalette.setColor(
      QPalette::ButtonText, color.lightness() < 128 ? Qt::white : Qt::black);
  button->setPalette(buttonPalette);
}

void ColorPaletteDialog::handleColorClicked(int index)
{
  const auto &colors = MedmColors::palette();
  if (index < 0 || index >= static_cast<int>(colors.size())) {
    return;
  }
  currentColor_ = colors[index];
  updateSelection();
  updateMessageLabel();
  if (colorSelectedCallback_) {
    colorSelectedCallback_(currentColor_);
  }
}

void ColorPaletteDialog::updateSelection()
{
  const auto &colors = MedmColors::palette();
  for (int i = 0; i < static_cast<int>(colorButtons_.size()); ++i) {
    QPushButton *button = colorButtons_[i];
    const bool isSelected = i < static_cast<int>(colors.size())
        && colors[i] == currentColor_;
    const QSignalBlocker blocker(button);
    button->setChecked(isSelected);
  }
}

void ColorPaletteDialog::updateMessageLabel()
{
  if (!messageLabel_) {
    return;
  }

  const QString colorText = currentColor_.isValid()
      ? currentColor_.name(QColor::HexRgb).toUpper()
      : QStringLiteral("N/A");

  const auto &colors = MedmColors::palette();
  int colorIndex = -1;
  for (int i = 0; i < static_cast<int>(colors.size()); ++i) {
    if (colors[i] == currentColor_) {
      colorIndex = i;
      break;
    }
  }

  QString displayText = colorText;
  if (colorIndex >= 0) {
    displayText =
        QStringLiteral("%1 (Color %2)").arg(colorText).arg(colorIndex);
  }

  if (description_.isEmpty()) {
    messageLabel_->setText(QStringLiteral("Select color (%1)").arg(displayText));
  } else {
    messageLabel_->setText(
        QStringLiteral("%1: %2").arg(description_, displayText));
  }
}

