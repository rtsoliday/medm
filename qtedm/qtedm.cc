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

enum class TextColorMode
{
  kStatic,
  kAlarm,
  kDiscrete
};

enum class TextVisibilityMode
{
  kStatic,
  kIfNotZero,
  kIfZero,
  kCalc
};

const std::array<QString, 16> &textFontAliases()
{
    static const std::array<QString, 16> kAliases = {
        QStringLiteral("widgetDM_4"),
        QStringLiteral("widgetDM_6"),
        QStringLiteral("widgetDM_8"),
        QStringLiteral("widgetDM_10"),
        QStringLiteral("widgetDM_12"),
        QStringLiteral("widgetDM_14"),
        QStringLiteral("widgetDM_16"),
        QStringLiteral("widgetDM_18"),
        QStringLiteral("widgetDM_20"),
        QStringLiteral("widgetDM_22"),
        QStringLiteral("widgetDM_24"),
        QStringLiteral("widgetDM_30"),
        QStringLiteral("widgetDM_36"),
        QStringLiteral("widgetDM_40"),
        QStringLiteral("widgetDM_48"),
        QStringLiteral("widgetDM_60"),
    };
    return kAliases;
}

QFont medmCompatibleTextFont(const QString &text, const QSize &availableSize)
{
    if (availableSize.width() <= 0 || availableSize.height() <= 0) {
        return QFont();
    }

    QString sample = text;
    if (sample.trimmed().isEmpty()) {
        sample = QStringLiteral("Ag");
    }

    QFont chosen;
    bool found = false;

    for (const QString &alias : textFontAliases()) {
        const QFont font = LegacyFonts::font(alias);
        if (font.family().isEmpty()) {
            continue;
        }

        const QFontMetrics metrics(font);
        const int fontHeight = metrics.ascent() + metrics.descent();
        if (fontHeight > availableSize.height()) {
            continue;
        }

        const int fontWidth = metrics.horizontalAdvance(sample);
        if (fontWidth > availableSize.width()) {
            continue;
        }

        chosen = font;
        found = true;
    }

    if (found) {
        return chosen;
    }

    // Fall back to the smallest available MEDM bitmap font.
    for (const QString &alias : textFontAliases()) {
        const QFont fallback = LegacyFonts::font(alias);
        if (!fallback.family().isEmpty()) {
            return fallback;
        }
    }

    return QFont();
}

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

constexpr int kDefaultDisplayWidth = 400;
constexpr int kDefaultDisplayHeight = 400;
constexpr int kDefaultGridSpacing = 5;
constexpr int kMinimumGridSpacing = 2;
constexpr bool kDefaultGridOn = false;
constexpr bool kDefaultSnapToGrid = false;
constexpr int kMinimumTextWidth = 40;
constexpr int kMinimumTextHeight = 20;
constexpr int kMainWindowRightMargin = 5;
constexpr int kMainWindowTopMargin = 5;

enum class CreateTool {
  kNone,
  kText,
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

    auto *entriesLayout = new QVBoxLayout(entriesWidget_);
    entriesLayout->setContentsMargins(0, 0, 0, 0);
    entriesLayout->setSpacing(12);

    geometrySection_ = new QWidget(entriesWidget_);
    auto *geometryLayout = new QGridLayout(geometrySection_);
    geometryLayout->setContentsMargins(0, 0, 0, 0);
    geometryLayout->setHorizontalSpacing(12);
    geometryLayout->setVerticalSpacing(6);

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

    addRow(geometryLayout, 0, QStringLiteral("X Position"), xEdit_);
    addRow(geometryLayout, 1, QStringLiteral("Y Position"), yEdit_);
    addRow(geometryLayout, 2, QStringLiteral("Width"), widthEdit_);
    addRow(geometryLayout, 3, QStringLiteral("Height"), heightEdit_);
    geometryLayout->setRowStretch(4, 1);
    entriesLayout->addWidget(geometrySection_);

    displaySection_ = new QWidget(entriesWidget_);
    auto *displayLayout = new QGridLayout(displaySection_);
    displayLayout->setContentsMargins(0, 0, 0, 0);
    displayLayout->setHorizontalSpacing(12);
    displayLayout->setVerticalSpacing(6);

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

