#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <cstddef>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDialog>
#include <QAbstractScrollArea>
#include <QList>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QEvent>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QPaintEvent>
#include <QPen>
#include <QPointer>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QPoint>
#include <QScreen>
#include <QSize>
#include <QSizePolicy>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QStyleFactory>
#include <QTimer>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <memory>
#include <algorithm>
#include <utility>
#include <array>
#include <vector>

#include "legacy_fonts.h"
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

    if (autoClose) {
        QTimer::singleShot(5000, dialog, &QDialog::accept);
    }

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

constexpr int kDefaultDisplayWidth = 400;
constexpr int kDefaultDisplayHeight = 400;
constexpr int kDefaultGridSpacing = 5;
constexpr int kMinimumGridSpacing = 2;
constexpr bool kDefaultGridOn = false;
constexpr bool kDefaultSnapToGrid = false;

class DisplayWindow;

struct DisplayState {
  bool editMode = true;
  QList<QPointer<DisplayWindow>> displays;
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

class ColorPaletteDialog : public QDialog
{
public:
  ColorPaletteDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr)
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
    QObject::connect(closeAction, &QAction::triggered, this,
        &QDialog::close);

    auto *helpMenu = menuBar->addMenu(QStringLiteral("&Help"));
    helpMenu->setFont(labelFont_);
    auto *helpAction = helpMenu->addAction(
        QStringLiteral("On &Color Palette"));
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

    const auto &colors = paletteColors();
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

  void setColorSelectedCallback(std::function<void(const QColor &)> callback)
  {
    colorSelectedCallback_ = std::move(callback);
  }

  void setCurrentColor(const QColor &color, const QString &description)
  {
    currentColor_ = color;
    description_ = description;
    updateSelection();
    updateMessageLabel();
  }

private:
  static constexpr int kColorRows = 5;

  static const std::array<QColor, 65> &paletteColors()
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

  void configureButtonColor(QPushButton *button, const QColor &color)
  {
    button->setAutoFillBackground(true);
    QPalette buttonPalette = button->palette();
    buttonPalette.setColor(QPalette::Button, color);
    buttonPalette.setColor(QPalette::Window, color);
    buttonPalette.setColor(QPalette::Base, color);
    buttonPalette.setColor(QPalette::ButtonText,
        color.lightness() < 128 ? Qt::white : Qt::black);
    button->setPalette(buttonPalette);
  }

  void handleColorClicked(int index)
  {
    const auto &colors = paletteColors();
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

  void updateSelection()
  {
    const auto &colors = paletteColors();
    for (int i = 0; i < static_cast<int>(colorButtons_.size()); ++i) {
      QPushButton *button = colorButtons_[i];
      const bool isSelected = i < static_cast<int>(colors.size())
          && colors[i] == currentColor_;
      const QSignalBlocker blocker(button);
      button->setChecked(isSelected);
    }
  }

  QFont labelFont_;
  QFont valueFont_;
  std::vector<QPushButton *> colorButtons_;
  QLabel *messageLabel_ = nullptr;
  QColor currentColor_;
  QString description_;
  std::function<void(const QColor &)> colorSelectedCallback_;

  void updateMessageLabel()
  {
    if (!messageLabel_) {
      return;
    }

    const QString colorText = currentColor_.isValid()
        ? currentColor_.name(QColor::HexRgb).toUpper()
        : QStringLiteral("N/A");

    const auto &colors = paletteColors();
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
      messageLabel_->setText(
          QStringLiteral("Select color (%1)").arg(displayText));
    } else {
      messageLabel_->setText(
          QStringLiteral("%1: %2").arg(description_, displayText));
    }
  }
};

