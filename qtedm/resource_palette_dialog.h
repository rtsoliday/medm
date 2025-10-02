#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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
#include <QGuiApplication>

#include "color_palette_dialog.h"
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
    }

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

    imageColorModeCombo_ = new QComboBox;
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
    }

    int imageRow = 0;
    addRow(imageLayout, imageRow++, QStringLiteral("Image Type"), imageTypeCombo_);
    addRow(imageLayout, imageRow++, QStringLiteral("Image Name"), imageNameEdit_);
    addRow(imageLayout, imageRow++, QStringLiteral("Calc"), imageCalcEdit_);
    addRow(imageLayout, imageRow++, QStringLiteral("Color Mode"), imageColorModeCombo_);
    addRow(imageLayout, imageRow++, QStringLiteral("Visibility"), imageVisibilityCombo_);
    addRow(imageLayout, imageRow++, QStringLiteral("Vis Calc"), imageVisibilityCalcEdit_);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel A"), imageChannelEdits_[0]);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel B"), imageChannelEdits_[1]);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel C"), imageChannelEdits_[2]);
    addRow(imageLayout, imageRow++, QStringLiteral("Channel D"), imageChannelEdits_[3]);
    imageLayout->setRowStretch(imageRow, 1);
    entriesLayout->addWidget(imageSection_);

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
    }

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

    textMonitorPrecisionEdit_ = createLineEdit();
    committedTexts_.insert(textMonitorPrecisionEdit_, textMonitorPrecisionEdit_->text());
    textMonitorPrecisionEdit_->installEventFilter(this);
    QObject::connect(textMonitorPrecisionEdit_, &QLineEdit::returnPressed, this,
        [this]() { commitTextMonitorPrecision(); });
    QObject::connect(textMonitorPrecisionEdit_, &QLineEdit::editingFinished,
        this, [this]() { commitTextMonitorPrecision(); });

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
    addRow(textMonitorLayout, 4, QStringLiteral("Precision"),
        textMonitorPrecisionEdit_);
    addRow(textMonitorLayout, 5, QStringLiteral("Color Mode"),
        textMonitorColorModeCombo_);
    addRow(textMonitorLayout, 6, QStringLiteral("Channel"),
        textMonitorChannelEdit_);
    addRow(textMonitorLayout, 7, QStringLiteral("Channel Limits"),
        textMonitorPvLimitsButton_);
    textMonitorLayout->setRowStretch(8, 1);
    entriesLayout->addWidget(textMonitorSection_);

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

    entriesLayout->addStretch(1);

  displaySection_->setVisible(false);
  rectangleSection_->setVisible(false);
  imageSection_->setVisible(false);
  lineSection_->setVisible(false);
  textSection_->setVisible(false);
  textMonitorSection_->setVisible(false);
  meterSection_->setVisible(false);
  barSection_->setVisible(false);
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
      std::function<void(const QString &)> channelSetter)
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

    if (textMonitorPrecisionEdit_) {
      const int precision = textMonitorPrecisionGetter_
              ? textMonitorPrecisionGetter_()
              : -1;
      const QSignalBlocker blocker(textMonitorPrecisionEdit_);
      if (precision < 0) {
        textMonitorPrecisionEdit_->clear();
      } else {
        textMonitorPrecisionEdit_->setText(QString::number(precision));
      }
      committedTexts_[textMonitorPrecisionEdit_] = textMonitorPrecisionEdit_->text();
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

    show();
    positionRelativeTo(parentWidget());
    raise();
    activateWindow();
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

    show();
    positionRelativeTo(parentWidget());
    raise();
    activateWindow();
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

    show();
    positionRelativeTo(parentWidget());
    raise();
    activateWindow();
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

    elementLabel_->setText(elementLabel);

    show();
    positionRelativeTo(parentWidget());
    raise();
    activateWindow();
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

    elementLabel_->setText(QStringLiteral("Image"));

    show();
    positionRelativeTo(parentWidget());
    raise();
    activateWindow();
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

  elementLabel_->setText(elementLabel);

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
    resetLineEdit(textMonitorPrecisionEdit_);
    resetLineEdit(textMonitorChannelEdit_);
    if (textMonitorPvLimitsButton_) {
      textMonitorPvLimitsButton_->setEnabled(false);
    }
    resetLineEdit(meterChannelEdit_);
    resetLineEdit(barChannelEdit_);
    resetLineEdit(rectangleLineWidthEdit_);
    resetLineEdit(rectangleVisibilityCalcEdit_);
    for (QLineEdit *edit : rectangleChannelEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(imageNameEdit_);
    resetLineEdit(imageCalcEdit_);
    resetLineEdit(imageVisibilityCalcEdit_);
    for (QLineEdit *edit : imageChannelEdits_) {
      resetLineEdit(edit);
    }
    resetLineEdit(lineLineWidthEdit_);
    resetLineEdit(lineVisibilityCalcEdit_);
    for (QLineEdit *edit : lineChannelEdits_) {
      resetLineEdit(edit);
    }

    resetColorButton(foregroundButton_);
    resetColorButton(backgroundButton_);
    resetColorButton(textForegroundButton_);
    resetColorButton(textMonitorForegroundButton_);
    resetColorButton(textMonitorBackgroundButton_);
    resetColorButton(meterForegroundButton_);
    resetColorButton(meterBackgroundButton_);
    resetColorButton(barForegroundButton_);
    resetColorButton(barBackgroundButton_);
    resetColorButton(rectangleForegroundButton_);
    resetColorButton(lineColorButton_);

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

    textMonitorPrecisionSourceGetter_ = {};
    textMonitorPrecisionSourceSetter_ = {};
    textMonitorPrecisionDefaultGetter_ = {};
    textMonitorPrecisionDefaultSetter_ = {};

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
  enum class SelectionKind {
    kNone,
    kDisplay,
    kRectangle,
    kImage,
    kPolygon,
    kLine,
    kText,
    kMeter,
    kBarMonitor,
    kTextMonitor
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
        if (edit == xEdit_ || edit == yEdit_ || edit == widthEdit_
            || edit == heightEdit_ || edit == gridSpacingEdit_
            || edit == rectangleLineWidthEdit_
            || edit == rectangleVisibilityCalcEdit_
            || edit == imageNameEdit_ || edit == imageCalcEdit_
            || edit == imageVisibilityCalcEdit_
            || edit == lineLineWidthEdit_
            || edit == lineVisibilityCalcEdit_
            || isRectangleChannelEdit || isLineChannelEdit
            || isImageChannelEdit) {
          revertLineEdit(edit);
        }
        if (edit == textMonitorPrecisionEdit_ || edit == textMonitorChannelEdit_
            || edit == meterChannelEdit_) {
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
    if (rectangleSection_) {
      const bool rectangleVisible = kind == SelectionKind::kRectangle
          || kind == SelectionKind::kPolygon;
      rectangleSection_->setVisible(rectangleVisible);
      rectangleSection_->setEnabled(rectangleVisible);
    }
    if (imageSection_) {
      const bool imageVisible = kind == SelectionKind::kImage;
      imageSection_->setVisible(imageVisible);
      imageSection_->setEnabled(imageVisible);
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

  void commitTextMonitorPrecision()
  {
    if (!textMonitorPrecisionEdit_) {
      return;
    }
    if (!textMonitorPrecisionSetter_) {
      revertLineEdit(textMonitorPrecisionEdit_);
      return;
    }
    const QString raw = textMonitorPrecisionEdit_->text().trimmed();
    if (raw.isEmpty()) {
      textMonitorPrecisionSetter_(-1);
      const QSignalBlocker blocker(textMonitorPrecisionEdit_);
      textMonitorPrecisionEdit_->clear();
      committedTexts_[textMonitorPrecisionEdit_] = QString();
      return;
    }
    bool ok = false;
    int value = raw.toInt(&ok);
    if (!ok) {
      revertLineEdit(textMonitorPrecisionEdit_);
      return;
    }
    value = std::clamp(value, -1, 17);
    textMonitorPrecisionSetter_(value);
    const QSignalBlocker blocker(textMonitorPrecisionEdit_);
    if (value < 0) {
      textMonitorPrecisionEdit_->clear();
    } else {
      textMonitorPrecisionEdit_->setText(QString::number(value));
    }
    committedTexts_[textMonitorPrecisionEdit_] = textMonitorPrecisionEdit_->text();
    updateTextMonitorLimitsFromDialog();
  }

  void updateTextMonitorPrecisionField()
  {
    if (!textMonitorPrecisionEdit_) {
      return;
    }
    const int precision = textMonitorPrecisionGetter_ ? textMonitorPrecisionGetter_()
                                                     : -1;
    const QSignalBlocker blocker(textMonitorPrecisionEdit_);
    if (precision < 0) {
      textMonitorPrecisionEdit_->clear();
    } else {
      textMonitorPrecisionEdit_->setText(QString::number(precision));
    }
    committedTexts_[textMonitorPrecisionEdit_] = textMonitorPrecisionEdit_->text();
  }

  void updateTextMonitorLimitsFromDialog()
  {
    updateTextMonitorPrecisionField();
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
          [this]() { updateTextMonitorLimitsFromDialog(); });
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
  QWidget *rectangleSection_ = nullptr;
  QWidget *imageSection_ = nullptr;
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
  std::array<QLineEdit *, 4> textChannelEdits_{};
  QWidget *textMonitorSection_ = nullptr;
  QPushButton *textMonitorForegroundButton_ = nullptr;
  QPushButton *textMonitorBackgroundButton_ = nullptr;
  QComboBox *textMonitorAlignmentCombo_ = nullptr;
  QComboBox *textMonitorFormatCombo_ = nullptr;
  QLineEdit *textMonitorPrecisionEdit_ = nullptr;
  QComboBox *textMonitorColorModeCombo_ = nullptr;
  QLineEdit *textMonitorChannelEdit_ = nullptr;
  QPushButton *textMonitorPvLimitsButton_ = nullptr;
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
  QPushButton *rectangleForegroundButton_ = nullptr;
  QComboBox *rectangleFillCombo_ = nullptr;
  QComboBox *rectangleLineStyleCombo_ = nullptr;
  QLineEdit *rectangleLineWidthEdit_ = nullptr;
  QComboBox *rectangleColorModeCombo_ = nullptr;
  QComboBox *rectangleVisibilityCombo_ = nullptr;
  QLineEdit *rectangleVisibilityCalcEdit_ = nullptr;
  std::array<QLineEdit *, 4> rectangleChannelEdits_{};
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
    if (textMonitorPrecisionEdit_) {
      committedTexts_[textMonitorPrecisionEdit_] = textMonitorPrecisionEdit_->text();
    }
    if (textMonitorChannelEdit_) {
      committedTexts_[textMonitorChannelEdit_] = textMonitorChannelEdit_->text();
    }
    if (meterChannelEdit_) {
      committedTexts_[meterChannelEdit_] = meterChannelEdit_->text();
    }
    if (barChannelEdit_) {
      committedTexts_[barChannelEdit_] = barChannelEdit_->text();
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
  ColorPaletteDialog *colorPaletteDialog_ = nullptr;
  PvLimitsDialog *pvLimitsDialog_ = nullptr;
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
    colorPaletteDialog_->show();
    colorPaletteDialog_->raise();
    colorPaletteDialog_->activateWindow();
  }

  void openTextMonitorPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!textMonitorPrecisionSourceGetter_) {
      dialog->clearTargets();
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
        [this]() { updateTextMonitorLimitsFromDialog(); });
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
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = meterChannelGetter_ ? meterChannelGetter_()
                                                     : QString();
    dialog->setMeterCallbacks(channelLabel, meterLimitsGetter_,
        meterLimitsSetter_, [this]() { updateMeterLimitsFromDialog(); });
    dialog->showForMeter();
  }

  void openBarMonitorPvLimitsDialog()
  {
    PvLimitsDialog *dialog = ensurePvLimitsDialog();
    if (!dialog) {
      return;
    }
    if (!barLimitsGetter_ || !barLimitsSetter_) {
      dialog->clearTargets();
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
      return;
    }
    const QString channelLabel = barChannelGetter_ ? barChannelGetter_()
                                                   : QString();
    dialog->setBarCallbacks(channelLabel, barLimitsGetter_, barLimitsSetter_,
        [this]() { updateBarLimitsFromDialog(); });
    dialog->showForBarMonitor();
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