    addRow(displayLayout, 0, QStringLiteral("Foreground"), foregroundButton_);
    addRow(displayLayout, 1, QStringLiteral("Background"), backgroundButton_);
    addRow(displayLayout, 2, QStringLiteral("Colormap"), colormapEdit_);
    addRow(displayLayout, 3, QStringLiteral("Grid Spacing"), gridSpacingEdit_);
    addRow(displayLayout, 4, QStringLiteral("Grid On"), gridOnCombo_);
    addRow(displayLayout, 5, QStringLiteral("Snap To Grid"), snapToGridCombo_);
    displayLayout->setRowStretch(6, 1);
    entriesLayout->addWidget(displaySection_);

    textSection_ = new QWidget(entriesWidget_);
    auto *textLayout = new QGridLayout(textSection_);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setHorizontalSpacing(12);
    textLayout->setVerticalSpacing(6);

    textStringEdit_ = createLineEdit();
    QObject::connect(textStringEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextString(); });
    QObject::connect(textStringEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextString(); });

    textAlignmentCombo_ = new QComboBox;
    textAlignmentCombo_->setFont(valueFont_);
    textAlignmentCombo_->setAutoFillBackground(true);
    textAlignmentCombo_->addItem(QStringLiteral("Left"));
    textAlignmentCombo_->addItem(QStringLiteral("Center"));
    textAlignmentCombo_->addItem(QStringLiteral("Right"));
    QObject::connect(textAlignmentCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAlignmentSetter_) {
            textAlignmentSetter_(alignmentFromIndex(index));
          }
        });

    textForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(textForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textForegroundButton_,
              QStringLiteral("Text Foreground"), textForegroundSetter_);
        });

    textColorModeCombo_ = new QComboBox;
    textColorModeCombo_->setFont(valueFont_);
    textColorModeCombo_->setAutoFillBackground(true);
    textColorModeCombo_->addItem(QStringLiteral("Static"));
    textColorModeCombo_->addItem(QStringLiteral("Alarm"));
    textColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(textColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textColorModeSetter_) {
            textColorModeSetter_(colorModeFromIndex(index));
          }
        });

    textVisibilityCombo_ = new QComboBox;
    textVisibilityCombo_->setFont(valueFont_);
    textVisibilityCombo_->setAutoFillBackground(true);
    textVisibilityCombo_->addItem(QStringLiteral("Static"));
    textVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    textVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    textVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(textVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textVisibilityModeSetter_) {
            textVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
        });

    textVisibilityCalcEdit_ = createLineEdit();
    QObject::connect(textVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextVisibilityCalc(); });
    QObject::connect(textVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(textChannelEdits_.size()); ++i) {
      textChannelEdits_[i] = createLineEdit();
      QObject::connect(textChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitTextChannel(i); });
      QObject::connect(textChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitTextChannel(i); });
    }

    addRow(textLayout, 0, QStringLiteral("Text String"), textStringEdit_);
    addRow(textLayout, 1, QStringLiteral("Alignment"), textAlignmentCombo_);
    addRow(textLayout, 2, QStringLiteral("Foreground"), textForegroundButton_);
    addRow(textLayout, 3, QStringLiteral("Color Mode"), textColorModeCombo_);
    addRow(textLayout, 4, QStringLiteral("Visibility"), textVisibilityCombo_);
    addRow(textLayout, 5, QStringLiteral("Vis Calc"), textVisibilityCalcEdit_);
    addRow(textLayout, 6, QStringLiteral("Channel A"), textChannelEdits_[0]);
    addRow(textLayout, 7, QStringLiteral("Channel B"), textChannelEdits_[1]);
    addRow(textLayout, 8, QStringLiteral("Channel C"), textChannelEdits_[2]);
    addRow(textLayout, 9, QStringLiteral("Channel D"), textChannelEdits_[3]);
    textLayout->setRowStretch(10, 1);
    entriesLayout->addWidget(textSection_);

    entriesLayout->addStretch(1);

    displaySection_->setVisible(false);
    textSection_->setVisible(false);
    updateSectionVisibility(selectionKind_);

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
    selectionKind_ = SelectionKind::kDisplay;
    updateSectionVisibility(selectionKind_);

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
    textGetter_ = {};
    textSetter_ = {};
    committedTextString_.clear();
    if (textStringEdit_) {
      const QSignalBlocker blocker(textStringEdit_);
      textStringEdit_->clear();
    }

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
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters)
  {
    selectionKind_ = SelectionKind::kText;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    foregroundColorGetter_ = {};
    foregroundColorSetter_ = {};
    backgroundColorGetter_ = {};
    backgroundColorSetter_ = {};
    activeColorSetter_ = {};
    gridSpacingGetter_ = {};
    gridSpacingSetter_ = {};
    gridOnGetter_ = {};
    gridOnSetter_ = {};
    textGetter_ = std::move(textGetter);
    textSetter_ = std::move(textSetter);
    textForegroundGetter_ = std::move(foregroundGetter);
    textForegroundSetter_ = std::move(foregroundSetter);
    textAlignmentGetter_ = std::move(alignmentGetter);
    textAlignmentSetter_ = std::move(alignmentSetter);
    textColorModeGetter_ = std::move(colorModeGetter);
    textColorModeSetter_ = std::move(colorModeSetter);
    textVisibilityModeGetter_ = std::move(visibilityModeGetter);
    textVisibilityModeSetter_ = std::move(visibilityModeSetter);
    textVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    textVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    textChannelGetters_ = std::move(channelGetters);
    textChannelSetters_ = std::move(channelSetters);

    QRect textGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (textGeometry.width() <= 0) {
      textGeometry.setWidth(kMinimumTextWidth);
    }
    if (textGeometry.height() <= 0) {
      textGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = textGeometry;

    updateGeometryEdits(textGeometry);
    if (textStringEdit_) {
      const QString currentText = textGetter_ ? textGetter_() : QString();
      const QSignalBlocker blocker(textStringEdit_);
      textStringEdit_->setText(currentText);
      committedTextString_ = currentText;
    }

    if (textAlignmentCombo_) {
      const QSignalBlocker blocker(textAlignmentCombo_);
      const Qt::Alignment alignment =
          textAlignmentGetter_ ? textAlignmentGetter_()
                               : (Qt::AlignLeft | Qt::AlignVCenter);
      textAlignmentCombo_->setCurrentIndex(alignmentToIndex(alignment));
    }

    if (textForegroundButton_) {
      const QColor color = textForegroundGetter_ ? textForegroundGetter_()
                                                 : palette().color(
                                                       QPalette::WindowText);
      setColorButtonColor(textForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (textColorModeCombo_) {
      const QSignalBlocker blocker(textColorModeCombo_);
      const TextColorMode mode = textColorModeGetter_ ? textColorModeGetter_()
                                                      : TextColorMode::kStatic;
      textColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (textVisibilityCombo_) {
      const QSignalBlocker blocker(textVisibilityCombo_);
      const TextVisibilityMode mode =
          textVisibilityModeGetter_ ? textVisibilityModeGetter_()
                                    : TextVisibilityMode::kStatic;
      textVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
    }

    if (textVisibilityCalcEdit_) {
      const QString calc =
          textVisibilityCalcGetter_ ? textVisibilityCalcGetter_() : QString();
      const QSignalBlocker blocker(textVisibilityCalcEdit_);
      textVisibilityCalcEdit_->setText(calc);
      committedTexts_[textVisibilityCalcEdit_] = textVisibilityCalcEdit_->text();
    }

    for (int i = 0; i < static_cast<int>(textChannelEdits_.size()); ++i) {
      QLineEdit *edit = textChannelEdits_[i];
      if (!edit) {
        continue;
      }
      const QString value =
          textChannelGetters_[i] ? textChannelGetters_[i]() : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      committedTexts_[edit] = edit->text();
    }

    elementLabel_->setText(QStringLiteral("Text"));

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
    textGetter_ = {};
    textSetter_ = {};
    textForegroundGetter_ = {};
    textForegroundSetter_ = {};
    textAlignmentGetter_ = {};
    textAlignmentSetter_ = {};
    textColorModeGetter_ = {};
    textColorModeSetter_ = {};
    textVisibilityModeGetter_ = {};
    textVisibilityModeSetter_ = {};
    textVisibilityCalcGetter_ = {};
    textVisibilityCalcSetter_ = {};
    for (auto &getter : textChannelGetters_) {
      getter = {};
    }
    for (auto &setter : textChannelSetters_) {
      setter = {};
    }
    activeColorButton_ = nullptr;
    lastCommittedGeometry_ = QRect();
    committedTextString_.clear();
    selectionKind_ = SelectionKind::kNone;

    if (colorPaletteDialog_) {
      colorPaletteDialog_->hide();
    }

    resetLineEdit(xEdit_);
    resetLineEdit(yEdit_);
    resetLineEdit(widthEdit_);
    resetLineEdit(heightEdit_);
    resetLineEdit(colormapEdit_);
    resetLineEdit(gridSpacingEdit_);
    resetLineEdit(textStringEdit_);
    resetLineEdit(textVisibilityCalcEdit_);
    for (QLineEdit *edit : textChannelEdits_) {
      resetLineEdit(edit);
    }

    resetColorButton(foregroundButton_);
    resetColorButton(backgroundButton_);
    resetColorButton(textForegroundButton_);

    if (gridOnCombo_) {
      const QSignalBlocker blocker(gridOnCombo_);
      gridOnCombo_->setCurrentIndex(0);
    }
    if (snapToGridCombo_) {
      const QSignalBlocker blocker(snapToGridCombo_);
      snapToGridCombo_->setCurrentIndex(0);
    }
    if (textAlignmentCombo_) {
      const QSignalBlocker blocker(textAlignmentCombo_);
      textAlignmentCombo_->setCurrentIndex(
          alignmentToIndex(Qt::AlignLeft | Qt::AlignVCenter));
    }
    if (textColorModeCombo_) {
      const QSignalBlocker blocker(textColorModeCombo_);
      textColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (textVisibilityCombo_) {
      const QSignalBlocker blocker(textVisibilityCombo_);
      textVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Select..."));
    }

    committedTexts_.clear();
    updateCommittedTexts();
    updateSectionVisibility(selectionKind_);
  }

protected:
  void closeEvent(QCloseEvent *event) override
  {
    clearSelectionState();
    QDialog::closeEvent(event);
  }

private:
  enum class SelectionKind { kNone, kDisplay, kText };
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

  void updateSectionVisibility(SelectionKind kind)
  {
    if (geometrySection_) {
      const bool showGeometry = kind != SelectionKind::kNone;
      geometrySection_->setVisible(showGeometry);
      geometrySection_->setEnabled(showGeometry);
    }
    if (displaySection_) {
      const bool displayVisible = kind == SelectionKind::kDisplay;
      displaySection_->setVisible(displayVisible);
      displaySection_->setEnabled(displayVisible);
    }
    if (textSection_) {
      const bool textVisible = kind == SelectionKind::kText;
      textSection_->setVisible(textVisible);
      textSection_->setEnabled(textVisible);
    }
    if (textStringEdit_) {
      textStringEdit_->setEnabled(kind == SelectionKind::kText);
    }
  }

  void commitTextString()
  {
    if (!textStringEdit_) {
      return;
    }
    if (!textSetter_) {
      revertTextString();
      return;
    }
    const QString value = textStringEdit_->text();
    textSetter_(value);
    committedTextString_ = value;
  }

  void revertTextString()
  {
    if (!textStringEdit_) {
      return;
    }
    if (textStringEdit_->text() == committedTextString_) {
      return;
    }
    const QSignalBlocker blocker(textStringEdit_);
    textStringEdit_->setText(committedTextString_);
  }

  void commitTextVisibilityCalc()
  {
    if (!textVisibilityCalcEdit_) {
      return;
    }
    if (!textVisibilityCalcSetter_) {
      revertLineEdit(textVisibilityCalcEdit_);
      return;
    }
    const QString value = textVisibilityCalcEdit_->text();
    textVisibilityCalcSetter_(value);
    committedTexts_[textVisibilityCalcEdit_] = value;
  }

  void commitTextChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(textChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = textChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!textChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    textChannelSetters_[index](value);
    committedTexts_[edit] = value;
  }

  Qt::Alignment alignmentFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return Qt::AlignHCenter | Qt::AlignVCenter;
    case 2:
      return Qt::AlignRight | Qt::AlignVCenter;
    default:
      return Qt::AlignLeft | Qt::AlignVCenter;
    }
  }

  int alignmentToIndex(Qt::Alignment alignment) const
  {
    const Qt::Alignment horizontal = alignment & Qt::AlignHorizontal_Mask;
    if (horizontal == Qt::AlignHCenter) {
      return 1;
    }
    if (horizontal == Qt::AlignRight) {
      return 2;
    }
    return 0;
  }

  TextColorMode colorModeFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return TextColorMode::kAlarm;
    case 2:
      return TextColorMode::kDiscrete;
    default:
      return TextColorMode::kStatic;
    }
  }

  int colorModeToIndex(TextColorMode mode) const
  {
    switch (mode) {
    case TextColorMode::kAlarm:
      return 1;
    case TextColorMode::kDiscrete:
      return 2;
    case TextColorMode::kStatic:
    default:
      return 0;
    }
  }

  TextVisibilityMode visibilityModeFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return TextVisibilityMode::kIfNotZero;
    case 2:
      return TextVisibilityMode::kIfZero;
    case 3:
      return TextVisibilityMode::kCalc;
    default:
      return TextVisibilityMode::kStatic;
    }
  }

  int visibilityModeToIndex(TextVisibilityMode mode) const
  {
    switch (mode) {
    case TextVisibilityMode::kIfNotZero:
      return 1;
    case TextVisibilityMode::kIfZero:
      return 2;
    case TextVisibilityMode::kCalc:
      return 3;
    case TextVisibilityMode::kStatic:
    default:
      return 0;
    }
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
        scheduleDeferredResize(reference);
        return;
      }
    }

    moveToTopRight(available, size());
    scheduleDeferredResize(reference);
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
    if (entriesWidget_) {
      entriesWidget_->adjustSize();
      if (QLayout *entriesLayout = entriesWidget_->layout()) {
        entriesLayout->activate();
      }
    }
    if (QLayout *dialogLayout = layout()) {
      dialogLayout->activate();
    }

    QSize target = sizeHint();
    if (scrollArea_ && entriesWidget_) {
      const QSize contentHint = entriesWidget_->sizeHint();
      const QSize scrollHint = scrollArea_->sizeHint();
      const int widthDelta = std::max(0, contentHint.width() - scrollHint.width());
      const int heightDelta = std::max(0, contentHint.height() - scrollHint.height());
      target.rwidth() += widthDelta;
      target.rheight() += heightDelta;
    }
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

  void scheduleDeferredResize(QWidget *reference)
  {
    QPointer<ResourcePaletteDialog> guard(this);
    QPointer<QWidget> ref(reference);
    QMetaObject::invokeMethod(this, [guard, ref]() {
      if (!guard) {
        return;
      }

  QWidget *referenceWidget = ref ? ref.data() : guard->parentWidget();
  QWidget *anchorWidget = referenceWidget ? referenceWidget : guard.data();
  QScreen *screen = guard->screenForWidget(anchorWidget);
      if (!screen) {
        screen = QGuiApplication::primaryScreen();
      }
      const QRect available = screen ? screen->availableGeometry() : QRect();

      guard->resizeToFitContents(available);

      if (referenceWidget) {
        const QRect referenceFrame = referenceWidget->frameGeometry();
        QPoint desiredTopLeft(referenceFrame.topRight());
        desiredTopLeft.rx() += 12;
        QRect desiredRect(desiredTopLeft, guard->size());
        if (available.isNull() || available.contains(desiredRect)) {
          guard->move(desiredTopLeft);
          return;
        }
      }

      guard->moveToTopRight(available, guard->size());
    }, Qt::QueuedConnection);
  }

  QFont labelFont_;
  QFont valueFont_;
  QWidget *geometrySection_ = nullptr;
  QWidget *displaySection_ = nullptr;
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
  std::array<QLineEdit *, 4> textChannelEdits_{};
  QPushButton *foregroundButton_ = nullptr;
  QPushButton *backgroundButton_ = nullptr;
  QComboBox *gridOnCombo_ = nullptr;
  QComboBox *snapToGridCombo_ = nullptr;
  QLabel *elementLabel_ = nullptr;
  QScrollArea *scrollArea_ = nullptr;
  QWidget *entriesWidget_ = nullptr;
  SelectionKind selectionKind_ = SelectionKind::kNone;
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
  std::array<std::function<QString()>, 4> textChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> textChannelSetters_{};
  QString committedTextString_;

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