class ResourcePaletteDialog : public QDialog
{
public:
  ResourcePaletteDialog(const QPalette &basePalette, const QFont &labelFont,
      const QFont &valueFont, QWidget *parent = nullptr)
    : QDialog(parent)
    , labelFont_(labelFont)
    , valueFont_(valueFont)
  {
    setObjectName(QStringLiteral("qtedmResourcePalette"));
    setWindowTitle(QStringLiteral("Resource Palette"));
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
    QObject::connect(closeAction, &QAction::triggered, this,
        &QDialog::close);

    auto *helpMenu = menuBar->addMenu(QStringLiteral("&Help"));
    helpMenu->setFont(labelFont_);
    auto *helpAction = helpMenu->addAction(
        QStringLiteral("On &Resource Palette"));
    QObject::connect(helpAction, &QAction::triggered, this, [this]() {
      QMessageBox::information(this, windowTitle(),
          QStringLiteral("Displays and edits display-related resources."));
    });

    mainLayout->setMenuBar(menuBar);

    auto *contentFrame = new QFrame;
    contentFrame->setFrameShape(QFrame::Panel);
    contentFrame->setFrameShadow(QFrame::Sunken);
    contentFrame->setLineWidth(2);
    contentFrame->setMidLineWidth(1);
    contentFrame->setAutoFillBackground(true);
    contentFrame->setPalette(basePalette);

    auto *contentLayout = new QVBoxLayout(contentFrame);
    contentLayout->setContentsMargins(6, 6, 6, 6);
    contentLayout->setSpacing(6);

    scrollArea_ = new QScrollArea;
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setAutoFillBackground(true);
    scrollArea_->setPalette(basePalette);

    entriesWidget_ = new QWidget;
    entriesWidget_->setAutoFillBackground(true);
    entriesWidget_->setPalette(basePalette);
    entriesWidget_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *gridLayout = new QGridLayout(entriesWidget_);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setHorizontalSpacing(12);
    gridLayout->setVerticalSpacing(6);

    xEdit_ = createLineEdit();
    yEdit_ = createLineEdit();
    widthEdit_ = createLineEdit();
    heightEdit_ = createLineEdit();
    colormapEdit_ = createLineEdit();
    gridSpacingEdit_ = createLineEdit();

    setupGeometryField(xEdit_, GeometryField::kX);
    setupGeometryField(yEdit_, GeometryField::kY);
    setupGeometryField(widthEdit_, GeometryField::kWidth);
    setupGeometryField(heightEdit_, GeometryField::kHeight);
    setupGridSpacingField(gridSpacingEdit_);

    foregroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    backgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));

    QObject::connect(foregroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(foregroundButton_,
              QStringLiteral("Display Foreground"),
              foregroundColorSetter_);
        });
    QObject::connect(backgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(backgroundButton_,
              QStringLiteral("Display Background"),
              backgroundColorSetter_);
        });

    gridOnCombo_ = createBooleanComboBox();
    QObject::connect(gridOnCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (gridOnSetter_) {
            gridOnSetter_(index == 1);
          }
        });
    snapToGridCombo_ = createBooleanComboBox();

    addRow(gridLayout, 0, QStringLiteral("X Position"), xEdit_);
    addRow(gridLayout, 1, QStringLiteral("Y Position"), yEdit_);
    addRow(gridLayout, 2, QStringLiteral("Width"), widthEdit_);
    addRow(gridLayout, 3, QStringLiteral("Height"), heightEdit_);
    addRow(gridLayout, 4, QStringLiteral("Foreground"), foregroundButton_);
    addRow(gridLayout, 5, QStringLiteral("Background"), backgroundButton_);
    addRow(gridLayout, 6, QStringLiteral("Colormap"), colormapEdit_);
    addRow(gridLayout, 7, QStringLiteral("Grid Spacing"), gridSpacingEdit_);
    addRow(gridLayout, 8, QStringLiteral("Grid On"), gridOnCombo_);
    addRow(gridLayout, 9, QStringLiteral("Snap To Grid"), snapToGridCombo_);
    gridLayout->setRowStretch(10, 1);

    scrollArea_->setWidget(entriesWidget_);
    contentLayout->addWidget(scrollArea_);
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

    elementLabel_ = new QLabel(QStringLiteral("Select..."));
    elementLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    elementLabel_->setFont(labelFont_);
    elementLabel_->setAutoFillBackground(false);
    messageLayout->addWidget(elementLabel_);

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setLineWidth(1);
    messageLayout->addWidget(separator);

    mainLayout->addWidget(messageFrame);

    adjustSize();
    setMinimumWidth(sizeHint().width());
  }

  void showForDisplay(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<int()> gridSpacingGetter,
      std::function<void(int)> gridSpacingSetter,
      std::function<bool()> gridOnGetter,
      std::function<void(bool)> gridOnSetter)
  {
    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    foregroundColorGetter_ = std::move(foregroundGetter);
    foregroundColorSetter_ = std::move(foregroundSetter);
    backgroundColorGetter_ = std::move(backgroundGetter);
    backgroundColorSetter_ = std::move(backgroundSetter);
    gridSpacingGetter_ = std::move(gridSpacingGetter);
    gridSpacingSetter_ = std::move(gridSpacingSetter);
    gridOnGetter_ = std::move(gridOnGetter);
    gridOnSetter_ = std::move(gridOnSetter);

    QRect displayGeometry = geometryGetter_ ? geometryGetter_()
                                            : QRect(QPoint(0, 0),
                                                  QSize(kDefaultDisplayWidth,
                                                      kDefaultDisplayHeight));
    if (displayGeometry.width() <= 0) {
      displayGeometry.setWidth(kDefaultDisplayWidth);
    }
    if (displayGeometry.height() <= 0) {
      displayGeometry.setHeight(kDefaultDisplayHeight);
    }
    lastCommittedGeometry_ = displayGeometry;

    updateGeometryEdits(displayGeometry);
    if (gridSpacingEdit_) {
      const QSignalBlocker blocker(gridSpacingEdit_);
      const int spacing = gridSpacingGetter_ ? gridSpacingGetter_()
                                             : kDefaultGridSpacing;
      gridSpacingEdit_->setText(
          QString::number(std::max(kMinimumGridSpacing, spacing)));
      committedTexts_[gridSpacingEdit_] = gridSpacingEdit_->text();
    }
    colormapEdit_->clear();

    setColorButtonColor(foregroundButton_, currentForegroundColor());
    setColorButtonColor(backgroundButton_, currentBackgroundColor());

    if (gridOnCombo_) {
      const QSignalBlocker blocker(gridOnCombo_);
      const bool gridOn = gridOnGetter_ ? gridOnGetter_() : kDefaultGridOn;
      gridOnCombo_->setCurrentIndex(gridOn ? 1 : 0);
    }
    snapToGridCombo_->setCurrentIndex(kDefaultSnapToGrid ? 1 : 0);

    elementLabel_->setText(QStringLiteral("Display"));

    show();
    positionRelativeTo(parentWidget());
    raise();
    activateWindow();
  }

  void clearSelectionState()
  {
    geometryGetter_ = {};
    geometrySetter_ = {};
    foregroundColorGetter_ = {};
    foregroundColorSetter_ = {};
    backgroundColorGetter_ = {};
    backgroundColorSetter_ = {};
    activeColorSetter_ = {};
    gridSpacingGetter_ = {};
    gridSpacingSetter_ = {};
    gridOnGetter_ = {};
    gridOnSetter_ = {};
    activeColorButton_ = nullptr;
    lastCommittedGeometry_ = QRect();

    if (colorPaletteDialog_) {
      colorPaletteDialog_->hide();
    }

    resetLineEdit(xEdit_);
    resetLineEdit(yEdit_);
    resetLineEdit(widthEdit_);
    resetLineEdit(heightEdit_);
    resetLineEdit(colormapEdit_);
    resetLineEdit(gridSpacingEdit_);

    resetColorButton(foregroundButton_);
    resetColorButton(backgroundButton_);

    if (gridOnCombo_) {
      gridOnCombo_->setCurrentIndex(0);
    }
    if (snapToGridCombo_) {
      snapToGridCombo_->setCurrentIndex(0);
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Select..."));
    }

    updateCommittedTexts();
  }

