#include "strip_chart_data_dialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>

#include "color_palette_dialog.h"
#include "strip_chart_element.h"

StripChartDataDialog::StripChartDataDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &valueFont, QWidget *parent)
    : QDialog(parent)
    , labelFont_(labelFont)
    , valueFont_(valueFont)
{
  setPalette(basePalette);
  setWindowTitle(tr("Strip Chart Data"));
  setModal(false);
  setupUi();
}

void StripChartDataDialog::setTarget(StripChartElement *element)
{
  element_ = element;
  populateFromElement();
  show();
  raise();
  activateWindow();
}

void StripChartDataDialog::clearTarget()
{
  element_ = nullptr;
  for (int i = 0; i < kMaxPens; ++i) {
    penRows_[i].channelLabel->clear();
    updateColorButton(i, Qt::black);
  }
}

void StripChartDataDialog::setupUi()
{
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(8);

  /* Create scrollable area for pen rows */
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto *scrollWidget = new QWidget();
  auto *penLayout = new QGridLayout(scrollWidget);
  penLayout->setSpacing(4);

  /* Header row */
  auto *headerChannel = new QLabel(tr("Channel"), this);
  headerChannel->setFont(labelFont_);
  headerChannel->setAlignment(Qt::AlignCenter);
  penLayout->addWidget(headerChannel, 0, 0);

  auto *headerColor = new QLabel(tr("Color"), this);
  headerColor->setFont(labelFont_);
  headerColor->setAlignment(Qt::AlignCenter);
  penLayout->addWidget(headerColor, 0, 1);

  auto *headerLoprSrc = new QLabel(tr("Low Src"), this);
  headerLoprSrc->setFont(labelFont_);
  headerLoprSrc->setAlignment(Qt::AlignCenter);
  penLayout->addWidget(headerLoprSrc, 0, 2);

  auto *headerLoprVal = new QLabel(tr("Low Val"), this);
  headerLoprVal->setFont(labelFont_);
  headerLoprVal->setAlignment(Qt::AlignCenter);
  penLayout->addWidget(headerLoprVal, 0, 3);

  auto *headerHoprSrc = new QLabel(tr("High Src"), this);
  headerHoprSrc->setFont(labelFont_);
  headerHoprSrc->setAlignment(Qt::AlignCenter);
  penLayout->addWidget(headerHoprSrc, 0, 4);

  auto *headerHoprVal = new QLabel(tr("High Val"), this);
  headerHoprVal->setFont(labelFont_);
  headerHoprVal->setAlignment(Qt::AlignCenter);
  penLayout->addWidget(headerHoprVal, 0, 5);

  /* Pen rows */
  for (int i = 0; i < kMaxPens; ++i) {
    const int row = i + 1;
    PenRow &penRow = penRows_[i];

    /* Channel label */
    penRow.channelLabel = new QLabel(this);
    penRow.channelLabel->setFont(valueFont_);
    penRow.channelLabel->setMinimumWidth(150);
    penLayout->addWidget(penRow.channelLabel, row, 0);

    /* Color button */
    penRow.colorButton = new QPushButton(this);
    penRow.colorButton->setFixedSize(24, 24);
    penRow.colorButton->setFlat(false);
    const int penIndex = i;
    connect(penRow.colorButton, &QPushButton::clicked, this, [this, penIndex]() {
      openColorPalette(penIndex);
    });
    penLayout->addWidget(penRow.colorButton, row, 1, Qt::AlignCenter);

    /* LOPR source combo */
    penRow.loprSourceCombo = new QComboBox(this);
    penRow.loprSourceCombo->setFont(valueFont_);
    penRow.loprSourceCombo->addItem(tr("Channel"), static_cast<int>(PvLimitSource::kChannel));
    penRow.loprSourceCombo->addItem(tr("Default"), static_cast<int>(PvLimitSource::kDefault));
    penRow.loprSourceCombo->addItem(tr("User"), static_cast<int>(PvLimitSource::kUser));
    connect(penRow.loprSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this, penIndex](int) { updateLoprValueEnabled(penIndex); });
    penLayout->addWidget(penRow.loprSourceCombo, row, 2);

    /* LOPR value spin */
    penRow.loprValueSpin = new QDoubleSpinBox(this);
    penRow.loprValueSpin->setFont(valueFont_);
    penRow.loprValueSpin->setRange(-1e12, 1e12);
    penRow.loprValueSpin->setDecimals(6);
    penRow.loprValueSpin->setMinimumWidth(80);
    penLayout->addWidget(penRow.loprValueSpin, row, 3);

    /* HOPR source combo */
    penRow.hoprSourceCombo = new QComboBox(this);
    penRow.hoprSourceCombo->setFont(valueFont_);
    penRow.hoprSourceCombo->addItem(tr("Channel"), static_cast<int>(PvLimitSource::kChannel));
    penRow.hoprSourceCombo->addItem(tr("Default"), static_cast<int>(PvLimitSource::kDefault));
    penRow.hoprSourceCombo->addItem(tr("User"), static_cast<int>(PvLimitSource::kUser));
    connect(penRow.hoprSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this, penIndex](int) { updateHoprValueEnabled(penIndex); });
    penLayout->addWidget(penRow.hoprSourceCombo, row, 4);

    /* HOPR value spin */
    penRow.hoprValueSpin = new QDoubleSpinBox(this);
    penRow.hoprValueSpin->setFont(valueFont_);
    penRow.hoprValueSpin->setRange(-1e12, 1e12);
    penRow.hoprValueSpin->setDecimals(6);
    penRow.hoprValueSpin->setMinimumWidth(80);
    penLayout->addWidget(penRow.hoprValueSpin, row, 5);
  }

  scrollArea->setWidget(scrollWidget);
  mainLayout->addWidget(scrollArea);

  /* Period and Units row */
  auto *periodLayout = new QHBoxLayout();
  periodLayout->setSpacing(8);

  auto *periodLabel = new QLabel(tr("Period:"), this);
  periodLabel->setFont(labelFont_);
  periodLayout->addWidget(periodLabel);

  periodSpin_ = new QDoubleSpinBox(this);
  periodSpin_->setFont(valueFont_);
  periodSpin_->setRange(0.001, 1e9);
  periodSpin_->setDecimals(3);
  periodSpin_->setMinimumWidth(100);
  periodLayout->addWidget(periodSpin_);

  auto *unitsLabel = new QLabel(tr("Units:"), this);
  unitsLabel->setFont(labelFont_);
  periodLayout->addWidget(unitsLabel);

  unitsCombo_ = new QComboBox(this);
  unitsCombo_->setFont(valueFont_);
  unitsCombo_->addItem(tr("Milliseconds"), static_cast<int>(TimeUnits::kMilliseconds));
  unitsCombo_->addItem(tr("Seconds"), static_cast<int>(TimeUnits::kSeconds));
  unitsCombo_->addItem(tr("Minutes"), static_cast<int>(TimeUnits::kMinutes));
  periodLayout->addWidget(unitsCombo_);

  periodLayout->addStretch();
  mainLayout->addLayout(periodLayout);

  /* Button row */
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->setSpacing(8);

  buttonLayout->addStretch();

  applyButton_ = new QPushButton(tr("Apply"), this);
  applyButton_->setFont(labelFont_);
  connect(applyButton_, &QPushButton::clicked, this, &StripChartDataDialog::applyChanges);
  buttonLayout->addWidget(applyButton_);

  cancelButton_ = new QPushButton(tr("Cancel"), this);
  cancelButton_->setFont(labelFont_);
  connect(cancelButton_, &QPushButton::clicked, this, [this]() {
    resetToOriginal();
    hide();
  });
  buttonLayout->addWidget(cancelButton_);

  closeButton_ = new QPushButton(tr("Close"), this);
  closeButton_->setFont(labelFont_);
  connect(closeButton_, &QPushButton::clicked, this, &QDialog::hide);
  buttonLayout->addWidget(closeButton_);

  buttonLayout->addStretch();
  mainLayout->addLayout(buttonLayout);

  /* Set minimum size */
  setMinimumWidth(650);
  setMinimumHeight(350);
}

