#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <functional>

#include <QAbstractScrollArea>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QHash>
#include <QPalette>
#include <QPointer>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>

#include "color_palette_dialog.h"
#include "cartesian_axis_dialog.h"
#include "display_properties.h"
#include "pv_limits_dialog.h"

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
    QObject::connect(snapToGridCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (snapToGridSetter_) {
            snapToGridSetter_(index == 1);
          }
        });

    addRow(displayLayout, 0, QStringLiteral("Foreground"), foregroundButton_);
    addRow(displayLayout, 1, QStringLiteral("Background"), backgroundButton_);
    addRow(displayLayout, 2, QStringLiteral("Colormap"), colormapEdit_);
    addRow(displayLayout, 3, QStringLiteral("Grid Spacing"), gridSpacingEdit_);
    addRow(displayLayout, 4, QStringLiteral("Grid On"), gridOnCombo_);
    addRow(displayLayout, 5, QStringLiteral("Snap To Grid"), snapToGridCombo_);
    displayLayout->setRowStretch(6, 1);
    entriesLayout->addWidget(displaySection_);

    rectangleSection_ = new QWidget(entriesWidget_);
    auto *rectangleLayout = new QGridLayout(rectangleSection_);
    rectangleLayout->setContentsMargins(0, 0, 0, 0);
    rectangleLayout->setHorizontalSpacing(12);
    rectangleLayout->setVerticalSpacing(6);

    rectangleForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(rectangleForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(rectangleForegroundButton_,
              QStringLiteral("Rectangle Color"), rectangleForegroundSetter_);
        });

    rectangleFillCombo_ = new QComboBox;
    rectangleFillCombo_->setFont(valueFont_);
    rectangleFillCombo_->setAutoFillBackground(true);
    rectangleFillCombo_->addItem(QStringLiteral("Outline"));
    rectangleFillCombo_->addItem(QStringLiteral("Solid"));
    QObject::connect(rectangleFillCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (rectangleFillSetter_) {
            rectangleFillSetter_(fillFromIndex(index));
          }
        });

    rectangleLineStyleCombo_ = new QComboBox;
    rectangleLineStyleCombo_->setFont(valueFont_);
    rectangleLineStyleCombo_->setAutoFillBackground(true);
    rectangleLineStyleCombo_->addItem(QStringLiteral("Solid"));
    rectangleLineStyleCombo_->addItem(QStringLiteral("Dash"));
    QObject::connect(rectangleLineStyleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (rectangleLineStyleSetter_) {
            rectangleLineStyleSetter_(lineStyleFromIndex(index));
          }
        });

    rectangleLineWidthEdit_ = createLineEdit();
    committedTexts_.insert(rectangleLineWidthEdit_, rectangleLineWidthEdit_->text());
    rectangleLineWidthEdit_->installEventFilter(this);
    QObject::connect(rectangleLineWidthEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitRectangleLineWidth(); });
    QObject::connect(rectangleLineWidthEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitRectangleLineWidth(); });

    arcBeginSpin_ = new QSpinBox;
    arcBeginSpin_->setFont(valueFont_);
    arcBeginSpin_->setRange(-360, 360);
    arcBeginSpin_->setSingleStep(5);
    arcBeginSpin_->setAccelerated(true);
    QObject::connect(arcBeginSpin_,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [this](int value) {
          if (rectangleIsArc_ && arcBeginSetter_) {
            arcBeginSetter_(degreesToAngle64(value));
          }
        });

    arcPathSpin_ = new QSpinBox;
    arcPathSpin_->setFont(valueFont_);
    arcPathSpin_->setRange(-360, 360);
    arcPathSpin_->setSingleStep(5);
    arcPathSpin_->setAccelerated(true);
    QObject::connect(arcPathSpin_,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [this](int value) {
          if (rectangleIsArc_ && arcPathSetter_) {
            arcPathSetter_(degreesToAngle64(value));
          }
        });

    rectangleColorModeCombo_ = new QComboBox;
    rectangleColorModeCombo_->setFont(valueFont_);
    rectangleColorModeCombo_->setAutoFillBackground(true);
    rectangleColorModeCombo_->addItem(QStringLiteral("Static"));
    rectangleColorModeCombo_->addItem(QStringLiteral("Alarm"));
    rectangleColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(rectangleColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (rectangleColorModeSetter_) {
            rectangleColorModeSetter_(colorModeFromIndex(index));
          }
        });

    rectangleVisibilityCombo_ = new QComboBox;
    rectangleVisibilityCombo_->setFont(valueFont_);
    rectangleVisibilityCombo_->setAutoFillBackground(true);
    rectangleVisibilityCombo_->addItem(QStringLiteral("Static"));
    rectangleVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    rectangleVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    rectangleVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(rectangleVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (rectangleVisibilityModeSetter_) {
            rectangleVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
        });

    rectangleVisibilityCalcEdit_ = createLineEdit();
    QColor rectangleDisabledBackground = basePalette.color(QPalette::Disabled, QPalette::Base);
    if (!rectangleDisabledBackground.isValid()) {
      rectangleDisabledBackground = QColor(0xd3, 0xd3, 0xd3);
    }
    rectangleVisibilityCalcEdit_->setStyleSheet(
        QStringLiteral("QLineEdit:disabled { background-color: %1; }")
            .arg(rectangleDisabledBackground.name(QColor::HexRgb).toUpper()));
    committedTexts_.insert(rectangleVisibilityCalcEdit_, rectangleVisibilityCalcEdit_->text());
    rectangleVisibilityCalcEdit_->installEventFilter(this);
    QObject::connect(rectangleVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitRectangleVisibilityCalc(); });
    QObject::connect(rectangleVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitRectangleVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(rectangleChannelEdits_.size()); ++i) {
      rectangleChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(rectangleChannelEdits_[i], rectangleChannelEdits_[i]->text());
      rectangleChannelEdits_[i]->installEventFilter(this);
      QObject::connect(rectangleChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitRectangleChannel(i); });
      QObject::connect(rectangleChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitRectangleChannel(i); });
      if (i == 0) {
        QObject::connect(rectangleChannelEdits_[i], &QLineEdit::textChanged, this,
            [this]() { updateRectangleChannelDependentControls(); });
      }
    }
    updateRectangleChannelDependentControls();

    int rectangleRow = 0;
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Color"), rectangleForegroundButton_);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Fill"), rectangleFillCombo_);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Line Style"), rectangleLineStyleCombo_);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Line Width"), rectangleLineWidthEdit_);

    arcBeginLabel_ = new QLabel(QStringLiteral("Begin Angle"));
    arcBeginLabel_->setFont(labelFont_);
    arcBeginLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    arcBeginLabel_->setAutoFillBackground(false);
    rectangleLayout->addWidget(arcBeginLabel_, rectangleRow, 0);
    rectangleLayout->addWidget(arcBeginSpin_, rectangleRow, 1);
    ++rectangleRow;

    arcPathLabel_ = new QLabel(QStringLiteral("Path Length"));
    arcPathLabel_->setFont(labelFont_);
    arcPathLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    arcPathLabel_->setAutoFillBackground(false);
    rectangleLayout->addWidget(arcPathLabel_, rectangleRow, 0);

    rectangleLayout->addWidget(arcPathSpin_, rectangleRow, 1);
    ++rectangleRow;

    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Color Mode"), rectangleColorModeCombo_);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Visibility"), rectangleVisibilityCombo_);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Vis Calc"), rectangleVisibilityCalcEdit_);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Channel A"), rectangleChannelEdits_[0]);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Channel B"), rectangleChannelEdits_[1]);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Channel C"), rectangleChannelEdits_[2]);
    addRow(rectangleLayout, rectangleRow++, QStringLiteral("Channel D"), rectangleChannelEdits_[3]);
    rectangleLayout->setRowStretch(rectangleRow, 1);
    entriesLayout->addWidget(rectangleSection_);

    compositeSection_ = new QWidget(entriesWidget_);
    auto *compositeLayout = new QGridLayout(compositeSection_);
    compositeLayout->setContentsMargins(0, 0, 0, 0);
    compositeLayout->setHorizontalSpacing(12);
    compositeLayout->setVerticalSpacing(6);

    compositeForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(compositeForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(compositeForegroundButton_,
              QStringLiteral("Composite Foreground"),
              compositeForegroundSetter_);
        });
    compositeForegroundButton_->setEnabled(false);

    compositeBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(compositeBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(compositeBackgroundButton_,
              QStringLiteral("Composite Background"),
              compositeBackgroundSetter_);
        });
    compositeBackgroundButton_->setEnabled(false);

    compositeFileEdit_ = createLineEdit();
    committedTexts_.insert(compositeFileEdit_, compositeFileEdit_->text());
    compositeFileEdit_->installEventFilter(this);
    QObject::connect(compositeFileEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCompositeFile(); });
    QObject::connect(compositeFileEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCompositeFile(); });

    compositeVisibilityCombo_ = new QComboBox;
    compositeVisibilityCombo_->setFont(valueFont_);
    compositeVisibilityCombo_->setAutoFillBackground(true);
    compositeVisibilityCombo_->addItem(QStringLiteral("Static"));
    compositeVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    compositeVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    compositeVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(compositeVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (compositeVisibilityModeSetter_) {
            compositeVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
          updateCompositeChannelDependentControls();
        });

    compositeVisibilityCalcEdit_ = createLineEdit();
    QColor compositeDisabledBackground = basePalette.color(QPalette::Disabled, QPalette::Base);
    if (!compositeDisabledBackground.isValid()) {
      compositeDisabledBackground = QColor(0xd3, 0xd3, 0xd3);
    }
    compositeVisibilityCalcEdit_->setStyleSheet(
        QStringLiteral("QLineEdit:disabled { background-color: %1; }")
            .arg(compositeDisabledBackground.name(QColor::HexRgb).toUpper()));
    committedTexts_.insert(compositeVisibilityCalcEdit_, compositeVisibilityCalcEdit_->text());
    compositeVisibilityCalcEdit_->installEventFilter(this);
    QObject::connect(compositeVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCompositeVisibilityCalc(); });
    QObject::connect(compositeVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCompositeVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(compositeChannelEdits_.size()); ++i) {
      compositeChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(compositeChannelEdits_[i], compositeChannelEdits_[i]->text());
      compositeChannelEdits_[i]->installEventFilter(this);
      QObject::connect(compositeChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitCompositeChannel(i); });
      QObject::connect(compositeChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitCompositeChannel(i); });
      if (i == 0) {
        QObject::connect(compositeChannelEdits_[i], &QLineEdit::textChanged, this,
            [this]() { updateCompositeChannelDependentControls(); });
      }
    }
    updateCompositeChannelDependentControls();

    int compositeRow = 0;
    addRow(compositeLayout, compositeRow++, QStringLiteral("Color"), compositeForegroundButton_);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Background"), compositeBackgroundButton_);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Composite File"), compositeFileEdit_);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Visibility"), compositeVisibilityCombo_);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Vis Calc"), compositeVisibilityCalcEdit_);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Channel A"), compositeChannelEdits_[0]);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Channel B"), compositeChannelEdits_[1]);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Channel C"), compositeChannelEdits_[2]);
    addRow(compositeLayout, compositeRow++, QStringLiteral("Channel D"), compositeChannelEdits_[3]);
    compositeLayout->setRowStretch(compositeRow, 1);
    entriesLayout->addWidget(compositeSection_);

    imageSection_ = new QWidget(entriesWidget_);
    auto *imageLayout = new QGridLayout(imageSection_);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setHorizontalSpacing(12);
    imageLayout->setVerticalSpacing(6);

    imageTypeCombo_ = new QComboBox;
    imageTypeCombo_->setFont(valueFont_);
    imageTypeCombo_->setAutoFillBackground(true);
    imageTypeCombo_->addItem(QStringLiteral("None"));
    imageTypeCombo_->addItem(QStringLiteral("GIF"));
    imageTypeCombo_->addItem(QStringLiteral("TIFF"));
    QObject::connect(imageTypeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (imageTypeSetter_) {
            imageTypeSetter_(imageTypeFromIndex(index));
          }
        });

    imageNameEdit_ = createLineEdit();
    committedTexts_.insert(imageNameEdit_, imageNameEdit_->text());
    imageNameEdit_->installEventFilter(this);
    QObject::connect(imageNameEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitImageName(); });
    QObject::connect(imageNameEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitImageName(); });

    imageCalcEdit_ = createLineEdit();
    committedTexts_.insert(imageCalcEdit_, imageCalcEdit_->text());
    imageCalcEdit_->installEventFilter(this);
    QObject::connect(imageCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitImageCalc(); });
    QObject::connect(imageCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitImageCalc(); });

  imageColorModeCombo_ = new QComboBox(imageSection_);
    imageColorModeCombo_->setFont(valueFont_);
    imageColorModeCombo_->setAutoFillBackground(true);
    imageColorModeCombo_->addItem(QStringLiteral("Static"));
    imageColorModeCombo_->addItem(QStringLiteral("Alarm"));
    imageColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(imageColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (imageColorModeSetter_) {
            imageColorModeSetter_(colorModeFromIndex(index));
          }
        });
    imageColorModeCombo_->hide();

    imageVisibilityCombo_ = new QComboBox;
    imageVisibilityCombo_->setFont(valueFont_);
    imageVisibilityCombo_->setAutoFillBackground(true);
    imageVisibilityCombo_->addItem(QStringLiteral("Static"));
    imageVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    imageVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    imageVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(imageVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (imageVisibilityModeSetter_) {
            imageVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
        });

    imageVisibilityCalcEdit_ = createLineEdit();
    committedTexts_.insert(imageVisibilityCalcEdit_, imageVisibilityCalcEdit_->text());
    imageVisibilityCalcEdit_->installEventFilter(this);
    QObject::connect(imageVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitImageVisibilityCalc(); });
    QObject::connect(imageVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitImageVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(imageChannelEdits_.size()); ++i) {
      imageChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(imageChannelEdits_[i], imageChannelEdits_[i]->text());
      imageChannelEdits_[i]->installEventFilter(this);
      QObject::connect(imageChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitImageChannel(i); });
      QObject::connect(imageChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitImageChannel(i); });
      if (i == 0) {
        QObject::connect(imageChannelEdits_[i], &QLineEdit::textChanged, this,
            [this]() { updateImageChannelDependentControls(); });
      }
    }
    updateImageChannelDependentControls();

    int imageRow = 0;
    addRow(imageLayout, imageRow++, QStringLiteral("Image Type"), imageTypeCombo_);
    addRow(imageLayout, imageRow++, QStringLiteral("Image Name"), imageNameEdit_);
  addRow(imageLayout, imageRow++, QStringLiteral("Image Calc"), imageCalcEdit_);
    addRow(imageLayout, imageRow++, QStringLiteral("Visibility"), imageVisibilityCombo_);
    addRow(imageLayout, imageRow++, QStringLiteral("Vis Calc"), imageVisibilityCalcEdit_);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel A"), imageChannelEdits_[0]);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel B"), imageChannelEdits_[1]);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel C"), imageChannelEdits_[2]);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel D"), imageChannelEdits_[3]);
    imageLayout->setRowStretch(imageRow, 1);
    entriesLayout->addWidget(imageSection_);

    heatmapSection_ = new QWidget(entriesWidget_);
    auto *heatmapLayout = new QGridLayout(heatmapSection_);
    heatmapLayout->setContentsMargins(0, 0, 0, 0);
    heatmapLayout->setHorizontalSpacing(12);
    heatmapLayout->setVerticalSpacing(6);

    heatmapTitleEdit_ = createLineEdit();
    committedTexts_.insert(heatmapTitleEdit_, heatmapTitleEdit_->text());
    heatmapTitleEdit_->installEventFilter(this);
    QObject::connect(heatmapTitleEdit_, &QLineEdit::returnPressed, this,
      [this]() { commitHeatmapTitle(); });
    QObject::connect(heatmapTitleEdit_, &QLineEdit::editingFinished, this,
      [this]() { commitHeatmapTitle(); });

    heatmapDataChannelEdit_ = createLineEdit();
    committedTexts_.insert(heatmapDataChannelEdit_, heatmapDataChannelEdit_->text());
    heatmapDataChannelEdit_->installEventFilter(this);
    QObject::connect(heatmapDataChannelEdit_, &QLineEdit::returnPressed, this,
      [this]() { commitHeatmapDataChannel(); });
    QObject::connect(heatmapDataChannelEdit_, &QLineEdit::editingFinished, this,
      [this]() { commitHeatmapDataChannel(); });

    heatmapXSourceCombo_ = new QComboBox;
    heatmapXSourceCombo_->setFont(valueFont_);
    heatmapXSourceCombo_->setAutoFillBackground(true);
    heatmapXSourceCombo_->addItem(QStringLiteral("Static"));
    heatmapXSourceCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(heatmapXSourceCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapXSourceSetter_) {
        heatmapXSourceSetter_(heatmapDimensionSourceFromIndex(index));
        }
        updateHeatmapDimensionControls();
      });

    heatmapYSourceCombo_ = new QComboBox;
    heatmapYSourceCombo_->setFont(valueFont_);
    heatmapYSourceCombo_->setAutoFillBackground(true);
    heatmapYSourceCombo_->addItem(QStringLiteral("Static"));
    heatmapYSourceCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(heatmapYSourceCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapYSourceSetter_) {
        heatmapYSourceSetter_(heatmapDimensionSourceFromIndex(index));
        }
        updateHeatmapDimensionControls();
      });

    heatmapXDimEdit_ = createLineEdit();
    committedTexts_.insert(heatmapXDimEdit_, heatmapXDimEdit_->text());
    heatmapXDimEdit_->installEventFilter(this);
    QObject::connect(heatmapXDimEdit_, &QLineEdit::returnPressed, this,
      [this]() { commitHeatmapXDimension(); });
    QObject::connect(heatmapXDimEdit_, &QLineEdit::editingFinished, this,
      [this]() { commitHeatmapXDimension(); });

    heatmapYDimEdit_ = createLineEdit();
    committedTexts_.insert(heatmapYDimEdit_, heatmapYDimEdit_->text());
    heatmapYDimEdit_->installEventFilter(this);
    QObject::connect(heatmapYDimEdit_, &QLineEdit::returnPressed, this,
      [this]() { commitHeatmapYDimension(); });
    QObject::connect(heatmapYDimEdit_, &QLineEdit::editingFinished, this,
      [this]() { commitHeatmapYDimension(); });

    heatmapXDimChannelEdit_ = createLineEdit();
    committedTexts_.insert(heatmapXDimChannelEdit_, heatmapXDimChannelEdit_->text());
    heatmapXDimChannelEdit_->installEventFilter(this);
    QObject::connect(heatmapXDimChannelEdit_, &QLineEdit::returnPressed, this,
      [this]() { commitHeatmapXDimensionChannel(); });
    QObject::connect(heatmapXDimChannelEdit_, &QLineEdit::editingFinished, this,
      [this]() { commitHeatmapXDimensionChannel(); });

    heatmapYDimChannelEdit_ = createLineEdit();
    committedTexts_.insert(heatmapYDimChannelEdit_, heatmapYDimChannelEdit_->text());
    heatmapYDimChannelEdit_->installEventFilter(this);
    QObject::connect(heatmapYDimChannelEdit_, &QLineEdit::returnPressed, this,
      [this]() { commitHeatmapYDimensionChannel(); });
    QObject::connect(heatmapYDimChannelEdit_, &QLineEdit::editingFinished, this,
      [this]() { commitHeatmapYDimensionChannel(); });

    heatmapOrderCombo_ = new QComboBox;
    heatmapOrderCombo_->setFont(valueFont_);
    heatmapOrderCombo_->setAutoFillBackground(true);
    heatmapOrderCombo_->addItem(QStringLiteral("Row-major"));
    heatmapOrderCombo_->addItem(QStringLiteral("Column-major"));
    QObject::connect(heatmapOrderCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapOrderSetter_) {
        heatmapOrderSetter_(heatmapOrderFromIndex(index));
        }
      });

    int heatmapRow = 0;
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Title"), heatmapTitleEdit_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Data PV"), heatmapDataChannelEdit_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("X Dimension Source"), heatmapXSourceCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("X Dimension"), heatmapXDimEdit_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("X Dim PV"), heatmapXDimChannelEdit_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Y Dimension Source"), heatmapYSourceCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Y Dimension"), heatmapYDimEdit_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Y Dim PV"), heatmapYDimChannelEdit_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Order"), heatmapOrderCombo_);
    heatmapLayout->setRowStretch(heatmapRow, 1);
    updateHeatmapDimensionControls();
    entriesLayout->addWidget(heatmapSection_);

    lineSection_ = new QWidget(entriesWidget_);
    auto *lineLayout = new QGridLayout(lineSection_);
    lineLayout->setContentsMargins(0, 0, 0, 0);
    lineLayout->setHorizontalSpacing(12);
    lineLayout->setVerticalSpacing(6);

    lineColorButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(lineColorButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(lineColorButton_,
              QStringLiteral("Line Color"), lineColorSetter_);
        });

    lineLineStyleCombo_ = new QComboBox;
    lineLineStyleCombo_->setFont(valueFont_);
    lineLineStyleCombo_->setAutoFillBackground(true);
    lineLineStyleCombo_->addItem(QStringLiteral("Solid"));
    lineLineStyleCombo_->addItem(QStringLiteral("Dash"));
    QObject::connect(lineLineStyleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (lineLineStyleSetter_) {
            lineLineStyleSetter_(lineStyleFromIndex(index));
          }
        });

    lineLineWidthEdit_ = createLineEdit();
    committedTexts_.insert(lineLineWidthEdit_, lineLineWidthEdit_->text());
    lineLineWidthEdit_->installEventFilter(this);
    QObject::connect(lineLineWidthEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitLineLineWidth(); });
    QObject::connect(lineLineWidthEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitLineLineWidth(); });

    lineColorModeCombo_ = new QComboBox;
    lineColorModeCombo_->setFont(valueFont_);
    lineColorModeCombo_->setAutoFillBackground(true);
    lineColorModeCombo_->addItem(QStringLiteral("Static"));
    lineColorModeCombo_->addItem(QStringLiteral("Alarm"));
    lineColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(lineColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (lineColorModeSetter_) {
            lineColorModeSetter_(colorModeFromIndex(index));
          }
        });

    lineVisibilityCombo_ = new QComboBox;
    lineVisibilityCombo_->setFont(valueFont_);
    lineVisibilityCombo_->setAutoFillBackground(true);
    lineVisibilityCombo_->addItem(QStringLiteral("Static"));
    lineVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    lineVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    lineVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(lineVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (lineVisibilityModeSetter_) {
            lineVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
        });

    lineVisibilityCalcEdit_ = createLineEdit();
    QColor lineDisabledBackground = basePalette.color(QPalette::Disabled, QPalette::Base);
    if (!lineDisabledBackground.isValid()) {
      lineDisabledBackground = QColor(0xd3, 0xd3, 0xd3);
    }
    lineVisibilityCalcEdit_->setStyleSheet(
        QStringLiteral("QLineEdit:disabled { background-color: %1; }")
            .arg(lineDisabledBackground.name(QColor::HexRgb).toUpper()));
    committedTexts_.insert(lineVisibilityCalcEdit_, lineVisibilityCalcEdit_->text());
    lineVisibilityCalcEdit_->installEventFilter(this);
    QObject::connect(lineVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitLineVisibilityCalc(); });
    QObject::connect(lineVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitLineVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(lineChannelEdits_.size()); ++i) {
      lineChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(lineChannelEdits_[i], lineChannelEdits_[i]->text());
      lineChannelEdits_[i]->installEventFilter(this);
      QObject::connect(lineChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitLineChannel(i); });
      QObject::connect(lineChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitLineChannel(i); });
      if (i == 0) {
        QObject::connect(lineChannelEdits_[i], &QLineEdit::textChanged, this,
            [this]() { updateLineChannelDependentControls(); });
      }
    }

    updateLineChannelDependentControls();

    addRow(lineLayout, 0, QStringLiteral("Color"), lineColorButton_);
    addRow(lineLayout, 1, QStringLiteral("Line Style"), lineLineStyleCombo_);
    addRow(lineLayout, 2, QStringLiteral("Line Width"), lineLineWidthEdit_);
    addRow(lineLayout, 3, QStringLiteral("Color Mode"), lineColorModeCombo_);
    addRow(lineLayout, 4, QStringLiteral("Visibility"), lineVisibilityCombo_);
    addRow(lineLayout, 5, QStringLiteral("Vis Calc"), lineVisibilityCalcEdit_);
    addRow(lineLayout, 6, QStringLiteral("Channel A"), lineChannelEdits_[0]);
    addRow(lineLayout, 7, QStringLiteral("Channel B"), lineChannelEdits_[1]);
    addRow(lineLayout, 8, QStringLiteral("Channel C"), lineChannelEdits_[2]);
    addRow(lineLayout, 9, QStringLiteral("Channel D"), lineChannelEdits_[3]);
    lineLayout->setRowStretch(10, 1);
    entriesLayout->addWidget(lineSection_);

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
  QColor disabledBackground = basePalette.color(QPalette::Disabled, QPalette::Base);
  if (!disabledBackground.isValid()) {
    disabledBackground = QColor(0xd3, 0xd3, 0xd3);
  }
  textVisibilityCalcEdit_->setStyleSheet(
    QStringLiteral("QLineEdit:disabled { background-color: %1; }")
      .arg(disabledBackground.name(QColor::HexRgb).toUpper()));
    QObject::connect(textVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextVisibilityCalc(); });
    QObject::connect(textVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(textChannelEdits_.size()); ++i) {
      if (i >= kTextVisibleChannelCount) {
        textChannelEdits_[i] = nullptr;
        continue;
      }
      textChannelEdits_[i] = createLineEdit();
      QObject::connect(textChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitTextChannel(i); });
      QObject::connect(textChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitTextChannel(i); });
      if (i == kTextChannelAIndex) {
        QObject::connect(textChannelEdits_[i], &QLineEdit::textChanged, this,
            [this]() { updateTextChannelDependentControls(); });
      }
    }

    addRow(textLayout, 0, QStringLiteral("Text String"), textStringEdit_);
    addRow(textLayout, 1, QStringLiteral("Alignment"), textAlignmentCombo_);
    addRow(textLayout, 2, QStringLiteral("Foreground"), textForegroundButton_);
    addRow(textLayout, 3, QStringLiteral("Color Mode"), textColorModeCombo_);
    addRow(textLayout, 4, QStringLiteral("Visibility"), textVisibilityCombo_);
    addRow(textLayout, 5, QStringLiteral("Vis Calc"), textVisibilityCalcEdit_);
  addRow(textLayout, 6, QStringLiteral("Channel A"), textChannelEdits_[kTextChannelAIndex]);
  addRow(textLayout, 7, QStringLiteral("Channel B"), textChannelEdits_[kTextChannelBIndex]);
  addRow(textLayout, 8, QStringLiteral("Channel C"), textChannelEdits_[kTextChannelCIndex]);
  addRow(textLayout, 9, QStringLiteral("Channel D"), textChannelEdits_[kTextChannelDIndex]);
  textLayout->setRowStretch(10, 1);
  updateTextChannelDependentControls();
    entriesLayout->addWidget(textSection_);

    textMonitorSection_ = new QWidget(entriesWidget_);
    auto *textMonitorLayout = new QGridLayout(textMonitorSection_);
    textMonitorLayout->setContentsMargins(0, 0, 0, 0);
    textMonitorLayout->setHorizontalSpacing(12);
    textMonitorLayout->setVerticalSpacing(6);

    textMonitorForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(textMonitorForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textMonitorForegroundButton_,
              QStringLiteral("Text Monitor Foreground"),
              textMonitorForegroundSetter_);
        });

    textMonitorBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(textMonitorBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textMonitorBackgroundButton_,
              QStringLiteral("Text Monitor Background"),
              textMonitorBackgroundSetter_);
        });

    textMonitorAlignmentCombo_ = new QComboBox;
    textMonitorAlignmentCombo_->setFont(valueFont_);
    textMonitorAlignmentCombo_->setAutoFillBackground(true);
    textMonitorAlignmentCombo_->addItem(QStringLiteral("Left"));
    textMonitorAlignmentCombo_->addItem(QStringLiteral("Center"));
    textMonitorAlignmentCombo_->addItem(QStringLiteral("Right"));
    QObject::connect(textMonitorAlignmentCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textMonitorAlignmentSetter_) {
            textMonitorAlignmentSetter_(alignmentFromIndex(index));
          }
        });

    textMonitorFormatCombo_ = new QComboBox;
    textMonitorFormatCombo_->setFont(valueFont_);
    textMonitorFormatCombo_->setAutoFillBackground(true);
    textMonitorFormatCombo_->addItem(QStringLiteral("Decimal"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Exponential"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Engineering"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Compact"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Truncated"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Hexadecimal"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Octal"));
    textMonitorFormatCombo_->addItem(QStringLiteral("String"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Sexagesimal"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Sexagesimal (H:M:S)"));
    textMonitorFormatCombo_->addItem(QStringLiteral("Sexagesimal (D:M:S)"));
    QObject::connect(textMonitorFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textMonitorFormatSetter_) {
            textMonitorFormatSetter_(textMonitorFormatFromIndex(index));
          }
        });

    textMonitorColorModeCombo_ = new QComboBox;
    textMonitorColorModeCombo_->setFont(valueFont_);
    textMonitorColorModeCombo_->setAutoFillBackground(true);
    textMonitorColorModeCombo_->addItem(QStringLiteral("Static"));
    textMonitorColorModeCombo_->addItem(QStringLiteral("Alarm"));
    textMonitorColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(textMonitorColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textMonitorColorModeSetter_) {
            textMonitorColorModeSetter_(colorModeFromIndex(index));
          }
        });

    textMonitorChannelEdit_ = createLineEdit();
    committedTexts_.insert(textMonitorChannelEdit_, textMonitorChannelEdit_->text());
    textMonitorChannelEdit_->installEventFilter(this);
    QObject::connect(textMonitorChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextMonitorChannel(); });
    QObject::connect(textMonitorChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextMonitorChannel(); });

    textMonitorPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    textMonitorPvLimitsButton_->setEnabled(false);
    QObject::connect(textMonitorPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openTextMonitorPvLimitsDialog(); });

    addRow(textMonitorLayout, 0, QStringLiteral("Foreground"),
        textMonitorForegroundButton_);
    addRow(textMonitorLayout, 1, QStringLiteral("Background"),
        textMonitorBackgroundButton_);
    addRow(textMonitorLayout, 2, QStringLiteral("Alignment"),
        textMonitorAlignmentCombo_);
    addRow(textMonitorLayout, 3, QStringLiteral("Format"),
        textMonitorFormatCombo_);
  addRow(textMonitorLayout, 4, QStringLiteral("Color Mode"),
        textMonitorColorModeCombo_);
  addRow(textMonitorLayout, 5, QStringLiteral("Channel"),
        textMonitorChannelEdit_);
  addRow(textMonitorLayout, 6, QStringLiteral("Channel Limits"),
        textMonitorPvLimitsButton_);
  textMonitorLayout->setRowStretch(7, 1);
    entriesLayout->addWidget(textMonitorSection_);

    textEntrySection_ = new QWidget(entriesWidget_);
    auto *textEntryLayout = new QGridLayout(textEntrySection_);
    textEntryLayout->setContentsMargins(0, 0, 0, 0);
    textEntryLayout->setHorizontalSpacing(12);
    textEntryLayout->setVerticalSpacing(6);

    textEntryForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(textEntryForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textEntryForegroundButton_,
              QStringLiteral("Text Entry Foreground"),
              textEntryForegroundSetter_);
        });

    textEntryBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(textEntryBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textEntryBackgroundButton_,
              QStringLiteral("Text Entry Background"),
              textEntryBackgroundSetter_);
        });

    textEntryFormatCombo_ = new QComboBox;
    textEntryFormatCombo_->setFont(valueFont_);
    textEntryFormatCombo_->setAutoFillBackground(true);
    textEntryFormatCombo_->addItem(QStringLiteral("Decimal"));
    textEntryFormatCombo_->addItem(QStringLiteral("Exponential"));
    textEntryFormatCombo_->addItem(QStringLiteral("Engineering"));
    textEntryFormatCombo_->addItem(QStringLiteral("Compact"));
    textEntryFormatCombo_->addItem(QStringLiteral("Truncated"));
    textEntryFormatCombo_->addItem(QStringLiteral("Hexadecimal"));
    textEntryFormatCombo_->addItem(QStringLiteral("Octal"));
    textEntryFormatCombo_->addItem(QStringLiteral("String"));
    textEntryFormatCombo_->addItem(QStringLiteral("Sexagesimal"));
    textEntryFormatCombo_->addItem(QStringLiteral("Sexagesimal (H:M:S)"));
    textEntryFormatCombo_->addItem(QStringLiteral("Sexagesimal (D:M:S)"));
    QObject::connect(textEntryFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textEntryFormatSetter_) {
            textEntryFormatSetter_(textMonitorFormatFromIndex(index));
          }
        });

    textEntryColorModeCombo_ = new QComboBox;
    textEntryColorModeCombo_->setFont(valueFont_);
    textEntryColorModeCombo_->setAutoFillBackground(true);
    textEntryColorModeCombo_->addItem(QStringLiteral("Static"));
    textEntryColorModeCombo_->addItem(QStringLiteral("Alarm"));
    textEntryColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(textEntryColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textEntryColorModeSetter_) {
            textEntryColorModeSetter_(colorModeFromIndex(index));
          }
        });

    textEntryChannelEdit_ = createLineEdit();
    committedTexts_.insert(textEntryChannelEdit_, textEntryChannelEdit_->text());
    textEntryChannelEdit_->installEventFilter(this);
    QObject::connect(textEntryChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextEntryChannel(); });
    QObject::connect(textEntryChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextEntryChannel(); });

    textEntryPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    textEntryPvLimitsButton_->setEnabled(false);
    QObject::connect(textEntryPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openTextEntryPvLimitsDialog(); });

    addRow(textEntryLayout, 0, QStringLiteral("Foreground"),
        textEntryForegroundButton_);
    addRow(textEntryLayout, 1, QStringLiteral("Background"),
        textEntryBackgroundButton_);
    addRow(textEntryLayout, 2, QStringLiteral("Format"),
        textEntryFormatCombo_);
    addRow(textEntryLayout, 3, QStringLiteral("Color Mode"),
        textEntryColorModeCombo_);
    addRow(textEntryLayout, 4, QStringLiteral("Channel"),
        textEntryChannelEdit_);
    addRow(textEntryLayout, 5, QStringLiteral("Channel Limits"),
        textEntryPvLimitsButton_);
    textEntryLayout->setRowStretch(6, 1);
    entriesLayout->addWidget(textEntrySection_);

    sliderSection_ = new QWidget(entriesWidget_);
    auto *sliderLayout = new QGridLayout(sliderSection_);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setHorizontalSpacing(12);
    sliderLayout->setVerticalSpacing(6);

    sliderForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(sliderForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(sliderForegroundButton_,
              QStringLiteral("Slider Foreground"), sliderForegroundSetter_);
        });

    sliderBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(sliderBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(sliderBackgroundButton_,
              QStringLiteral("Slider Background"), sliderBackgroundSetter_);
        });

    sliderLabelCombo_ = new QComboBox;
    sliderLabelCombo_->setFont(valueFont_);
    sliderLabelCombo_->setAutoFillBackground(true);
    sliderLabelCombo_->addItem(QStringLiteral("None"));
    sliderLabelCombo_->addItem(QStringLiteral("No Decorations"));
    sliderLabelCombo_->addItem(QStringLiteral("Outline"));
    sliderLabelCombo_->addItem(QStringLiteral("Limits"));
    sliderLabelCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(sliderLabelCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (sliderLabelSetter_) {
            sliderLabelSetter_(meterLabelFromIndex(index));
          }
        });

    sliderColorModeCombo_ = new QComboBox;
    sliderColorModeCombo_->setFont(valueFont_);
    sliderColorModeCombo_->setAutoFillBackground(true);
    sliderColorModeCombo_->addItem(QStringLiteral("Static"));
    sliderColorModeCombo_->addItem(QStringLiteral("Alarm"));
    sliderColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(sliderColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (sliderColorModeSetter_) {
            sliderColorModeSetter_(colorModeFromIndex(index));
          }
        });

    sliderDirectionCombo_ = new QComboBox;
    sliderDirectionCombo_->setFont(valueFont_);
    sliderDirectionCombo_->setAutoFillBackground(true);
    sliderDirectionCombo_->addItem(QStringLiteral("Up"));
    sliderDirectionCombo_->addItem(QStringLiteral("Right"));
    sliderDirectionCombo_->addItem(QStringLiteral("Down"));
    sliderDirectionCombo_->addItem(QStringLiteral("Left"));
    QObject::connect(sliderDirectionCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (sliderDirectionSetter_) {
            sliderDirectionSetter_(barDirectionFromIndex(index));
          }
        });

    sliderIncrementEdit_ = createLineEdit();
    sliderIncrementEdit_->setValidator(new QDoubleValidator(
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::max(), 6, sliderIncrementEdit_));
    committedTexts_.insert(sliderIncrementEdit_, sliderIncrementEdit_->text());
    sliderIncrementEdit_->installEventFilter(this);
    QObject::connect(sliderIncrementEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitSliderIncrement(); });
    QObject::connect(sliderIncrementEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitSliderIncrement(); });

    sliderChannelEdit_ = createLineEdit();
    committedTexts_.insert(sliderChannelEdit_, sliderChannelEdit_->text());
    sliderChannelEdit_->installEventFilter(this);
    QObject::connect(sliderChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitSliderChannel(); });
    QObject::connect(sliderChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitSliderChannel(); });

    sliderPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    sliderPvLimitsButton_->setEnabled(false);
    QObject::connect(sliderPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openSliderPvLimitsDialog(); });

    addRow(sliderLayout, 0, QStringLiteral("Foreground"), sliderForegroundButton_);
    addRow(sliderLayout, 1, QStringLiteral("Background"), sliderBackgroundButton_);
    addRow(sliderLayout, 2, QStringLiteral("Label"), sliderLabelCombo_);
    addRow(sliderLayout, 3, QStringLiteral("Color Mode"), sliderColorModeCombo_);
    addRow(sliderLayout, 4, QStringLiteral("Direction"), sliderDirectionCombo_);
    addRow(sliderLayout, 5, QStringLiteral("Increment"), sliderIncrementEdit_);
    addRow(sliderLayout, 6, QStringLiteral("Channel"), sliderChannelEdit_);
    addRow(sliderLayout, 7, QStringLiteral("Channel Limits"), sliderPvLimitsButton_);
    sliderLayout->setRowStretch(8, 1);
    entriesLayout->addWidget(sliderSection_);

    wheelSwitchSection_ = new QWidget(entriesWidget_);
    auto *wheelLayout = new QGridLayout(wheelSwitchSection_);
    wheelLayout->setContentsMargins(0, 0, 0, 0);
    wheelLayout->setHorizontalSpacing(12);
    wheelLayout->setVerticalSpacing(6);

    wheelSwitchForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(wheelSwitchForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(wheelSwitchForegroundButton_,
              QStringLiteral("Wheel Switch Foreground"),
              wheelSwitchForegroundSetter_);
        });

    wheelSwitchBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(wheelSwitchBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(wheelSwitchBackgroundButton_,
              QStringLiteral("Wheel Switch Background"),
              wheelSwitchBackgroundSetter_);
        });

    wheelSwitchColorModeCombo_ = new QComboBox;
    wheelSwitchColorModeCombo_->setFont(valueFont_);
    wheelSwitchColorModeCombo_->setAutoFillBackground(true);
    wheelSwitchColorModeCombo_->addItem(QStringLiteral("Static"));
    wheelSwitchColorModeCombo_->addItem(QStringLiteral("Alarm"));
    wheelSwitchColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(wheelSwitchColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (wheelSwitchColorModeSetter_) {
            wheelSwitchColorModeSetter_(colorModeFromIndex(index));
          }
        });

    wheelSwitchPrecisionEdit_ = createLineEdit();
    wheelSwitchPrecisionEdit_->setValidator(new QDoubleValidator(
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::max(), 6, wheelSwitchPrecisionEdit_));
    committedTexts_.insert(wheelSwitchPrecisionEdit_, wheelSwitchPrecisionEdit_->text());
    wheelSwitchPrecisionEdit_->installEventFilter(this);
    QObject::connect(wheelSwitchPrecisionEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitWheelSwitchPrecision(); });
    QObject::connect(wheelSwitchPrecisionEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitWheelSwitchPrecision(); });

    wheelSwitchFormatEdit_ = createLineEdit();
    committedTexts_.insert(wheelSwitchFormatEdit_, wheelSwitchFormatEdit_->text());
    wheelSwitchFormatEdit_->installEventFilter(this);
    QObject::connect(wheelSwitchFormatEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitWheelSwitchFormat(); });
    QObject::connect(wheelSwitchFormatEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitWheelSwitchFormat(); });

    wheelSwitchChannelEdit_ = createLineEdit();
    committedTexts_.insert(wheelSwitchChannelEdit_, wheelSwitchChannelEdit_->text());
    wheelSwitchChannelEdit_->installEventFilter(this);
    QObject::connect(wheelSwitchChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitWheelSwitchChannel(); });
    QObject::connect(wheelSwitchChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitWheelSwitchChannel(); });

    wheelSwitchPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    wheelSwitchPvLimitsButton_->setEnabled(false);
    QObject::connect(wheelSwitchPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openWheelSwitchPvLimitsDialog(); });

    addRow(wheelLayout, 0, QStringLiteral("Foreground"),
        wheelSwitchForegroundButton_);
    addRow(wheelLayout, 1, QStringLiteral("Background"),
        wheelSwitchBackgroundButton_);
    addRow(wheelLayout, 2, QStringLiteral("Color Mode"),
        wheelSwitchColorModeCombo_);
    addRow(wheelLayout, 3, QStringLiteral("Precision"),
        wheelSwitchPrecisionEdit_);
    addRow(wheelLayout, 4, QStringLiteral("Format"), wheelSwitchFormatEdit_);
    addRow(wheelLayout, 5, QStringLiteral("Channel"), wheelSwitchChannelEdit_);
    addRow(wheelLayout, 6, QStringLiteral("Channel Limits"),
        wheelSwitchPvLimitsButton_);
    wheelLayout->setRowStretch(7, 1);
    entriesLayout->addWidget(wheelSwitchSection_);

    choiceButtonSection_ = new QWidget(entriesWidget_);
    auto *choiceLayout = new QGridLayout(choiceButtonSection_);
    choiceLayout->setContentsMargins(0, 0, 0, 0);
    choiceLayout->setHorizontalSpacing(12);
    choiceLayout->setVerticalSpacing(6);

    choiceButtonForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(choiceButtonForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(choiceButtonForegroundButton_,
              QStringLiteral("Choice Button Foreground"),
              choiceButtonForegroundSetter_);
        });

    choiceButtonBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(choiceButtonBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(choiceButtonBackgroundButton_,
              QStringLiteral("Choice Button Background"),
              choiceButtonBackgroundSetter_);
        });

    choiceButtonColorModeCombo_ = new QComboBox;
    choiceButtonColorModeCombo_->setFont(valueFont_);
    choiceButtonColorModeCombo_->setAutoFillBackground(true);
    choiceButtonColorModeCombo_->addItem(QStringLiteral("Static"));
    choiceButtonColorModeCombo_->addItem(QStringLiteral("Alarm"));
    choiceButtonColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(choiceButtonColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (choiceButtonColorModeSetter_) {
            choiceButtonColorModeSetter_(colorModeFromIndex(index));
          }
        });

    choiceButtonStackingCombo_ = new QComboBox;
    choiceButtonStackingCombo_->setFont(valueFont_);
    choiceButtonStackingCombo_->setAutoFillBackground(true);
    choiceButtonStackingCombo_->addItem(QStringLiteral("Row"));
    choiceButtonStackingCombo_->addItem(QStringLiteral("Column"));
    choiceButtonStackingCombo_->addItem(QStringLiteral("Row Column"));
    QObject::connect(choiceButtonStackingCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (choiceButtonStackingSetter_) {
            choiceButtonStackingSetter_(choiceButtonStackingFromIndex(index));
          }
        });

    choiceButtonChannelEdit_ = createLineEdit();
    committedTexts_.insert(choiceButtonChannelEdit_, choiceButtonChannelEdit_->text());
    choiceButtonChannelEdit_->installEventFilter(this);
    QObject::connect(choiceButtonChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitChoiceButtonChannel(); });
    QObject::connect(choiceButtonChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitChoiceButtonChannel(); });

    addRow(choiceLayout, 0, QStringLiteral("Foreground"),
        choiceButtonForegroundButton_);
    addRow(choiceLayout, 1, QStringLiteral("Background"),
        choiceButtonBackgroundButton_);
    addRow(choiceLayout, 2, QStringLiteral("Color Mode"),
        choiceButtonColorModeCombo_);
    addRow(choiceLayout, 3, QStringLiteral("Stacking"),
        choiceButtonStackingCombo_);
    addRow(choiceLayout, 4, QStringLiteral("Channel"),
        choiceButtonChannelEdit_);
    choiceLayout->setRowStretch(5, 1);
    entriesLayout->addWidget(choiceButtonSection_);

    menuSection_ = new QWidget(entriesWidget_);
    auto *menuLayout = new QGridLayout(menuSection_);
    menuLayout->setContentsMargins(0, 0, 0, 0);
    menuLayout->setHorizontalSpacing(12);
    menuLayout->setVerticalSpacing(6);

    menuForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(menuForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(menuForegroundButton_,
              QStringLiteral("Menu Foreground"), menuForegroundSetter_);
        });

    menuBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(menuBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(menuBackgroundButton_,
              QStringLiteral("Menu Background"), menuBackgroundSetter_);
        });

    menuColorModeCombo_ = new QComboBox;
    menuColorModeCombo_->setFont(valueFont_);
    menuColorModeCombo_->setAutoFillBackground(true);
    menuColorModeCombo_->addItem(QStringLiteral("Static"));
    menuColorModeCombo_->addItem(QStringLiteral("Alarm"));
    menuColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(menuColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (menuColorModeSetter_) {
            menuColorModeSetter_(colorModeFromIndex(index));
          }
        });

    menuChannelEdit_ = createLineEdit();
    committedTexts_.insert(menuChannelEdit_, menuChannelEdit_->text());
    menuChannelEdit_->installEventFilter(this);
    QObject::connect(menuChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitMenuChannel(); });
    QObject::connect(menuChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitMenuChannel(); });

    addRow(menuLayout, 0, QStringLiteral("Foreground"), menuForegroundButton_);
    addRow(menuLayout, 1, QStringLiteral("Background"), menuBackgroundButton_);
    addRow(menuLayout, 2, QStringLiteral("Color Mode"), menuColorModeCombo_);
    addRow(menuLayout, 3, QStringLiteral("Channel"), menuChannelEdit_);
    menuLayout->setRowStretch(4, 1);
    entriesLayout->addWidget(menuSection_);

    messageButtonSection_ = new QWidget(entriesWidget_);
    auto *messageButtonLayout = new QGridLayout(messageButtonSection_);
    messageButtonLayout->setContentsMargins(0, 0, 0, 0);
    messageButtonLayout->setHorizontalSpacing(12);
    messageButtonLayout->setVerticalSpacing(6);

    messageButtonForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(messageButtonForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(messageButtonForegroundButton_,
              QStringLiteral("Message Button Foreground"),
              messageButtonForegroundSetter_);
        });

    messageButtonBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(messageButtonBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(messageButtonBackgroundButton_,
              QStringLiteral("Message Button Background"),
              messageButtonBackgroundSetter_);
        });

    messageButtonColorModeCombo_ = new QComboBox;
    messageButtonColorModeCombo_->setFont(valueFont_);
    messageButtonColorModeCombo_->setAutoFillBackground(true);
    messageButtonColorModeCombo_->addItem(QStringLiteral("Static"));
    messageButtonColorModeCombo_->addItem(QStringLiteral("Alarm"));
    messageButtonColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(messageButtonColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (messageButtonColorModeSetter_) {
            messageButtonColorModeSetter_(colorModeFromIndex(index));
          }
        });

    messageButtonLabelEdit_ = createLineEdit();
    committedTexts_.insert(messageButtonLabelEdit_, messageButtonLabelEdit_->text());
    messageButtonLabelEdit_->installEventFilter(this);
    QObject::connect(messageButtonLabelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitMessageButtonLabel(); });
    QObject::connect(messageButtonLabelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitMessageButtonLabel(); });

    messageButtonPressEdit_ = createLineEdit();
    committedTexts_.insert(messageButtonPressEdit_, messageButtonPressEdit_->text());
    messageButtonPressEdit_->installEventFilter(this);
    QObject::connect(messageButtonPressEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitMessageButtonPressMessage(); });
    QObject::connect(messageButtonPressEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitMessageButtonPressMessage(); });

    messageButtonReleaseEdit_ = createLineEdit();
    committedTexts_.insert(messageButtonReleaseEdit_, messageButtonReleaseEdit_->text());
    messageButtonReleaseEdit_->installEventFilter(this);
    QObject::connect(messageButtonReleaseEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitMessageButtonReleaseMessage(); });
    QObject::connect(messageButtonReleaseEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitMessageButtonReleaseMessage(); });

    messageButtonChannelEdit_ = createLineEdit();
    committedTexts_.insert(messageButtonChannelEdit_, messageButtonChannelEdit_->text());
    messageButtonChannelEdit_->installEventFilter(this);
    QObject::connect(messageButtonChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitMessageButtonChannel(); });
    QObject::connect(messageButtonChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitMessageButtonChannel(); });

    addRow(messageButtonLayout, 0, QStringLiteral("Foreground"),
        messageButtonForegroundButton_);
    addRow(messageButtonLayout, 1, QStringLiteral("Background"),
        messageButtonBackgroundButton_);
    addRow(messageButtonLayout, 2, QStringLiteral("Color Mode"),
        messageButtonColorModeCombo_);
    addRow(messageButtonLayout, 3, QStringLiteral("Label"), messageButtonLabelEdit_);
    addRow(messageButtonLayout, 4, QStringLiteral("Press Message"),
        messageButtonPressEdit_);
    addRow(messageButtonLayout, 5, QStringLiteral("Release Message"),
        messageButtonReleaseEdit_);
    addRow(messageButtonLayout, 6, QStringLiteral("Channel"), messageButtonChannelEdit_);
    messageButtonLayout->setRowStretch(7, 1);
    entriesLayout->addWidget(messageButtonSection_);

    shellCommandSection_ = new QWidget(entriesWidget_);
    auto *shellCommandLayout = new QGridLayout(shellCommandSection_);
    shellCommandLayout->setContentsMargins(0, 0, 0, 0);
    shellCommandLayout->setHorizontalSpacing(12);
    shellCommandLayout->setVerticalSpacing(6);

    shellCommandForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(shellCommandForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(shellCommandForegroundButton_,
              QStringLiteral("Shell Command Foreground"),
              shellCommandForegroundSetter_);
        });

    shellCommandBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(shellCommandBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(shellCommandBackgroundButton_,
              QStringLiteral("Shell Command Background"),
              shellCommandBackgroundSetter_);
        });

    shellCommandLabelEdit_ = createLineEdit();
    committedTexts_.insert(shellCommandLabelEdit_, shellCommandLabelEdit_->text());
    shellCommandLabelEdit_->installEventFilter(this);
    QObject::connect(shellCommandLabelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitShellCommandLabel(); });
    QObject::connect(shellCommandLabelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitShellCommandLabel(); });

    shellCommandEntriesWidget_ = new QWidget(shellCommandSection_);
    auto *commandEntriesLayout = new QGridLayout(shellCommandEntriesWidget_);
    commandEntriesLayout->setContentsMargins(0, 0, 0, 0);
    commandEntriesLayout->setHorizontalSpacing(8);
    commandEntriesLayout->setVerticalSpacing(4);

    auto *shellEntryHeader = new QLabel(QStringLiteral("Entry"));
    shellEntryHeader->setFont(labelFont_);
    shellEntryHeader->setAlignment(Qt::AlignCenter);
    commandEntriesLayout->addWidget(shellEntryHeader, 0, 0);

    auto *shellLabelHeader = new QLabel(QStringLiteral("Label"));
    shellLabelHeader->setFont(labelFont_);
    shellLabelHeader->setAlignment(Qt::AlignCenter);
    commandEntriesLayout->addWidget(shellLabelHeader, 0, 1);

    auto *shellCommandHeader = new QLabel(QStringLiteral("Command"));
    shellCommandHeader->setFont(labelFont_);
    shellCommandHeader->setAlignment(Qt::AlignCenter);
    commandEntriesLayout->addWidget(shellCommandHeader, 0, 2);

    auto *shellArgsHeader = new QLabel(QStringLiteral("Arguments"));
    shellArgsHeader->setFont(labelFont_);
    shellArgsHeader->setAlignment(Qt::AlignCenter);
    commandEntriesLayout->addWidget(shellArgsHeader, 0, 3);

    for (int i = 0; i < kShellCommandEntryCount; ++i) {
      auto *rowLabel = new QLabel(QStringLiteral("%1").arg(i + 1));
      rowLabel->setFont(labelFont_);
      rowLabel->setAlignment(Qt::AlignCenter);
      commandEntriesLayout->addWidget(rowLabel, i + 1, 0);

      shellCommandEntryLabelEdits_[i] = createLineEdit();
      committedTexts_.insert(shellCommandEntryLabelEdits_[i],
          shellCommandEntryLabelEdits_[i]->text());
      shellCommandEntryLabelEdits_[i]->setMaximumWidth(160);
      shellCommandEntryLabelEdits_[i]->installEventFilter(this);
      QObject::connect(shellCommandEntryLabelEdits_[i], &QLineEdit::returnPressed,
          this, [this, i]() { commitShellCommandEntryLabel(i); });
      QObject::connect(shellCommandEntryLabelEdits_[i], &QLineEdit::editingFinished,
          this, [this, i]() { commitShellCommandEntryLabel(i); });
      commandEntriesLayout->addWidget(shellCommandEntryLabelEdits_[i], i + 1, 1);

      shellCommandEntryCommandEdits_[i] = createLineEdit();
      committedTexts_.insert(shellCommandEntryCommandEdits_[i],
          shellCommandEntryCommandEdits_[i]->text());
      shellCommandEntryCommandEdits_[i]->setMaximumWidth(200);
      shellCommandEntryCommandEdits_[i]->installEventFilter(this);
      QObject::connect(shellCommandEntryCommandEdits_[i], &QLineEdit::returnPressed,
          this, [this, i]() { commitShellCommandEntryCommand(i); });
      QObject::connect(shellCommandEntryCommandEdits_[i], &QLineEdit::editingFinished,
          this, [this, i]() { commitShellCommandEntryCommand(i); });
      commandEntriesLayout->addWidget(shellCommandEntryCommandEdits_[i], i + 1, 2);

      shellCommandEntryArgsEdits_[i] = createLineEdit();
      committedTexts_.insert(shellCommandEntryArgsEdits_[i],
          shellCommandEntryArgsEdits_[i]->text());
      shellCommandEntryArgsEdits_[i]->setMaximumWidth(200);
      shellCommandEntryArgsEdits_[i]->installEventFilter(this);
      QObject::connect(shellCommandEntryArgsEdits_[i], &QLineEdit::returnPressed,
          this, [this, i]() { commitShellCommandEntryArgs(i); });
      QObject::connect(shellCommandEntryArgsEdits_[i], &QLineEdit::editingFinished,
          this, [this, i]() { commitShellCommandEntryArgs(i); });
      commandEntriesLayout->addWidget(shellCommandEntryArgsEdits_[i], i + 1, 3);
    }

    commandEntriesLayout->setColumnStretch(1, 1);
    commandEntriesLayout->setColumnStretch(2, 1);
    commandEntriesLayout->setColumnStretch(3, 1);

    int shellRow = 0;
    addRow(shellCommandLayout, shellRow++, QStringLiteral("Foreground"),
        shellCommandForegroundButton_);
    addRow(shellCommandLayout, shellRow++, QStringLiteral("Background"),
        shellCommandBackgroundButton_);
    addRow(shellCommandLayout, shellRow++, QStringLiteral("Label"),
        shellCommandLabelEdit_);
    addRow(shellCommandLayout, shellRow++, QStringLiteral("Commands"),
        shellCommandEntriesWidget_);
    shellCommandLayout->setRowStretch(shellRow, 1);
    entriesLayout->addWidget(shellCommandSection_);

    relatedDisplaySection_ = new QWidget(entriesWidget_);
    auto *relatedLayout = new QGridLayout(relatedDisplaySection_);
    relatedLayout->setContentsMargins(0, 0, 0, 0);
    relatedLayout->setHorizontalSpacing(12);
    relatedLayout->setVerticalSpacing(6);

    relatedDisplayForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(relatedDisplayForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(relatedDisplayForegroundButton_,
              QStringLiteral("Related Display Foreground"),
              relatedDisplayForegroundSetter_);
        });

    relatedDisplayBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(relatedDisplayBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(relatedDisplayBackgroundButton_,
              QStringLiteral("Related Display Background"),
              relatedDisplayBackgroundSetter_);
        });

    relatedDisplayLabelEdit_ = createLineEdit();
    committedTexts_.insert(
        relatedDisplayLabelEdit_, relatedDisplayLabelEdit_->text());
    relatedDisplayLabelEdit_->installEventFilter(this);
    QObject::connect(relatedDisplayLabelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitRelatedDisplayLabel(); });
    QObject::connect(relatedDisplayLabelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitRelatedDisplayLabel(); });

    relatedDisplayVisualCombo_ = new QComboBox;
    relatedDisplayVisualCombo_->setFont(valueFont_);
    relatedDisplayVisualCombo_->setAutoFillBackground(true);
    relatedDisplayVisualCombo_->addItem(QStringLiteral("Menu"));
    relatedDisplayVisualCombo_->addItem(QStringLiteral("Row of Buttons"));
    relatedDisplayVisualCombo_->addItem(QStringLiteral("Column of Buttons"));
    relatedDisplayVisualCombo_->addItem(QStringLiteral("Hidden Button"));
    QObject::connect(relatedDisplayVisualCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (relatedDisplayVisualSetter_) {
            relatedDisplayVisualSetter_(relatedDisplayVisualFromIndex(index));
          }
        });

    relatedDisplayEntriesWidget_ = new QWidget(relatedDisplaySection_);
    auto *relatedEntriesLayout = new QGridLayout(relatedDisplayEntriesWidget_);
    relatedEntriesLayout->setContentsMargins(0, 0, 0, 0);
    relatedEntriesLayout->setHorizontalSpacing(8);
    relatedEntriesLayout->setVerticalSpacing(4);

    auto *relatedDisplayHeader = new QLabel(QStringLiteral("Display"));
    relatedDisplayHeader->setFont(labelFont_);
    relatedDisplayHeader->setAlignment(Qt::AlignCenter);
    relatedEntriesLayout->addWidget(relatedDisplayHeader, 0, 0);

    auto *relatedLabelHeader = new QLabel(QStringLiteral("Label"));
    relatedLabelHeader->setFont(labelFont_);
    relatedLabelHeader->setAlignment(Qt::AlignCenter);
    relatedEntriesLayout->addWidget(relatedLabelHeader, 0, 1);

    auto *relatedNameHeader = new QLabel(QStringLiteral("Name"));
    relatedNameHeader->setFont(labelFont_);
    relatedNameHeader->setAlignment(Qt::AlignCenter);
    relatedEntriesLayout->addWidget(relatedNameHeader, 0, 2);

    auto *relatedArgsHeader = new QLabel(QStringLiteral("Args"));
    relatedArgsHeader->setFont(labelFont_);
    relatedArgsHeader->setAlignment(Qt::AlignCenter);
    relatedEntriesLayout->addWidget(relatedArgsHeader, 0, 3);

    auto *relatedModeHeader = new QLabel(QStringLiteral("Policy"));
    relatedModeHeader->setFont(labelFont_);
    relatedModeHeader->setAlignment(Qt::AlignCenter);
    relatedEntriesLayout->addWidget(relatedModeHeader, 0, 4);

    for (int i = 0; i < kRelatedDisplayEntryCount; ++i) {
      auto *rowLabel = new QLabel(QStringLiteral("%1").arg(i + 1));
      rowLabel->setFont(labelFont_);
      rowLabel->setAlignment(Qt::AlignCenter);
      relatedEntriesLayout->addWidget(rowLabel, i + 1, 0);

      relatedDisplayEntryLabelEdits_[i] = createLineEdit();
      committedTexts_.insert(relatedDisplayEntryLabelEdits_[i],
          relatedDisplayEntryLabelEdits_[i]->text());
      relatedDisplayEntryLabelEdits_[i]->setMaximumWidth(160);
      relatedDisplayEntryLabelEdits_[i]->installEventFilter(this);
      QObject::connect(relatedDisplayEntryLabelEdits_[i], &QLineEdit::returnPressed,
          this, [this, i]() { commitRelatedDisplayEntryLabel(i); });
      QObject::connect(relatedDisplayEntryLabelEdits_[i], &QLineEdit::editingFinished,
          this, [this, i]() { commitRelatedDisplayEntryLabel(i); });
      relatedEntriesLayout->addWidget(relatedDisplayEntryLabelEdits_[i], i + 1, 1);

      relatedDisplayEntryNameEdits_[i] = createLineEdit();
      committedTexts_.insert(relatedDisplayEntryNameEdits_[i],
          relatedDisplayEntryNameEdits_[i]->text());
      relatedDisplayEntryNameEdits_[i]->setMaximumWidth(160);
      relatedDisplayEntryNameEdits_[i]->installEventFilter(this);
      QObject::connect(relatedDisplayEntryNameEdits_[i], &QLineEdit::returnPressed,
          this, [this, i]() { commitRelatedDisplayEntryName(i); });
      QObject::connect(relatedDisplayEntryNameEdits_[i], &QLineEdit::editingFinished,
          this, [this, i]() { commitRelatedDisplayEntryName(i); });
      relatedEntriesLayout->addWidget(relatedDisplayEntryNameEdits_[i], i + 1, 2);

      relatedDisplayEntryArgsEdits_[i] = createLineEdit();
      committedTexts_.insert(relatedDisplayEntryArgsEdits_[i],
          relatedDisplayEntryArgsEdits_[i]->text());
      relatedDisplayEntryArgsEdits_[i]->setMaximumWidth(160);
      relatedDisplayEntryArgsEdits_[i]->installEventFilter(this);
      QObject::connect(relatedDisplayEntryArgsEdits_[i], &QLineEdit::returnPressed,
          this, [this, i]() { commitRelatedDisplayEntryArgs(i); });
      QObject::connect(relatedDisplayEntryArgsEdits_[i], &QLineEdit::editingFinished,
          this, [this, i]() { commitRelatedDisplayEntryArgs(i); });
      relatedEntriesLayout->addWidget(relatedDisplayEntryArgsEdits_[i], i + 1, 3);

      relatedDisplayEntryModeCombos_[i] = new QComboBox;
      relatedDisplayEntryModeCombos_[i]->setFont(valueFont_);
      relatedDisplayEntryModeCombos_[i]->setAutoFillBackground(true);
      relatedDisplayEntryModeCombos_[i]->addItem(
          QStringLiteral("Add New Display"));
      relatedDisplayEntryModeCombos_[i]->addItem(
          QStringLiteral("Replace Display"));
      QObject::connect(relatedDisplayEntryModeCombos_[i],
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, [this, i](int index) {
            if (relatedDisplayEntryModeSetters_[i]) {
              relatedDisplayEntryModeSetters_[i](
                  relatedDisplayModeFromIndex(index));
            }
          });
      relatedEntriesLayout->addWidget(relatedDisplayEntryModeCombos_[i], i + 1, 4);
    }

    relatedEntriesLayout->setColumnStretch(1, 1);
    relatedEntriesLayout->setColumnStretch(2, 1);
    relatedEntriesLayout->setColumnStretch(3, 1);
    relatedEntriesLayout->setColumnStretch(4, 1);

    int relatedRow = 0;
    addRow(relatedLayout, relatedRow++, QStringLiteral("Foreground"),
        relatedDisplayForegroundButton_);
    addRow(relatedLayout, relatedRow++, QStringLiteral("Background"),
        relatedDisplayBackgroundButton_);
    addRow(relatedLayout, relatedRow++, QStringLiteral("Label"),
        relatedDisplayLabelEdit_);
    addRow(relatedLayout, relatedRow++, QStringLiteral("Visual"),
        relatedDisplayVisualCombo_);
    addRow(relatedLayout, relatedRow++, QStringLiteral("Displays"),
        relatedDisplayEntriesWidget_);
    relatedLayout->setRowStretch(relatedRow, 1);
    entriesLayout->addWidget(relatedDisplaySection_);

    meterSection_ = new QWidget(entriesWidget_);
    auto *meterLayout = new QGridLayout(meterSection_);
    meterLayout->setContentsMargins(0, 0, 0, 0);
    meterLayout->setHorizontalSpacing(12);
    meterLayout->setVerticalSpacing(6);

    meterForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(meterForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(meterForegroundButton_,
              QStringLiteral("Meter Foreground"), meterForegroundSetter_);
        });

    meterBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(meterBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(meterBackgroundButton_,
              QStringLiteral("Meter Background"), meterBackgroundSetter_);
        });

    meterLabelCombo_ = new QComboBox;
    meterLabelCombo_->setFont(valueFont_);
    meterLabelCombo_->setAutoFillBackground(true);
    meterLabelCombo_->addItem(QStringLiteral("None"));
    meterLabelCombo_->addItem(QStringLiteral("No Decorations"));
    meterLabelCombo_->addItem(QStringLiteral("Outline"));
    meterLabelCombo_->addItem(QStringLiteral("Limits"));
    meterLabelCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(meterLabelCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (meterLabelSetter_) {
            meterLabelSetter_(meterLabelFromIndex(index));
          }
        });

    meterColorModeCombo_ = new QComboBox;
    meterColorModeCombo_->setFont(valueFont_);
    meterColorModeCombo_->setAutoFillBackground(true);
    meterColorModeCombo_->addItem(QStringLiteral("Static"));
    meterColorModeCombo_->addItem(QStringLiteral("Alarm"));
    meterColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(meterColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (meterColorModeSetter_) {
            meterColorModeSetter_(colorModeFromIndex(index));
          }
        });

    meterChannelEdit_ = createLineEdit();
    committedTexts_.insert(meterChannelEdit_, meterChannelEdit_->text());
    meterChannelEdit_->installEventFilter(this);
    QObject::connect(meterChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitMeterChannel(); });
    QObject::connect(meterChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitMeterChannel(); });

    meterPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    meterPvLimitsButton_->setEnabled(false);
    QObject::connect(meterPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openMeterPvLimitsDialog(); });

    addRow(meterLayout, 0, QStringLiteral("Foreground"), meterForegroundButton_);
    addRow(meterLayout, 1, QStringLiteral("Background"), meterBackgroundButton_);
    addRow(meterLayout, 2, QStringLiteral("Label"), meterLabelCombo_);
    addRow(meterLayout, 3, QStringLiteral("Color Mode"), meterColorModeCombo_);
    addRow(meterLayout, 4, QStringLiteral("Channel"), meterChannelEdit_);
    addRow(meterLayout, 5, QStringLiteral("Channel Limits"), meterPvLimitsButton_);
    meterLayout->setRowStretch(6, 1);
    entriesLayout->addWidget(meterSection_);

    barSection_ = new QWidget(entriesWidget_);
    auto *barLayout = new QGridLayout(barSection_);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setHorizontalSpacing(12);
    barLayout->setVerticalSpacing(6);

    barForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(barForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(barForegroundButton_,
              QStringLiteral("Bar Monitor Foreground"),
              barForegroundSetter_);
        });

    barBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(barBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(barBackgroundButton_,
              QStringLiteral("Bar Monitor Background"),
              barBackgroundSetter_);
        });

    barLabelCombo_ = new QComboBox;
    barLabelCombo_->setFont(valueFont_);
    barLabelCombo_->setAutoFillBackground(true);
    barLabelCombo_->addItem(QStringLiteral("None"));
    barLabelCombo_->addItem(QStringLiteral("No Decorations"));
    barLabelCombo_->addItem(QStringLiteral("Outline"));
    barLabelCombo_->addItem(QStringLiteral("Limits"));
    barLabelCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(barLabelCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (barLabelSetter_) {
            barLabelSetter_(meterLabelFromIndex(index));
          }
        });

    barColorModeCombo_ = new QComboBox;
    barColorModeCombo_->setFont(valueFont_);
    barColorModeCombo_->setAutoFillBackground(true);
    barColorModeCombo_->addItem(QStringLiteral("Static"));
    barColorModeCombo_->addItem(QStringLiteral("Alarm"));
    barColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(barColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (barColorModeSetter_) {
            barColorModeSetter_(colorModeFromIndex(index));
          }
        });

    barDirectionCombo_ = new QComboBox;
    barDirectionCombo_->setFont(valueFont_);
    barDirectionCombo_->setAutoFillBackground(true);
    barDirectionCombo_->addItem(QStringLiteral("Up"));
    barDirectionCombo_->addItem(QStringLiteral("Right"));
    barDirectionCombo_->addItem(QStringLiteral("Down"));
    barDirectionCombo_->addItem(QStringLiteral("Left"));
    QObject::connect(barDirectionCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (barDirectionSetter_) {
            barDirectionSetter_(barDirectionFromIndex(index));
          }
        });

    barFillCombo_ = new QComboBox;
    barFillCombo_->setFont(valueFont_);
    barFillCombo_->setAutoFillBackground(true);
    barFillCombo_->addItem(QStringLiteral("From Edge"));
    barFillCombo_->addItem(QStringLiteral("From Center"));
    QObject::connect(barFillCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (barFillModeSetter_) {
            barFillModeSetter_(barFillFromIndex(index));
          }
        });

    barChannelEdit_ = createLineEdit();
    committedTexts_.insert(barChannelEdit_, barChannelEdit_->text());
    barChannelEdit_->installEventFilter(this);
    QObject::connect(barChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitBarChannel(); });
    QObject::connect(barChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitBarChannel(); });

    barPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    barPvLimitsButton_->setEnabled(false);
    QObject::connect(barPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openBarMonitorPvLimitsDialog(); });

    addRow(barLayout, 0, QStringLiteral("Foreground"), barForegroundButton_);
    addRow(barLayout, 1, QStringLiteral("Background"), barBackgroundButton_);
    addRow(barLayout, 2, QStringLiteral("Label"), barLabelCombo_);
    addRow(barLayout, 3, QStringLiteral("Color Mode"), barColorModeCombo_);
    addRow(barLayout, 4, QStringLiteral("Direction"), barDirectionCombo_);
    addRow(barLayout, 5, QStringLiteral("Fill Mode"), barFillCombo_);
    addRow(barLayout, 6, QStringLiteral("Channel"), barChannelEdit_);
    addRow(barLayout, 7, QStringLiteral("Channel Limits"), barPvLimitsButton_);
    barLayout->setRowStretch(8, 1);
    entriesLayout->addWidget(barSection_);

    scaleSection_ = new QWidget(entriesWidget_);
    auto *scaleLayout = new QGridLayout(scaleSection_);
    scaleLayout->setContentsMargins(0, 0, 0, 0);
    scaleLayout->setHorizontalSpacing(12);
    scaleLayout->setVerticalSpacing(6);

    scaleForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(scaleForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(scaleForegroundButton_,
              QStringLiteral("Scale Monitor Foreground"),
              scaleForegroundSetter_);
        });

    scaleBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(scaleBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(scaleBackgroundButton_,
              QStringLiteral("Scale Monitor Background"),
              scaleBackgroundSetter_);
        });

    scaleLabelCombo_ = new QComboBox;
    scaleLabelCombo_->setFont(valueFont_);
    scaleLabelCombo_->setAutoFillBackground(true);
    scaleLabelCombo_->addItem(QStringLiteral("None"));
    scaleLabelCombo_->addItem(QStringLiteral("No Decorations"));
    scaleLabelCombo_->addItem(QStringLiteral("Outline"));
    scaleLabelCombo_->addItem(QStringLiteral("Limits"));
    scaleLabelCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(scaleLabelCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (scaleLabelSetter_) {
            scaleLabelSetter_(meterLabelFromIndex(index));
          }
        });

    scaleColorModeCombo_ = new QComboBox;
    scaleColorModeCombo_->setFont(valueFont_);
    scaleColorModeCombo_->setAutoFillBackground(true);
    scaleColorModeCombo_->addItem(QStringLiteral("Static"));
    scaleColorModeCombo_->addItem(QStringLiteral("Alarm"));
    scaleColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(scaleColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (scaleColorModeSetter_) {
            scaleColorModeSetter_(colorModeFromIndex(index));
          }
        });

    scaleDirectionCombo_ = new QComboBox;
    scaleDirectionCombo_->setFont(valueFont_);
    scaleDirectionCombo_->setAutoFillBackground(true);
    scaleDirectionCombo_->addItem(QStringLiteral("Up"));
    scaleDirectionCombo_->addItem(QStringLiteral("Right"));
    QObject::connect(scaleDirectionCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (scaleDirectionSetter_) {
            scaleDirectionSetter_(scaleDirectionFromIndex(index));
          }
        });

    scaleChannelEdit_ = createLineEdit();
    committedTexts_.insert(scaleChannelEdit_, scaleChannelEdit_->text());
    scaleChannelEdit_->installEventFilter(this);
    QObject::connect(scaleChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitScaleChannel(); });
    QObject::connect(scaleChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitScaleChannel(); });

    scalePvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    scalePvLimitsButton_->setEnabled(false);
    QObject::connect(scalePvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openScaleMonitorPvLimitsDialog(); });

    addRow(scaleLayout, 0, QStringLiteral("Foreground"), scaleForegroundButton_);
    addRow(scaleLayout, 1, QStringLiteral("Background"), scaleBackgroundButton_);
    addRow(scaleLayout, 2, QStringLiteral("Label"), scaleLabelCombo_);
    addRow(scaleLayout, 3, QStringLiteral("Color Mode"), scaleColorModeCombo_);
    addRow(scaleLayout, 4, QStringLiteral("Direction"), scaleDirectionCombo_);
    addRow(scaleLayout, 5, QStringLiteral("Channel"), scaleChannelEdit_);
    addRow(scaleLayout, 6, QStringLiteral("Channel Limits"), scalePvLimitsButton_);
    scaleLayout->setRowStretch(7, 1);
    entriesLayout->addWidget(scaleSection_);

    stripChartSection_ = new QWidget(entriesWidget_);
    auto *stripLayout = new QGridLayout(stripChartSection_);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->setHorizontalSpacing(12);
    stripLayout->setVerticalSpacing(6);

    stripTitleEdit_ = createLineEdit();
    committedTexts_.insert(stripTitleEdit_, stripTitleEdit_->text());
    stripTitleEdit_->installEventFilter(this);
    QObject::connect(stripTitleEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitStripChartTitle(); });
    QObject::connect(stripTitleEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitStripChartTitle(); });

    stripXLabelEdit_ = createLineEdit();
    committedTexts_.insert(stripXLabelEdit_, stripXLabelEdit_->text());
    stripXLabelEdit_->installEventFilter(this);
    QObject::connect(stripXLabelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitStripChartXLabel(); });
    QObject::connect(stripXLabelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitStripChartXLabel(); });

    stripYLabelEdit_ = createLineEdit();
    committedTexts_.insert(stripYLabelEdit_, stripYLabelEdit_->text());
    stripYLabelEdit_->installEventFilter(this);
    QObject::connect(stripYLabelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitStripChartYLabel(); });
    QObject::connect(stripYLabelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitStripChartYLabel(); });

    stripForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(stripForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(stripForegroundButton_,
              QStringLiteral("Strip Chart Foreground"),
              stripForegroundSetter_);
        });

    stripBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(stripBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(stripBackgroundButton_,
              QStringLiteral("Strip Chart Background"),
              stripBackgroundSetter_);
        });

    stripPeriodEdit_ = createLineEdit();
    stripPeriodEdit_->setValidator(
        new QDoubleValidator(0.001, 1.0e9, 3, stripPeriodEdit_));
    committedTexts_.insert(stripPeriodEdit_, stripPeriodEdit_->text());
    stripPeriodEdit_->installEventFilter(this);
    QObject::connect(stripPeriodEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitStripChartPeriod(); });
    QObject::connect(stripPeriodEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitStripChartPeriod(); });

    stripUnitsCombo_ = new QComboBox;
    stripUnitsCombo_->setFont(valueFont_);
    stripUnitsCombo_->setAutoFillBackground(true);
    stripUnitsCombo_->addItem(QStringLiteral("Milliseconds"));
    stripUnitsCombo_->addItem(QStringLiteral("Seconds"));
    stripUnitsCombo_->addItem(QStringLiteral("Minutes"));
    QObject::connect(stripUnitsCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) { handleStripChartUnitsChanged(index); });

    auto *penWidget = new QWidget(stripChartSection_);
    auto *penLayout = new QGridLayout(penWidget);
    penLayout->setContentsMargins(0, 0, 0, 0);
    penLayout->setHorizontalSpacing(8);
    penLayout->setVerticalSpacing(4);

    for (int i = 0; i < kStripChartPenCount; ++i) {
      auto *label = new QLabel(QStringLiteral("Pen %1").arg(i + 1));
      label->setFont(labelFont_);
      penLayout->addWidget(label, i, 0);

      stripPenColorButtons_[i] = createColorButton(
          basePalette.color(QPalette::WindowText));
      QObject::connect(stripPenColorButtons_[i], &QPushButton::clicked, this,
          [this, i]() {
            openColorPalette(stripPenColorButtons_[i],
                QStringLiteral("Strip Chart Pen %1 Color").arg(i + 1),
                stripPenColorSetters_[i]);
          });
      penLayout->addWidget(stripPenColorButtons_[i], i, 1);

      stripPenChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(stripPenChannelEdits_[i],
          stripPenChannelEdits_[i]->text());
      stripPenChannelEdits_[i]->installEventFilter(this);
      QObject::connect(stripPenChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitStripChartChannel(i); });
      QObject::connect(stripPenChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitStripChartChannel(i); });
      penLayout->addWidget(stripPenChannelEdits_[i], i, 2);

      stripPenLimitsButtons_[i] = createActionButton(
          QStringLiteral("Limits..."));
      stripPenLimitsButtons_[i]->setEnabled(false);
      QObject::connect(stripPenLimitsButtons_[i], &QPushButton::clicked, this,
          [this, i]() { openStripChartLimitsDialog(i); });
      penLayout->addWidget(stripPenLimitsButtons_[i], i, 3);
    }

    addRow(stripLayout, 0, QStringLiteral("Title"), stripTitleEdit_);
    addRow(stripLayout, 1, QStringLiteral("X Label"), stripXLabelEdit_);
    addRow(stripLayout, 2, QStringLiteral("Y Label"), stripYLabelEdit_);
    addRow(stripLayout, 3, QStringLiteral("Foreground"),
        stripForegroundButton_);
    addRow(stripLayout, 4, QStringLiteral("Background"),
        stripBackgroundButton_);
    addRow(stripLayout, 5, QStringLiteral("Period"), stripPeriodEdit_);
    addRow(stripLayout, 6, QStringLiteral("Units"), stripUnitsCombo_);
    addRow(stripLayout, 7, QStringLiteral("Pens"), penWidget);
    stripLayout->setRowStretch(8, 1);
    entriesLayout->addWidget(stripChartSection_);

    cartesianSection_ = new QWidget(entriesWidget_);
    auto *cartesianLayout = new QGridLayout(cartesianSection_);
    cartesianLayout->setContentsMargins(0, 0, 0, 0);
    cartesianLayout->setHorizontalSpacing(12);
    cartesianLayout->setVerticalSpacing(6);

    cartesianTitleEdit_ = createLineEdit();
    committedTexts_.insert(cartesianTitleEdit_, cartesianTitleEdit_->text());
    cartesianTitleEdit_->installEventFilter(this);
    QObject::connect(cartesianTitleEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCartesianTitle(); });
    QObject::connect(cartesianTitleEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCartesianTitle(); });

    cartesianXLabelEdit_ = createLineEdit();
    committedTexts_.insert(cartesianXLabelEdit_, cartesianXLabelEdit_->text());
    cartesianXLabelEdit_->installEventFilter(this);
    QObject::connect(cartesianXLabelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCartesianXLabel(); });
    QObject::connect(cartesianXLabelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCartesianXLabel(); });

    for (int i = 0; i < 4; ++i) {
      cartesianYLabelEdits_[i] = createLineEdit();
      committedTexts_.insert(cartesianYLabelEdits_[i],
          cartesianYLabelEdits_[i]->text());
      cartesianYLabelEdits_[i]->installEventFilter(this);
      QObject::connect(cartesianYLabelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitCartesianYLabel(i); });
      QObject::connect(cartesianYLabelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitCartesianYLabel(i); });
    }

    cartesianForegroundButton_ = createColorButton(
        palette().color(QPalette::WindowText));
    QObject::connect(cartesianForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(cartesianForegroundButton_,
              QStringLiteral("Cartesian Foreground"),
              cartesianForegroundSetter_);
        });

    cartesianBackgroundButton_ = createColorButton(
        palette().color(QPalette::Window));
    QObject::connect(cartesianBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(cartesianBackgroundButton_,
              QStringLiteral("Cartesian Background"),
              cartesianBackgroundSetter_);
        });

    cartesianStyleCombo_ = new QComboBox;
    cartesianStyleCombo_->setFont(valueFont_);
    cartesianStyleCombo_->setAutoFillBackground(true);
    cartesianStyleCombo_->addItem(QStringLiteral("Point Plot"));
    cartesianStyleCombo_->addItem(QStringLiteral("Line Plot"));
    cartesianStyleCombo_->addItem(QStringLiteral("Step Plot"));
    cartesianStyleCombo_->addItem(QStringLiteral("Fill Under"));
    QObject::connect(cartesianStyleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) { handleCartesianStyleChanged(index); });

    cartesianEraseOldestCombo_ = createBooleanComboBox();
    QObject::connect(cartesianEraseOldestCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) { handleCartesianEraseOldestChanged(index); });

    cartesianCountEdit_ = createLineEdit();
    cartesianCountEdit_->setValidator(new QIntValidator(1, 100000,
        cartesianCountEdit_));
    committedTexts_.insert(cartesianCountEdit_, cartesianCountEdit_->text());
    cartesianCountEdit_->installEventFilter(this);
    QObject::connect(cartesianCountEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCartesianCount(); });
    QObject::connect(cartesianCountEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCartesianCount(); });

    cartesianEraseModeCombo_ = new QComboBox;
    cartesianEraseModeCombo_->setFont(valueFont_);
    cartesianEraseModeCombo_->setAutoFillBackground(true);
    cartesianEraseModeCombo_->addItem(QStringLiteral("If Not Zero"));
    cartesianEraseModeCombo_->addItem(QStringLiteral("If Zero"));
    QObject::connect(cartesianEraseModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) { handleCartesianEraseModeChanged(index); });

    cartesianTriggerEdit_ = createLineEdit();
    committedTexts_.insert(cartesianTriggerEdit_, cartesianTriggerEdit_->text());
    cartesianTriggerEdit_->installEventFilter(this);
    QObject::connect(cartesianTriggerEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCartesianTrigger(); });
    QObject::connect(cartesianTriggerEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCartesianTrigger(); });

    cartesianEraseEdit_ = createLineEdit();
    committedTexts_.insert(cartesianEraseEdit_, cartesianEraseEdit_->text());
    cartesianEraseEdit_->installEventFilter(this);
    QObject::connect(cartesianEraseEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCartesianErase(); });
    QObject::connect(cartesianEraseEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCartesianErase(); });

    cartesianCountPvEdit_ = createLineEdit();
    committedTexts_.insert(cartesianCountPvEdit_, cartesianCountPvEdit_->text());
    cartesianCountPvEdit_->installEventFilter(this);
    QObject::connect(cartesianCountPvEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitCartesianCountPv(); });
    QObject::connect(cartesianCountPvEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitCartesianCountPv(); });

    auto *cartesianTraceWidget = new QWidget(cartesianSection_);
    auto *cartesianTraceLayout = new QGridLayout(cartesianTraceWidget);
    cartesianTraceLayout->setContentsMargins(0, 0, 0, 0);
    cartesianTraceLayout->setHorizontalSpacing(8);
    cartesianTraceLayout->setVerticalSpacing(4);

    for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
      auto *traceLabel = new QLabel(QStringLiteral("Trace %1").arg(i + 1));
      traceLabel->setFont(labelFont_);
      cartesianTraceLayout->addWidget(traceLabel, i, 0);

      cartesianTraceColorButtons_[i] = createColorButton(
          palette().color(QPalette::WindowText));
      QObject::connect(cartesianTraceColorButtons_[i], &QPushButton::clicked, this,
          [this, i]() {
            openColorPalette(cartesianTraceColorButtons_[i],
                QStringLiteral("Trace Color"), cartesianTraceColorSetters_[i]);
          });
      cartesianTraceLayout->addWidget(cartesianTraceColorButtons_[i], i, 1);

      cartesianTraceXEdits_[i] = createLineEdit();
      committedTexts_.insert(cartesianTraceXEdits_[i],
          cartesianTraceXEdits_[i]->text());
      cartesianTraceXEdits_[i]->installEventFilter(this);
      QObject::connect(cartesianTraceXEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitCartesianTraceXChannel(i); });
      QObject::connect(cartesianTraceXEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitCartesianTraceXChannel(i); });
      cartesianTraceLayout->addWidget(cartesianTraceXEdits_[i], i, 2);

      cartesianTraceYEdits_[i] = createLineEdit();
      committedTexts_.insert(cartesianTraceYEdits_[i],
          cartesianTraceYEdits_[i]->text());
      cartesianTraceYEdits_[i]->installEventFilter(this);
      QObject::connect(cartesianTraceYEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitCartesianTraceYChannel(i); });
      QObject::connect(cartesianTraceYEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitCartesianTraceYChannel(i); });
      cartesianTraceLayout->addWidget(cartesianTraceYEdits_[i], i, 3);

      cartesianTraceAxisCombos_[i] = new QComboBox;
      cartesianTraceAxisCombos_[i]->setFont(valueFont_);
      cartesianTraceAxisCombos_[i]->setAutoFillBackground(true);
      cartesianTraceAxisCombos_[i]->addItem(QStringLiteral("Y1"));
      cartesianTraceAxisCombos_[i]->addItem(QStringLiteral("Y2"));
      cartesianTraceAxisCombos_[i]->addItem(QStringLiteral("Y3"));
      cartesianTraceAxisCombos_[i]->addItem(QStringLiteral("Y4"));
      QObject::connect(cartesianTraceAxisCombos_[i],
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, [this, i](int index) { handleCartesianTraceAxisChanged(i, index); });
      cartesianTraceLayout->addWidget(cartesianTraceAxisCombos_[i], i, 4);

      cartesianTraceSideCombos_[i] = new QComboBox;
      cartesianTraceSideCombos_[i]->setFont(valueFont_);
      cartesianTraceSideCombos_[i]->setAutoFillBackground(true);
      cartesianTraceSideCombos_[i]->addItem(QStringLiteral("Left"));
      cartesianTraceSideCombos_[i]->addItem(QStringLiteral("Right"));
      QObject::connect(cartesianTraceSideCombos_[i],
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, [this, i](int index) { handleCartesianTraceSideChanged(i, index); });
      cartesianTraceLayout->addWidget(cartesianTraceSideCombos_[i], i, 5);
    }

    addRow(cartesianLayout, 0, QStringLiteral("Title"), cartesianTitleEdit_);
    addRow(cartesianLayout, 1, QStringLiteral("X Label"), cartesianXLabelEdit_);
    addRow(cartesianLayout, 2, QStringLiteral("Y1 Label"), cartesianYLabelEdits_[0]);
    addRow(cartesianLayout, 3, QStringLiteral("Y2 Label"), cartesianYLabelEdits_[1]);
    addRow(cartesianLayout, 4, QStringLiteral("Y3 Label"), cartesianYLabelEdits_[2]);
    addRow(cartesianLayout, 5, QStringLiteral("Y4 Label"), cartesianYLabelEdits_[3]);
    addRow(cartesianLayout, 6, QStringLiteral("Foreground"), cartesianForegroundButton_);
    addRow(cartesianLayout, 7, QStringLiteral("Background"), cartesianBackgroundButton_);
    addRow(cartesianLayout, 8, QStringLiteral("Style"), cartesianStyleCombo_);
    addRow(cartesianLayout, 9, QStringLiteral("Erase Oldest"), cartesianEraseOldestCombo_);
    addRow(cartesianLayout, 10, QStringLiteral("Count"), cartesianCountEdit_);
    addRow(cartesianLayout, 11, QStringLiteral("Erase Mode"), cartesianEraseModeCombo_);
    addRow(cartesianLayout, 12, QStringLiteral("Trigger"), cartesianTriggerEdit_);
    addRow(cartesianLayout, 13, QStringLiteral("Erase"), cartesianEraseEdit_);
    addRow(cartesianLayout, 14, QStringLiteral("Count PV"), cartesianCountPvEdit_);

    cartesianAxisButton_ = createActionButton(QStringLiteral("Axis Data..."));
    cartesianAxisButton_->setEnabled(false);
    QObject::connect(cartesianAxisButton_, &QPushButton::clicked, this,
        [this]() { openCartesianAxisDialog(); });

    addRow(cartesianLayout, 15, QStringLiteral("Axis Data"), cartesianAxisButton_);
    addRow(cartesianLayout, 16, QStringLiteral("Traces"), cartesianTraceWidget);
    cartesianLayout->setRowStretch(17, 1);
    entriesLayout->addWidget(cartesianSection_);

    byteSection_ = new QWidget(entriesWidget_);
    auto *byteLayout = new QGridLayout(byteSection_);
    byteLayout->setContentsMargins(0, 0, 0, 0);
    byteLayout->setHorizontalSpacing(12);
    byteLayout->setVerticalSpacing(6);

    byteForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(byteForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(byteForegroundButton_,
              QStringLiteral("Byte Monitor Foreground"),
              byteForegroundSetter_);
        });

    byteBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(byteBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(byteBackgroundButton_,
              QStringLiteral("Byte Monitor Background"),
              byteBackgroundSetter_);
        });

    byteColorModeCombo_ = new QComboBox;
    byteColorModeCombo_->setFont(valueFont_);
    byteColorModeCombo_->setAutoFillBackground(true);
    byteColorModeCombo_->addItem(QStringLiteral("Static"));
    byteColorModeCombo_->addItem(QStringLiteral("Alarm"));
    byteColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(byteColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (byteColorModeSetter_) {
            byteColorModeSetter_(colorModeFromIndex(index));
          }
        });

    byteDirectionCombo_ = new QComboBox;
    byteDirectionCombo_->setFont(valueFont_);
    byteDirectionCombo_->setAutoFillBackground(true);
    byteDirectionCombo_->addItem(QStringLiteral("Up"));
    byteDirectionCombo_->addItem(QStringLiteral("Right"));
    byteDirectionCombo_->addItem(QStringLiteral("Down"));
    byteDirectionCombo_->addItem(QStringLiteral("Left"));
    QObject::connect(byteDirectionCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (byteDirectionSetter_) {
            byteDirectionSetter_(barDirectionFromIndex(index));
          }
        });

    byteStartBitSpin_ = new QSpinBox;
    byteStartBitSpin_->setFont(valueFont_);
    byteStartBitSpin_->setAutoFillBackground(true);
    QPalette byteSpinPalette = palette();
    byteSpinPalette.setColor(QPalette::Base, Qt::white);
    byteSpinPalette.setColor(QPalette::Text, Qt::black);
    byteStartBitSpin_->setPalette(byteSpinPalette);
    byteStartBitSpin_->setRange(0, 31);
    QObject::connect(byteStartBitSpin_,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [this](int value) { commitByteStartBit(value); });

    byteEndBitSpin_ = new QSpinBox;
    byteEndBitSpin_->setFont(valueFont_);
    byteEndBitSpin_->setAutoFillBackground(true);
    byteEndBitSpin_->setPalette(byteSpinPalette);
    byteEndBitSpin_->setRange(0, 31);
    QObject::connect(byteEndBitSpin_,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [this](int value) { commitByteEndBit(value); });

    byteChannelEdit_ = createLineEdit();
    committedTexts_.insert(byteChannelEdit_, byteChannelEdit_->text());
    byteChannelEdit_->installEventFilter(this);
    QObject::connect(byteChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitByteChannel(); });
    QObject::connect(byteChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitByteChannel(); });

    addRow(byteLayout, 0, QStringLiteral("Foreground"), byteForegroundButton_);
    addRow(byteLayout, 1, QStringLiteral("Background"), byteBackgroundButton_);
    addRow(byteLayout, 2, QStringLiteral("Color Mode"), byteColorModeCombo_);
    addRow(byteLayout, 3, QStringLiteral("Direction"), byteDirectionCombo_);
    addRow(byteLayout, 4, QStringLiteral("Start Bit"), byteStartBitSpin_);
    addRow(byteLayout, 5, QStringLiteral("End Bit"), byteEndBitSpin_);
    addRow(byteLayout, 6, QStringLiteral("Channel"), byteChannelEdit_);
    byteLayout->setRowStretch(7, 1);
    entriesLayout->addWidget(byteSection_);

    entriesLayout->addStretch(1);

  displaySection_->setVisible(false);
  rectangleSection_->setVisible(false);
  compositeSection_->setVisible(false);
  imageSection_->setVisible(false);
  heatmapSection_->setVisible(false);
  lineSection_->setVisible(false);
  textSection_->setVisible(false);
  textEntrySection_->setVisible(false);
  textMonitorSection_->setVisible(false);
  meterSection_->setVisible(false);
  barSection_->setVisible(false);
  scaleSection_->setVisible(false);
  byteSection_->setVisible(false);
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
      std::function<void(bool)> gridOnSetter,
      std::function<bool()> snapToGridGetter,
      std::function<void(bool)> snapToGridSetter)
  {
    clearSelectionState();
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
    snapToGridGetter_ = std::move(snapToGridGetter);
    snapToGridSetter_ = std::move(snapToGridSetter);
    textGetter_ = {};
    textSetter_ = {};
    committedTextString_.clear();
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textMonitorLimitsGetter_ = {};
    textMonitorLimitsSetter_ = {};
    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }
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
    if (snapToGridCombo_) {
      const QSignalBlocker blocker(snapToGridCombo_);
      const bool snap = snapToGridGetter_ ? snapToGridGetter_()
                                          : kDefaultSnapToGrid;
      snapToGridCombo_->setCurrentIndex(snap ? 1 : 0);
    }

    elementLabel_->setText(QStringLiteral("Display"));

    showPaletteWithoutActivating();
  }

  void showForMultipleSelection()
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kNone;
    updateSectionVisibility(selectionKind_);

    auto clearLineEdit = [&](QLineEdit *edit) {
      if (!edit) {
        return;
      }
      const QSignalBlocker blocker(edit);
      edit->clear();
    };

    clearLineEdit(xEdit_);
    clearLineEdit(yEdit_);
    clearLineEdit(widthEdit_);
    clearLineEdit(heightEdit_);
    clearLineEdit(gridSpacingEdit_);
    lastCommittedGeometry_ = QRect();
    updateCommittedTexts();

    showPaletteWithoutActivating();
  }

  void refreshGeometryFromSelection()
  {
    if (!isVisible() || !geometryGetter_) {
      return;
    }
    QRect geometry = geometryGetter_();
    if (!geometry.isValid()) {
      return;
    }
    if (geometry == lastCommittedGeometry_) {
      return;
    }
    lastCommittedGeometry_ = geometry;
    updateGeometryEdits(geometry);
  }

  void refreshDisplayControls()
  {
    if (selectionKind_ != SelectionKind::kDisplay) {
      return;
    }
    if (gridSpacingEdit_ && gridSpacingGetter_) {
      const QSignalBlocker blocker(gridSpacingEdit_);
      const int spacing = gridSpacingGetter_ ? gridSpacingGetter_()
                                             : kDefaultGridSpacing;
      gridSpacingEdit_->setText(
          QString::number(std::max(kMinimumGridSpacing, spacing)));
      committedTexts_[gridSpacingEdit_] = gridSpacingEdit_->text();
    }
    if (gridOnCombo_ && gridOnGetter_) {
      const QSignalBlocker blocker(gridOnCombo_);
      const bool gridOn = gridOnGetter_();
      gridOnCombo_->setCurrentIndex(gridOn ? 1 : 0);
    }
    if (snapToGridCombo_ && snapToGridGetter_) {
      const QSignalBlocker blocker(snapToGridCombo_);
      const bool snap = snapToGridGetter_();
      snapToGridCombo_->setCurrentIndex(snap ? 1 : 0);
    }
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
  std::array<std::function<QString()>, 5> channelGetters,
  std::array<std::function<void(const QString &)>, 5> channelSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kText;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    foregroundColorGetter_ = {};
    foregroundColorSetter_ = {};
    backgroundColorGetter_ = {};
    backgroundColorSetter_ = {};
    activeColorSetter_ = {};
    compositeForegroundGetter_ = {};
    compositeForegroundSetter_ = {};
    compositeBackgroundGetter_ = {};
    compositeBackgroundSetter_ = {};
    compositeFileGetter_ = {};
    compositeFileSetter_ = {};
    compositeVisibilityModeGetter_ = {};
    compositeVisibilityModeSetter_ = {};
    compositeVisibilityCalcGetter_ = {};
    compositeVisibilityCalcSetter_ = {};
    for (auto &getter : compositeChannelGetters_) {
      getter = {};
    }
    for (auto &setter : compositeChannelSetters_) {
      setter = {};
    }
    gridSpacingGetter_ = {};
    gridSpacingSetter_ = {};
    gridOnGetter_ = {};
    gridOnSetter_ = {};
    snapToGridGetter_ = {};
    snapToGridSetter_ = {};
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    QRect textGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    bool geometryAdjusted = false;
    if (textGeometry.width() <= 0) {
      textGeometry.setWidth(kMinimumTextWidth);
      geometryAdjusted = true;
    }
    if (textGeometry.height() < kMinimumTextElementHeight) {
      textGeometry.setHeight(kMinimumTextElementHeight);
      geometryAdjusted = true;
    }
    if (geometryAdjusted && geometrySetter_) {
      geometrySetter_(textGeometry);
      textGeometry = geometryGetter_ ? geometryGetter_() : textGeometry;
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
      QString value =
          textChannelGetters_[i] ? textChannelGetters_[i]() : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      committedTexts_[edit] = edit->text();
    }

    updateTextChannelDependentControls();

    elementLabel_->setText(QStringLiteral("Text"));

    showPaletteWithoutActivating();
  }

  void showForTextEntry(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kTextEntry;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textEntryForegroundGetter_ = std::move(foregroundGetter);
    textEntryForegroundSetter_ = std::move(foregroundSetter);
    textEntryBackgroundGetter_ = std::move(backgroundGetter);
    textEntryBackgroundSetter_ = std::move(backgroundSetter);
    textEntryFormatGetter_ = std::move(formatGetter);
    textEntryFormatSetter_ = std::move(formatSetter);
    textEntryPrecisionGetter_ = std::move(precisionGetter);
    textEntryPrecisionSetter_ = std::move(precisionSetter);
    textEntryPrecisionSourceGetter_ = std::move(precisionSourceGetter);
    textEntryPrecisionSourceSetter_ = std::move(precisionSourceSetter);
    textEntryPrecisionDefaultGetter_ = std::move(precisionDefaultGetter);
    textEntryPrecisionDefaultSetter_ = std::move(precisionDefaultSetter);
    textEntryColorModeGetter_ = std::move(colorModeGetter);
    textEntryColorModeSetter_ = std::move(colorModeSetter);
    textEntryChannelGetter_ = std::move(channelGetter);
    textEntryChannelSetter_ = std::move(channelSetter);
    textEntryLimitsGetter_ = std::move(limitsGetter);
    textEntryLimitsSetter_ = std::move(limitsSetter);

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    QRect entryGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (entryGeometry.width() <= 0) {
      entryGeometry.setWidth(kMinimumTextWidth);
    }
    if (entryGeometry.height() <= 0) {
      entryGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = entryGeometry;

    updateGeometryEdits(entryGeometry);

    if (textEntryForegroundButton_) {
      const QColor color = textEntryForegroundGetter_ ? textEntryForegroundGetter_()
                                                     : palette().color(QPalette::WindowText);
      setColorButtonColor(textEntryForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (textEntryBackgroundButton_) {
      const QColor color = textEntryBackgroundGetter_ ? textEntryBackgroundGetter_()
                                                     : palette().color(QPalette::Window);
      setColorButtonColor(textEntryBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (textEntryFormatCombo_) {
      const QSignalBlocker blocker(textEntryFormatCombo_);
      const int index = textEntryFormatGetter_
          ? textMonitorFormatToIndex(textEntryFormatGetter_())
          : textMonitorFormatToIndex(TextMonitorFormat::kDecimal);
      textEntryFormatCombo_->setCurrentIndex(index);
    }

    if (textEntryColorModeCombo_) {
      const QSignalBlocker blocker(textEntryColorModeCombo_);
      const int index = textEntryColorModeGetter_
          ? colorModeToIndex(textEntryColorModeGetter_())
          : colorModeToIndex(TextColorMode::kStatic);
      textEntryColorModeCombo_->setCurrentIndex(index);
    }

    if (textEntryChannelEdit_) {
      const QString channel = textEntryChannelGetter_ ? textEntryChannelGetter_()
                                                      : QString();
      const QSignalBlocker blocker(textEntryChannelEdit_);
      textEntryChannelEdit_->setText(channel);
      committedTexts_[textEntryChannelEdit_] = textEntryChannelEdit_->text();
    }

    if (textEntryPvLimitsButton_) {
      textEntryPvLimitsButton_->setEnabled(
          static_cast<bool>(textEntryPrecisionSourceGetter_)
          && static_cast<bool>(textEntryPrecisionSourceSetter_)
          && static_cast<bool>(textEntryPrecisionDefaultGetter_)
          && static_cast<bool>(textEntryPrecisionDefaultSetter_)
          && static_cast<bool>(textEntryLimitsGetter_)
          && static_cast<bool>(textEntryLimitsSetter_));
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Text Entry"));
    }

    showPaletteWithoutActivating();
  }

  void showForSlider(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<double()> incrementGetter,
      std::function<void(double)> incrementSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kSlider;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textEntryForegroundGetter_ = {};
    textEntryForegroundSetter_ = {};
    textEntryBackgroundGetter_ = {};
    textEntryBackgroundSetter_ = {};
    textEntryFormatGetter_ = {};
    textEntryFormatSetter_ = {};
    textEntryPrecisionGetter_ = {};
    textEntryPrecisionSetter_ = {};
    textEntryPrecisionSourceGetter_ = {};
    textEntryPrecisionSourceSetter_ = {};
    textEntryPrecisionDefaultGetter_ = {};
    textEntryPrecisionDefaultSetter_ = {};
    textEntryColorModeGetter_ = {};
    textEntryColorModeSetter_ = {};
    textEntryChannelGetter_ = {};
    textEntryChannelSetter_ = {};
    textEntryLimitsGetter_ = {};
    textEntryLimitsSetter_ = {};

    sliderForegroundGetter_ = std::move(foregroundGetter);
    sliderForegroundSetter_ = std::move(foregroundSetter);
    sliderBackgroundGetter_ = std::move(backgroundGetter);
    sliderBackgroundSetter_ = std::move(backgroundSetter);
    sliderLabelGetter_ = std::move(labelGetter);
    sliderLabelSetter_ = std::move(labelSetter);
    sliderColorModeGetter_ = std::move(colorModeGetter);
    sliderColorModeSetter_ = std::move(colorModeSetter);
    sliderDirectionGetter_ = std::move(directionGetter);
    sliderDirectionSetter_ = std::move(directionSetter);
    sliderIncrementGetter_ = std::move(incrementGetter);
    sliderIncrementSetter_ = std::move(incrementSetter);
    sliderChannelGetter_ = std::move(channelGetter);
    sliderChannelSetter_ = std::move(channelSetter);
    sliderLimitsGetter_ = std::move(limitsGetter);
    sliderLimitsSetter_ = std::move(limitsSetter);

    QRect entryGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (entryGeometry.width() <= 0) {
      entryGeometry.setWidth(kMinimumSliderWidth);
    }
    if (entryGeometry.height() <= 0) {
      entryGeometry.setHeight(kMinimumSliderHeight);
    }
    lastCommittedGeometry_ = entryGeometry;

    updateGeometryEdits(entryGeometry);

    if (sliderForegroundButton_) {
      const QColor color = sliderForegroundGetter_ ? sliderForegroundGetter_()
                                                  : palette().color(QPalette::WindowText);
      setColorButtonColor(sliderForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (sliderBackgroundButton_) {
      const QColor color = sliderBackgroundGetter_ ? sliderBackgroundGetter_()
                                                  : palette().color(QPalette::Window);
      setColorButtonColor(sliderBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (sliderLabelCombo_) {
      const QSignalBlocker blocker(sliderLabelCombo_);
      const MeterLabel label = sliderLabelGetter_ ? sliderLabelGetter_()
                                                 : MeterLabel::kOutline;
      sliderLabelCombo_->setCurrentIndex(meterLabelToIndex(label));
    }

    if (sliderColorModeCombo_) {
      const QSignalBlocker blocker(sliderColorModeCombo_);
      const TextColorMode mode = sliderColorModeGetter_ ? sliderColorModeGetter_()
                                                       : TextColorMode::kStatic;
      sliderColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (sliderDirectionCombo_) {
      const QSignalBlocker blocker(sliderDirectionCombo_);
      const BarDirection direction = sliderDirectionGetter_ ? sliderDirectionGetter_()
                                                            : BarDirection::kRight;
      sliderDirectionCombo_->setCurrentIndex(barDirectionToIndex(direction));
    }

    updateSliderIncrementEdit();

    if (sliderChannelEdit_) {
      const QString channel = sliderChannelGetter_ ? sliderChannelGetter_()
                                                  : QString();
      const QSignalBlocker blocker(sliderChannelEdit_);
      sliderChannelEdit_->setText(channel);
      committedTexts_[sliderChannelEdit_] = sliderChannelEdit_->text();
    }

    if (sliderPvLimitsButton_) {
      sliderPvLimitsButton_->setEnabled(static_cast<bool>(sliderLimitsGetter_)
          && static_cast<bool>(sliderLimitsSetter_));
    }

    updateSliderLimitsFromDialog();

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Slider"));
    }

    showPaletteWithoutActivating();
  }

  void showForWheelSwitch(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> formatGetter,
      std::function<void(const QString &)> formatSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kWheelSwitch;
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
    sliderForegroundGetter_ = {};
    sliderForegroundSetter_ = {};
    sliderBackgroundGetter_ = {};
    sliderBackgroundSetter_ = {};
    sliderLabelGetter_ = {};
    sliderLabelSetter_ = {};
    sliderColorModeGetter_ = {};
    sliderColorModeSetter_ = {};
    sliderDirectionGetter_ = {};
    sliderDirectionSetter_ = {};
    sliderIncrementGetter_ = {};
    sliderIncrementSetter_ = {};
    sliderChannelGetter_ = {};
    sliderChannelSetter_ = {};
    sliderLimitsGetter_ = {};
    sliderLimitsSetter_ = {};
    wheelSwitchForegroundGetter_ = {};
    wheelSwitchForegroundSetter_ = {};
    wheelSwitchBackgroundGetter_ = {};
    wheelSwitchBackgroundSetter_ = {};
    wheelSwitchColorModeGetter_ = {};
    wheelSwitchColorModeSetter_ = {};
    wheelSwitchPrecisionGetter_ = {};
    wheelSwitchPrecisionSetter_ = {};
    wheelSwitchFormatGetter_ = {};
    wheelSwitchFormatSetter_ = {};
    wheelSwitchChannelGetter_ = {};
    wheelSwitchChannelSetter_ = {};
    wheelSwitchLimitsGetter_ = {};
    wheelSwitchLimitsSetter_ = {};

    wheelSwitchForegroundGetter_ = std::move(foregroundGetter);
    wheelSwitchForegroundSetter_ = std::move(foregroundSetter);
    wheelSwitchBackgroundGetter_ = std::move(backgroundGetter);
    wheelSwitchBackgroundSetter_ = std::move(backgroundSetter);
    wheelSwitchColorModeGetter_ = std::move(colorModeGetter);
    wheelSwitchColorModeSetter_ = std::move(colorModeSetter);
    wheelSwitchFormatGetter_ = std::move(formatGetter);
    wheelSwitchFormatSetter_ = std::move(formatSetter);
    wheelSwitchChannelGetter_ = std::move(channelGetter);
    wheelSwitchChannelSetter_ = std::move(channelSetter);
    wheelSwitchLimitsGetter_ = std::move(limitsGetter);
    wheelSwitchLimitsSetter_ = std::move(limitsSetter);

    QRect entryGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (entryGeometry.width() <= 0) {
      entryGeometry.setWidth(kMinimumWheelSwitchWidth);
    }
    if (entryGeometry.height() <= 0) {
      entryGeometry.setHeight(kMinimumWheelSwitchHeight);
    }
    lastCommittedGeometry_ = entryGeometry;

    updateGeometryEdits(entryGeometry);

    if (wheelSwitchForegroundButton_) {
      const QColor color = wheelSwitchForegroundGetter_ ? wheelSwitchForegroundGetter_()
                                                       : palette().color(QPalette::WindowText);
      setColorButtonColor(wheelSwitchForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (wheelSwitchBackgroundButton_) {
      const QColor color = wheelSwitchBackgroundGetter_ ? wheelSwitchBackgroundGetter_()
                                                       : palette().color(QPalette::Window);
      setColorButtonColor(wheelSwitchBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (wheelSwitchColorModeCombo_) {
      const QSignalBlocker blocker(wheelSwitchColorModeCombo_);
      const int index = wheelSwitchColorModeGetter_
              ? colorModeToIndex(wheelSwitchColorModeGetter_())
              : colorModeToIndex(TextColorMode::kStatic);
      wheelSwitchColorModeCombo_->setCurrentIndex(index);
    }

    if (wheelSwitchFormatEdit_) {
      const QString format = wheelSwitchFormatGetter_ ? wheelSwitchFormatGetter_()
                                                      : QString();
      const QSignalBlocker blocker(wheelSwitchFormatEdit_);
      wheelSwitchFormatEdit_->setText(format);
      committedTexts_[wheelSwitchFormatEdit_] = wheelSwitchFormatEdit_->text();
    }

    if (wheelSwitchChannelEdit_) {
      const QString channel = wheelSwitchChannelGetter_ ? wheelSwitchChannelGetter_()
                                                        : QString();
      const QSignalBlocker blocker(wheelSwitchChannelEdit_);
      wheelSwitchChannelEdit_->setText(channel);
      committedTexts_[wheelSwitchChannelEdit_] = wheelSwitchChannelEdit_->text();
    }

    if (wheelSwitchPvLimitsButton_) {
      wheelSwitchPvLimitsButton_->setEnabled(
          static_cast<bool>(wheelSwitchLimitsGetter_)
          && static_cast<bool>(wheelSwitchLimitsSetter_));
    }

    updateWheelSwitchLimitsFromDialog();

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Wheel Switch"));
    }

    showPaletteWithoutActivating();
  }

  void showForChoiceButton(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<ChoiceButtonStacking()> stackingGetter,
      std::function<void(ChoiceButtonStacking)> stackingSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kChoiceButton;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textEntryForegroundGetter_ = {};
    textEntryForegroundSetter_ = {};
    textEntryBackgroundGetter_ = {};
    textEntryBackgroundSetter_ = {};
    textEntryFormatGetter_ = {};
    textEntryFormatSetter_ = {};
    textEntryPrecisionGetter_ = {};
    textEntryPrecisionSetter_ = {};
    textEntryPrecisionSourceGetter_ = {};
    textEntryPrecisionSourceSetter_ = {};
    textEntryPrecisionDefaultGetter_ = {};
    textEntryPrecisionDefaultSetter_ = {};
    textEntryColorModeGetter_ = {};
    textEntryColorModeSetter_ = {};
    textEntryChannelGetter_ = {};
    textEntryChannelSetter_ = {};

    choiceButtonForegroundGetter_ = std::move(foregroundGetter);
    choiceButtonForegroundSetter_ = std::move(foregroundSetter);
    choiceButtonBackgroundGetter_ = std::move(backgroundGetter);
    choiceButtonBackgroundSetter_ = std::move(backgroundSetter);
    choiceButtonColorModeGetter_ = std::move(colorModeGetter);
    choiceButtonColorModeSetter_ = std::move(colorModeSetter);
    choiceButtonStackingGetter_ = std::move(stackingGetter);
    choiceButtonStackingSetter_ = std::move(stackingSetter);
    choiceButtonChannelGetter_ = std::move(channelGetter);
    choiceButtonChannelSetter_ = std::move(channelSetter);

    meterForegroundGetter_ = {};
    meterForegroundSetter_ = {};
    meterBackgroundGetter_ = {};
    meterBackgroundSetter_ = {};
    meterLabelGetter_ = {};
    meterLabelSetter_ = {};
    meterColorModeGetter_ = {};
    meterColorModeSetter_ = {};
    meterChannelGetter_ = {};
    meterChannelSetter_ = {};
    meterLimitsGetter_ = {};
    meterLimitsSetter_ = {};
    barForegroundGetter_ = {};
    barForegroundSetter_ = {};
    barBackgroundGetter_ = {};
    barBackgroundSetter_ = {};
    barLabelGetter_ = {};
    barLabelSetter_ = {};
    barColorModeGetter_ = {};
    barColorModeSetter_ = {};
    barDirectionGetter_ = {};
    barDirectionSetter_ = {};
    barFillModeGetter_ = {};
    barFillModeSetter_ = {};
    barChannelGetter_ = {};
    barChannelSetter_ = {};
    barLimitsGetter_ = {};
    barLimitsSetter_ = {};
    scaleForegroundGetter_ = {};
    scaleForegroundSetter_ = {};
    scaleBackgroundGetter_ = {};
    scaleBackgroundSetter_ = {};
    scaleLabelGetter_ = {};
    scaleLabelSetter_ = {};
    scaleColorModeGetter_ = {};
    scaleColorModeSetter_ = {};
    scaleDirectionGetter_ = {};
    scaleDirectionSetter_ = {};
    scaleChannelGetter_ = {};
    scaleChannelSetter_ = {};
    scaleLimitsGetter_ = {};
    scaleLimitsSetter_ = {};
    stripTitleGetter_ = {};
    stripTitleSetter_ = {};
    stripXLabelGetter_ = {};
    stripXLabelSetter_ = {};
    stripYLabelGetter_ = {};
    stripYLabelSetter_ = {};
    stripForegroundGetter_ = {};
    stripForegroundSetter_ = {};
    stripBackgroundGetter_ = {};
    stripBackgroundSetter_ = {};
    stripPeriodGetter_ = {};
    stripPeriodSetter_ = {};
    stripUnitsGetter_ = {};
    stripUnitsSetter_ = {};
    for (auto &getter : stripPenChannelGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenChannelSetters_) {
      setter = {};
    }
    cartesianTitleGetter_ = {};
    cartesianTitleSetter_ = {};
    cartesianXLabelGetter_ = {};
    cartesianXLabelSetter_ = {};
    for (auto &getter : cartesianYLabelGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianYLabelSetters_) {
      setter = {};
    }
    cartesianForegroundGetter_ = {};
    cartesianForegroundSetter_ = {};
    cartesianBackgroundGetter_ = {};
    cartesianBackgroundSetter_ = {};
    for (auto &getter : cartesianAxisStyleGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianAxisStyleSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianAxisRangeGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianAxisRangeSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianAxisMinimumGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianAxisMinimumSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianAxisMaximumGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianAxisMaximumSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianAxisTimeFormatGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianAxisTimeFormatSetters_) {
      setter = {};
    }
    cartesianStyleGetter_ = {};
    cartesianStyleSetter_ = {};
    cartesianEraseOldestGetter_ = {};
    cartesianEraseOldestSetter_ = {};
    cartesianCountGetter_ = {};
    cartesianCountSetter_ = {};
    cartesianEraseModeGetter_ = {};
    cartesianEraseModeSetter_ = {};
    cartesianTriggerGetter_ = {};
    cartesianTriggerSetter_ = {};
    cartesianEraseGetter_ = {};
    cartesianEraseSetter_ = {};
    cartesianCountPvGetter_ = {};
    cartesianCountPvSetter_ = {};
    for (auto &getter : cartesianTraceXGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceXSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceYGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceYSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceColorGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceColorSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceAxisGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceAxisSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceSideGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceSideSetters_) {
      setter = {};
    }
    byteForegroundGetter_ = {};
    byteForegroundSetter_ = {};
    byteBackgroundGetter_ = {};
    byteBackgroundSetter_ = {};
    byteColorModeGetter_ = {};
    byteColorModeSetter_ = {};
    byteDirectionGetter_ = {};
    byteDirectionSetter_ = {};
    byteStartBitGetter_ = {};
    byteStartBitSetter_ = {};
    byteEndBitGetter_ = {};
    byteEndBitSetter_ = {};
    byteChannelGetter_ = {};
    byteChannelSetter_ = {};
    rectangleForegroundGetter_ = {};
    rectangleForegroundSetter_ = {};
    rectangleFillGetter_ = {};
    rectangleFillSetter_ = {};
    rectangleLineStyleGetter_ = {};
    rectangleLineStyleSetter_ = {};
    rectangleLineWidthGetter_ = {};
    rectangleLineWidthSetter_ = {};
    rectangleColorModeGetter_ = {};
    rectangleColorModeSetter_ = {};
    rectangleVisibilityModeGetter_ = {};
    rectangleVisibilityModeSetter_ = {};
    rectangleVisibilityCalcGetter_ = {};
    rectangleVisibilityCalcSetter_ = {};
    for (auto &getter : rectangleChannelGetters_) {
      getter = {};
    }
    for (auto &setter : rectangleChannelSetters_) {
      setter = {};
    }
    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    QRect choiceGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (choiceGeometry.width() <= 0) {
      choiceGeometry.setWidth(kMinimumTextWidth);
    }
    if (choiceGeometry.height() <= 0) {
      choiceGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = choiceGeometry;

    updateGeometryEdits(choiceGeometry);

    if (choiceButtonForegroundButton_) {
      const QColor color = choiceButtonForegroundGetter_
              ? choiceButtonForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(choiceButtonForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (choiceButtonBackgroundButton_) {
      const QColor color = choiceButtonBackgroundGetter_
              ? choiceButtonBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(choiceButtonBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (choiceButtonColorModeCombo_) {
      const QSignalBlocker blocker(choiceButtonColorModeCombo_);
      const int index = choiceButtonColorModeGetter_
              ? colorModeToIndex(choiceButtonColorModeGetter_())
              : colorModeToIndex(TextColorMode::kStatic);
      choiceButtonColorModeCombo_->setCurrentIndex(index);
    }

    if (choiceButtonStackingCombo_) {
      const QSignalBlocker blocker(choiceButtonStackingCombo_);
      const int index = choiceButtonStackingGetter_
              ? choiceButtonStackingToIndex(choiceButtonStackingGetter_())
              : choiceButtonStackingToIndex(ChoiceButtonStacking::kRow);
      choiceButtonStackingCombo_->setCurrentIndex(index);
    }

    if (choiceButtonChannelEdit_) {
      const QString channel = choiceButtonChannelGetter_
              ? choiceButtonChannelGetter_()
              : QString();
      const QSignalBlocker blocker(choiceButtonChannelEdit_);
      choiceButtonChannelEdit_->setText(channel);
      committedTexts_[choiceButtonChannelEdit_] = choiceButtonChannelEdit_->text();
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Choice Button"));
    }

    showPaletteWithoutActivating();
  }

  void showForMenu(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kMenu;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    menuForegroundGetter_ = std::move(foregroundGetter);
    menuForegroundSetter_ = std::move(foregroundSetter);
    menuBackgroundGetter_ = std::move(backgroundGetter);
    menuBackgroundSetter_ = std::move(backgroundSetter);
    menuColorModeGetter_ = std::move(colorModeGetter);
    menuColorModeSetter_ = std::move(colorModeSetter);
    menuChannelGetter_ = std::move(channelGetter);
    menuChannelSetter_ = std::move(channelSetter);

    QRect menuGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (menuGeometry.width() <= 0) {
      menuGeometry.setWidth(kMinimumTextWidth);
    }
    if (menuGeometry.height() <= 0) {
      menuGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = menuGeometry;

    updateGeometryEdits(menuGeometry);

    if (menuForegroundButton_) {
      const QColor color = menuForegroundGetter_ ? menuForegroundGetter_()
                                                : palette().color(QPalette::WindowText);
      setColorButtonColor(menuForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (menuBackgroundButton_) {
      const QColor color = menuBackgroundGetter_ ? menuBackgroundGetter_()
                                                : palette().color(QPalette::Window);
      setColorButtonColor(menuBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (menuColorModeCombo_) {
      const QSignalBlocker blocker(menuColorModeCombo_);
      const int index = menuColorModeGetter_
          ? colorModeToIndex(menuColorModeGetter_())
          : colorModeToIndex(TextColorMode::kStatic);
      menuColorModeCombo_->setCurrentIndex(index);
    }

    if (menuChannelEdit_) {
      const QString channel = menuChannelGetter_ ? menuChannelGetter_() : QString();
      const QSignalBlocker blocker(menuChannelEdit_);
      menuChannelEdit_->setText(channel);
      committedTexts_[menuChannelEdit_] = menuChannelEdit_->text();
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Menu"));
    }

    showPaletteWithoutActivating();
  }

  void showForMessageButton(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::function<QString()> pressGetter,
      std::function<void(const QString &)> pressSetter,
      std::function<QString()> releaseGetter,
      std::function<void(const QString &)> releaseSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kMessageButton;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    messageButtonForegroundGetter_ = std::move(foregroundGetter);
    messageButtonForegroundSetter_ = std::move(foregroundSetter);
    messageButtonBackgroundGetter_ = std::move(backgroundGetter);
    messageButtonBackgroundSetter_ = std::move(backgroundSetter);
    messageButtonColorModeGetter_ = std::move(colorModeGetter);
    messageButtonColorModeSetter_ = std::move(colorModeSetter);
    messageButtonLabelGetter_ = std::move(labelGetter);
    messageButtonLabelSetter_ = std::move(labelSetter);
    messageButtonPressGetter_ = std::move(pressGetter);
    messageButtonPressSetter_ = std::move(pressSetter);
    messageButtonReleaseGetter_ = std::move(releaseGetter);
    messageButtonReleaseSetter_ = std::move(releaseSetter);
    messageButtonChannelGetter_ = std::move(channelGetter);
    messageButtonChannelSetter_ = std::move(channelSetter);

    QRect messageGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (messageGeometry.width() <= 0) {
      messageGeometry.setWidth(kMinimumTextWidth);
    }
    if (messageGeometry.height() <= 0) {
      messageGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = messageGeometry;

    updateGeometryEdits(messageGeometry);

    if (messageButtonForegroundButton_) {
      const QColor color = messageButtonForegroundGetter_
              ? messageButtonForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(messageButtonForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (messageButtonBackgroundButton_) {
      const QColor color = messageButtonBackgroundGetter_
              ? messageButtonBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(messageButtonBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (messageButtonColorModeCombo_) {
      const QSignalBlocker blocker(messageButtonColorModeCombo_);
      const int index = messageButtonColorModeGetter_
              ? colorModeToIndex(messageButtonColorModeGetter_())
              : colorModeToIndex(TextColorMode::kStatic);
      messageButtonColorModeCombo_->setCurrentIndex(index);
    }

    if (messageButtonLabelEdit_) {
      const QString text = messageButtonLabelGetter_ ? messageButtonLabelGetter_()
                                                     : QString();
      const QSignalBlocker blocker(messageButtonLabelEdit_);
      messageButtonLabelEdit_->setText(text);
      committedTexts_[messageButtonLabelEdit_] = text;
    }

    if (messageButtonPressEdit_) {
      const QString text = messageButtonPressGetter_ ? messageButtonPressGetter_()
                                                     : QString();
      const QSignalBlocker blocker(messageButtonPressEdit_);
      messageButtonPressEdit_->setText(text);
      committedTexts_[messageButtonPressEdit_] = text;
    }

    if (messageButtonReleaseEdit_) {
      const QString text = messageButtonReleaseGetter_
              ? messageButtonReleaseGetter_()
              : QString();
      const QSignalBlocker blocker(messageButtonReleaseEdit_);
      messageButtonReleaseEdit_->setText(text);
      committedTexts_[messageButtonReleaseEdit_] = text;
    }

    if (messageButtonChannelEdit_) {
      const QString channel = messageButtonChannelGetter_
              ? messageButtonChannelGetter_()
              : QString();
      const QSignalBlocker blocker(messageButtonChannelEdit_);
      messageButtonChannelEdit_->setText(channel);
      committedTexts_[messageButtonChannelEdit_] = channel;
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Message Button"));
    }

    showPaletteWithoutActivating();
  }

  void showForShellCommand(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::array<std::function<QString()>, kShellCommandEntryCount> entryLabelGetters,
      std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryLabelSetters,
      std::array<std::function<QString()>, kShellCommandEntryCount> entryCommandGetters,
      std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryCommandSetters,
      std::array<std::function<QString()>, kShellCommandEntryCount> entryArgsGetters,
      std::array<std::function<void(const QString &)>, kShellCommandEntryCount> entryArgsSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kShellCommand;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    shellCommandForegroundGetter_ = std::move(foregroundGetter);
    shellCommandForegroundSetter_ = std::move(foregroundSetter);
    shellCommandBackgroundGetter_ = std::move(backgroundGetter);
    shellCommandBackgroundSetter_ = std::move(backgroundSetter);
    shellCommandLabelGetter_ = std::move(labelGetter);
    shellCommandLabelSetter_ = std::move(labelSetter);
    shellCommandEntryLabelGetters_ = std::move(entryLabelGetters);
    shellCommandEntryLabelSetters_ = std::move(entryLabelSetters);
    shellCommandEntryCommandGetters_ = std::move(entryCommandGetters);
    shellCommandEntryCommandSetters_ = std::move(entryCommandSetters);
    shellCommandEntryArgsGetters_ = std::move(entryArgsGetters);
    shellCommandEntryArgsSetters_ = std::move(entryArgsSetters);

    QRect commandGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (commandGeometry.width() <= 0) {
      commandGeometry.setWidth(kMinimumTextWidth);
    }
    if (commandGeometry.height() <= 0) {
      commandGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = commandGeometry;

    updateGeometryEdits(commandGeometry);

    if (shellCommandForegroundButton_) {
      const QColor color = shellCommandForegroundGetter_
              ? shellCommandForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(shellCommandForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      shellCommandForegroundButton_->setEnabled(
          static_cast<bool>(shellCommandForegroundSetter_));
    }

    if (shellCommandBackgroundButton_) {
      const QColor color = shellCommandBackgroundGetter_
              ? shellCommandBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(shellCommandBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      shellCommandBackgroundButton_->setEnabled(
          static_cast<bool>(shellCommandBackgroundSetter_));
    }

    if (shellCommandLabelEdit_) {
      const QString text = shellCommandLabelGetter_ ? shellCommandLabelGetter_() : QString();
      const QSignalBlocker blocker(shellCommandLabelEdit_);
      shellCommandLabelEdit_->setText(text);
      shellCommandLabelEdit_->setEnabled(static_cast<bool>(shellCommandLabelSetter_));
      committedTexts_[shellCommandLabelEdit_] = text;
    }

    for (int i = 0; i < kShellCommandEntryCount; ++i) {
      QLineEdit *labelEdit = shellCommandEntryLabelEdits_[i];
      if (labelEdit) {
        const QSignalBlocker blocker(labelEdit);
        const QString value = shellCommandEntryLabelGetters_[i]
                                  ? shellCommandEntryLabelGetters_[i]()
                                  : QString();
        labelEdit->setText(value);
        labelEdit->setEnabled(static_cast<bool>(shellCommandEntryLabelSetters_[i]));
        committedTexts_[labelEdit] = value;
      }

      QLineEdit *commandEdit = shellCommandEntryCommandEdits_[i];
      if (commandEdit) {
        const QSignalBlocker blocker(commandEdit);
        const QString value = shellCommandEntryCommandGetters_[i]
                                  ? shellCommandEntryCommandGetters_[i]()
                                  : QString();
        commandEdit->setText(value);
        commandEdit->setEnabled(static_cast<bool>(shellCommandEntryCommandSetters_[i]));
        committedTexts_[commandEdit] = value;
      }

      QLineEdit *argsEdit = shellCommandEntryArgsEdits_[i];
      if (argsEdit) {
        const QSignalBlocker blocker(argsEdit);
        const QString value = shellCommandEntryArgsGetters_[i]
                                  ? shellCommandEntryArgsGetters_[i]()
                                  : QString();
        argsEdit->setText(value);
        argsEdit->setEnabled(static_cast<bool>(shellCommandEntryArgsSetters_[i]));
        committedTexts_[argsEdit] = value;
      }
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Shell Command"));
    }

    showPaletteWithoutActivating();
  }

  void showForRelatedDisplay(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::function<RelatedDisplayVisual()> visualGetter,
      std::function<void(RelatedDisplayVisual)> visualSetter,
      std::array<std::function<QString()>, kRelatedDisplayEntryCount> entryLabelGetters,
      std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> entryLabelSetters,
      std::array<std::function<QString()>, kRelatedDisplayEntryCount> entryNameGetters,
      std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> entryNameSetters,
      std::array<std::function<QString()>, kRelatedDisplayEntryCount> entryArgsGetters,
      std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> entryArgsSetters,
      std::array<std::function<RelatedDisplayMode()>, kRelatedDisplayEntryCount> entryModeGetters,
      std::array<std::function<void(RelatedDisplayMode)>, kRelatedDisplayEntryCount> entryModeSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kRelatedDisplay;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    relatedDisplayForegroundGetter_ = std::move(foregroundGetter);
    relatedDisplayForegroundSetter_ = std::move(foregroundSetter);
    relatedDisplayBackgroundGetter_ = std::move(backgroundGetter);
    relatedDisplayBackgroundSetter_ = std::move(backgroundSetter);
    relatedDisplayLabelGetter_ = std::move(labelGetter);
    relatedDisplayLabelSetter_ = std::move(labelSetter);
    relatedDisplayVisualGetter_ = std::move(visualGetter);
    relatedDisplayVisualSetter_ = std::move(visualSetter);
    relatedDisplayEntryLabelGetters_ = std::move(entryLabelGetters);
    relatedDisplayEntryLabelSetters_ = std::move(entryLabelSetters);
    relatedDisplayEntryNameGetters_ = std::move(entryNameGetters);
    relatedDisplayEntryNameSetters_ = std::move(entryNameSetters);
    relatedDisplayEntryArgsGetters_ = std::move(entryArgsGetters);
    relatedDisplayEntryArgsSetters_ = std::move(entryArgsSetters);
    relatedDisplayEntryModeGetters_ = std::move(entryModeGetters);
    relatedDisplayEntryModeSetters_ = std::move(entryModeSetters);

    QRect rdGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (rdGeometry.width() <= 0) {
      rdGeometry.setWidth(kMinimumTextWidth);
    }
    if (rdGeometry.height() <= 0) {
      rdGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = rdGeometry;

    updateGeometryEdits(rdGeometry);

    if (relatedDisplayForegroundButton_) {
      const QColor color = relatedDisplayForegroundGetter_
              ? relatedDisplayForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(relatedDisplayForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      relatedDisplayForegroundButton_->setEnabled(
          static_cast<bool>(relatedDisplayForegroundSetter_));
    }

    if (relatedDisplayBackgroundButton_) {
      const QColor color = relatedDisplayBackgroundGetter_
              ? relatedDisplayBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(relatedDisplayBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      relatedDisplayBackgroundButton_->setEnabled(
          static_cast<bool>(relatedDisplayBackgroundSetter_));
    }

    if (relatedDisplayLabelEdit_) {
      const QString text = relatedDisplayLabelGetter_
              ? relatedDisplayLabelGetter_()
              : QString();
      const QSignalBlocker blocker(relatedDisplayLabelEdit_);
      relatedDisplayLabelEdit_->setText(text);
      relatedDisplayLabelEdit_->setEnabled(
          static_cast<bool>(relatedDisplayLabelSetter_));
      committedTexts_[relatedDisplayLabelEdit_] = text;
    }

    if (relatedDisplayVisualCombo_) {
      const QSignalBlocker blocker(relatedDisplayVisualCombo_);
      const int index = relatedDisplayVisualGetter_
              ? relatedDisplayVisualToIndex(relatedDisplayVisualGetter_())
              : relatedDisplayVisualToIndex(RelatedDisplayVisual::kMenu);
      relatedDisplayVisualCombo_->setCurrentIndex(index);
      relatedDisplayVisualCombo_->setEnabled(
          static_cast<bool>(relatedDisplayVisualSetter_));
    }

    for (int i = 0; i < kRelatedDisplayEntryCount; ++i) {
      if (relatedDisplayEntryLabelEdits_[i]) {
        const QString value = relatedDisplayEntryLabelGetters_[i]
                ? relatedDisplayEntryLabelGetters_[i]()
                : QString();
        const QSignalBlocker blocker(relatedDisplayEntryLabelEdits_[i]);
        relatedDisplayEntryLabelEdits_[i]->setText(value);
        relatedDisplayEntryLabelEdits_[i]->setEnabled(
            static_cast<bool>(relatedDisplayEntryLabelSetters_[i]));
        committedTexts_[relatedDisplayEntryLabelEdits_[i]] = value;
      }
      if (relatedDisplayEntryNameEdits_[i]) {
        const QString value = relatedDisplayEntryNameGetters_[i]
                ? relatedDisplayEntryNameGetters_[i]()
                : QString();
        const QSignalBlocker blocker(relatedDisplayEntryNameEdits_[i]);
        relatedDisplayEntryNameEdits_[i]->setText(value);
        relatedDisplayEntryNameEdits_[i]->setEnabled(
            static_cast<bool>(relatedDisplayEntryNameSetters_[i]));
        committedTexts_[relatedDisplayEntryNameEdits_[i]] = value;
      }
      if (relatedDisplayEntryArgsEdits_[i]) {
        const QString value = relatedDisplayEntryArgsGetters_[i]
                ? relatedDisplayEntryArgsGetters_[i]()
                : QString();
        const QSignalBlocker blocker(relatedDisplayEntryArgsEdits_[i]);
        relatedDisplayEntryArgsEdits_[i]->setText(value);
        relatedDisplayEntryArgsEdits_[i]->setEnabled(
            static_cast<bool>(relatedDisplayEntryArgsSetters_[i]));
        committedTexts_[relatedDisplayEntryArgsEdits_[i]] = value;
      }
      if (relatedDisplayEntryModeCombos_[i]) {
        const QSignalBlocker blocker(relatedDisplayEntryModeCombos_[i]);
        const RelatedDisplayMode mode = relatedDisplayEntryModeGetters_[i]
                ? relatedDisplayEntryModeGetters_[i]()
                : RelatedDisplayMode::kAdd;
        relatedDisplayEntryModeCombos_[i]->setCurrentIndex(
            relatedDisplayModeToIndex(mode));
        relatedDisplayEntryModeCombos_[i]->setEnabled(
            static_cast<bool>(relatedDisplayEntryModeSetters_[i]));
      }
    }

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Related Display"));
    }

    showPaletteWithoutActivating();
  }

  void showForTextMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<Qt::Alignment()> alignmentGetter,
      std::function<void(Qt::Alignment)> alignmentSetter,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      std::function<PvLimitSource()> precisionSourceGetter,
      std::function<void(PvLimitSource)> precisionSourceSetter,
      std::function<int()> precisionDefaultGetter,
      std::function<void(int)> precisionDefaultSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter = {},
      std::function<void(const PvLimits &)> limitsSetter = {})
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kTextMonitor;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textMonitorLimitsGetter_ = {};
    textMonitorLimitsSetter_ = {};
    textMonitorForegroundGetter_ = std::move(foregroundGetter);
    textMonitorForegroundSetter_ = std::move(foregroundSetter);
    textMonitorBackgroundGetter_ = std::move(backgroundGetter);
    textMonitorBackgroundSetter_ = std::move(backgroundSetter);
    textMonitorAlignmentGetter_ = std::move(alignmentGetter);
    textMonitorAlignmentSetter_ = std::move(alignmentSetter);
    textMonitorFormatGetter_ = std::move(formatGetter);
    textMonitorFormatSetter_ = std::move(formatSetter);
    textMonitorPrecisionGetter_ = std::move(precisionGetter);
    textMonitorPrecisionSetter_ = std::move(precisionSetter);
    textMonitorPrecisionSourceGetter_ = std::move(precisionSourceGetter);
    textMonitorPrecisionSourceSetter_ = std::move(precisionSourceSetter);
    textMonitorPrecisionDefaultGetter_ = std::move(precisionDefaultGetter);
    textMonitorPrecisionDefaultSetter_ = std::move(precisionDefaultSetter);
    textMonitorColorModeGetter_ = std::move(colorModeGetter);
    textMonitorColorModeSetter_ = std::move(colorModeSetter);
    textMonitorChannelGetter_ = std::move(channelGetter);
    textMonitorChannelSetter_ = std::move(channelSetter);
    textMonitorLimitsGetter_ = std::move(limitsGetter);
    textMonitorLimitsSetter_ = std::move(limitsSetter);

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    QRect monitorGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (monitorGeometry.width() <= 0) {
      monitorGeometry.setWidth(kMinimumTextWidth);
    }
    if (monitorGeometry.height() <= 0) {
      monitorGeometry.setHeight(kMinimumTextHeight);
    }
    lastCommittedGeometry_ = monitorGeometry;

    updateGeometryEdits(monitorGeometry);

    if (textMonitorForegroundButton_) {
      const QColor color = textMonitorForegroundGetter_
              ? textMonitorForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(textMonitorForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (textMonitorBackgroundButton_) {
      const QColor color = textMonitorBackgroundGetter_
              ? textMonitorBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(textMonitorBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (textMonitorAlignmentCombo_) {
      const QSignalBlocker blocker(textMonitorAlignmentCombo_);
      const Qt::Alignment alignment = textMonitorAlignmentGetter_
              ? textMonitorAlignmentGetter_()
              : (Qt::AlignLeft | Qt::AlignVCenter);
      textMonitorAlignmentCombo_->setCurrentIndex(
          alignmentToIndex(alignment));
    }

    if (textMonitorFormatCombo_) {
      const QSignalBlocker blocker(textMonitorFormatCombo_);
      const TextMonitorFormat format = textMonitorFormatGetter_
              ? textMonitorFormatGetter_()
              : TextMonitorFormat::kDecimal;
      textMonitorFormatCombo_->setCurrentIndex(
          textMonitorFormatToIndex(format));
    }

    if (textMonitorColorModeCombo_) {
      const QSignalBlocker blocker(textMonitorColorModeCombo_);
      const TextColorMode mode = textMonitorColorModeGetter_
              ? textMonitorColorModeGetter_()
              : TextColorMode::kStatic;
      textMonitorColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (textMonitorChannelEdit_) {
      const QString channel = textMonitorChannelGetter_
              ? textMonitorChannelGetter_()
              : QString();
      const QSignalBlocker blocker(textMonitorChannelEdit_);
      textMonitorChannelEdit_->setText(channel);
      committedTexts_[textMonitorChannelEdit_] = channel;
    }

    updateTextMonitorLimitsFromDialog();

    if (textMonitorPvLimitsButton_) {
      textMonitorPvLimitsButton_->setEnabled(
          static_cast<bool>(textMonitorPrecisionSourceSetter_));
    }

    elementLabel_->setText(QStringLiteral("Text Monitor"));

    showPaletteWithoutActivating();
  }

  void showForMeter(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kMeter;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    meterForegroundGetter_ = std::move(foregroundGetter);
    meterForegroundSetter_ = std::move(foregroundSetter);
    meterBackgroundGetter_ = std::move(backgroundGetter);
    meterBackgroundSetter_ = std::move(backgroundSetter);
    meterLabelGetter_ = std::move(labelGetter);
    meterLabelSetter_ = std::move(labelSetter);
    meterColorModeGetter_ = std::move(colorModeGetter);
    meterColorModeSetter_ = std::move(colorModeSetter);
    meterChannelGetter_ = std::move(channelGetter);
    meterChannelSetter_ = std::move(channelSetter);
    meterLimitsGetter_ = std::move(limitsGetter);
    meterLimitsSetter_ = std::move(limitsSetter);

    QRect meterGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (meterGeometry.width() <= 0) {
      meterGeometry.setWidth(kMinimumMeterSize);
    }
    if (meterGeometry.height() <= 0) {
      meterGeometry.setHeight(kMinimumMeterSize);
    }
    lastCommittedGeometry_ = meterGeometry;

    updateGeometryEdits(meterGeometry);

    if (meterForegroundButton_) {
      const QColor color = meterForegroundGetter_
              ? meterForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(meterForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (meterBackgroundButton_) {
      const QColor color = meterBackgroundGetter_
              ? meterBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(meterBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (meterLabelCombo_) {
      const QSignalBlocker blocker(meterLabelCombo_);
      const MeterLabel label = meterLabelGetter_ ? meterLabelGetter_()
                                                : MeterLabel::kOutline;
      meterLabelCombo_->setCurrentIndex(meterLabelToIndex(label));
    }

    if (meterColorModeCombo_) {
      const QSignalBlocker blocker(meterColorModeCombo_);
      const TextColorMode mode = meterColorModeGetter_
              ? meterColorModeGetter_()
              : TextColorMode::kStatic;
      meterColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (meterChannelEdit_) {
      const QString channel = meterChannelGetter_ ? meterChannelGetter_()
                                                  : QString();
      const QSignalBlocker blocker(meterChannelEdit_);
      meterChannelEdit_->setText(channel);
      committedTexts_[meterChannelEdit_] = meterChannelEdit_->text();
    }

    if (meterPvLimitsButton_) {
      meterPvLimitsButton_->setEnabled(static_cast<bool>(meterLimitsSetter_));
    }

    updateMeterLimitsFromDialog();

    elementLabel_->setText(QStringLiteral("Meter"));

    showPaletteWithoutActivating();
  }

  void showForBarMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<BarFill()> fillGetter,
      std::function<void(BarFill)> fillSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kBarMonitor;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    meterForegroundGetter_ = {};
    meterForegroundSetter_ = {};
    meterBackgroundGetter_ = {};
    meterBackgroundSetter_ = {};
    meterLabelGetter_ = {};
    meterLabelSetter_ = {};
    meterColorModeGetter_ = {};
    meterColorModeSetter_ = {};
    meterChannelGetter_ = {};
    meterChannelSetter_ = {};
    meterLimitsGetter_ = {};
    meterLimitsSetter_ = {};

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    barForegroundGetter_ = std::move(foregroundGetter);
    barForegroundSetter_ = std::move(foregroundSetter);
    barBackgroundGetter_ = std::move(backgroundGetter);
    barBackgroundSetter_ = std::move(backgroundSetter);
    barLabelGetter_ = std::move(labelGetter);
    barLabelSetter_ = std::move(labelSetter);
    barColorModeGetter_ = std::move(colorModeGetter);
    barColorModeSetter_ = std::move(colorModeSetter);
    barDirectionGetter_ = std::move(directionGetter);
    barDirectionSetter_ = std::move(directionSetter);
    barFillModeGetter_ = std::move(fillGetter);
    barFillModeSetter_ = std::move(fillSetter);
    barChannelGetter_ = std::move(channelGetter);
    barChannelSetter_ = std::move(channelSetter);
    barLimitsGetter_ = std::move(limitsGetter);
    barLimitsSetter_ = std::move(limitsSetter);

    QRect barGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (barGeometry.width() <= 0) {
      barGeometry.setWidth(kMinimumBarSize);
    }
    if (barGeometry.height() <= 0) {
      barGeometry.setHeight(kMinimumBarSize);
    }
    lastCommittedGeometry_ = barGeometry;

    updateGeometryEdits(barGeometry);

    if (barForegroundButton_) {
      const QColor color = barForegroundGetter_
              ? barForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(barForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (barBackgroundButton_) {
      const QColor color = barBackgroundGetter_
              ? barBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(barBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (barLabelCombo_) {
      const QSignalBlocker blocker(barLabelCombo_);
      const MeterLabel label = barLabelGetter_ ? barLabelGetter_()
                                               : MeterLabel::kOutline;
      barLabelCombo_->setCurrentIndex(meterLabelToIndex(label));
    }

    if (barColorModeCombo_) {
      const QSignalBlocker blocker(barColorModeCombo_);
      const TextColorMode mode = barColorModeGetter_ ? barColorModeGetter_()
                                                    : TextColorMode::kStatic;
      barColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (barDirectionCombo_) {
      const QSignalBlocker blocker(barDirectionCombo_);
      const BarDirection direction = barDirectionGetter_ ? barDirectionGetter_()
                                                         : BarDirection::kRight;
      barDirectionCombo_->setCurrentIndex(barDirectionToIndex(direction));
    }

    if (barFillCombo_) {
      const QSignalBlocker blocker(barFillCombo_);
      const BarFill fill = barFillModeGetter_ ? barFillModeGetter_()
                                              : BarFill::kFromEdge;
      barFillCombo_->setCurrentIndex(barFillToIndex(fill));
    }

    if (barChannelEdit_) {
      const QString channel = barChannelGetter_ ? barChannelGetter_()
                                                : QString();
      const QSignalBlocker blocker(barChannelEdit_);
      barChannelEdit_->setText(channel);
      committedTexts_[barChannelEdit_] = channel;
    }

    updateBarLimitsFromDialog();

    if (barPvLimitsButton_) {
      barPvLimitsButton_->setEnabled(static_cast<bool>(barLimitsSetter_));
    }

    elementLabel_->setText(QStringLiteral("Bar Monitor"));

    showPaletteWithoutActivating();
  }

  void showForScaleMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kScaleMonitor;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    meterForegroundGetter_ = {};
    meterForegroundSetter_ = {};
    meterBackgroundGetter_ = {};
    meterBackgroundSetter_ = {};
    meterLabelGetter_ = {};
    meterLabelSetter_ = {};
    meterColorModeGetter_ = {};
    meterColorModeSetter_ = {};
    meterChannelGetter_ = {};
    meterChannelSetter_ = {};
    meterLimitsGetter_ = {};
    meterLimitsSetter_ = {};

    barForegroundGetter_ = {};
    barForegroundSetter_ = {};
    barBackgroundGetter_ = {};
    barBackgroundSetter_ = {};
    barLabelGetter_ = {};
    barLabelSetter_ = {};
    barColorModeGetter_ = {};
    barColorModeSetter_ = {};
    barDirectionGetter_ = {};
    barDirectionSetter_ = {};
    barFillModeGetter_ = {};
    barFillModeSetter_ = {};
    barChannelGetter_ = {};
    barChannelSetter_ = {};
    barLimitsGetter_ = {};
    barLimitsSetter_ = {};

    scaleForegroundGetter_ = std::move(foregroundGetter);
    scaleForegroundSetter_ = std::move(foregroundSetter);
    scaleBackgroundGetter_ = std::move(backgroundGetter);
    scaleBackgroundSetter_ = std::move(backgroundSetter);
    scaleLabelGetter_ = std::move(labelGetter);
    scaleLabelSetter_ = std::move(labelSetter);
    scaleColorModeGetter_ = std::move(colorModeGetter);
    scaleColorModeSetter_ = std::move(colorModeSetter);
    scaleDirectionGetter_ = std::move(directionGetter);
    scaleDirectionSetter_ = std::move(directionSetter);
    scaleChannelGetter_ = std::move(channelGetter);
    scaleChannelSetter_ = std::move(channelSetter);
    scaleLimitsGetter_ = std::move(limitsGetter);
    scaleLimitsSetter_ = std::move(limitsSetter);

    byteForegroundGetter_ = {};
    byteForegroundSetter_ = {};
    byteBackgroundGetter_ = {};
    byteBackgroundSetter_ = {};
    byteColorModeGetter_ = {};
    byteColorModeSetter_ = {};
    byteDirectionGetter_ = {};
    byteDirectionSetter_ = {};
    byteStartBitGetter_ = {};
    byteStartBitSetter_ = {};
    byteEndBitGetter_ = {};
    byteEndBitSetter_ = {};
    byteChannelGetter_ = {};
    byteChannelSetter_ = {};

    QRect scaleGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (scaleGeometry.width() <= 0) {
      scaleGeometry.setWidth(kMinimumScaleSize);
    }
    if (scaleGeometry.height() <= 0) {
      scaleGeometry.setHeight(kMinimumScaleSize);
    }
    lastCommittedGeometry_ = scaleGeometry;

    updateGeometryEdits(scaleGeometry);

    if (scaleForegroundButton_) {
      const QColor color = scaleForegroundGetter_
              ? scaleForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(scaleForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (scaleBackgroundButton_) {
      const QColor color = scaleBackgroundGetter_
              ? scaleBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(scaleBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (scaleLabelCombo_) {
      const QSignalBlocker blocker(scaleLabelCombo_);
      const MeterLabel label = scaleLabelGetter_ ? scaleLabelGetter_()
                                                : MeterLabel::kOutline;
      scaleLabelCombo_->setCurrentIndex(meterLabelToIndex(label));
    }

    if (scaleColorModeCombo_) {
      const QSignalBlocker blocker(scaleColorModeCombo_);
      const TextColorMode mode = scaleColorModeGetter_
              ? scaleColorModeGetter_()
              : TextColorMode::kStatic;
      scaleColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (scaleDirectionCombo_) {
      const QSignalBlocker blocker(scaleDirectionCombo_);
      const BarDirection direction = scaleDirectionGetter_
              ? scaleDirectionGetter_()
              : BarDirection::kRight;
      scaleDirectionCombo_->setCurrentIndex(scaleDirectionToIndex(direction));
    }

    if (scaleChannelEdit_) {
      const QString channel = scaleChannelGetter_ ? scaleChannelGetter_()
                                                  : QString();
      const QSignalBlocker blocker(scaleChannelEdit_);
      scaleChannelEdit_->setText(channel);
      committedTexts_[scaleChannelEdit_] = scaleChannelEdit_->text();
      scaleChannelEdit_->setEnabled(static_cast<bool>(scaleChannelSetter_));
    }

    if (scalePvLimitsButton_) {
      scalePvLimitsButton_->setEnabled(static_cast<bool>(scaleLimitsSetter_));
    }

    updateScaleLimitsFromDialog();

    elementLabel_->setText(QStringLiteral("Scale Monitor"));

    showPaletteWithoutActivating();
  }

  void showForStripChart(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> xLabelGetter,
      std::function<void(const QString &)> xLabelSetter,
      std::function<QString()> yLabelGetter,
      std::function<void(const QString &)> yLabelSetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<double()> periodGetter,
      std::function<void(double)> periodSetter,
      std::function<TimeUnits()> unitsGetter,
      std::function<void(TimeUnits)> unitsSetter,
      std::array<std::function<QString()>, kStripChartPenCount> channelGetters,
      std::array<std::function<void(const QString &)>, kStripChartPenCount> channelSetters,
      std::array<std::function<QColor()>, kStripChartPenCount> colorGetters,
      std::array<std::function<void(const QColor &)>, kStripChartPenCount> colorSetters,
      std::array<std::function<PvLimits()>, kStripChartPenCount> limitsGetters,
      std::array<std::function<void(const PvLimits &)>, kStripChartPenCount> limitsSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kStripChart;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    meterForegroundGetter_ = {};
    meterForegroundSetter_ = {};
    meterBackgroundGetter_ = {};
    meterBackgroundSetter_ = {};
    meterLabelGetter_ = {};
    meterLabelSetter_ = {};
    meterColorModeGetter_ = {};
    meterColorModeSetter_ = {};
    meterChannelGetter_ = {};
    meterChannelSetter_ = {};
    meterLimitsGetter_ = {};
    meterLimitsSetter_ = {};

    barForegroundGetter_ = {};
    barForegroundSetter_ = {};
    barBackgroundGetter_ = {};
    barBackgroundSetter_ = {};
    barLabelGetter_ = {};
    barLabelSetter_ = {};
    barColorModeGetter_ = {};
    barColorModeSetter_ = {};
    barDirectionGetter_ = {};
    barDirectionSetter_ = {};
    barFillModeGetter_ = {};
    barFillModeSetter_ = {};
    barChannelGetter_ = {};
    barChannelSetter_ = {};
    barLimitsGetter_ = {};
    barLimitsSetter_ = {};

    scaleForegroundGetter_ = {};
    scaleForegroundSetter_ = {};
    scaleBackgroundGetter_ = {};
    scaleBackgroundSetter_ = {};
    scaleLabelGetter_ = {};
    scaleLabelSetter_ = {};
    scaleColorModeGetter_ = {};
    scaleColorModeSetter_ = {};
    scaleDirectionGetter_ = {};
    scaleDirectionSetter_ = {};
    scaleChannelGetter_ = {};
    scaleChannelSetter_ = {};
    scaleLimitsGetter_ = {};
    scaleLimitsSetter_ = {};

    stripTitleGetter_ = std::move(titleGetter);
    stripTitleSetter_ = std::move(titleSetter);
    stripXLabelGetter_ = std::move(xLabelGetter);
    stripXLabelSetter_ = std::move(xLabelSetter);
    stripYLabelGetter_ = std::move(yLabelGetter);
    stripYLabelSetter_ = std::move(yLabelSetter);
    stripForegroundGetter_ = std::move(foregroundGetter);
    stripForegroundSetter_ = std::move(foregroundSetter);
    stripBackgroundGetter_ = std::move(backgroundGetter);
    stripBackgroundSetter_ = std::move(backgroundSetter);
    stripPeriodGetter_ = std::move(periodGetter);
    stripPeriodSetter_ = std::move(periodSetter);
    stripUnitsGetter_ = std::move(unitsGetter);
    stripUnitsSetter_ = std::move(unitsSetter);
    stripPenChannelGetters_ = std::move(channelGetters);
    stripPenChannelSetters_ = std::move(channelSetters);
    stripPenColorGetters_ = std::move(colorGetters);
    stripPenColorSetters_ = std::move(colorSetters);
    stripPenLimitsGetters_ = std::move(limitsGetters);
    stripPenLimitsSetters_ = std::move(limitsSetters);

    QRect chartGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (chartGeometry.width() <= 0) {
      chartGeometry.setWidth(kMinimumStripChartWidth);
    }
    if (chartGeometry.height() <= 0) {
      chartGeometry.setHeight(kMinimumStripChartHeight);
    }
    lastCommittedGeometry_ = chartGeometry;

    updateGeometryEdits(chartGeometry);

    if (stripForegroundButton_) {
      const QColor color = stripForegroundGetter_ ? stripForegroundGetter_()
                                                  : palette().color(QPalette::WindowText);
      setColorButtonColor(stripForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (stripBackgroundButton_) {
      const QColor color = stripBackgroundGetter_ ? stripBackgroundGetter_()
                                                  : palette().color(QPalette::Window);
      setColorButtonColor(stripBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (stripTitleEdit_) {
      const QString value = stripTitleGetter_ ? stripTitleGetter_() : QString();
      const QSignalBlocker blocker(stripTitleEdit_);
      stripTitleEdit_->setText(value);
      committedTexts_[stripTitleEdit_] = value;
    }

    if (stripXLabelEdit_) {
      const QString value = stripXLabelGetter_ ? stripXLabelGetter_() : QString();
      const QSignalBlocker blocker(stripXLabelEdit_);
      stripXLabelEdit_->setText(value);
      committedTexts_[stripXLabelEdit_] = value;
    }

    if (stripYLabelEdit_) {
      const QString value = stripYLabelGetter_ ? stripYLabelGetter_() : QString();
      const QSignalBlocker blocker(stripYLabelEdit_);
      stripYLabelEdit_->setText(value);
      committedTexts_[stripYLabelEdit_] = value;
    }

    if (stripPeriodEdit_) {
      double value = stripPeriodGetter_ ? stripPeriodGetter_()
                                        : kDefaultStripChartPeriod;
      if (value <= 0.0) {
        value = kDefaultStripChartPeriod;
      }
      QString text = QString::number(value, 'f', 3);
      if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
          text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
          text.chop(1);
        }
      }
      const QSignalBlocker blocker(stripPeriodEdit_);
      stripPeriodEdit_->setText(text);
      stripPeriodEdit_->setEnabled(static_cast<bool>(stripPeriodSetter_));
      committedTexts_[stripPeriodEdit_] = stripPeriodEdit_->text();
    }

    if (stripUnitsCombo_) {
      const QSignalBlocker blocker(stripUnitsCombo_);
      const TimeUnits units = stripUnitsGetter_ ? stripUnitsGetter_()
                                               : TimeUnits::kSeconds;
      stripUnitsCombo_->setCurrentIndex(timeUnitsToIndex(units));
      stripUnitsCombo_->setEnabled(static_cast<bool>(stripUnitsSetter_));
    }

    for (int i = 0; i < kStripChartPenCount; ++i) {
      if (stripPenColorButtons_[i]) {
        const QColor color = stripPenColorGetters_[i]
                ? stripPenColorGetters_[i]()
                : palette().color(QPalette::WindowText);
        setColorButtonColor(stripPenColorButtons_[i],
            color.isValid() ? color : palette().color(QPalette::WindowText));
      }
      if (stripPenChannelEdits_[i]) {
        const QString channel = stripPenChannelGetters_[i]
                ? stripPenChannelGetters_[i]()
                : QString();
        const QSignalBlocker blocker(stripPenChannelEdits_[i]);
        stripPenChannelEdits_[i]->setText(channel);
        stripPenChannelEdits_[i]->setEnabled(static_cast<bool>(stripPenChannelSetters_[i]));
        committedTexts_[stripPenChannelEdits_[i]] = channel;
      }
      if (stripPenLimitsButtons_[i]) {
        stripPenLimitsButtons_[i]->setEnabled(static_cast<bool>(stripPenLimitsSetters_[i]));
      }
    }

    elementLabel_->setText(QStringLiteral("Strip Chart"));

    showPaletteWithoutActivating();
  }

  void showForCartesianPlot(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> xLabelGetter,
      std::function<void(const QString &)> xLabelSetter,
      std::array<std::function<QString()>, 4> yLabelGetters,
      std::array<std::function<void(const QString &)>, 4> yLabelSetters,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> axisStyleGetters,
      std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> axisStyleSetters,
      std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> axisRangeGetters,
      std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> axisRangeSetters,
      std::array<std::function<double()>, kCartesianAxisCount> axisMinimumGetters,
      std::array<std::function<void(double)>, kCartesianAxisCount> axisMinimumSetters,
      std::array<std::function<double()>, kCartesianAxisCount> axisMaximumGetters,
      std::array<std::function<void(double)>, kCartesianAxisCount> axisMaximumSetters,
      std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> axisTimeFormatGetters,
      std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> axisTimeFormatSetters,
      std::function<CartesianPlotStyle()> styleGetter,
      std::function<void(CartesianPlotStyle)> styleSetter,
      std::function<bool()> eraseOldestGetter,
      std::function<void(bool)> eraseOldestSetter,
      std::function<int()> countGetter,
      std::function<void(int)> countSetter,
      std::function<CartesianPlotEraseMode()> eraseModeGetter,
      std::function<void(CartesianPlotEraseMode)> eraseModeSetter,
      std::function<QString()> triggerGetter,
      std::function<void(const QString &)> triggerSetter,
      std::function<QString()> eraseGetter,
      std::function<void(const QString &)> eraseSetter,
      std::function<QString()> countPvGetter,
      std::function<void(const QString &)> countPvSetter,
      std::array<std::function<QString()>, kCartesianPlotTraceCount> xChannelGetters,
      std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> xChannelSetters,
      std::array<std::function<QString()>, kCartesianPlotTraceCount> yChannelGetters,
      std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> yChannelSetters,
      std::array<std::function<QColor()>, kCartesianPlotTraceCount> colorGetters,
      std::array<std::function<void(const QColor &)>, kCartesianPlotTraceCount> colorSetters,
      std::array<std::function<CartesianPlotYAxis()>, kCartesianPlotTraceCount> axisGetters,
      std::array<std::function<void(CartesianPlotYAxis)>, kCartesianPlotTraceCount> axisSetters,
      std::array<std::function<bool()>, kCartesianPlotTraceCount> sideGetters,
      std::array<std::function<void(bool)>, kCartesianPlotTraceCount> sideSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kCartesianPlot;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    meterForegroundGetter_ = {};
    meterForegroundSetter_ = {};
    meterBackgroundGetter_ = {};
    meterBackgroundSetter_ = {};
    meterLabelGetter_ = {};
    meterLabelSetter_ = {};
    meterColorModeGetter_ = {};
    meterColorModeSetter_ = {};
    meterChannelGetter_ = {};
    meterChannelSetter_ = {};
    meterLimitsGetter_ = {};
    meterLimitsSetter_ = {};

    barForegroundGetter_ = {};
    barForegroundSetter_ = {};
    barBackgroundGetter_ = {};
    barBackgroundSetter_ = {};
    barLabelGetter_ = {};
    barLabelSetter_ = {};
    barColorModeGetter_ = {};
    barColorModeSetter_ = {};
    barDirectionGetter_ = {};
    barDirectionSetter_ = {};
    barFillModeGetter_ = {};
    barFillModeSetter_ = {};
    barChannelGetter_ = {};
    barChannelSetter_ = {};
    barLimitsGetter_ = {};
    barLimitsSetter_ = {};

    scaleForegroundGetter_ = {};
    scaleForegroundSetter_ = {};
    scaleBackgroundGetter_ = {};
    scaleBackgroundSetter_ = {};
    scaleLabelGetter_ = {};
    scaleLabelSetter_ = {};
    scaleColorModeGetter_ = {};
    scaleColorModeSetter_ = {};
    scaleDirectionGetter_ = {};
    scaleDirectionSetter_ = {};
    scaleChannelGetter_ = {};
    scaleChannelSetter_ = {};
    scaleLimitsGetter_ = {};
    scaleLimitsSetter_ = {};

    stripTitleGetter_ = {};
    stripTitleSetter_ = {};
    stripXLabelGetter_ = {};
    stripXLabelSetter_ = {};
    stripYLabelGetter_ = {};
    stripYLabelSetter_ = {};
    stripForegroundGetter_ = {};
    stripForegroundSetter_ = {};
    stripBackgroundGetter_ = {};
    stripBackgroundSetter_ = {};
    stripPeriodGetter_ = {};
    stripPeriodSetter_ = {};
    stripUnitsGetter_ = {};
    stripUnitsSetter_ = {};
    for (auto &getter : stripPenChannelGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenChannelSetters_) {
      setter = {};
    }
    for (auto &getter : stripPenColorGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenColorSetters_) {
      setter = {};
    }
    for (auto &getter : stripPenLimitsGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenLimitsSetters_) {
      setter = {};
    }

    byteForegroundGetter_ = {};
    byteForegroundSetter_ = {};
    byteBackgroundGetter_ = {};
    byteBackgroundSetter_ = {};
    byteColorModeGetter_ = {};
    byteColorModeSetter_ = {};
    byteDirectionGetter_ = {};
    byteDirectionSetter_ = {};
    byteStartBitGetter_ = {};
    byteStartBitSetter_ = {};
    byteEndBitGetter_ = {};
    byteEndBitSetter_ = {};
    byteChannelGetter_ = {};
    byteChannelSetter_ = {};

    cartesianTitleGetter_ = std::move(titleGetter);
    cartesianTitleSetter_ = std::move(titleSetter);
    cartesianXLabelGetter_ = std::move(xLabelGetter);
    cartesianXLabelSetter_ = std::move(xLabelSetter);
    cartesianYLabelGetters_ = std::move(yLabelGetters);
    cartesianYLabelSetters_ = std::move(yLabelSetters);
    cartesianForegroundGetter_ = std::move(foregroundGetter);
    cartesianForegroundSetter_ = std::move(foregroundSetter);
    cartesianBackgroundGetter_ = std::move(backgroundGetter);
    cartesianBackgroundSetter_ = std::move(backgroundSetter);
    cartesianAxisStyleGetters_ = std::move(axisStyleGetters);
    cartesianAxisStyleSetters_ = std::move(axisStyleSetters);
    cartesianAxisRangeGetters_ = std::move(axisRangeGetters);
    cartesianAxisRangeSetters_ = std::move(axisRangeSetters);
    cartesianAxisMinimumGetters_ = std::move(axisMinimumGetters);
    cartesianAxisMinimumSetters_ = std::move(axisMinimumSetters);
    cartesianAxisMaximumGetters_ = std::move(axisMaximumGetters);
    cartesianAxisMaximumSetters_ = std::move(axisMaximumSetters);
    cartesianAxisTimeFormatGetters_ = std::move(axisTimeFormatGetters);
    cartesianAxisTimeFormatSetters_ = std::move(axisTimeFormatSetters);
    cartesianStyleGetter_ = std::move(styleGetter);
    cartesianStyleSetter_ = std::move(styleSetter);
    cartesianEraseOldestGetter_ = std::move(eraseOldestGetter);
    cartesianEraseOldestSetter_ = std::move(eraseOldestSetter);
    cartesianCountGetter_ = std::move(countGetter);
    cartesianCountSetter_ = std::move(countSetter);
    cartesianEraseModeGetter_ = std::move(eraseModeGetter);
    cartesianEraseModeSetter_ = std::move(eraseModeSetter);
    cartesianTriggerGetter_ = std::move(triggerGetter);
    cartesianTriggerSetter_ = std::move(triggerSetter);
    cartesianEraseGetter_ = std::move(eraseGetter);
    cartesianEraseSetter_ = std::move(eraseSetter);
    cartesianCountPvGetter_ = std::move(countPvGetter);
    cartesianCountPvSetter_ = std::move(countPvSetter);
    cartesianTraceXGetters_ = std::move(xChannelGetters);
    cartesianTraceXSetters_ = std::move(xChannelSetters);
    cartesianTraceYGetters_ = std::move(yChannelGetters);
    cartesianTraceYSetters_ = std::move(yChannelSetters);
    cartesianTraceColorGetters_ = std::move(colorGetters);
    cartesianTraceColorSetters_ = std::move(colorSetters);
    cartesianTraceAxisGetters_ = std::move(axisGetters);
    cartesianTraceAxisSetters_ = std::move(axisSetters);
    cartesianTraceSideGetters_ = std::move(sideGetters);
    cartesianTraceSideSetters_ = std::move(sideSetters);

    QRect geometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (geometry.width() <= 0) {
      geometry.setWidth(kMinimumCartesianPlotWidth);
    }
    if (geometry.height() <= 0) {
      geometry.setHeight(kMinimumCartesianPlotHeight);
    }
    lastCommittedGeometry_ = geometry;

    updateGeometryEdits(geometry);

    if (cartesianTitleEdit_) {
      const QString value = cartesianTitleGetter_ ? cartesianTitleGetter_()
                                                 : QString();
      const QSignalBlocker blocker(cartesianTitleEdit_);
      cartesianTitleEdit_->setText(value);
      committedTexts_[cartesianTitleEdit_] = value;
    }

    if (cartesianXLabelEdit_) {
      const QString value = cartesianXLabelGetter_ ? cartesianXLabelGetter_()
                                                   : QString();
      const QSignalBlocker blocker(cartesianXLabelEdit_);
      cartesianXLabelEdit_->setText(value);
      committedTexts_[cartesianXLabelEdit_] = value;
    }

    for (int i = 0; i < 4; ++i) {
      if (!cartesianYLabelEdits_[i]) {
        continue;
      }
      const QString value = cartesianYLabelGetters_[i]
              ? cartesianYLabelGetters_[i]()
              : QString();
      const QSignalBlocker blocker(cartesianYLabelEdits_[i]);
      cartesianYLabelEdits_[i]->setText(value);
      committedTexts_[cartesianYLabelEdits_[i]] = value;
    }

    if (cartesianForegroundButton_) {
      const QColor color = cartesianForegroundGetter_ ? cartesianForegroundGetter_()
                                                      : palette().color(QPalette::WindowText);
      setColorButtonColor(cartesianForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      cartesianForegroundButton_->setEnabled(static_cast<bool>(cartesianForegroundSetter_));
    }

    if (cartesianBackgroundButton_) {
      const QColor color = cartesianBackgroundGetter_ ? cartesianBackgroundGetter_()
                                                      : palette().color(QPalette::Window);
      setColorButtonColor(cartesianBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      cartesianBackgroundButton_->setEnabled(static_cast<bool>(cartesianBackgroundSetter_));
    }

    if (cartesianStyleCombo_) {
      const QSignalBlocker blocker(cartesianStyleCombo_);
      const int index = cartesianStyleGetter_
              ? cartesianPlotStyleToIndex(cartesianStyleGetter_())
              : cartesianPlotStyleToIndex(CartesianPlotStyle::kLine);
      cartesianStyleCombo_->setCurrentIndex(index);
      cartesianStyleCombo_->setEnabled(static_cast<bool>(cartesianStyleSetter_));
    }

    if (cartesianEraseOldestCombo_) {
      const QSignalBlocker blocker(cartesianEraseOldestCombo_);
      const bool eraseOldest = cartesianEraseOldestGetter_
              ? cartesianEraseOldestGetter_()
              : false;
      cartesianEraseOldestCombo_->setCurrentIndex(eraseOldest ? 1 : 0);
      cartesianEraseOldestCombo_->setEnabled(static_cast<bool>(cartesianEraseOldestSetter_));
    }

    if (cartesianCountEdit_) {
      const int countValue = cartesianCountGetter_ ? cartesianCountGetter_() : 1;
      const QSignalBlocker blocker(cartesianCountEdit_);
      cartesianCountEdit_->setText(QString::number(std::max(countValue, 1)));
      cartesianCountEdit_->setEnabled(static_cast<bool>(cartesianCountSetter_));
      committedTexts_[cartesianCountEdit_] = cartesianCountEdit_->text();
    }

    if (cartesianEraseModeCombo_) {
      const QSignalBlocker blocker(cartesianEraseModeCombo_);
      const int index = cartesianEraseModeGetter_
              ? cartesianEraseModeToIndex(cartesianEraseModeGetter_())
              : cartesianEraseModeToIndex(CartesianPlotEraseMode::kIfNotZero);
      cartesianEraseModeCombo_->setCurrentIndex(index);
      cartesianEraseModeCombo_->setEnabled(static_cast<bool>(cartesianEraseModeSetter_));
    }

    if (cartesianTriggerEdit_) {
      const QString value = cartesianTriggerGetter_ ? cartesianTriggerGetter_()
                                                   : QString();
      const QSignalBlocker blocker(cartesianTriggerEdit_);
      cartesianTriggerEdit_->setText(value);
      cartesianTriggerEdit_->setEnabled(static_cast<bool>(cartesianTriggerSetter_));
      committedTexts_[cartesianTriggerEdit_] = value;
    }

    if (cartesianEraseEdit_) {
      const QString value = cartesianEraseGetter_ ? cartesianEraseGetter_()
                                                 : QString();
      const QSignalBlocker blocker(cartesianEraseEdit_);
      cartesianEraseEdit_->setText(value);
      cartesianEraseEdit_->setEnabled(static_cast<bool>(cartesianEraseSetter_));
      committedTexts_[cartesianEraseEdit_] = value;
    }

    if (cartesianCountPvEdit_) {
      const QString value = cartesianCountPvGetter_ ? cartesianCountPvGetter_()
                                                   : QString();
      const QSignalBlocker blocker(cartesianCountPvEdit_);
      cartesianCountPvEdit_->setText(value);
      cartesianCountPvEdit_->setEnabled(static_cast<bool>(cartesianCountPvSetter_));
      committedTexts_[cartesianCountPvEdit_] = value;
    }

    for (int i = 0; i < kCartesianPlotTraceCount; ++i) {
      if (cartesianTraceColorButtons_[i]) {
        const QColor color = cartesianTraceColorGetters_[i]
                ? cartesianTraceColorGetters_[i]()
                : palette().color(QPalette::WindowText);
        setColorButtonColor(cartesianTraceColorButtons_[i],
            color.isValid() ? color : palette().color(QPalette::WindowText));
        cartesianTraceColorButtons_[i]->setEnabled(static_cast<bool>(cartesianTraceColorSetters_[i]));
      }
      if (cartesianTraceXEdits_[i]) {
        const QString value = cartesianTraceXGetters_[i]
                ? cartesianTraceXGetters_[i]()
                : QString();
        const QSignalBlocker blocker(cartesianTraceXEdits_[i]);
        cartesianTraceXEdits_[i]->setText(value);
        cartesianTraceXEdits_[i]->setEnabled(static_cast<bool>(cartesianTraceXSetters_[i]));
        committedTexts_[cartesianTraceXEdits_[i]] = value;
      }
      if (cartesianTraceYEdits_[i]) {
        const QString value = cartesianTraceYGetters_[i]
                ? cartesianTraceYGetters_[i]()
                : QString();
        const QSignalBlocker blocker(cartesianTraceYEdits_[i]);
        cartesianTraceYEdits_[i]->setText(value);
        cartesianTraceYEdits_[i]->setEnabled(static_cast<bool>(cartesianTraceYSetters_[i]));
        committedTexts_[cartesianTraceYEdits_[i]] = value;
      }
      if (cartesianTraceAxisCombos_[i]) {
        const QSignalBlocker blocker(cartesianTraceAxisCombos_[i]);
        const int index = cartesianTraceAxisGetters_[i]
                ? cartesianAxisToIndex(cartesianTraceAxisGetters_[i]())
                : cartesianAxisToIndex(CartesianPlotYAxis::kY1);
        cartesianTraceAxisCombos_[i]->setCurrentIndex(index);
        cartesianTraceAxisCombos_[i]->setEnabled(static_cast<bool>(cartesianTraceAxisSetters_[i]));
      }
      if (cartesianTraceSideCombos_[i]) {
        const QSignalBlocker blocker(cartesianTraceSideCombos_[i]);
        const bool usesRight = cartesianTraceSideGetters_[i]
                ? cartesianTraceSideGetters_[i]()
                : false;
        cartesianTraceSideCombos_[i]->setCurrentIndex(usesRight ? 1 : 0);
        cartesianTraceSideCombos_[i]->setEnabled(static_cast<bool>(cartesianTraceSideSetters_[i]));
      }
    }

    updateCartesianAxisButtonState();

    elementLabel_->setText(QStringLiteral("Cartesian Plot"));

    showPaletteWithoutActivating();
  }

  void showForByteMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<BarDirection()> directionGetter,
      std::function<void(BarDirection)> directionSetter,
      std::function<int()> startBitGetter,
      std::function<void(int)> startBitSetter,
      std::function<int()> endBitGetter,
      std::function<void(int)> endBitSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kByteMonitor;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    byteForegroundGetter_ = std::move(foregroundGetter);
    byteForegroundSetter_ = std::move(foregroundSetter);
    byteBackgroundGetter_ = std::move(backgroundGetter);
    byteBackgroundSetter_ = std::move(backgroundSetter);
    byteColorModeGetter_ = std::move(colorModeGetter);
    byteColorModeSetter_ = std::move(colorModeSetter);
    byteDirectionGetter_ = std::move(directionGetter);
    byteDirectionSetter_ = std::move(directionSetter);
    byteStartBitGetter_ = std::move(startBitGetter);
    byteStartBitSetter_ = std::move(startBitSetter);
    byteEndBitGetter_ = std::move(endBitGetter);
    byteEndBitSetter_ = std::move(endBitSetter);
    byteChannelGetter_ = std::move(channelGetter);
    byteChannelSetter_ = std::move(channelSetter);

    QRect byteGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (byteGeometry.width() <= 0) {
      byteGeometry.setWidth(kMinimumByteSize);
    }
    if (byteGeometry.height() <= 0) {
      byteGeometry.setHeight(kMinimumByteSize);
    }
    lastCommittedGeometry_ = byteGeometry;

    updateGeometryEdits(byteGeometry);

    if (byteForegroundButton_) {
      const QColor color = byteForegroundGetter_ ? byteForegroundGetter_()
                                                : palette().color(QPalette::WindowText);
      setColorButtonColor(byteForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (byteBackgroundButton_) {
      const QColor color = byteBackgroundGetter_ ? byteBackgroundGetter_()
                                                : palette().color(QPalette::Window);
      setColorButtonColor(byteBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (byteColorModeCombo_) {
      const QSignalBlocker blocker(byteColorModeCombo_);
      const TextColorMode mode = byteColorModeGetter_ ? byteColorModeGetter_()
                                                     : TextColorMode::kStatic;
      byteColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (byteDirectionCombo_) {
      const QSignalBlocker blocker(byteDirectionCombo_);
      const BarDirection direction = byteDirectionGetter_ ? byteDirectionGetter_()
                                                         : BarDirection::kRight;
      byteDirectionCombo_->setCurrentIndex(barDirectionToIndex(direction));
    }

    if (byteStartBitSpin_) {
      const QSignalBlocker blocker(byteStartBitSpin_);
      int value = byteStartBitGetter_ ? byteStartBitGetter_() : 15;
      value = std::clamp(value, 0, 31);
      byteStartBitSpin_->setValue(value);
      byteStartBitSpin_->setEnabled(static_cast<bool>(byteStartBitSetter_));
    }

    if (byteEndBitSpin_) {
      const QSignalBlocker blocker(byteEndBitSpin_);
      int value = byteEndBitGetter_ ? byteEndBitGetter_() : 0;
      value = std::clamp(value, 0, 31);
      byteEndBitSpin_->setValue(value);
      byteEndBitSpin_->setEnabled(static_cast<bool>(byteEndBitSetter_));
    }

    if (byteChannelEdit_) {
      const QString channel = byteChannelGetter_ ? byteChannelGetter_()
                                                 : QString();
      const QSignalBlocker blocker(byteChannelEdit_);
      byteChannelEdit_->setText(channel);
      committedTexts_[byteChannelEdit_] = byteChannelEdit_->text();
      byteChannelEdit_->setEnabled(static_cast<bool>(byteChannelSetter_));
    }

    elementLabel_->setText(QStringLiteral("Byte Monitor"));

    showPaletteWithoutActivating();
  }


  void showForComposite(std::function<QRect()> geometryGetter,
    std::function<void(const QRect &)> geometrySetter,
    std::function<QColor()> foregroundGetter,
    std::function<void(const QColor &)> foregroundSetter,
    std::function<QColor()> backgroundGetter,
    std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> fileGetter,
      std::function<void(const QString &)> fileSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters,
      const QString &elementLabel = QStringLiteral("Composite"))
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kComposite;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    compositeForegroundGetter_ = std::move(foregroundGetter);
    compositeForegroundSetter_ = std::move(foregroundSetter);
    compositeBackgroundGetter_ = std::move(backgroundGetter);
    compositeBackgroundSetter_ = std::move(backgroundSetter);
    compositeFileGetter_ = std::move(fileGetter);
    compositeFileSetter_ = std::move(fileSetter);
    compositeVisibilityModeGetter_ = std::move(visibilityModeGetter);
    compositeVisibilityModeSetter_ = std::move(visibilityModeSetter);
    compositeVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    compositeVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    compositeChannelGetters_ = std::move(channelGetters);
    compositeChannelSetters_ = std::move(channelSetters);

    if (compositeForegroundButton_) {
      const QColor color = compositeForegroundGetter_ ? compositeForegroundGetter_()
                                                      : palette().color(QPalette::WindowText);
      setColorButtonColor(compositeForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      compositeForegroundButton_->setEnabled(static_cast<bool>(compositeForegroundSetter_));
    }

    if (compositeBackgroundButton_) {
      const QColor color = compositeBackgroundGetter_ ? compositeBackgroundGetter_()
                                                      : palette().color(QPalette::Window);
      setColorButtonColor(compositeBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      compositeBackgroundButton_->setEnabled(static_cast<bool>(compositeBackgroundSetter_));
    }

    QRect compositeGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (compositeGeometry.width() <= 0) {
      compositeGeometry.setWidth(1);
    }
    if (compositeGeometry.height() <= 0) {
      compositeGeometry.setHeight(1);
    }
    lastCommittedGeometry_ = compositeGeometry;
    updateGeometryEdits(compositeGeometry);

    if (compositeFileEdit_) {
      const QString file = compositeFileGetter_ ? compositeFileGetter_() : QString();
      const QSignalBlocker blocker(compositeFileEdit_);
      compositeFileEdit_->setText(file);
      compositeFileEdit_->setEnabled(static_cast<bool>(compositeFileSetter_));
      committedTexts_[compositeFileEdit_] = compositeFileEdit_->text();
    }

    if (compositeVisibilityCombo_) {
      const QSignalBlocker blocker(compositeVisibilityCombo_);
      const TextVisibilityMode mode = compositeVisibilityModeGetter_
          ? compositeVisibilityModeGetter_()
          : TextVisibilityMode::kStatic;
      compositeVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
      compositeVisibilityCombo_->setEnabled(static_cast<bool>(compositeVisibilityModeSetter_));
    }

    if (compositeVisibilityCalcEdit_) {
      const QString calc = compositeVisibilityCalcGetter_
          ? compositeVisibilityCalcGetter_()
          : QString();
      const QSignalBlocker blocker(compositeVisibilityCalcEdit_);
      compositeVisibilityCalcEdit_->setText(calc);
      compositeVisibilityCalcEdit_->setEnabled(static_cast<bool>(compositeVisibilityCalcSetter_));
      committedTexts_[compositeVisibilityCalcEdit_] = compositeVisibilityCalcEdit_->text();
    }

    for (int i = 0; i < static_cast<int>(compositeChannelEdits_.size()); ++i) {
      QLineEdit *edit = compositeChannelEdits_[i];
      if (!edit) {
        continue;
      }
      const QString value = compositeChannelGetters_[i]
          ? compositeChannelGetters_[i]()
          : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      edit->setEnabled(static_cast<bool>(compositeChannelSetters_[i]));
      committedTexts_[edit] = edit->text();
    }

    updateCompositeChannelDependentControls();

    elementLabel_->setText(elementLabel);

    showPaletteWithoutActivating();
  }

  void showForRectangle(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> colorGetter,
      std::function<void(const QColor &)> colorSetter,
      std::function<RectangleFill()> fillGetter,
      std::function<void(RectangleFill)> fillSetter,
      std::function<RectangleLineStyle()> lineStyleGetter,
      std::function<void(RectangleLineStyle)> lineStyleSetter,
      std::function<int()> lineWidthGetter,
      std::function<void(int)> lineWidthSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters,
      const QString &elementLabel = QStringLiteral("Rectangle"),
      bool treatAsPolygon = false,
      std::function<int()> arcBeginGetter = {},
      std::function<void(int)> arcBeginSetter = {},
      std::function<int()> arcPathGetter = {},
      std::function<void(int)> arcPathSetter = {})
  {
    clearSelectionState();
    const bool hasArcAngles = static_cast<bool>(arcBeginGetter)
        || static_cast<bool>(arcPathGetter)
        || static_cast<bool>(arcBeginSetter)
        || static_cast<bool>(arcPathSetter);
    rectangleIsArc_ = hasArcAngles;
    arcBeginGetter_ = std::move(arcBeginGetter);
    arcBeginSetter_ = std::move(arcBeginSetter);
    arcPathGetter_ = std::move(arcPathGetter);
    arcPathSetter_ = std::move(arcPathSetter);

    selectionKind_ = treatAsPolygon ? SelectionKind::kPolygon
                                    : SelectionKind::kRectangle;
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    rectangleForegroundGetter_ = std::move(colorGetter);
    rectangleForegroundSetter_ = std::move(colorSetter);
    rectangleFillGetter_ = std::move(fillGetter);
    rectangleFillSetter_ = std::move(fillSetter);
    rectangleLineStyleGetter_ = std::move(lineStyleGetter);
    rectangleLineStyleSetter_ = std::move(lineStyleSetter);
    rectangleLineWidthGetter_ = std::move(lineWidthGetter);
    rectangleLineWidthSetter_ = std::move(lineWidthSetter);
    rectangleColorModeGetter_ = std::move(colorModeGetter);
    rectangleColorModeSetter_ = std::move(colorModeSetter);
    rectangleVisibilityModeGetter_ = std::move(visibilityModeGetter);
    rectangleVisibilityModeSetter_ = std::move(visibilityModeSetter);
    rectangleVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    rectangleVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    rectangleChannelGetters_ = std::move(channelGetters);
    rectangleChannelSetters_ = std::move(channelSetters);

    QRect rectGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (rectGeometry.width() <= 0) {
      rectGeometry.setWidth(1);
    }
    if (rectGeometry.height() <= 0) {
      rectGeometry.setHeight(1);
    }
    lastCommittedGeometry_ = rectGeometry;
    updateGeometryEdits(rectGeometry);

    if (rectangleForegroundButton_) {
      const QColor color = rectangleForegroundGetter_ ? rectangleForegroundGetter_()
                                                      : palette().color(
                                                            QPalette::WindowText);
      setColorButtonColor(rectangleForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (rectangleFillCombo_) {
      const QSignalBlocker blocker(rectangleFillCombo_);
      const RectangleFill fill = rectangleFillGetter_ ? rectangleFillGetter_()
                                                     : RectangleFill::kOutline;
      rectangleFillCombo_->setCurrentIndex(fillToIndex(fill));
    }

    if (rectangleLineStyleCombo_) {
      const QSignalBlocker blocker(rectangleLineStyleCombo_);
      const RectangleLineStyle style = rectangleLineStyleGetter_ ? rectangleLineStyleGetter_()
                                                                 : RectangleLineStyle::kSolid;
      rectangleLineStyleCombo_->setCurrentIndex(lineStyleToIndex(style));
    }

    if (rectangleLineWidthEdit_) {
      const int width = rectangleLineWidthGetter_ ? rectangleLineWidthGetter_()
                                                 : 1;
      const int clampedWidth = std::max(1, width);
      const QSignalBlocker blocker(rectangleLineWidthEdit_);
      rectangleLineWidthEdit_->setText(QString::number(clampedWidth));
      committedTexts_[rectangleLineWidthEdit_] = rectangleLineWidthEdit_->text();
    }

    if (arcBeginSpin_) {
      const QSignalBlocker blocker(arcBeginSpin_);
      const int angle = arcBeginGetter_ ? arcBeginGetter_() : 0;
      arcBeginSpin_->setValue(angle64ToDegrees(angle));
      arcBeginSpin_->setEnabled(rectangleIsArc_ && static_cast<bool>(arcBeginSetter_));
    }

    if (arcPathSpin_) {
      const QSignalBlocker blocker(arcPathSpin_);
      const int angle = arcPathGetter_ ? arcPathGetter_() : 0;
      arcPathSpin_->setValue(angle64ToDegrees(angle));
      arcPathSpin_->setEnabled(rectangleIsArc_ && static_cast<bool>(arcPathSetter_));
    }

    if (rectangleColorModeCombo_) {
      const QSignalBlocker blocker(rectangleColorModeCombo_);
      const TextColorMode mode = rectangleColorModeGetter_ ? rectangleColorModeGetter_()
                                                          : TextColorMode::kStatic;
      rectangleColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (rectangleVisibilityCombo_) {
      const QSignalBlocker blocker(rectangleVisibilityCombo_);
      const TextVisibilityMode mode = rectangleVisibilityModeGetter_
          ? rectangleVisibilityModeGetter_()
          : TextVisibilityMode::kStatic;
      rectangleVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
    }

    if (rectangleVisibilityCalcEdit_) {
      const QString calc = rectangleVisibilityCalcGetter_ ? rectangleVisibilityCalcGetter_()
                                                          : QString();
      const QSignalBlocker blocker(rectangleVisibilityCalcEdit_);
      rectangleVisibilityCalcEdit_->setText(calc);
      committedTexts_[rectangleVisibilityCalcEdit_] = rectangleVisibilityCalcEdit_->text();
    }

    for (int i = 0; i < static_cast<int>(rectangleChannelEdits_.size()); ++i) {
      QLineEdit *edit = rectangleChannelEdits_[i];
      if (!edit) {
        continue;
      }
      const QString value = rectangleChannelGetters_[i]
          ? rectangleChannelGetters_[i]()
          : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      committedTexts_[edit] = edit->text();
    }

    updateRectangleChannelDependentControls();

    elementLabel_->setText(elementLabel);

    showPaletteWithoutActivating();
  }

  void showForImage(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<ImageType()> typeGetter,
      std::function<void(ImageType)> typeSetter,
      std::function<QString()> nameGetter,
      std::function<void(const QString &)> nameSetter,
      std::function<QString()> calcGetter,
      std::function<void(const QString &)> calcSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kImage;
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

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    rectangleForegroundGetter_ = {};
    rectangleForegroundSetter_ = {};
    rectangleFillGetter_ = {};
    rectangleFillSetter_ = {};
    rectangleLineStyleGetter_ = {};
    rectangleLineStyleSetter_ = {};
    rectangleLineWidthGetter_ = {};
    rectangleLineWidthSetter_ = {};
    rectangleColorModeGetter_ = {};
    rectangleColorModeSetter_ = {};
    rectangleVisibilityModeGetter_ = {};
    rectangleVisibilityModeSetter_ = {};
    rectangleVisibilityCalcGetter_ = {};
    rectangleVisibilityCalcSetter_ = {};
    for (auto &getter : rectangleChannelGetters_) {
      getter = {};
    }
    for (auto &setter : rectangleChannelSetters_) {
      setter = {};
    }

    arcBeginGetter_ = {};
    arcBeginSetter_ = {};
    arcPathGetter_ = {};
    arcPathSetter_ = {};
    rectangleIsArc_ = false;

    lineColorGetter_ = {};
    lineColorSetter_ = {};
    lineLineStyleGetter_ = {};
    lineLineStyleSetter_ = {};
    lineLineWidthGetter_ = {};
    lineLineWidthSetter_ = {};
    lineColorModeGetter_ = {};
    lineColorModeSetter_ = {};
    lineVisibilityModeGetter_ = {};
    lineVisibilityModeSetter_ = {};
    lineVisibilityCalcGetter_ = {};
    lineVisibilityCalcSetter_ = {};
    for (auto &getter : lineChannelGetters_) {
      getter = {};
    }
    for (auto &setter : lineChannelSetters_) {
      setter = {};
    }

    imageTypeGetter_ = std::move(typeGetter);
    imageTypeSetter_ = std::move(typeSetter);
    imageNameGetter_ = std::move(nameGetter);
    imageNameSetter_ = std::move(nameSetter);
    imageCalcGetter_ = std::move(calcGetter);
    imageCalcSetter_ = std::move(calcSetter);
    imageColorModeGetter_ = std::move(colorModeGetter);
    imageColorModeSetter_ = std::move(colorModeSetter);
    imageVisibilityModeGetter_ = std::move(visibilityModeGetter);
    imageVisibilityModeSetter_ = std::move(visibilityModeSetter);
    imageVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    imageVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    imageChannelGetters_ = std::move(channelGetters);
    imageChannelSetters_ = std::move(channelSetters);

    QRect imageGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (imageGeometry.width() <= 0) {
      imageGeometry.setWidth(1);
    }
    if (imageGeometry.height() <= 0) {
      imageGeometry.setHeight(1);
    }
    lastCommittedGeometry_ = imageGeometry;
    updateGeometryEdits(imageGeometry);

    if (imageTypeCombo_) {
      const QSignalBlocker blocker(imageTypeCombo_);
      const ImageType type = imageTypeGetter_ ? imageTypeGetter_()
                                              : ImageType::kNone;
      imageTypeCombo_->setCurrentIndex(imageTypeToIndex(type));
    }

    if (imageNameEdit_) {
      const QString name = imageNameGetter_ ? imageNameGetter_() : QString();
      const QSignalBlocker blocker(imageNameEdit_);
      imageNameEdit_->setText(name);
      committedTexts_[imageNameEdit_] = imageNameEdit_->text();
    }

    if (imageCalcEdit_) {
      const QString calc = imageCalcGetter_ ? imageCalcGetter_() : QString();
      const QSignalBlocker blocker(imageCalcEdit_);
      imageCalcEdit_->setText(calc);
      committedTexts_[imageCalcEdit_] = imageCalcEdit_->text();
    }

    if (imageColorModeCombo_) {
      const QSignalBlocker blocker(imageColorModeCombo_);
      const TextColorMode mode = imageColorModeGetter_ ? imageColorModeGetter_()
                                                      : TextColorMode::kStatic;
      imageColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (imageVisibilityCombo_) {
      const QSignalBlocker blocker(imageVisibilityCombo_);
      const TextVisibilityMode mode = imageVisibilityModeGetter_
          ? imageVisibilityModeGetter_()
          : TextVisibilityMode::kStatic;
      imageVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
    }

    if (imageVisibilityCalcEdit_) {
      const QString calc = imageVisibilityCalcGetter_
              ? imageVisibilityCalcGetter_()
              : QString();
      const QSignalBlocker blocker(imageVisibilityCalcEdit_);
      imageVisibilityCalcEdit_->setText(calc);
      committedTexts_[imageVisibilityCalcEdit_] = imageVisibilityCalcEdit_->text();
    }

    for (int i = 0; i < static_cast<int>(imageChannelEdits_.size()); ++i) {
      QLineEdit *edit = imageChannelEdits_[i];
      if (!edit) {
        continue;
      }
      const QString value = imageChannelGetters_[i]
          ? imageChannelGetters_[i]()
          : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      committedTexts_[edit] = edit->text();
    }

    updateImageChannelDependentControls();

    elementLabel_->setText(QStringLiteral("Image"));

    showPaletteWithoutActivating();
  }

  void showForHeatmap(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> dataChannelGetter,
      std::function<void(const QString &)> dataChannelSetter,
      std::function<HeatmapDimensionSource()> xSourceGetter,
      std::function<void(HeatmapDimensionSource)> xSourceSetter,
      std::function<HeatmapDimensionSource()> ySourceGetter,
      std::function<void(HeatmapDimensionSource)> ySourceSetter,
      std::function<int()> xDimGetter,
      std::function<void(int)> xDimSetter,
      std::function<int()> yDimGetter,
      std::function<void(int)> yDimSetter,
      std::function<QString()> xDimChannelGetter,
      std::function<void(const QString &)> xDimChannelSetter,
      std::function<QString()> yDimChannelGetter,
      std::function<void(const QString &)> yDimChannelSetter,
      std::function<HeatmapOrder()> orderGetter,
      std::function<void(HeatmapOrder)> orderSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kHeatmap;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);

    heatmapTitleGetter_ = std::move(titleGetter);
    heatmapTitleSetter_ = std::move(titleSetter);
    heatmapDataChannelGetter_ = std::move(dataChannelGetter);
    heatmapDataChannelSetter_ = std::move(dataChannelSetter);
    heatmapXSourceGetter_ = std::move(xSourceGetter);
    heatmapXSourceSetter_ = std::move(xSourceSetter);
    heatmapYSourceGetter_ = std::move(ySourceGetter);
    heatmapYSourceSetter_ = std::move(ySourceSetter);
    heatmapXDimensionGetter_ = std::move(xDimGetter);
    heatmapXDimensionSetter_ = std::move(xDimSetter);
    heatmapYDimensionGetter_ = std::move(yDimGetter);
    heatmapYDimensionSetter_ = std::move(yDimSetter);
    heatmapXDimChannelGetter_ = std::move(xDimChannelGetter);
    heatmapXDimChannelSetter_ = std::move(xDimChannelSetter);
    heatmapYDimChannelGetter_ = std::move(yDimChannelGetter);
    heatmapYDimChannelSetter_ = std::move(yDimChannelSetter);
    heatmapOrderGetter_ = std::move(orderGetter);
    heatmapOrderSetter_ = std::move(orderSetter);

    QRect heatmapGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (heatmapGeometry.width() <= 0) {
      heatmapGeometry.setWidth(1);
    }
    if (heatmapGeometry.height() <= 0) {
      heatmapGeometry.setHeight(1);
    }
    lastCommittedGeometry_ = heatmapGeometry;
    updateGeometryEdits(heatmapGeometry);

    if (heatmapTitleEdit_) {
      const QString value = heatmapTitleGetter_ ? heatmapTitleGetter_() : QString();
      const QSignalBlocker blocker(heatmapTitleEdit_);
      heatmapTitleEdit_->setText(value);
      committedTexts_[heatmapTitleEdit_] = heatmapTitleEdit_->text();
    }

    if (heatmapDataChannelEdit_) {
      const QString value = heatmapDataChannelGetter_ ? heatmapDataChannelGetter_() : QString();
      const QSignalBlocker blocker(heatmapDataChannelEdit_);
      heatmapDataChannelEdit_->setText(value);
      committedTexts_[heatmapDataChannelEdit_] = heatmapDataChannelEdit_->text();
    }

    if (heatmapXSourceCombo_) {
      const QSignalBlocker blocker(heatmapXSourceCombo_);
      const HeatmapDimensionSource source = heatmapXSourceGetter_
          ? heatmapXSourceGetter_()
          : HeatmapDimensionSource::kStatic;
      heatmapXSourceCombo_->setCurrentIndex(heatmapDimensionSourceToIndex(source));
    }

    if (heatmapYSourceCombo_) {
      const QSignalBlocker blocker(heatmapYSourceCombo_);
      const HeatmapDimensionSource source = heatmapYSourceGetter_
          ? heatmapYSourceGetter_()
          : HeatmapDimensionSource::kStatic;
      heatmapYSourceCombo_->setCurrentIndex(heatmapDimensionSourceToIndex(source));
    }

    if (heatmapXDimEdit_) {
      const int value = heatmapXDimensionGetter_ ? heatmapXDimensionGetter_() : 0;
      const QSignalBlocker blocker(heatmapXDimEdit_);
      heatmapXDimEdit_->setText(QString::number(std::max(0, value)));
      committedTexts_[heatmapXDimEdit_] = heatmapXDimEdit_->text();
    }

    if (heatmapYDimEdit_) {
      const int value = heatmapYDimensionGetter_ ? heatmapYDimensionGetter_() : 0;
      const QSignalBlocker blocker(heatmapYDimEdit_);
      heatmapYDimEdit_->setText(QString::number(std::max(0, value)));
      committedTexts_[heatmapYDimEdit_] = heatmapYDimEdit_->text();
    }

    if (heatmapXDimChannelEdit_) {
      const QString value = heatmapXDimChannelGetter_ ? heatmapXDimChannelGetter_() : QString();
      const QSignalBlocker blocker(heatmapXDimChannelEdit_);
      heatmapXDimChannelEdit_->setText(value);
      committedTexts_[heatmapXDimChannelEdit_] = heatmapXDimChannelEdit_->text();
    }

    if (heatmapYDimChannelEdit_) {
      const QString value = heatmapYDimChannelGetter_ ? heatmapYDimChannelGetter_() : QString();
      const QSignalBlocker blocker(heatmapYDimChannelEdit_);
      heatmapYDimChannelEdit_->setText(value);
      committedTexts_[heatmapYDimChannelEdit_] = heatmapYDimChannelEdit_->text();
    }

    if (heatmapOrderCombo_) {
      const QSignalBlocker blocker(heatmapOrderCombo_);
      const HeatmapOrder order = heatmapOrderGetter_ ? heatmapOrderGetter_()
                                                    : HeatmapOrder::kRowMajor;
      heatmapOrderCombo_->setCurrentIndex(heatmapOrderToIndex(order));
    }

    updateHeatmapDimensionControls();
    elementLabel_->setText(QStringLiteral("Heatmap"));
    showPaletteWithoutActivating();
  }

  void showForLine(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> colorGetter,
      std::function<void(const QColor &)> colorSetter,
      std::function<RectangleLineStyle()> lineStyleGetter,
      std::function<void(RectangleLineStyle)> lineStyleSetter,
      std::function<int()> lineWidthGetter,
      std::function<void(int)> lineWidthSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
    std::array<std::function<QString()>, 4> channelGetters,
    std::array<std::function<void(const QString &)>, 4> channelSetters,
    const QString &elementLabel = QStringLiteral("Line"))
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kLine;
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

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    rectangleForegroundGetter_ = {};
    rectangleForegroundSetter_ = {};
    rectangleFillGetter_ = {};
    rectangleFillSetter_ = {};
    rectangleLineStyleGetter_ = {};
    rectangleLineStyleSetter_ = {};
    rectangleLineWidthGetter_ = {};
    rectangleLineWidthSetter_ = {};
    rectangleColorModeGetter_ = {};
    rectangleColorModeSetter_ = {};
    rectangleVisibilityModeGetter_ = {};
    rectangleVisibilityModeSetter_ = {};
    rectangleVisibilityCalcGetter_ = {};
    rectangleVisibilityCalcSetter_ = {};
    for (auto &getter : rectangleChannelGetters_) {
      getter = {};
    }
    for (auto &setter : rectangleChannelSetters_) {
      setter = {};
    }

    lineColorGetter_ = std::move(colorGetter);
    lineColorSetter_ = std::move(colorSetter);
    lineLineStyleGetter_ = std::move(lineStyleGetter);
    lineLineStyleSetter_ = std::move(lineStyleSetter);
    lineLineWidthGetter_ = std::move(lineWidthGetter);
    lineLineWidthSetter_ = std::move(lineWidthSetter);
    lineColorModeGetter_ = std::move(colorModeGetter);
    lineColorModeSetter_ = std::move(colorModeSetter);
    lineVisibilityModeGetter_ = std::move(visibilityModeGetter);
    lineVisibilityModeSetter_ = std::move(visibilityModeSetter);
    lineVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    lineVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    lineChannelGetters_ = std::move(channelGetters);
    lineChannelSetters_ = std::move(channelSetters);

    QRect lineGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (lineGeometry.width() <= 0) {
      lineGeometry.setWidth(1);
    }
    if (lineGeometry.height() <= 0) {
      lineGeometry.setHeight(1);
    }
    lastCommittedGeometry_ = lineGeometry;
    updateGeometryEdits(lineGeometry);

    if (lineColorButton_) {
      const QColor color = lineColorGetter_ ? lineColorGetter_()
                                            : palette().color(
                                                  QPalette::WindowText);
      setColorButtonColor(lineColorButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (lineLineStyleCombo_) {
      const QSignalBlocker blocker(lineLineStyleCombo_);
      const RectangleLineStyle style = lineLineStyleGetter_ ? lineLineStyleGetter_()
                                                           : RectangleLineStyle::kSolid;
      lineLineStyleCombo_->setCurrentIndex(lineStyleToIndex(style));
    }

    if (lineLineWidthEdit_) {
      const int width = lineLineWidthGetter_ ? lineLineWidthGetter_() : 1;
      const int clampedWidth = std::max(1, width);
      const QSignalBlocker blocker(lineLineWidthEdit_);
      lineLineWidthEdit_->setText(QString::number(clampedWidth));
      committedTexts_[lineLineWidthEdit_] = lineLineWidthEdit_->text();
    }

    if (lineColorModeCombo_) {
      const QSignalBlocker blocker(lineColorModeCombo_);
      const TextColorMode mode = lineColorModeGetter_ ? lineColorModeGetter_()
                                                     : TextColorMode::kStatic;
      lineColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (lineVisibilityCombo_) {
      const QSignalBlocker blocker(lineVisibilityCombo_);
      const TextVisibilityMode mode = lineVisibilityModeGetter_
          ? lineVisibilityModeGetter_()
          : TextVisibilityMode::kStatic;
      lineVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
    }

    if (lineVisibilityCalcEdit_) {
      const QString calc = lineVisibilityCalcGetter_ ? lineVisibilityCalcGetter_()
                                                     : QString();
      const QSignalBlocker blocker(lineVisibilityCalcEdit_);
      lineVisibilityCalcEdit_->setText(calc);
      committedTexts_[lineVisibilityCalcEdit_] = lineVisibilityCalcEdit_->text();
    }

    for (int i = 0; i < static_cast<int>(lineChannelEdits_.size()); ++i) {
      QLineEdit *edit = lineChannelEdits_[i];
      if (!edit) {
        continue;
      }
      const QString value = lineChannelGetters_[i]
          ? lineChannelGetters_[i]()
          : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      committedTexts_[edit] = edit->text();
    }

    updateLineChannelDependentControls();

    elementLabel_->setText(elementLabel);

    showPaletteWithoutActivating();
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
    textMonitorForegroundGetter_ = {};
    textMonitorForegroundSetter_ = {};
    textMonitorBackgroundGetter_ = {};
    textMonitorBackgroundSetter_ = {};
    textMonitorAlignmentGetter_ = {};
    textMonitorAlignmentSetter_ = {};
    textMonitorFormatGetter_ = {};
    textMonitorFormatSetter_ = {};
    textMonitorPrecisionGetter_ = {};
    textMonitorPrecisionSetter_ = {};
    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};
    textMonitorColorModeGetter_ = {};
    textMonitorColorModeSetter_ = {};
    textMonitorChannelGetter_ = {};
    textMonitorChannelSetter_ = {};
    textEntryForegroundGetter_ = {};
    textEntryForegroundSetter_ = {};
    textEntryBackgroundGetter_ = {};
    textEntryBackgroundSetter_ = {};
    textEntryFormatGetter_ = {};
    textEntryFormatSetter_ = {};
    textEntryPrecisionGetter_ = {};
    textEntryPrecisionSetter_ = {};
    textEntryPrecisionSourceGetter_ = {};
    textEntryPrecisionSourceSetter_ = {};
    textEntryPrecisionDefaultGetter_ = {};
    textEntryPrecisionDefaultSetter_ = {};
    textEntryColorModeGetter_ = {};
    textEntryColorModeSetter_ = {};
    textEntryChannelGetter_ = {};
    textEntryChannelSetter_ = {};
    sliderForegroundGetter_ = {};
    sliderForegroundSetter_ = {};
    sliderBackgroundGetter_ = {};
    sliderBackgroundSetter_ = {};
    sliderLabelGetter_ = {};
    sliderLabelSetter_ = {};
    sliderColorModeGetter_ = {};
    sliderColorModeSetter_ = {};
    sliderDirectionGetter_ = {};
    sliderDirectionSetter_ = {};
    sliderIncrementGetter_ = {};
    sliderIncrementSetter_ = {};
    sliderChannelGetter_ = {};
    sliderChannelSetter_ = {};
    sliderLimitsGetter_ = {};
    sliderLimitsSetter_ = {};
    choiceButtonForegroundGetter_ = {};
    choiceButtonForegroundSetter_ = {};
    choiceButtonBackgroundGetter_ = {};
    choiceButtonBackgroundSetter_ = {};
    choiceButtonColorModeGetter_ = {};
    choiceButtonColorModeSetter_ = {};
    choiceButtonStackingGetter_ = {};
    choiceButtonStackingSetter_ = {};
    choiceButtonChannelGetter_ = {};
    choiceButtonChannelSetter_ = {};
    menuForegroundGetter_ = {};
    menuForegroundSetter_ = {};
    menuBackgroundGetter_ = {};
    menuBackgroundSetter_ = {};
    menuColorModeGetter_ = {};
    menuColorModeSetter_ = {};
    menuChannelGetter_ = {};
    menuChannelSetter_ = {};
    messageButtonForegroundGetter_ = {};
    messageButtonForegroundSetter_ = {};
    messageButtonBackgroundGetter_ = {};
    messageButtonBackgroundSetter_ = {};
    messageButtonColorModeGetter_ = {};
    messageButtonColorModeSetter_ = {};
    messageButtonLabelGetter_ = {};
    messageButtonLabelSetter_ = {};
    messageButtonPressGetter_ = {};
    messageButtonPressSetter_ = {};
    messageButtonReleaseGetter_ = {};
    messageButtonReleaseSetter_ = {};
    messageButtonChannelGetter_ = {};
    messageButtonChannelSetter_ = {};
    if (messageButtonColorModeCombo_) {
      const QSignalBlocker blocker(messageButtonColorModeCombo_);
      messageButtonColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (messageButtonLabelEdit_) {
      const QSignalBlocker blocker(messageButtonLabelEdit_);
      messageButtonLabelEdit_->clear();
      committedTexts_[messageButtonLabelEdit_] = messageButtonLabelEdit_->text();
    }
    if (messageButtonPressEdit_) {
      const QSignalBlocker blocker(messageButtonPressEdit_);
      messageButtonPressEdit_->clear();
      committedTexts_[messageButtonPressEdit_] = messageButtonPressEdit_->text();
    }
    if (messageButtonReleaseEdit_) {
      const QSignalBlocker blocker(messageButtonReleaseEdit_);
      messageButtonReleaseEdit_->clear();
      committedTexts_[messageButtonReleaseEdit_] = messageButtonReleaseEdit_->text();
    }
    if (messageButtonChannelEdit_) {
      const QSignalBlocker blocker(messageButtonChannelEdit_);
      messageButtonChannelEdit_->clear();
      committedTexts_[messageButtonChannelEdit_] = messageButtonChannelEdit_->text();
    }
    shellCommandForegroundGetter_ = {};
    shellCommandForegroundSetter_ = {};
    shellCommandBackgroundGetter_ = {};
    shellCommandBackgroundSetter_ = {};
    shellCommandLabelGetter_ = {};
    shellCommandLabelSetter_ = {};
    for (auto &getter : shellCommandEntryLabelGetters_) {
      getter = {};
    }
    for (auto &setter : shellCommandEntryLabelSetters_) {
      setter = {};
    }
    for (auto &getter : shellCommandEntryCommandGetters_) {
      getter = {};
    }
    for (auto &setter : shellCommandEntryCommandSetters_) {
      setter = {};
    }
    for (auto &getter : shellCommandEntryArgsGetters_) {
      getter = {};
    }
    for (auto &setter : shellCommandEntryArgsSetters_) {
      setter = {};
    }
    if (shellCommandForegroundButton_) {
      shellCommandForegroundButton_->setEnabled(false);
    }
    if (shellCommandBackgroundButton_) {
      shellCommandBackgroundButton_->setEnabled(false);
    }
    if (shellCommandLabelEdit_) {
      const QSignalBlocker blocker(shellCommandLabelEdit_);
      shellCommandLabelEdit_->clear();
      shellCommandLabelEdit_->setEnabled(false);
      committedTexts_[shellCommandLabelEdit_] = shellCommandLabelEdit_->text();
    }
    for (QLineEdit *edit : shellCommandEntryLabelEdits_) {
      if (edit) {
        const QSignalBlocker blocker(edit);
        edit->clear();
        edit->setEnabled(false);
        committedTexts_[edit] = edit->text();
      }
    }
    for (QLineEdit *edit : shellCommandEntryCommandEdits_) {
      if (edit) {
        const QSignalBlocker blocker(edit);
        edit->clear();
        edit->setEnabled(false);
        committedTexts_[edit] = edit->text();
      }
    }
    for (QLineEdit *edit : shellCommandEntryArgsEdits_) {
      if (edit) {
        const QSignalBlocker blocker(edit);
        edit->clear();
        edit->setEnabled(false);
        committedTexts_[edit] = edit->text();
      }
    }
    relatedDisplayForegroundGetter_ = {};
    relatedDisplayForegroundSetter_ = {};
    relatedDisplayBackgroundGetter_ = {};
    relatedDisplayBackgroundSetter_ = {};
    relatedDisplayLabelGetter_ = {};
    relatedDisplayLabelSetter_ = {};
    relatedDisplayVisualGetter_ = {};
    relatedDisplayVisualSetter_ = {};
    for (auto &getter : relatedDisplayEntryLabelGetters_) {
      getter = {};
    }
    for (auto &setter : relatedDisplayEntryLabelSetters_) {
      setter = {};
    }
    for (auto &getter : relatedDisplayEntryNameGetters_) {
      getter = {};
    }
    for (auto &setter : relatedDisplayEntryNameSetters_) {
      setter = {};
    }
    for (auto &getter : relatedDisplayEntryArgsGetters_) {
      getter = {};
    }
    for (auto &setter : relatedDisplayEntryArgsSetters_) {
      setter = {};
    }
    for (auto &getter : relatedDisplayEntryModeGetters_) {
      getter = {};
    }
    for (auto &setter : relatedDisplayEntryModeSetters_) {
      setter = {};
    }
    if (relatedDisplayForegroundButton_) {
      relatedDisplayForegroundButton_->setEnabled(false);
    }
    if (relatedDisplayBackgroundButton_) {
      relatedDisplayBackgroundButton_->setEnabled(false);
    }
    if (relatedDisplayLabelEdit_) {
      const QSignalBlocker blocker(relatedDisplayLabelEdit_);
      relatedDisplayLabelEdit_->clear();
      relatedDisplayLabelEdit_->setEnabled(false);
    }
    if (relatedDisplayVisualCombo_) {
      const QSignalBlocker blocker(relatedDisplayVisualCombo_);
      relatedDisplayVisualCombo_->setCurrentIndex(0);
      relatedDisplayVisualCombo_->setEnabled(false);
    }
    for (QLineEdit *edit : relatedDisplayEntryLabelEdits_) {
      if (edit) {
        const QSignalBlocker blocker(edit);
        edit->clear();
        edit->setEnabled(false);
      }
    }
    for (QLineEdit *edit : relatedDisplayEntryNameEdits_) {
      if (edit) {
        const QSignalBlocker blocker(edit);
        edit->clear();
        edit->setEnabled(false);
      }
    }
    for (QLineEdit *edit : relatedDisplayEntryArgsEdits_) {
      if (edit) {
        const QSignalBlocker blocker(edit);
        edit->clear();
        edit->setEnabled(false);
      }
    }
    for (QComboBox *combo : relatedDisplayEntryModeCombos_) {
      if (combo) {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(0);
        combo->setEnabled(false);
      }
    }
    if (textEntryPvLimitsButton_) {
      textEntryPvLimitsButton_->setEnabled(false);
    }
    meterForegroundGetter_ = {};
    meterForegroundSetter_ = {};
    meterBackgroundGetter_ = {};
    meterBackgroundSetter_ = {};
    meterLabelGetter_ = {};
    meterLabelSetter_ = {};
    meterColorModeGetter_ = {};
    meterColorModeSetter_ = {};
    meterChannelGetter_ = {};
    meterChannelSetter_ = {};
    meterLimitsGetter_ = {};
    meterLimitsSetter_ = {};
    if (meterPvLimitsButton_) {
      meterPvLimitsButton_->setEnabled(false);
    }
    barForegroundGetter_ = {};
    barForegroundSetter_ = {};
    barBackgroundGetter_ = {};
    barBackgroundSetter_ = {};
    barLabelGetter_ = {};
    barLabelSetter_ = {};
    barColorModeGetter_ = {};
    barColorModeSetter_ = {};
    barDirectionGetter_ = {};
    barDirectionSetter_ = {};
    barFillModeGetter_ = {};
    barFillModeSetter_ = {};
    barChannelGetter_ = {};
    barChannelSetter_ = {};
    barLimitsGetter_ = {};
    barLimitsSetter_ = {};
    if (barPvLimitsButton_) {
      barPvLimitsButton_->setEnabled(false);
    }
    scaleForegroundGetter_ = {};
    scaleForegroundSetter_ = {};
    scaleBackgroundGetter_ = {};
    scaleBackgroundSetter_ = {};
    scaleLabelGetter_ = {};
    scaleLabelSetter_ = {};
    scaleColorModeGetter_ = {};
    scaleColorModeSetter_ = {};
    scaleDirectionGetter_ = {};
    scaleDirectionSetter_ = {};
    scaleChannelGetter_ = {};
    scaleChannelSetter_ = {};
    scaleLimitsGetter_ = {};
    scaleLimitsSetter_ = {};
    if (scaleChannelEdit_) {
      scaleChannelEdit_->setEnabled(false);
    }
    if (scalePvLimitsButton_) {
      scalePvLimitsButton_->setEnabled(false);
    }
    stripTitleGetter_ = {};
    stripTitleSetter_ = {};
    stripXLabelGetter_ = {};
    stripXLabelSetter_ = {};
    stripYLabelGetter_ = {};
    stripYLabelSetter_ = {};
    stripForegroundGetter_ = {};
    stripForegroundSetter_ = {};
    stripBackgroundGetter_ = {};
    stripBackgroundSetter_ = {};
    stripPeriodGetter_ = {};
    stripPeriodSetter_ = {};
    stripUnitsGetter_ = {};
    stripUnitsSetter_ = {};
    for (auto &getter : stripPenChannelGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenChannelSetters_) {
      setter = {};
    }
    for (auto &getter : stripPenColorGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenColorSetters_) {
      setter = {};
    }
    for (auto &getter : stripPenLimitsGetters_) {
      getter = {};
    }
    for (auto &setter : stripPenLimitsSetters_) {
      setter = {};
    }
    for (QPushButton *button : stripPenLimitsButtons_) {
      if (button) {
        button->setEnabled(false);
      }
    }
    if (stripUnitsCombo_) {
      const QSignalBlocker blocker(stripUnitsCombo_);
      stripUnitsCombo_->setCurrentIndex(timeUnitsToIndex(TimeUnits::kSeconds));
    }
    cartesianTitleGetter_ = {};
    cartesianTitleSetter_ = {};
    cartesianXLabelGetter_ = {};
    cartesianXLabelSetter_ = {};
    for (auto &getter : cartesianYLabelGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianYLabelSetters_) {
      setter = {};
    }
    cartesianForegroundGetter_ = {};
    cartesianForegroundSetter_ = {};
    cartesianBackgroundGetter_ = {};
    cartesianBackgroundSetter_ = {};
    cartesianStyleGetter_ = {};
    cartesianStyleSetter_ = {};
    cartesianEraseOldestGetter_ = {};
    cartesianEraseOldestSetter_ = {};
    cartesianCountGetter_ = {};
    cartesianCountSetter_ = {};
    cartesianEraseModeGetter_ = {};
    cartesianEraseModeSetter_ = {};
    cartesianTriggerGetter_ = {};
    cartesianTriggerSetter_ = {};
    cartesianEraseGetter_ = {};
    cartesianEraseSetter_ = {};
    cartesianCountPvGetter_ = {};
    cartesianCountPvSetter_ = {};
    for (auto &getter : cartesianTraceXGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceXSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceYGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceYSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceColorGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceColorSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceAxisGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceAxisSetters_) {
      setter = {};
    }
    for (auto &getter : cartesianTraceSideGetters_) {
      getter = {};
    }
    for (auto &setter : cartesianTraceSideSetters_) {
      setter = {};
    }
    if (cartesianStyleCombo_) {
      const QSignalBlocker blocker(cartesianStyleCombo_);
      cartesianStyleCombo_->setCurrentIndex(cartesianPlotStyleToIndex(
          CartesianPlotStyle::kLine));
      cartesianStyleCombo_->setEnabled(false);
    }
    if (cartesianEraseOldestCombo_) {
      const QSignalBlocker blocker(cartesianEraseOldestCombo_);
      cartesianEraseOldestCombo_->setCurrentIndex(0);
      cartesianEraseOldestCombo_->setEnabled(false);
    }
    if (cartesianCountEdit_) {
      cartesianCountEdit_->setEnabled(false);
    }
    if (cartesianEraseModeCombo_) {
      const QSignalBlocker blocker(cartesianEraseModeCombo_);
      cartesianEraseModeCombo_->setCurrentIndex(
          cartesianEraseModeToIndex(CartesianPlotEraseMode::kIfNotZero));
      cartesianEraseModeCombo_->setEnabled(false);
    }
    if (cartesianTriggerEdit_) {
      cartesianTriggerEdit_->setEnabled(false);
    }
    if (cartesianEraseEdit_) {
      cartesianEraseEdit_->setEnabled(false);
    }
    if (cartesianCountPvEdit_) {
      cartesianCountPvEdit_->setEnabled(false);
    }
    for (auto *button : cartesianTraceColorButtons_) {
      if (button) {
        button->setEnabled(false);
      }
    }
    for (auto *edit : cartesianTraceXEdits_) {
      if (edit) {
        edit->setEnabled(false);
      }
    }
    for (auto *edit : cartesianTraceYEdits_) {
      if (edit) {
        edit->setEnabled(false);
      }
    }
    for (auto *combo : cartesianTraceAxisCombos_) {
      if (combo) {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(0);
        combo->setEnabled(false);
      }
    }
    for (auto *combo : cartesianTraceSideCombos_) {
      if (combo) {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(0);
        combo->setEnabled(false);
      }
    }
    byteForegroundGetter_ = {};
    byteForegroundSetter_ = {};
    byteBackgroundGetter_ = {};
    byteBackgroundSetter_ = {};
    byteColorModeGetter_ = {};
    byteColorModeSetter_ = {};
    byteDirectionGetter_ = {};
    byteDirectionSetter_ = {};
    byteStartBitGetter_ = {};
    byteStartBitSetter_ = {};
    byteEndBitGetter_ = {};
    byteEndBitSetter_ = {};
    byteChannelGetter_ = {};
    byteChannelSetter_ = {};
    if (byteStartBitSpin_) {
      byteStartBitSpin_->setEnabled(false);
    }
    if (byteEndBitSpin_) {
      byteEndBitSpin_->setEnabled(false);
    }
    if (byteChannelEdit_) {
      byteChannelEdit_->setEnabled(false);
    }
    rectangleForegroundGetter_ = {};
    rectangleForegroundSetter_ = {};
    rectangleFillGetter_ = {};
    rectangleFillSetter_ = {};
    rectangleLineStyleGetter_ = {};
    rectangleLineStyleSetter_ = {};
    rectangleLineWidthGetter_ = {};
    rectangleLineWidthSetter_ = {};
    rectangleColorModeGetter_ = {};
    rectangleColorModeSetter_ = {};
    rectangleVisibilityModeGetter_ = {};
    rectangleVisibilityModeSetter_ = {};
    rectangleVisibilityCalcGetter_ = {};
    rectangleVisibilityCalcSetter_ = {};
    for (auto &getter : rectangleChannelGetters_) {
      getter = {};
    }
    for (auto &setter : rectangleChannelSetters_) {
      setter = {};
    }
    lineColorGetter_ = {};
    lineColorSetter_ = {};
    lineLineStyleGetter_ = {};
    lineLineStyleSetter_ = {};
    lineLineWidthGetter_ = {};
    lineLineWidthSetter_ = {};
    lineColorModeGetter_ = {};
    lineColorModeSetter_ = {};
    lineVisibilityModeGetter_ = {};
    lineVisibilityModeSetter_ = {};
    lineVisibilityCalcGetter_ = {};
    lineVisibilityCalcSetter_ = {};
    for (auto &getter : lineChannelGetters_) {
      getter = {};
    }
    for (auto &setter : lineChannelSetters_) {
      setter = {};
    }
    activeColorButton_ = nullptr;
    lastCommittedGeometry_ = QRect();
    committedTextString_.clear();
    selectionKind_ = SelectionKind::kNone;

    if (colorPaletteDialog_) {
      colorPaletteDialog_->hide();
    }
    if (pvLimitsDialog_) {
      pvLimitsDialog_->clearTargets();
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
    updateTextChannelDependentControls();
    resetLineEdit(textEntryChannelEdit_);
    resetLineEdit(choiceButtonChannelEdit_);
    resetLineEdit(menuChannelEdit_);
    resetLineEdit(messageButtonLabelEdit_);
    resetLineEdit(messageButtonPressEdit_);
    resetLineEdit(messageButtonReleaseEdit_);
    resetLineEdit(messageButtonChannelEdit_);
    resetLineEdit(shellCommandLabelEdit_);
    for (QLineEdit *edit : shellCommandEntryLabelEdits_) {
      resetLineEdit(edit);
    }
    for (QLineEdit *edit : shellCommandEntryCommandEdits_) {
      resetLineEdit(edit);
    }
    for (QLineEdit *edit : shellCommandEntryArgsEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(textMonitorChannelEdit_);
    if (textMonitorPvLimitsButton_) {
      textMonitorPvLimitsButton_->setEnabled(false);
    }
    resetLineEdit(meterChannelEdit_);
    resetLineEdit(stripTitleEdit_);
    resetLineEdit(stripXLabelEdit_);
    resetLineEdit(stripYLabelEdit_);
    resetLineEdit(stripPeriodEdit_);
    for (QLineEdit *edit : stripPenChannelEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(cartesianTitleEdit_);
    resetLineEdit(cartesianXLabelEdit_);
    for (QLineEdit *edit : cartesianYLabelEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(cartesianCountEdit_);
    resetLineEdit(cartesianTriggerEdit_);
    resetLineEdit(cartesianEraseEdit_);
    resetLineEdit(cartesianCountPvEdit_);
    for (QLineEdit *edit : cartesianTraceXEdits_) {
      resetLineEdit(edit);
    }
    for (QLineEdit *edit : cartesianTraceYEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(barChannelEdit_);
    resetLineEdit(scaleChannelEdit_);
    resetLineEdit(rectangleLineWidthEdit_);
    resetLineEdit(rectangleVisibilityCalcEdit_);
    for (QLineEdit *edit : rectangleChannelEdits_) {
      resetLineEdit(edit);
    }
    updateRectangleChannelDependentControls();
    resetLineEdit(compositeFileEdit_);
    resetLineEdit(compositeVisibilityCalcEdit_);
    for (QLineEdit *edit : compositeChannelEdits_) {
      resetLineEdit(edit);
    }
    updateCompositeChannelDependentControls();
    if (compositeFileEdit_) {
      compositeFileEdit_->setEnabled(false);
    }
    if (compositeVisibilityCalcEdit_) {
      compositeVisibilityCalcEdit_->setEnabled(false);
    }
    for (QLineEdit *edit : compositeChannelEdits_) {
      if (edit) {
        edit->setEnabled(false);
      }
    }
    resetLineEdit(imageNameEdit_);
    resetLineEdit(imageCalcEdit_);
    resetLineEdit(imageVisibilityCalcEdit_);
    for (QLineEdit *edit : imageChannelEdits_) {
      resetLineEdit(edit);
    }
    updateImageChannelDependentControls();
    resetLineEdit(lineLineWidthEdit_);
    resetLineEdit(lineVisibilityCalcEdit_);
    for (QLineEdit *edit : lineChannelEdits_) {
      resetLineEdit(edit);
    }
    updateLineChannelDependentControls();

    resetColorButton(foregroundButton_);
    resetColorButton(backgroundButton_);
    resetColorButton(textForegroundButton_);
    resetColorButton(textMonitorForegroundButton_);
    resetColorButton(textMonitorBackgroundButton_);
    resetColorButton(choiceButtonForegroundButton_);
    resetColorButton(choiceButtonBackgroundButton_);
    resetColorButton(menuForegroundButton_);
    resetColorButton(menuBackgroundButton_);
    resetColorButton(messageButtonForegroundButton_);
    resetColorButton(messageButtonBackgroundButton_);
    resetColorButton(shellCommandForegroundButton_);
    resetColorButton(shellCommandBackgroundButton_);
    resetColorButton(meterForegroundButton_);
    resetColorButton(meterBackgroundButton_);
    resetColorButton(barForegroundButton_);
    resetColorButton(barBackgroundButton_);
    resetColorButton(scaleForegroundButton_);
    resetColorButton(scaleBackgroundButton_);
    resetColorButton(stripForegroundButton_);
    resetColorButton(stripBackgroundButton_);
    for (QPushButton *button : stripPenColorButtons_) {
      resetColorButton(button);
    }
    resetColorButton(cartesianForegroundButton_);
    resetColorButton(cartesianBackgroundButton_);
    for (QPushButton *button : cartesianTraceColorButtons_) {
      resetColorButton(button);
    }
    resetColorButton(rectangleForegroundButton_);
    resetColorButton(lineColorButton_);
    resetColorButton(compositeForegroundButton_);
    resetColorButton(compositeBackgroundButton_);
    if (compositeForegroundButton_) {
      compositeForegroundButton_->setEnabled(false);
    }
    if (compositeBackgroundButton_) {
      compositeBackgroundButton_->setEnabled(false);
    }

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
    if (textMonitorAlignmentCombo_) {
      const QSignalBlocker blocker(textMonitorAlignmentCombo_);
      textMonitorAlignmentCombo_->setCurrentIndex(
          alignmentToIndex(Qt::AlignLeft | Qt::AlignVCenter));
    }
    if (textMonitorFormatCombo_) {
      const QSignalBlocker blocker(textMonitorFormatCombo_);
      textMonitorFormatCombo_->setCurrentIndex(
          textMonitorFormatToIndex(TextMonitorFormat::kDecimal));
    }
    if (textColorModeCombo_) {
      const QSignalBlocker blocker(textColorModeCombo_);
      textColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (textMonitorColorModeCombo_) {
      const QSignalBlocker blocker(textMonitorColorModeCombo_);
      textMonitorColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (choiceButtonColorModeCombo_) {
      const QSignalBlocker blocker(choiceButtonColorModeCombo_);
      choiceButtonColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (choiceButtonStackingCombo_) {
      const QSignalBlocker blocker(choiceButtonStackingCombo_);
      choiceButtonStackingCombo_->setCurrentIndex(
          choiceButtonStackingToIndex(ChoiceButtonStacking::kRow));
    }
    if (meterLabelCombo_) {
      const QSignalBlocker blocker(meterLabelCombo_);
      meterLabelCombo_->setCurrentIndex(meterLabelToIndex(MeterLabel::kOutline));
    }
    if (meterColorModeCombo_) {
      const QSignalBlocker blocker(meterColorModeCombo_);
      meterColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (barLabelCombo_) {
      const QSignalBlocker blocker(barLabelCombo_);
      barLabelCombo_->setCurrentIndex(meterLabelToIndex(MeterLabel::kOutline));
    }
    if (barColorModeCombo_) {
      const QSignalBlocker blocker(barColorModeCombo_);
      barColorModeCombo_->setCurrentIndex(colorModeToIndex(TextColorMode::kStatic));
    }
    if (scaleLabelCombo_) {
      const QSignalBlocker blocker(scaleLabelCombo_);
      scaleLabelCombo_->setCurrentIndex(meterLabelToIndex(MeterLabel::kOutline));
    }
    if (scaleColorModeCombo_) {
      const QSignalBlocker blocker(scaleColorModeCombo_);
      scaleColorModeCombo_->setCurrentIndex(colorModeToIndex(TextColorMode::kStatic));
    }
    if (scaleDirectionCombo_) {
      const QSignalBlocker blocker(scaleDirectionCombo_);
      scaleDirectionCombo_->setCurrentIndex(scaleDirectionToIndex(BarDirection::kRight));
    }
    if (barDirectionCombo_) {
      const QSignalBlocker blocker(barDirectionCombo_);
      barDirectionCombo_->setCurrentIndex(barDirectionToIndex(BarDirection::kRight));
    }
    if (barFillCombo_) {
      const QSignalBlocker blocker(barFillCombo_);
      barFillCombo_->setCurrentIndex(barFillToIndex(BarFill::kFromEdge));
    }
    if (textVisibilityCombo_) {
      const QSignalBlocker blocker(textVisibilityCombo_);
      textVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
    }
    if (rectangleFillCombo_) {
      const QSignalBlocker blocker(rectangleFillCombo_);
      rectangleFillCombo_->setCurrentIndex(fillToIndex(RectangleFill::kOutline));
    }
    if (rectangleLineStyleCombo_) {
      const QSignalBlocker blocker(rectangleLineStyleCombo_);
      rectangleLineStyleCombo_->setCurrentIndex(lineStyleToIndex(RectangleLineStyle::kSolid));
    }
    if (rectangleColorModeCombo_) {
      const QSignalBlocker blocker(rectangleColorModeCombo_);
      rectangleColorModeCombo_->setCurrentIndex(colorModeToIndex(TextColorMode::kStatic));
    }
    if (rectangleVisibilityCombo_) {
      const QSignalBlocker blocker(rectangleVisibilityCombo_);
      rectangleVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
    }
    if (compositeVisibilityCombo_) {
      const QSignalBlocker blocker(compositeVisibilityCombo_);
      compositeVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
      compositeVisibilityCombo_->setEnabled(false);
    }
    if (compositeVisibilityCalcEdit_) {
      compositeVisibilityCalcEdit_->setEnabled(false);
    }
    if (imageTypeCombo_) {
      const QSignalBlocker blocker(imageTypeCombo_);
      imageTypeCombo_->setCurrentIndex(imageTypeToIndex(ImageType::kNone));
    }
    if (imageColorModeCombo_) {
      const QSignalBlocker blocker(imageColorModeCombo_);
      imageColorModeCombo_->setCurrentIndex(colorModeToIndex(TextColorMode::kStatic));
    }
    if (imageVisibilityCombo_) {
      const QSignalBlocker blocker(imageVisibilityCombo_);
      imageVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
    }
    if (lineLineStyleCombo_) {
      const QSignalBlocker blocker(lineLineStyleCombo_);
      lineLineStyleCombo_->setCurrentIndex(lineStyleToIndex(RectangleLineStyle::kSolid));
    }
    if (lineColorModeCombo_) {
      const QSignalBlocker blocker(lineColorModeCombo_);
      lineColorModeCombo_->setCurrentIndex(colorModeToIndex(TextColorMode::kStatic));
    }
    if (lineVisibilityCombo_) {
      const QSignalBlocker blocker(lineVisibilityCombo_);
      lineVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
    }

    if (heatmapXSourceCombo_) {
      const QSignalBlocker blocker(heatmapXSourceCombo_);
      heatmapXSourceCombo_->setCurrentIndex(
          heatmapDimensionSourceToIndex(HeatmapDimensionSource::kStatic));
    }
    if (heatmapYSourceCombo_) {
      const QSignalBlocker blocker(heatmapYSourceCombo_);
      heatmapYSourceCombo_->setCurrentIndex(
          heatmapDimensionSourceToIndex(HeatmapDimensionSource::kStatic));
    }
    if (heatmapOrderCombo_) {
      const QSignalBlocker blocker(heatmapOrderCombo_);
      heatmapOrderCombo_->setCurrentIndex(
          heatmapOrderToIndex(HeatmapOrder::kRowMajor));
    }

    resetLineEdit(heatmapTitleEdit_);
    resetLineEdit(heatmapDataChannelEdit_);
    resetLineEdit(heatmapXDimEdit_);
    resetLineEdit(heatmapYDimEdit_);
    resetLineEdit(heatmapXDimChannelEdit_);
    resetLineEdit(heatmapYDimChannelEdit_);

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Select..."));
    }

    imageTypeGetter_ = {};
    imageTypeSetter_ = {};
    imageNameGetter_ = {};
    imageNameSetter_ = {};
    imageCalcGetter_ = {};
    imageCalcSetter_ = {};
    imageColorModeGetter_ = {};
    imageColorModeSetter_ = {};
    imageVisibilityModeGetter_ = {};
    imageVisibilityModeSetter_ = {};
    imageVisibilityCalcGetter_ = {};
    imageVisibilityCalcSetter_ = {};
    for (auto &getter : imageChannelGetters_) {
      getter = {};
    }
    for (auto &setter : imageChannelSetters_) {
      setter = {};
    }

    heatmapTitleGetter_ = {};
    heatmapTitleSetter_ = {};
    heatmapDataChannelGetter_ = {};
    heatmapDataChannelSetter_ = {};
    heatmapXSourceGetter_ = {};
    heatmapXSourceSetter_ = {};
    heatmapYSourceGetter_ = {};
    heatmapYSourceSetter_ = {};
    heatmapXDimensionGetter_ = {};
    heatmapXDimensionSetter_ = {};
    heatmapYDimensionGetter_ = {};
    heatmapYDimensionSetter_ = {};
    heatmapXDimChannelGetter_ = {};
    heatmapXDimChannelSetter_ = {};
    heatmapYDimChannelGetter_ = {};
    heatmapYDimChannelSetter_ = {};
    heatmapOrderGetter_ = {};
    heatmapOrderSetter_ = {};

    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};

    committedTexts_.clear();
    updateCommittedTexts();
    updateCartesianAxisButtonState();
    if (cartesianAxisDialog_) {
      cartesianAxisDialog_->clearCallbacks();
    }
    updateSectionVisibility(selectionKind_);
  }

  void openCartesianAxisDialogForElement(CartesianPlotElement *element)
  {
    if (!element) {
      return;
    }
    CartesianAxisDialog *dialog = ensureCartesianAxisDialog();
    if (!dialog) {
      return;
    }
    
    // Set up getters and setters for this specific element
    std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> styleGetters;
    std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> styleSetters;
    std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> rangeGetters;
    std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> rangeSetters;
    std::array<std::function<double()>, kCartesianAxisCount> minimumGetters;
    std::array<std::function<void(double)>, kCartesianAxisCount> minimumSetters;
    std::array<std::function<double()>, kCartesianAxisCount> maximumGetters;
    std::array<std::function<void(double)>, kCartesianAxisCount> maximumSetters;
    std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> timeFormatGetters;
    std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> timeFormatSetters;
    
    for (int i = 0; i < kCartesianAxisCount; ++i) {
      styleGetters[i] = [element, i]() { return element->axisStyle(i); };
      styleSetters[i] = [element, i](CartesianPlotAxisStyle style) {
        element->setAxisStyle(i, style);
        element->update();
      };
      rangeGetters[i] = [element, i]() { return element->axisRangeStyle(i); };
      rangeSetters[i] = [element, i](CartesianPlotRangeStyle style) {
        element->setAxisRangeStyle(i, style);
        element->update();
      };
      minimumGetters[i] = [element, i]() { return element->axisMinimum(i); };
      minimumSetters[i] = [element, i](double value) {
        element->setAxisMinimum(i, value);
        element->update();
      };
      maximumGetters[i] = [element, i]() { return element->axisMaximum(i); };
      maximumSetters[i] = [element, i](double value) {
        element->setAxisMaximum(i, value);
        element->update();
      };
      timeFormatGetters[i] = [element, i]() { return element->axisTimeFormat(i); };
      timeFormatSetters[i] = [element, i](CartesianPlotTimeFormat format) {
        element->setAxisTimeFormat(i, format);
        element->update();
      };
    }
    
    dialog->setCartesianCallbacks(styleGetters, styleSetters, rangeGetters,
        rangeSetters, minimumGetters, minimumSetters, maximumGetters,
        maximumSetters, timeFormatGetters, timeFormatSetters,
        [element]() { element->update(); });
    positionCartesianAxisDialog(dialog);
    dialog->showDialog();
  }

protected:
  void closeEvent(QCloseEvent *event) override
  {
    clearSelectionState();
    QDialog::closeEvent(event);
  }

private:
  static constexpr int kPaletteSpacing = 12;
  static constexpr int kTextVisibleChannelCount = 4;
  static constexpr int kTextChannelAIndex = 0;
  static constexpr int kTextChannelBIndex = 1;
  static constexpr int kTextChannelCIndex = 2;
  static constexpr int kTextChannelDIndex = 3;
  enum class SelectionKind {
    kNone,
    kDisplay,
    kRectangle,
    kImage,
    kHeatmap,
    kPolygon,
    kComposite,
    kLine,
    kText,
    kTextEntry,
    kSlider,
    kWheelSwitch,
    kChoiceButton,
    kMenu,
    kMessageButton,
    kShellCommand,
    kRelatedDisplay,
    kTextMonitor,
    kMeter,
    kBarMonitor,
    kScaleMonitor,
    kStripChart,
    kCartesianPlot,
    kByteMonitor
  };
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

  QPushButton *createActionButton(const QString &text)
  {
    auto *button = new QPushButton(text);
    button->setFont(valueFont_);
    button->setAutoDefault(false);
    button->setDefault(false);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    button->setFocusPolicy(Qt::NoFocus);
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
    fieldLabels_.insert(field, labelWidget);
  }

  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (event->type() == QEvent::FocusOut) {
      if (auto *edit = qobject_cast<QLineEdit *>(object)) {
        const bool isRectangleChannelEdit = std::find(
            rectangleChannelEdits_.begin(), rectangleChannelEdits_.end(), edit)
            != rectangleChannelEdits_.end();
        const bool isLineChannelEdit = std::find(
            lineChannelEdits_.begin(), lineChannelEdits_.end(), edit)
            != lineChannelEdits_.end();
        const bool isImageChannelEdit = std::find(
            imageChannelEdits_.begin(), imageChannelEdits_.end(), edit)
            != imageChannelEdits_.end();
    const bool isCompositeChannelEdit = std::find(
      compositeChannelEdits_.begin(), compositeChannelEdits_.end(), edit)
      != compositeChannelEdits_.end();
        const bool isRelatedLabelEdit = std::find(
            relatedDisplayEntryLabelEdits_.begin(),
            relatedDisplayEntryLabelEdits_.end(), edit)
            != relatedDisplayEntryLabelEdits_.end();
        const bool isRelatedNameEdit = std::find(
            relatedDisplayEntryNameEdits_.begin(),
            relatedDisplayEntryNameEdits_.end(), edit)
            != relatedDisplayEntryNameEdits_.end();
        const bool isRelatedArgsEdit = std::find(
            relatedDisplayEntryArgsEdits_.begin(),
            relatedDisplayEntryArgsEdits_.end(), edit)
            != relatedDisplayEntryArgsEdits_.end();
        if (edit == xEdit_ || edit == yEdit_ || edit == widthEdit_
            || edit == heightEdit_ || edit == gridSpacingEdit_
            || edit == rectangleLineWidthEdit_
            || edit == rectangleVisibilityCalcEdit_
            || edit == compositeFileEdit_
            || edit == compositeVisibilityCalcEdit_
            || edit == imageNameEdit_ || edit == imageCalcEdit_
            || edit == imageVisibilityCalcEdit_
            || edit == lineLineWidthEdit_
            || edit == lineVisibilityCalcEdit_
            || isRectangleChannelEdit || isLineChannelEdit
            || isImageChannelEdit || isCompositeChannelEdit
            || isRelatedLabelEdit
            || isRelatedNameEdit || isRelatedArgsEdit
            || edit == relatedDisplayLabelEdit_) {
          revertLineEdit(edit);
        }
        if (edit == textMonitorChannelEdit_
            || edit == meterChannelEdit_ || edit == sliderIncrementEdit_
            || edit == sliderChannelEdit_) {
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

    /* Set stylesheet to prevent gradient rendering */
    QString colorName = color.name(QColor::HexRgb);
    QString textColor = color.lightness() < 128
        ? QStringLiteral("#ffffff")
        : QStringLiteral("#000000");
    QString stylesheet = QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid #808080; }")
        .arg(colorName, textColor);
    button->setStyleSheet(stylesheet);
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
    button->setStyleSheet(QString());
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
    if (rectangleSection_) {
      const bool rectangleVisible = kind == SelectionKind::kRectangle
          || kind == SelectionKind::kPolygon;
      rectangleSection_->setVisible(rectangleVisible);
      rectangleSection_->setEnabled(rectangleVisible);
    }
    if (compositeSection_) {
      const bool compositeVisible = kind == SelectionKind::kComposite;
      compositeSection_->setVisible(compositeVisible);
      compositeSection_->setEnabled(compositeVisible);
    }
    if (imageSection_) {
      const bool imageVisible = kind == SelectionKind::kImage;
      imageSection_->setVisible(imageVisible);
      imageSection_->setEnabled(imageVisible);
    }
    if (heatmapSection_) {
      const bool heatmapVisible = kind == SelectionKind::kHeatmap;
      heatmapSection_->setVisible(heatmapVisible);
      heatmapSection_->setEnabled(heatmapVisible);
    }
    const bool showArcControls = (kind == SelectionKind::kRectangle
        || kind == SelectionKind::kPolygon) && rectangleIsArc_;
    if (arcBeginLabel_) {
      arcBeginLabel_->setVisible(showArcControls);
    }
    if (arcBeginSpin_) {
      arcBeginSpin_->setVisible(showArcControls);
      arcBeginSpin_->setEnabled(showArcControls
          && static_cast<bool>(arcBeginSetter_));
    }
    if (arcPathLabel_) {
      arcPathLabel_->setVisible(showArcControls);
    }
    if (arcPathSpin_) {
      arcPathSpin_->setVisible(showArcControls);
      arcPathSpin_->setEnabled(showArcControls
          && static_cast<bool>(arcPathSetter_));
    }
    if (lineSection_) {
      const bool lineVisible = kind == SelectionKind::kLine;
      lineSection_->setVisible(lineVisible);
      lineSection_->setEnabled(lineVisible);
    }
    if (textSection_) {
      const bool textVisible = kind == SelectionKind::kText;
      textSection_->setVisible(textVisible);
      textSection_->setEnabled(textVisible);
    }
    if (textStringEdit_) {
      textStringEdit_->setEnabled(kind == SelectionKind::kText);
    }
    if (textEntrySection_) {
      const bool textEntryVisible = kind == SelectionKind::kTextEntry;
      textEntrySection_->setVisible(textEntryVisible);
      textEntrySection_->setEnabled(textEntryVisible);
    }
    if (sliderSection_) {
      const bool sliderVisible = kind == SelectionKind::kSlider;
      sliderSection_->setVisible(sliderVisible);
      sliderSection_->setEnabled(sliderVisible);
    }
    if (wheelSwitchSection_) {
      const bool wheelVisible = kind == SelectionKind::kWheelSwitch;
      wheelSwitchSection_->setVisible(wheelVisible);
      wheelSwitchSection_->setEnabled(wheelVisible);
    }
    // Hide precision row for wheel switch - it should use PV Limits precision
    if (wheelSwitchPrecisionEdit_) {
      const bool showPrecision = false;  // Never show for wheel switch
      wheelSwitchPrecisionEdit_->setVisible(showPrecision);
      if (fieldLabels_.contains(wheelSwitchPrecisionEdit_)) {
        fieldLabels_[wheelSwitchPrecisionEdit_]->setVisible(showPrecision);
      }
    }
    if (choiceButtonSection_) {
      const bool choiceVisible = kind == SelectionKind::kChoiceButton;
      choiceButtonSection_->setVisible(choiceVisible);
      choiceButtonSection_->setEnabled(choiceVisible);
    }
    if (menuSection_) {
      const bool menuVisible = kind == SelectionKind::kMenu;
      menuSection_->setVisible(menuVisible);
      menuSection_->setEnabled(menuVisible);
    }
    if (messageButtonSection_) {
      const bool messageVisible = kind == SelectionKind::kMessageButton;
      messageButtonSection_->setVisible(messageVisible);
      messageButtonSection_->setEnabled(messageVisible);
    }
    if (shellCommandSection_) {
      const bool shellVisible = kind == SelectionKind::kShellCommand;
      shellCommandSection_->setVisible(shellVisible);
      shellCommandSection_->setEnabled(shellVisible);
    }
    if (relatedDisplaySection_) {
      const bool relatedVisible = kind == SelectionKind::kRelatedDisplay;
      relatedDisplaySection_->setVisible(relatedVisible);
      relatedDisplaySection_->setEnabled(relatedVisible);
    }
    if (textMonitorSection_) {
      const bool monitorVisible = kind == SelectionKind::kTextMonitor;
      textMonitorSection_->setVisible(monitorVisible);
      textMonitorSection_->setEnabled(monitorVisible);
    }
    if (meterSection_) {
      const bool meterVisible = kind == SelectionKind::kMeter;
      meterSection_->setVisible(meterVisible);
      meterSection_->setEnabled(meterVisible);
    }
    if (barSection_) {
      const bool barVisible = kind == SelectionKind::kBarMonitor;
      barSection_->setVisible(barVisible);
      barSection_->setEnabled(barVisible);
    }
    if (scaleSection_) {
      const bool scaleVisible = kind == SelectionKind::kScaleMonitor;
      scaleSection_->setVisible(scaleVisible);
      scaleSection_->setEnabled(scaleVisible);
    }
    if (stripChartSection_) {
      const bool stripVisible = kind == SelectionKind::kStripChart;
      stripChartSection_->setVisible(stripVisible);
      stripChartSection_->setEnabled(stripVisible);
    }
    if (cartesianSection_) {
      const bool cartesianVisible = kind == SelectionKind::kCartesianPlot;
      cartesianSection_->setVisible(cartesianVisible);
      cartesianSection_->setEnabled(cartesianVisible);
    }
    if (byteSection_) {
      const bool byteVisible = kind == SelectionKind::kByteMonitor;
      byteSection_->setVisible(byteVisible);
      byteSection_->setEnabled(byteVisible);
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
    if (index == kTextChannelAIndex) {
      updateTextChannelDependentControls();
    }
  }

  void updateImageChannelDependentControls()
  {
    bool hasChannelA = false;
    if (imageChannelGetters_[0]) {
      const QString value = imageChannelGetters_[0]();
      hasChannelA = !value.trimmed().isEmpty();
    }
    if (!hasChannelA) {
      QLineEdit *channelEdit = imageChannelEdits_[0];
      if (channelEdit) {
        hasChannelA = !channelEdit->text().trimmed().isEmpty();
      }
    }
    setFieldEnabled(imageColorModeCombo_, hasChannelA);
    setFieldEnabled(imageVisibilityCombo_, hasChannelA);
    setFieldEnabled(imageVisibilityCalcEdit_, hasChannelA);
  }

  void updateHeatmapDimensionControls()
  {
    const bool xFromChannel = heatmapXSourceCombo_
        && heatmapXSourceCombo_->currentIndex() == 1;
    const bool yFromChannel = heatmapYSourceCombo_
        && heatmapYSourceCombo_->currentIndex() == 1;
    setFieldEnabled(heatmapXDimEdit_, !xFromChannel);
    setFieldEnabled(heatmapXDimChannelEdit_, xFromChannel);
    setFieldEnabled(heatmapYDimEdit_, !yFromChannel);
    setFieldEnabled(heatmapYDimChannelEdit_, yFromChannel);
  }

  void updateLineChannelDependentControls()
  {
    bool hasChannelA = false;
    if (lineChannelGetters_[0]) {
      const QString value = lineChannelGetters_[0]();
      hasChannelA = !value.trimmed().isEmpty();
    }
    if (!hasChannelA) {
      QLineEdit *channelEdit = lineChannelEdits_[0];
      if (channelEdit) {
        hasChannelA = !channelEdit->text().trimmed().isEmpty();
      }
    }
    setFieldEnabled(lineColorModeCombo_, hasChannelA);
    setFieldEnabled(lineVisibilityCombo_, hasChannelA);
    setFieldEnabled(lineVisibilityCalcEdit_, hasChannelA);
  }

  void updateTextChannelDependentControls()
  {
    bool hasChannelA = false;
    if (textChannelGetters_[kTextChannelAIndex]) {
      const QString value = textChannelGetters_[kTextChannelAIndex]();
      hasChannelA = !value.trimmed().isEmpty();
    }
    if (!hasChannelA) {
      QLineEdit *channelEdit = textChannelEdits_[kTextChannelAIndex];
      if (channelEdit) {
        hasChannelA = !channelEdit->text().trimmed().isEmpty();
      }
    }

    setFieldEnabled(textColorModeCombo_, hasChannelA);
    setFieldEnabled(textVisibilityCombo_, hasChannelA);
    setFieldEnabled(textVisibilityCalcEdit_, hasChannelA);
  }

  void updateRectangleChannelDependentControls()
  {
    bool hasChannelA = false;
    if (rectangleChannelGetters_[0]) {
      const QString value = rectangleChannelGetters_[0]();
      hasChannelA = !value.trimmed().isEmpty();
    }
    if (!hasChannelA) {
      QLineEdit *channelEdit = rectangleChannelEdits_[0];
      if (channelEdit) {
        hasChannelA = !channelEdit->text().trimmed().isEmpty();
      }
    }
    setFieldEnabled(rectangleColorModeCombo_, hasChannelA);
    setFieldEnabled(rectangleVisibilityCombo_, hasChannelA);
    setFieldEnabled(rectangleVisibilityCalcEdit_, hasChannelA);
  }

  void updateCompositeChannelDependentControls()
  {
    bool hasChannelA = false;
    if (compositeChannelGetters_[0]) {
      const QString value = compositeChannelGetters_[0]();
      hasChannelA = !value.trimmed().isEmpty();
    }
    if (!hasChannelA) {
      QLineEdit *channelEdit = compositeChannelEdits_[0];
      if (channelEdit) {
        hasChannelA = !channelEdit->text().trimmed().isEmpty();
      }
    }
    if (compositeVisibilityCombo_) {
      compositeVisibilityCombo_->setEnabled(hasChannelA
          && static_cast<bool>(compositeVisibilityModeSetter_));
    }
    if (compositeVisibilityCalcEdit_) {
      bool enableCalc = hasChannelA
          && static_cast<bool>(compositeVisibilityCalcSetter_);
      if (compositeVisibilityCombo_) {
        enableCalc = enableCalc
            && visibilityModeFromIndex(compositeVisibilityCombo_->currentIndex())
                == TextVisibilityMode::kCalc;
      }
      compositeVisibilityCalcEdit_->setEnabled(enableCalc);
    }
  }

  void setFieldEnabled(QWidget *field, bool enabled)
  {
    if (!field) {
      return;
    }
    field->setEnabled(enabled);
    if (QLabel *label = fieldLabels_.value(field, nullptr)) {
      label->setEnabled(enabled);
    }
  }

  void commitTextEntryChannel()
  {
    if (!textEntryChannelEdit_) {
      return;
    }
    if (!textEntryChannelSetter_) {
      revertLineEdit(textEntryChannelEdit_);
      return;
    }
    const QString value = textEntryChannelEdit_->text();
    textEntryChannelSetter_(value);
    committedTexts_[textEntryChannelEdit_] = value;
  }

  void commitSliderIncrement()
  {
    if (!sliderIncrementEdit_) {
      return;
    }
    if (!sliderIncrementSetter_) {
      revertLineEdit(sliderIncrementEdit_);
      return;
    }
    bool ok = false;
    const double value = sliderIncrementEdit_->text().toDouble(&ok);
    if (!ok) {
      revertLineEdit(sliderIncrementEdit_);
      return;
    }
    sliderIncrementSetter_(value);
    committedTexts_[sliderIncrementEdit_] = sliderIncrementEdit_->text();
    updateSliderLimitsFromDialog();
  }

  void commitSliderChannel()
  {
    if (!sliderChannelEdit_) {
      return;
    }
    if (!sliderChannelSetter_) {
      revertLineEdit(sliderChannelEdit_);
      return;
    }
    const QString value = sliderChannelEdit_->text();
    sliderChannelSetter_(value);
    committedTexts_[sliderChannelEdit_] = value;
    updateSliderLimitsFromDialog();
  }

  void commitWheelSwitchPrecision()
  {
    if (!wheelSwitchPrecisionEdit_) {
      return;
    }
    if (!wheelSwitchPrecisionSetter_) {
      revertLineEdit(wheelSwitchPrecisionEdit_);
      return;
    }
    bool ok = false;
    const double value = wheelSwitchPrecisionEdit_->text().toDouble(&ok);
    if (!ok) {
      revertLineEdit(wheelSwitchPrecisionEdit_);
      return;
    }
    wheelSwitchPrecisionSetter_(value);
    committedTexts_[wheelSwitchPrecisionEdit_] = wheelSwitchPrecisionEdit_->text();
    updateWheelSwitchLimitsFromDialog();
  }

  void commitWheelSwitchFormat()
  {
    if (!wheelSwitchFormatEdit_) {
      return;
    }
    if (!wheelSwitchFormatSetter_) {
      revertLineEdit(wheelSwitchFormatEdit_);
      return;
    }
    const QString value = wheelSwitchFormatEdit_->text();
    wheelSwitchFormatSetter_(value);
    committedTexts_[wheelSwitchFormatEdit_] = value;
  }

  void commitWheelSwitchChannel()
  {
    if (!wheelSwitchChannelEdit_) {
      return;
    }
    if (!wheelSwitchChannelSetter_) {
      revertLineEdit(wheelSwitchChannelEdit_);
      return;
    }
    const QString value = wheelSwitchChannelEdit_->text();
    wheelSwitchChannelSetter_(value);
    committedTexts_[wheelSwitchChannelEdit_] = value;
    updateWheelSwitchLimitsFromDialog();
  }

  void commitChoiceButtonChannel()
  {
    if (!choiceButtonChannelEdit_) {
      return;
    }
    if (!choiceButtonChannelSetter_) {
      revertLineEdit(choiceButtonChannelEdit_);
      return;
    }
    const QString value = choiceButtonChannelEdit_->text();
    choiceButtonChannelSetter_(value);
    committedTexts_[choiceButtonChannelEdit_] = value;
  }

  void commitMenuChannel()
  {
    if (!menuChannelEdit_) {
      return;
    }
    if (!menuChannelSetter_) {
      revertLineEdit(menuChannelEdit_);
      return;
    }
    const QString value = menuChannelEdit_->text();
    menuChannelSetter_(value);
    committedTexts_[menuChannelEdit_] = value;
  }

  void commitMessageButtonLabel()
  {
    if (!messageButtonLabelEdit_) {
      return;
    }
    if (!messageButtonLabelSetter_) {
      revertLineEdit(messageButtonLabelEdit_);
      return;
    }
    const QString value = messageButtonLabelEdit_->text();
    messageButtonLabelSetter_(value);
    committedTexts_[messageButtonLabelEdit_] = value;
  }

  void commitMessageButtonPressMessage()
  {
    if (!messageButtonPressEdit_) {
      return;
    }
    if (!messageButtonPressSetter_) {
      revertLineEdit(messageButtonPressEdit_);
      return;
    }
    const QString value = messageButtonPressEdit_->text();
    messageButtonPressSetter_(value);
    committedTexts_[messageButtonPressEdit_] = value;
  }

  void commitMessageButtonReleaseMessage()
  {
    if (!messageButtonReleaseEdit_) {
      return;
    }
    if (!messageButtonReleaseSetter_) {
      revertLineEdit(messageButtonReleaseEdit_);
      return;
    }
    const QString value = messageButtonReleaseEdit_->text();
    messageButtonReleaseSetter_(value);
    committedTexts_[messageButtonReleaseEdit_] = value;
  }

  void commitMessageButtonChannel()
  {
    if (!messageButtonChannelEdit_) {
      return;
    }
    if (!messageButtonChannelSetter_) {
      revertLineEdit(messageButtonChannelEdit_);
      return;
    }
    const QString value = messageButtonChannelEdit_->text();
    messageButtonChannelSetter_(value);
    committedTexts_[messageButtonChannelEdit_] = value;
  }

  void commitShellCommandLabel()
  {
    if (!shellCommandLabelEdit_) {
      return;
    }
    if (!shellCommandLabelSetter_) {
      revertLineEdit(shellCommandLabelEdit_);
      return;
    }
    const QString value = shellCommandLabelEdit_->text();
    shellCommandLabelSetter_(value);
    committedTexts_[shellCommandLabelEdit_] = value;
  }

  void commitShellCommandEntryLabel(int index)
  {
    if (index < 0 || index >= kShellCommandEntryCount) {
      return;
    }
    QLineEdit *edit = shellCommandEntryLabelEdits_[index];
    if (!edit) {
      return;
    }
    if (!shellCommandEntryLabelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    shellCommandEntryLabelSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitShellCommandEntryCommand(int index)
  {
    if (index < 0 || index >= kShellCommandEntryCount) {
      return;
    }
    QLineEdit *edit = shellCommandEntryCommandEdits_[index];
    if (!edit) {
      return;
    }
    if (!shellCommandEntryCommandSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    shellCommandEntryCommandSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitShellCommandEntryArgs(int index)
  {
    if (index < 0 || index >= kShellCommandEntryCount) {
      return;
    }
    QLineEdit *edit = shellCommandEntryArgsEdits_[index];
    if (!edit) {
      return;
    }
    if (!shellCommandEntryArgsSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    shellCommandEntryArgsSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitRelatedDisplayLabel()
  {
    if (!relatedDisplayLabelEdit_) {
      return;
    }
    if (!relatedDisplayLabelSetter_) {
      revertLineEdit(relatedDisplayLabelEdit_);
      return;
    }
    const QString value = relatedDisplayLabelEdit_->text();
    relatedDisplayLabelSetter_(value);
    committedTexts_[relatedDisplayLabelEdit_] = value;
  }

  void commitRelatedDisplayEntryLabel(int index)
  {
    if (index < 0 || index >= kRelatedDisplayEntryCount) {
      return;
    }
    QLineEdit *edit = relatedDisplayEntryLabelEdits_[index];
    if (!edit) {
      return;
    }
    if (!relatedDisplayEntryLabelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    relatedDisplayEntryLabelSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitRelatedDisplayEntryName(int index)
  {
    if (index < 0 || index >= kRelatedDisplayEntryCount) {
      return;
    }
    QLineEdit *edit = relatedDisplayEntryNameEdits_[index];
    if (!edit) {
      return;
    }
    if (!relatedDisplayEntryNameSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    relatedDisplayEntryNameSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitRelatedDisplayEntryArgs(int index)
  {
    if (index < 0 || index >= kRelatedDisplayEntryCount) {
      return;
    }
    QLineEdit *edit = relatedDisplayEntryArgsEdits_[index];
    if (!edit) {
      return;
    }
    if (!relatedDisplayEntryArgsSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    relatedDisplayEntryArgsSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitTextMonitorChannel()
  {
    if (!textMonitorChannelEdit_) {
      return;
    }
    if (!textMonitorChannelSetter_) {
      revertLineEdit(textMonitorChannelEdit_);
      return;
    }
    const QString value = textMonitorChannelEdit_->text();
    textMonitorChannelSetter_(value);
    committedTexts_[textMonitorChannelEdit_] = value;
  }

  void commitMeterChannel()
  {
    if (!meterChannelEdit_) {
      return;
    }
    if (!meterChannelSetter_) {
      revertLineEdit(meterChannelEdit_);
      return;
    }
    const QString value = meterChannelEdit_->text();
    meterChannelSetter_(value);
    committedTexts_[meterChannelEdit_] = value;
    updateMeterLimitsFromDialog();
  }

  void commitBarChannel()
  {
    if (!barChannelEdit_) {
      return;
    }
    if (!barChannelSetter_) {
      revertLineEdit(barChannelEdit_);
      return;
    }
    const QString value = barChannelEdit_->text();
    barChannelSetter_(value);
    committedTexts_[barChannelEdit_] = value;
    updateBarLimitsFromDialog();
  }

  void commitScaleChannel()
  {
    if (!scaleChannelEdit_) {
      return;
    }
    if (!scaleChannelSetter_) {
      revertLineEdit(scaleChannelEdit_);
      return;
    }
    const QString value = scaleChannelEdit_->text();
    scaleChannelSetter_(value);
    committedTexts_[scaleChannelEdit_] = value;
    updateScaleLimitsFromDialog();
  }

  void commitStripChartTitle()
  {
    if (!stripTitleEdit_) {
      return;
    }
    if (!stripTitleSetter_) {
      revertLineEdit(stripTitleEdit_);
      return;
    }
    const QString value = stripTitleEdit_->text();
    stripTitleSetter_(value);
    committedTexts_[stripTitleEdit_] = value;
  }

  void commitStripChartXLabel()
  {
    if (!stripXLabelEdit_) {
      return;
    }
    if (!stripXLabelSetter_) {
      revertLineEdit(stripXLabelEdit_);
      return;
    }
    const QString value = stripXLabelEdit_->text();
    stripXLabelSetter_(value);
    committedTexts_[stripXLabelEdit_] = value;
  }

  void commitStripChartYLabel()
  {
    if (!stripYLabelEdit_) {
      return;
    }
    if (!stripYLabelSetter_) {
      revertLineEdit(stripYLabelEdit_);
      return;
    }
    const QString value = stripYLabelEdit_->text();
    stripYLabelSetter_(value);
    committedTexts_[stripYLabelEdit_] = value;
  }

  void commitStripChartPeriod()
  {
    if (!stripPeriodEdit_) {
      return;
    }
    if (!stripPeriodSetter_) {
      revertLineEdit(stripPeriodEdit_);
      return;
    }

    bool ok = false;
    double value = stripPeriodEdit_->text().toDouble(&ok);
    if (!ok || value <= 0.0) {
      revertLineEdit(stripPeriodEdit_);
      return;
    }

    stripPeriodSetter_(value);
    double effective = stripPeriodGetter_ ? stripPeriodGetter_() : value;
    if (effective <= 0.0) {
      effective = kDefaultStripChartPeriod;
    }
    QString text = QString::number(effective, 'f', 3);
    if (text.contains(QLatin1Char('.'))) {
      while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
      }
      if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
      }
    }
    const QSignalBlocker blocker(stripPeriodEdit_);
    stripPeriodEdit_->setText(text);
    committedTexts_[stripPeriodEdit_] = stripPeriodEdit_->text();
  }

  void commitStripChartChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(stripPenChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = stripPenChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!stripPenChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    stripPenChannelSetters_[index](value);
    committedTexts_[edit] = value;
    updateStripChartPenLimitsFromDialog(index);
  }

  void commitCartesianTitle()
  {
    if (!cartesianTitleEdit_) {
      return;
    }
    if (!cartesianTitleSetter_) {
      revertLineEdit(cartesianTitleEdit_);
      return;
    }
    const QString value = cartesianTitleEdit_->text();
    cartesianTitleSetter_(value);
    committedTexts_[cartesianTitleEdit_] = value;
  }

  void commitCartesianXLabel()
  {
    if (!cartesianXLabelEdit_) {
      return;
    }
    if (!cartesianXLabelSetter_) {
      revertLineEdit(cartesianXLabelEdit_);
      return;
    }
    const QString value = cartesianXLabelEdit_->text();
    cartesianXLabelSetter_(value);
    committedTexts_[cartesianXLabelEdit_] = value;
  }

  void commitCartesianYLabel(int index)
  {
    if (index < 0 || index >= static_cast<int>(cartesianYLabelEdits_.size())) {
      return;
    }
    QLineEdit *edit = cartesianYLabelEdits_[index];
    if (!edit) {
      return;
    }
    if (!cartesianYLabelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    cartesianYLabelSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitCartesianCount()
  {
    if (!cartesianCountEdit_) {
      return;
    }
    if (!cartesianCountSetter_) {
      revertLineEdit(cartesianCountEdit_);
      return;
    }
    bool ok = false;
    int value = cartesianCountEdit_->text().toInt(&ok);
    if (!ok || value <= 0) {
      revertLineEdit(cartesianCountEdit_);
      return;
    }
    cartesianCountSetter_(value);
    cartesianCountEdit_->setText(QString::number(std::max(value, 1)));
    committedTexts_[cartesianCountEdit_] = cartesianCountEdit_->text();
  }

  void commitCartesianTrigger()
  {
    if (!cartesianTriggerEdit_) {
      return;
    }
    if (!cartesianTriggerSetter_) {
      revertLineEdit(cartesianTriggerEdit_);
      return;
    }
    const QString value = cartesianTriggerEdit_->text();
    cartesianTriggerSetter_(value);
    committedTexts_[cartesianTriggerEdit_] = value;
  }

  void commitCartesianErase()
  {
    if (!cartesianEraseEdit_) {
      return;
    }
    if (!cartesianEraseSetter_) {
      revertLineEdit(cartesianEraseEdit_);
      return;
    }
    const QString value = cartesianEraseEdit_->text();
    cartesianEraseSetter_(value);
    committedTexts_[cartesianEraseEdit_] = value;
  }

  void commitCartesianCountPv()
  {
    if (!cartesianCountPvEdit_) {
      return;
    }
    if (!cartesianCountPvSetter_) {
      revertLineEdit(cartesianCountPvEdit_);
      return;
    }
    const QString value = cartesianCountPvEdit_->text();
    cartesianCountPvSetter_(value);
    committedTexts_[cartesianCountPvEdit_] = value;
  }

  void commitCartesianTraceXChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(cartesianTraceXEdits_.size())) {
      return;
    }
    QLineEdit *edit = cartesianTraceXEdits_[index];
    if (!edit) {
      return;
    }
    if (!cartesianTraceXSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    cartesianTraceXSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitCartesianTraceYChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(cartesianTraceYEdits_.size())) {
      return;
    }
    QLineEdit *edit = cartesianTraceYEdits_[index];
    if (!edit) {
      return;
    }
    if (!cartesianTraceYSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    cartesianTraceYSetters_[index](value);
    committedTexts_[edit] = value;
  }

  void commitByteChannel()
  {
    if (!byteChannelEdit_) {
      return;
    }
    if (!byteChannelSetter_) {
      revertLineEdit(byteChannelEdit_);
      return;
    }
    const QString value = byteChannelEdit_->text();
    byteChannelSetter_(value);
    committedTexts_[byteChannelEdit_] = value;
  }

  void handleStripChartUnitsChanged(int index)
  {
    if (!stripUnitsCombo_) {
      return;
    }
    if (!stripUnitsSetter_) {
      const QSignalBlocker blocker(stripUnitsCombo_);
      const int currentIndex = stripUnitsGetter_
              ? timeUnitsToIndex(stripUnitsGetter_())
              : timeUnitsToIndex(TimeUnits::kSeconds);
      stripUnitsCombo_->setCurrentIndex(currentIndex);
      return;
    }
    const TimeUnits units = timeUnitsFromIndex(index);
    stripUnitsSetter_(units);
    if (stripUnitsGetter_) {
      const QSignalBlocker blocker(stripUnitsCombo_);
      stripUnitsCombo_->setCurrentIndex(timeUnitsToIndex(stripUnitsGetter_()));
    }
  }

  void handleCartesianStyleChanged(int index)
  {
    if (!cartesianStyleCombo_) {
      return;
    }
    if (!cartesianStyleSetter_) {
      const QSignalBlocker blocker(cartesianStyleCombo_);
      const int currentIndex = cartesianStyleGetter_
              ? cartesianPlotStyleToIndex(cartesianStyleGetter_())
              : cartesianPlotStyleToIndex(CartesianPlotStyle::kLine);
      cartesianStyleCombo_->setCurrentIndex(currentIndex);
      return;
    }
    cartesianStyleSetter_(indexToCartesianPlotStyle(index));
    if (cartesianStyleGetter_) {
      const QSignalBlocker blocker(cartesianStyleCombo_);
      cartesianStyleCombo_->setCurrentIndex(
          cartesianPlotStyleToIndex(cartesianStyleGetter_()));
    }
  }

  void handleCartesianEraseOldestChanged(int index)
  {
    if (!cartesianEraseOldestCombo_) {
      return;
    }
    if (!cartesianEraseOldestSetter_) {
      const QSignalBlocker blocker(cartesianEraseOldestCombo_);
      const bool eraseOldest = cartesianEraseOldestGetter_
              ? cartesianEraseOldestGetter_()
              : false;
      cartesianEraseOldestCombo_->setCurrentIndex(eraseOldest ? 1 : 0);
      return;
    }
    const bool value = (index != 0);
    cartesianEraseOldestSetter_(value);
    if (cartesianEraseOldestGetter_) {
      const QSignalBlocker blocker(cartesianEraseOldestCombo_);
      cartesianEraseOldestCombo_->setCurrentIndex(
          cartesianEraseOldestGetter_() ? 1 : 0);
    }
  }

  void handleCartesianEraseModeChanged(int index)
  {
    if (!cartesianEraseModeCombo_) {
      return;
    }
    if (!cartesianEraseModeSetter_) {
      const QSignalBlocker blocker(cartesianEraseModeCombo_);
      const int currentIndex = cartesianEraseModeGetter_
              ? cartesianEraseModeToIndex(cartesianEraseModeGetter_())
              : cartesianEraseModeToIndex(CartesianPlotEraseMode::kIfNotZero);
      cartesianEraseModeCombo_->setCurrentIndex(currentIndex);
      return;
    }
    cartesianEraseModeSetter_(indexToCartesianPlotEraseMode(index));
    if (cartesianEraseModeGetter_) {
      const QSignalBlocker blocker(cartesianEraseModeCombo_);
      cartesianEraseModeCombo_->setCurrentIndex(
          cartesianEraseModeToIndex(cartesianEraseModeGetter_()));
    }
  }

  void handleCartesianTraceAxisChanged(int index, int comboIndex)
  {
    if (index < 0 || index >= static_cast<int>(cartesianTraceAxisCombos_.size())) {
      return;
    }
    if (!cartesianTraceAxisCombos_[index]) {
      return;
    }
    if (!cartesianTraceAxisSetters_[index]) {
      const QSignalBlocker blocker(cartesianTraceAxisCombos_[index]);
      const int currentIndex = cartesianTraceAxisGetters_[index]
              ? cartesianAxisToIndex(cartesianTraceAxisGetters_[index]())
              : cartesianAxisToIndex(CartesianPlotYAxis::kY1);
      cartesianTraceAxisCombos_[index]->setCurrentIndex(currentIndex);
      return;
    }
    cartesianTraceAxisSetters_[index](indexToCartesianAxis(comboIndex));
    if (cartesianTraceAxisGetters_[index]) {
      const QSignalBlocker blocker(cartesianTraceAxisCombos_[index]);
      cartesianTraceAxisCombos_[index]->setCurrentIndex(
          cartesianAxisToIndex(cartesianTraceAxisGetters_[index]()));
    }
  }

  void handleCartesianTraceSideChanged(int index, int comboIndex)
  {
    if (index < 0 || index >= static_cast<int>(cartesianTraceSideCombos_.size())) {
      return;
    }
    if (!cartesianTraceSideCombos_[index]) {
      return;
    }
    if (!cartesianTraceSideSetters_[index]) {
      const QSignalBlocker blocker(cartesianTraceSideCombos_[index]);
      const bool usesRight = cartesianTraceSideGetters_[index]
              ? cartesianTraceSideGetters_[index]()
              : false;
      cartesianTraceSideCombos_[index]->setCurrentIndex(usesRight ? 1 : 0);
      return;
    }
    const bool usesRight = (comboIndex != 0);
    cartesianTraceSideSetters_[index](usesRight);
    if (cartesianTraceSideGetters_[index]) {
      const QSignalBlocker blocker(cartesianTraceSideCombos_[index]);
      cartesianTraceSideCombos_[index]->setCurrentIndex(
          cartesianTraceSideGetters_[index]() ? 1 : 0);
    }
  }

  void openStripChartLimitsDialog(int index)
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (index < 0 || index >= kStripChartPenCount) {
      dialog->clearTargets();
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    if (stripPenLimitsGetters_[index] && stripPenLimitsSetters_[index]) {
      const QString channelLabel = stripPenChannelGetters_[index]
              ? stripPenChannelGetters_[index]()
              : QString();
      dialog->setStripChartCallbacks(channelLabel,
          stripPenLimitsGetters_[index], stripPenLimitsSetters_[index],
          [this, index]() { updateStripChartPenLimitsFromDialog(index); });
      dialog->showForStripChart();
    } else {
      dialog->clearTargets();
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
    }
  }

  void updateStripChartPenLimitsFromDialog(int index)
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (index < 0 || index >= kStripChartPenCount) {
      pvLimitsDialog_->clearTargets();
      return;
    }
    if (stripPenLimitsGetters_[index] && stripPenLimitsSetters_[index]) {
      const QString channelLabel = stripPenChannelGetters_[index]
              ? stripPenChannelGetters_[index]()
              : QString();
      pvLimitsDialog_->setStripChartCallbacks(channelLabel,
          stripPenLimitsGetters_[index], stripPenLimitsSetters_[index],
          [this, index]() { updateStripChartPenLimitsFromDialog(index); });
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void commitByteStartBit(int value)
  {
    if (!byteStartBitSpin_) {
      return;
    }
    if (!byteStartBitSetter_) {
      if (byteStartBitGetter_) {
        const QSignalBlocker blocker(byteStartBitSpin_);
        int current = std::clamp(byteStartBitGetter_(), 0, 31);
        byteStartBitSpin_->setValue(current);
      }
      return;
    }
    value = std::clamp(value, 0, 31);
    byteStartBitSetter_(value);
    if (byteStartBitGetter_) {
      const QSignalBlocker blocker(byteStartBitSpin_);
      int current = std::clamp(byteStartBitGetter_(), 0, 31);
      byteStartBitSpin_->setValue(current);
    }
  }

  void commitByteEndBit(int value)
  {
    if (!byteEndBitSpin_) {
      return;
    }
    if (!byteEndBitSetter_) {
      if (byteEndBitGetter_) {
        const QSignalBlocker blocker(byteEndBitSpin_);
        int current = std::clamp(byteEndBitGetter_(), 0, 31);
        byteEndBitSpin_->setValue(current);
      }
      return;
    }
    value = std::clamp(value, 0, 31);
    byteEndBitSetter_(value);
    if (byteEndBitGetter_) {
      const QSignalBlocker blocker(byteEndBitSpin_);
      int current = std::clamp(byteEndBitGetter_(), 0, 31);
      byteEndBitSpin_->setValue(current);
    }
  }

  void updateSliderIncrementEdit()
  {
    if (!sliderIncrementEdit_) {
      return;
    }
    const QSignalBlocker blocker(sliderIncrementEdit_);
    if (!sliderIncrementGetter_) {
      sliderIncrementEdit_->clear();
    } else {
      const double increment = sliderIncrementGetter_();
      sliderIncrementEdit_->setText(QString::number(increment, 'g', 6));
    }
    committedTexts_[sliderIncrementEdit_] = sliderIncrementEdit_->text();
  }

  void updateWheelSwitchPrecisionEdit()
  {
    if (!wheelSwitchPrecisionEdit_) {
      return;
    }
    const QSignalBlocker blocker(wheelSwitchPrecisionEdit_);
    if (!wheelSwitchPrecisionGetter_) {
      wheelSwitchPrecisionEdit_->clear();
    } else {
      const double precision = wheelSwitchPrecisionGetter_();
      wheelSwitchPrecisionEdit_->setText(QString::number(precision, 'g', 6));
    }
    committedTexts_[wheelSwitchPrecisionEdit_] = wheelSwitchPrecisionEdit_->text();
  }

  void updateTextMonitorLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (textMonitorPrecisionSourceGetter_) {
      const QString channelLabel = textMonitorChannelGetter_
              ? textMonitorChannelGetter_()
              : QString();
      pvLimitsDialog_->setTextMonitorCallbacks(channelLabel,
          textMonitorPrecisionSourceGetter_, textMonitorPrecisionSourceSetter_,
          textMonitorPrecisionDefaultGetter_,
          textMonitorPrecisionDefaultSetter_,
          [this]() { updateTextMonitorLimitsFromDialog(); },
          textMonitorLimitsGetter_, textMonitorLimitsSetter_);
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateTextEntryLimitsFromDialog()
  {
  }

  void updateSliderLimitsFromDialog()
  {
    updateSliderIncrementEdit();
    if (!pvLimitsDialog_) {
      return;
    }
    if (sliderLimitsGetter_ && sliderLimitsSetter_) {
      const QString channelLabel = sliderChannelGetter_ ? sliderChannelGetter_()
                                                        : QString();
      pvLimitsDialog_->setSliderCallbacks(channelLabel, sliderLimitsGetter_,
          sliderLimitsSetter_, [this]() { updateSliderLimitsFromDialog(); });
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateWheelSwitchLimitsFromDialog()
  {
    updateWheelSwitchPrecisionEdit();
    if (!pvLimitsDialog_) {
      return;
    }
    if (wheelSwitchLimitsGetter_ && wheelSwitchLimitsSetter_) {
      const QString channelLabel = wheelSwitchChannelGetter_ ? wheelSwitchChannelGetter_()
                                                             : QString();
      pvLimitsDialog_->setWheelSwitchCallbacks(channelLabel,
          wheelSwitchLimitsGetter_, wheelSwitchLimitsSetter_,
          [this]() { updateWheelSwitchLimitsFromDialog(); });
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateMeterLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (meterLimitsGetter_ && meterLimitsSetter_) {
      const QString channelLabel = meterChannelGetter_ ? meterChannelGetter_()
                                                       : QString();
      pvLimitsDialog_->setMeterCallbacks(channelLabel, meterLimitsGetter_,
          meterLimitsSetter_, [this]() { updateMeterLimitsFromDialog(); });
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateBarLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (barLimitsGetter_ && barLimitsSetter_) {
      const QString channelLabel = barChannelGetter_ ? barChannelGetter_()
                                                     : QString();
      pvLimitsDialog_->setBarCallbacks(channelLabel, barLimitsGetter_,
          barLimitsSetter_, [this]() { updateBarLimitsFromDialog(); });
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateScaleLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (scaleLimitsGetter_ && scaleLimitsSetter_) {
      const QString channelLabel = scaleChannelGetter_ ? scaleChannelGetter_()
                                                       : QString();
      pvLimitsDialog_->setScaleCallbacks(channelLabel, scaleLimitsGetter_,
          scaleLimitsSetter_, [this]() { updateScaleLimitsFromDialog(); });
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateCartesianAxisButtonState()
  {
    if (!cartesianAxisButton_) {
      return;
    }
    bool hasSetter = false;
    for (int i = 0; i < kCartesianAxisCount; ++i) {
      if (cartesianAxisStyleSetters_[i]
          || cartesianAxisRangeSetters_[i]
          || cartesianAxisMinimumSetters_[i]
          || cartesianAxisMaximumSetters_[i]
          || cartesianAxisTimeFormatSetters_[i]) {
        hasSetter = true;
        break;
      }
    }
    cartesianAxisButton_->setEnabled(hasSetter);
    if (hasSetter) {
      cartesianAxisButton_->setToolTip(
          QStringLiteral("Edit Cartesian plot axis styles, ranges, and time format"));
    } else {
      cartesianAxisButton_->setToolTip(QString());
    }
  }

  void commitRectangleLineWidth()
  {
    if (!rectangleLineWidthEdit_) {
      return;
    }
    if (!rectangleLineWidthSetter_) {
      revertLineEdit(rectangleLineWidthEdit_);
      return;
    }
    bool ok = false;
    int value = rectangleLineWidthEdit_->text().toInt(&ok);
    if (!ok) {
      revertLineEdit(rectangleLineWidthEdit_);
      return;
    }
    value = std::max(1, value);
    rectangleLineWidthSetter_(value);
    const int effectiveWidth = rectangleLineWidthGetter_
        ? rectangleLineWidthGetter_()
        : value;
    const int clampedWidth = std::max(1, effectiveWidth);
    const QSignalBlocker blocker(rectangleLineWidthEdit_);
    rectangleLineWidthEdit_->setText(QString::number(clampedWidth));
    committedTexts_[rectangleLineWidthEdit_] = rectangleLineWidthEdit_->text();
  }

  void commitRectangleVisibilityCalc()
  {
    if (!rectangleVisibilityCalcEdit_) {
      return;
    }
    if (!rectangleVisibilityCalcSetter_) {
      revertLineEdit(rectangleVisibilityCalcEdit_);
      return;
    }
    const QString value = rectangleVisibilityCalcEdit_->text();
    rectangleVisibilityCalcSetter_(value);
    committedTexts_[rectangleVisibilityCalcEdit_] = value;
  }

  void commitRectangleChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(rectangleChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = rectangleChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!rectangleChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    rectangleChannelSetters_[index](value);
    committedTexts_[edit] = value;
    if (index == 0) {
      updateRectangleChannelDependentControls();
    }
  }

  void commitCompositeFile()
  {
    if (!compositeFileEdit_) {
      return;
    }
    if (!compositeFileSetter_) {
      revertLineEdit(compositeFileEdit_);
      return;
    }
    const QString value = compositeFileEdit_->text();
    compositeFileSetter_(value);
    committedTexts_[compositeFileEdit_] = value;
  }

  void commitCompositeVisibilityCalc()
  {
    if (!compositeVisibilityCalcEdit_) {
      return;
    }
    if (!compositeVisibilityCalcSetter_) {
      revertLineEdit(compositeVisibilityCalcEdit_);
      return;
    }
    const QString value = compositeVisibilityCalcEdit_->text();
    compositeVisibilityCalcSetter_(value);
    committedTexts_[compositeVisibilityCalcEdit_] = value;
  }

  void commitCompositeChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(compositeChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = compositeChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!compositeChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    compositeChannelSetters_[index](value);
    committedTexts_[edit] = value;
    if (index == 0) {
      updateCompositeChannelDependentControls();
    }
  }

  void commitImageName()
  {
    if (!imageNameEdit_) {
      return;
    }
    if (!imageNameSetter_) {
      revertLineEdit(imageNameEdit_);
      return;
    }
    const QString value = imageNameEdit_->text();
    imageNameSetter_(value);
    committedTexts_[imageNameEdit_] = value;
  }

  void commitImageCalc()
  {
    if (!imageCalcEdit_) {
      return;
    }
    if (!imageCalcSetter_) {
      revertLineEdit(imageCalcEdit_);
      return;
    }
    const QString value = imageCalcEdit_->text();
    imageCalcSetter_(value);
    committedTexts_[imageCalcEdit_] = value;
  }

  void commitImageVisibilityCalc()
  {
    if (!imageVisibilityCalcEdit_) {
      return;
    }
    if (!imageVisibilityCalcSetter_) {
      revertLineEdit(imageVisibilityCalcEdit_);
      return;
    }
    const QString value = imageVisibilityCalcEdit_->text();
    imageVisibilityCalcSetter_(value);
    committedTexts_[imageVisibilityCalcEdit_] = value;
  }

  void commitHeatmapDataChannel()
  {
    if (!heatmapDataChannelEdit_) {
      return;
    }
    if (!heatmapDataChannelSetter_) {
      revertLineEdit(heatmapDataChannelEdit_);
      return;
    }
    const QString value = heatmapDataChannelEdit_->text();
    heatmapDataChannelSetter_(value);
    committedTexts_[heatmapDataChannelEdit_] = value;
  }

  void commitHeatmapTitle()
  {
    if (!heatmapTitleEdit_) {
      return;
    }
    if (!heatmapTitleSetter_) {
      revertLineEdit(heatmapTitleEdit_);
      return;
    }
    const QString value = heatmapTitleEdit_->text();
    heatmapTitleSetter_(value);
    committedTexts_[heatmapTitleEdit_] = value;
  }

  void commitHeatmapXDimension()
  {
    if (!heatmapXDimEdit_) {
      return;
    }
    if (!heatmapXDimensionSetter_) {
      revertLineEdit(heatmapXDimEdit_);
      return;
    }
    bool ok = false;
    const int value = heatmapXDimEdit_->text().toInt(&ok);
    if (!ok || value <= 0) {
      revertLineEdit(heatmapXDimEdit_);
      return;
    }
    heatmapXDimensionSetter_(value);
    committedTexts_[heatmapXDimEdit_] = heatmapXDimEdit_->text();
  }

  void commitHeatmapYDimension()
  {
    if (!heatmapYDimEdit_) {
      return;
    }
    if (!heatmapYDimensionSetter_) {
      revertLineEdit(heatmapYDimEdit_);
      return;
    }
    bool ok = false;
    const int value = heatmapYDimEdit_->text().toInt(&ok);
    if (!ok || value <= 0) {
      revertLineEdit(heatmapYDimEdit_);
      return;
    }
    heatmapYDimensionSetter_(value);
    committedTexts_[heatmapYDimEdit_] = heatmapYDimEdit_->text();
  }

  void commitHeatmapXDimensionChannel()
  {
    if (!heatmapXDimChannelEdit_) {
      return;
    }
    if (!heatmapXDimChannelSetter_) {
      revertLineEdit(heatmapXDimChannelEdit_);
      return;
    }
    const QString value = heatmapXDimChannelEdit_->text();
    heatmapXDimChannelSetter_(value);
    committedTexts_[heatmapXDimChannelEdit_] = value;
  }

  void commitHeatmapYDimensionChannel()
  {
    if (!heatmapYDimChannelEdit_) {
      return;
    }
    if (!heatmapYDimChannelSetter_) {
      revertLineEdit(heatmapYDimChannelEdit_);
      return;
    }
    const QString value = heatmapYDimChannelEdit_->text();
    heatmapYDimChannelSetter_(value);
    committedTexts_[heatmapYDimChannelEdit_] = value;
  }

  void commitImageChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(imageChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = imageChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!imageChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    imageChannelSetters_[index](value);
    committedTexts_[edit] = value;
    if (index == 0) {
      updateImageChannelDependentControls();
    }
  }

  void commitLineLineWidth()
  {
    if (!lineLineWidthEdit_) {
      return;
    }
    if (!lineLineWidthSetter_) {
      revertLineEdit(lineLineWidthEdit_);
      return;
    }
    bool ok = false;
    int value = lineLineWidthEdit_->text().toInt(&ok);
    if (!ok) {
      revertLineEdit(lineLineWidthEdit_);
      return;
    }
    value = std::max(1, value);
    lineLineWidthSetter_(value);
    const int effectiveWidth = lineLineWidthGetter_ ? lineLineWidthGetter_()
                                                    : value;
    const int clampedWidth = std::max(1, effectiveWidth);
    const QSignalBlocker blocker(lineLineWidthEdit_);
    lineLineWidthEdit_->setText(QString::number(clampedWidth));
    committedTexts_[lineLineWidthEdit_] = lineLineWidthEdit_->text();
  }

  void commitLineVisibilityCalc()
  {
    if (!lineVisibilityCalcEdit_) {
      return;
    }
    if (!lineVisibilityCalcSetter_) {
      revertLineEdit(lineVisibilityCalcEdit_);
      return;
    }
    const QString value = lineVisibilityCalcEdit_->text();
    lineVisibilityCalcSetter_(value);
    committedTexts_[lineVisibilityCalcEdit_] = value;
  }

  void commitLineChannel(int index)
  {
    if (index < 0 || index >= static_cast<int>(lineChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = lineChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!lineChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    lineChannelSetters_[index](value);
    committedTexts_[edit] = value;
    if (index == 0) {
      updateLineChannelDependentControls();
    }
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

  TextMonitorFormat textMonitorFormatFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return TextMonitorFormat::kExponential;
    case 2:
      return TextMonitorFormat::kEngineering;
    case 3:
      return TextMonitorFormat::kCompact;
    case 4:
      return TextMonitorFormat::kTruncated;
    case 5:
      return TextMonitorFormat::kHexadecimal;
    case 6:
      return TextMonitorFormat::kOctal;
    case 7:
      return TextMonitorFormat::kString;
    case 8:
      return TextMonitorFormat::kSexagesimal;
    case 9:
      return TextMonitorFormat::kSexagesimalHms;
    case 10:
      return TextMonitorFormat::kSexagesimalDms;
    case 0:
    default:
      return TextMonitorFormat::kDecimal;
    }
  }

  int textMonitorFormatToIndex(TextMonitorFormat format) const
  {
    switch (format) {
    case TextMonitorFormat::kExponential:
      return 1;
    case TextMonitorFormat::kEngineering:
      return 2;
    case TextMonitorFormat::kCompact:
      return 3;
    case TextMonitorFormat::kTruncated:
      return 4;
    case TextMonitorFormat::kHexadecimal:
      return 5;
    case TextMonitorFormat::kOctal:
      return 6;
    case TextMonitorFormat::kString:
      return 7;
    case TextMonitorFormat::kSexagesimal:
      return 8;
    case TextMonitorFormat::kSexagesimalHms:
      return 9;
    case TextMonitorFormat::kSexagesimalDms:
      return 10;
    case TextMonitorFormat::kDecimal:
    default:
      return 0;
    }
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

  HeatmapDimensionSource heatmapDimensionSourceFromIndex(int index) const
  {
    return index == 1 ? HeatmapDimensionSource::kChannel
                      : HeatmapDimensionSource::kStatic;
  }

  int heatmapDimensionSourceToIndex(HeatmapDimensionSource source) const
  {
    return source == HeatmapDimensionSource::kChannel ? 1 : 0;
  }

  HeatmapOrder heatmapOrderFromIndex(int index) const
  {
    return index == 1 ? HeatmapOrder::kColumnMajor
                      : HeatmapOrder::kRowMajor;
  }

  int heatmapOrderToIndex(HeatmapOrder order) const
  {
    return order == HeatmapOrder::kColumnMajor ? 1 : 0;
  }

  MeterLabel meterLabelFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return MeterLabel::kNoDecorations;
    case 2:
      return MeterLabel::kOutline;
    case 3:
      return MeterLabel::kLimits;
    case 4:
      return MeterLabel::kChannel;
    case 0:
    default:
      return MeterLabel::kNone;
    }
  }

  int meterLabelToIndex(MeterLabel label) const
  {
    switch (label) {
    case MeterLabel::kNoDecorations:
      return 1;
    case MeterLabel::kOutline:
      return 2;
    case MeterLabel::kLimits:
      return 3;
    case MeterLabel::kChannel:
      return 4;
    case MeterLabel::kNone:
    default:
      return 0;
    }
  }

  BarDirection barDirectionFromIndex(int index) const
  {
    switch (index) {
    case 0:
      return BarDirection::kUp;
    case 1:
      return BarDirection::kRight;
    case 2:
      return BarDirection::kDown;
    case 3:
    default:
      return BarDirection::kLeft;
    }
  }

  int barDirectionToIndex(BarDirection direction) const
  {
    switch (direction) {
    case BarDirection::kUp:
      return 0;
    case BarDirection::kRight:
      return 1;
    case BarDirection::kDown:
      return 2;
    case BarDirection::kLeft:
    default:
      return 3;
    }
  }

  BarDirection scaleDirectionFromIndex(int index) const
  {
    switch (index) {
    case 0:
      return BarDirection::kUp;
    case 1:
    default:
      return BarDirection::kRight;
    }
  }

  int scaleDirectionToIndex(BarDirection direction) const
  {
    return (direction == BarDirection::kUp) ? 0 : 1;
  }

  BarFill barFillFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return BarFill::kFromCenter;
    case 0:
    default:
      return BarFill::kFromEdge;
    }
  }

  int barFillToIndex(BarFill fill) const
  {
    switch (fill) {
    case BarFill::kFromCenter:
      return 1;
    case BarFill::kFromEdge:
    default:
      return 0;
    }
  }

  TimeUnits timeUnitsFromIndex(int index) const
  {
    switch (index) {
    case 0:
      return TimeUnits::kMilliseconds;
    case 2:
      return TimeUnits::kMinutes;
    case 1:
    default:
      return TimeUnits::kSeconds;
    }
  }

  int timeUnitsToIndex(TimeUnits units) const
  {
    switch (units) {
    case TimeUnits::kMilliseconds:
      return 0;
    case TimeUnits::kMinutes:
      return 2;
    case TimeUnits::kSeconds:
    default:
      return 1;
    }
  }

  int cartesianPlotStyleToIndex(CartesianPlotStyle style) const
  {
    switch (style) {
    case CartesianPlotStyle::kPoint:
      return 0;
    case CartesianPlotStyle::kLine:
      return 1;
    case CartesianPlotStyle::kStep:
      return 2;
    case CartesianPlotStyle::kFillUnder:
      return 3;
    default:
      return 1;
    }
  }

  CartesianPlotStyle indexToCartesianPlotStyle(int index) const
  {
    switch (index) {
    case 0:
      return CartesianPlotStyle::kPoint;
    case 2:
      return CartesianPlotStyle::kStep;
    case 3:
      return CartesianPlotStyle::kFillUnder;
    case 1:
    default:
      return CartesianPlotStyle::kLine;
    }
  }

  int cartesianEraseModeToIndex(CartesianPlotEraseMode mode) const
  {
    switch (mode) {
    case CartesianPlotEraseMode::kIfZero:
      return 1;
    case CartesianPlotEraseMode::kIfNotZero:
    default:
      return 0;
    }
  }

  CartesianPlotEraseMode indexToCartesianPlotEraseMode(int index) const
  {
    switch (index) {
    case 1:
      return CartesianPlotEraseMode::kIfZero;
    case 0:
    default:
      return CartesianPlotEraseMode::kIfNotZero;
    }
  }

  int cartesianAxisToIndex(CartesianPlotYAxis axis) const
  {
    switch (axis) {
    case CartesianPlotYAxis::kY2:
      return 1;
    case CartesianPlotYAxis::kY3:
      return 2;
    case CartesianPlotYAxis::kY4:
      return 3;
    case CartesianPlotYAxis::kY1:
    default:
      return 0;
    }
  }

  CartesianPlotYAxis indexToCartesianAxis(int index) const
  {
    switch (index) {
    case 1:
      return CartesianPlotYAxis::kY2;
    case 2:
      return CartesianPlotYAxis::kY3;
    case 3:
      return CartesianPlotYAxis::kY4;
    case 0:
    default:
      return CartesianPlotYAxis::kY1;
    }
  }

  static int degreesToAngle64(int degrees)
  {
    return degrees * 64;
  }

  static int angle64ToDegrees(int angle64)
  {
    if (angle64 >= 0) {
      return (angle64 + 32) / 64;
    }
    return (angle64 - 32) / 64;
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

  RectangleFill fillFromIndex(int index) const
  {
    return index == 1 ? RectangleFill::kSolid : RectangleFill::kOutline;
  }

  int fillToIndex(RectangleFill fill) const
  {
    return fill == RectangleFill::kSolid ? 1 : 0;
  }

  RectangleLineStyle lineStyleFromIndex(int index) const
  {
    return index == 1 ? RectangleLineStyle::kDash : RectangleLineStyle::kSolid;
  }

  int lineStyleToIndex(RectangleLineStyle style) const
  {
    return style == RectangleLineStyle::kDash ? 1 : 0;
  }

  ImageType imageTypeFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return ImageType::kGif;
    case 2:
      return ImageType::kTiff;
    default:
      return ImageType::kNone;
    }
  }

  int imageTypeToIndex(ImageType type) const
  {
    switch (type) {
    case ImageType::kGif:
      return 1;
    case ImageType::kTiff:
      return 2;
    case ImageType::kNone:
    default:
      return 0;
    }
  }

  ChoiceButtonStacking choiceButtonStackingFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return ChoiceButtonStacking::kColumn;
    case 2:
      return ChoiceButtonStacking::kRowColumn;
    case 0:
    default:
      return ChoiceButtonStacking::kRow;
    }
  }

  int choiceButtonStackingToIndex(ChoiceButtonStacking stacking) const
  {
    switch (stacking) {
    case ChoiceButtonStacking::kColumn:
      return 1;
    case ChoiceButtonStacking::kRowColumn:
      return 2;
    case ChoiceButtonStacking::kRow:
    default:
      return 0;
    }
  }

  RelatedDisplayVisual relatedDisplayVisualFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return RelatedDisplayVisual::kRowOfButtons;
    case 2:
      return RelatedDisplayVisual::kColumnOfButtons;
    case 3:
      return RelatedDisplayVisual::kHiddenButton;
    case 0:
    default:
      return RelatedDisplayVisual::kMenu;
    }
  }

  int relatedDisplayVisualToIndex(RelatedDisplayVisual visual) const
  {
    switch (visual) {
    case RelatedDisplayVisual::kRowOfButtons:
      return 1;
    case RelatedDisplayVisual::kColumnOfButtons:
      return 2;
    case RelatedDisplayVisual::kHiddenButton:
      return 3;
    case RelatedDisplayVisual::kMenu:
    default:
      return 0;
    }
  }

  RelatedDisplayMode relatedDisplayModeFromIndex(int index) const
  {
    return index == 1 ? RelatedDisplayMode::kReplace : RelatedDisplayMode::kAdd;
  }

  int relatedDisplayModeToIndex(RelatedDisplayMode mode) const
  {
    return mode == RelatedDisplayMode::kReplace ? 1 : 0;
  }

  void showPaletteWithoutActivating()
  {
    const bool alreadySet =
        testAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    show();
    positionRelativeTo(parentWidget());
    raise();
    if (!alreadySet) {
      setAttribute(Qt::WA_ShowWithoutActivating, false);
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
    QPointer<QWidget> ref(reference);
    QMetaObject::invokeMethod(this, [this, ref]() {
      QWidget *referenceWidget = ref ? ref.data() : parentWidget();
      QWidget *anchorWidget = referenceWidget ? referenceWidget : this;
      QScreen *screen = screenForWidget(anchorWidget);
      if (!screen) {
        screen = QGuiApplication::primaryScreen();
      }
      const QRect available = screen ? screen->availableGeometry() : QRect();

      resizeToFitContents(available);

      if (referenceWidget) {
        const QRect referenceFrame = referenceWidget->frameGeometry();
        QPoint desiredTopLeft(referenceFrame.topRight());
        desiredTopLeft.rx() += 12;
        QRect desiredRect(desiredTopLeft, size());
        if (available.isNull() || available.contains(desiredRect)) {
          move(desiredTopLeft);
          return;
        }
      }

      moveToTopRight(available, size());
    }, Qt::QueuedConnection);
  }

  QFont labelFont_;
  QFont valueFont_;
  QWidget *geometrySection_ = nullptr;
  QWidget *displaySection_ = nullptr;
  QWidget *rectangleSection_ = nullptr;
  QWidget *compositeSection_ = nullptr;
  QWidget *imageSection_ = nullptr;
  QWidget *heatmapSection_ = nullptr;
  QWidget *lineSection_ = nullptr;
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
  std::array<QLineEdit *, 5> textChannelEdits_{};
  QLineEdit *heatmapTitleEdit_ = nullptr;
  QLineEdit *heatmapDataChannelEdit_ = nullptr;
  QComboBox *heatmapXSourceCombo_ = nullptr;
  QComboBox *heatmapYSourceCombo_ = nullptr;
  QLineEdit *heatmapXDimEdit_ = nullptr;
  QLineEdit *heatmapYDimEdit_ = nullptr;
  QLineEdit *heatmapXDimChannelEdit_ = nullptr;
  QLineEdit *heatmapYDimChannelEdit_ = nullptr;
  QComboBox *heatmapOrderCombo_ = nullptr;
  QWidget *textMonitorSection_ = nullptr;
  QPushButton *textMonitorForegroundButton_ = nullptr;
  QPushButton *textMonitorBackgroundButton_ = nullptr;
  QComboBox *textMonitorAlignmentCombo_ = nullptr;
  QComboBox *textMonitorFormatCombo_ = nullptr;
  QComboBox *textMonitorColorModeCombo_ = nullptr;
  QLineEdit *textMonitorChannelEdit_ = nullptr;
  QPushButton *textMonitorPvLimitsButton_ = nullptr;
  QWidget *textEntrySection_ = nullptr;
  QPushButton *textEntryForegroundButton_ = nullptr;
  QPushButton *textEntryBackgroundButton_ = nullptr;
  QComboBox *textEntryFormatCombo_ = nullptr;
  QComboBox *textEntryColorModeCombo_ = nullptr;
  QLineEdit *textEntryChannelEdit_ = nullptr;
  QPushButton *textEntryPvLimitsButton_ = nullptr;
  QWidget *sliderSection_ = nullptr;
  QPushButton *sliderForegroundButton_ = nullptr;
  QPushButton *sliderBackgroundButton_ = nullptr;
  QComboBox *sliderLabelCombo_ = nullptr;
  QComboBox *sliderColorModeCombo_ = nullptr;
  QComboBox *sliderDirectionCombo_ = nullptr;
  QLineEdit *sliderIncrementEdit_ = nullptr;
  QLineEdit *sliderChannelEdit_ = nullptr;
  QPushButton *sliderPvLimitsButton_ = nullptr;
  QWidget *wheelSwitchSection_ = nullptr;
  QPushButton *wheelSwitchForegroundButton_ = nullptr;
  QPushButton *wheelSwitchBackgroundButton_ = nullptr;
  QComboBox *wheelSwitchColorModeCombo_ = nullptr;
  QLineEdit *wheelSwitchPrecisionEdit_ = nullptr;
  QLineEdit *wheelSwitchFormatEdit_ = nullptr;
  QLineEdit *wheelSwitchChannelEdit_ = nullptr;
  QPushButton *wheelSwitchPvLimitsButton_ = nullptr;
  QWidget *choiceButtonSection_ = nullptr;
  QPushButton *choiceButtonForegroundButton_ = nullptr;
  QPushButton *choiceButtonBackgroundButton_ = nullptr;
  QComboBox *choiceButtonColorModeCombo_ = nullptr;
  QComboBox *choiceButtonStackingCombo_ = nullptr;
  QLineEdit *choiceButtonChannelEdit_ = nullptr;
  QWidget *menuSection_ = nullptr;
  QPushButton *menuForegroundButton_ = nullptr;
  QPushButton *menuBackgroundButton_ = nullptr;
  QComboBox *menuColorModeCombo_ = nullptr;
  QLineEdit *menuChannelEdit_ = nullptr;
  QWidget *messageButtonSection_ = nullptr;
  QPushButton *messageButtonForegroundButton_ = nullptr;
  QPushButton *messageButtonBackgroundButton_ = nullptr;
  QComboBox *messageButtonColorModeCombo_ = nullptr;
  QLineEdit *messageButtonLabelEdit_ = nullptr;
  QLineEdit *messageButtonPressEdit_ = nullptr;
  QLineEdit *messageButtonReleaseEdit_ = nullptr;
  QLineEdit *messageButtonChannelEdit_ = nullptr;
  QWidget *shellCommandSection_ = nullptr;
  QPushButton *shellCommandForegroundButton_ = nullptr;
  QPushButton *shellCommandBackgroundButton_ = nullptr;
  QLineEdit *shellCommandLabelEdit_ = nullptr;
  QWidget *shellCommandEntriesWidget_ = nullptr;
  std::array<QLineEdit *, kShellCommandEntryCount> shellCommandEntryLabelEdits_{};
  std::array<QLineEdit *, kShellCommandEntryCount> shellCommandEntryCommandEdits_{};
  std::array<QLineEdit *, kShellCommandEntryCount> shellCommandEntryArgsEdits_{};
  QWidget *relatedDisplaySection_ = nullptr;
  QPushButton *relatedDisplayForegroundButton_ = nullptr;
  QPushButton *relatedDisplayBackgroundButton_ = nullptr;
  QLineEdit *relatedDisplayLabelEdit_ = nullptr;
  QComboBox *relatedDisplayVisualCombo_ = nullptr;
  QWidget *relatedDisplayEntriesWidget_ = nullptr;
  std::array<QLineEdit *, kRelatedDisplayEntryCount> relatedDisplayEntryLabelEdits_{};
  std::array<QLineEdit *, kRelatedDisplayEntryCount> relatedDisplayEntryNameEdits_{};
  std::array<QLineEdit *, kRelatedDisplayEntryCount> relatedDisplayEntryArgsEdits_{};
  std::array<QComboBox *, kRelatedDisplayEntryCount> relatedDisplayEntryModeCombos_{};
  QWidget *meterSection_ = nullptr;
  QPushButton *meterForegroundButton_ = nullptr;
  QPushButton *meterBackgroundButton_ = nullptr;
  QComboBox *meterLabelCombo_ = nullptr;
  QComboBox *meterColorModeCombo_ = nullptr;
  QLineEdit *meterChannelEdit_ = nullptr;
  QPushButton *meterPvLimitsButton_ = nullptr;
  QWidget *barSection_ = nullptr;
  QPushButton *barForegroundButton_ = nullptr;
  QPushButton *barBackgroundButton_ = nullptr;
  QComboBox *barLabelCombo_ = nullptr;
  QComboBox *barColorModeCombo_ = nullptr;
  QComboBox *barDirectionCombo_ = nullptr;
  QComboBox *barFillCombo_ = nullptr;
  QLineEdit *barChannelEdit_ = nullptr;
  QPushButton *barPvLimitsButton_ = nullptr;
  QWidget *scaleSection_ = nullptr;
  QPushButton *scaleForegroundButton_ = nullptr;
  QPushButton *scaleBackgroundButton_ = nullptr;
  QComboBox *scaleLabelCombo_ = nullptr;
  QComboBox *scaleColorModeCombo_ = nullptr;
  QComboBox *scaleDirectionCombo_ = nullptr;
  QLineEdit *scaleChannelEdit_ = nullptr;
  QPushButton *scalePvLimitsButton_ = nullptr;
  QWidget *stripChartSection_ = nullptr;
  QLineEdit *stripTitleEdit_ = nullptr;
  QLineEdit *stripXLabelEdit_ = nullptr;
  QLineEdit *stripYLabelEdit_ = nullptr;
  QPushButton *stripForegroundButton_ = nullptr;
  QPushButton *stripBackgroundButton_ = nullptr;
  QLineEdit *stripPeriodEdit_ = nullptr;
  QComboBox *stripUnitsCombo_ = nullptr;
  std::array<QPushButton *, kStripChartPenCount> stripPenColorButtons_{};
  std::array<QLineEdit *, kStripChartPenCount> stripPenChannelEdits_{};
  std::array<QPushButton *, kStripChartPenCount> stripPenLimitsButtons_{};
  QWidget *cartesianSection_ = nullptr;
  QLineEdit *cartesianTitleEdit_ = nullptr;
  QLineEdit *cartesianXLabelEdit_ = nullptr;
  std::array<QLineEdit *, 4> cartesianYLabelEdits_{};
  QPushButton *cartesianForegroundButton_ = nullptr;
  QPushButton *cartesianBackgroundButton_ = nullptr;
  QComboBox *cartesianStyleCombo_ = nullptr;
  QComboBox *cartesianEraseOldestCombo_ = nullptr;
  QLineEdit *cartesianCountEdit_ = nullptr;
  QComboBox *cartesianEraseModeCombo_ = nullptr;
  QLineEdit *cartesianTriggerEdit_ = nullptr;
  QLineEdit *cartesianEraseEdit_ = nullptr;
  QLineEdit *cartesianCountPvEdit_ = nullptr;
  QPushButton *cartesianAxisButton_ = nullptr;
  std::array<QPushButton *, kCartesianPlotTraceCount> cartesianTraceColorButtons_{};
  std::array<QLineEdit *, kCartesianPlotTraceCount> cartesianTraceXEdits_{};
  std::array<QLineEdit *, kCartesianPlotTraceCount> cartesianTraceYEdits_{};
  std::array<QComboBox *, kCartesianPlotTraceCount> cartesianTraceAxisCombos_{};
  std::array<QComboBox *, kCartesianPlotTraceCount> cartesianTraceSideCombos_{};
  QWidget *byteSection_ = nullptr;
  QPushButton *byteForegroundButton_ = nullptr;
  QPushButton *byteBackgroundButton_ = nullptr;
  QComboBox *byteColorModeCombo_ = nullptr;
  QComboBox *byteDirectionCombo_ = nullptr;
  QSpinBox *byteStartBitSpin_ = nullptr;
  QSpinBox *byteEndBitSpin_ = nullptr;
  QLineEdit *byteChannelEdit_ = nullptr;
  QPushButton *rectangleForegroundButton_ = nullptr;
  QComboBox *rectangleFillCombo_ = nullptr;
  QComboBox *rectangleLineStyleCombo_ = nullptr;
  QLineEdit *rectangleLineWidthEdit_ = nullptr;
  QComboBox *rectangleColorModeCombo_ = nullptr;
  QComboBox *rectangleVisibilityCombo_ = nullptr;
  QLineEdit *rectangleVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> rectangleChannelEdits_{};
  QPushButton *compositeForegroundButton_ = nullptr;
  QPushButton *compositeBackgroundButton_ = nullptr;
  QLineEdit *compositeFileEdit_ = nullptr;
  QComboBox *compositeVisibilityCombo_ = nullptr;
  QLineEdit *compositeVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> compositeChannelEdits_{};
  QComboBox *imageTypeCombo_ = nullptr;
  QLineEdit *imageNameEdit_ = nullptr;
  QLineEdit *imageCalcEdit_ = nullptr;
  QComboBox *imageColorModeCombo_ = nullptr;
  QComboBox *imageVisibilityCombo_ = nullptr;
  QLineEdit *imageVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> imageChannelEdits_{};
  QLabel *arcBeginLabel_ = nullptr;
  QLabel *arcPathLabel_ = nullptr;
  QSpinBox *arcBeginSpin_ = nullptr;
  QSpinBox *arcPathSpin_ = nullptr;
  QPushButton *lineColorButton_ = nullptr;
  QComboBox *lineLineStyleCombo_ = nullptr;
  QLineEdit *lineLineWidthEdit_ = nullptr;
  QComboBox *lineColorModeCombo_ = nullptr;
  QComboBox *lineVisibilityCombo_ = nullptr;
  QLineEdit *lineVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> lineChannelEdits_{};
  QPushButton *foregroundButton_ = nullptr;
  QPushButton *backgroundButton_ = nullptr;
  QComboBox *gridOnCombo_ = nullptr;
  QComboBox *snapToGridCombo_ = nullptr;
  QLabel *elementLabel_ = nullptr;
  QScrollArea *scrollArea_ = nullptr;
  QWidget *entriesWidget_ = nullptr;
  SelectionKind selectionKind_ = SelectionKind::kNone;
  bool rectangleIsArc_ = false;
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
    if (textStringEdit_) {
      committedTexts_[textStringEdit_] = textStringEdit_->text();
    }
    if (textVisibilityCalcEdit_) {
      committedTexts_[textVisibilityCalcEdit_] = textVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : textChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (textEntryChannelEdit_) {
      committedTexts_[textEntryChannelEdit_] = textEntryChannelEdit_->text();
    }
    if (sliderIncrementEdit_) {
      committedTexts_[sliderIncrementEdit_] = sliderIncrementEdit_->text();
    }
    if (sliderChannelEdit_) {
      committedTexts_[sliderChannelEdit_] = sliderChannelEdit_->text();
    }
    if (wheelSwitchPrecisionEdit_) {
      committedTexts_[wheelSwitchPrecisionEdit_] = wheelSwitchPrecisionEdit_->text();
    }
    if (wheelSwitchFormatEdit_) {
      committedTexts_[wheelSwitchFormatEdit_] = wheelSwitchFormatEdit_->text();
    }
    if (wheelSwitchChannelEdit_) {
      committedTexts_[wheelSwitchChannelEdit_] = wheelSwitchChannelEdit_->text();
    }
    if (wheelSwitchPrecisionEdit_) {
      const QSignalBlocker blocker(wheelSwitchPrecisionEdit_);
      wheelSwitchPrecisionEdit_->clear();
      committedTexts_[wheelSwitchPrecisionEdit_] = wheelSwitchPrecisionEdit_->text();
    }
    if (wheelSwitchFormatEdit_) {
      const QSignalBlocker blocker(wheelSwitchFormatEdit_);
      wheelSwitchFormatEdit_->clear();
      committedTexts_[wheelSwitchFormatEdit_] = wheelSwitchFormatEdit_->text();
    }
    if (wheelSwitchChannelEdit_) {
      const QSignalBlocker blocker(wheelSwitchChannelEdit_);
      wheelSwitchChannelEdit_->clear();
      committedTexts_[wheelSwitchChannelEdit_] = wheelSwitchChannelEdit_->text();
    }
    if (wheelSwitchColorModeCombo_) {
      const QSignalBlocker blocker(wheelSwitchColorModeCombo_);
      wheelSwitchColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (wheelSwitchPvLimitsButton_) {
      wheelSwitchPvLimitsButton_->setEnabled(false);
    }
    if (choiceButtonChannelEdit_) {
      committedTexts_[choiceButtonChannelEdit_] = choiceButtonChannelEdit_->text();
    }
    if (menuChannelEdit_) {
      committedTexts_[menuChannelEdit_] = menuChannelEdit_->text();
    }
    if (messageButtonLabelEdit_) {
      committedTexts_[messageButtonLabelEdit_] = messageButtonLabelEdit_->text();
    }
    if (messageButtonPressEdit_) {
      committedTexts_[messageButtonPressEdit_] = messageButtonPressEdit_->text();
    }
    if (messageButtonReleaseEdit_) {
      committedTexts_[messageButtonReleaseEdit_] = messageButtonReleaseEdit_->text();
    }
    if (messageButtonChannelEdit_) {
      committedTexts_[messageButtonChannelEdit_] = messageButtonChannelEdit_->text();
    }
    if (textMonitorChannelEdit_) {
      committedTexts_[textMonitorChannelEdit_] = textMonitorChannelEdit_->text();
    }
    if (meterChannelEdit_) {
      committedTexts_[meterChannelEdit_] = meterChannelEdit_->text();
    }
    if (stripTitleEdit_) {
      committedTexts_[stripTitleEdit_] = stripTitleEdit_->text();
    }
    if (stripXLabelEdit_) {
      committedTexts_[stripXLabelEdit_] = stripXLabelEdit_->text();
    }
    if (stripYLabelEdit_) {
      committedTexts_[stripYLabelEdit_] = stripYLabelEdit_->text();
    }
    if (stripPeriodEdit_) {
      committedTexts_[stripPeriodEdit_] = stripPeriodEdit_->text();
    }
    for (QLineEdit *edit : stripPenChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (cartesianTitleEdit_) {
      committedTexts_[cartesianTitleEdit_] = cartesianTitleEdit_->text();
    }
    if (cartesianXLabelEdit_) {
      committedTexts_[cartesianXLabelEdit_] = cartesianXLabelEdit_->text();
    }
    for (QLineEdit *edit : cartesianYLabelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (cartesianCountEdit_) {
      committedTexts_[cartesianCountEdit_] = cartesianCountEdit_->text();
    }
    if (cartesianTriggerEdit_) {
      committedTexts_[cartesianTriggerEdit_] = cartesianTriggerEdit_->text();
    }
    if (cartesianEraseEdit_) {
      committedTexts_[cartesianEraseEdit_] = cartesianEraseEdit_->text();
    }
    if (cartesianCountPvEdit_) {
      committedTexts_[cartesianCountPvEdit_] = cartesianCountPvEdit_->text();
    }
    for (QLineEdit *edit : cartesianTraceXEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    for (QLineEdit *edit : cartesianTraceYEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (barChannelEdit_) {
      committedTexts_[barChannelEdit_] = barChannelEdit_->text();
    }
    if (byteChannelEdit_) {
      committedTexts_[byteChannelEdit_] = byteChannelEdit_->text();
    }
    if (rectangleLineWidthEdit_) {
      committedTexts_[rectangleLineWidthEdit_] = rectangleLineWidthEdit_->text();
    }
    if (rectangleVisibilityCalcEdit_) {
      committedTexts_[rectangleVisibilityCalcEdit_] = rectangleVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : rectangleChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (compositeFileEdit_) {
      committedTexts_[compositeFileEdit_] = compositeFileEdit_->text();
    }
    if (compositeVisibilityCalcEdit_) {
      committedTexts_[compositeVisibilityCalcEdit_] = compositeVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : compositeChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (imageNameEdit_) {
      committedTexts_[imageNameEdit_] = imageNameEdit_->text();
    }
    if (imageCalcEdit_) {
      committedTexts_[imageCalcEdit_] = imageCalcEdit_->text();
    }
    if (imageVisibilityCalcEdit_) {
      committedTexts_[imageVisibilityCalcEdit_] = imageVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : imageChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (lineLineWidthEdit_) {
      committedTexts_[lineLineWidthEdit_] = lineLineWidthEdit_->text();
    }
    if (lineVisibilityCalcEdit_) {
      committedTexts_[lineVisibilityCalcEdit_] = lineVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : lineChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
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
  QHash<QWidget *, QLabel *> fieldLabels_;
  ColorPaletteDialog *colorPaletteDialog_ = nullptr;
  PvLimitsDialog *pvLimitsDialog_ = nullptr;
  CartesianAxisDialog *cartesianAxisDialog_ = nullptr;
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
  std::function<bool()> snapToGridGetter_;
  std::function<void(bool)> snapToGridSetter_;
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
  std::array<std::function<QString()>, 5> textChannelGetters_{};
  std::array<std::function<void(const QString &)>, 5> textChannelSetters_{};
  std::function<QColor()> textMonitorForegroundGetter_;
  std::function<void(const QColor &)> textMonitorForegroundSetter_;
  std::function<QColor()> textMonitorBackgroundGetter_;
  std::function<void(const QColor &)> textMonitorBackgroundSetter_;
  std::function<Qt::Alignment()> textMonitorAlignmentGetter_;
  std::function<void(Qt::Alignment)> textMonitorAlignmentSetter_;
  std::function<TextMonitorFormat()> textMonitorFormatGetter_;
  std::function<void(TextMonitorFormat)> textMonitorFormatSetter_;
  std::function<int()> textMonitorPrecisionGetter_;
  std::function<void(int)> textMonitorPrecisionSetter_;
  std::function<PvLimitSource()> textMonitorPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> textMonitorPrecisionSourceSetter_;
  std::function<int()> textMonitorPrecisionDefaultGetter_;
  std::function<void(int)> textMonitorPrecisionDefaultSetter_;
  std::function<TextColorMode()> textMonitorColorModeGetter_;
  std::function<void(TextColorMode)> textMonitorColorModeSetter_;
  std::function<QString()> textMonitorChannelGetter_;
  std::function<void(const QString &)> textMonitorChannelSetter_;
  std::function<PvLimits()> textMonitorLimitsGetter_;
  std::function<void(const PvLimits &)> textMonitorLimitsSetter_;
  std::function<QColor()> textEntryForegroundGetter_;
  std::function<void(const QColor &)> textEntryForegroundSetter_;
  std::function<QColor()> textEntryBackgroundGetter_;
  std::function<void(const QColor &)> textEntryBackgroundSetter_;
  std::function<TextMonitorFormat()> textEntryFormatGetter_;
  std::function<void(TextMonitorFormat)> textEntryFormatSetter_;
  std::function<int()> textEntryPrecisionGetter_;
  std::function<void(int)> textEntryPrecisionSetter_;
  std::function<PvLimitSource()> textEntryPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> textEntryPrecisionSourceSetter_;
  std::function<int()> textEntryPrecisionDefaultGetter_;
  std::function<void(int)> textEntryPrecisionDefaultSetter_;
  std::function<TextColorMode()> textEntryColorModeGetter_;
  std::function<void(TextColorMode)> textEntryColorModeSetter_;
  std::function<QString()> textEntryChannelGetter_;
  std::function<void(const QString &)> textEntryChannelSetter_;
  std::function<PvLimits()> textEntryLimitsGetter_;
  std::function<void(const PvLimits &)> textEntryLimitsSetter_;
  std::function<QColor()> sliderForegroundGetter_;
  std::function<void(const QColor &)> sliderForegroundSetter_;
  std::function<QColor()> sliderBackgroundGetter_;
  std::function<void(const QColor &)> sliderBackgroundSetter_;
  std::function<MeterLabel()> sliderLabelGetter_;
  std::function<void(MeterLabel)> sliderLabelSetter_;
  std::function<TextColorMode()> sliderColorModeGetter_;
  std::function<void(TextColorMode)> sliderColorModeSetter_;
  std::function<BarDirection()> sliderDirectionGetter_;
  std::function<void(BarDirection)> sliderDirectionSetter_;
  std::function<double()> sliderIncrementGetter_;
  std::function<void(double)> sliderIncrementSetter_;
  std::function<QString()> sliderChannelGetter_;
  std::function<void(const QString &)> sliderChannelSetter_;
  std::function<PvLimits()> sliderLimitsGetter_;
  std::function<void(const PvLimits &)> sliderLimitsSetter_;
  std::function<QColor()> wheelSwitchForegroundGetter_;
  std::function<void(const QColor &)> wheelSwitchForegroundSetter_;
  std::function<QColor()> wheelSwitchBackgroundGetter_;
  std::function<void(const QColor &)> wheelSwitchBackgroundSetter_;
  std::function<TextColorMode()> wheelSwitchColorModeGetter_;
  std::function<void(TextColorMode)> wheelSwitchColorModeSetter_;
  std::function<double()> wheelSwitchPrecisionGetter_;
  std::function<void(double)> wheelSwitchPrecisionSetter_;
  std::function<QString()> wheelSwitchFormatGetter_;
  std::function<void(const QString &)> wheelSwitchFormatSetter_;
  std::function<QString()> wheelSwitchChannelGetter_;
  std::function<void(const QString &)> wheelSwitchChannelSetter_;
  std::function<PvLimits()> wheelSwitchLimitsGetter_;
  std::function<void(const PvLimits &)> wheelSwitchLimitsSetter_;
  std::function<QColor()> choiceButtonForegroundGetter_;
  std::function<void(const QColor &)> choiceButtonForegroundSetter_;
  std::function<QColor()> choiceButtonBackgroundGetter_;
  std::function<void(const QColor &)> choiceButtonBackgroundSetter_;
  std::function<TextColorMode()> choiceButtonColorModeGetter_;
  std::function<void(TextColorMode)> choiceButtonColorModeSetter_;
  std::function<ChoiceButtonStacking()> choiceButtonStackingGetter_;
  std::function<void(ChoiceButtonStacking)> choiceButtonStackingSetter_;
  std::function<QString()> choiceButtonChannelGetter_;
  std::function<void(const QString &)> choiceButtonChannelSetter_;
  std::function<QColor()> menuForegroundGetter_;
  std::function<void(const QColor &)> menuForegroundSetter_;
  std::function<QColor()> menuBackgroundGetter_;
  std::function<void(const QColor &)> menuBackgroundSetter_;
  std::function<TextColorMode()> menuColorModeGetter_;
  std::function<void(TextColorMode)> menuColorModeSetter_;
  std::function<QString()> menuChannelGetter_;
  std::function<void(const QString &)> menuChannelSetter_;
  std::function<QColor()> messageButtonForegroundGetter_;
  std::function<void(const QColor &)> messageButtonForegroundSetter_;
  std::function<QColor()> messageButtonBackgroundGetter_;
  std::function<void(const QColor &)> messageButtonBackgroundSetter_;
  std::function<TextColorMode()> messageButtonColorModeGetter_;
  std::function<void(TextColorMode)> messageButtonColorModeSetter_;
  std::function<QString()> messageButtonLabelGetter_;
  std::function<void(const QString &)> messageButtonLabelSetter_;
  std::function<QString()> messageButtonPressGetter_;
  std::function<void(const QString &)> messageButtonPressSetter_;
  std::function<QString()> messageButtonReleaseGetter_;
  std::function<void(const QString &)> messageButtonReleaseSetter_;
  std::function<QString()> messageButtonChannelGetter_;
  std::function<void(const QString &)> messageButtonChannelSetter_;
  std::function<QColor()> shellCommandForegroundGetter_;
  std::function<void(const QColor &)> shellCommandForegroundSetter_;
  std::function<QColor()> shellCommandBackgroundGetter_;
  std::function<void(const QColor &)> shellCommandBackgroundSetter_;
  std::function<QString()> shellCommandLabelGetter_;
  std::function<void(const QString &)> shellCommandLabelSetter_;
  std::array<std::function<QString()>, kShellCommandEntryCount> shellCommandEntryLabelGetters_{};
  std::array<std::function<void(const QString &)>, kShellCommandEntryCount> shellCommandEntryLabelSetters_{};
  std::array<std::function<QString()>, kShellCommandEntryCount> shellCommandEntryCommandGetters_{};
  std::array<std::function<void(const QString &)>, kShellCommandEntryCount> shellCommandEntryCommandSetters_{};
  std::array<std::function<QString()>, kShellCommandEntryCount> shellCommandEntryArgsGetters_{};
  std::array<std::function<void(const QString &)>, kShellCommandEntryCount> shellCommandEntryArgsSetters_{};
  std::function<QColor()> relatedDisplayForegroundGetter_;
  std::function<void(const QColor &)> relatedDisplayForegroundSetter_;
  std::function<QColor()> relatedDisplayBackgroundGetter_;
  std::function<void(const QColor &)> relatedDisplayBackgroundSetter_;
  std::function<QString()> relatedDisplayLabelGetter_;
  std::function<void(const QString &)> relatedDisplayLabelSetter_;
  std::function<RelatedDisplayVisual()> relatedDisplayVisualGetter_;
  std::function<void(RelatedDisplayVisual)> relatedDisplayVisualSetter_;
  std::array<std::function<QString()>, kRelatedDisplayEntryCount> relatedDisplayEntryLabelGetters_{};
  std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> relatedDisplayEntryLabelSetters_{};
  std::array<std::function<QString()>, kRelatedDisplayEntryCount> relatedDisplayEntryNameGetters_{};
  std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> relatedDisplayEntryNameSetters_{};
  std::array<std::function<QString()>, kRelatedDisplayEntryCount> relatedDisplayEntryArgsGetters_{};
  std::array<std::function<void(const QString &)>, kRelatedDisplayEntryCount> relatedDisplayEntryArgsSetters_{};
  std::array<std::function<RelatedDisplayMode()>, kRelatedDisplayEntryCount> relatedDisplayEntryModeGetters_{};
  std::array<std::function<void(RelatedDisplayMode)>, kRelatedDisplayEntryCount> relatedDisplayEntryModeSetters_{};
  std::function<QColor()> meterForegroundGetter_;
  std::function<void(const QColor &)> meterForegroundSetter_;
  std::function<QColor()> meterBackgroundGetter_;
  std::function<void(const QColor &)> meterBackgroundSetter_;
  std::function<MeterLabel()> meterLabelGetter_;
  std::function<void(MeterLabel)> meterLabelSetter_;
  std::function<TextColorMode()> meterColorModeGetter_;
  std::function<void(TextColorMode)> meterColorModeSetter_;
  std::function<QString()> meterChannelGetter_;
  std::function<void(const QString &)> meterChannelSetter_;
  std::function<PvLimits()> meterLimitsGetter_;
  std::function<void(const PvLimits &)> meterLimitsSetter_;
  std::function<QColor()> barForegroundGetter_;
  std::function<void(const QColor &)> barForegroundSetter_;
  std::function<QColor()> barBackgroundGetter_;
  std::function<void(const QColor &)> barBackgroundSetter_;
  std::function<MeterLabel()> barLabelGetter_;
  std::function<void(MeterLabel)> barLabelSetter_;
  std::function<TextColorMode()> barColorModeGetter_;
  std::function<void(TextColorMode)> barColorModeSetter_;
  std::function<BarDirection()> barDirectionGetter_;
  std::function<void(BarDirection)> barDirectionSetter_;
  std::function<BarFill()> barFillModeGetter_;
  std::function<void(BarFill)> barFillModeSetter_;
  std::function<QString()> barChannelGetter_;
  std::function<void(const QString &)> barChannelSetter_;
  std::function<PvLimits()> barLimitsGetter_;
  std::function<void(const PvLimits &)> barLimitsSetter_;
  std::function<QColor()> scaleForegroundGetter_;
  std::function<void(const QColor &)> scaleForegroundSetter_;
  std::function<QColor()> scaleBackgroundGetter_;
  std::function<void(const QColor &)> scaleBackgroundSetter_;
  std::function<MeterLabel()> scaleLabelGetter_;
  std::function<void(MeterLabel)> scaleLabelSetter_;
  std::function<TextColorMode()> scaleColorModeGetter_;
  std::function<void(TextColorMode)> scaleColorModeSetter_;
  std::function<BarDirection()> scaleDirectionGetter_;
  std::function<void(BarDirection)> scaleDirectionSetter_;
  std::function<QString()> scaleChannelGetter_;
  std::function<void(const QString &)> scaleChannelSetter_;
  std::function<PvLimits()> scaleLimitsGetter_;
  std::function<void(const PvLimits &)> scaleLimitsSetter_;
  std::function<QString()> stripTitleGetter_;
  std::function<void(const QString &)> stripTitleSetter_;
  std::function<QString()> stripXLabelGetter_;
  std::function<void(const QString &)> stripXLabelSetter_;
  std::function<QString()> stripYLabelGetter_;
  std::function<void(const QString &)> stripYLabelSetter_;
  std::function<QColor()> stripForegroundGetter_;
  std::function<void(const QColor &)> stripForegroundSetter_;
  std::function<QColor()> stripBackgroundGetter_;
  std::function<void(const QColor &)> stripBackgroundSetter_;
  std::function<double()> stripPeriodGetter_;
  std::function<void(double)> stripPeriodSetter_;
  std::function<TimeUnits()> stripUnitsGetter_;
  std::function<void(TimeUnits)> stripUnitsSetter_;
  std::array<std::function<QString()>, kStripChartPenCount> stripPenChannelGetters_{};
  std::array<std::function<void(const QString &)>, kStripChartPenCount> stripPenChannelSetters_{};
  std::array<std::function<QColor()>, kStripChartPenCount> stripPenColorGetters_{};
  std::array<std::function<void(const QColor &)>, kStripChartPenCount> stripPenColorSetters_{};
  std::array<std::function<PvLimits()>, kStripChartPenCount> stripPenLimitsGetters_{};
  std::array<std::function<void(const PvLimits &)>, kStripChartPenCount> stripPenLimitsSetters_{};
  std::function<QString()> cartesianTitleGetter_;
  std::function<void(const QString &)> cartesianTitleSetter_;
  std::function<QString()> cartesianXLabelGetter_;
  std::function<void(const QString &)> cartesianXLabelSetter_;
  std::array<std::function<QString()>, 4> cartesianYLabelGetters_{};
  std::array<std::function<void(const QString &)>, 4> cartesianYLabelSetters_{};
  std::function<QColor()> cartesianForegroundGetter_;
  std::function<void(const QColor &)> cartesianForegroundSetter_;
  std::function<QColor()> cartesianBackgroundGetter_;
  std::function<void(const QColor &)> cartesianBackgroundSetter_;
  std::function<CartesianPlotStyle()> cartesianStyleGetter_;
  std::function<void(CartesianPlotStyle)> cartesianStyleSetter_;
  std::function<bool()> cartesianEraseOldestGetter_;
  std::function<void(bool)> cartesianEraseOldestSetter_;
  std::function<int()> cartesianCountGetter_;
  std::function<void(int)> cartesianCountSetter_;
  std::function<CartesianPlotEraseMode()> cartesianEraseModeGetter_;
  std::function<void(CartesianPlotEraseMode)> cartesianEraseModeSetter_;
  std::function<QString()> cartesianTriggerGetter_;
  std::function<void(const QString &)> cartesianTriggerSetter_;
  std::function<QString()> cartesianEraseGetter_;
  std::function<void(const QString &)> cartesianEraseSetter_;
  std::function<QString()> cartesianCountPvGetter_;
  std::function<void(const QString &)> cartesianCountPvSetter_;
  std::array<std::function<QString()>, kCartesianPlotTraceCount> cartesianTraceXGetters_{};
  std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> cartesianTraceXSetters_{};
  std::array<std::function<QString()>, kCartesianPlotTraceCount> cartesianTraceYGetters_{};
  std::array<std::function<void(const QString &)>, kCartesianPlotTraceCount> cartesianTraceYSetters_{};
  std::array<std::function<QColor()>, kCartesianPlotTraceCount> cartesianTraceColorGetters_{};
  std::array<std::function<void(const QColor &)>, kCartesianPlotTraceCount> cartesianTraceColorSetters_{};
  std::array<std::function<CartesianPlotYAxis()>, kCartesianPlotTraceCount> cartesianTraceAxisGetters_{};
  std::array<std::function<void(CartesianPlotYAxis)>, kCartesianPlotTraceCount> cartesianTraceAxisSetters_{};
  std::array<std::function<bool()>, kCartesianPlotTraceCount> cartesianTraceSideGetters_{};
  std::array<std::function<void(bool)>, kCartesianPlotTraceCount> cartesianTraceSideSetters_{};
  std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> cartesianAxisStyleGetters_{};
  std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> cartesianAxisStyleSetters_{};
  std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> cartesianAxisRangeGetters_{};
  std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> cartesianAxisRangeSetters_{};
  std::array<std::function<double()>, kCartesianAxisCount> cartesianAxisMinimumGetters_{};
  std::array<std::function<void(double)>, kCartesianAxisCount> cartesianAxisMinimumSetters_{};
  std::array<std::function<double()>, kCartesianAxisCount> cartesianAxisMaximumGetters_{};
  std::array<std::function<void(double)>, kCartesianAxisCount> cartesianAxisMaximumSetters_{};
  std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> cartesianAxisTimeFormatGetters_{};
  std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> cartesianAxisTimeFormatSetters_{};
  std::function<QColor()> byteForegroundGetter_;
  std::function<void(const QColor &)> byteForegroundSetter_;
  std::function<QColor()> byteBackgroundGetter_;
  std::function<void(const QColor &)> byteBackgroundSetter_;
  std::function<TextColorMode()> byteColorModeGetter_;
  std::function<void(TextColorMode)> byteColorModeSetter_;
  std::function<BarDirection()> byteDirectionGetter_;
  std::function<void(BarDirection)> byteDirectionSetter_;
  std::function<int()> byteStartBitGetter_;
  std::function<void(int)> byteStartBitSetter_;
  std::function<int()> byteEndBitGetter_;
  std::function<void(int)> byteEndBitSetter_;
  std::function<QString()> byteChannelGetter_;
  std::function<void(const QString &)> byteChannelSetter_;
  QString committedTextString_;
  std::function<QColor()> rectangleForegroundGetter_;
  std::function<void(const QColor &)> rectangleForegroundSetter_;
  std::function<RectangleFill()> rectangleFillGetter_;
  std::function<void(RectangleFill)> rectangleFillSetter_;
  std::function<RectangleLineStyle()> rectangleLineStyleGetter_;
  std::function<void(RectangleLineStyle)> rectangleLineStyleSetter_;
  std::function<int()> rectangleLineWidthGetter_;
  std::function<void(int)> rectangleLineWidthSetter_;
  std::function<int()> arcBeginGetter_;
  std::function<void(int)> arcBeginSetter_;
  std::function<int()> arcPathGetter_;
  std::function<void(int)> arcPathSetter_;
  std::function<TextColorMode()> rectangleColorModeGetter_;
  std::function<void(TextColorMode)> rectangleColorModeSetter_;
  std::function<TextVisibilityMode()> rectangleVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> rectangleVisibilityModeSetter_;
  std::function<QString()> rectangleVisibilityCalcGetter_;
  std::function<void(const QString &)> rectangleVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> rectangleChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> rectangleChannelSetters_{};
  std::function<QColor()> compositeForegroundGetter_;
  std::function<void(const QColor &)> compositeForegroundSetter_;
  std::function<QColor()> compositeBackgroundGetter_;
  std::function<void(const QColor &)> compositeBackgroundSetter_;
  std::function<QString()> compositeFileGetter_;
  std::function<void(const QString &)> compositeFileSetter_;
  std::function<TextVisibilityMode()> compositeVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> compositeVisibilityModeSetter_;
  std::function<QString()> compositeVisibilityCalcGetter_;
  std::function<void(const QString &)> compositeVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> compositeChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> compositeChannelSetters_{};
  std::function<ImageType()> imageTypeGetter_;
  std::function<void(ImageType)> imageTypeSetter_;
  std::function<QString()> imageNameGetter_;
  std::function<void(const QString &)> imageNameSetter_;
  std::function<QString()> imageCalcGetter_;
  std::function<void(const QString &)> imageCalcSetter_;
  std::function<TextColorMode()> imageColorModeGetter_;
  std::function<void(TextColorMode)> imageColorModeSetter_;
  std::function<TextVisibilityMode()> imageVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> imageVisibilityModeSetter_;
  std::function<QString()> imageVisibilityCalcGetter_;
  std::function<void(const QString &)> imageVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> imageChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> imageChannelSetters_{};
  std::function<QString()> heatmapTitleGetter_;
  std::function<void(const QString &)> heatmapTitleSetter_;
  std::function<QString()> heatmapDataChannelGetter_;
  std::function<void(const QString &)> heatmapDataChannelSetter_;
  std::function<HeatmapDimensionSource()> heatmapXSourceGetter_;
  std::function<void(HeatmapDimensionSource)> heatmapXSourceSetter_;
  std::function<HeatmapDimensionSource()> heatmapYSourceGetter_;
  std::function<void(HeatmapDimensionSource)> heatmapYSourceSetter_;
  std::function<int()> heatmapXDimensionGetter_;
  std::function<void(int)> heatmapXDimensionSetter_;
  std::function<int()> heatmapYDimensionGetter_;
  std::function<void(int)> heatmapYDimensionSetter_;
  std::function<QString()> heatmapXDimChannelGetter_;
  std::function<void(const QString &)> heatmapXDimChannelSetter_;
  std::function<QString()> heatmapYDimChannelGetter_;
  std::function<void(const QString &)> heatmapYDimChannelSetter_;
  std::function<HeatmapOrder()> heatmapOrderGetter_;
  std::function<void(HeatmapOrder)> heatmapOrderSetter_;
  std::function<QColor()> lineColorGetter_;
  std::function<void(const QColor &)> lineColorSetter_;
  std::function<RectangleLineStyle()> lineLineStyleGetter_;
  std::function<void(RectangleLineStyle)> lineLineStyleSetter_;
  std::function<int()> lineLineWidthGetter_;
  std::function<void(int)> lineLineWidthSetter_;
  std::function<TextColorMode()> lineColorModeGetter_;
  std::function<void(TextColorMode)> lineColorModeSetter_;
  std::function<TextVisibilityMode()> lineVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> lineVisibilityModeSetter_;
  std::function<QString()> lineVisibilityCalcGetter_;
  std::function<void(const QString &)> lineVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> lineChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> lineChannelSetters_{};

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
    const bool dialogWasVisible = colorPaletteDialog_->isVisible();
    if (!dialogWasVisible) {
      positionColorPaletteRelativeToResource();
    }
    colorPaletteDialog_->show();
    if (!dialogWasVisible) {
      scheduleColorPalettePlacement();
    }
    colorPaletteDialog_->raise();
    colorPaletteDialog_->activateWindow();
  }

  bool positionColorPaletteRelativeToResource()
  {
    if (!colorPaletteDialog_) {
      return false;
    }

    QWidget *reference = this;
    if (!reference) {
      return false;
    }

    colorPaletteDialog_->ensurePolished();
    if (QLayout *dialogLayout = colorPaletteDialog_->layout()) {
      dialogLayout->activate();
    }
    colorPaletteDialog_->adjustSize();

    QSize paletteSize = colorPaletteDialog_->isVisible()
        ? colorPaletteDialog_->size()
        : colorPaletteDialog_->sizeHint().expandedTo(
            colorPaletteDialog_->minimumSize());
    if (paletteSize.isEmpty()) {
      paletteSize = colorPaletteDialog_->size();
    }
    if (paletteSize.isEmpty()) {
      return false;
    }

    QScreen *screen = screenForWidget(reference);
    if (!screen) {
      screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen ? screen->availableGeometry() : QRect();

    const QRect referenceFrame = reference->frameGeometry();
    if (!referenceFrame.isValid()) {
      return false;
    }

    return placeColorPaletteRelativeToReference(colorPaletteDialog_, paletteSize,
        referenceFrame, available);
  }

  void scheduleColorPalettePlacement()
  {
    if (!colorPaletteDialog_) {
      return;
    }

    QPointer<ColorPaletteDialog> palette(colorPaletteDialog_);
    QMetaObject::invokeMethod(this, [this, palette]() {
      ColorPaletteDialog *dialog = palette.data();
      if (!dialog) {
        return;
      }
      if (!dialog->isVisible()) {
        return;
      }

      QScreen *screen = screenForWidget(this);
      if (!screen) {
        screen = QGuiApplication::primaryScreen();
      }
      const QRect available = screen ? screen->availableGeometry() : QRect();

      const QRect referenceFrame = frameGeometry();
      if (!referenceFrame.isValid()) {
        return;
      }

      QSize paletteSize = dialog->size();
      if (paletteSize.isEmpty()) {
        paletteSize = dialog->sizeHint().expandedTo(dialog->minimumSize());
      }
      if (paletteSize.isEmpty()) {
        return;
      }

      placeColorPaletteRelativeToReference(dialog, paletteSize,
          referenceFrame, available);
    }, Qt::QueuedConnection);
  }

  bool placeColorPaletteRelativeToReference(ColorPaletteDialog *palette,
      const QSize &paletteSize, const QRect &referenceFrame,
      const QRect &available)
  {
    if (!palette || paletteSize.isEmpty() || !referenceFrame.isValid()) {
      return false;
    }

    const auto clampHorizontal = [&](int desiredX) {
      if (available.isNull()) {
        return desiredX;
      }
      int minX = available.left();
      int maxX = available.right() - paletteSize.width() + 1;
      if (maxX < minX) {
        maxX = minX;
      }
      return std::clamp(desiredX, minX, maxX);
    };

    const int targetX = clampHorizontal(referenceFrame.left());
    const int spacing = ResourcePaletteDialog::kPaletteSpacing;

    const int belowY = referenceFrame.bottom() + spacing;
    QRect belowRect(QPoint(targetX, belowY), paletteSize);
    if (available.isNull() || available.contains(belowRect)) {
      palette->move(belowRect.topLeft());
      return true;
    }

    const int aboveY = referenceFrame.top() - spacing - paletteSize.height();
    QRect aboveRect(QPoint(targetX, aboveY), paletteSize);
    if (available.isNull() || available.contains(aboveRect)) {
      palette->move(aboveRect.topLeft());
      return true;
    }

    if (available.isNull()) {
      palette->move(belowRect.topLeft());
      return false;
    }

    int minY = available.top();
    int maxY = available.bottom() - paletteSize.height() + 1;
    if (maxY < minY) {
      maxY = minY;
    }
    const int clampedY = std::clamp(belowY, minY, maxY);
    palette->move(targetX, clampedY);
    return false;
  }

  void openTextEntryPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!textEntryPrecisionSourceGetter_ || !textEntryPrecisionSourceSetter_
        || !textEntryPrecisionDefaultGetter_
        || !textEntryPrecisionDefaultSetter_
        || !textEntryLimitsGetter_ || !textEntryLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = textEntryChannelGetter_ ? textEntryChannelGetter_()
                                                         : QString();
    dialog->setTextMonitorCallbacks(channelLabel, textEntryPrecisionSourceGetter_,
        textEntryPrecisionSourceSetter_, textEntryPrecisionDefaultGetter_,
        textEntryPrecisionDefaultSetter_,
        [this]() { updateTextEntryLimitsFromDialog(); }, textEntryLimitsGetter_,
        textEntryLimitsSetter_);
    positionPvLimitsDialog(dialog);
    dialog->showForTextMonitor();
  }

  void openTextMonitorPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!textMonitorPrecisionSourceGetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = textMonitorChannelGetter_
            ? textMonitorChannelGetter_()
            : QString();
    dialog->setTextMonitorCallbacks(channelLabel,
        textMonitorPrecisionSourceGetter_, textMonitorPrecisionSourceSetter_,
        textMonitorPrecisionDefaultGetter_,
        textMonitorPrecisionDefaultSetter_,
        [this]() { updateTextMonitorLimitsFromDialog(); },
        textMonitorLimitsGetter_, textMonitorLimitsSetter_);
    positionPvLimitsDialog(dialog);
    dialog->showForTextMonitor();
  }

  void openMeterPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!meterLimitsGetter_ || !meterLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = meterChannelGetter_ ? meterChannelGetter_()
                                                     : QString();
    dialog->setMeterCallbacks(channelLabel, meterLimitsGetter_,
        meterLimitsSetter_, [this]() { updateMeterLimitsFromDialog(); });
    positionPvLimitsDialog(dialog);
    dialog->showForMeter();
  }

  void openSliderPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!sliderLimitsGetter_ || !sliderLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = sliderChannelGetter_ ? sliderChannelGetter_()
                                                      : QString();
    dialog->setSliderCallbacks(channelLabel, sliderLimitsGetter_,
        sliderLimitsSetter_, [this]() { updateSliderLimitsFromDialog(); });
    positionPvLimitsDialog(dialog);
    dialog->showForSlider();
  }

  void openWheelSwitchPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!wheelSwitchLimitsGetter_ || !wheelSwitchLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = wheelSwitchChannelGetter_ ? wheelSwitchChannelGetter_()
                                                           : QString();
    dialog->setWheelSwitchCallbacks(channelLabel, wheelSwitchLimitsGetter_,
        wheelSwitchLimitsSetter_, [this]() { updateWheelSwitchLimitsFromDialog(); });
    positionPvLimitsDialog(dialog);
    dialog->showForWheelSwitch();
  }

  void openBarMonitorPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!barLimitsGetter_ || !barLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = barChannelGetter_ ? barChannelGetter_()
                                                   : QString();
    dialog->setBarCallbacks(channelLabel, barLimitsGetter_, barLimitsSetter_,
        [this]() { updateBarLimitsFromDialog(); });
    positionPvLimitsDialog(dialog);
    dialog->showForBarMonitor();
  }

  void openScaleMonitorPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!scaleLimitsGetter_ || !scaleLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = scaleChannelGetter_ ? scaleChannelGetter_()
                                                     : QString();
    dialog->setScaleCallbacks(channelLabel, scaleLimitsGetter_,
        scaleLimitsSetter_, [this]() { updateScaleLimitsFromDialog(); });
    positionPvLimitsDialog(dialog);
    dialog->showForScaleMonitor();
  }

  void openCartesianAxisDialog()
  {
    CartesianAxisDialog *dialog = ensureCartesianAxisDialog();
    if (!dialog) {
      return;
    }
    bool hasSetter = false;
    for (int i = 0; i < kCartesianAxisCount; ++i) {
      if (cartesianAxisStyleSetters_[i]
          || cartesianAxisRangeSetters_[i]
          || cartesianAxisMinimumSetters_[i]
          || cartesianAxisMaximumSetters_[i]
          || cartesianAxisTimeFormatSetters_[i]) {
        hasSetter = true;
        break;
      }
    }
    if (!hasSetter) {
      dialog->clearCallbacks();
      dialog->hide();
      return;
    }
    dialog->setCartesianCallbacks(cartesianAxisStyleGetters_,
        cartesianAxisStyleSetters_, cartesianAxisRangeGetters_,
        cartesianAxisRangeSetters_, cartesianAxisMinimumGetters_,
        cartesianAxisMinimumSetters_, cartesianAxisMaximumGetters_,
        cartesianAxisMaximumSetters_, cartesianAxisTimeFormatGetters_,
        cartesianAxisTimeFormatSetters_, [this]() {
          updateCartesianAxisButtonState();
        });
    positionCartesianAxisDialog(dialog);
    dialog->showDialog();
  }

  void positionCartesianAxisDialog(CartesianAxisDialog *dialog)
  {
    if (!dialog) {
      return;
    }
    dialog->adjustSize();
    const QRect paletteFrame = frameGeometry();

    QRect availableRect;
    if (QScreen *paletteScreen = screen()) {
      availableRect = paletteScreen->availableGeometry();
    } else if (QScreen *screenAtPalette = QGuiApplication::screenAt(paletteFrame.center())) {
      availableRect = screenAtPalette->availableGeometry();
    } else if (QScreen *primaryScreen = QGuiApplication::primaryScreen()) {
      availableRect = primaryScreen->availableGeometry();
    }

    QRect desiredRect(paletteFrame.topRight(), dialog->size());
    desiredRect.translate(12, 0);
    if (!availableRect.isNull() && !availableRect.contains(desiredRect)) {
      desiredRect.moveTopRight(paletteFrame.topLeft());
      desiredRect.translate(-12, 0);
      if (!availableRect.contains(desiredRect)) {
        desiredRect.moveTopLeft(paletteFrame.bottomLeft());
        desiredRect.translate(0, 12);
      }
    }

    if (!availableRect.isNull()) {
      if (!availableRect.contains(desiredRect)) {
        desiredRect.moveTopLeft(availableRect.topLeft());
      }
      desiredRect.setSize(desiredRect.size().boundedTo(availableRect.size()));
    }

    dialog->move(desiredRect.topLeft());
  }

  void positionPvLimitsDialog(PvLimitsDialog *dialog)
  {
    if (!dialog) {
      return;
    }
    dialog->adjustSize();
    const QRect paletteFrame = frameGeometry();

    QRect availableRect;
    if (QScreen *paletteScreen = screen()) {
      availableRect = paletteScreen->availableGeometry();
    } else if (QScreen *screenAtPalette = QGuiApplication::screenAt(paletteFrame.center())) {
      availableRect = screenAtPalette->availableGeometry();
    } else {
      const auto screens = QGuiApplication::screens();
      if (!screens.isEmpty()) {
        availableRect = screens.front()->availableGeometry();
      }
    }
    if (!availableRect.isValid() || availableRect.isNull()) {
      availableRect = QRect(paletteFrame.topLeft(), paletteFrame.size());
    }

    const QSize dialogSize = dialog->size();
    const int dialogWidth = dialogSize.width();
    const int dialogHeight = dialogSize.height();

    const int availableLeft = availableRect.left();
    const int availableTop = availableRect.top();
    const int availableRight = availableRect.right();
    const int availableBottom = availableRect.bottom();

    int maxY = availableBottom - dialogHeight + 1;
    if (maxY < availableTop) {
      maxY = availableTop;
    }
    int targetY = std::clamp(paletteFrame.top(), availableTop, maxY);

    QPoint rightTop(paletteFrame.right() + 1, targetY);
    QRect rightRect(rightTop, dialogSize);
    if (availableRect.contains(rightRect)) {
      dialog->move(rightRect.topLeft());
      return;
    }

    int maxLeft = availableRight - dialogWidth + 1;
    if (maxLeft < availableLeft) {
      maxLeft = availableLeft;
    }
    int leftX = std::clamp(paletteFrame.left() - dialogWidth, availableLeft, maxLeft);
    QPoint leftTop(leftX, targetY);
    QRect leftRect(leftTop, dialogSize);
    if (!availableRect.contains(leftRect)) {
      leftX = std::clamp(leftX, availableLeft, maxLeft);
      leftTop.setX(leftX);
      leftRect.moveTo(leftTop);
    }
    dialog->move(leftRect.topLeft());
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

  PvLimitsDialog *ensurePvLimitsDialog()
  {
    if (!pvLimitsDialog_) {
      pvLimitsDialog_ = new PvLimitsDialog(palette(), labelFont_, valueFont_, this);
    }
    return pvLimitsDialog_;
  }

  CartesianAxisDialog *ensureCartesianAxisDialog()
  {
    if (!cartesianAxisDialog_) {
      cartesianAxisDialog_ = new CartesianAxisDialog(palette(), labelFont_,
          valueFont_, this);
    }
    return cartesianAxisDialog_;
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
