#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <cstddef>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>
#include <QWidget>

#include "resources/fonts/misc_fixed_10_otb.h"
#include "resources/fonts/misc_fixed_13_otb.h"

namespace {

QFont loadMiscFixedFont(const unsigned char *data, std::size_t size,
    int pixelSize)
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
        font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }

    font.setStyleHint(QFont::TypeWriter, QFont::PreferBitmap);
    font.setFixedPitch(true);
    font.setPixelSize(pixelSize);
    return font;
}

void showVersionDialog(QWidget *parent, const QFont &titleFont,
    const QFont &bodyFont, const QPalette &palette)
{
    auto *dialog = new QDialog(parent, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    dialog->setObjectName("qtedmVersionDialog");
    dialog->setWindowTitle(QStringLiteral("Version"));
    dialog->setModal(false);
    dialog->setAutoFillBackground(true);
    dialog->setPalette(palette);
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
    nameFrame->setPalette(palette);
    nameFrame->setBackgroundRole(QPalette::Button);

    auto *nameLayout = new QVBoxLayout(nameFrame);
    nameLayout->setContentsMargins(12, 8, 12, 8);
    nameLayout->setSpacing(0);

    auto *nameLabel = new QLabel(QStringLiteral("Q\nt\nE\nD\nM"), nameFrame);
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

    QObject::connect(okButton, &QPushButton::clicked, dialog, &QDialog::accept);
    QTimer::singleShot(5000, dialog, &QDialog::accept);

    dialog->adjustSize();
    dialog->setFixedSize(dialog->sizeHint());
    dialog->show();
}

} // namespace

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

    // Load the packaged Misc Fixed font so every widget matches the legacy MEDM
    // appearance.  Fall back to the system fixed font if the embedded data
    // cannot be registered for some reason.
    const QFont fixed10Font = loadMiscFixedFont(kMiscFixed10FontData,
        kMiscFixed10FontSize, 10);
    app.setFont(fixed10Font);

    const QFont fixed13Font = loadMiscFixedFont(kMiscFixed13FontData,
        kMiscFixed13FontSize, 13);

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
    fileMenu->addAction("&New");
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
    helpMenu->addAction("On &Version");

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
