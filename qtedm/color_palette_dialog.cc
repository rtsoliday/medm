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

namespace {

const std::array<QColor, 65> &paletteColorsInternal()
{
  static const std::array<QColor, 65> colors = {QColor(255, 255, 255),
      QColor(236, 236, 236), QColor(218, 218, 218), QColor(200, 200, 200),
      QColor(187, 187, 187), QColor(174, 174, 174), QColor(158, 158, 158),
      QColor(145, 145, 145), QColor(133, 133, 133), QColor(120, 120, 120),
      QColor(105, 105, 105), QColor(90, 90, 90), QColor(70, 70, 70),
      QColor(45, 45, 45), QColor(0, 0, 0), QColor(0, 216, 0),
      QColor(30, 187, 0), QColor(51, 153, 0), QColor(45, 127, 0),
      QColor(33, 108, 0), QColor(253, 0, 0), QColor(222, 19, 9),
      QColor(190, 25, 11), QColor(160, 18, 7), QColor(130, 4, 0),
      QColor(88, 147, 255), QColor(89, 126, 225), QColor(75, 110, 199),
      QColor(58, 94, 171), QColor(39, 84, 141), QColor(251, 243, 74),
      QColor(249, 218, 60), QColor(238, 182, 43), QColor(225, 144, 21),
      QColor(205, 97, 0), QColor(255, 176, 255), QColor(214, 127, 226),
      QColor(174, 78, 188), QColor(139, 26, 150), QColor(97, 10, 117),
      QColor(164, 170, 255), QColor(135, 147, 226), QColor(106, 115, 193),
      QColor(77, 82, 164), QColor(52, 51, 134), QColor(199, 187, 109),
      QColor(183, 157, 92), QColor(164, 126, 60), QColor(125, 86, 39),
      QColor(88, 52, 15), QColor(153, 255, 255), QColor(115, 223, 255),
      QColor(78, 165, 249), QColor(42, 99, 228), QColor(10, 0, 184),
      QColor(235, 241, 181), QColor(212, 219, 157), QColor(187, 193, 135),
      QColor(166, 164, 98), QColor(139, 130, 57), QColor(115, 255, 107),
      QColor(82, 218, 59), QColor(60, 180, 32), QColor(40, 147, 21),
      QColor(26, 115, 9)};
  return colors;
}

} // namespace

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

  const auto &colors = paletteColorsInternal();
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
  return paletteColorsInternal();
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
  const auto &colors = paletteColorsInternal();
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
  const auto &colors = paletteColorsInternal();
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

  const auto &colors = paletteColorsInternal();
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