class TextElement : public QLabel
{
public:
  explicit TextElement(QWidget *parent = nullptr)
    : QLabel(parent)
  {
    setAutoFillBackground(false);
    setWordWrap(true);
    setContentsMargins(2, 2, 2, 2);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    setForegroundColor(palette().color(QPalette::WindowText));
    setColorMode(TextColorMode::kStatic);
    setVisibilityMode(TextVisibilityMode::kStatic);
    updateSelectionVisual();
  }

  void setSelected(bool selected)
  {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    updateSelectionVisual();
  }

  bool isSelected() const
  {
    return selected_;
  }

  QColor foregroundColor() const
  {
    return foregroundColor_;
  }

  void setForegroundColor(const QColor &color)
  {
    QColor effective = color;
    if (!effective.isValid()) {
      effective = defaultForegroundColor();
    }
    const bool changed = (foregroundColor_ != effective);
    foregroundColor_ = effective;
    applyTextColor();
    if (changed) {
      update();
    }
  }

  void setText(const QString &value)
  {
    QLabel::setText(value);
    updateFontForGeometry();
  }

  Qt::Alignment textAlignment() const
  {
    return alignment_;
  }

  void setTextAlignment(Qt::Alignment alignment)
  {
    Qt::Alignment effective = alignment;
    if (!(effective & Qt::AlignHorizontal_Mask)) {
      effective |= Qt::AlignLeft;
    }
    effective &= ~Qt::AlignVertical_Mask;
    effective |= Qt::AlignTop;
    if (alignment_ == effective) {
      return;
    }
    alignment_ = effective;
    QLabel::setAlignment(alignment_);
  }