void StripChartDataDialog::populateFromElement()
{
  if (!element_) {
    return;
  }

  /* Store original values and populate UI */
  for (int i = 0; i < kMaxPens; ++i) {
    PenRow &penRow = penRows_[i];
    QString channel = element_->channel(i);
    QColor color = element_->penColor(i);
    PvLimits limits = element_->penLimits(i);

    /* Store original */
    originalPenData_[i].color = color;
    originalPenData_[i].limits = limits;

    /* Populate UI */
    penRow.channelLabel->setText(channel);
    updateColorButton(i, color);

    /* LOPR */
    int loprIndex = penRow.loprSourceCombo->findData(static_cast<int>(limits.lowSource));
    if (loprIndex >= 0) {
      penRow.loprSourceCombo->setCurrentIndex(loprIndex);
    }
    penRow.loprValueSpin->setValue(limits.lowDefault);
    updateLoprValueEnabled(i);

    /* HOPR */
    int hoprIndex = penRow.hoprSourceCombo->findData(static_cast<int>(limits.highSource));
    if (hoprIndex >= 0) {
      penRow.hoprSourceCombo->setCurrentIndex(hoprIndex);
    }
    penRow.hoprValueSpin->setValue(limits.highDefault);
    updateHoprValueEnabled(i);

    /* Enable/disable row based on whether channel is set */
    bool hasChannel = !channel.isEmpty();
    penRow.colorButton->setEnabled(hasChannel);
    penRow.loprSourceCombo->setEnabled(hasChannel);
    penRow.loprValueSpin->setEnabled(hasChannel);
    penRow.hoprSourceCombo->setEnabled(hasChannel);
    penRow.hoprValueSpin->setEnabled(hasChannel);
  }

  /* Period and units */
  originalPeriod_ = element_->period();
  originalUnits_ = element_->units();

  periodSpin_->setValue(originalPeriod_);
  int unitsIndex = unitsCombo_->findData(static_cast<int>(originalUnits_));
  if (unitsIndex >= 0) {
    unitsCombo_->setCurrentIndex(unitsIndex);
  }
}

