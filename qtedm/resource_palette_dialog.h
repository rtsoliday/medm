#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <functional>

#include <QAbstractScrollArea>
#include <QAction>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
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
#include "medm_colors.h"
#include "pv_limits_dialog.h"

class ResourcePaletteDialog : public QDialog
{
public:
  static constexpr int kPvTableRowCount = 8;
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

    heatmapColorMapCombo_ = new QComboBox;
    heatmapColorMapCombo_->setFont(valueFont_);
    heatmapColorMapCombo_->setAutoFillBackground(true);
    heatmapColorMapCombo_->addItem(QStringLiteral("Grayscale"));
    heatmapColorMapCombo_->addItem(QStringLiteral("Jet"));
    heatmapColorMapCombo_->addItem(QStringLiteral("Hot"));
    heatmapColorMapCombo_->addItem(QStringLiteral("Cool"));
    heatmapColorMapCombo_->addItem(QStringLiteral("Rainbow"));
    heatmapColorMapCombo_->addItem(QStringLiteral("Turbo"));
    QObject::connect(heatmapColorMapCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapColorMapSetter_) {
          heatmapColorMapSetter_(static_cast<HeatmapColorMap>(index));
        }
      });

    heatmapInvertGreyscaleCombo_ = new QComboBox;
    heatmapInvertGreyscaleCombo_->setFont(valueFont_);
    heatmapInvertGreyscaleCombo_->setAutoFillBackground(true);
    heatmapInvertGreyscaleCombo_->addItem(QStringLiteral("False"));
    heatmapInvertGreyscaleCombo_->addItem(QStringLiteral("True"));
    QObject::connect(heatmapInvertGreyscaleCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapInvertGreyscaleSetter_) {
          heatmapInvertGreyscaleSetter_(index == 1);
        }
      });

    heatmapPreserveAspectRatioCombo_ = new QComboBox;
    heatmapPreserveAspectRatioCombo_->setFont(valueFont_);
    heatmapPreserveAspectRatioCombo_->setAutoFillBackground(true);
    heatmapPreserveAspectRatioCombo_->addItem(QStringLiteral("False"));
    heatmapPreserveAspectRatioCombo_->addItem(QStringLiteral("True"));
    QObject::connect(heatmapPreserveAspectRatioCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapPreserveAspectRatioSetter_) {
          heatmapPreserveAspectRatioSetter_(index == 1);
        }
      });

    heatmapShowTopProfileCombo_ = new QComboBox;
    heatmapShowTopProfileCombo_->setFont(valueFont_);
    heatmapShowTopProfileCombo_->setAutoFillBackground(true);
    heatmapShowTopProfileCombo_->addItem(QStringLiteral("No"));
    heatmapShowTopProfileCombo_->addItem(QStringLiteral("Yes"));
    QObject::connect(heatmapShowTopProfileCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapShowTopProfileSetter_) {
          heatmapShowTopProfileSetter_(heatmapBoolFromIndex(index));
        }
      });

    heatmapShowRightProfileCombo_ = new QComboBox;
    heatmapShowRightProfileCombo_->setFont(valueFont_);
    heatmapShowRightProfileCombo_->setAutoFillBackground(true);
    heatmapShowRightProfileCombo_->addItem(QStringLiteral("No"));
    heatmapShowRightProfileCombo_->addItem(QStringLiteral("Yes"));
    QObject::connect(heatmapShowRightProfileCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapShowRightProfileSetter_) {
          heatmapShowRightProfileSetter_(heatmapBoolFromIndex(index));
        }
      });

    heatmapProfileModeCombo_ = new QComboBox;
    heatmapProfileModeCombo_->setFont(valueFont_);
    heatmapProfileModeCombo_->setAutoFillBackground(true);
    heatmapProfileModeCombo_->addItem(QStringLiteral("Absolute Profiles"));
    heatmapProfileModeCombo_->addItem(QStringLiteral("Averaged Profiles"));
    QObject::connect(heatmapProfileModeCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, [this](int index) {
        if (heatmapProfileModeSetter_) {
          heatmapProfileModeSetter_(heatmapProfileModeFromIndex(index));
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
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Color Map"), heatmapColorMapCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Invert Color Scale"),
      heatmapInvertGreyscaleCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Preserve Aspect Ratio"),
      heatmapPreserveAspectRatioCombo_);

    heatmapFlipHorizontalCombo_ = new QComboBox;
    heatmapFlipHorizontalCombo_->setFont(valueFont_);
    heatmapFlipHorizontalCombo_->setAutoFillBackground(true);
    heatmapFlipHorizontalCombo_->addItem(QStringLiteral("False"));
    heatmapFlipHorizontalCombo_->addItem(QStringLiteral("True"));
    QObject::connect(heatmapFlipHorizontalCombo_,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [this](int index) {
                       if (heatmapFlipHorizontalSetter_) {
                         heatmapFlipHorizontalSetter_(index == 1);
                       }
                     });
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Flip Horizontal"),
           heatmapFlipHorizontalCombo_);

    heatmapFlipVerticalCombo_ = new QComboBox;
    heatmapFlipVerticalCombo_->setFont(valueFont_);
    heatmapFlipVerticalCombo_->setAutoFillBackground(true);
    heatmapFlipVerticalCombo_->addItem(QStringLiteral("False"));
    heatmapFlipVerticalCombo_->addItem(QStringLiteral("True"));
    QObject::connect(heatmapFlipVerticalCombo_,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [this](int index) {
                       if (heatmapFlipVerticalSetter_) {
                         heatmapFlipVerticalSetter_(index == 1);
                       }
                     });
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Flip Vertical"),
           heatmapFlipVerticalCombo_);

    heatmapRotationCombo_ = new QComboBox;
    heatmapRotationCombo_->setFont(valueFont_);
    heatmapRotationCombo_->setAutoFillBackground(true);
    heatmapRotationCombo_->addItem(QStringLiteral("None"));
    heatmapRotationCombo_->addItem(QStringLiteral("90"));
    heatmapRotationCombo_->addItem(QStringLiteral("180"));
    heatmapRotationCombo_->addItem(QStringLiteral("270"));
    QObject::connect(heatmapRotationCombo_,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [this](int index) {
                       if (heatmapRotationSetter_) {
                         heatmapRotationSetter_(static_cast<HeatmapRotation>(index));
                       }
                     });
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Rotation"),
           heatmapRotationCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Profile Mode"),
      heatmapProfileModeCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Show Top Profile"),
      heatmapShowTopProfileCombo_);
    addRow(heatmapLayout, heatmapRow++, QStringLiteral("Show Right Profile"),
      heatmapShowRightProfileCombo_);
    heatmapLayout->setRowStretch(heatmapRow, 1);
    updateHeatmapDimensionControls();
    entriesLayout->addWidget(heatmapSection_);

    waterfallSection_ = new QWidget(entriesWidget_);
    auto *waterfallLayout = new QGridLayout(waterfallSection_);
    waterfallLayout->setContentsMargins(0, 0, 0, 0);
    waterfallLayout->setHorizontalSpacing(12);
    waterfallLayout->setVerticalSpacing(6);

    waterfallForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(waterfallForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(waterfallForegroundButton_,
              QStringLiteral("Waterfall Foreground"),
              waterfallForegroundSetter_);
        });

    waterfallBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(waterfallBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(waterfallBackgroundButton_,
              QStringLiteral("Waterfall Background"),
              waterfallBackgroundSetter_);
        });

    auto setupWaterfallEdit = [this](QLineEdit **field,
                                 const std::function<void()> &commit) {
      *field = createLineEdit();
      committedTexts_.insert(*field, (*field)->text());
      (*field)->installEventFilter(this);
      QObject::connect(*field, &QLineEdit::returnPressed, this, commit);
      QObject::connect(*field, &QLineEdit::editingFinished, this, commit);
    };

    setupWaterfallEdit(&waterfallTitleEdit_,
        [this]() { commitWaterfallTitle(); });
    setupWaterfallEdit(&waterfallXLabelEdit_,
        [this]() { commitWaterfallXLabel(); });
    setupWaterfallEdit(&waterfallYLabelEdit_,
        [this]() { commitWaterfallYLabel(); });
    setupWaterfallEdit(&waterfallDataChannelEdit_,
        [this]() { commitWaterfallDataChannel(); });
    setupWaterfallEdit(&waterfallCountChannelEdit_,
        [this]() { commitWaterfallCountChannel(); });
    setupWaterfallEdit(&waterfallTriggerChannelEdit_,
        [this]() { commitWaterfallTriggerChannel(); });
    setupWaterfallEdit(&waterfallEraseChannelEdit_,
        [this]() { commitWaterfallEraseChannel(); });
    setupWaterfallEdit(&waterfallHistoryEdit_,
        [this]() { commitWaterfallHistoryCount(); });
    setupWaterfallEdit(&waterfallIntensityMinEdit_,
        [this]() { commitWaterfallIntensityMinimum(); });
    setupWaterfallEdit(&waterfallIntensityMaxEdit_,
        [this]() { commitWaterfallIntensityMaximum(); });
    setupWaterfallEdit(&waterfallSamplePeriodEdit_,
        [this]() { commitWaterfallSamplePeriod(); });

    waterfallEraseModeCombo_ = new QComboBox;
    waterfallEraseModeCombo_->setFont(valueFont_);
    waterfallEraseModeCombo_->setAutoFillBackground(true);
    waterfallEraseModeCombo_->addItem(QStringLiteral("If Not Zero"));
    waterfallEraseModeCombo_->addItem(QStringLiteral("If Zero"));
    QObject::connect(waterfallEraseModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallEraseModeSetter_) {
            waterfallEraseModeSetter_(static_cast<WaterfallEraseMode>(index));
          }
        });

    waterfallScrollDirectionCombo_ = new QComboBox;
    waterfallScrollDirectionCombo_->setFont(valueFont_);
    waterfallScrollDirectionCombo_->setAutoFillBackground(true);
    waterfallScrollDirectionCombo_->addItem(QStringLiteral("Top To Bottom"));
    waterfallScrollDirectionCombo_->addItem(QStringLiteral("Bottom To Top"));
    waterfallScrollDirectionCombo_->addItem(QStringLiteral("Left To Right"));
    waterfallScrollDirectionCombo_->addItem(QStringLiteral("Right To Left"));
    QObject::connect(waterfallScrollDirectionCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallScrollDirectionSetter_) {
            waterfallScrollDirectionSetter_(
                static_cast<WaterfallScrollDirection>(index));
          }
        });

    waterfallColorMapCombo_ = new QComboBox;
    waterfallColorMapCombo_->setFont(valueFont_);
    waterfallColorMapCombo_->setAutoFillBackground(true);
    waterfallColorMapCombo_->addItem(QStringLiteral("Grayscale"));
    waterfallColorMapCombo_->addItem(QStringLiteral("Jet"));
    waterfallColorMapCombo_->addItem(QStringLiteral("Hot"));
    waterfallColorMapCombo_->addItem(QStringLiteral("Cool"));
    waterfallColorMapCombo_->addItem(QStringLiteral("Rainbow"));
    waterfallColorMapCombo_->addItem(QStringLiteral("Turbo"));
    QObject::connect(waterfallColorMapCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallColorMapSetter_) {
            waterfallColorMapSetter_(static_cast<HeatmapColorMap>(index));
          }
          updateWaterfallDependentControls();
        });

    waterfallInvertGreyscaleCombo_ = createBooleanComboBox();
    QObject::connect(waterfallInvertGreyscaleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallInvertGreyscaleSetter_) {
            waterfallInvertGreyscaleSetter_(index == 1);
          }
        });

    waterfallIntensityScaleCombo_ = new QComboBox;
    waterfallIntensityScaleCombo_->setFont(valueFont_);
    waterfallIntensityScaleCombo_->setAutoFillBackground(true);
    waterfallIntensityScaleCombo_->addItem(QStringLiteral("Auto"));
    waterfallIntensityScaleCombo_->addItem(QStringLiteral("Manual"));
    waterfallIntensityScaleCombo_->addItem(QStringLiteral("Log"));
    QObject::connect(waterfallIntensityScaleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallIntensityScaleSetter_) {
            waterfallIntensityScaleSetter_(
                static_cast<WaterfallIntensityScale>(index));
          }
          updateWaterfallDependentControls();
        });

    waterfallShowLegendCombo_ = createBooleanComboBox();
    QObject::connect(waterfallShowLegendCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallShowLegendSetter_) {
            waterfallShowLegendSetter_(index == 1);
          }
        });

    waterfallShowGridCombo_ = createBooleanComboBox();
    QObject::connect(waterfallShowGridCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallShowGridSetter_) {
            waterfallShowGridSetter_(index == 1);
          }
        });

    waterfallUnitsCombo_ = new QComboBox;
    waterfallUnitsCombo_->setFont(valueFont_);
    waterfallUnitsCombo_->setAutoFillBackground(true);
    waterfallUnitsCombo_->addItem(QStringLiteral("Milliseconds"));
    waterfallUnitsCombo_->addItem(QStringLiteral("Seconds"));
    waterfallUnitsCombo_->addItem(QStringLiteral("Minutes"));
    QObject::connect(waterfallUnitsCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waterfallUnitsSetter_) {
            waterfallUnitsSetter_(timeUnitsFromIndex(index));
          }
        });

    int waterfallRow = 0;
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Foreground"),
        waterfallForegroundButton_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Background"),
        waterfallBackgroundButton_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Title"),
        waterfallTitleEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("X Label"),
        waterfallXLabelEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Y Label"),
        waterfallYLabelEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Data PV"),
        waterfallDataChannelEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Count PV"),
        waterfallCountChannelEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Trigger PV"),
        waterfallTriggerChannelEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Erase PV"),
        waterfallEraseChannelEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Erase Mode"),
        waterfallEraseModeCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("History Count"),
        waterfallHistoryEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Scroll Direction"),
        waterfallScrollDirectionCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Color Map"),
        waterfallColorMapCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Invert Color Scale"),
        waterfallInvertGreyscaleCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Intensity Scale"),
        waterfallIntensityScaleCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Intensity Minimum"),
        waterfallIntensityMinEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Intensity Maximum"),
        waterfallIntensityMaxEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Show Legend"),
        waterfallShowLegendCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Show Grid"),
        waterfallShowGridCombo_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Sample Period"),
        waterfallSamplePeriodEdit_);
    addRow(waterfallLayout, waterfallRow++, QStringLiteral("Units"),
        waterfallUnitsCombo_);
    waterfallLayout->setRowStretch(waterfallRow, 1);
    updateWaterfallDependentControls();
    entriesLayout->addWidget(waterfallSection_);

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

    pvTableSection_ = new QWidget(entriesWidget_);
    auto *pvTableLayout = new QGridLayout(pvTableSection_);
    pvTableLayout->setContentsMargins(0, 0, 0, 0);
    pvTableLayout->setHorizontalSpacing(12);
    pvTableLayout->setVerticalSpacing(6);

    pvTableForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(pvTableForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(pvTableForegroundButton_,
              QStringLiteral("PV Table Foreground"),
              pvTableForegroundSetter_);
        });

    pvTableBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(pvTableBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(pvTableBackgroundButton_,
              QStringLiteral("PV Table Background"),
              pvTableBackgroundSetter_);
        });

    pvTableColorModeCombo_ = new QComboBox;
    pvTableColorModeCombo_->setFont(valueFont_);
    pvTableColorModeCombo_->setAutoFillBackground(true);
    pvTableColorModeCombo_->addItem(QStringLiteral("Static"));
    pvTableColorModeCombo_->addItem(QStringLiteral("Alarm"));
    pvTableColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(pvTableColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (pvTableColorModeSetter_) {
            pvTableColorModeSetter_(colorModeFromIndex(index));
          }
        });

    pvTableShowHeadersCombo_ = createBooleanComboBox();
    QObject::connect(pvTableShowHeadersCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (pvTableShowHeadersSetter_) {
            pvTableShowHeadersSetter_(index != 0);
          }
        });

    pvTableColumnsEdit_ = createLineEdit();
    committedTexts_.insert(pvTableColumnsEdit_, pvTableColumnsEdit_->text());
    pvTableColumnsEdit_->installEventFilter(this);
    QObject::connect(pvTableColumnsEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitPvTableColumns(); });
    QObject::connect(pvTableColumnsEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitPvTableColumns(); });

    pvTableRowsWidget_ = new QWidget(pvTableSection_);
    auto *pvTableRowsLayout = new QGridLayout(pvTableRowsWidget_);
    pvTableRowsLayout->setContentsMargins(0, 0, 0, 0);
    pvTableRowsLayout->setHorizontalSpacing(8);
    pvTableRowsLayout->setVerticalSpacing(4);
    auto *pvTableRowHeader = new QLabel(QStringLiteral("Row"));
    pvTableRowHeader->setFont(labelFont_);
    pvTableRowHeader->setAlignment(Qt::AlignCenter);
    pvTableRowsLayout->addWidget(pvTableRowHeader, 0, 0);
    auto *pvTableLabelHeader = new QLabel(QStringLiteral("Label"));
    pvTableLabelHeader->setFont(labelFont_);
    pvTableLabelHeader->setAlignment(Qt::AlignCenter);
    pvTableRowsLayout->addWidget(pvTableLabelHeader, 0, 1);
    auto *pvTableChannelHeader = new QLabel(QStringLiteral("Channel"));
    pvTableChannelHeader->setFont(labelFont_);
    pvTableChannelHeader->setAlignment(Qt::AlignCenter);
    pvTableRowsLayout->addWidget(pvTableChannelHeader, 0, 2);
    for (int i = 0; i < kPvTableRowCount; ++i) {
      auto *rowLabel = new QLabel(QStringLiteral("%1").arg(i + 1));
      rowLabel->setFont(labelFont_);
      rowLabel->setAlignment(Qt::AlignCenter);
      pvTableRowsLayout->addWidget(rowLabel, i + 1, 0);

      pvTableRowLabelEdits_[i] = createLineEdit();
      committedTexts_.insert(pvTableRowLabelEdits_[i], pvTableRowLabelEdits_[i]->text());
      pvTableRowLabelEdits_[i]->installEventFilter(this);
      QObject::connect(pvTableRowLabelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitPvTableRowLabel(i); });
      QObject::connect(pvTableRowLabelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitPvTableRowLabel(i); });
      pvTableRowsLayout->addWidget(pvTableRowLabelEdits_[i], i + 1, 1);

      pvTableRowChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(pvTableRowChannelEdits_[i], pvTableRowChannelEdits_[i]->text());
      pvTableRowChannelEdits_[i]->installEventFilter(this);
      QObject::connect(pvTableRowChannelEdits_[i], &QLineEdit::returnPressed, this,
          [this, i]() { commitPvTableRowChannel(i); });
      QObject::connect(pvTableRowChannelEdits_[i], &QLineEdit::editingFinished, this,
          [this, i]() { commitPvTableRowChannel(i); });
      pvTableRowsLayout->addWidget(pvTableRowChannelEdits_[i], i + 1, 2);
    }
    pvTableRowsLayout->setColumnStretch(1, 1);
    pvTableRowsLayout->setColumnStretch(2, 1);

    addRow(pvTableLayout, 0, QStringLiteral("Foreground"), pvTableForegroundButton_);
    addRow(pvTableLayout, 1, QStringLiteral("Background"), pvTableBackgroundButton_);
    addRow(pvTableLayout, 2, QStringLiteral("Color Mode"), pvTableColorModeCombo_);
    addRow(pvTableLayout, 3, QStringLiteral("Show Headers"), pvTableShowHeadersCombo_);
    addRow(pvTableLayout, 4, QStringLiteral("Columns"), pvTableColumnsEdit_);
    addRow(pvTableLayout, 5, QStringLiteral("Rows"), pvTableRowsWidget_);
    pvTableLayout->setRowStretch(6, 1);
    entriesLayout->addWidget(pvTableSection_);

    waveTableSection_ = new QWidget(entriesWidget_);
    auto *waveTableLayout = new QGridLayout(waveTableSection_);
    waveTableLayout->setContentsMargins(0, 0, 0, 0);
    waveTableLayout->setHorizontalSpacing(12);
    waveTableLayout->setVerticalSpacing(6);

    waveTableForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(waveTableForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(waveTableForegroundButton_,
              QStringLiteral("Waveform Table Foreground"),
              waveTableForegroundSetter_);
        });

    waveTableBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(waveTableBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(waveTableBackgroundButton_,
              QStringLiteral("Waveform Table Background"),
              waveTableBackgroundSetter_);
        });

    waveTableColorModeCombo_ = new QComboBox;
    waveTableColorModeCombo_->setFont(valueFont_);
    waveTableColorModeCombo_->setAutoFillBackground(true);
    waveTableColorModeCombo_->addItem(QStringLiteral("Static"));
    waveTableColorModeCombo_->addItem(QStringLiteral("Alarm"));
    waveTableColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(waveTableColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waveTableColorModeSetter_) {
            waveTableColorModeSetter_(colorModeFromIndex(index));
          }
        });

    waveTableShowHeadersCombo_ = createBooleanComboBox();
    QObject::connect(waveTableShowHeadersCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waveTableShowHeadersSetter_) {
            waveTableShowHeadersSetter_(index != 0);
          }
        });

    waveTableChannelEdit_ = createLineEdit();
    committedTexts_.insert(waveTableChannelEdit_, waveTableChannelEdit_->text());
    waveTableChannelEdit_->installEventFilter(this);
    QObject::connect(waveTableChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitWaveTableChannel(); });
    QObject::connect(waveTableChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitWaveTableChannel(); });

    waveTableLayoutCombo_ = new QComboBox;
    waveTableLayoutCombo_->setFont(valueFont_);
    waveTableLayoutCombo_->setAutoFillBackground(true);
    waveTableLayoutCombo_->addItem(QStringLiteral("Grid"));
    waveTableLayoutCombo_->addItem(QStringLiteral("Column"));
    waveTableLayoutCombo_->addItem(QStringLiteral("Row"));
    QObject::connect(waveTableLayoutCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waveTableLayoutSetter_) {
            waveTableLayoutSetter_(waveTableLayoutFromIndex(index));
          }
        });

    waveTableColumnsEdit_ = createLineEdit();
    waveTableColumnsEdit_->setValidator(new QIntValidator(1, 100000, this));
    committedTexts_.insert(waveTableColumnsEdit_, waveTableColumnsEdit_->text());
    waveTableColumnsEdit_->installEventFilter(this);
    QObject::connect(waveTableColumnsEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitWaveTableColumns(); });
    QObject::connect(waveTableColumnsEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitWaveTableColumns(); });

    waveTableMaxElementsEdit_ = createLineEdit();
    waveTableMaxElementsEdit_->setValidator(new QIntValidator(0, 1000000, this));
    committedTexts_.insert(waveTableMaxElementsEdit_,
        waveTableMaxElementsEdit_->text());
    waveTableMaxElementsEdit_->installEventFilter(this);
    QObject::connect(waveTableMaxElementsEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitWaveTableMaxElements(); });
    QObject::connect(waveTableMaxElementsEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitWaveTableMaxElements(); });

    waveTableIndexBaseCombo_ = new QComboBox;
    waveTableIndexBaseCombo_->setFont(valueFont_);
    waveTableIndexBaseCombo_->setAutoFillBackground(true);
    waveTableIndexBaseCombo_->addItem(QStringLiteral("0"));
    waveTableIndexBaseCombo_->addItem(QStringLiteral("1"));
    QObject::connect(waveTableIndexBaseCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waveTableIndexBaseSetter_) {
            waveTableIndexBaseSetter_(index == 0 ? 0 : 1);
          }
        });

    waveTableValueFormatCombo_ = new QComboBox;
    waveTableValueFormatCombo_->setFont(valueFont_);
    waveTableValueFormatCombo_->setAutoFillBackground(true);
    waveTableValueFormatCombo_->addItem(QStringLiteral("Default"));
    waveTableValueFormatCombo_->addItem(QStringLiteral("Fixed"));
    waveTableValueFormatCombo_->addItem(QStringLiteral("Scientific"));
    waveTableValueFormatCombo_->addItem(QStringLiteral("Hex"));
    waveTableValueFormatCombo_->addItem(QStringLiteral("Engineering"));
    QObject::connect(waveTableValueFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waveTableValueFormatSetter_) {
            waveTableValueFormatSetter_(waveTableValueFormatFromIndex(index));
          }
        });

    waveTableCharModeCombo_ = new QComboBox;
    waveTableCharModeCombo_->setFont(valueFont_);
    waveTableCharModeCombo_->setAutoFillBackground(true);
    waveTableCharModeCombo_->addItem(QStringLiteral("String"));
    waveTableCharModeCombo_->addItem(QStringLiteral("Bytes"));
    waveTableCharModeCombo_->addItem(QStringLiteral("ASCII"));
    waveTableCharModeCombo_->addItem(QStringLiteral("Numeric"));
    QObject::connect(waveTableCharModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (waveTableCharModeSetter_) {
            waveTableCharModeSetter_(waveTableCharModeFromIndex(index));
          }
        });

    addRow(waveTableLayout, 0, QStringLiteral("Foreground"),
        waveTableForegroundButton_);
    addRow(waveTableLayout, 1, QStringLiteral("Background"),
        waveTableBackgroundButton_);
    addRow(waveTableLayout, 2, QStringLiteral("Color Mode"),
        waveTableColorModeCombo_);
    addRow(waveTableLayout, 3, QStringLiteral("Show Headers"),
        waveTableShowHeadersCombo_);
    addRow(waveTableLayout, 4, QStringLiteral("Channel"),
        waveTableChannelEdit_);
    addRow(waveTableLayout, 5, QStringLiteral("Layout"),
        waveTableLayoutCombo_);
    addRow(waveTableLayout, 6, QStringLiteral("Columns"),
        waveTableColumnsEdit_);
    addRow(waveTableLayout, 7, QStringLiteral("Max Elements"),
        waveTableMaxElementsEdit_);
    addRow(waveTableLayout, 8, QStringLiteral("Index Base"),
        waveTableIndexBaseCombo_);
    addRow(waveTableLayout, 9, QStringLiteral("Value Format"),
        waveTableValueFormatCombo_);
    addRow(waveTableLayout, 10, QStringLiteral("Char Mode"),
        waveTableCharModeCombo_);
    waveTableLayout->setRowStretch(11, 1);
    entriesLayout->addWidget(waveTableSection_);

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

    setpointControlSection_ = new QWidget(entriesWidget_);
    auto *setpointLayout = new QGridLayout(setpointControlSection_);
    setpointLayout->setContentsMargins(0, 0, 0, 0);
    setpointLayout->setHorizontalSpacing(12);
    setpointLayout->setVerticalSpacing(6);

    setpointControlForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(setpointControlForegroundButton_, &QPushButton::clicked,
        this, [this]() {
          openColorPalette(setpointControlForegroundButton_,
              QStringLiteral("Setpoint Control Foreground"),
              setpointControlForegroundSetter_);
        });

    setpointControlBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(setpointControlBackgroundButton_, &QPushButton::clicked,
        this, [this]() {
          openColorPalette(setpointControlBackgroundButton_,
              QStringLiteral("Setpoint Control Background"),
              setpointControlBackgroundSetter_);
        });

    setpointControlFormatCombo_ = new QComboBox;
    setpointControlFormatCombo_->setFont(valueFont_);
    setpointControlFormatCombo_->setAutoFillBackground(true);
    setpointControlFormatCombo_->addItem(QStringLiteral("Decimal"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Exponential"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Engineering"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Compact"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Truncated"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Hexadecimal"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Octal"));
    setpointControlFormatCombo_->addItem(QStringLiteral("String"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Sexagesimal"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Sexagesimal (H:M:S)"));
    setpointControlFormatCombo_->addItem(QStringLiteral("Sexagesimal (D:M:S)"));
    QObject::connect(setpointControlFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (setpointControlFormatSetter_) {
            setpointControlFormatSetter_(textMonitorFormatFromIndex(index));
          }
        });

    setpointControlColorModeCombo_ = new QComboBox;
    setpointControlColorModeCombo_->setFont(valueFont_);
    setpointControlColorModeCombo_->setAutoFillBackground(true);
    setpointControlColorModeCombo_->addItem(QStringLiteral("Static"));
    setpointControlColorModeCombo_->addItem(QStringLiteral("Alarm"));
    setpointControlColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(setpointControlColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (setpointControlColorModeSetter_) {
            setpointControlColorModeSetter_(colorModeFromIndex(index));
          }
        });

    setpointControlLabelEdit_ = createLineEdit();
    committedTexts_.insert(setpointControlLabelEdit_,
        setpointControlLabelEdit_->text());
    setpointControlLabelEdit_->installEventFilter(this);
    QObject::connect(setpointControlLabelEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitSetpointControlLabel(); });
    QObject::connect(setpointControlLabelEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitSetpointControlLabel(); });

    setpointControlSetpointEdit_ = createLineEdit();
    committedTexts_.insert(setpointControlSetpointEdit_,
        setpointControlSetpointEdit_->text());
    setpointControlSetpointEdit_->installEventFilter(this);
    QObject::connect(setpointControlSetpointEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitSetpointControlSetpoint(); });
    QObject::connect(setpointControlSetpointEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitSetpointControlSetpoint(); });

    setpointControlReadbackEdit_ = createLineEdit();
    committedTexts_.insert(setpointControlReadbackEdit_,
        setpointControlReadbackEdit_->text());
    setpointControlReadbackEdit_->installEventFilter(this);
    QObject::connect(setpointControlReadbackEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitSetpointControlReadback(); });
    QObject::connect(setpointControlReadbackEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitSetpointControlReadback(); });

    setpointControlToleranceModeCombo_ = new QComboBox;
    setpointControlToleranceModeCombo_->setFont(valueFont_);
    setpointControlToleranceModeCombo_->setAutoFillBackground(true);
    setpointControlToleranceModeCombo_->addItem(QStringLiteral("None"));
    setpointControlToleranceModeCombo_->addItem(QStringLiteral("Absolute"));
    QObject::connect(setpointControlToleranceModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (!setpointControlToleranceModeSetter_) {
            return;
          }
          setpointControlToleranceModeSetter_(
              index == 1 ? SetpointToleranceMode::kAbsolute
                         : SetpointToleranceMode::kNone);
        });

    setpointControlToleranceEdit_ = createLineEdit();
    setpointControlToleranceEdit_->setValidator(new QDoubleValidator(
        0.0, std::numeric_limits<double>::max(), 12,
        setpointControlToleranceEdit_));
    committedTexts_.insert(setpointControlToleranceEdit_,
        setpointControlToleranceEdit_->text());
    setpointControlToleranceEdit_->installEventFilter(this);
    QObject::connect(setpointControlToleranceEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitSetpointControlTolerance(); });
    QObject::connect(setpointControlToleranceEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitSetpointControlTolerance(); });

    setpointControlShowReadbackCombo_ = createBooleanComboBox();
    QObject::connect(setpointControlShowReadbackCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (setpointControlShowReadbackSetter_) {
            setpointControlShowReadbackSetter_(index != 0);
          }
        });

    setpointControlPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    setpointControlPvLimitsButton_->setEnabled(false);
    QObject::connect(setpointControlPvLimitsButton_, &QPushButton::clicked,
        this, [this]() { openSetpointControlPvLimitsDialog(); });

    addRow(setpointLayout, 0, QStringLiteral("Foreground"),
        setpointControlForegroundButton_);
    addRow(setpointLayout, 1, QStringLiteral("Background"),
        setpointControlBackgroundButton_);
    addRow(setpointLayout, 2, QStringLiteral("Format"),
        setpointControlFormatCombo_);
    addRow(setpointLayout, 3, QStringLiteral("Color Mode"),
        setpointControlColorModeCombo_);
    addRow(setpointLayout, 4, QStringLiteral("Label"),
        setpointControlLabelEdit_);
    addRow(setpointLayout, 5, QStringLiteral("Setpoint"),
        setpointControlSetpointEdit_);
    addRow(setpointLayout, 6, QStringLiteral("Readback"),
        setpointControlReadbackEdit_);
    addRow(setpointLayout, 7, QStringLiteral("Tolerance Mode"),
        setpointControlToleranceModeCombo_);
    addRow(setpointLayout, 8, QStringLiteral("Tolerance"),
        setpointControlToleranceEdit_);
    addRow(setpointLayout, 9, QStringLiteral("Show Readback"),
        setpointControlShowReadbackCombo_);
    addRow(setpointLayout, 10, QStringLiteral("Channel Limits"),
        setpointControlPvLimitsButton_);
    setpointLayout->setRowStretch(11, 1);
    entriesLayout->addWidget(setpointControlSection_);

    textAreaSection_ = new QWidget(entriesWidget_);
    auto *textAreaLayout = new QGridLayout(textAreaSection_);
    textAreaLayout->setContentsMargins(0, 0, 0, 0);
    textAreaLayout->setHorizontalSpacing(12);
    textAreaLayout->setVerticalSpacing(6);

    textAreaForegroundButton_ =
        createColorButton(basePalette.color(QPalette::WindowText));
    QObject::connect(textAreaForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textAreaForegroundButton_,
              QStringLiteral("Text Area Foreground"),
              textAreaForegroundSetter_);
        });

    textAreaBackgroundButton_ =
        createColorButton(basePalette.color(QPalette::Window));
    QObject::connect(textAreaBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(textAreaBackgroundButton_,
              QStringLiteral("Text Area Background"),
              textAreaBackgroundSetter_);
        });

    textAreaFormatCombo_ = new QComboBox;
    textAreaFormatCombo_->setFont(valueFont_);
    textAreaFormatCombo_->setAutoFillBackground(true);
    textAreaFormatCombo_->addItem(QStringLiteral("Decimal"));
    textAreaFormatCombo_->addItem(QStringLiteral("Exponential"));
    textAreaFormatCombo_->addItem(QStringLiteral("Engineering"));
    textAreaFormatCombo_->addItem(QStringLiteral("Compact"));
    textAreaFormatCombo_->addItem(QStringLiteral("Truncated"));
    textAreaFormatCombo_->addItem(QStringLiteral("Hexadecimal"));
    textAreaFormatCombo_->addItem(QStringLiteral("Octal"));
    textAreaFormatCombo_->addItem(QStringLiteral("String"));
    textAreaFormatCombo_->addItem(QStringLiteral("Sexagesimal"));
    textAreaFormatCombo_->addItem(QStringLiteral("Sexagesimal (H:M:S)"));
    textAreaFormatCombo_->addItem(QStringLiteral("Sexagesimal (D:M:S)"));
    QObject::connect(textAreaFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaFormatSetter_) {
            textAreaFormatSetter_(textMonitorFormatFromIndex(index));
          }
        });

    textAreaColorModeCombo_ = new QComboBox;
    textAreaColorModeCombo_->setFont(valueFont_);
    textAreaColorModeCombo_->setAutoFillBackground(true);
    textAreaColorModeCombo_->addItem(QStringLiteral("Static"));
    textAreaColorModeCombo_->addItem(QStringLiteral("Alarm"));
    textAreaColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(textAreaColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaColorModeSetter_) {
            textAreaColorModeSetter_(colorModeFromIndex(index));
          }
        });

    textAreaChannelEdit_ = createLineEdit();
    committedTexts_.insert(textAreaChannelEdit_, textAreaChannelEdit_->text());
    textAreaChannelEdit_->installEventFilter(this);
    QObject::connect(textAreaChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextAreaChannel(); });
    QObject::connect(textAreaChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextAreaChannel(); });

    textAreaPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    textAreaPvLimitsButton_->setEnabled(false);
    QObject::connect(textAreaPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openTextAreaPvLimitsDialog(); });

    textAreaReadOnlyCombo_ = createBooleanComboBox();
    QObject::connect(textAreaReadOnlyCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaReadOnlySetter_) {
            textAreaReadOnlySetter_(index != 0);
          }
        });

    textAreaWordWrapCombo_ = createBooleanComboBox();
    QObject::connect(textAreaWordWrapCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaWordWrapSetter_) {
            textAreaWordWrapSetter_(index != 0);
          }
          updateTextAreaDependentControls();
        });

    textAreaLineWrapModeCombo_ = new QComboBox;
    textAreaLineWrapModeCombo_->setFont(valueFont_);
    textAreaLineWrapModeCombo_->setAutoFillBackground(true);
    textAreaLineWrapModeCombo_->addItem(QStringLiteral("No Wrap"));
    textAreaLineWrapModeCombo_->addItem(QStringLiteral("Widget Width"));
    textAreaLineWrapModeCombo_->addItem(QStringLiteral("Fixed Column Width"));
    QObject::connect(textAreaLineWrapModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaWrapModeSetter_) {
            textAreaWrapModeSetter_(textAreaWrapModeFromIndex(index));
          }
          updateTextAreaDependentControls();
        });

    textAreaWrapColumnWidthEdit_ = createLineEdit();
    committedTexts_.insert(textAreaWrapColumnWidthEdit_,
        textAreaWrapColumnWidthEdit_->text());
    textAreaWrapColumnWidthEdit_->installEventFilter(this);
    QObject::connect(textAreaWrapColumnWidthEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitTextAreaWrapColumnWidth(); });
    QObject::connect(textAreaWrapColumnWidthEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitTextAreaWrapColumnWidth(); });

    textAreaVerticalScrollBarCombo_ = createBooleanComboBox();
    QObject::connect(textAreaVerticalScrollBarCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaShowVerticalScrollBarSetter_) {
            textAreaShowVerticalScrollBarSetter_(index != 0);
          }
        });

    textAreaHorizontalScrollBarCombo_ = createBooleanComboBox();
    QObject::connect(textAreaHorizontalScrollBarCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaShowHorizontalScrollBarSetter_) {
            textAreaShowHorizontalScrollBarSetter_(index != 0);
          }
        });

    textAreaCommitModeCombo_ = new QComboBox;
    textAreaCommitModeCombo_->setFont(valueFont_);
    textAreaCommitModeCombo_->setAutoFillBackground(true);
    textAreaCommitModeCombo_->addItem(QStringLiteral("Ctrl+Enter"));
    textAreaCommitModeCombo_->addItem(QStringLiteral("Enter"));
    textAreaCommitModeCombo_->addItem(QStringLiteral("On Focus Lost"));
    textAreaCommitModeCombo_->addItem(QStringLiteral("Explicit"));
    QObject::connect(textAreaCommitModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaCommitModeSetter_) {
            textAreaCommitModeSetter_(textAreaCommitModeFromIndex(index));
          }
        });

    textAreaTabInsertsSpacesCombo_ = createBooleanComboBox();
    QObject::connect(textAreaTabInsertsSpacesCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (textAreaTabInsertsSpacesSetter_) {
            textAreaTabInsertsSpacesSetter_(index != 0);
          }
        });

    textAreaTabWidthEdit_ = createLineEdit();
    committedTexts_.insert(textAreaTabWidthEdit_, textAreaTabWidthEdit_->text());
    textAreaTabWidthEdit_->installEventFilter(this);
    QObject::connect(textAreaTabWidthEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextAreaTabWidth(); });
    QObject::connect(textAreaTabWidthEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextAreaTabWidth(); });

    textAreaFontFamilyEdit_ = createLineEdit();
    committedTexts_.insert(textAreaFontFamilyEdit_, textAreaFontFamilyEdit_->text());
    textAreaFontFamilyEdit_->installEventFilter(this);
    QObject::connect(textAreaFontFamilyEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextAreaFontFamily(); });
    QObject::connect(textAreaFontFamilyEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitTextAreaFontFamily(); });

    addRow(textAreaLayout, 0, QStringLiteral("Foreground"),
        textAreaForegroundButton_);
    addRow(textAreaLayout, 1, QStringLiteral("Background"),
        textAreaBackgroundButton_);
    addRow(textAreaLayout, 2, QStringLiteral("Format"),
        textAreaFormatCombo_);
    addRow(textAreaLayout, 3, QStringLiteral("Color Mode"),
        textAreaColorModeCombo_);
    addRow(textAreaLayout, 4, QStringLiteral("Channel"),
        textAreaChannelEdit_);
    addRow(textAreaLayout, 5, QStringLiteral("Channel Limits"),
        textAreaPvLimitsButton_);
    addRow(textAreaLayout, 6, QStringLiteral("Read Only"),
        textAreaReadOnlyCombo_);
    addRow(textAreaLayout, 7, QStringLiteral("Word Wrap"),
        textAreaWordWrapCombo_);
    addRow(textAreaLayout, 8, QStringLiteral("Line Wrap Mode"),
        textAreaLineWrapModeCombo_);
    addRow(textAreaLayout, 9, QStringLiteral("Wrap Column Width"),
        textAreaWrapColumnWidthEdit_);
    addRow(textAreaLayout, 10, QStringLiteral("Vertical Scroll Bar"),
        textAreaVerticalScrollBarCombo_);
    addRow(textAreaLayout, 11, QStringLiteral("Horizontal Scroll Bar"),
        textAreaHorizontalScrollBarCombo_);
    addRow(textAreaLayout, 12, QStringLiteral("Commit Mode"),
        textAreaCommitModeCombo_);
    addRow(textAreaLayout, 13, QStringLiteral("Tab Inserts Spaces"),
        textAreaTabInsertsSpacesCombo_);
    addRow(textAreaLayout, 14, QStringLiteral("Tab Width"),
        textAreaTabWidthEdit_);
    addRow(textAreaLayout, 15, QStringLiteral("Font Family"),
        textAreaFontFamilyEdit_);
    textAreaLayout->setRowStretch(16, 1);
    entriesLayout->addWidget(textAreaSection_);

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

    thermometerSection_ = new QWidget(entriesWidget_);
    auto *thermometerLayout = new QGridLayout(thermometerSection_);
    thermometerLayout->setContentsMargins(0, 0, 0, 0);
    thermometerLayout->setHorizontalSpacing(12);
    thermometerLayout->setVerticalSpacing(6);

    thermometerForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(thermometerForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(thermometerForegroundButton_,
              QStringLiteral("Thermometer Foreground"),
              thermometerForegroundSetter_);
        });

    thermometerBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(thermometerBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(thermometerBackgroundButton_,
              QStringLiteral("Thermometer Background"),
              thermometerBackgroundSetter_);
        });

    thermometerTextButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(thermometerTextButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(thermometerTextButton_,
              QStringLiteral("Thermometer Text"),
              thermometerTextSetter_);
        });

    thermometerLabelCombo_ = new QComboBox;
    thermometerLabelCombo_->setFont(valueFont_);
    thermometerLabelCombo_->setAutoFillBackground(true);
    thermometerLabelCombo_->addItem(QStringLiteral("None"));
    thermometerLabelCombo_->addItem(QStringLiteral("No Decorations"));
    thermometerLabelCombo_->addItem(QStringLiteral("Outline"));
    thermometerLabelCombo_->addItem(QStringLiteral("Limits"));
    thermometerLabelCombo_->addItem(QStringLiteral("Channel"));
    QObject::connect(thermometerLabelCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (thermometerLabelSetter_) {
            thermometerLabelSetter_(meterLabelFromIndex(index));
          }
        });

    thermometerColorModeCombo_ = new QComboBox;
    thermometerColorModeCombo_->setFont(valueFont_);
    thermometerColorModeCombo_->setAutoFillBackground(true);
    thermometerColorModeCombo_->addItem(QStringLiteral("Static"));
    thermometerColorModeCombo_->addItem(QStringLiteral("Alarm"));
    thermometerColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(thermometerColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (thermometerColorModeSetter_) {
            thermometerColorModeSetter_(colorModeFromIndex(index));
          }
        });

    thermometerFormatCombo_ = new QComboBox;
    thermometerFormatCombo_->setFont(valueFont_);
    thermometerFormatCombo_->setAutoFillBackground(true);
    thermometerFormatCombo_->addItem(QStringLiteral("Decimal"));
    thermometerFormatCombo_->addItem(QStringLiteral("Exponential"));
    thermometerFormatCombo_->addItem(QStringLiteral("Engineering"));
    thermometerFormatCombo_->addItem(QStringLiteral("Compact"));
    thermometerFormatCombo_->addItem(QStringLiteral("Truncated"));
    QObject::connect(thermometerFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (thermometerFormatSetter_) {
            thermometerFormatSetter_(textMonitorFormatFromIndex(index));
          }
        });

    thermometerShowValueCombo_ = new QComboBox;
    thermometerShowValueCombo_->setFont(valueFont_);
    thermometerShowValueCombo_->setAutoFillBackground(true);
    thermometerShowValueCombo_->addItem(QStringLiteral("Off"));
    thermometerShowValueCombo_->addItem(QStringLiteral("On"));
    QObject::connect(thermometerShowValueCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (thermometerShowValueSetter_) {
            thermometerShowValueSetter_(index != 0);
          }
        });

    thermometerVisibilityCombo_ = new QComboBox;
    thermometerVisibilityCombo_->setFont(valueFont_);
    thermometerVisibilityCombo_->setAutoFillBackground(true);
    thermometerVisibilityCombo_->addItem(QStringLiteral("Static"));
    thermometerVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    thermometerVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    thermometerVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(thermometerVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (thermometerVisibilityModeSetter_) {
            thermometerVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
        });

    thermometerVisibilityCalcEdit_ = createLineEdit();
    committedTexts_.insert(thermometerVisibilityCalcEdit_,
        thermometerVisibilityCalcEdit_->text());
    thermometerVisibilityCalcEdit_->installEventFilter(this);
    QObject::connect(thermometerVisibilityCalcEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitThermometerVisibilityCalc(); });
    QObject::connect(thermometerVisibilityCalcEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitThermometerVisibilityCalc(); });

    thermometerChannelEdit_ = createLineEdit();
    committedTexts_.insert(thermometerChannelEdit_,
        thermometerChannelEdit_->text());
    thermometerChannelEdit_->installEventFilter(this);
    QObject::connect(thermometerChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitThermometerChannel(); });
    QObject::connect(thermometerChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitThermometerChannel(); });
    for (int i = 0;
         i < static_cast<int>(thermometerVisibilityChannelEdits_.size()); ++i) {
      thermometerVisibilityChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(thermometerVisibilityChannelEdits_[i],
          thermometerVisibilityChannelEdits_[i]->text());
      thermometerVisibilityChannelEdits_[i]->installEventFilter(this);
      QObject::connect(thermometerVisibilityChannelEdits_[i],
          &QLineEdit::returnPressed, this,
          [this, i]() { commitThermometerVisibilityChannel(i); });
      QObject::connect(thermometerVisibilityChannelEdits_[i],
          &QLineEdit::editingFinished, this,
          [this, i]() { commitThermometerVisibilityChannel(i); });
      if (i == 0) {
        QObject::connect(thermometerVisibilityChannelEdits_[i],
            &QLineEdit::textChanged, this,
            [this]() { updateThermometerChannelDependentControls(); });
      }
    }

    thermometerPvLimitsButton_ = createActionButton(
        QStringLiteral("Channel Limits..."));
    thermometerPvLimitsButton_->setEnabled(false);
    QObject::connect(thermometerPvLimitsButton_, &QPushButton::clicked, this,
        [this]() { openThermometerPvLimitsDialog(); });

    addRow(thermometerLayout, 0, QStringLiteral("Foreground"),
        thermometerForegroundButton_);
    addRow(thermometerLayout, 1, QStringLiteral("Background"),
        thermometerBackgroundButton_);
    addRow(thermometerLayout, 2, QStringLiteral("Text Color"),
        thermometerTextButton_);
    addRow(thermometerLayout, 3, QStringLiteral("Label"),
        thermometerLabelCombo_);
    addRow(thermometerLayout, 4, QStringLiteral("Color Mode"),
        thermometerColorModeCombo_);
    addRow(thermometerLayout, 5, QStringLiteral("Format"),
        thermometerFormatCombo_);
    addRow(thermometerLayout, 6, QStringLiteral("Value Overlay"),
        thermometerShowValueCombo_);
    thermometerShowValueCombo_->setVisible(false);
    if (QLabel *label = fieldLabels_.value(thermometerShowValueCombo_, nullptr)) {
      label->setVisible(false);
    }
    addRow(thermometerLayout, 7, QStringLiteral("Visibility"),
        thermometerVisibilityCombo_);
    addRow(thermometerLayout, 8, QStringLiteral("Vis Calc"),
        thermometerVisibilityCalcEdit_);
    addRow(thermometerLayout, 9, QStringLiteral("Channel"),
        thermometerChannelEdit_);
    addRow(thermometerLayout, 10, QStringLiteral("Channel A"),
        thermometerVisibilityChannelEdits_[0]);
    addRow(thermometerLayout, 11, QStringLiteral("Channel B"),
        thermometerVisibilityChannelEdits_[1]);
    addRow(thermometerLayout, 12, QStringLiteral("Channel C"),
        thermometerVisibilityChannelEdits_[2]);
    addRow(thermometerLayout, 13, QStringLiteral("Channel D"),
        thermometerVisibilityChannelEdits_[3]);
    addRow(thermometerLayout, 14, QStringLiteral("Channel Limits"),
        thermometerPvLimitsButton_);
    thermometerLayout->setRowStretch(15, 1);
    entriesLayout->addWidget(thermometerSection_);

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
    cartesianCountEdit_->setValidator(new QIntValidator(0,
        std::numeric_limits<int>::max(),
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

    cartesianDrawMajorCombo_ = createBooleanComboBox();
    QObject::connect(cartesianDrawMajorCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (cartesianDrawMajorSetter_) {
            cartesianDrawMajorSetter_(index == 1);
          }
        });

    cartesianDrawMinorCombo_ = createBooleanComboBox();
    QObject::connect(cartesianDrawMinorCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (cartesianDrawMinorSetter_) {
            cartesianDrawMinorSetter_(index == 1);
          }
        });

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
    addRow(cartesianLayout, 8, QStringLiteral("Draw Major Grid"), cartesianDrawMajorCombo_);
    addRow(cartesianLayout, 9, QStringLiteral("Draw Minor Grid"), cartesianDrawMinorCombo_);
    addRow(cartesianLayout, 10, QStringLiteral("Style"), cartesianStyleCombo_);
    addRow(cartesianLayout, 11, QStringLiteral("Erase Oldest"), cartesianEraseOldestCombo_);
    addRow(cartesianLayout, 12, QStringLiteral("Count"), cartesianCountEdit_);
    addRow(cartesianLayout, 13, QStringLiteral("Erase Mode"), cartesianEraseModeCombo_);
    addRow(cartesianLayout, 14, QStringLiteral("Trigger"), cartesianTriggerEdit_);
    addRow(cartesianLayout, 15, QStringLiteral("Erase"), cartesianEraseEdit_);
    addRow(cartesianLayout, 16, QStringLiteral("Count PV"), cartesianCountPvEdit_);

    cartesianAxisButton_ = createActionButton(QStringLiteral("Axis Data..."));
    cartesianAxisButton_->setEnabled(false);
    QObject::connect(cartesianAxisButton_, &QPushButton::clicked, this,
        [this]() { openCartesianAxisDialog(); });

    addRow(cartesianLayout, 17, QStringLiteral("Axis Data"), cartesianAxisButton_);
    addRow(cartesianLayout, 18, QStringLiteral("Traces"), cartesianTraceWidget);
    cartesianLayout->setRowStretch(19, 1);
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

    ledSection_ = new QWidget(entriesWidget_);
    auto *ledLayout = new QGridLayout(ledSection_);
    ledLayout->setContentsMargins(0, 0, 0, 0);
    ledLayout->setHorizontalSpacing(12);
    ledLayout->setVerticalSpacing(6);

    ledForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(ledForegroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(ledForegroundButton_,
              QStringLiteral("LED Monitor Foreground"),
              ledForegroundSetter_);
        });

    ledBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(ledBackgroundButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(ledBackgroundButton_,
              QStringLiteral("LED Monitor Background"),
              ledBackgroundSetter_);
        });

    ledOnColorButton_ = createColorButton(MedmColors::alarmColorForSeverity(0));
    QObject::connect(ledOnColorButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(ledOnColorButton_,
              QStringLiteral("LED Monitor On Color"),
              [this](const QColor &color) {
                if (ledOnColorSetter_) {
                  ledOnColorSetter_(color);
                }
                refreshLedMonitorColorButtons();
              });
        });

    ledOffColorButton_ = createColorButton(QColor(45, 45, 45));
    QObject::connect(ledOffColorButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(ledOffColorButton_,
              QStringLiteral("LED Monitor Off Color"),
              [this](const QColor &color) {
                if (ledOffColorSetter_) {
                  ledOffColorSetter_(color);
                }
                refreshLedMonitorColorButtons();
              });
        });

    ledUndefinedColorButton_ = createColorButton(QColor(204, 204, 204));
    QObject::connect(ledUndefinedColorButton_, &QPushButton::clicked, this,
        [this]() {
          openColorPalette(ledUndefinedColorButton_,
              QStringLiteral("LED Monitor Undefined Color"),
              ledUndefinedColorSetter_);
        });

    ledColorModeCombo_ = new QComboBox;
    ledColorModeCombo_->setFont(valueFont_);
    ledColorModeCombo_->setAutoFillBackground(true);
    ledColorModeCombo_->addItem(QStringLiteral("Static"));
    ledColorModeCombo_->addItem(QStringLiteral("Alarm"));
    ledColorModeCombo_->addItem(QStringLiteral("Binary"));
    ledColorModeCombo_->addItem(QStringLiteral("Discrete"));
    QObject::connect(ledColorModeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          const LedColorModeChoice choice = ledColorModeChoiceFromIndex(index);
          if (ledColorModeSetter_) {
            ledColorModeSetter_(ledTextColorModeForChoice(choice));
          }
          if (choice == LedColorModeChoice::kBinary) {
            applyBinaryLedPreset();
          }
          updateLedMonitorStateColorControls();
        });

    ledShapeCombo_ = new QComboBox;
    ledShapeCombo_->setFont(valueFont_);
    ledShapeCombo_->setAutoFillBackground(true);
    ledShapeCombo_->addItem(QStringLiteral("Circle"));
    ledShapeCombo_->addItem(QStringLiteral("Square"));
    ledShapeCombo_->addItem(QStringLiteral("Rounded Square"));
    QObject::connect(ledShapeCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (ledShapeSetter_) {
            ledShapeSetter_(ledShapeFromIndex(index));
          }
        });

    ledBezelCheckBox_ = new QCheckBox;
    ledBezelCheckBox_->setFont(valueFont_);
    QObject::connect(ledBezelCheckBox_, &QCheckBox::toggled, this,
        [this](bool checked) {
          if (ledBezelSetter_) {
            ledBezelSetter_(checked);
          }
        });

    ledStateCountSpin_ = new QSpinBox;
    ledStateCountSpin_->setFont(valueFont_);
    ledStateCountSpin_->setAutoFillBackground(true);
    QPalette ledSpinPalette = palette();
    ledSpinPalette.setColor(QPalette::Base, Qt::white);
    ledSpinPalette.setColor(QPalette::Text, Qt::black);
    ledStateCountSpin_->setPalette(ledSpinPalette);
    ledStateCountSpin_->setRange(1, kLedStateCount);
    QObject::connect(ledStateCountSpin_,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [this](int value) { commitLedStateCount(value); });

    ledStateColorsWidget_ = new QWidget(entriesWidget_);
    auto *ledStateColorsLayout = new QGridLayout(ledStateColorsWidget_);
    ledStateColorsLayout->setContentsMargins(0, 0, 0, 0);
    ledStateColorsLayout->setHorizontalSpacing(6);
    ledStateColorsLayout->setVerticalSpacing(6);
    for (int i = 0; i < kLedStateCount; ++i) {
      auto *button = createColorButton(basePalette.color(QPalette::WindowText));
      button->setFixedSize(88, 24);
      button->setToolTip(QStringLiteral("State %1").arg(i));
      ledStateColorButtons_[static_cast<std::size_t>(i)] = button;
      QObject::connect(button, &QPushButton::clicked, this,
          [this, i]() {
            openColorPalette(ledStateColorButtons_[static_cast<std::size_t>(i)],
                QStringLiteral("LED Monitor State %1").arg(i),
                [this, i](const QColor &color) {
                  const auto &setter =
                      ledStateColorSetters_[static_cast<std::size_t>(i)];
                  if (setter) {
                    setter(color);
                  }
                  refreshLedMonitorColorButtons();
                });
          });
      ledStateColorsLayout->addWidget(button, i / 4, i % 4);
    }

    ledChannelEdit_ = createLineEdit();
    committedTexts_.insert(ledChannelEdit_, ledChannelEdit_->text());
    ledChannelEdit_->installEventFilter(this);
    QObject::connect(ledChannelEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitLedChannel(); });
    QObject::connect(ledChannelEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitLedChannel(); });

    ledVisibilityCombo_ = new QComboBox;
    ledVisibilityCombo_->setFont(valueFont_);
    ledVisibilityCombo_->setAutoFillBackground(true);
    ledVisibilityCombo_->addItem(QStringLiteral("Static"));
    ledVisibilityCombo_->addItem(QStringLiteral("If Not Zero"));
    ledVisibilityCombo_->addItem(QStringLiteral("If Zero"));
    ledVisibilityCombo_->addItem(QStringLiteral("Calc"));
    QObject::connect(ledVisibilityCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (ledVisibilityModeSetter_) {
            ledVisibilityModeSetter_(visibilityModeFromIndex(index));
          }
          updateLedMonitorChannelDependentControls();
        });

    ledVisibilityCalcEdit_ = createLineEdit();
    committedTexts_.insert(ledVisibilityCalcEdit_,
        ledVisibilityCalcEdit_->text());
    ledVisibilityCalcEdit_->installEventFilter(this);
    QObject::connect(ledVisibilityCalcEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitLedVisibilityCalc(); });
    QObject::connect(ledVisibilityCalcEdit_, &QLineEdit::editingFinished, this,
        [this]() { commitLedVisibilityCalc(); });

    for (int i = 0; i < static_cast<int>(ledVisibilityChannelEdits_.size()); ++i) {
      auto *edit = createLineEdit();
      ledVisibilityChannelEdits_[static_cast<std::size_t>(i)] = edit;
      committedTexts_.insert(edit, edit->text());
      edit->installEventFilter(this);
      QObject::connect(edit, &QLineEdit::returnPressed, this,
          [this, i]() { commitLedVisibilityChannel(i); });
      QObject::connect(edit, &QLineEdit::editingFinished, this,
          [this, i]() { commitLedVisibilityChannel(i); });
    }

    addRow(ledLayout, 0, QStringLiteral("Foreground"), ledForegroundButton_);
    addRow(ledLayout, 1, QStringLiteral("Background"), ledBackgroundButton_);
    addRow(ledLayout, 2, QStringLiteral("On Color"), ledOnColorButton_);
    addRow(ledLayout, 3, QStringLiteral("Off Color"), ledOffColorButton_);
    addRow(ledLayout, 4, QStringLiteral("Undefined"), ledUndefinedColorButton_);
    addRow(ledLayout, 5, QStringLiteral("Color Mode"), ledColorModeCombo_);
    addRow(ledLayout, 6, QStringLiteral("Shape"), ledShapeCombo_);
    addRow(ledLayout, 7, QStringLiteral("Bezel"), ledBezelCheckBox_);
    addRow(ledLayout, 8, QStringLiteral("State Count"), ledStateCountSpin_);
    addRow(ledLayout, 9, QStringLiteral("State Colors"), ledStateColorsWidget_);
    addRow(ledLayout, 10, QStringLiteral("Channel"), ledChannelEdit_);
    addRow(ledLayout, 11, QStringLiteral("Visibility"), ledVisibilityCombo_);
    addRow(ledLayout, 12, QStringLiteral("Visibility Calc"),
        ledVisibilityCalcEdit_);
    addRow(ledLayout, 13, QStringLiteral("Vis Channel A"),
        ledVisibilityChannelEdits_[0]);
    addRow(ledLayout, 14, QStringLiteral("Vis Channel B"),
        ledVisibilityChannelEdits_[1]);
    addRow(ledLayout, 15, QStringLiteral("Vis Channel C"),
        ledVisibilityChannelEdits_[2]);
    addRow(ledLayout, 16, QStringLiteral("Vis Channel D"),
        ledVisibilityChannelEdits_[3]);
    ledLayout->setRowStretch(17, 1);
    entriesLayout->addWidget(ledSection_);

    expressionChannelSection_ = new QWidget(entriesWidget_);
    auto *expressionChannelLayout = new QGridLayout(expressionChannelSection_);
    expressionChannelLayout->setContentsMargins(0, 0, 0, 0);
    expressionChannelLayout->setHorizontalSpacing(12);
    expressionChannelLayout->setVerticalSpacing(6);

    expressionChannelForegroundButton_ = createColorButton(
        basePalette.color(QPalette::WindowText));
    QObject::connect(expressionChannelForegroundButton_,
        &QPushButton::clicked, this, [this]() {
          openColorPalette(expressionChannelForegroundButton_,
              QStringLiteral("Expression Channel Foreground"),
              expressionChannelForegroundSetter_);
        });

    expressionChannelBackgroundButton_ = createColorButton(
        basePalette.color(QPalette::Window));
    QObject::connect(expressionChannelBackgroundButton_,
        &QPushButton::clicked, this, [this]() {
          openColorPalette(expressionChannelBackgroundButton_,
              QStringLiteral("Expression Channel Background"),
              expressionChannelBackgroundSetter_);
        });

    expressionChannelVariableEdit_ = createLineEdit();
    expressionChannelVariableEdit_->setPlaceholderText(
        QStringLiteral("(auto-generated if empty)"));
    committedTexts_.insert(expressionChannelVariableEdit_,
        expressionChannelVariableEdit_->text());
    expressionChannelVariableEdit_->installEventFilter(this);
    QObject::connect(expressionChannelVariableEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitExpressionChannelVariable(); });
    QObject::connect(expressionChannelVariableEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitExpressionChannelVariable(); });

    expressionChannelCalcEdit_ = createLineEdit();
    expressionChannelCalcEdit_->setPlaceholderText(
        QStringLiteral("e.g. A+B"));
    committedTexts_.insert(expressionChannelCalcEdit_,
        expressionChannelCalcEdit_->text());
    expressionChannelCalcEdit_->installEventFilter(this);
    QObject::connect(expressionChannelCalcEdit_, &QLineEdit::returnPressed,
        this, [this]() { commitExpressionChannelCalc(); });
    QObject::connect(expressionChannelCalcEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitExpressionChannelCalc(); });

    for (int i = 0;
         i < static_cast<int>(expressionChannelChannelEdits_.size()); ++i) {
      expressionChannelChannelEdits_[i] = createLineEdit();
      committedTexts_.insert(expressionChannelChannelEdits_[i],
          expressionChannelChannelEdits_[i]->text());
      expressionChannelChannelEdits_[i]->installEventFilter(this);
      QObject::connect(expressionChannelChannelEdits_[i],
          &QLineEdit::returnPressed, this,
          [this, i]() { commitExpressionChannelChannel(i); });
      QObject::connect(expressionChannelChannelEdits_[i],
          &QLineEdit::editingFinished, this,
          [this, i]() { commitExpressionChannelChannel(i); });
    }

    expressionChannelInitialValueSpin_ = new QDoubleSpinBox;
    expressionChannelInitialValueSpin_->setFont(valueFont_);
    expressionChannelInitialValueSpin_->setAutoFillBackground(true);
    QPalette expressionSpinPalette = palette();
    expressionSpinPalette.setColor(QPalette::Base, Qt::white);
    expressionSpinPalette.setColor(QPalette::Text, Qt::black);
    expressionChannelInitialValueSpin_->setPalette(expressionSpinPalette);
    expressionChannelInitialValueSpin_->setRange(-1.0e12, 1.0e12);
    expressionChannelInitialValueSpin_->setDecimals(6);
    expressionChannelInitialValueSpin_->setSingleStep(0.1);
    expressionChannelInitialValueSpin_->setAccelerated(true);
    QObject::connect(expressionChannelInitialValueSpin_,
        static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
        this, [this](double value) {
          if (!expressionChannelInitialValueSetter_) {
            if (expressionChannelInitialValueGetter_) {
              const QSignalBlocker blocker(expressionChannelInitialValueSpin_);
              expressionChannelInitialValueSpin_->setValue(
                  expressionChannelInitialValueGetter_());
            }
            return;
          }
          expressionChannelInitialValueSetter_(value);
          if (expressionChannelInitialValueGetter_) {
            const QSignalBlocker blocker(expressionChannelInitialValueSpin_);
            expressionChannelInitialValueSpin_->setValue(
                expressionChannelInitialValueGetter_());
          }
        });

    expressionChannelEventSignalCombo_ = new QComboBox;
    expressionChannelEventSignalCombo_->setFont(valueFont_);
    expressionChannelEventSignalCombo_->setAutoFillBackground(true);
    expressionChannelEventSignalCombo_->addItem(QStringLiteral("Never"));
    expressionChannelEventSignalCombo_->addItem(
        QStringLiteral("On First Change"));
    expressionChannelEventSignalCombo_->addItem(
        QStringLiteral("On Any Change"));
    expressionChannelEventSignalCombo_->addItem(
        QStringLiteral("Trigger 0->1"));
    expressionChannelEventSignalCombo_->addItem(
        QStringLiteral("Trigger 1->0"));
    QObject::connect(expressionChannelEventSignalCombo_,
        static_cast<void (QComboBox::*)(int)>(
            &QComboBox::currentIndexChanged),
        this, [this](int index) {
          if (!expressionChannelEventSignalSetter_) {
            if (expressionChannelEventSignalGetter_) {
              const QSignalBlocker blocker(expressionChannelEventSignalCombo_);
              expressionChannelEventSignalCombo_->setCurrentIndex(
                  expressionChannelEventSignalToIndex(
                      expressionChannelEventSignalGetter_()));
            }
            return;
          }
          expressionChannelEventSignalSetter_(
              expressionChannelEventSignalFromIndex(index));
          if (expressionChannelEventSignalGetter_) {
            const QSignalBlocker blocker(expressionChannelEventSignalCombo_);
            expressionChannelEventSignalCombo_->setCurrentIndex(
                expressionChannelEventSignalToIndex(
                    expressionChannelEventSignalGetter_()));
          }
        });

    expressionChannelPrecisionSpin_ = new QSpinBox;
    expressionChannelPrecisionSpin_->setFont(valueFont_);
    expressionChannelPrecisionSpin_->setAutoFillBackground(true);
    expressionChannelPrecisionSpin_->setPalette(expressionSpinPalette);
    expressionChannelPrecisionSpin_->setRange(0, 17);
    expressionChannelPrecisionSpin_->setAccelerated(true);
    QObject::connect(expressionChannelPrecisionSpin_,
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [this](int value) {
          if (!expressionChannelPrecisionSetter_) {
            if (expressionChannelPrecisionGetter_) {
              const QSignalBlocker blocker(expressionChannelPrecisionSpin_);
              expressionChannelPrecisionSpin_->setValue(
                  expressionChannelPrecisionGetter_());
            }
            return;
          }
          expressionChannelPrecisionSetter_(value);
          if (expressionChannelPrecisionGetter_) {
            const QSignalBlocker blocker(expressionChannelPrecisionSpin_);
            expressionChannelPrecisionSpin_->setValue(
                expressionChannelPrecisionGetter_());
          }
        });

    int expressionChannelRow = 0;
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Foreground"), expressionChannelForegroundButton_);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Background"), expressionChannelBackgroundButton_);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Variable"), expressionChannelVariableEdit_);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Calc"), expressionChannelCalcEdit_);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Channel A"), expressionChannelChannelEdits_[0]);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Channel B"), expressionChannelChannelEdits_[1]);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Channel C"), expressionChannelChannelEdits_[2]);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Channel D"), expressionChannelChannelEdits_[3]);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Initial Value"), expressionChannelInitialValueSpin_);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Event Signal"), expressionChannelEventSignalCombo_);
    addRow(expressionChannelLayout, expressionChannelRow++,
        QStringLiteral("Precision"), expressionChannelPrecisionSpin_);
    expressionChannelLayout->setRowStretch(expressionChannelRow, 1);
    entriesLayout->addWidget(expressionChannelSection_);

    entriesLayout->addStretch(1);

  displaySection_->setVisible(false);
  rectangleSection_->setVisible(false);
  compositeSection_->setVisible(false);
  imageSection_->setVisible(false);
  heatmapSection_->setVisible(false);
  waterfallSection_->setVisible(false);
  lineSection_->setVisible(false);
  textSection_->setVisible(false);
  textEntrySection_->setVisible(false);
  setpointControlSection_->setVisible(false);
  textAreaSection_->setVisible(false);
  textMonitorSection_->setVisible(false);
  pvTableSection_->setVisible(false);
  waveTableSection_->setVisible(false);
  meterSection_->setVisible(false);
  barSection_->setVisible(false);
  thermometerSection_->setVisible(false);
  scaleSection_->setVisible(false);
  byteSection_->setVisible(false);
  ledSection_->setVisible(false);
  expressionChannelSection_->setVisible(false);
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

  void showForSetpointControl(std::function<QRect()> geometryGetter,
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
      std::function<QString()> setpointGetter,
      std::function<void(const QString &)> setpointSetter,
      std::function<QString()> readbackGetter,
      std::function<void(const QString &)> readbackSetter,
      std::function<QString()> labelGetter,
      std::function<void(const QString &)> labelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<SetpointToleranceMode()> toleranceModeGetter,
      std::function<void(SetpointToleranceMode)> toleranceModeSetter,
      std::function<double()> toleranceGetter,
      std::function<void(double)> toleranceSetter,
      std::function<bool()> showReadbackGetter,
      std::function<void(bool)> showReadbackSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kSetpointControl;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    setpointControlForegroundGetter_ = std::move(foregroundGetter);
    setpointControlForegroundSetter_ = std::move(foregroundSetter);
    setpointControlBackgroundGetter_ = std::move(backgroundGetter);
    setpointControlBackgroundSetter_ = std::move(backgroundSetter);
    setpointControlFormatGetter_ = std::move(formatGetter);
    setpointControlFormatSetter_ = std::move(formatSetter);
    setpointControlPrecisionGetter_ = std::move(precisionGetter);
    setpointControlPrecisionSetter_ = std::move(precisionSetter);
    setpointControlPrecisionSourceGetter_ = std::move(precisionSourceGetter);
    setpointControlPrecisionSourceSetter_ = std::move(precisionSourceSetter);
    setpointControlPrecisionDefaultGetter_ = std::move(precisionDefaultGetter);
    setpointControlPrecisionDefaultSetter_ = std::move(precisionDefaultSetter);
    setpointControlColorModeGetter_ = std::move(colorModeGetter);
    setpointControlColorModeSetter_ = std::move(colorModeSetter);
    setpointControlSetpointGetter_ = std::move(setpointGetter);
    setpointControlSetpointSetter_ = std::move(setpointSetter);
    setpointControlReadbackGetter_ = std::move(readbackGetter);
    setpointControlReadbackSetter_ = std::move(readbackSetter);
    setpointControlLabelGetter_ = std::move(labelGetter);
    setpointControlLabelSetter_ = std::move(labelSetter);
    setpointControlLimitsGetter_ = std::move(limitsGetter);
    setpointControlLimitsSetter_ = std::move(limitsSetter);
    setpointControlToleranceModeGetter_ = std::move(toleranceModeGetter);
    setpointControlToleranceModeSetter_ = std::move(toleranceModeSetter);
    setpointControlToleranceGetter_ = std::move(toleranceGetter);
    setpointControlToleranceSetter_ = std::move(toleranceSetter);
    setpointControlShowReadbackGetter_ = std::move(showReadbackGetter);
    setpointControlShowReadbackSetter_ = std::move(showReadbackSetter);

    QRect elementGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (elementGeometry.width() <= 0) {
      elementGeometry.setWidth(kMinimumTextWidth * 3);
    }
    if (elementGeometry.height() <= 0) {
      elementGeometry.setHeight(kMinimumTextHeight * 2);
    }
    lastCommittedGeometry_ = elementGeometry;
    updateGeometryEdits(elementGeometry);

    if (setpointControlForegroundButton_) {
      const QColor color = setpointControlForegroundGetter_
          ? setpointControlForegroundGetter_()
          : palette().color(QPalette::WindowText);
      setColorButtonColor(setpointControlForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }
    if (setpointControlBackgroundButton_) {
      const QColor color = setpointControlBackgroundGetter_
          ? setpointControlBackgroundGetter_()
          : palette().color(QPalette::Window);
      setColorButtonColor(setpointControlBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }
    if (setpointControlFormatCombo_) {
      const QSignalBlocker blocker(setpointControlFormatCombo_);
      const int index = setpointControlFormatGetter_
          ? textMonitorFormatToIndex(setpointControlFormatGetter_())
          : textMonitorFormatToIndex(TextMonitorFormat::kDecimal);
      setpointControlFormatCombo_->setCurrentIndex(index);
    }
    if (setpointControlColorModeCombo_) {
      const QSignalBlocker blocker(setpointControlColorModeCombo_);
      const int index = setpointControlColorModeGetter_
          ? colorModeToIndex(setpointControlColorModeGetter_())
          : colorModeToIndex(TextColorMode::kAlarm);
      setpointControlColorModeCombo_->setCurrentIndex(index);
    }
    if (setpointControlLabelEdit_) {
      const QString label = setpointControlLabelGetter_
          ? setpointControlLabelGetter_() : QString();
      const QSignalBlocker blocker(setpointControlLabelEdit_);
      setpointControlLabelEdit_->setText(label);
      committedTexts_[setpointControlLabelEdit_] =
          setpointControlLabelEdit_->text();
    }
    if (setpointControlSetpointEdit_) {
      const QString channel = setpointControlSetpointGetter_
          ? setpointControlSetpointGetter_() : QString();
      const QSignalBlocker blocker(setpointControlSetpointEdit_);
      setpointControlSetpointEdit_->setText(channel);
      committedTexts_[setpointControlSetpointEdit_] =
          setpointControlSetpointEdit_->text();
    }
    if (setpointControlReadbackEdit_) {
      const QString channel = setpointControlReadbackGetter_
          ? setpointControlReadbackGetter_() : QString();
      const QSignalBlocker blocker(setpointControlReadbackEdit_);
      setpointControlReadbackEdit_->setText(channel);
      committedTexts_[setpointControlReadbackEdit_] =
          setpointControlReadbackEdit_->text();
    }
    if (setpointControlToleranceModeCombo_) {
      const QSignalBlocker blocker(setpointControlToleranceModeCombo_);
      const SetpointToleranceMode mode = setpointControlToleranceModeGetter_
          ? setpointControlToleranceModeGetter_()
          : SetpointToleranceMode::kNone;
      setpointControlToleranceModeCombo_->setCurrentIndex(
          mode == SetpointToleranceMode::kAbsolute ? 1 : 0);
    }
    if (setpointControlToleranceEdit_) {
      const double tolerance = setpointControlToleranceGetter_
          ? setpointControlToleranceGetter_() : 0.0;
      const QSignalBlocker blocker(setpointControlToleranceEdit_);
      setpointControlToleranceEdit_->setText(
          QString::number(tolerance, 'g', 12));
      committedTexts_[setpointControlToleranceEdit_] =
          setpointControlToleranceEdit_->text();
    }
    if (setpointControlShowReadbackCombo_) {
      const QSignalBlocker blocker(setpointControlShowReadbackCombo_);
      const bool show = setpointControlShowReadbackGetter_
          ? setpointControlShowReadbackGetter_() : true;
      setpointControlShowReadbackCombo_->setCurrentIndex(show ? 1 : 0);
    }
    if (setpointControlPvLimitsButton_) {
      setpointControlPvLimitsButton_->setEnabled(
          static_cast<bool>(setpointControlPrecisionSourceGetter_)
          && static_cast<bool>(setpointControlPrecisionSourceSetter_)
          && static_cast<bool>(setpointControlPrecisionDefaultGetter_)
          && static_cast<bool>(setpointControlPrecisionDefaultSetter_)
          && static_cast<bool>(setpointControlLimitsGetter_)
          && static_cast<bool>(setpointControlLimitsSetter_));
    }
    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Setpoint Control"));
    }

    showPaletteWithoutActivating();
  }

  void showForTextArea(std::function<QRect()> geometryGetter,
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
      std::function<void(const PvLimits &)> limitsSetter,
      std::function<bool()> readOnlyGetter,
      std::function<void(bool)> readOnlySetter,
      std::function<bool()> wordWrapGetter,
      std::function<void(bool)> wordWrapSetter,
      std::function<TextAreaWrapMode()> wrapModeGetter,
      std::function<void(TextAreaWrapMode)> wrapModeSetter,
      std::function<int()> wrapColumnWidthGetter,
      std::function<void(int)> wrapColumnWidthSetter,
      std::function<bool()> showVerticalScrollBarGetter,
      std::function<void(bool)> showVerticalScrollBarSetter,
      std::function<bool()> showHorizontalScrollBarGetter,
      std::function<void(bool)> showHorizontalScrollBarSetter,
      std::function<TextAreaCommitMode()> commitModeGetter,
      std::function<void(TextAreaCommitMode)> commitModeSetter,
      std::function<bool()> tabInsertsSpacesGetter,
      std::function<void(bool)> tabInsertsSpacesSetter,
      std::function<int()> tabWidthGetter,
      std::function<void(int)> tabWidthSetter,
      std::function<QString()> fontFamilyGetter,
      std::function<void(const QString &)> fontFamilySetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kTextArea;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    textAreaForegroundGetter_ = std::move(foregroundGetter);
    textAreaForegroundSetter_ = std::move(foregroundSetter);
    textAreaBackgroundGetter_ = std::move(backgroundGetter);
    textAreaBackgroundSetter_ = std::move(backgroundSetter);
    textAreaFormatGetter_ = std::move(formatGetter);
    textAreaFormatSetter_ = std::move(formatSetter);
    textAreaPrecisionGetter_ = std::move(precisionGetter);
    textAreaPrecisionSetter_ = std::move(precisionSetter);
    textAreaPrecisionSourceGetter_ = std::move(precisionSourceGetter);
    textAreaPrecisionSourceSetter_ = std::move(precisionSourceSetter);
    textAreaPrecisionDefaultGetter_ = std::move(precisionDefaultGetter);
    textAreaPrecisionDefaultSetter_ = std::move(precisionDefaultSetter);
    textAreaColorModeGetter_ = std::move(colorModeGetter);
    textAreaColorModeSetter_ = std::move(colorModeSetter);
    textAreaChannelGetter_ = std::move(channelGetter);
    textAreaChannelSetter_ = std::move(channelSetter);
    textAreaLimitsGetter_ = std::move(limitsGetter);
    textAreaLimitsSetter_ = std::move(limitsSetter);
    textAreaReadOnlyGetter_ = std::move(readOnlyGetter);
    textAreaReadOnlySetter_ = std::move(readOnlySetter);
    textAreaWordWrapGetter_ = std::move(wordWrapGetter);
    textAreaWordWrapSetter_ = std::move(wordWrapSetter);
    textAreaWrapModeGetter_ = std::move(wrapModeGetter);
    textAreaWrapModeSetter_ = std::move(wrapModeSetter);
    textAreaWrapColumnWidthGetter_ = std::move(wrapColumnWidthGetter);
    textAreaWrapColumnWidthSetter_ = std::move(wrapColumnWidthSetter);
    textAreaShowVerticalScrollBarGetter_ =
        std::move(showVerticalScrollBarGetter);
    textAreaShowVerticalScrollBarSetter_ =
        std::move(showVerticalScrollBarSetter);
    textAreaShowHorizontalScrollBarGetter_ =
        std::move(showHorizontalScrollBarGetter);
    textAreaShowHorizontalScrollBarSetter_ =
        std::move(showHorizontalScrollBarSetter);
    textAreaCommitModeGetter_ = std::move(commitModeGetter);
    textAreaCommitModeSetter_ = std::move(commitModeSetter);
    textAreaTabInsertsSpacesGetter_ = std::move(tabInsertsSpacesGetter);
    textAreaTabInsertsSpacesSetter_ = std::move(tabInsertsSpacesSetter);
    textAreaTabWidthGetter_ = std::move(tabWidthGetter);
    textAreaTabWidthSetter_ = std::move(tabWidthSetter);
    textAreaFontFamilyGetter_ = std::move(fontFamilyGetter);
    textAreaFontFamilySetter_ = std::move(fontFamilySetter);

    QRect areaGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (areaGeometry.width() <= 0) {
      areaGeometry.setWidth(kMinimumTextWidth * 2);
    }
    if (areaGeometry.height() <= 0) {
      areaGeometry.setHeight(kMinimumTextHeight * 3);
    }
    lastCommittedGeometry_ = areaGeometry;
    updateGeometryEdits(areaGeometry);

    if (textAreaForegroundButton_) {
      const QColor color = textAreaForegroundGetter_
          ? textAreaForegroundGetter_()
          : palette().color(QPalette::WindowText);
      setColorButtonColor(textAreaForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (textAreaBackgroundButton_) {
      const QColor color = textAreaBackgroundGetter_
          ? textAreaBackgroundGetter_()
          : palette().color(QPalette::Window);
      setColorButtonColor(textAreaBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (textAreaFormatCombo_) {
      const QSignalBlocker blocker(textAreaFormatCombo_);
      const int index = textAreaFormatGetter_
          ? textMonitorFormatToIndex(textAreaFormatGetter_())
          : textMonitorFormatToIndex(TextMonitorFormat::kDecimal);
      textAreaFormatCombo_->setCurrentIndex(index);
    }

    if (textAreaColorModeCombo_) {
      const QSignalBlocker blocker(textAreaColorModeCombo_);
      const int index = textAreaColorModeGetter_
          ? colorModeToIndex(textAreaColorModeGetter_())
          : colorModeToIndex(TextColorMode::kStatic);
      textAreaColorModeCombo_->setCurrentIndex(index);
    }

    if (textAreaChannelEdit_) {
      const QString channel = textAreaChannelGetter_ ? textAreaChannelGetter_()
                                                     : QString();
      const QSignalBlocker blocker(textAreaChannelEdit_);
      textAreaChannelEdit_->setText(channel);
      committedTexts_[textAreaChannelEdit_] = textAreaChannelEdit_->text();
    }

    if (textAreaReadOnlyCombo_) {
      const QSignalBlocker blocker(textAreaReadOnlyCombo_);
      textAreaReadOnlyCombo_->setCurrentIndex(
          textAreaReadOnlyGetter_ && textAreaReadOnlyGetter_() ? 1 : 0);
    }

    if (textAreaWordWrapCombo_) {
      const QSignalBlocker blocker(textAreaWordWrapCombo_);
      textAreaWordWrapCombo_->setCurrentIndex(
          !textAreaWordWrapGetter_ || textAreaWordWrapGetter_() ? 1 : 0);
    }

    if (textAreaLineWrapModeCombo_) {
      const QSignalBlocker blocker(textAreaLineWrapModeCombo_);
      const int index = textAreaWrapModeGetter_
          ? textAreaWrapModeToIndex(textAreaWrapModeGetter_())
          : textAreaWrapModeToIndex(TextAreaWrapMode::kWidgetWidth);
      textAreaLineWrapModeCombo_->setCurrentIndex(index);
    }

    if (textAreaWrapColumnWidthEdit_) {
      const int width = textAreaWrapColumnWidthGetter_
          ? std::max(1, textAreaWrapColumnWidthGetter_())
          : 80;
      const QSignalBlocker blocker(textAreaWrapColumnWidthEdit_);
      textAreaWrapColumnWidthEdit_->setText(QString::number(width));
      committedTexts_[textAreaWrapColumnWidthEdit_]
          = textAreaWrapColumnWidthEdit_->text();
    }

    if (textAreaVerticalScrollBarCombo_) {
      const QSignalBlocker blocker(textAreaVerticalScrollBarCombo_);
      textAreaVerticalScrollBarCombo_->setCurrentIndex(
          !textAreaShowVerticalScrollBarGetter_
              || textAreaShowVerticalScrollBarGetter_()
          ? 1
          : 0);
    }

    if (textAreaHorizontalScrollBarCombo_) {
      const QSignalBlocker blocker(textAreaHorizontalScrollBarCombo_);
      textAreaHorizontalScrollBarCombo_->setCurrentIndex(
          textAreaShowHorizontalScrollBarGetter_
              && textAreaShowHorizontalScrollBarGetter_()
          ? 1
          : 0);
    }

    if (textAreaCommitModeCombo_) {
      const QSignalBlocker blocker(textAreaCommitModeCombo_);
      const int index = textAreaCommitModeGetter_
          ? textAreaCommitModeToIndex(textAreaCommitModeGetter_())
          : textAreaCommitModeToIndex(TextAreaCommitMode::kCtrlEnter);
      textAreaCommitModeCombo_->setCurrentIndex(index);
    }

    if (textAreaTabInsertsSpacesCombo_) {
      const QSignalBlocker blocker(textAreaTabInsertsSpacesCombo_);
      textAreaTabInsertsSpacesCombo_->setCurrentIndex(
          !textAreaTabInsertsSpacesGetter_
              || textAreaTabInsertsSpacesGetter_()
          ? 1
          : 0);
    }

    if (textAreaTabWidthEdit_) {
      const int width = textAreaTabWidthGetter_
          ? std::max(1, textAreaTabWidthGetter_())
          : 8;
      const QSignalBlocker blocker(textAreaTabWidthEdit_);
      textAreaTabWidthEdit_->setText(QString::number(width));
      committedTexts_[textAreaTabWidthEdit_] = textAreaTabWidthEdit_->text();
    }

    if (textAreaFontFamilyEdit_) {
      const QString family = textAreaFontFamilyGetter_
          ? textAreaFontFamilyGetter_()
          : QString();
      const QSignalBlocker blocker(textAreaFontFamilyEdit_);
      textAreaFontFamilyEdit_->setText(family);
      committedTexts_[textAreaFontFamilyEdit_] = textAreaFontFamilyEdit_->text();
    }

    if (textAreaPvLimitsButton_) {
      textAreaPvLimitsButton_->setEnabled(
          static_cast<bool>(textAreaPrecisionSourceGetter_)
          && static_cast<bool>(textAreaPrecisionSourceSetter_)
          && static_cast<bool>(textAreaPrecisionDefaultGetter_)
          && static_cast<bool>(textAreaPrecisionDefaultSetter_)
          && static_cast<bool>(textAreaLimitsGetter_)
          && static_cast<bool>(textAreaLimitsSetter_));
    }

    updateTextAreaDependentControls();

    if (elementLabel_) {
      elementLabel_->setText(QStringLiteral("Text Area"));
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
    waveTableForegroundGetter_ = {};
    waveTableForegroundSetter_ = {};
    waveTableBackgroundGetter_ = {};
    waveTableBackgroundSetter_ = {};
    waveTableColorModeGetter_ = {};
    waveTableColorModeSetter_ = {};
    waveTableShowHeadersGetter_ = {};
    waveTableShowHeadersSetter_ = {};
    waveTableChannelGetter_ = {};
    waveTableChannelSetter_ = {};
    waveTableLayoutGetter_ = {};
    waveTableLayoutSetter_ = {};
    waveTableColumnsGetter_ = {};
    waveTableColumnsSetter_ = {};
    waveTableMaxElementsGetter_ = {};
    waveTableMaxElementsSetter_ = {};
    waveTableIndexBaseGetter_ = {};
    waveTableIndexBaseSetter_ = {};
    waveTableValueFormatGetter_ = {};
    waveTableValueFormatSetter_ = {};
    waveTableCharModeGetter_ = {};
    waveTableCharModeSetter_ = {};
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
    cartesianDrawMajorGetter_ = {};
    cartesianDrawMajorSetter_ = {};
    cartesianDrawMinorGetter_ = {};
    cartesianDrawMinorSetter_ = {};
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

  void showForPvTable(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<bool()> showHeadersGetter,
      std::function<void(bool)> showHeadersSetter,
      std::function<QString()> columnsGetter,
      std::function<void(const QString &)> columnsSetter,
      std::array<std::function<QString()>, kPvTableRowCount> rowLabelGetters,
      std::array<std::function<void(const QString &)>, kPvTableRowCount> rowLabelSetters,
      std::array<std::function<QString()>, kPvTableRowCount> rowChannelGetters,
      std::array<std::function<void(const QString &)>, kPvTableRowCount> rowChannelSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kPvTable;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    pvTableForegroundGetter_ = std::move(foregroundGetter);
    pvTableForegroundSetter_ = std::move(foregroundSetter);
    pvTableBackgroundGetter_ = std::move(backgroundGetter);
    pvTableBackgroundSetter_ = std::move(backgroundSetter);
    pvTableColorModeGetter_ = std::move(colorModeGetter);
    pvTableColorModeSetter_ = std::move(colorModeSetter);
    pvTableShowHeadersGetter_ = std::move(showHeadersGetter);
    pvTableShowHeadersSetter_ = std::move(showHeadersSetter);
    pvTableColumnsGetter_ = std::move(columnsGetter);
    pvTableColumnsSetter_ = std::move(columnsSetter);
    pvTableRowLabelGetters_ = std::move(rowLabelGetters);
    pvTableRowLabelSetters_ = std::move(rowLabelSetters);
    pvTableRowChannelGetters_ = std::move(rowChannelGetters);
    pvTableRowChannelSetters_ = std::move(rowChannelSetters);

    QRect geometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (geometry.width() <= 0) {
      geometry.setWidth(kMinimumTextWidth * 4);
    }
    if (geometry.height() <= 0) {
      geometry.setHeight(kMinimumTextHeight * 4);
    }
    lastCommittedGeometry_ = geometry;
    updateGeometryEdits(geometry);

    if (pvTableForegroundButton_) {
      setColorButtonColor(pvTableForegroundButton_,
          pvTableForegroundGetter_ ? pvTableForegroundGetter_()
                                   : palette().color(QPalette::WindowText));
    }
    if (pvTableBackgroundButton_) {
      setColorButtonColor(pvTableBackgroundButton_,
          pvTableBackgroundGetter_ ? pvTableBackgroundGetter_()
                                   : palette().color(QPalette::Window));
    }
    if (pvTableColorModeCombo_) {
      const QSignalBlocker blocker(pvTableColorModeCombo_);
      pvTableColorModeCombo_->setCurrentIndex(colorModeToIndex(
          pvTableColorModeGetter_ ? pvTableColorModeGetter_()
                                  : TextColorMode::kAlarm));
    }
    if (pvTableShowHeadersCombo_) {
      const QSignalBlocker blocker(pvTableShowHeadersCombo_);
      pvTableShowHeadersCombo_->setCurrentIndex(
          (pvTableShowHeadersGetter_ && pvTableShowHeadersGetter_()) ? 1 : 0);
    }
    if (pvTableColumnsEdit_) {
      const QString value = pvTableColumnsGetter_ ? pvTableColumnsGetter_() : QString();
      const QSignalBlocker blocker(pvTableColumnsEdit_);
      pvTableColumnsEdit_->setText(value);
      committedTexts_[pvTableColumnsEdit_] = value;
    }
    for (int i = 0; i < kPvTableRowCount; ++i) {
      if (pvTableRowLabelEdits_[i]) {
        const QString value = pvTableRowLabelGetters_[i] ? pvTableRowLabelGetters_[i]() : QString();
        const QSignalBlocker blocker(pvTableRowLabelEdits_[i]);
        pvTableRowLabelEdits_[i]->setText(value);
        committedTexts_[pvTableRowLabelEdits_[i]] = value;
      }
      if (pvTableRowChannelEdits_[i]) {
        const QString value = pvTableRowChannelGetters_[i] ? pvTableRowChannelGetters_[i]() : QString();
        const QSignalBlocker blocker(pvTableRowChannelEdits_[i]);
        pvTableRowChannelEdits_[i]->setText(value);
        committedTexts_[pvTableRowChannelEdits_[i]] = value;
      }
    }
    elementLabel_->setText(QStringLiteral("PV Table"));
    showPaletteWithoutActivating();
  }

  void showForWaveTable(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<bool()> showHeadersGetter,
      std::function<void(bool)> showHeadersSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<WaveTableLayout()> layoutGetter,
      std::function<void(WaveTableLayout)> layoutSetter,
      std::function<int()> columnsGetter,
      std::function<void(int)> columnsSetter,
      std::function<int()> maxElementsGetter,
      std::function<void(int)> maxElementsSetter,
      std::function<int()> indexBaseGetter,
      std::function<void(int)> indexBaseSetter,
      std::function<WaveTableValueFormat()> valueFormatGetter,
      std::function<void(WaveTableValueFormat)> valueFormatSetter,
      std::function<WaveTableCharMode()> charModeGetter,
      std::function<void(WaveTableCharMode)> charModeSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kWaveTable;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    waveTableForegroundGetter_ = std::move(foregroundGetter);
    waveTableForegroundSetter_ = std::move(foregroundSetter);
    waveTableBackgroundGetter_ = std::move(backgroundGetter);
    waveTableBackgroundSetter_ = std::move(backgroundSetter);
    waveTableColorModeGetter_ = std::move(colorModeGetter);
    waveTableColorModeSetter_ = std::move(colorModeSetter);
    waveTableShowHeadersGetter_ = std::move(showHeadersGetter);
    waveTableShowHeadersSetter_ = std::move(showHeadersSetter);
    waveTableChannelGetter_ = std::move(channelGetter);
    waveTableChannelSetter_ = std::move(channelSetter);
    waveTableLayoutGetter_ = std::move(layoutGetter);
    waveTableLayoutSetter_ = std::move(layoutSetter);
    waveTableColumnsGetter_ = std::move(columnsGetter);
    waveTableColumnsSetter_ = std::move(columnsSetter);
    waveTableMaxElementsGetter_ = std::move(maxElementsGetter);
    waveTableMaxElementsSetter_ = std::move(maxElementsSetter);
    waveTableIndexBaseGetter_ = std::move(indexBaseGetter);
    waveTableIndexBaseSetter_ = std::move(indexBaseSetter);
    waveTableValueFormatGetter_ = std::move(valueFormatGetter);
    waveTableValueFormatSetter_ = std::move(valueFormatSetter);
    waveTableCharModeGetter_ = std::move(charModeGetter);
    waveTableCharModeSetter_ = std::move(charModeSetter);

    QRect geometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (geometry.width() <= 0) {
      geometry.setWidth(kMinimumTextWidth * 4);
    }
    if (geometry.height() <= 0) {
      geometry.setHeight(kMinimumTextHeight * 4);
    }
    lastCommittedGeometry_ = geometry;
    updateGeometryEdits(geometry);

    if (waveTableForegroundButton_) {
      setColorButtonColor(waveTableForegroundButton_,
          waveTableForegroundGetter_ ? waveTableForegroundGetter_()
                                     : palette().color(QPalette::WindowText));
    }
    if (waveTableBackgroundButton_) {
      setColorButtonColor(waveTableBackgroundButton_,
          waveTableBackgroundGetter_ ? waveTableBackgroundGetter_()
                                     : palette().color(QPalette::Window));
    }
    if (waveTableColorModeCombo_) {
      const QSignalBlocker blocker(waveTableColorModeCombo_);
      waveTableColorModeCombo_->setCurrentIndex(colorModeToIndex(
          waveTableColorModeGetter_ ? waveTableColorModeGetter_()
                                    : TextColorMode::kAlarm));
    }
    if (waveTableShowHeadersCombo_) {
      const QSignalBlocker blocker(waveTableShowHeadersCombo_);
      waveTableShowHeadersCombo_->setCurrentIndex(
          (waveTableShowHeadersGetter_ && waveTableShowHeadersGetter_())
              ? 1
              : 0);
    }
    if (waveTableChannelEdit_) {
      const QString value =
          waveTableChannelGetter_ ? waveTableChannelGetter_() : QString();
      const QSignalBlocker blocker(waveTableChannelEdit_);
      waveTableChannelEdit_->setText(value);
      committedTexts_[waveTableChannelEdit_] = value;
    }
    if (waveTableLayoutCombo_) {
      const QSignalBlocker blocker(waveTableLayoutCombo_);
      waveTableLayoutCombo_->setCurrentIndex(waveTableLayoutToIndex(
          waveTableLayoutGetter_ ? waveTableLayoutGetter_()
                                 : WaveTableLayout::kGrid));
    }
    if (waveTableColumnsEdit_) {
      const QString value = QString::number(
          waveTableColumnsGetter_ ? std::max(1, waveTableColumnsGetter_()) : 8);
      const QSignalBlocker blocker(waveTableColumnsEdit_);
      waveTableColumnsEdit_->setText(value);
      committedTexts_[waveTableColumnsEdit_] = value;
    }
    if (waveTableMaxElementsEdit_) {
      const QString value = QString::number(
          waveTableMaxElementsGetter_
              ? std::max(0, waveTableMaxElementsGetter_())
              : 0);
      const QSignalBlocker blocker(waveTableMaxElementsEdit_);
      waveTableMaxElementsEdit_->setText(value);
      committedTexts_[waveTableMaxElementsEdit_] = value;
    }
    if (waveTableIndexBaseCombo_) {
      const QSignalBlocker blocker(waveTableIndexBaseCombo_);
      const int value =
          waveTableIndexBaseGetter_ ? waveTableIndexBaseGetter_() : 0;
      waveTableIndexBaseCombo_->setCurrentIndex(value == 1 ? 1 : 0);
    }
    if (waveTableValueFormatCombo_) {
      const QSignalBlocker blocker(waveTableValueFormatCombo_);
      waveTableValueFormatCombo_->setCurrentIndex(
          waveTableValueFormatToIndex(waveTableValueFormatGetter_
                  ? waveTableValueFormatGetter_()
                  : WaveTableValueFormat::kDefault));
    }
    if (waveTableCharModeCombo_) {
      const QSignalBlocker blocker(waveTableCharModeCombo_);
      waveTableCharModeCombo_->setCurrentIndex(waveTableCharModeToIndex(
          waveTableCharModeGetter_ ? waveTableCharModeGetter_()
                                   : WaveTableCharMode::kString));
    }
    elementLabel_->setText(QStringLiteral("Waveform Table"));
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

  void showForThermometer(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QColor()> textGetter,
      std::function<void(const QColor &)> textSetter,
      std::function<MeterLabel()> labelGetter,
      std::function<void(MeterLabel)> labelSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> visibilityChannelGetters,
      std::array<std::function<void(const QString &)>, 4> visibilityChannelSetters,
      std::function<TextMonitorFormat()> formatGetter,
      std::function<void(TextMonitorFormat)> formatSetter,
      std::function<bool()> showValueGetter,
      std::function<void(bool)> showValueSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<PvLimits()> limitsGetter,
      std::function<void(const PvLimits &)> limitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kThermometer;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);

    thermometerForegroundGetter_ = std::move(foregroundGetter);
    thermometerForegroundSetter_ = std::move(foregroundSetter);
    thermometerBackgroundGetter_ = std::move(backgroundGetter);
    thermometerBackgroundSetter_ = std::move(backgroundSetter);
    thermometerTextGetter_ = std::move(textGetter);
    thermometerTextSetter_ = std::move(textSetter);
    thermometerLabelGetter_ = std::move(labelGetter);
    thermometerLabelSetter_ = std::move(labelSetter);
    thermometerColorModeGetter_ = std::move(colorModeGetter);
    thermometerColorModeSetter_ = std::move(colorModeSetter);
    thermometerVisibilityModeGetter_ = std::move(visibilityModeGetter);
    thermometerVisibilityModeSetter_ = std::move(visibilityModeSetter);
    thermometerVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    thermometerVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    thermometerVisibilityChannelGetters_ = std::move(visibilityChannelGetters);
    thermometerVisibilityChannelSetters_ = std::move(visibilityChannelSetters);
    thermometerFormatGetter_ = std::move(formatGetter);
    thermometerFormatSetter_ = std::move(formatSetter);
    thermometerShowValueGetter_ = std::move(showValueGetter);
    thermometerShowValueSetter_ = std::move(showValueSetter);
    thermometerChannelGetter_ = std::move(channelGetter);
    thermometerChannelSetter_ = std::move(channelSetter);
    thermometerLimitsGetter_ = std::move(limitsGetter);
    thermometerLimitsSetter_ = std::move(limitsSetter);

    QRect thermometerGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (thermometerGeometry.width() <= 0) {
      thermometerGeometry.setWidth(kMinimumThermometerSize);
    }
    if (thermometerGeometry.height() <= 0) {
      thermometerGeometry.setHeight(kMinimumThermometerSize);
    }
    lastCommittedGeometry_ = thermometerGeometry;
    updateGeometryEdits(thermometerGeometry);

    if (thermometerForegroundButton_) {
      const QColor color = thermometerForegroundGetter_
              ? thermometerForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(thermometerForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (thermometerBackgroundButton_) {
      const QColor color = thermometerBackgroundGetter_
              ? thermometerBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(thermometerBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
    }

    if (thermometerTextButton_) {
      QColor color = thermometerTextGetter_ ? thermometerTextGetter_() : QColor();
      if (!color.isValid()) {
        color = thermometerForegroundGetter_ ? thermometerForegroundGetter_()
                                            : palette().color(QPalette::WindowText);
      }
      setColorButtonColor(thermometerTextButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
    }

    if (thermometerLabelCombo_) {
      const QSignalBlocker blocker(thermometerLabelCombo_);
      const MeterLabel label = thermometerLabelGetter_ ? thermometerLabelGetter_()
                                                       : MeterLabel::kNone;
      thermometerLabelCombo_->setCurrentIndex(meterLabelToIndex(label));
    }

    if (thermometerColorModeCombo_) {
      const QSignalBlocker blocker(thermometerColorModeCombo_);
      const TextColorMode mode = thermometerColorModeGetter_
              ? thermometerColorModeGetter_()
              : TextColorMode::kStatic;
      thermometerColorModeCombo_->setCurrentIndex(colorModeToIndex(mode));
    }

    if (thermometerVisibilityCombo_) {
      const QSignalBlocker blocker(thermometerVisibilityCombo_);
      const TextVisibilityMode mode = thermometerVisibilityModeGetter_
              ? thermometerVisibilityModeGetter_()
              : TextVisibilityMode::kStatic;
      thermometerVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
    }

    if (thermometerVisibilityCalcEdit_) {
      const QString calc = thermometerVisibilityCalcGetter_
              ? thermometerVisibilityCalcGetter_()
              : QString();
      const QSignalBlocker blocker(thermometerVisibilityCalcEdit_);
      thermometerVisibilityCalcEdit_->setText(calc);
      committedTexts_[thermometerVisibilityCalcEdit_]
          = thermometerVisibilityCalcEdit_->text();
    }

    if (thermometerFormatCombo_) {
      const QSignalBlocker blocker(thermometerFormatCombo_);
      const TextMonitorFormat format = thermometerFormatGetter_
              ? thermometerFormatGetter_()
              : TextMonitorFormat::kDecimal;
      int index = textMonitorFormatToIndex(format);
      if (index < 0 || index >= thermometerFormatCombo_->count()) {
        index = textMonitorFormatToIndex(TextMonitorFormat::kDecimal);
      }
      thermometerFormatCombo_->setCurrentIndex(index);
    }

    if (thermometerShowValueCombo_) {
      const QSignalBlocker blocker(thermometerShowValueCombo_);
      const bool showValue = thermometerShowValueGetter_
          ? thermometerShowValueGetter_()
          : false;
      thermometerShowValueCombo_->setCurrentIndex(showValue ? 1 : 0);
    }

    if (thermometerChannelEdit_) {
      const QString channel = thermometerChannelGetter_
          ? thermometerChannelGetter_()
          : QString();
      const QSignalBlocker blocker(thermometerChannelEdit_);
      thermometerChannelEdit_->setText(channel);
      committedTexts_[thermometerChannelEdit_] = channel;
    }

    for (int i = 0;
         i < static_cast<int>(thermometerVisibilityChannelEdits_.size()); ++i) {
      QLineEdit *edit = thermometerVisibilityChannelEdits_[i];
      if (!edit) {
        continue;
      }
      const QString value = thermometerVisibilityChannelGetters_[i]
          ? thermometerVisibilityChannelGetters_[i]()
          : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      committedTexts_[edit] = edit->text();
    }

    updateThermometerChannelDependentControls();
    updateThermometerLimitsFromDialog();

    if (thermometerPvLimitsButton_) {
      thermometerPvLimitsButton_->setEnabled(
          static_cast<bool>(thermometerLimitsSetter_));
    }

    elementLabel_->setText(QStringLiteral("Thermometer"));

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
      std::function<bool()> drawMajorGetter,
      std::function<void(bool)> drawMajorSetter,
      std::function<bool()> drawMinorGetter,
      std::function<void(bool)> drawMinorSetter,
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
    cartesianDrawMajorGetter_ = std::move(drawMajorGetter);
    cartesianDrawMajorSetter_ = std::move(drawMajorSetter);
    cartesianDrawMinorGetter_ = std::move(drawMinorGetter);
    cartesianDrawMinorSetter_ = std::move(drawMinorSetter);
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

    if (cartesianDrawMajorCombo_) {
      const QSignalBlocker blocker(cartesianDrawMajorCombo_);
      const bool drawMajor = cartesianDrawMajorGetter_
              ? cartesianDrawMajorGetter_()
              : true;
      cartesianDrawMajorCombo_->setCurrentIndex(drawMajor ? 1 : 0);
      cartesianDrawMajorCombo_->setEnabled(static_cast<bool>(cartesianDrawMajorSetter_));
    }

    if (cartesianDrawMinorCombo_) {
      const QSignalBlocker blocker(cartesianDrawMinorCombo_);
      const bool drawMinor = cartesianDrawMinorGetter_
              ? cartesianDrawMinorGetter_()
              : false;
      cartesianDrawMinorCombo_->setCurrentIndex(drawMinor ? 1 : 0);
      cartesianDrawMinorCombo_->setEnabled(static_cast<bool>(cartesianDrawMinorSetter_));
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
      cartesianCountEdit_->setText(QString::number(std::max(countValue, 0)));
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

  void showForLedMonitor(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<TextColorMode()> colorModeGetter,
      std::function<void(TextColorMode)> colorModeSetter,
      std::function<LedShape()> shapeGetter,
      std::function<void(LedShape)> shapeSetter,
      std::function<bool()> bezelGetter,
      std::function<void(bool)> bezelSetter,
      std::function<QColor()> onColorGetter,
      std::function<void(const QColor &)> onColorSetter,
      std::function<QColor()> offColorGetter,
      std::function<void(const QColor &)> offColorSetter,
      std::function<QColor()> undefinedColorGetter,
      std::function<void(const QColor &)> undefinedColorSetter,
      std::array<std::function<QColor()>, kLedStateCount> stateColorGetters,
      std::array<std::function<void(const QColor &)>, kLedStateCount> stateColorSetters,
      std::function<int()> stateCountGetter,
      std::function<void(int)> stateCountSetter,
      std::function<QString()> channelGetter,
      std::function<void(const QString &)> channelSetter,
      std::function<TextVisibilityMode()> visibilityModeGetter,
      std::function<void(TextVisibilityMode)> visibilityModeSetter,
      std::function<QString()> visibilityCalcGetter,
      std::function<void(const QString &)> visibilityCalcSetter,
      std::array<std::function<QString()>, 4> visibilityChannelGetters,
      std::array<std::function<void(const QString &)>, 4> visibilityChannelSetters)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kLedMonitor;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    ledForegroundGetter_ = std::move(foregroundGetter);
    ledForegroundSetter_ = std::move(foregroundSetter);
    ledBackgroundGetter_ = std::move(backgroundGetter);
    ledBackgroundSetter_ = std::move(backgroundSetter);
    ledColorModeGetter_ = std::move(colorModeGetter);
    ledColorModeSetter_ = std::move(colorModeSetter);
    ledShapeGetter_ = std::move(shapeGetter);
    ledShapeSetter_ = std::move(shapeSetter);
    ledBezelGetter_ = std::move(bezelGetter);
    ledBezelSetter_ = std::move(bezelSetter);
    ledOnColorGetter_ = std::move(onColorGetter);
    ledOnColorSetter_ = std::move(onColorSetter);
    ledOffColorGetter_ = std::move(offColorGetter);
    ledOffColorSetter_ = std::move(offColorSetter);
    ledUndefinedColorGetter_ = std::move(undefinedColorGetter);
    ledUndefinedColorSetter_ = std::move(undefinedColorSetter);
    ledStateColorGetters_ = std::move(stateColorGetters);
    ledStateColorSetters_ = std::move(stateColorSetters);
    ledStateCountGetter_ = std::move(stateCountGetter);
    ledStateCountSetter_ = std::move(stateCountSetter);
    ledChannelGetter_ = std::move(channelGetter);
    ledChannelSetter_ = std::move(channelSetter);
    ledVisibilityModeGetter_ = std::move(visibilityModeGetter);
    ledVisibilityModeSetter_ = std::move(visibilityModeSetter);
    ledVisibilityCalcGetter_ = std::move(visibilityCalcGetter);
    ledVisibilityCalcSetter_ = std::move(visibilityCalcSetter);
    ledVisibilityChannelGetters_ = std::move(visibilityChannelGetters);
    ledVisibilityChannelSetters_ = std::move(visibilityChannelSetters);

    QRect ledGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (ledGeometry.width() <= 0) {
      ledGeometry.setWidth(kDefaultLedSize);
    }
    if (ledGeometry.height() <= 0) {
      ledGeometry.setHeight(kDefaultLedSize);
    }
    lastCommittedGeometry_ = ledGeometry;
    updateGeometryEdits(ledGeometry);

    if (ledForegroundButton_) {
      const QColor color = ledForegroundGetter_ ? ledForegroundGetter_()
                                                : palette().color(QPalette::WindowText);
      setColorButtonColor(ledForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      ledForegroundButton_->setEnabled(static_cast<bool>(ledForegroundSetter_));
    }

    if (ledBackgroundButton_) {
      const QColor color = ledBackgroundGetter_ ? ledBackgroundGetter_()
                                                : palette().color(QPalette::Window);
      setColorButtonColor(ledBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      ledBackgroundButton_->setEnabled(static_cast<bool>(ledBackgroundSetter_));
    }

    refreshLedMonitorColorButtons();

    if (ledOnColorButton_) {
      ledOnColorButton_->setEnabled(static_cast<bool>(ledOnColorSetter_));
    }

    if (ledOffColorButton_) {
      ledOffColorButton_->setEnabled(static_cast<bool>(ledOffColorSetter_));
    }

    if (ledUndefinedColorButton_) {
      const QColor color = ledUndefinedColorGetter_ ? ledUndefinedColorGetter_()
                                                    : QColor(204, 204, 204);
      setColorButtonColor(ledUndefinedColorButton_,
          color.isValid() ? color : QColor(204, 204, 204));
      ledUndefinedColorButton_->setEnabled(
          static_cast<bool>(ledUndefinedColorSetter_));
    }

    if (ledColorModeCombo_) {
      const QSignalBlocker blocker(ledColorModeCombo_);
      const TextColorMode mode = ledColorModeGetter_ ? ledColorModeGetter_()
                                                     : TextColorMode::kAlarm;
      const int stateCount = ledStateCountGetter_ ? ledStateCountGetter_() : 2;
      ledColorModeCombo_->setCurrentIndex(ledColorModeChoiceToIndex(
          ledColorModeChoiceForState(mode, stateCount)));
      ledColorModeCombo_->setEnabled(static_cast<bool>(ledColorModeSetter_));
    }

    if (ledShapeCombo_) {
      const QSignalBlocker blocker(ledShapeCombo_);
      const LedShape shape = ledShapeGetter_ ? ledShapeGetter_()
                                             : LedShape::kCircle;
      ledShapeCombo_->setCurrentIndex(ledShapeToIndex(shape));
      ledShapeCombo_->setEnabled(static_cast<bool>(ledShapeSetter_));
    }

    if (ledBezelCheckBox_) {
      const QSignalBlocker blocker(ledBezelCheckBox_);
      ledBezelCheckBox_->setChecked(ledBezelGetter_ ? ledBezelGetter_() : true);
      ledBezelCheckBox_->setEnabled(static_cast<bool>(ledBezelSetter_));
    }

    if (ledStateCountSpin_) {
      const QSignalBlocker blocker(ledStateCountSpin_);
      int count = ledStateCountGetter_ ? ledStateCountGetter_() : 2;
      count = std::clamp(count, 1, kLedStateCount);
      ledStateCountSpin_->setValue(count);
      ledStateCountSpin_->setEnabled(static_cast<bool>(ledStateCountSetter_));
    }

    if (ledChannelEdit_) {
      const QString channel = ledChannelGetter_ ? ledChannelGetter_()
                                                : QString();
      const QSignalBlocker blocker(ledChannelEdit_);
      ledChannelEdit_->setText(channel);
      committedTexts_[ledChannelEdit_] = ledChannelEdit_->text();
      ledChannelEdit_->setEnabled(static_cast<bool>(ledChannelSetter_));
    }

    if (ledVisibilityCombo_) {
      const QSignalBlocker blocker(ledVisibilityCombo_);
      const TextVisibilityMode mode = ledVisibilityModeGetter_
              ? ledVisibilityModeGetter_()
              : TextVisibilityMode::kStatic;
      ledVisibilityCombo_->setCurrentIndex(visibilityModeToIndex(mode));
    }

    if (ledVisibilityCalcEdit_) {
      const QString calc = ledVisibilityCalcGetter_ ? ledVisibilityCalcGetter_()
                                                    : QString();
      const QSignalBlocker blocker(ledVisibilityCalcEdit_);
      ledVisibilityCalcEdit_->setText(calc);
      committedTexts_[ledVisibilityCalcEdit_] = ledVisibilityCalcEdit_->text();
    }

    for (int i = 0; i < static_cast<int>(ledVisibilityChannelEdits_.size()); ++i) {
      QLineEdit *edit = ledVisibilityChannelEdits_[static_cast<std::size_t>(i)];
      if (!edit) {
        continue;
      }
      const QString channel = ledVisibilityChannelGetters_[static_cast<std::size_t>(i)]
              ? ledVisibilityChannelGetters_[static_cast<std::size_t>(i)]()
              : QString();
      const QSignalBlocker blocker(edit);
      edit->setText(channel);
      committedTexts_[edit] = edit->text();
      edit->setEnabled(static_cast<bool>(
          ledVisibilityChannelSetters_[static_cast<std::size_t>(i)]));
    }

    updateLedMonitorStateColorControls();
    updateLedMonitorChannelDependentControls();

    elementLabel_->setText(QStringLiteral("LED Monitor"));

    showPaletteWithoutActivating();
  }

  void showForExpressionChannel(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> variableGetter,
      std::function<void(const QString &)> variableSetter,
      std::function<QString()> calcGetter,
      std::function<void(const QString &)> calcSetter,
      std::array<std::function<QString()>, 4> channelGetters,
      std::array<std::function<void(const QString &)>, 4> channelSetters,
      std::function<double()> initialValueGetter,
      std::function<void(double)> initialValueSetter,
      std::function<ExpressionChannelEventSignalMode()> eventSignalGetter,
      std::function<void(ExpressionChannelEventSignalMode)> eventSignalSetter,
      std::function<int()> precisionGetter,
      std::function<void(int)> precisionSetter,
      const QString &elementLabel = QStringLiteral("Expression Channel"))
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kExpressionChannel;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    expressionChannelForegroundGetter_ = std::move(foregroundGetter);
    expressionChannelForegroundSetter_ = std::move(foregroundSetter);
    expressionChannelBackgroundGetter_ = std::move(backgroundGetter);
    expressionChannelBackgroundSetter_ = std::move(backgroundSetter);
    expressionChannelVariableGetter_ = std::move(variableGetter);
    expressionChannelVariableSetter_ = std::move(variableSetter);
    expressionChannelCalcGetter_ = std::move(calcGetter);
    expressionChannelCalcSetter_ = std::move(calcSetter);
    expressionChannelChannelGetters_ = std::move(channelGetters);
    expressionChannelChannelSetters_ = std::move(channelSetters);
    expressionChannelInitialValueGetter_ = std::move(initialValueGetter);
    expressionChannelInitialValueSetter_ = std::move(initialValueSetter);
    expressionChannelEventSignalGetter_ = std::move(eventSignalGetter);
    expressionChannelEventSignalSetter_ = std::move(eventSignalSetter);
    expressionChannelPrecisionGetter_ = std::move(precisionGetter);
    expressionChannelPrecisionSetter_ = std::move(precisionSetter);

    QRect geometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (geometry.width() <= 0) {
      geometry.setWidth(kMinimumExpressionChannelWidth);
    }
    if (geometry.height() <= 0) {
      geometry.setHeight(kMinimumExpressionChannelHeight);
    }
    lastCommittedGeometry_ = geometry;
    updateGeometryEdits(geometry);

    if (expressionChannelForegroundButton_) {
      const QColor color = expressionChannelForegroundGetter_
              ? expressionChannelForegroundGetter_()
              : palette().color(QPalette::WindowText);
      setColorButtonColor(expressionChannelForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      expressionChannelForegroundButton_->setEnabled(
          static_cast<bool>(expressionChannelForegroundSetter_));
    }

    if (expressionChannelBackgroundButton_) {
      const QColor color = expressionChannelBackgroundGetter_
              ? expressionChannelBackgroundGetter_()
              : palette().color(QPalette::Window);
      setColorButtonColor(expressionChannelBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      expressionChannelBackgroundButton_->setEnabled(
          static_cast<bool>(expressionChannelBackgroundSetter_));
    }

    if (expressionChannelVariableEdit_) {
      const QString value = expressionChannelVariableGetter_
              ? expressionChannelVariableGetter_()
              : QString();
      const QSignalBlocker blocker(expressionChannelVariableEdit_);
      expressionChannelVariableEdit_->setText(value);
      expressionChannelVariableEdit_->setEnabled(
          static_cast<bool>(expressionChannelVariableSetter_));
      committedTexts_[expressionChannelVariableEdit_] = value;
    }

    if (expressionChannelCalcEdit_) {
      const QString value = expressionChannelCalcGetter_
              ? expressionChannelCalcGetter_()
              : QString();
      const QSignalBlocker blocker(expressionChannelCalcEdit_);
      expressionChannelCalcEdit_->setText(value);
      expressionChannelCalcEdit_->setEnabled(
          static_cast<bool>(expressionChannelCalcSetter_));
      committedTexts_[expressionChannelCalcEdit_] = value;
    }

    for (int i = 0;
         i < static_cast<int>(expressionChannelChannelEdits_.size()); ++i) {
      if (!expressionChannelChannelEdits_[i]) {
        continue;
      }
      const QString value = expressionChannelChannelGetters_[i]
              ? expressionChannelChannelGetters_[i]()
              : QString();
      const QSignalBlocker blocker(expressionChannelChannelEdits_[i]);
      expressionChannelChannelEdits_[i]->setText(value);
      expressionChannelChannelEdits_[i]->setEnabled(
          static_cast<bool>(expressionChannelChannelSetters_[i]));
      committedTexts_[expressionChannelChannelEdits_[i]] = value;
    }

    if (expressionChannelInitialValueSpin_) {
      const QSignalBlocker blocker(expressionChannelInitialValueSpin_);
      expressionChannelInitialValueSpin_->setValue(
          expressionChannelInitialValueGetter_
              ? expressionChannelInitialValueGetter_()
              : 0.0);
      expressionChannelInitialValueSpin_->setEnabled(
          static_cast<bool>(expressionChannelInitialValueSetter_));
    }

    if (expressionChannelEventSignalCombo_) {
      const QSignalBlocker blocker(expressionChannelEventSignalCombo_);
      expressionChannelEventSignalCombo_->setCurrentIndex(
          expressionChannelEventSignalGetter_
              ? expressionChannelEventSignalToIndex(
                    expressionChannelEventSignalGetter_())
              : expressionChannelEventSignalToIndex(
                    ExpressionChannelEventSignalMode::kOnAnyChange));
      expressionChannelEventSignalCombo_->setEnabled(
          static_cast<bool>(expressionChannelEventSignalSetter_));
    }

    if (expressionChannelPrecisionSpin_) {
      const QSignalBlocker blocker(expressionChannelPrecisionSpin_);
      expressionChannelPrecisionSpin_->setValue(
          expressionChannelPrecisionGetter_
              ? std::max(0, expressionChannelPrecisionGetter_())
              : 0);
      expressionChannelPrecisionSpin_->setEnabled(
          static_cast<bool>(expressionChannelPrecisionSetter_));
    }

    if (elementLabel_) {
      elementLabel_->setText(elementLabel);
    }

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
      std::function<void(HeatmapOrder)> orderSetter,
      std::function<HeatmapColorMap()> colorMapGetter,
      std::function<void(HeatmapColorMap)> colorMapSetter,
      std::function<bool()> invertGreyscaleGetter,
      std::function<void(bool)> invertGreyscaleSetter,
      std::function<bool()> showTopProfileGetter,
      std::function<void(bool)> showTopProfileSetter,
      std::function<bool()> showRightProfileGetter,
      std::function<void(bool)> showRightProfileSetter,
      std::function<HeatmapProfileMode()> profileModeGetter,
      std::function<void(HeatmapProfileMode)> profileModeSetter,
      std::function<bool()> preserveAspectRatioGetter,
      std::function<void(bool)> preserveAspectRatioSetter,
      std::function<bool()> flipHorizontalGetter,
      std::function<void(bool)> flipHorizontalSetter,
      std::function<bool()> flipVerticalGetter,
      std::function<void(bool)> flipVerticalSetter,
      std::function<HeatmapRotation()> rotationGetter,
      std::function<void(HeatmapRotation)> rotationSetter)
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
    heatmapColorMapGetter_ = std::move(colorMapGetter);
    heatmapColorMapSetter_ = std::move(colorMapSetter);
    heatmapInvertGreyscaleGetter_ = std::move(invertGreyscaleGetter);
    heatmapInvertGreyscaleSetter_ = std::move(invertGreyscaleSetter);
    heatmapPreserveAspectRatioGetter_ = std::move(preserveAspectRatioGetter);
    heatmapPreserveAspectRatioSetter_ = std::move(preserveAspectRatioSetter);
    heatmapFlipHorizontalGetter_ = std::move(flipHorizontalGetter);
    heatmapFlipHorizontalSetter_ = std::move(flipHorizontalSetter);
    heatmapFlipVerticalGetter_ = std::move(flipVerticalGetter);
    heatmapFlipVerticalSetter_ = std::move(flipVerticalSetter);
    heatmapRotationGetter_ = std::move(rotationGetter);
    heatmapRotationSetter_ = std::move(rotationSetter);
    heatmapShowTopProfileGetter_ = std::move(showTopProfileGetter);
    heatmapShowTopProfileSetter_ = std::move(showTopProfileSetter);
    heatmapShowRightProfileGetter_ = std::move(showRightProfileGetter);
    heatmapShowRightProfileSetter_ = std::move(showRightProfileSetter);
    heatmapProfileModeGetter_ = std::move(profileModeGetter);
    heatmapProfileModeSetter_ = std::move(profileModeSetter);

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

    if (heatmapColorMapCombo_) {
      const QSignalBlocker blocker(heatmapColorMapCombo_);
      const HeatmapColorMap colorMap = heatmapColorMapGetter_ ? heatmapColorMapGetter_()
                                                              : HeatmapColorMap::kGrayscale;
      heatmapColorMapCombo_->setCurrentIndex(static_cast<int>(colorMap));
    }

    if (heatmapInvertGreyscaleCombo_) {
      const QSignalBlocker blocker(heatmapInvertGreyscaleCombo_);
      const bool invert = heatmapInvertGreyscaleGetter_ ? heatmapInvertGreyscaleGetter_() : false;
      heatmapInvertGreyscaleCombo_->setCurrentIndex(heatmapBoolToIndex(invert));
    }
    if (heatmapPreserveAspectRatioCombo_) {
      const QSignalBlocker blocker(heatmapPreserveAspectRatioCombo_);
      const bool preserve = heatmapPreserveAspectRatioGetter_ ? heatmapPreserveAspectRatioGetter_() : true;
      heatmapPreserveAspectRatioCombo_->setCurrentIndex(heatmapBoolToIndex(preserve));
    }
    if (heatmapFlipHorizontalCombo_) {
      const QSignalBlocker blocker(heatmapFlipHorizontalCombo_);
      const bool flip = heatmapFlipHorizontalGetter_ ? heatmapFlipHorizontalGetter_() : false;
      heatmapFlipHorizontalCombo_->setCurrentIndex(heatmapBoolToIndex(flip));
    }
    if (heatmapFlipVerticalCombo_) {
      const QSignalBlocker blocker(heatmapFlipVerticalCombo_);
      const bool flip = heatmapFlipVerticalGetter_ ? heatmapFlipVerticalGetter_() : false;
      heatmapFlipVerticalCombo_->setCurrentIndex(heatmapBoolToIndex(flip));
    }
    if (heatmapRotationCombo_) {
      const QSignalBlocker blocker(heatmapRotationCombo_);
      const HeatmapRotation rot = heatmapRotationGetter_ ? heatmapRotationGetter_() : HeatmapRotation::kNone;
      heatmapRotationCombo_->setCurrentIndex(static_cast<int>(rot));
    }
    if (heatmapShowTopProfileCombo_) {
      const QSignalBlocker blocker(heatmapShowTopProfileCombo_);
      const bool show = heatmapShowTopProfileGetter_
          ? heatmapShowTopProfileGetter_()
          : false;
      heatmapShowTopProfileCombo_->setCurrentIndex(heatmapBoolToIndex(show));
    }
    if (heatmapShowRightProfileCombo_) {
      const QSignalBlocker blocker(heatmapShowRightProfileCombo_);
      const bool show = heatmapShowRightProfileGetter_
          ? heatmapShowRightProfileGetter_()
          : false;
      heatmapShowRightProfileCombo_->setCurrentIndex(heatmapBoolToIndex(show));
    }
    if (heatmapProfileModeCombo_) {
      const QSignalBlocker blocker(heatmapProfileModeCombo_);
      const HeatmapProfileMode mode = heatmapProfileModeGetter_
          ? heatmapProfileModeGetter_()
          : HeatmapProfileMode::kAbsolute;
      heatmapProfileModeCombo_->setCurrentIndex(heatmapProfileModeToIndex(mode));
    }

    updateHeatmapDimensionControls();
    elementLabel_->setText(QStringLiteral("Heatmap"));
    showPaletteWithoutActivating();
  }

  void showForWaterfall(std::function<QRect()> geometryGetter,
      std::function<void(const QRect &)> geometrySetter,
      std::function<QColor()> foregroundGetter,
      std::function<void(const QColor &)> foregroundSetter,
      std::function<QColor()> backgroundGetter,
      std::function<void(const QColor &)> backgroundSetter,
      std::function<QString()> titleGetter,
      std::function<void(const QString &)> titleSetter,
      std::function<QString()> xLabelGetter,
      std::function<void(const QString &)> xLabelSetter,
      std::function<QString()> yLabelGetter,
      std::function<void(const QString &)> yLabelSetter,
      std::function<QString()> dataChannelGetter,
      std::function<void(const QString &)> dataChannelSetter,
      std::function<QString()> countChannelGetter,
      std::function<void(const QString &)> countChannelSetter,
      std::function<QString()> triggerChannelGetter,
      std::function<void(const QString &)> triggerChannelSetter,
      std::function<QString()> eraseChannelGetter,
      std::function<void(const QString &)> eraseChannelSetter,
      std::function<WaterfallEraseMode()> eraseModeGetter,
      std::function<void(WaterfallEraseMode)> eraseModeSetter,
      std::function<int()> historyCountGetter,
      std::function<void(int)> historyCountSetter,
      std::function<WaterfallScrollDirection()> scrollDirectionGetter,
      std::function<void(WaterfallScrollDirection)> scrollDirectionSetter,
      std::function<HeatmapColorMap()> colorMapGetter,
      std::function<void(HeatmapColorMap)> colorMapSetter,
      std::function<bool()> invertGreyscaleGetter,
      std::function<void(bool)> invertGreyscaleSetter,
      std::function<WaterfallIntensityScale()> intensityScaleGetter,
      std::function<void(WaterfallIntensityScale)> intensityScaleSetter,
      std::function<double()> intensityMinGetter,
      std::function<void(double)> intensityMinSetter,
      std::function<double()> intensityMaxGetter,
      std::function<void(double)> intensityMaxSetter,
      std::function<bool()> showLegendGetter,
      std::function<void(bool)> showLegendSetter,
      std::function<bool()> showGridGetter,
      std::function<void(bool)> showGridSetter,
      std::function<double()> samplePeriodGetter,
      std::function<void(double)> samplePeriodSetter,
      std::function<TimeUnits()> unitsGetter,
      std::function<void(TimeUnits)> unitsSetter)
  {
    clearSelectionState();
    selectionKind_ = SelectionKind::kWaterfallPlot;
    updateSectionVisibility(selectionKind_);

    geometryGetter_ = std::move(geometryGetter);
    geometrySetter_ = std::move(geometrySetter);
    waterfallForegroundGetter_ = std::move(foregroundGetter);
    waterfallForegroundSetter_ = std::move(foregroundSetter);
    waterfallBackgroundGetter_ = std::move(backgroundGetter);
    waterfallBackgroundSetter_ = std::move(backgroundSetter);
    waterfallTitleGetter_ = std::move(titleGetter);
    waterfallTitleSetter_ = std::move(titleSetter);
    waterfallXLabelGetter_ = std::move(xLabelGetter);
    waterfallXLabelSetter_ = std::move(xLabelSetter);
    waterfallYLabelGetter_ = std::move(yLabelGetter);
    waterfallYLabelSetter_ = std::move(yLabelSetter);
    waterfallDataChannelGetter_ = std::move(dataChannelGetter);
    waterfallDataChannelSetter_ = std::move(dataChannelSetter);
    waterfallCountChannelGetter_ = std::move(countChannelGetter);
    waterfallCountChannelSetter_ = std::move(countChannelSetter);
    waterfallTriggerChannelGetter_ = std::move(triggerChannelGetter);
    waterfallTriggerChannelSetter_ = std::move(triggerChannelSetter);
    waterfallEraseChannelGetter_ = std::move(eraseChannelGetter);
    waterfallEraseChannelSetter_ = std::move(eraseChannelSetter);
    waterfallEraseModeGetter_ = std::move(eraseModeGetter);
    waterfallEraseModeSetter_ = std::move(eraseModeSetter);
    waterfallHistoryCountGetter_ = std::move(historyCountGetter);
    waterfallHistoryCountSetter_ = std::move(historyCountSetter);
    waterfallScrollDirectionGetter_ = std::move(scrollDirectionGetter);
    waterfallScrollDirectionSetter_ = std::move(scrollDirectionSetter);
    waterfallColorMapGetter_ = std::move(colorMapGetter);
    waterfallColorMapSetter_ = std::move(colorMapSetter);
    waterfallInvertGreyscaleGetter_ = std::move(invertGreyscaleGetter);
    waterfallInvertGreyscaleSetter_ = std::move(invertGreyscaleSetter);
    waterfallIntensityScaleGetter_ = std::move(intensityScaleGetter);
    waterfallIntensityScaleSetter_ = std::move(intensityScaleSetter);
    waterfallIntensityMinGetter_ = std::move(intensityMinGetter);
    waterfallIntensityMinSetter_ = std::move(intensityMinSetter);
    waterfallIntensityMaxGetter_ = std::move(intensityMaxGetter);
    waterfallIntensityMaxSetter_ = std::move(intensityMaxSetter);
    waterfallShowLegendGetter_ = std::move(showLegendGetter);
    waterfallShowLegendSetter_ = std::move(showLegendSetter);
    waterfallShowGridGetter_ = std::move(showGridGetter);
    waterfallShowGridSetter_ = std::move(showGridSetter);
    waterfallSamplePeriodGetter_ = std::move(samplePeriodGetter);
    waterfallSamplePeriodSetter_ = std::move(samplePeriodSetter);
    waterfallUnitsGetter_ = std::move(unitsGetter);
    waterfallUnitsSetter_ = std::move(unitsSetter);

    QRect waterfallGeometry = geometryGetter_ ? geometryGetter_() : QRect();
    if (waterfallGeometry.width() <= 0) {
      waterfallGeometry.setWidth(kMinimumWaterfallPlotWidth);
    }
    if (waterfallGeometry.height() <= 0) {
      waterfallGeometry.setHeight(kMinimumWaterfallPlotHeight);
    }
    lastCommittedGeometry_ = waterfallGeometry;
    updateGeometryEdits(waterfallGeometry);

    if (waterfallForegroundButton_) {
      const QColor color = waterfallForegroundGetter_
          ? waterfallForegroundGetter_()
          : palette().color(QPalette::WindowText);
      setColorButtonColor(waterfallForegroundButton_,
          color.isValid() ? color : palette().color(QPalette::WindowText));
      waterfallForegroundButton_->setEnabled(
          static_cast<bool>(waterfallForegroundSetter_));
    }

    if (waterfallBackgroundButton_) {
      const QColor color = waterfallBackgroundGetter_
          ? waterfallBackgroundGetter_()
          : palette().color(QPalette::Window);
      setColorButtonColor(waterfallBackgroundButton_,
          color.isValid() ? color : palette().color(QPalette::Window));
      waterfallBackgroundButton_->setEnabled(
          static_cast<bool>(waterfallBackgroundSetter_));
    }

    auto setWaterfallEdit = [this](QLineEdit *edit, const QString &value,
                               bool enabled) {
      if (!edit) {
        return;
      }
      const QSignalBlocker blocker(edit);
      edit->setText(value);
      edit->setEnabled(enabled);
      committedTexts_[edit] = edit->text();
    };

    auto formatNumber = [](double value) {
      QString text = QString::number(value, 'g', 15);
      if (text == QStringLiteral("-0")) {
        text = QStringLiteral("0");
      }
      return text;
    };

    int historyCount = waterfallHistoryCountGetter_
        ? waterfallHistoryCountGetter_()
        : kWaterfallDefaultHistory;
    historyCount = std::clamp(historyCount, 1, kWaterfallMaxHistory);

    setWaterfallEdit(waterfallTitleEdit_,
        waterfallTitleGetter_ ? waterfallTitleGetter_() : QString(),
        static_cast<bool>(waterfallTitleSetter_));
    setWaterfallEdit(waterfallXLabelEdit_,
        waterfallXLabelGetter_ ? waterfallXLabelGetter_() : QString(),
        static_cast<bool>(waterfallXLabelSetter_));
    setWaterfallEdit(waterfallYLabelEdit_,
        waterfallYLabelGetter_ ? waterfallYLabelGetter_() : QString(),
        static_cast<bool>(waterfallYLabelSetter_));
    setWaterfallEdit(waterfallDataChannelEdit_,
        waterfallDataChannelGetter_ ? waterfallDataChannelGetter_() : QString(),
        static_cast<bool>(waterfallDataChannelSetter_));
    setWaterfallEdit(waterfallCountChannelEdit_,
        waterfallCountChannelGetter_
            ? waterfallCountChannelGetter_()
            : QString(),
        static_cast<bool>(waterfallCountChannelSetter_));
    setWaterfallEdit(waterfallTriggerChannelEdit_,
        waterfallTriggerChannelGetter_
            ? waterfallTriggerChannelGetter_()
            : QString(),
        static_cast<bool>(waterfallTriggerChannelSetter_));
    setWaterfallEdit(waterfallEraseChannelEdit_,
        waterfallEraseChannelGetter_
            ? waterfallEraseChannelGetter_()
            : QString(),
        static_cast<bool>(waterfallEraseChannelSetter_));
    setWaterfallEdit(waterfallHistoryEdit_, QString::number(historyCount),
        static_cast<bool>(waterfallHistoryCountSetter_));
    setWaterfallEdit(waterfallIntensityMinEdit_,
        formatNumber(waterfallIntensityMinGetter_
                ? waterfallIntensityMinGetter_()
                : 0.0),
        static_cast<bool>(waterfallIntensityMinSetter_));
    setWaterfallEdit(waterfallIntensityMaxEdit_,
        formatNumber(waterfallIntensityMaxGetter_
                ? waterfallIntensityMaxGetter_()
                : 1.0),
        static_cast<bool>(waterfallIntensityMaxSetter_));
    setWaterfallEdit(waterfallSamplePeriodEdit_,
        formatNumber(waterfallSamplePeriodGetter_
                ? waterfallSamplePeriodGetter_()
                : 0.0),
        static_cast<bool>(waterfallSamplePeriodSetter_));

    if (waterfallEraseModeCombo_) {
      const QSignalBlocker blocker(waterfallEraseModeCombo_);
      const auto mode = waterfallEraseModeGetter_
          ? waterfallEraseModeGetter_()
          : WaterfallEraseMode::kIfNotZero;
      waterfallEraseModeCombo_->setCurrentIndex(std::clamp(
          static_cast<int>(mode), 0, waterfallEraseModeCombo_->count() - 1));
    }

    if (waterfallScrollDirectionCombo_) {
      const QSignalBlocker blocker(waterfallScrollDirectionCombo_);
      const auto direction = waterfallScrollDirectionGetter_
          ? waterfallScrollDirectionGetter_()
          : WaterfallScrollDirection::kTopToBottom;
      waterfallScrollDirectionCombo_->setCurrentIndex(std::clamp(
          static_cast<int>(direction), 0,
          waterfallScrollDirectionCombo_->count() - 1));
      waterfallScrollDirectionCombo_->setEnabled(
          static_cast<bool>(waterfallScrollDirectionSetter_));
    }

    if (waterfallColorMapCombo_) {
      const QSignalBlocker blocker(waterfallColorMapCombo_);
      const auto colorMap = waterfallColorMapGetter_
          ? waterfallColorMapGetter_()
          : HeatmapColorMap::kGrayscale;
      waterfallColorMapCombo_->setCurrentIndex(std::clamp(
          static_cast<int>(colorMap), 0, waterfallColorMapCombo_->count() - 1));
      waterfallColorMapCombo_->setEnabled(
          static_cast<bool>(waterfallColorMapSetter_));
    }

    if (waterfallInvertGreyscaleCombo_) {
      const QSignalBlocker blocker(waterfallInvertGreyscaleCombo_);
      const bool invert = waterfallInvertGreyscaleGetter_
          ? waterfallInvertGreyscaleGetter_()
          : true;
      waterfallInvertGreyscaleCombo_->setCurrentIndex(invert ? 1 : 0);
    }

    if (waterfallIntensityScaleCombo_) {
      const QSignalBlocker blocker(waterfallIntensityScaleCombo_);
      const auto scale = waterfallIntensityScaleGetter_
          ? waterfallIntensityScaleGetter_()
          : WaterfallIntensityScale::kAuto;
      waterfallIntensityScaleCombo_->setCurrentIndex(std::clamp(
          static_cast<int>(scale), 0,
          waterfallIntensityScaleCombo_->count() - 1));
      waterfallIntensityScaleCombo_->setEnabled(
          static_cast<bool>(waterfallIntensityScaleSetter_));
    }

    if (waterfallShowLegendCombo_) {
      const QSignalBlocker blocker(waterfallShowLegendCombo_);
      const bool show = waterfallShowLegendGetter_
          ? waterfallShowLegendGetter_()
          : true;
      waterfallShowLegendCombo_->setCurrentIndex(show ? 1 : 0);
      waterfallShowLegendCombo_->setEnabled(
          static_cast<bool>(waterfallShowLegendSetter_));
    }

    if (waterfallShowGridCombo_) {
      const QSignalBlocker blocker(waterfallShowGridCombo_);
      const bool show = waterfallShowGridGetter_
          ? waterfallShowGridGetter_()
          : false;
      waterfallShowGridCombo_->setCurrentIndex(show ? 1 : 0);
      waterfallShowGridCombo_->setEnabled(
          static_cast<bool>(waterfallShowGridSetter_));
    }

    if (waterfallUnitsCombo_) {
      const QSignalBlocker blocker(waterfallUnitsCombo_);
      const TimeUnits units = waterfallUnitsGetter_
          ? waterfallUnitsGetter_()
          : TimeUnits::kSeconds;
      waterfallUnitsCombo_->setCurrentIndex(timeUnitsToIndex(units));
      waterfallUnitsCombo_->setEnabled(static_cast<bool>(waterfallUnitsSetter_));
    }

    updateWaterfallDependentControls();
    elementLabel_->setText(QStringLiteral("Waterfall Plot"));
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
    setpointControlForegroundGetter_ = {};
    setpointControlForegroundSetter_ = {};
    setpointControlBackgroundGetter_ = {};
    setpointControlBackgroundSetter_ = {};
    setpointControlFormatGetter_ = {};
    setpointControlFormatSetter_ = {};
    setpointControlPrecisionGetter_ = {};
    setpointControlPrecisionSetter_ = {};
    setpointControlPrecisionSourceGetter_ = {};
    setpointControlPrecisionSourceSetter_ = {};
    setpointControlPrecisionDefaultGetter_ = {};
    setpointControlPrecisionDefaultSetter_ = {};
    setpointControlColorModeGetter_ = {};
    setpointControlColorModeSetter_ = {};
    setpointControlSetpointGetter_ = {};
    setpointControlSetpointSetter_ = {};
    setpointControlReadbackGetter_ = {};
    setpointControlReadbackSetter_ = {};
    setpointControlLabelGetter_ = {};
    setpointControlLabelSetter_ = {};
    setpointControlLimitsGetter_ = {};
    setpointControlLimitsSetter_ = {};
    setpointControlToleranceModeGetter_ = {};
    setpointControlToleranceModeSetter_ = {};
    setpointControlToleranceGetter_ = {};
    setpointControlToleranceSetter_ = {};
    setpointControlShowReadbackGetter_ = {};
    setpointControlShowReadbackSetter_ = {};
    textAreaForegroundGetter_ = {};
    textAreaForegroundSetter_ = {};
    textAreaBackgroundGetter_ = {};
    textAreaBackgroundSetter_ = {};
    textAreaFormatGetter_ = {};
    textAreaFormatSetter_ = {};
    textAreaPrecisionGetter_ = {};
    textAreaPrecisionSetter_ = {};
    textAreaPrecisionSourceGetter_ = {};
    textAreaPrecisionSourceSetter_ = {};
    textAreaPrecisionDefaultGetter_ = {};
    textAreaPrecisionDefaultSetter_ = {};
    textAreaColorModeGetter_ = {};
    textAreaColorModeSetter_ = {};
    textAreaChannelGetter_ = {};
    textAreaChannelSetter_ = {};
    textAreaLimitsGetter_ = {};
    textAreaLimitsSetter_ = {};
    textAreaReadOnlyGetter_ = {};
    textAreaReadOnlySetter_ = {};
    textAreaWordWrapGetter_ = {};
    textAreaWordWrapSetter_ = {};
    textAreaWrapModeGetter_ = {};
    textAreaWrapModeSetter_ = {};
    textAreaWrapColumnWidthGetter_ = {};
    textAreaWrapColumnWidthSetter_ = {};
    textAreaShowVerticalScrollBarGetter_ = {};
    textAreaShowVerticalScrollBarSetter_ = {};
    textAreaShowHorizontalScrollBarGetter_ = {};
    textAreaShowHorizontalScrollBarSetter_ = {};
    textAreaCommitModeGetter_ = {};
    textAreaCommitModeSetter_ = {};
    textAreaTabInsertsSpacesGetter_ = {};
    textAreaTabInsertsSpacesSetter_ = {};
    textAreaTabWidthGetter_ = {};
    textAreaTabWidthSetter_ = {};
    textAreaFontFamilyGetter_ = {};
    textAreaFontFamilySetter_ = {};
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
    if (setpointControlPvLimitsButton_) {
      setpointControlPvLimitsButton_->setEnabled(false);
    }
    if (textAreaPvLimitsButton_) {
      textAreaPvLimitsButton_->setEnabled(false);
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
    thermometerForegroundGetter_ = {};
    thermometerForegroundSetter_ = {};
    thermometerBackgroundGetter_ = {};
    thermometerBackgroundSetter_ = {};
    thermometerTextGetter_ = {};
    thermometerTextSetter_ = {};
    thermometerLabelGetter_ = {};
    thermometerLabelSetter_ = {};
    thermometerColorModeGetter_ = {};
    thermometerColorModeSetter_ = {};
    thermometerVisibilityModeGetter_ = {};
    thermometerVisibilityModeSetter_ = {};
    thermometerVisibilityCalcGetter_ = {};
    thermometerVisibilityCalcSetter_ = {};
    for (auto &getter : thermometerVisibilityChannelGetters_) {
      getter = {};
    }
    for (auto &setter : thermometerVisibilityChannelSetters_) {
      setter = {};
    }
    thermometerFormatGetter_ = {};
    thermometerFormatSetter_ = {};
    thermometerShowValueGetter_ = {};
    thermometerShowValueSetter_ = {};
    thermometerChannelGetter_ = {};
    thermometerChannelSetter_ = {};
    thermometerLimitsGetter_ = {};
    thermometerLimitsSetter_ = {};
    if (thermometerPvLimitsButton_) {
      thermometerPvLimitsButton_->setEnabled(false);
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
    if (cartesianDrawMajorCombo_) {
      const QSignalBlocker blocker(cartesianDrawMajorCombo_);
      cartesianDrawMajorCombo_->setCurrentIndex(1);
      cartesianDrawMajorCombo_->setEnabled(false);
    }
    if (cartesianDrawMinorCombo_) {
      const QSignalBlocker blocker(cartesianDrawMinorCombo_);
      cartesianDrawMinorCombo_->setCurrentIndex(0);
      cartesianDrawMinorCombo_->setEnabled(false);
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
    ledForegroundGetter_ = {};
    ledForegroundSetter_ = {};
    ledBackgroundGetter_ = {};
    ledBackgroundSetter_ = {};
    ledColorModeGetter_ = {};
    ledColorModeSetter_ = {};
    ledShapeGetter_ = {};
    ledShapeSetter_ = {};
    ledBezelGetter_ = {};
    ledBezelSetter_ = {};
    ledOnColorGetter_ = {};
    ledOnColorSetter_ = {};
    ledOffColorGetter_ = {};
    ledOffColorSetter_ = {};
    ledUndefinedColorGetter_ = {};
    ledUndefinedColorSetter_ = {};
    for (auto &getter : ledStateColorGetters_) {
      getter = {};
    }
    for (auto &setter : ledStateColorSetters_) {
      setter = {};
    }
    ledStateCountGetter_ = {};
    ledStateCountSetter_ = {};
    ledChannelGetter_ = {};
    ledChannelSetter_ = {};
    ledVisibilityModeGetter_ = {};
    ledVisibilityModeSetter_ = {};
    ledVisibilityCalcGetter_ = {};
    ledVisibilityCalcSetter_ = {};
    for (auto &getter : ledVisibilityChannelGetters_) {
      getter = {};
    }
    for (auto &setter : ledVisibilityChannelSetters_) {
      setter = {};
    }
    if (ledForegroundButton_) {
      ledForegroundButton_->setEnabled(false);
    }
    if (ledBackgroundButton_) {
      ledBackgroundButton_->setEnabled(false);
    }
    if (ledOnColorButton_) {
      ledOnColorButton_->setEnabled(false);
    }
    if (ledOffColorButton_) {
      ledOffColorButton_->setEnabled(false);
    }
    if (ledUndefinedColorButton_) {
      ledUndefinedColorButton_->setEnabled(false);
    }
    if (ledColorModeCombo_) {
      ledColorModeCombo_->setEnabled(false);
    }
    if (ledShapeCombo_) {
      ledShapeCombo_->setEnabled(false);
    }
    if (ledBezelCheckBox_) {
      ledBezelCheckBox_->setEnabled(false);
    }
    if (ledStateCountSpin_) {
      ledStateCountSpin_->setEnabled(false);
    }
    if (ledChannelEdit_) {
      ledChannelEdit_->setEnabled(false);
    }
    if (ledVisibilityCombo_) {
      ledVisibilityCombo_->setEnabled(false);
    }
    if (ledVisibilityCalcEdit_) {
      ledVisibilityCalcEdit_->setEnabled(false);
    }
    for (QLineEdit *edit : ledVisibilityChannelEdits_) {
      if (edit) {
        edit->setEnabled(false);
      }
    }
    for (QPushButton *button : ledStateColorButtons_) {
      if (button) {
        button->setEnabled(false);
      }
    }
    expressionChannelForegroundGetter_ = {};
    expressionChannelForegroundSetter_ = {};
    expressionChannelBackgroundGetter_ = {};
    expressionChannelBackgroundSetter_ = {};
    expressionChannelVariableGetter_ = {};
    expressionChannelVariableSetter_ = {};
    expressionChannelCalcGetter_ = {};
    expressionChannelCalcSetter_ = {};
    for (auto &getter : expressionChannelChannelGetters_) {
      getter = {};
    }
    for (auto &setter : expressionChannelChannelSetters_) {
      setter = {};
    }
    expressionChannelInitialValueGetter_ = {};
    expressionChannelInitialValueSetter_ = {};
    expressionChannelEventSignalGetter_ = {};
    expressionChannelEventSignalSetter_ = {};
    expressionChannelPrecisionGetter_ = {};
    expressionChannelPrecisionSetter_ = {};
    if (expressionChannelForegroundButton_) {
      expressionChannelForegroundButton_->setEnabled(false);
    }
    if (expressionChannelBackgroundButton_) {
      expressionChannelBackgroundButton_->setEnabled(false);
    }
    if (expressionChannelVariableEdit_) {
      expressionChannelVariableEdit_->setEnabled(false);
    }
    if (expressionChannelCalcEdit_) {
      expressionChannelCalcEdit_->setEnabled(false);
    }
    for (QLineEdit *edit : expressionChannelChannelEdits_) {
      if (edit) {
        edit->setEnabled(false);
      }
    }
    if (expressionChannelInitialValueSpin_) {
      expressionChannelInitialValueSpin_->setEnabled(false);
    }
    if (expressionChannelEventSignalCombo_) {
      expressionChannelEventSignalCombo_->setEnabled(false);
    }
    if (expressionChannelPrecisionSpin_) {
      expressionChannelPrecisionSpin_->setEnabled(false);
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
    resetLineEdit(setpointControlLabelEdit_);
    resetLineEdit(setpointControlSetpointEdit_);
    resetLineEdit(setpointControlReadbackEdit_);
    resetLineEdit(setpointControlToleranceEdit_);
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
    resetLineEdit(pvTableColumnsEdit_);
    for (QLineEdit *edit : pvTableRowLabelEdits_) {
      resetLineEdit(edit);
    }
    for (QLineEdit *edit : pvTableRowChannelEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(waveTableChannelEdit_);
    resetLineEdit(waveTableColumnsEdit_);
    resetLineEdit(waveTableMaxElementsEdit_);
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
    resetLineEdit(thermometerChannelEdit_);
    resetLineEdit(thermometerVisibilityCalcEdit_);
    for (QLineEdit *edit : thermometerVisibilityChannelEdits_) {
      resetLineEdit(edit);
    }
    updateThermometerChannelDependentControls();
    resetLineEdit(scaleChannelEdit_);
    resetLineEdit(ledChannelEdit_);
    resetLineEdit(ledVisibilityCalcEdit_);
    for (QLineEdit *edit : ledVisibilityChannelEdits_) {
      resetLineEdit(edit);
    }
    updateLedMonitorChannelDependentControls();
    resetLineEdit(expressionChannelVariableEdit_);
    resetLineEdit(expressionChannelCalcEdit_);
    for (QLineEdit *edit : expressionChannelChannelEdits_) {
      resetLineEdit(edit);
    }
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
    resetColorButton(thermometerForegroundButton_);
    resetColorButton(thermometerBackgroundButton_);
    resetColorButton(thermometerTextButton_);
    resetColorButton(scaleForegroundButton_);
    resetColorButton(scaleBackgroundButton_);
    resetColorButton(ledForegroundButton_);
    resetColorButton(ledBackgroundButton_);
    resetColorButton(ledOnColorButton_);
    resetColorButton(ledOffColorButton_);
    resetColorButton(ledUndefinedColorButton_);
    for (QPushButton *button : ledStateColorButtons_) {
      resetColorButton(button);
    }
    resetColorButton(waterfallForegroundButton_);
    resetColorButton(waterfallBackgroundButton_);
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
    resetColorButton(expressionChannelForegroundButton_);
    resetColorButton(expressionChannelBackgroundButton_);
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
    if (thermometerLabelCombo_) {
      const QSignalBlocker blocker(thermometerLabelCombo_);
      thermometerLabelCombo_->setCurrentIndex(
          meterLabelToIndex(MeterLabel::kNone));
    }
    if (thermometerColorModeCombo_) {
      const QSignalBlocker blocker(thermometerColorModeCombo_);
      thermometerColorModeCombo_->setCurrentIndex(
          colorModeToIndex(TextColorMode::kStatic));
    }
    if (thermometerFormatCombo_) {
      const QSignalBlocker blocker(thermometerFormatCombo_);
      thermometerFormatCombo_->setCurrentIndex(
          textMonitorFormatToIndex(TextMonitorFormat::kDecimal));
    }
    if (thermometerShowValueCombo_) {
      const QSignalBlocker blocker(thermometerShowValueCombo_);
      thermometerShowValueCombo_->setCurrentIndex(0);
    }
    if (thermometerVisibilityCombo_) {
      const QSignalBlocker blocker(thermometerVisibilityCombo_);
      thermometerVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
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
    if (ledColorModeCombo_) {
      const QSignalBlocker blocker(ledColorModeCombo_);
      ledColorModeCombo_->setCurrentIndex(
          ledColorModeChoiceToIndex(LedColorModeChoice::kStatic));
    }
    if (ledShapeCombo_) {
      const QSignalBlocker blocker(ledShapeCombo_);
      ledShapeCombo_->setCurrentIndex(ledShapeToIndex(LedShape::kCircle));
    }
    if (ledBezelCheckBox_) {
      const QSignalBlocker blocker(ledBezelCheckBox_);
      ledBezelCheckBox_->setChecked(true);
    }
    if (ledStateCountSpin_) {
      const QSignalBlocker blocker(ledStateCountSpin_);
      ledStateCountSpin_->setValue(2);
    }
    if (ledVisibilityCombo_) {
      const QSignalBlocker blocker(ledVisibilityCombo_);
      ledVisibilityCombo_->setCurrentIndex(
          visibilityModeToIndex(TextVisibilityMode::kStatic));
    }
    updateLedMonitorStateColorControls();
    if (barDirectionCombo_) {
      const QSignalBlocker blocker(barDirectionCombo_);
      barDirectionCombo_->setCurrentIndex(barDirectionToIndex(BarDirection::kRight));
    }
    if (barFillCombo_) {
      const QSignalBlocker blocker(barFillCombo_);
      barFillCombo_->setCurrentIndex(barFillToIndex(BarFill::kFromEdge));
    }
    if (expressionChannelInitialValueSpin_) {
      const QSignalBlocker blocker(expressionChannelInitialValueSpin_);
      expressionChannelInitialValueSpin_->setValue(0.0);
    }
    if (expressionChannelEventSignalCombo_) {
      const QSignalBlocker blocker(expressionChannelEventSignalCombo_);
      expressionChannelEventSignalCombo_->setCurrentIndex(
          expressionChannelEventSignalToIndex(
              ExpressionChannelEventSignalMode::kOnAnyChange));
    }
    if (expressionChannelPrecisionSpin_) {
      const QSignalBlocker blocker(expressionChannelPrecisionSpin_);
      expressionChannelPrecisionSpin_->setValue(0);
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
    if (heatmapColorMapCombo_) {
      const QSignalBlocker blocker(heatmapColorMapCombo_);
      heatmapColorMapCombo_->setCurrentIndex(
          static_cast<int>(HeatmapColorMap::kGrayscale));
    }
    if (heatmapInvertGreyscaleCombo_) {
      const QSignalBlocker blocker(heatmapInvertGreyscaleCombo_);
      heatmapInvertGreyscaleCombo_->setCurrentIndex(1);
    }
    if (heatmapPreserveAspectRatioCombo_) {
      const QSignalBlocker blocker(heatmapPreserveAspectRatioCombo_);
      heatmapPreserveAspectRatioCombo_->setCurrentIndex(0);
    }
    if (heatmapFlipHorizontalCombo_) {
      const QSignalBlocker blocker(heatmapFlipHorizontalCombo_);
      heatmapFlipHorizontalCombo_->setCurrentIndex(0);
    }
    if (heatmapFlipVerticalCombo_) {
      const QSignalBlocker blocker(heatmapFlipVerticalCombo_);
      heatmapFlipVerticalCombo_->setCurrentIndex(0);
    }
    if (heatmapRotationCombo_) {
      const QSignalBlocker blocker(heatmapRotationCombo_);
      heatmapRotationCombo_->setCurrentIndex(0);
    }
    if (heatmapShowTopProfileCombo_) {
      const QSignalBlocker blocker(heatmapShowTopProfileCombo_);
      heatmapShowTopProfileCombo_->setCurrentIndex(heatmapBoolToIndex(false));
    }
    if (heatmapShowRightProfileCombo_) {
      const QSignalBlocker blocker(heatmapShowRightProfileCombo_);
      heatmapShowRightProfileCombo_->setCurrentIndex(heatmapBoolToIndex(false));
    }
    if (heatmapProfileModeCombo_) {
      const QSignalBlocker blocker(heatmapProfileModeCombo_);
      heatmapProfileModeCombo_->setCurrentIndex(
          heatmapProfileModeToIndex(HeatmapProfileMode::kAbsolute));
    }
    if (waterfallEraseModeCombo_) {
      const QSignalBlocker blocker(waterfallEraseModeCombo_);
      waterfallEraseModeCombo_->setCurrentIndex(
          static_cast<int>(WaterfallEraseMode::kIfNotZero));
    }
    if (waterfallScrollDirectionCombo_) {
      const QSignalBlocker blocker(waterfallScrollDirectionCombo_);
      waterfallScrollDirectionCombo_->setCurrentIndex(
          static_cast<int>(WaterfallScrollDirection::kTopToBottom));
    }
    if (waterfallColorMapCombo_) {
      const QSignalBlocker blocker(waterfallColorMapCombo_);
      waterfallColorMapCombo_->setCurrentIndex(
          static_cast<int>(HeatmapColorMap::kGrayscale));
    }
    if (waterfallInvertGreyscaleCombo_) {
      const QSignalBlocker blocker(waterfallInvertGreyscaleCombo_);
      waterfallInvertGreyscaleCombo_->setCurrentIndex(1);
    }
    if (waterfallIntensityScaleCombo_) {
      const QSignalBlocker blocker(waterfallIntensityScaleCombo_);
      waterfallIntensityScaleCombo_->setCurrentIndex(
          static_cast<int>(WaterfallIntensityScale::kAuto));
    }
    if (waterfallShowLegendCombo_) {
      const QSignalBlocker blocker(waterfallShowLegendCombo_);
      waterfallShowLegendCombo_->setCurrentIndex(1);
    }
    if (waterfallShowGridCombo_) {
      const QSignalBlocker blocker(waterfallShowGridCombo_);
      waterfallShowGridCombo_->setCurrentIndex(0);
    }
    if (waterfallUnitsCombo_) {
      const QSignalBlocker blocker(waterfallUnitsCombo_);
      waterfallUnitsCombo_->setCurrentIndex(timeUnitsToIndex(TimeUnits::kSeconds));
    }

    resetLineEdit(heatmapTitleEdit_);
    resetLineEdit(heatmapDataChannelEdit_);
    resetLineEdit(heatmapXDimEdit_);
    resetLineEdit(heatmapYDimEdit_);
    resetLineEdit(heatmapXDimChannelEdit_);
    resetLineEdit(heatmapYDimChannelEdit_);
    resetLineEdit(waterfallTitleEdit_);
    resetLineEdit(waterfallXLabelEdit_);
    resetLineEdit(waterfallYLabelEdit_);
    resetLineEdit(waterfallDataChannelEdit_);
    resetLineEdit(waterfallCountChannelEdit_);
    resetLineEdit(waterfallTriggerChannelEdit_);
    resetLineEdit(waterfallEraseChannelEdit_);
    resetLineEdit(waterfallHistoryEdit_);
    resetLineEdit(waterfallIntensityMinEdit_);
    resetLineEdit(waterfallIntensityMaxEdit_);
    resetLineEdit(waterfallSamplePeriodEdit_);
    updateWaterfallDependentControls();

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
    heatmapColorMapGetter_ = {};
    heatmapColorMapSetter_ = {};
    heatmapInvertGreyscaleGetter_ = {};
    heatmapInvertGreyscaleSetter_ = {};
    heatmapPreserveAspectRatioGetter_ = {};
    heatmapPreserveAspectRatioSetter_ = {};
    heatmapFlipHorizontalGetter_ = {};
    heatmapFlipHorizontalSetter_ = {};
    heatmapFlipVerticalGetter_ = {};
    heatmapFlipVerticalSetter_ = {};
    heatmapRotationGetter_ = {};
    heatmapRotationSetter_ = {};
    heatmapShowTopProfileGetter_ = {};
    heatmapShowTopProfileSetter_ = {};
    heatmapShowRightProfileGetter_ = {};
    heatmapShowRightProfileSetter_ = {};
    heatmapProfileModeGetter_ = {};
    heatmapProfileModeSetter_ = {};
    waterfallForegroundGetter_ = {};
    waterfallForegroundSetter_ = {};
    waterfallBackgroundGetter_ = {};
    waterfallBackgroundSetter_ = {};
    waterfallTitleGetter_ = {};
    waterfallTitleSetter_ = {};
    waterfallXLabelGetter_ = {};
    waterfallXLabelSetter_ = {};
    waterfallYLabelGetter_ = {};
    waterfallYLabelSetter_ = {};
    waterfallDataChannelGetter_ = {};
    waterfallDataChannelSetter_ = {};
    waterfallCountChannelGetter_ = {};
    waterfallCountChannelSetter_ = {};
    waterfallTriggerChannelGetter_ = {};
    waterfallTriggerChannelSetter_ = {};
    waterfallEraseChannelGetter_ = {};
    waterfallEraseChannelSetter_ = {};
    waterfallEraseModeGetter_ = {};
    waterfallEraseModeSetter_ = {};
    waterfallHistoryCountGetter_ = {};
    waterfallHistoryCountSetter_ = {};
    waterfallScrollDirectionGetter_ = {};
    waterfallScrollDirectionSetter_ = {};
    waterfallColorMapGetter_ = {};
    waterfallColorMapSetter_ = {};
    waterfallInvertGreyscaleGetter_ = {};
    waterfallInvertGreyscaleSetter_ = {};
    waterfallIntensityScaleGetter_ = {};
    waterfallIntensityScaleSetter_ = {};
    waterfallIntensityMinGetter_ = {};
    waterfallIntensityMinSetter_ = {};
    waterfallIntensityMaxGetter_ = {};
    waterfallIntensityMaxSetter_ = {};
    waterfallShowLegendGetter_ = {};
    waterfallShowLegendSetter_ = {};
    waterfallShowGridGetter_ = {};
    waterfallShowGridSetter_ = {};
    waterfallSamplePeriodGetter_ = {};
    waterfallSamplePeriodSetter_ = {};
    waterfallUnitsGetter_ = {};
    waterfallUnitsSetter_ = {};

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
    kExpressionChannel,
    kRectangle,
    kImage,
    kHeatmap,
    kWaterfallPlot,
    kPolygon,
    kComposite,
    kLine,
    kText,
    kTextEntry,
    kSetpointControl,
    kTextArea,
    kSlider,
    kWheelSwitch,
    kChoiceButton,
    kMenu,
    kMessageButton,
    kShellCommand,
    kRelatedDisplay,
    kPvTable,
    kWaveTable,
    kTextMonitor,
    kMeter,
    kBarMonitor,
    kThermometer,
    kScaleMonitor,
    kStripChart,
    kCartesianPlot,
    kByteMonitor,
    kLedMonitor
  };

  enum class LedColorModeChoice {
    kStatic,
    kAlarm,
    kBinary,
    kDiscrete,
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
        const bool isExpressionChannelEdit = std::find(
            expressionChannelChannelEdits_.begin(),
            expressionChannelChannelEdits_.end(), edit)
            != expressionChannelChannelEdits_.end();
        const bool isCompositeChannelEdit = std::find(
            compositeChannelEdits_.begin(), compositeChannelEdits_.end(), edit)
            != compositeChannelEdits_.end();
        const bool isThermometerVisibilityChannelEdit = std::find(
            thermometerVisibilityChannelEdits_.begin(),
            thermometerVisibilityChannelEdits_.end(), edit)
            != thermometerVisibilityChannelEdits_.end();
        const bool isLedVisibilityChannelEdit = std::find(
            ledVisibilityChannelEdits_.begin(),
            ledVisibilityChannelEdits_.end(), edit)
            != ledVisibilityChannelEdits_.end();
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
            || edit == waterfallTitleEdit_
            || edit == waterfallXLabelEdit_
            || edit == waterfallYLabelEdit_
            || edit == waterfallDataChannelEdit_
            || edit == waterfallCountChannelEdit_
            || edit == waterfallTriggerChannelEdit_
            || edit == waterfallEraseChannelEdit_
            || edit == waterfallHistoryEdit_
            || edit == waterfallIntensityMinEdit_
            || edit == waterfallIntensityMaxEdit_
            || edit == waterfallSamplePeriodEdit_
            || edit == expressionChannelVariableEdit_
            || edit == expressionChannelCalcEdit_
            || edit == imageVisibilityCalcEdit_
            || edit == thermometerVisibilityCalcEdit_
            || edit == ledVisibilityCalcEdit_
            || edit == lineLineWidthEdit_
            || edit == lineVisibilityCalcEdit_
            || isRectangleChannelEdit || isLineChannelEdit
            || isImageChannelEdit || isExpressionChannelEdit
            || isCompositeChannelEdit
            || isThermometerVisibilityChannelEdit
            || isLedVisibilityChannelEdit
            || isRelatedLabelEdit
            || isRelatedNameEdit || isRelatedArgsEdit
            || edit == relatedDisplayLabelEdit_) {
          revertLineEdit(edit);
        }
        if (edit == textMonitorChannelEdit_
            || edit == pvTableColumnsEdit_
            || edit == waveTableChannelEdit_
            || edit == waveTableColumnsEdit_
            || edit == waveTableMaxElementsEdit_
            || edit == setpointControlLabelEdit_
            || edit == setpointControlSetpointEdit_
            || edit == setpointControlReadbackEdit_
            || edit == setpointControlToleranceEdit_
            || edit == textAreaChannelEdit_
            || edit == textAreaWrapColumnWidthEdit_
            || edit == textAreaTabWidthEdit_
            || edit == textAreaFontFamilyEdit_
            || edit == meterChannelEdit_ || edit == thermometerChannelEdit_
            || edit == ledChannelEdit_
            || edit == sliderIncrementEdit_
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
    if (waterfallSection_) {
      const bool waterfallVisible = kind == SelectionKind::kWaterfallPlot;
      waterfallSection_->setVisible(waterfallVisible);
      waterfallSection_->setEnabled(waterfallVisible);
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
    if (setpointControlSection_) {
      const bool setpointVisible = kind == SelectionKind::kSetpointControl;
      setpointControlSection_->setVisible(setpointVisible);
      setpointControlSection_->setEnabled(setpointVisible);
    }
    if (textAreaSection_) {
      const bool textAreaVisible = kind == SelectionKind::kTextArea;
      textAreaSection_->setVisible(textAreaVisible);
      textAreaSection_->setEnabled(textAreaVisible);
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
    if (pvTableSection_) {
      const bool pvTableVisible = kind == SelectionKind::kPvTable;
      pvTableSection_->setVisible(pvTableVisible);
      pvTableSection_->setEnabled(pvTableVisible);
    }
    if (waveTableSection_) {
      const bool waveTableVisible = kind == SelectionKind::kWaveTable;
      waveTableSection_->setVisible(waveTableVisible);
      waveTableSection_->setEnabled(waveTableVisible);
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
    if (thermometerSection_) {
      const bool thermometerVisible = kind == SelectionKind::kThermometer;
      thermometerSection_->setVisible(thermometerVisible);
      thermometerSection_->setEnabled(thermometerVisible);
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
    if (ledSection_) {
      const bool ledVisible = kind == SelectionKind::kLedMonitor;
      ledSection_->setVisible(ledVisible);
      ledSection_->setEnabled(ledVisible);
    }
    if (expressionChannelSection_) {
      const bool expressionVisible = kind == SelectionKind::kExpressionChannel;
      expressionChannelSection_->setVisible(expressionVisible);
      expressionChannelSection_->setEnabled(expressionVisible);
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

  void updateWaterfallDependentControls()
  {
    bool hasEraseChannel = false;
    if (waterfallEraseChannelGetter_) {
      hasEraseChannel = !waterfallEraseChannelGetter_().trimmed().isEmpty();
    }
    if (!hasEraseChannel && waterfallEraseChannelEdit_) {
      hasEraseChannel = !waterfallEraseChannelEdit_->text().trimmed().isEmpty();
    }
    setFieldEnabled(waterfallEraseModeCombo_,
        hasEraseChannel && static_cast<bool>(waterfallEraseModeSetter_));

    const bool manualScale = waterfallIntensityScaleCombo_
        && waterfallIntensityScaleCombo_->currentIndex()
            == static_cast<int>(WaterfallIntensityScale::kManual);
    setFieldEnabled(waterfallIntensityMinEdit_,
        manualScale && static_cast<bool>(waterfallIntensityMinSetter_));
    setFieldEnabled(waterfallIntensityMaxEdit_,
        manualScale && static_cast<bool>(waterfallIntensityMaxSetter_));

    const bool grayscale = waterfallColorMapCombo_
        && waterfallColorMapCombo_->currentIndex()
            == static_cast<int>(HeatmapColorMap::kGrayscale);
    setFieldEnabled(waterfallInvertGreyscaleCombo_,
        grayscale && static_cast<bool>(waterfallInvertGreyscaleSetter_));
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

  void updateThermometerChannelDependentControls()
  {
    bool hasChannelA = false;
    if (thermometerVisibilityChannelGetters_[0]) {
      const QString value = thermometerVisibilityChannelGetters_[0]();
      hasChannelA = !value.trimmed().isEmpty();
    }
    if (!hasChannelA) {
      QLineEdit *channelEdit = thermometerVisibilityChannelEdits_[0];
      if (channelEdit) {
        hasChannelA = !channelEdit->text().trimmed().isEmpty();
      }
    }

    setFieldEnabled(thermometerVisibilityCombo_, hasChannelA);
    setFieldEnabled(thermometerVisibilityCalcEdit_, hasChannelA);
  }

  void refreshLedMonitorColorButtons()
  {
    if (ledOnColorButton_) {
      QColor color = ledOnColorGetter_ ? ledOnColorGetter_() : QColor();
      if (!color.isValid()) {
        color = MedmColors::alarmColorForSeverity(0);
      }
      setColorButtonColor(ledOnColorButton_, color);
    }

    if (ledOffColorButton_) {
      QColor color = ledOffColorGetter_ ? ledOffColorGetter_() : QColor();
      if (!color.isValid()) {
        color = QColor(45, 45, 45);
      }
      setColorButtonColor(ledOffColorButton_, color);
    }

    for (int i = 0; i < kLedStateCount; ++i) {
      QPushButton *button = ledStateColorButtons_[static_cast<std::size_t>(i)];
      if (!button) {
        continue;
      }
      QColor color = ledStateColorGetters_[static_cast<std::size_t>(i)]
          ? ledStateColorGetters_[static_cast<std::size_t>(i)]()
          : QColor();
      if (!color.isValid()) {
        color = QColor(204, 204, 204);
      }
      setColorButtonColor(button, color);
    }
  }

  void applyBinaryLedPreset()
  {
    if (ledStateCountSetter_) {
      ledStateCountSetter_(2);
    }

    if (ledStateColorSetters_[0]) {
      QColor offColor = ledOffColorGetter_ ? ledOffColorGetter_() : QColor();
      if (!offColor.isValid()) {
        offColor = QColor(45, 45, 45);
      }
      ledStateColorSetters_[0](offColor);
    }

    if (ledStateColorSetters_[1]) {
      QColor onColor = ledOnColorGetter_ ? ledOnColorGetter_() : QColor();
      if (!onColor.isValid()) {
        onColor = MedmColors::alarmColorForSeverity(0);
      }
      ledStateColorSetters_[1](onColor);
    }

    if (ledStateCountSpin_) {
      const QSignalBlocker blocker(ledStateCountSpin_);
      ledStateCountSpin_->setValue(2);
    }

    refreshLedMonitorColorButtons();
  }

  void updateLedMonitorStateColorControls()
  {
    const LedColorModeChoice choice = ledColorModeCombo_
        ? ledColorModeChoiceFromIndex(ledColorModeCombo_->currentIndex())
        : LedColorModeChoice::kAlarm;
    const bool discrete = choice == LedColorModeChoice::kBinary
        || choice == LedColorModeChoice::kDiscrete;
    int stateCount = ledStateCountGetter_ ? ledStateCountGetter_() : 2;
    if (ledStateCountSpin_) {
      stateCount = ledStateCountSpin_->value();
    }
    if (choice == LedColorModeChoice::kBinary) {
      stateCount = 2;
    }
    stateCount = std::clamp(stateCount, 1, kLedStateCount);

    setFieldEnabled(ledStateCountSpin_,
        choice == LedColorModeChoice::kDiscrete
        && static_cast<bool>(ledStateCountSetter_));
    setFieldEnabled(ledStateColorsWidget_, discrete);

    for (int i = 0; i < kLedStateCount; ++i) {
      QPushButton *button = ledStateColorButtons_[static_cast<std::size_t>(i)];
      if (!button) {
        continue;
      }
      const bool enabled = discrete
          && i < stateCount
          && static_cast<bool>(ledStateColorSetters_[static_cast<std::size_t>(i)]);
      button->setEnabled(enabled);
    }
  }

  void updateLedMonitorChannelDependentControls()
  {
    bool hasVisibilitySource = false;
    if (ledVisibilityChannelGetters_[0]) {
      const QString value = ledVisibilityChannelGetters_[0]();
      hasVisibilitySource = !value.trimmed().isEmpty();
    }
    if (!hasVisibilitySource && ledVisibilityChannelEdits_[0]) {
      hasVisibilitySource =
          !ledVisibilityChannelEdits_[0]->text().trimmed().isEmpty();
    }
    if (!hasVisibilitySource && ledChannelGetter_) {
      hasVisibilitySource = !ledChannelGetter_().trimmed().isEmpty();
    }
    if (!hasVisibilitySource && ledChannelEdit_) {
      hasVisibilitySource = !ledChannelEdit_->text().trimmed().isEmpty();
    }

    setFieldEnabled(ledVisibilityCombo_,
        hasVisibilitySource && static_cast<bool>(ledVisibilityModeSetter_));

    bool enableCalc = hasVisibilitySource
        && static_cast<bool>(ledVisibilityCalcSetter_);
    if (ledVisibilityCombo_) {
      enableCalc = enableCalc
          && visibilityModeFromIndex(ledVisibilityCombo_->currentIndex())
              == TextVisibilityMode::kCalc;
    }
    setFieldEnabled(ledVisibilityCalcEdit_, enableCalc);
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

  void updateTextAreaDependentControls()
  {
    bool wordWrap = true;
    if (textAreaWordWrapGetter_) {
      wordWrap = textAreaWordWrapGetter_();
    } else if (textAreaWordWrapCombo_) {
      wordWrap = textAreaWordWrapCombo_->currentIndex() != 0;
    }

    TextAreaWrapMode mode = TextAreaWrapMode::kWidgetWidth;
    if (textAreaWrapModeGetter_) {
      mode = textAreaWrapModeGetter_();
    } else if (textAreaLineWrapModeCombo_) {
      mode = textAreaWrapModeFromIndex(textAreaLineWrapModeCombo_->currentIndex());
    }

    const bool enableWrapColumn = wordWrap
        && mode == TextAreaWrapMode::kFixedColumnWidth
        && static_cast<bool>(textAreaWrapColumnWidthSetter_);
    setFieldEnabled(textAreaWrapColumnWidthEdit_, enableWrapColumn);

    const bool enableHorizontalScroll = (!wordWrap
            || mode == TextAreaWrapMode::kNoWrap)
        && static_cast<bool>(textAreaShowHorizontalScrollBarSetter_);
    setFieldEnabled(textAreaHorizontalScrollBarCombo_, enableHorizontalScroll);
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

  void commitSetpointControlLabel()
  {
    if (!setpointControlLabelEdit_) {
      return;
    }
    if (!setpointControlLabelSetter_) {
      revertLineEdit(setpointControlLabelEdit_);
      return;
    }
    const QString value = setpointControlLabelEdit_->text();
    setpointControlLabelSetter_(value);
    committedTexts_[setpointControlLabelEdit_] = value;
  }

  void commitSetpointControlSetpoint()
  {
    if (!setpointControlSetpointEdit_) {
      return;
    }
    if (!setpointControlSetpointSetter_) {
      revertLineEdit(setpointControlSetpointEdit_);
      return;
    }
    const QString value = setpointControlSetpointEdit_->text();
    setpointControlSetpointSetter_(value);
    committedTexts_[setpointControlSetpointEdit_] = value;
    updateSetpointControlLimitsFromDialog();
  }

  void commitSetpointControlReadback()
  {
    if (!setpointControlReadbackEdit_) {
      return;
    }
    if (!setpointControlReadbackSetter_) {
      revertLineEdit(setpointControlReadbackEdit_);
      return;
    }
    const QString value = setpointControlReadbackEdit_->text();
    setpointControlReadbackSetter_(value);
    committedTexts_[setpointControlReadbackEdit_] = value;
  }

  void commitSetpointControlTolerance()
  {
    if (!setpointControlToleranceEdit_) {
      return;
    }
    if (!setpointControlToleranceSetter_) {
      revertLineEdit(setpointControlToleranceEdit_);
      return;
    }
    bool ok = false;
    double value = setpointControlToleranceEdit_->text().toDouble(&ok);
    if (!ok || !std::isfinite(value) || value < 0.0) {
      revertLineEdit(setpointControlToleranceEdit_);
      return;
    }
    setpointControlToleranceSetter_(value);
    const QString text = QString::number(value, 'g', 12);
    setpointControlToleranceEdit_->setText(text);
    committedTexts_[setpointControlToleranceEdit_] = text;
  }

  void commitTextAreaChannel()
  {
    if (!textAreaChannelEdit_) {
      return;
    }
    if (!textAreaChannelSetter_) {
      revertLineEdit(textAreaChannelEdit_);
      return;
    }
    const QString value = textAreaChannelEdit_->text();
    textAreaChannelSetter_(value);
    committedTexts_[textAreaChannelEdit_] = value;
    updateTextAreaLimitsFromDialog();
  }

  void commitTextAreaWrapColumnWidth()
  {
    if (!textAreaWrapColumnWidthEdit_) {
      return;
    }
    if (!textAreaWrapColumnWidthSetter_) {
      revertLineEdit(textAreaWrapColumnWidthEdit_);
      return;
    }
    bool ok = false;
    int value = textAreaWrapColumnWidthEdit_->text().toInt(&ok);
    if (!ok) {
      revertLineEdit(textAreaWrapColumnWidthEdit_);
      return;
    }
    value = std::max(1, value);
    textAreaWrapColumnWidthSetter_(value);
    const int effectiveValue = textAreaWrapColumnWidthGetter_
        ? std::max(1, textAreaWrapColumnWidthGetter_())
        : value;
    const QSignalBlocker blocker(textAreaWrapColumnWidthEdit_);
    textAreaWrapColumnWidthEdit_->setText(QString::number(effectiveValue));
    committedTexts_[textAreaWrapColumnWidthEdit_]
        = textAreaWrapColumnWidthEdit_->text();
  }

  void commitTextAreaTabWidth()
  {
    if (!textAreaTabWidthEdit_) {
      return;
    }
    if (!textAreaTabWidthSetter_) {
      revertLineEdit(textAreaTabWidthEdit_);
      return;
    }
    bool ok = false;
    int value = textAreaTabWidthEdit_->text().toInt(&ok);
    if (!ok) {
      revertLineEdit(textAreaTabWidthEdit_);
      return;
    }
    value = std::max(1, value);
    textAreaTabWidthSetter_(value);
    const int effectiveValue = textAreaTabWidthGetter_
        ? std::max(1, textAreaTabWidthGetter_())
        : value;
    const QSignalBlocker blocker(textAreaTabWidthEdit_);
    textAreaTabWidthEdit_->setText(QString::number(effectiveValue));
    committedTexts_[textAreaTabWidthEdit_] = textAreaTabWidthEdit_->text();
  }

  void commitTextAreaFontFamily()
  {
    if (!textAreaFontFamilyEdit_) {
      return;
    }
    if (!textAreaFontFamilySetter_) {
      revertLineEdit(textAreaFontFamilyEdit_);
      return;
    }
    const QString value = textAreaFontFamilyEdit_->text();
    textAreaFontFamilySetter_(value);
    committedTexts_[textAreaFontFamilyEdit_] = value;
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

  void commitPvTableColumns()
  {
    if (!pvTableColumnsEdit_) {
      return;
    }
    if (!pvTableColumnsSetter_) {
      revertLineEdit(pvTableColumnsEdit_);
      return;
    }
    const QString value = pvTableColumnsEdit_->text();
    pvTableColumnsSetter_(value);
    committedTexts_[pvTableColumnsEdit_] = value;
  }

  void commitPvTableRowLabel(int index)
  {
    if (index < 0 || index >= kPvTableRowCount || !pvTableRowLabelEdits_[index]) {
      return;
    }
    if (!pvTableRowLabelSetters_[index]) {
      revertLineEdit(pvTableRowLabelEdits_[index]);
      return;
    }
    const QString value = pvTableRowLabelEdits_[index]->text();
    pvTableRowLabelSetters_[index](value);
    committedTexts_[pvTableRowLabelEdits_[index]] = value;
  }

  void commitPvTableRowChannel(int index)
  {
    if (index < 0 || index >= kPvTableRowCount || !pvTableRowChannelEdits_[index]) {
      return;
    }
    if (!pvTableRowChannelSetters_[index]) {
      revertLineEdit(pvTableRowChannelEdits_[index]);
      return;
    }
    const QString value = pvTableRowChannelEdits_[index]->text();
    pvTableRowChannelSetters_[index](value);
    committedTexts_[pvTableRowChannelEdits_[index]] = value;
  }

  void commitWaveTableChannel()
  {
    if (!waveTableChannelEdit_) {
      return;
    }
    if (!waveTableChannelSetter_) {
      revertLineEdit(waveTableChannelEdit_);
      return;
    }
    const QString value = waveTableChannelEdit_->text();
    waveTableChannelSetter_(value);
    const QString effective =
        waveTableChannelGetter_ ? waveTableChannelGetter_() : value;
    const QSignalBlocker blocker(waveTableChannelEdit_);
    waveTableChannelEdit_->setText(effective);
    committedTexts_[waveTableChannelEdit_] = effective;
  }

  void commitWaveTableColumns()
  {
    if (!waveTableColumnsEdit_) {
      return;
    }
    if (!waveTableColumnsSetter_) {
      revertLineEdit(waveTableColumnsEdit_);
      return;
    }
    bool ok = false;
    const int value = waveTableColumnsEdit_->text().toInt(&ok);
    if (!ok || value < 1) {
      revertLineEdit(waveTableColumnsEdit_);
      return;
    }
    waveTableColumnsSetter_(value);
    const int effective =
        waveTableColumnsGetter_ ? std::max(1, waveTableColumnsGetter_())
                                : value;
    const QString text = QString::number(effective);
    const QSignalBlocker blocker(waveTableColumnsEdit_);
    waveTableColumnsEdit_->setText(text);
    committedTexts_[waveTableColumnsEdit_] = text;
  }

  void commitWaveTableMaxElements()
  {
    if (!waveTableMaxElementsEdit_) {
      return;
    }
    if (!waveTableMaxElementsSetter_) {
      revertLineEdit(waveTableMaxElementsEdit_);
      return;
    }
    bool ok = false;
    const int value = waveTableMaxElementsEdit_->text().toInt(&ok);
    if (!ok || value < 0) {
      revertLineEdit(waveTableMaxElementsEdit_);
      return;
    }
    waveTableMaxElementsSetter_(value);
    const int effective =
        waveTableMaxElementsGetter_
            ? std::max(0, waveTableMaxElementsGetter_())
            : value;
    const QString text = QString::number(effective);
    const QSignalBlocker blocker(waveTableMaxElementsEdit_);
    waveTableMaxElementsEdit_->setText(text);
    committedTexts_[waveTableMaxElementsEdit_] = text;
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

  void commitThermometerChannel()
  {
    if (!thermometerChannelEdit_) {
      return;
    }
    if (!thermometerChannelSetter_) {
      revertLineEdit(thermometerChannelEdit_);
      return;
    }
    const QString value = thermometerChannelEdit_->text();
    thermometerChannelSetter_(value);
    committedTexts_[thermometerChannelEdit_] = value;
    updateThermometerChannelDependentControls();
    updateThermometerLimitsFromDialog();
  }

  void commitThermometerVisibilityCalc()
  {
    if (!thermometerVisibilityCalcEdit_) {
      return;
    }
    if (!thermometerVisibilityCalcSetter_) {
      revertLineEdit(thermometerVisibilityCalcEdit_);
      return;
    }
    const QString value = thermometerVisibilityCalcEdit_->text();
    thermometerVisibilityCalcSetter_(value);
    committedTexts_[thermometerVisibilityCalcEdit_] = value;
  }

  void commitThermometerVisibilityChannel(int index)
  {
    if (index < 0
        || index >= static_cast<int>(thermometerVisibilityChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = thermometerVisibilityChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!thermometerVisibilityChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    thermometerVisibilityChannelSetters_[index](value);
    committedTexts_[edit] = value;
    if (index == 0) {
      updateThermometerChannelDependentControls();
    }
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
    if (!ok || value < 0) {
      revertLineEdit(cartesianCountEdit_);
      return;
    }
    cartesianCountSetter_(value);
    const int effectiveCount = cartesianCountGetter_ ? cartesianCountGetter_()
                                                     : value;
    const QSignalBlocker blocker(cartesianCountEdit_);
    cartesianCountEdit_->setText(
        QString::number(std::max(effectiveCount, 0)));
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

  void commitLedChannel()
  {
    if (!ledChannelEdit_) {
      return;
    }
    if (!ledChannelSetter_) {
      revertLineEdit(ledChannelEdit_);
      return;
    }
    const QString value = ledChannelEdit_->text();
    ledChannelSetter_(value);
    committedTexts_[ledChannelEdit_] = value;
    updateLedMonitorChannelDependentControls();
  }

  void commitLedVisibilityCalc()
  {
    if (!ledVisibilityCalcEdit_) {
      return;
    }
    if (!ledVisibilityCalcSetter_) {
      revertLineEdit(ledVisibilityCalcEdit_);
      return;
    }
    const QString value = ledVisibilityCalcEdit_->text();
    ledVisibilityCalcSetter_(value);
    committedTexts_[ledVisibilityCalcEdit_] = value;
  }

  void commitLedVisibilityChannel(int index)
  {
    if (index < 0
        || index >= static_cast<int>(ledVisibilityChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = ledVisibilityChannelEdits_[static_cast<std::size_t>(index)];
    if (!edit) {
      return;
    }
    if (!ledVisibilityChannelSetters_[static_cast<std::size_t>(index)]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    ledVisibilityChannelSetters_[static_cast<std::size_t>(index)](value);
    committedTexts_[edit] = value;
    if (index == 0) {
      updateLedMonitorChannelDependentControls();
    }
  }

  void commitLedStateCount(int value)
  {
    if (!ledStateCountSpin_) {
      return;
    }
    if (!ledStateCountSetter_) {
      if (ledStateCountGetter_) {
        const QSignalBlocker blocker(ledStateCountSpin_);
        ledStateCountSpin_->setValue(std::clamp(ledStateCountGetter_(), 1,
            kLedStateCount));
      }
      updateLedMonitorStateColorControls();
      return;
    }
    value = std::clamp(value, 1, kLedStateCount);
    ledStateCountSetter_(value);
    if (ledStateCountGetter_) {
      const QSignalBlocker blocker(ledStateCountSpin_);
      ledStateCountSpin_->setValue(std::clamp(ledStateCountGetter_(), 1,
          kLedStateCount));
    }
    updateLedMonitorStateColorControls();
  }

  void commitExpressionChannelVariable()
  {
    if (!expressionChannelVariableEdit_) {
      return;
    }
    if (!expressionChannelVariableSetter_) {
      revertLineEdit(expressionChannelVariableEdit_);
      return;
    }
    const QString value = expressionChannelVariableEdit_->text();
    expressionChannelVariableSetter_(value);
    committedTexts_[expressionChannelVariableEdit_] = value;
  }

  void commitExpressionChannelCalc()
  {
    if (!expressionChannelCalcEdit_) {
      return;
    }
    if (!expressionChannelCalcSetter_) {
      revertLineEdit(expressionChannelCalcEdit_);
      return;
    }
    const QString value = expressionChannelCalcEdit_->text();
    expressionChannelCalcSetter_(value);
    committedTexts_[expressionChannelCalcEdit_] = value;
  }

  void commitExpressionChannelChannel(int index)
  {
    if (index < 0
        || index >= static_cast<int>(expressionChannelChannelEdits_.size())) {
      return;
    }
    QLineEdit *edit = expressionChannelChannelEdits_[index];
    if (!edit) {
      return;
    }
    if (!expressionChannelChannelSetters_[index]) {
      revertLineEdit(edit);
      return;
    }
    const QString value = edit->text();
    expressionChannelChannelSetters_[index](value);
    committedTexts_[edit] = value;
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
    for (int i = 0; i < static_cast<int>(cartesianTraceSideCombos_.size()); ++i) {
      if (!cartesianTraceSideCombos_[i]) {
        continue;
      }
      const QSignalBlocker blocker(cartesianTraceSideCombos_[i]);
      const bool usesRight = cartesianTraceSideGetters_[i]
              ? cartesianTraceSideGetters_[i]()
              : false;
      cartesianTraceSideCombos_[i]->setCurrentIndex(usesRight ? 1 : 0);
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
    for (int i = 0; i < static_cast<int>(cartesianTraceSideCombos_.size()); ++i) {
      if (!cartesianTraceSideCombos_[i]) {
        continue;
      }
      const QSignalBlocker blocker(cartesianTraceSideCombos_[i]);
      const bool currentUsesRight = cartesianTraceSideGetters_[i]
              ? cartesianTraceSideGetters_[i]()
              : false;
      cartesianTraceSideCombos_[i]->setCurrentIndex(
          currentUsesRight ? 1 : 0);
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
    if (!pvLimitsDialog_) {
      return;
    }
    if (textEntryPrecisionSourceGetter_) {
      const QString channelLabel = textEntryChannelGetter_
              ? textEntryChannelGetter_()
              : QString();
      pvLimitsDialog_->setTextEntryCallbacks(channelLabel,
          textEntryPrecisionSourceGetter_, textEntryPrecisionSourceSetter_,
          textEntryPrecisionDefaultGetter_,
          textEntryPrecisionDefaultSetter_,
          [this]() { updateTextEntryLimitsFromDialog(); },
          textEntryLimitsGetter_, textEntryLimitsSetter_);
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateSetpointControlLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (setpointControlPrecisionSourceGetter_) {
      const QString channelLabel = setpointControlSetpointGetter_
              ? setpointControlSetpointGetter_()
              : QString();
      pvLimitsDialog_->setTextEntryCallbacks(channelLabel,
          setpointControlPrecisionSourceGetter_,
          setpointControlPrecisionSourceSetter_,
          setpointControlPrecisionDefaultGetter_,
          setpointControlPrecisionDefaultSetter_,
          [this]() { updateSetpointControlLimitsFromDialog(); },
          setpointControlLimitsGetter_, setpointControlLimitsSetter_);
    } else {
      pvLimitsDialog_->clearTargets();
    }
  }

  void updateTextAreaLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (textAreaPrecisionSourceGetter_) {
      const QString channelLabel = textAreaChannelGetter_
              ? textAreaChannelGetter_()
              : QString();
      pvLimitsDialog_->setTextEntryCallbacks(channelLabel,
          textAreaPrecisionSourceGetter_, textAreaPrecisionSourceSetter_,
          textAreaPrecisionDefaultGetter_,
          textAreaPrecisionDefaultSetter_,
          [this]() { updateTextAreaLimitsFromDialog(); },
          textAreaLimitsGetter_, textAreaLimitsSetter_);
    } else {
      pvLimitsDialog_->clearTargets();
    }
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

  void updateThermometerLimitsFromDialog()
  {
    if (!pvLimitsDialog_) {
      return;
    }
    if (thermometerLimitsGetter_ && thermometerLimitsSetter_) {
      const QString channelLabel = thermometerChannelGetter_
          ? thermometerChannelGetter_()
          : QString();
      pvLimitsDialog_->setThermometerCallbacks(channelLabel,
          thermometerLimitsGetter_, thermometerLimitsSetter_,
          [this]() { updateThermometerLimitsFromDialog(); });
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

  void commitWaterfallTitle()
  {
    if (!waterfallTitleEdit_) {
      return;
    }
    if (!waterfallTitleSetter_) {
      revertLineEdit(waterfallTitleEdit_);
      return;
    }
    const QString value = waterfallTitleEdit_->text();
    waterfallTitleSetter_(value);
    committedTexts_[waterfallTitleEdit_] = value;
  }

  void commitWaterfallXLabel()
  {
    if (!waterfallXLabelEdit_) {
      return;
    }
    if (!waterfallXLabelSetter_) {
      revertLineEdit(waterfallXLabelEdit_);
      return;
    }
    const QString value = waterfallXLabelEdit_->text();
    waterfallXLabelSetter_(value);
    committedTexts_[waterfallXLabelEdit_] = value;
  }

  void commitWaterfallYLabel()
  {
    if (!waterfallYLabelEdit_) {
      return;
    }
    if (!waterfallYLabelSetter_) {
      revertLineEdit(waterfallYLabelEdit_);
      return;
    }
    const QString value = waterfallYLabelEdit_->text();
    waterfallYLabelSetter_(value);
    committedTexts_[waterfallYLabelEdit_] = value;
  }

  void commitWaterfallDataChannel()
  {
    if (!waterfallDataChannelEdit_) {
      return;
    }
    if (!waterfallDataChannelSetter_) {
      revertLineEdit(waterfallDataChannelEdit_);
      return;
    }
    const QString value = waterfallDataChannelEdit_->text();
    waterfallDataChannelSetter_(value);
    committedTexts_[waterfallDataChannelEdit_] = value;
  }

  void commitWaterfallCountChannel()
  {
    if (!waterfallCountChannelEdit_) {
      return;
    }
    if (!waterfallCountChannelSetter_) {
      revertLineEdit(waterfallCountChannelEdit_);
      return;
    }
    const QString value = waterfallCountChannelEdit_->text();
    waterfallCountChannelSetter_(value);
    committedTexts_[waterfallCountChannelEdit_] = value;
  }

  void commitWaterfallTriggerChannel()
  {
    if (!waterfallTriggerChannelEdit_) {
      return;
    }
    if (!waterfallTriggerChannelSetter_) {
      revertLineEdit(waterfallTriggerChannelEdit_);
      return;
    }
    const QString value = waterfallTriggerChannelEdit_->text();
    waterfallTriggerChannelSetter_(value);
    committedTexts_[waterfallTriggerChannelEdit_] = value;
  }

  void commitWaterfallEraseChannel()
  {
    if (!waterfallEraseChannelEdit_) {
      return;
    }
    if (!waterfallEraseChannelSetter_) {
      revertLineEdit(waterfallEraseChannelEdit_);
      return;
    }
    const QString value = waterfallEraseChannelEdit_->text();
    waterfallEraseChannelSetter_(value);
    committedTexts_[waterfallEraseChannelEdit_] = value;
    updateWaterfallDependentControls();
  }

  void commitWaterfallHistoryCount()
  {
    if (!waterfallHistoryEdit_) {
      return;
    }
    if (!waterfallHistoryCountSetter_) {
      revertLineEdit(waterfallHistoryEdit_);
      return;
    }
    bool ok = false;
    const int value = waterfallHistoryEdit_->text().toInt(&ok);
    if (!ok || value < 1 || value > kWaterfallMaxHistory) {
      revertLineEdit(waterfallHistoryEdit_);
      return;
    }
    waterfallHistoryCountSetter_(value);
    const int effective = waterfallHistoryCountGetter_
        ? std::clamp(waterfallHistoryCountGetter_(), 1, kWaterfallMaxHistory)
        : value;
    const QSignalBlocker blocker(waterfallHistoryEdit_);
    waterfallHistoryEdit_->setText(QString::number(effective));
    committedTexts_[waterfallHistoryEdit_] = waterfallHistoryEdit_->text();
  }

  void commitWaterfallIntensityMinimum()
  {
    if (!waterfallIntensityMinEdit_) {
      return;
    }
    if (!waterfallIntensityMinSetter_) {
      revertLineEdit(waterfallIntensityMinEdit_);
      return;
    }
    bool ok = false;
    const double value = waterfallIntensityMinEdit_->text().toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
      revertLineEdit(waterfallIntensityMinEdit_);
      return;
    }
    waterfallIntensityMinSetter_(value);
    committedTexts_[waterfallIntensityMinEdit_] =
        waterfallIntensityMinEdit_->text();
  }

  void commitWaterfallIntensityMaximum()
  {
    if (!waterfallIntensityMaxEdit_) {
      return;
    }
    if (!waterfallIntensityMaxSetter_) {
      revertLineEdit(waterfallIntensityMaxEdit_);
      return;
    }
    bool ok = false;
    const double value = waterfallIntensityMaxEdit_->text().toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
      revertLineEdit(waterfallIntensityMaxEdit_);
      return;
    }
    waterfallIntensityMaxSetter_(value);
    committedTexts_[waterfallIntensityMaxEdit_] =
        waterfallIntensityMaxEdit_->text();
  }

  void commitWaterfallSamplePeriod()
  {
    if (!waterfallSamplePeriodEdit_) {
      return;
    }
    if (!waterfallSamplePeriodSetter_) {
      revertLineEdit(waterfallSamplePeriodEdit_);
      return;
    }
    bool ok = false;
    const double value = waterfallSamplePeriodEdit_->text().toDouble(&ok);
    if (!ok || !std::isfinite(value) || value < 0.0) {
      revertLineEdit(waterfallSamplePeriodEdit_);
      return;
    }
    waterfallSamplePeriodSetter_(value);
    committedTexts_[waterfallSamplePeriodEdit_] =
        waterfallSamplePeriodEdit_->text();
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

  TextAreaWrapMode textAreaWrapModeFromIndex(int index) const
  {
    switch (index) {
    case 0:
      return TextAreaWrapMode::kNoWrap;
    case 2:
      return TextAreaWrapMode::kFixedColumnWidth;
    case 1:
    default:
      return TextAreaWrapMode::kWidgetWidth;
    }
  }

  int textAreaWrapModeToIndex(TextAreaWrapMode mode) const
  {
    switch (mode) {
    case TextAreaWrapMode::kNoWrap:
      return 0;
    case TextAreaWrapMode::kFixedColumnWidth:
      return 2;
    case TextAreaWrapMode::kWidgetWidth:
    default:
      return 1;
    }
  }

  TextAreaCommitMode textAreaCommitModeFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return TextAreaCommitMode::kEnter;
    case 2:
      return TextAreaCommitMode::kOnFocusLost;
    case 3:
      return TextAreaCommitMode::kExplicit;
    case 0:
    default:
      return TextAreaCommitMode::kCtrlEnter;
    }
  }

  int textAreaCommitModeToIndex(TextAreaCommitMode mode) const
  {
    switch (mode) {
    case TextAreaCommitMode::kEnter:
      return 1;
    case TextAreaCommitMode::kOnFocusLost:
      return 2;
    case TextAreaCommitMode::kExplicit:
      return 3;
    case TextAreaCommitMode::kCtrlEnter:
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

  WaveTableLayout waveTableLayoutFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return WaveTableLayout::kColumn;
    case 2:
      return WaveTableLayout::kRow;
    case 0:
    default:
      return WaveTableLayout::kGrid;
    }
  }

  int waveTableLayoutToIndex(WaveTableLayout layout) const
  {
    switch (layout) {
    case WaveTableLayout::kColumn:
      return 1;
    case WaveTableLayout::kRow:
      return 2;
    case WaveTableLayout::kGrid:
    default:
      return 0;
    }
  }

  WaveTableValueFormat waveTableValueFormatFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return WaveTableValueFormat::kFixed;
    case 2:
      return WaveTableValueFormat::kScientific;
    case 3:
      return WaveTableValueFormat::kHex;
    case 4:
      return WaveTableValueFormat::kEngineering;
    case 0:
    default:
      return WaveTableValueFormat::kDefault;
    }
  }

  int waveTableValueFormatToIndex(WaveTableValueFormat format) const
  {
    switch (format) {
    case WaveTableValueFormat::kFixed:
      return 1;
    case WaveTableValueFormat::kScientific:
      return 2;
    case WaveTableValueFormat::kHex:
      return 3;
    case WaveTableValueFormat::kEngineering:
      return 4;
    case WaveTableValueFormat::kDefault:
    default:
      return 0;
    }
  }

  WaveTableCharMode waveTableCharModeFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return WaveTableCharMode::kBytes;
    case 2:
      return WaveTableCharMode::kAscii;
    case 3:
      return WaveTableCharMode::kNumeric;
    case 0:
    default:
      return WaveTableCharMode::kString;
    }
  }

  int waveTableCharModeToIndex(WaveTableCharMode mode) const
  {
    switch (mode) {
    case WaveTableCharMode::kBytes:
      return 1;
    case WaveTableCharMode::kAscii:
      return 2;
    case WaveTableCharMode::kNumeric:
      return 3;
    case WaveTableCharMode::kString:
    default:
      return 0;
    }
  }

  LedColorModeChoice ledColorModeChoiceFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return LedColorModeChoice::kAlarm;
    case 2:
      return LedColorModeChoice::kBinary;
    case 3:
      return LedColorModeChoice::kDiscrete;
    case 0:
    default:
      return LedColorModeChoice::kStatic;
    }
  }

  int ledColorModeChoiceToIndex(LedColorModeChoice choice) const
  {
    switch (choice) {
    case LedColorModeChoice::kAlarm:
      return 1;
    case LedColorModeChoice::kBinary:
      return 2;
    case LedColorModeChoice::kDiscrete:
      return 3;
    case LedColorModeChoice::kStatic:
    default:
      return 0;
    }
  }

  LedColorModeChoice ledColorModeChoiceForState(TextColorMode mode,
      int stateCount) const
  {
    switch (mode) {
    case TextColorMode::kAlarm:
      return LedColorModeChoice::kAlarm;
    case TextColorMode::kDiscrete:
      return stateCount == 2 ? LedColorModeChoice::kBinary
                             : LedColorModeChoice::kDiscrete;
    case TextColorMode::kStatic:
    default:
      return LedColorModeChoice::kStatic;
    }
  }

  TextColorMode ledTextColorModeForChoice(LedColorModeChoice choice) const
  {
    switch (choice) {
    case LedColorModeChoice::kAlarm:
      return TextColorMode::kAlarm;
    case LedColorModeChoice::kBinary:
    case LedColorModeChoice::kDiscrete:
      return TextColorMode::kDiscrete;
    case LedColorModeChoice::kStatic:
    default:
      return TextColorMode::kStatic;
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

  HeatmapProfileMode heatmapProfileModeFromIndex(int index) const
  {
    return index == 1 ? HeatmapProfileMode::kAveraged
                      : HeatmapProfileMode::kAbsolute;
  }

  int heatmapProfileModeToIndex(HeatmapProfileMode mode) const
  {
    return mode == HeatmapProfileMode::kAveraged ? 1 : 0;
  }

  bool heatmapBoolFromIndex(int index) const
  {
    return index == 1;
  }

  int heatmapBoolToIndex(bool value) const
  {
    return value ? 1 : 0;
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

  LedShape ledShapeFromIndex(int index) const
  {
    switch (index) {
    case 1:
      return LedShape::kSquare;
    case 2:
      return LedShape::kRoundedSquare;
    case 0:
    default:
      return LedShape::kCircle;
    }
  }

  int ledShapeToIndex(LedShape shape) const
  {
    switch (shape) {
    case LedShape::kSquare:
      return 1;
    case LedShape::kRoundedSquare:
      return 2;
    case LedShape::kCircle:
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

  ExpressionChannelEventSignalMode expressionChannelEventSignalFromIndex(
      int index) const
  {
    switch (index) {
    case 0:
      return ExpressionChannelEventSignalMode::kNever;
    case 1:
      return ExpressionChannelEventSignalMode::kOnFirstChange;
    case 3:
      return ExpressionChannelEventSignalMode::kTriggerZeroToOne;
    case 4:
      return ExpressionChannelEventSignalMode::kTriggerOneToZero;
    case 2:
    default:
      return ExpressionChannelEventSignalMode::kOnAnyChange;
    }
  }

  int expressionChannelEventSignalToIndex(
      ExpressionChannelEventSignalMode mode) const
  {
    switch (mode) {
    case ExpressionChannelEventSignalMode::kNever:
      return 0;
    case ExpressionChannelEventSignalMode::kOnFirstChange:
      return 1;
    case ExpressionChannelEventSignalMode::kTriggerZeroToOne:
      return 3;
    case ExpressionChannelEventSignalMode::kTriggerOneToZero:
      return 4;
    case ExpressionChannelEventSignalMode::kOnAnyChange:
    default:
      return 2;
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
  QWidget *waterfallSection_ = nullptr;
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
  QComboBox *heatmapColorMapCombo_ = nullptr;
  QComboBox *heatmapInvertGreyscaleCombo_ = nullptr;
  QComboBox *heatmapPreserveAspectRatioCombo_ = nullptr;
  QComboBox *heatmapFlipHorizontalCombo_ = nullptr;
  QComboBox *heatmapFlipVerticalCombo_ = nullptr;
  QComboBox *heatmapRotationCombo_ = nullptr;
  QComboBox *heatmapProfileModeCombo_ = nullptr;
  QComboBox *heatmapShowTopProfileCombo_ = nullptr;
  QComboBox *heatmapShowRightProfileCombo_ = nullptr;
  QPushButton *waterfallForegroundButton_ = nullptr;
  QPushButton *waterfallBackgroundButton_ = nullptr;
  QLineEdit *waterfallTitleEdit_ = nullptr;
  QLineEdit *waterfallXLabelEdit_ = nullptr;
  QLineEdit *waterfallYLabelEdit_ = nullptr;
  QLineEdit *waterfallDataChannelEdit_ = nullptr;
  QLineEdit *waterfallCountChannelEdit_ = nullptr;
  QLineEdit *waterfallTriggerChannelEdit_ = nullptr;
  QLineEdit *waterfallEraseChannelEdit_ = nullptr;
  QComboBox *waterfallEraseModeCombo_ = nullptr;
  QLineEdit *waterfallHistoryEdit_ = nullptr;
  QComboBox *waterfallScrollDirectionCombo_ = nullptr;
  QComboBox *waterfallColorMapCombo_ = nullptr;
  QComboBox *waterfallInvertGreyscaleCombo_ = nullptr;
  QComboBox *waterfallIntensityScaleCombo_ = nullptr;
  QLineEdit *waterfallIntensityMinEdit_ = nullptr;
  QLineEdit *waterfallIntensityMaxEdit_ = nullptr;
  QComboBox *waterfallShowLegendCombo_ = nullptr;
  QComboBox *waterfallShowGridCombo_ = nullptr;
  QLineEdit *waterfallSamplePeriodEdit_ = nullptr;
  QComboBox *waterfallUnitsCombo_ = nullptr;
  QWidget *pvTableSection_ = nullptr;
  QPushButton *pvTableForegroundButton_ = nullptr;
  QPushButton *pvTableBackgroundButton_ = nullptr;
  QComboBox *pvTableColorModeCombo_ = nullptr;
  QComboBox *pvTableShowHeadersCombo_ = nullptr;
  QLineEdit *pvTableColumnsEdit_ = nullptr;
  QWidget *pvTableRowsWidget_ = nullptr;
  std::array<QLineEdit *, kPvTableRowCount> pvTableRowLabelEdits_{};
  std::array<QLineEdit *, kPvTableRowCount> pvTableRowChannelEdits_{};
  QWidget *waveTableSection_ = nullptr;
  QPushButton *waveTableForegroundButton_ = nullptr;
  QPushButton *waveTableBackgroundButton_ = nullptr;
  QComboBox *waveTableColorModeCombo_ = nullptr;
  QComboBox *waveTableShowHeadersCombo_ = nullptr;
  QLineEdit *waveTableChannelEdit_ = nullptr;
  QComboBox *waveTableLayoutCombo_ = nullptr;
  QLineEdit *waveTableColumnsEdit_ = nullptr;
  QLineEdit *waveTableMaxElementsEdit_ = nullptr;
  QComboBox *waveTableIndexBaseCombo_ = nullptr;
  QComboBox *waveTableValueFormatCombo_ = nullptr;
  QComboBox *waveTableCharModeCombo_ = nullptr;
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
  QWidget *setpointControlSection_ = nullptr;
  QPushButton *setpointControlForegroundButton_ = nullptr;
  QPushButton *setpointControlBackgroundButton_ = nullptr;
  QComboBox *setpointControlFormatCombo_ = nullptr;
  QComboBox *setpointControlColorModeCombo_ = nullptr;
  QLineEdit *setpointControlLabelEdit_ = nullptr;
  QLineEdit *setpointControlSetpointEdit_ = nullptr;
  QLineEdit *setpointControlReadbackEdit_ = nullptr;
  QComboBox *setpointControlToleranceModeCombo_ = nullptr;
  QLineEdit *setpointControlToleranceEdit_ = nullptr;
  QComboBox *setpointControlShowReadbackCombo_ = nullptr;
  QPushButton *setpointControlPvLimitsButton_ = nullptr;
  QWidget *textAreaSection_ = nullptr;
  QPushButton *textAreaForegroundButton_ = nullptr;
  QPushButton *textAreaBackgroundButton_ = nullptr;
  QComboBox *textAreaFormatCombo_ = nullptr;
  QComboBox *textAreaColorModeCombo_ = nullptr;
  QLineEdit *textAreaChannelEdit_ = nullptr;
  QPushButton *textAreaPvLimitsButton_ = nullptr;
  QComboBox *textAreaReadOnlyCombo_ = nullptr;
  QComboBox *textAreaWordWrapCombo_ = nullptr;
  QComboBox *textAreaLineWrapModeCombo_ = nullptr;
  QLineEdit *textAreaWrapColumnWidthEdit_ = nullptr;
  QComboBox *textAreaVerticalScrollBarCombo_ = nullptr;
  QComboBox *textAreaHorizontalScrollBarCombo_ = nullptr;
  QComboBox *textAreaCommitModeCombo_ = nullptr;
  QComboBox *textAreaTabInsertsSpacesCombo_ = nullptr;
  QLineEdit *textAreaTabWidthEdit_ = nullptr;
  QLineEdit *textAreaFontFamilyEdit_ = nullptr;
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
  QWidget *thermometerSection_ = nullptr;
  QPushButton *thermometerForegroundButton_ = nullptr;
  QPushButton *thermometerBackgroundButton_ = nullptr;
  QPushButton *thermometerTextButton_ = nullptr;
  QComboBox *thermometerLabelCombo_ = nullptr;
  QComboBox *thermometerColorModeCombo_ = nullptr;
  QComboBox *thermometerFormatCombo_ = nullptr;
  QComboBox *thermometerShowValueCombo_ = nullptr;
  QComboBox *thermometerVisibilityCombo_ = nullptr;
  QLineEdit *thermometerVisibilityCalcEdit_ = nullptr;
  QLineEdit *thermometerChannelEdit_ = nullptr;
  std::array<QLineEdit *, 4> thermometerVisibilityChannelEdits_{};
  QPushButton *thermometerPvLimitsButton_ = nullptr;
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
  QComboBox *cartesianDrawMajorCombo_ = nullptr;
  QComboBox *cartesianDrawMinorCombo_ = nullptr;
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
  QWidget *ledSection_ = nullptr;
  QPushButton *ledForegroundButton_ = nullptr;
  QPushButton *ledBackgroundButton_ = nullptr;
  QPushButton *ledOnColorButton_ = nullptr;
  QPushButton *ledOffColorButton_ = nullptr;
  QPushButton *ledUndefinedColorButton_ = nullptr;
  QComboBox *ledColorModeCombo_ = nullptr;
  QComboBox *ledShapeCombo_ = nullptr;
  QCheckBox *ledBezelCheckBox_ = nullptr;
  QSpinBox *ledStateCountSpin_ = nullptr;
  QWidget *ledStateColorsWidget_ = nullptr;
  std::array<QPushButton *, kLedStateCount> ledStateColorButtons_{};
  QLineEdit *ledChannelEdit_ = nullptr;
  QComboBox *ledVisibilityCombo_ = nullptr;
  QLineEdit *ledVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> ledVisibilityChannelEdits_{};
  QWidget *expressionChannelSection_ = nullptr;
  QPushButton *expressionChannelForegroundButton_ = nullptr;
  QPushButton *expressionChannelBackgroundButton_ = nullptr;
  QLineEdit *expressionChannelVariableEdit_ = nullptr;
  QLineEdit *expressionChannelCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> expressionChannelChannelEdits_{};
  QDoubleSpinBox *expressionChannelInitialValueSpin_ = nullptr;
  QComboBox *expressionChannelEventSignalCombo_ = nullptr;
  QSpinBox *expressionChannelPrecisionSpin_ = nullptr;
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
    if (setpointControlLabelEdit_) {
      committedTexts_[setpointControlLabelEdit_] =
          setpointControlLabelEdit_->text();
    }
    if (setpointControlSetpointEdit_) {
      committedTexts_[setpointControlSetpointEdit_] =
          setpointControlSetpointEdit_->text();
    }
    if (setpointControlReadbackEdit_) {
      committedTexts_[setpointControlReadbackEdit_] =
          setpointControlReadbackEdit_->text();
    }
    if (setpointControlToleranceEdit_) {
      committedTexts_[setpointControlToleranceEdit_] =
          setpointControlToleranceEdit_->text();
    }
    if (textAreaChannelEdit_) {
      committedTexts_[textAreaChannelEdit_] = textAreaChannelEdit_->text();
    }
    if (textAreaWrapColumnWidthEdit_) {
      committedTexts_[textAreaWrapColumnWidthEdit_]
          = textAreaWrapColumnWidthEdit_->text();
    }
    if (textAreaTabWidthEdit_) {
      committedTexts_[textAreaTabWidthEdit_] = textAreaTabWidthEdit_->text();
    }
    if (textAreaFontFamilyEdit_) {
      committedTexts_[textAreaFontFamilyEdit_] = textAreaFontFamilyEdit_->text();
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
    if (thermometerVisibilityCalcEdit_) {
      committedTexts_[thermometerVisibilityCalcEdit_]
          = thermometerVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : thermometerVisibilityChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
    }
    if (ledChannelEdit_) {
      committedTexts_[ledChannelEdit_] = ledChannelEdit_->text();
    }
    if (ledVisibilityCalcEdit_) {
      committedTexts_[ledVisibilityCalcEdit_] =
          ledVisibilityCalcEdit_->text();
    }
    for (QLineEdit *edit : ledVisibilityChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
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
    if (waterfallTitleEdit_) {
      committedTexts_[waterfallTitleEdit_] = waterfallTitleEdit_->text();
    }
    if (waterfallXLabelEdit_) {
      committedTexts_[waterfallXLabelEdit_] = waterfallXLabelEdit_->text();
    }
    if (waterfallYLabelEdit_) {
      committedTexts_[waterfallYLabelEdit_] = waterfallYLabelEdit_->text();
    }
    if (waterfallDataChannelEdit_) {
      committedTexts_[waterfallDataChannelEdit_] = waterfallDataChannelEdit_->text();
    }
    if (waterfallCountChannelEdit_) {
      committedTexts_[waterfallCountChannelEdit_] = waterfallCountChannelEdit_->text();
    }
    if (waterfallTriggerChannelEdit_) {
      committedTexts_[waterfallTriggerChannelEdit_] = waterfallTriggerChannelEdit_->text();
    }
    if (waterfallEraseChannelEdit_) {
      committedTexts_[waterfallEraseChannelEdit_] = waterfallEraseChannelEdit_->text();
    }
    if (waterfallHistoryEdit_) {
      committedTexts_[waterfallHistoryEdit_] = waterfallHistoryEdit_->text();
    }
    if (waterfallIntensityMinEdit_) {
      committedTexts_[waterfallIntensityMinEdit_] = waterfallIntensityMinEdit_->text();
    }
    if (waterfallIntensityMaxEdit_) {
      committedTexts_[waterfallIntensityMaxEdit_] = waterfallIntensityMaxEdit_->text();
    }
    if (waterfallSamplePeriodEdit_) {
      committedTexts_[waterfallSamplePeriodEdit_] = waterfallSamplePeriodEdit_->text();
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
    if (thermometerChannelEdit_) {
      committedTexts_[thermometerChannelEdit_] = thermometerChannelEdit_->text();
    }
    if (byteChannelEdit_) {
      committedTexts_[byteChannelEdit_] = byteChannelEdit_->text();
    }
    if (expressionChannelVariableEdit_) {
      committedTexts_[expressionChannelVariableEdit_]
          = expressionChannelVariableEdit_->text();
    }
    if (expressionChannelCalcEdit_) {
      committedTexts_[expressionChannelCalcEdit_]
          = expressionChannelCalcEdit_->text();
    }
    for (QLineEdit *edit : expressionChannelChannelEdits_) {
      if (edit) {
        committedTexts_[edit] = edit->text();
      }
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
  std::function<QColor()> pvTableForegroundGetter_;
  std::function<void(const QColor &)> pvTableForegroundSetter_;
  std::function<QColor()> pvTableBackgroundGetter_;
  std::function<void(const QColor &)> pvTableBackgroundSetter_;
  std::function<TextColorMode()> pvTableColorModeGetter_;
  std::function<void(TextColorMode)> pvTableColorModeSetter_;
  std::function<bool()> pvTableShowHeadersGetter_;
  std::function<void(bool)> pvTableShowHeadersSetter_;
  std::function<QString()> pvTableColumnsGetter_;
  std::function<void(const QString &)> pvTableColumnsSetter_;
  std::array<std::function<QString()>, kPvTableRowCount> pvTableRowLabelGetters_{};
  std::array<std::function<void(const QString &)>, kPvTableRowCount> pvTableRowLabelSetters_{};
  std::array<std::function<QString()>, kPvTableRowCount> pvTableRowChannelGetters_{};
  std::array<std::function<void(const QString &)>, kPvTableRowCount> pvTableRowChannelSetters_{};
  std::function<QColor()> waveTableForegroundGetter_;
  std::function<void(const QColor &)> waveTableForegroundSetter_;
  std::function<QColor()> waveTableBackgroundGetter_;
  std::function<void(const QColor &)> waveTableBackgroundSetter_;
  std::function<TextColorMode()> waveTableColorModeGetter_;
  std::function<void(TextColorMode)> waveTableColorModeSetter_;
  std::function<bool()> waveTableShowHeadersGetter_;
  std::function<void(bool)> waveTableShowHeadersSetter_;
  std::function<QString()> waveTableChannelGetter_;
  std::function<void(const QString &)> waveTableChannelSetter_;
  std::function<WaveTableLayout()> waveTableLayoutGetter_;
  std::function<void(WaveTableLayout)> waveTableLayoutSetter_;
  std::function<int()> waveTableColumnsGetter_;
  std::function<void(int)> waveTableColumnsSetter_;
  std::function<int()> waveTableMaxElementsGetter_;
  std::function<void(int)> waveTableMaxElementsSetter_;
  std::function<int()> waveTableIndexBaseGetter_;
  std::function<void(int)> waveTableIndexBaseSetter_;
  std::function<WaveTableValueFormat()> waveTableValueFormatGetter_;
  std::function<void(WaveTableValueFormat)> waveTableValueFormatSetter_;
  std::function<WaveTableCharMode()> waveTableCharModeGetter_;
  std::function<void(WaveTableCharMode)> waveTableCharModeSetter_;
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
  std::function<QColor()> setpointControlForegroundGetter_;
  std::function<void(const QColor &)> setpointControlForegroundSetter_;
  std::function<QColor()> setpointControlBackgroundGetter_;
  std::function<void(const QColor &)> setpointControlBackgroundSetter_;
  std::function<TextMonitorFormat()> setpointControlFormatGetter_;
  std::function<void(TextMonitorFormat)> setpointControlFormatSetter_;
  std::function<int()> setpointControlPrecisionGetter_;
  std::function<void(int)> setpointControlPrecisionSetter_;
  std::function<PvLimitSource()> setpointControlPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> setpointControlPrecisionSourceSetter_;
  std::function<int()> setpointControlPrecisionDefaultGetter_;
  std::function<void(int)> setpointControlPrecisionDefaultSetter_;
  std::function<TextColorMode()> setpointControlColorModeGetter_;
  std::function<void(TextColorMode)> setpointControlColorModeSetter_;
  std::function<QString()> setpointControlSetpointGetter_;
  std::function<void(const QString &)> setpointControlSetpointSetter_;
  std::function<QString()> setpointControlReadbackGetter_;
  std::function<void(const QString &)> setpointControlReadbackSetter_;
  std::function<QString()> setpointControlLabelGetter_;
  std::function<void(const QString &)> setpointControlLabelSetter_;
  std::function<PvLimits()> setpointControlLimitsGetter_;
  std::function<void(const PvLimits &)> setpointControlLimitsSetter_;
  std::function<SetpointToleranceMode()> setpointControlToleranceModeGetter_;
  std::function<void(SetpointToleranceMode)> setpointControlToleranceModeSetter_;
  std::function<double()> setpointControlToleranceGetter_;
  std::function<void(double)> setpointControlToleranceSetter_;
  std::function<bool()> setpointControlShowReadbackGetter_;
  std::function<void(bool)> setpointControlShowReadbackSetter_;
  std::function<QColor()> textAreaForegroundGetter_;
  std::function<void(const QColor &)> textAreaForegroundSetter_;
  std::function<QColor()> textAreaBackgroundGetter_;
  std::function<void(const QColor &)> textAreaBackgroundSetter_;
  std::function<TextMonitorFormat()> textAreaFormatGetter_;
  std::function<void(TextMonitorFormat)> textAreaFormatSetter_;
  std::function<int()> textAreaPrecisionGetter_;
  std::function<void(int)> textAreaPrecisionSetter_;
  std::function<PvLimitSource()> textAreaPrecisionSourceGetter_;
  std::function<void(PvLimitSource)> textAreaPrecisionSourceSetter_;
  std::function<int()> textAreaPrecisionDefaultGetter_;
  std::function<void(int)> textAreaPrecisionDefaultSetter_;
  std::function<TextColorMode()> textAreaColorModeGetter_;
  std::function<void(TextColorMode)> textAreaColorModeSetter_;
  std::function<QString()> textAreaChannelGetter_;
  std::function<void(const QString &)> textAreaChannelSetter_;
  std::function<PvLimits()> textAreaLimitsGetter_;
  std::function<void(const PvLimits &)> textAreaLimitsSetter_;
  std::function<bool()> textAreaReadOnlyGetter_;
  std::function<void(bool)> textAreaReadOnlySetter_;
  std::function<bool()> textAreaWordWrapGetter_;
  std::function<void(bool)> textAreaWordWrapSetter_;
  std::function<TextAreaWrapMode()> textAreaWrapModeGetter_;
  std::function<void(TextAreaWrapMode)> textAreaWrapModeSetter_;
  std::function<int()> textAreaWrapColumnWidthGetter_;
  std::function<void(int)> textAreaWrapColumnWidthSetter_;
  std::function<bool()> textAreaShowVerticalScrollBarGetter_;
  std::function<void(bool)> textAreaShowVerticalScrollBarSetter_;
  std::function<bool()> textAreaShowHorizontalScrollBarGetter_;
  std::function<void(bool)> textAreaShowHorizontalScrollBarSetter_;
  std::function<TextAreaCommitMode()> textAreaCommitModeGetter_;
  std::function<void(TextAreaCommitMode)> textAreaCommitModeSetter_;
  std::function<bool()> textAreaTabInsertsSpacesGetter_;
  std::function<void(bool)> textAreaTabInsertsSpacesSetter_;
  std::function<int()> textAreaTabWidthGetter_;
  std::function<void(int)> textAreaTabWidthSetter_;
  std::function<QString()> textAreaFontFamilyGetter_;
  std::function<void(const QString &)> textAreaFontFamilySetter_;
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
  std::function<QColor()> thermometerForegroundGetter_;
  std::function<void(const QColor &)> thermometerForegroundSetter_;
  std::function<QColor()> thermometerBackgroundGetter_;
  std::function<void(const QColor &)> thermometerBackgroundSetter_;
  std::function<QColor()> thermometerTextGetter_;
  std::function<void(const QColor &)> thermometerTextSetter_;
  std::function<MeterLabel()> thermometerLabelGetter_;
  std::function<void(MeterLabel)> thermometerLabelSetter_;
  std::function<TextColorMode()> thermometerColorModeGetter_;
  std::function<void(TextColorMode)> thermometerColorModeSetter_;
  std::function<TextVisibilityMode()> thermometerVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> thermometerVisibilityModeSetter_;
  std::function<QString()> thermometerVisibilityCalcGetter_;
  std::function<void(const QString &)> thermometerVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> thermometerVisibilityChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4> thermometerVisibilityChannelSetters_{};
  std::function<TextMonitorFormat()> thermometerFormatGetter_;
  std::function<void(TextMonitorFormat)> thermometerFormatSetter_;
  std::function<bool()> thermometerShowValueGetter_;
  std::function<void(bool)> thermometerShowValueSetter_;
  std::function<QString()> thermometerChannelGetter_;
  std::function<void(const QString &)> thermometerChannelSetter_;
  std::function<PvLimits()> thermometerLimitsGetter_;
  std::function<void(const PvLimits &)> thermometerLimitsSetter_;
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
  std::function<bool()> cartesianDrawMajorGetter_;
  std::function<void(bool)> cartesianDrawMajorSetter_;
  std::function<bool()> cartesianDrawMinorGetter_;
  std::function<void(bool)> cartesianDrawMinorSetter_;
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
  std::function<QColor()> ledForegroundGetter_;
  std::function<void(const QColor &)> ledForegroundSetter_;
  std::function<QColor()> ledBackgroundGetter_;
  std::function<void(const QColor &)> ledBackgroundSetter_;
  std::function<TextColorMode()> ledColorModeGetter_;
  std::function<void(TextColorMode)> ledColorModeSetter_;
  std::function<LedShape()> ledShapeGetter_;
  std::function<void(LedShape)> ledShapeSetter_;
  std::function<bool()> ledBezelGetter_;
  std::function<void(bool)> ledBezelSetter_;
  std::function<QColor()> ledOnColorGetter_;
  std::function<void(const QColor &)> ledOnColorSetter_;
  std::function<QColor()> ledOffColorGetter_;
  std::function<void(const QColor &)> ledOffColorSetter_;
  std::function<QColor()> ledUndefinedColorGetter_;
  std::function<void(const QColor &)> ledUndefinedColorSetter_;
  std::array<std::function<QColor()>, kLedStateCount> ledStateColorGetters_{};
  std::array<std::function<void(const QColor &)>, kLedStateCount>
      ledStateColorSetters_{};
  std::function<int()> ledStateCountGetter_;
  std::function<void(int)> ledStateCountSetter_;
  std::function<QString()> ledChannelGetter_;
  std::function<void(const QString &)> ledChannelSetter_;
  std::function<TextVisibilityMode()> ledVisibilityModeGetter_;
  std::function<void(TextVisibilityMode)> ledVisibilityModeSetter_;
  std::function<QString()> ledVisibilityCalcGetter_;
  std::function<void(const QString &)> ledVisibilityCalcSetter_;
  std::array<std::function<QString()>, 4> ledVisibilityChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4>
      ledVisibilityChannelSetters_{};
  std::function<QColor()> expressionChannelForegroundGetter_;
  std::function<void(const QColor &)> expressionChannelForegroundSetter_;
  std::function<QColor()> expressionChannelBackgroundGetter_;
  std::function<void(const QColor &)> expressionChannelBackgroundSetter_;
  std::function<QString()> expressionChannelVariableGetter_;
  std::function<void(const QString &)> expressionChannelVariableSetter_;
  std::function<QString()> expressionChannelCalcGetter_;
  std::function<void(const QString &)> expressionChannelCalcSetter_;
  std::array<std::function<QString()>, 4> expressionChannelChannelGetters_{};
  std::array<std::function<void(const QString &)>, 4>
      expressionChannelChannelSetters_{};
  std::function<double()> expressionChannelInitialValueGetter_;
  std::function<void(double)> expressionChannelInitialValueSetter_;
  std::function<ExpressionChannelEventSignalMode()>
      expressionChannelEventSignalGetter_;
  std::function<void(ExpressionChannelEventSignalMode)>
      expressionChannelEventSignalSetter_;
  std::function<int()> expressionChannelPrecisionGetter_;
  std::function<void(int)> expressionChannelPrecisionSetter_;
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
  std::function<HeatmapColorMap()> heatmapColorMapGetter_;
  std::function<void(HeatmapColorMap)> heatmapColorMapSetter_;
  std::function<bool()> heatmapInvertGreyscaleGetter_;
  std::function<void(bool)> heatmapInvertGreyscaleSetter_;
  std::function<bool()> heatmapPreserveAspectRatioGetter_;
  std::function<void(bool)> heatmapPreserveAspectRatioSetter_;
  std::function<bool()> heatmapFlipHorizontalGetter_;
  std::function<void(bool)> heatmapFlipHorizontalSetter_;
  std::function<bool()> heatmapFlipVerticalGetter_;
  std::function<void(bool)> heatmapFlipVerticalSetter_;
  std::function<HeatmapRotation()> heatmapRotationGetter_;
  std::function<void(HeatmapRotation)> heatmapRotationSetter_;
  std::function<bool()> heatmapShowTopProfileGetter_;
  std::function<void(bool)> heatmapShowTopProfileSetter_;
  std::function<bool()> heatmapShowRightProfileGetter_;
  std::function<void(bool)> heatmapShowRightProfileSetter_;
  std::function<HeatmapProfileMode()> heatmapProfileModeGetter_;
  std::function<void(HeatmapProfileMode)> heatmapProfileModeSetter_;
  std::function<QColor()> waterfallForegroundGetter_;
  std::function<void(const QColor &)> waterfallForegroundSetter_;
  std::function<QColor()> waterfallBackgroundGetter_;
  std::function<void(const QColor &)> waterfallBackgroundSetter_;
  std::function<QString()> waterfallTitleGetter_;
  std::function<void(const QString &)> waterfallTitleSetter_;
  std::function<QString()> waterfallXLabelGetter_;
  std::function<void(const QString &)> waterfallXLabelSetter_;
  std::function<QString()> waterfallYLabelGetter_;
  std::function<void(const QString &)> waterfallYLabelSetter_;
  std::function<QString()> waterfallDataChannelGetter_;
  std::function<void(const QString &)> waterfallDataChannelSetter_;
  std::function<QString()> waterfallCountChannelGetter_;
  std::function<void(const QString &)> waterfallCountChannelSetter_;
  std::function<QString()> waterfallTriggerChannelGetter_;
  std::function<void(const QString &)> waterfallTriggerChannelSetter_;
  std::function<QString()> waterfallEraseChannelGetter_;
  std::function<void(const QString &)> waterfallEraseChannelSetter_;
  std::function<WaterfallEraseMode()> waterfallEraseModeGetter_;
  std::function<void(WaterfallEraseMode)> waterfallEraseModeSetter_;
  std::function<int()> waterfallHistoryCountGetter_;
  std::function<void(int)> waterfallHistoryCountSetter_;
  std::function<WaterfallScrollDirection()> waterfallScrollDirectionGetter_;
  std::function<void(WaterfallScrollDirection)>
      waterfallScrollDirectionSetter_;
  std::function<HeatmapColorMap()> waterfallColorMapGetter_;
  std::function<void(HeatmapColorMap)> waterfallColorMapSetter_;
  std::function<bool()> waterfallInvertGreyscaleGetter_;
  std::function<void(bool)> waterfallInvertGreyscaleSetter_;
  std::function<WaterfallIntensityScale()> waterfallIntensityScaleGetter_;
  std::function<void(WaterfallIntensityScale)> waterfallIntensityScaleSetter_;
  std::function<double()> waterfallIntensityMinGetter_;
  std::function<void(double)> waterfallIntensityMinSetter_;
  std::function<double()> waterfallIntensityMaxGetter_;
  std::function<void(double)> waterfallIntensityMaxSetter_;
  std::function<bool()> waterfallShowLegendGetter_;
  std::function<void(bool)> waterfallShowLegendSetter_;
  std::function<bool()> waterfallShowGridGetter_;
  std::function<void(bool)> waterfallShowGridSetter_;
  std::function<double()> waterfallSamplePeriodGetter_;
  std::function<void(double)> waterfallSamplePeriodSetter_;
  std::function<TimeUnits()> waterfallUnitsGetter_;
  std::function<void(TimeUnits)> waterfallUnitsSetter_;
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
    dialog->setTextEntryCallbacks(channelLabel, textEntryPrecisionSourceGetter_,
        textEntryPrecisionSourceSetter_, textEntryPrecisionDefaultGetter_,
        textEntryPrecisionDefaultSetter_,
        [this]() { updateTextEntryLimitsFromDialog(); }, textEntryLimitsGetter_,
        textEntryLimitsSetter_);
    positionPvLimitsDialog(dialog);
    dialog->showForTextEntry();
  }

  void openSetpointControlPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!setpointControlPrecisionSourceGetter_
        || !setpointControlPrecisionSourceSetter_
        || !setpointControlPrecisionDefaultGetter_
        || !setpointControlPrecisionDefaultSetter_
        || !setpointControlLimitsGetter_
        || !setpointControlLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = setpointControlSetpointGetter_
        ? setpointControlSetpointGetter_()
        : QString();
    dialog->setTextEntryCallbacks(channelLabel,
        setpointControlPrecisionSourceGetter_,
        setpointControlPrecisionSourceSetter_,
        setpointControlPrecisionDefaultGetter_,
        setpointControlPrecisionDefaultSetter_,
        [this]() { updateSetpointControlLimitsFromDialog(); },
        setpointControlLimitsGetter_, setpointControlLimitsSetter_);
    positionPvLimitsDialog(dialog);
    dialog->showForTextEntry();
  }

  void openTextAreaPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!textAreaPrecisionSourceGetter_ || !textAreaPrecisionSourceSetter_
        || !textAreaPrecisionDefaultGetter_
        || !textAreaPrecisionDefaultSetter_
        || !textAreaLimitsGetter_ || !textAreaLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = textAreaChannelGetter_ ? textAreaChannelGetter_()
                                                        : QString();
    dialog->setTextEntryCallbacks(channelLabel, textAreaPrecisionSourceGetter_,
        textAreaPrecisionSourceSetter_, textAreaPrecisionDefaultGetter_,
        textAreaPrecisionDefaultSetter_,
        [this]() { updateTextAreaLimitsFromDialog(); }, textAreaLimitsGetter_,
        textAreaLimitsSetter_);
    positionPvLimitsDialog(dialog);
    dialog->showForTextEntry();
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

  void openThermometerPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!thermometerLimitsGetter_ || !thermometerLimitsSetter_) {
      dialog->clearTargets();
      positionPvLimitsDialog(dialog);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = thermometerChannelGetter_
        ? thermometerChannelGetter_()
        : QString();
    dialog->setThermometerCallbacks(channelLabel, thermometerLimitsGetter_,
        thermometerLimitsSetter_,
        [this]() { updateThermometerLimitsFromDialog(); });
    positionPvLimitsDialog(dialog);
    dialog->showForThermometer();
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