protected:
  void closeEvent(QCloseEvent *event) override
  {
    clearSelectionState();
    QDialog::closeEvent(event);
  }

private:
  QLineEdit *createLineEdit()
  {
    auto *edit = new QLineEdit;
    edit->setFont(valueFont_);
    edit->setAutoFillBackground(true);
    QPalette editPalette = palette();
    editPalette.setColor(QPalette::Base, Qt::white);
    editPalette.setColor(QPalette::Text, Qt::black);
    edit->setPalette(editPalette);
    edit->setMaximumWidth(160);
    return edit;
  }

  QPushButton *createColorButton(const QColor &color)
  {
    auto *button = new QPushButton;
    button->setFont(valueFont_);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setFixedSize(120, 24);
    button->setFocusPolicy(Qt::NoFocus);
    setColorButtonColor(button, color);
    return button;
  }

  QComboBox *createBooleanComboBox()
  {
    auto *combo = new QComboBox;
    combo->setFont(valueFont_);
    combo->setAutoFillBackground(true);
    combo->addItem(QStringLiteral("false"));
    combo->addItem(QStringLiteral("true"));
    return combo;
  }

  void addRow(QGridLayout *layout, int row, const QString &label,
      QWidget *field)
  {
    auto *labelWidget = new QLabel(label);
    labelWidget->setFont(labelFont_);
    labelWidget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    labelWidget->setAutoFillBackground(false);
    layout->addWidget(labelWidget, row, 0);
    layout->addWidget(field, row, 1);
  }

  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (event->type() == QEvent::FocusOut) {
      if (auto *edit = qobject_cast<QLineEdit *>(object)) {
        if (edit == xEdit_ || edit == yEdit_ || edit == widthEdit_
            || edit == heightEdit_ || edit == gridSpacingEdit_) {
          revertLineEdit(edit);
        }
      }
    }

    return QDialog::eventFilter(object, event);
  }

  void setColorButtonColor(QPushButton *button, const QColor &color)
  {
    QPalette buttonPalette = button->palette();
    buttonPalette.setColor(QPalette::Button, color);
    buttonPalette.setColor(QPalette::Window, color);
    buttonPalette.setColor(QPalette::Base, color);
    buttonPalette.setColor(QPalette::ButtonText,
        color.lightness() < 128 ? Qt::white : Qt::black);
    button->setPalette(buttonPalette);
    button->setText(color.name(QColor::HexRgb).toUpper());
  }

  void resetLineEdit(QLineEdit *edit)
  {
    if (!edit) {
      return;
    }
    const QSignalBlocker blocker(edit);
    edit->clear();
  }

  void resetColorButton(QPushButton *button)
  {
    if (!button) {
      return;
    }
    QPalette buttonPalette = palette();
    button->setPalette(buttonPalette);
    button->setText(QString());
  }

  void positionRelativeTo(QWidget *reference)
  {
    QScreen *screen = screenForWidget(reference);
    if (!screen) {
      screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen ? screen->availableGeometry() : QRect();

    resizeToFitContents(available);

    if (reference) {
      const QRect referenceFrame = reference->frameGeometry();
      QPoint desiredTopLeft(referenceFrame.topRight());
      desiredTopLeft.rx() += 12;
      QRect desiredRect(desiredTopLeft, size());
      if (available.isNull() || available.contains(desiredRect)) {
        move(desiredTopLeft);
        return;
      }
    }

    moveToTopRight(available, size());
  }

  QScreen *screenForWidget(const QWidget *widget) const
  {
    if (!widget) {
      return nullptr;
    }
    if (QScreen *screen = widget->screen()) {
      return screen;
    }
    const QPoint globalCenter = widget->mapToGlobal(
        QPoint(widget->width() / 2, widget->height() / 2));
    return QGuiApplication::screenAt(globalCenter);
  }

  void moveToTopRight(const QRect &area, const QSize &dialogSize)
  {
    if (area.isNull()) {
      move(0, 0);
      return;
    }
    const int x = std::max(area.left(), area.right() - dialogSize.width() + 1);
    const int y = area.top();
    move(x, y);
  }

  void resizeToFitContents(const QRect &available)
  {
    QSize target = sizeHint();
    target.rwidth() += 48;
    target.rheight() += 48;
    if (!available.isNull()) {
      target.setWidth(std::min(target.width(), available.width()));
      target.setHeight(std::min(target.height(), available.height()));
    }

    QSize newSize = size();
    newSize.setWidth(std::max(newSize.width(), target.width()));
    newSize.setHeight(std::max(newSize.height(), target.height()));
    resize(newSize);
  }

  QFont labelFont_;
  QFont valueFont_;
  QLineEdit *xEdit_ = nullptr;
  QLineEdit *yEdit_ = nullptr;
  QLineEdit *widthEdit_ = nullptr;
  QLineEdit *heightEdit_ = nullptr;
  QLineEdit *colormapEdit_ = nullptr;
  QLineEdit *gridSpacingEdit_ = nullptr;
  QPushButton *foregroundButton_ = nullptr;
  QPushButton *backgroundButton_ = nullptr;
  QComboBox *gridOnCombo_ = nullptr;
  QComboBox *snapToGridCombo_ = nullptr;
  QLabel *elementLabel_ = nullptr;
  QScrollArea *scrollArea_ = nullptr;
  QWidget *entriesWidget_ = nullptr;
  enum class GeometryField { kX, kY, kWidth, kHeight };
  void setupGeometryField(QLineEdit *edit, GeometryField field)
  {
    committedTexts_.insert(edit, edit->text());
    edit->installEventFilter(this);
    QObject::connect(edit, &QLineEdit::returnPressed, this,
        [this, field]() { commitGeometryField(field); });
  }

  void setupGridSpacingField(QLineEdit *edit)
  {
    if (!edit) {
      return;
    }
    committedTexts_.insert(edit, edit->text());
    edit->installEventFilter(this);
    QObject::connect(edit, &QLineEdit::returnPressed, this,
        [this]() { commitGridSpacing(); });
    QObject::connect(edit, &QLineEdit::editingFinished, this,
        [this]() { commitGridSpacing(); });
  }

  void commitGeometryField(GeometryField field)
  {
    if (!geometrySetter_) {
      revertLineEdit(editForField(field));
      return;
    }

    QLineEdit *edit = editForField(field);
    if (!edit) {
      return;
    }

    bool ok = false;
    const int value = edit->text().toInt(&ok);
    if (!ok) {
      revertLineEdit(edit);
      return;
    }

    QRect geometry = geometryGetter_ ? geometryGetter_() : lastCommittedGeometry_;
    switch (field) {
    case GeometryField::kX:
      geometry.moveLeft(value);
      break;
    case GeometryField::kY:
      geometry.moveTop(value);
      break;
    case GeometryField::kWidth:
      geometry.setWidth(value);
      break;
    case GeometryField::kHeight:
      geometry.setHeight(value);
      break;
    }

    if (geometry.width() <= 0 || geometry.height() <= 0) {
      revertLineEdit(edit);
      return;
    }

    geometrySetter_(geometry);
    const QRect effectiveGeometry = geometryGetter_ ? geometryGetter_() : geometry;
    lastCommittedGeometry_ = effectiveGeometry;
    updateGeometryEdits(effectiveGeometry);
  }

  void revertLineEdit(QLineEdit *edit)
  {
    if (!edit) {
      return;
    }

    const QString committed = committedTexts_.value(edit, edit->text());
    if (edit->text() != committed) {
      const QSignalBlocker blocker(edit);
      edit->setText(committed);
    }
  }

  void updateGeometryEdits(const QRect &geometry)
  {
    {
      const QSignalBlocker blocker(xEdit_);
      xEdit_->setText(QString::number(geometry.x()));
    }
    {
      const QSignalBlocker blocker(yEdit_);
      yEdit_->setText(QString::number(geometry.y()));
    }
    {
      const QSignalBlocker blocker(widthEdit_);
      widthEdit_->setText(QString::number(geometry.width()));
    }
    {
      const QSignalBlocker blocker(heightEdit_);
      heightEdit_->setText(QString::number(geometry.height()));
    }

    updateCommittedTexts();
  }

  void updateCommittedTexts()
  {
    committedTexts_[xEdit_] = xEdit_->text();
    committedTexts_[yEdit_] = yEdit_->text();
    committedTexts_[widthEdit_] = widthEdit_->text();
    committedTexts_[heightEdit_] = heightEdit_->text();
    if (gridSpacingEdit_) {
      committedTexts_[gridSpacingEdit_] = gridSpacingEdit_->text();
    }
  }

  void commitGridSpacing()
  {
    if (!gridSpacingEdit_) {
      return;
    }
    if (!gridSpacingSetter_) {
      revertLineEdit(gridSpacingEdit_);
      return;
    }

    bool ok = false;
    int value = gridSpacingEdit_->text().toInt(&ok);
    if (!ok) {
      revertLineEdit(gridSpacingEdit_);
      return;
    }

    value = std::max(kMinimumGridSpacing, value);
    gridSpacingSetter_(value);

    const int effectiveSpacing = gridSpacingGetter_ ? gridSpacingGetter_()
                                                   : value;
    const int clampedSpacing = std::max(kMinimumGridSpacing, effectiveSpacing);
    const QSignalBlocker blocker(gridSpacingEdit_);
    gridSpacingEdit_->setText(QString::number(clampedSpacing));
    committedTexts_[gridSpacingEdit_] = gridSpacingEdit_->text();
  }

  std::function<QRect()> geometryGetter_;
  std::function<void(const QRect &)> geometrySetter_;
  QRect lastCommittedGeometry_;
  QHash<QLineEdit *, QString> committedTexts_;
  ColorPaletteDialog *colorPaletteDialog_ = nullptr;
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

  QLineEdit *editForField(GeometryField field) const
  {
    switch (field) {
    case GeometryField::kX:
      return xEdit_;
    case GeometryField::kY:
      return yEdit_;
    case GeometryField::kWidth:
      return widthEdit_;
    case GeometryField::kHeight:
      return heightEdit_;
    }

    return nullptr;
  }

  void openColorPalette(QPushButton *button, const QString &description,
      const std::function<void(const QColor &)> &setter)
  {
    if (!button) {
      return;
    }

    if (!colorPaletteDialog_) {
      colorPaletteDialog_ = new ColorPaletteDialog(palette(), labelFont_,
          valueFont_, this);
      colorPaletteDialog_->setColorSelectedCallback(
          [this](const QColor &color) {
            if (activeColorButton_) {
              setColorButtonColor(activeColorButton_, color);
            }
            if (activeColorSetter_) {
              activeColorSetter_(color);
            }
          });
      QObject::connect(colorPaletteDialog_, &QDialog::finished, this,
          [this](int) {
            activeColorButton_ = nullptr;
            activeColorSetter_ = {};
          });
    }

    activeColorButton_ = button;
    activeColorSetter_ = setter;
    colorPaletteDialog_->setCurrentColor(colorFromButton(button), description);
    colorPaletteDialog_->show();
    colorPaletteDialog_->raise();
    colorPaletteDialog_->activateWindow();
  }

  QColor colorFromButton(const QPushButton *button) const
  {
    if (!button) {
      return QColor();
    }
    return button->palette().color(QPalette::Button);
  }

  QColor currentForegroundColor() const
  {
    if (foregroundColorGetter_) {
      const QColor color = foregroundColorGetter_();
      if (color.isValid()) {
        return color;
      }
    }
    return palette().color(QPalette::WindowText);
  }

  QColor currentBackgroundColor() const
  {
    if (backgroundColorGetter_) {
      const QColor color = backgroundColorGetter_();
      if (color.isValid()) {
        return color;
      }
    }
    return palette().color(QPalette::Window);
  }
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

  void clearSelection()
  {
    if (!displaySelected_) {
      return;
    }
    if (!resourcePalette_.isNull() && resourcePalette_->isVisible()) {
      resourcePalette_->close();
    }
    setDisplaySelected(false);
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

protected:
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      if (auto state = state_.lock(); state && state->editMode) {
        if (displaySelected_) {
          clearSelection();
          event->accept();
          return;
        }
        if (resourcePalette_.isNull()) {
          resourcePalette_ = new ResourcePaletteDialog(
              resourcePaletteBase_, labelFont_, font(), this);
          QObject::connect(resourcePalette_, &QDialog::finished, this,
              [this](int) {
                clearSelection();
              });
          QObject::connect(resourcePalette_, &QObject::destroyed, this,
              [this]() {
                clearSelection();
              });
        }

        for (auto &display : state->displays) {
          if (!display.isNull() && display != this) {
            display->clearSelection();
          }
        }

        setDisplaySelected(true);
        resourcePalette_->showForDisplay(
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
        event->accept();
        return;
      }
    }

    QMainWindow::mousePressEvent(event);
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

          QObject::connect(displayWin, &QObject::destroyed, &win,
              [state, updateMenus]() {
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
            for (auto &display : state->displays) {
              if (!display.isNull()) {
                display->clearSelection();
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
    return app.exec();
}