  TextColorMode colorMode() const
  {
    return colorMode_;
  }

  void setColorMode(TextColorMode mode)
  {
    colorMode_ = mode;
  }

  TextVisibilityMode visibilityMode() const
  {
    return visibilityMode_;
  }

  void setVisibilityMode(TextVisibilityMode mode)
  {
    visibilityMode_ = mode;
  }

  QString visibilityCalc() const
  {
    return visibilityCalc_;
  }

  void setVisibilityCalc(const QString &calc)
  {
    if (visibilityCalc_ == calc) {
      return;
    }
    visibilityCalc_ = calc;
  }

  QString channel(int index) const
  {
    if (index < 0 || index >= static_cast<int>(channels_.size())) {
      return QString();
    }
    return channels_[index];
  }

  void setChannel(int index, const QString &value)
  {
    if (index < 0 || index >= static_cast<int>(channels_.size())) {
      return;
    }
    if (channels_[index] == value) {
      return;
    }
    channels_[index] = value;
  }

private:
  void updateSelectionVisual()
  {
    applyTextColor();
    update();
  }

  QColor defaultForegroundColor() const
  {
    if (const QWidget *parent = parentWidget()) {
      return parent->palette().color(QPalette::WindowText);
    }
    if (qApp) {
      return qApp->palette().color(QPalette::WindowText);
    }
    return QColor(Qt::black);
  }