void StripChartDataDialog::applyChanges()
{
  if (!element_) {
    return;
  }

  for (int i = 0; i < kMaxPens; ++i) {
    PenRow &penRow = penRows_[i];
    QString channel = element_->channel(i);
    if (channel.isEmpty()) {
      continue;
    }

    /* Color - get from button's background */
    QColor color = penRow.colorButton->palette().color(QPalette::Button);
    element_->setPenColor(i, color);

    /* Limits */
    PvLimits limits = element_->penLimits(i);
    limits.lowSource = static_cast<PvLimitSource>(
        penRow.loprSourceCombo->currentData().toInt());
    limits.lowDefault = penRow.loprValueSpin->value();
    limits.highSource = static_cast<PvLimitSource>(
        penRow.hoprSourceCombo->currentData().toInt());
    limits.highDefault = penRow.hoprValueSpin->value();
    element_->setPenLimits(i, limits);
  }

  /* Period and units */
  element_->setPeriod(periodSpin_->value());
  element_->setUnits(static_cast<TimeUnits>(unitsCombo_->currentData().toInt()));

  element_->update();
}

void StripChartDataDialog::resetToOriginal()
{
  if (!element_) {
    return;
  }

  for (int i = 0; i < kMaxPens; ++i) {
    QString channel = element_->channel(i);
    if (channel.isEmpty()) {
      continue;
    }
    element_->setPenColor(i, originalPenData_[i].color);
    element_->setPenLimits(i, originalPenData_[i].limits);
  }

  element_->setPeriod(originalPeriod_);
  element_->setUnits(originalUnits_);
  element_->update();
}

void StripChartDataDialog::openColorPalette(int penIndex)
{
  if (penIndex < 0 || penIndex >= kMaxPens) {
    return;
  }

  if (!colorPaletteDialog_) {
    colorPaletteDialog_ = new ColorPaletteDialog(palette(), labelFont_,
        valueFont_, this);
    colorPaletteDialog_->setColorSelectedCallback(
        [this](const QColor &color) {
          if (activeColorPenIndex_ >= 0 && activeColorPenIndex_ < kMaxPens) {
            updateColorButton(activeColorPenIndex_, color);
          }
        });
    connect(colorPaletteDialog_, &QDialog::finished, this,
        [this](int) { activeColorPenIndex_ = -1; });
  }

  activeColorPenIndex_ = penIndex;
  QColor currentColor = penRows_[penIndex].colorButton->palette().color(QPalette::Button);
  QString desc = tr("Pen %1 Color").arg(penIndex + 1);
  colorPaletteDialog_->setCurrentColor(currentColor, desc);
  colorPaletteDialog_->show();
  colorPaletteDialog_->raise();
  colorPaletteDialog_->activateWindow();
}

void StripChartDataDialog::updateColorButton(int penIndex, const QColor &color)
{
  if (penIndex < 0 || penIndex >= kMaxPens) {
    return;
  }
  QPushButton *button = penRows_[penIndex].colorButton;
  if (!button) {
    return;
  }
  QPalette pal = button->palette();
  pal.setColor(QPalette::Button, color);
  pal.setColor(QPalette::ButtonText, color.lightness() > 128 ? Qt::black : Qt::white);
  button->setPalette(pal);
  button->setAutoFillBackground(true);
  button->setStyleSheet(
      QStringLiteral("background-color: %1; border: 1px solid gray;").arg(color.name()));
}

void StripChartDataDialog::updateLoprValueEnabled(int penIndex)
{
  if (penIndex < 0 || penIndex >= kMaxPens) {
    return;
  }
  PenRow &penRow = penRows_[penIndex];
  auto source = static_cast<PvLimitSource>(
      penRow.loprSourceCombo->currentData().toInt());
  penRow.loprValueSpin->setEnabled(source == PvLimitSource::kUser);
}

void StripChartDataDialog::updateHoprValueEnabled(int penIndex)
{
  if (penIndex < 0 || penIndex >= kMaxPens) {
    return;
  }
  PenRow &penRow = penRows_[penIndex];
  auto source = static_cast<PvLimitSource>(
      penRow.hoprSourceCombo->currentData().toInt());
  penRow.hoprValueSpin->setEnabled(source == PvLimitSource::kUser);
}