  bool selected_ = false;
  QColor foregroundColor_;
  void applyTextColor()
  {
    const QColor color = foregroundColor_.isValid()
        ? foregroundColor_
        : defaultForegroundColor();

    setStyleSheet(QString());

    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, color);
    pal.setColor(QPalette::Text, color);
    pal.setColor(QPalette::ButtonText, color);
    setPalette(pal);
  }

  void resizeEvent(QResizeEvent *event) override
  {
    QLabel::resizeEvent(event);
    updateFontForGeometry();
  }

  void updateFontForGeometry()
  {
    const QSize available = contentsRect().size();
    if (available.isEmpty()) {
      return;
    }

    const QFont newFont = medmCompatibleTextFont(text(), available);
    if (newFont.family().isEmpty()) {
      return;
    }

    if (font() != newFont) {
      QLabel::setFont(newFont);
    }
  }

  void paintEvent(QPaintEvent *event) override
  {
    QLabel::paintEvent(event);
    if (!selected_) {
      return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(Qt::black);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
  Qt::Alignment alignment_ = Qt::AlignLeft | Qt::AlignVCenter;
  TextColorMode colorMode_ = TextColorMode::kStatic;
  TextVisibilityMode visibilityMode_ = TextVisibilityMode::kStatic;
  QString visibilityCalc_;
  std::array<QString, 4> channels_{};
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
        if (state->createTool == CreateTool::kText) {
          if (displayArea_) {
            const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
            if (displayArea_->rect().contains(areaPos)) {
              clearTextSelection();
              startTextRubberBand(areaPos);
            }
          }
          event->accept();
          return;
        }
        if (state->createTool != CreateTool::kNone) {
          event->accept();
          return;
        }

        if (TextElement *element = textElementAt(event->pos())) {
          selectTextElement(element);
          showResourcePaletteForText(element);
          event->accept();
          return;
        }

        clearTextSelection();

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
    if (textDragActive_) {
      if (auto state = state_.lock(); state && state->editMode
          && state->createTool == CreateTool::kText && displayArea_) {
        const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
        updateTextRubberBand(areaPos);
        event->accept();
        return;
      }
    }

    QMainWindow::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() == Qt::LeftButton) {
      if (textDragActive_) {
        if (auto state = state_.lock(); state && state->editMode
            && displayArea_) {
          const QPoint areaPos = displayArea_->mapFrom(this, event->pos());
          finishTextRubberBand(areaPos);
          event->accept();
          return;
        }
      }
    }

    QMainWindow::mouseReleaseEvent(event);
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
  QRubberBand *rubberBand_ = nullptr;
  bool textDragActive_ = false;
  QPoint textDragOrigin_;

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

  void clearSelections()
  {
    clearDisplaySelection();
    clearTextSelection();
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
          element->setGeometry(adjustRectToDisplayArea(newGeometry));
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

  TextElement *textElementAt(const QPoint &windowPos) const
  {
    if (!displayArea_) {
      return nullptr;
    }
    const QPoint areaPos = displayArea_->mapFrom(this, windowPos);
    if (!displayArea_->rect().contains(areaPos)) {
      return nullptr;
    }
    for (auto it = textElements_.crbegin(); it != textElements_.crend(); ++it) {
      TextElement *element = *it;
      if (element && element->geometry().contains(areaPos)) {
        return element;
      }
    }
    return nullptr;
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
    selectedTextElement_ = element;
    selectedTextElement_->setSelected(true);
  }

  void startTextRubberBand(const QPoint &areaPos)
  {
    textDragActive_ = true;
    textDragOrigin_ = clampToDisplayArea(areaPos);
    ensureRubberBand();
    if (rubberBand_) {
      rubberBand_->setGeometry(QRect(textDragOrigin_, QSize(1, 1)));
      rubberBand_->show();
    }
  }

  void updateTextRubberBand(const QPoint &areaPos)
  {
    if (!textDragActive_ || !rubberBand_) {
      return;
    }
    const QPoint clamped = clampToDisplayArea(areaPos);
    rubberBand_->setGeometry(QRect(textDragOrigin_, clamped).normalized());
  }

  void finishTextRubberBand(const QPoint &areaPos)
  {
    if (!textDragActive_) {
      return;
    }
    textDragActive_ = false;
    if (rubberBand_) {
      rubberBand_->hide();
    }
    if (!displayArea_) {
      return;
    }
    const QPoint clamped = clampToDisplayArea(areaPos);
    QRect rect = QRect(textDragOrigin_, clamped).normalized();
    if (rect.width() < kMinimumTextWidth) {
      rect.setWidth(kMinimumTextWidth);
    }
    if (rect.height() < kMinimumTextHeight) {
      rect.setHeight(kMinimumTextHeight);
    }
    rect = adjustRectToDisplayArea(rect);
    createTextElement(rect);
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
    const bool textToolActive =
        state && state->createTool == CreateTool::kText;
    if (displayArea_) {
      if (textToolActive) {
        displayArea_->setCursor(Qt::CrossCursor);
      } else {
        displayArea_->unsetCursor();
      }
    }
    if (textToolActive) {
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
          display->clearSelections();
        }
      }
      state->createTool = tool;
      for (auto &display : state->displays) {
        if (!display.isNull()) {
          display->updateCreateCursor();
        }
      }
      textDragActive_ = false;
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
          display->updateCreateCursor();
        }
      }
    }
    textDragActive_ = false;
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
    addMenuAction(graphicsMenu, QStringLiteral("Rectangle"));
    addMenuAction(graphicsMenu, QStringLiteral("Line"));
    addMenuAction(graphicsMenu, QStringLiteral("Polygon"));
    addMenuAction(graphicsMenu, QStringLiteral("Polyline"));
    addMenuAction(graphicsMenu, QStringLiteral("Oval"));
    addMenuAction(graphicsMenu, QStringLiteral("Arc"));
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
