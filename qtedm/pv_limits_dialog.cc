#include "pv_limits_dialog.h"

#include <algorithm>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QGridLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPalette>
#include <QSignalBlocker>
#include <QWidget>
#include <QVBoxLayout>

namespace {

int sourceIndexFor(PvLimitSource source)
{
  switch (source) {
  case PvLimitSource::kDefault:
    return 1;
  case PvLimitSource::kUser:
    return 2;
  case PvLimitSource::kChannel:
  default:
    return 0;
  }
}

PvLimitSource sourceForIndex(int index)
{
  switch (index) {
  case 1:
    return PvLimitSource::kDefault;
  case 2:
    return PvLimitSource::kUser;
  case 0:
  default:
    return PvLimitSource::kChannel;
  }
}

QComboBox *createSourceCombo(const QFont &font)
{
  auto *combo = new QComboBox;
  combo->setFont(font);
  combo->setAutoFillBackground(true);
  combo->addItem(QStringLiteral("Channel"));
  combo->addItem(QStringLiteral("Default"));
  combo->addItem(QStringLiteral("User"));
  combo->model()->setData(combo->model()->index(2, 0), 0, Qt::UserRole - 1);
  return combo;
}

QLineEdit *createValueEdit(const QFont &font)
{
  auto *edit = new QLineEdit;
  edit->setFont(font);
  edit->setAutoFillBackground(true);
  QPalette pal = edit->palette();
  pal.setColor(QPalette::Base, Qt::white);
  pal.setColor(QPalette::Text, Qt::black);
  edit->setPalette(pal);
  edit->setMaximumWidth(120);
  return edit;
}

} // namespace

PvLimitsDialog::PvLimitsDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &valueFont, QWidget *parent)
  : QDialog(parent)
  , labelFont_(labelFont)
  , valueFont_(valueFont)
{
  setObjectName(QStringLiteral("qtedmPvLimitsDialog"));
  setWindowTitle(QStringLiteral("PV Limits"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(10);

  titleLabel_ = new QLabel(QStringLiteral("Edit Mode Limits"));
  titleLabel_->setFont(labelFont_);
  titleLabel_->setAlignment(Qt::AlignCenter);
  titleLabel_->setFrameShape(QFrame::Panel);
  titleLabel_->setFrameShadow(QFrame::Sunken);
  titleLabel_->setLineWidth(2);
  titleLabel_->setAutoFillBackground(true);
  titleLabel_->setPalette(basePalette);
  mainLayout->addWidget(titleLabel_);

  auto *gridWidget = new QWidget;
  gridWidget->setAutoFillBackground(true);
  gridWidget->setPalette(basePalette);
  auto *grid = new QGridLayout(gridWidget);
  grid->setContentsMargins(6, 6, 6, 6);
  grid->setHorizontalSpacing(12);
  grid->setVerticalSpacing(8);

  loprLabel_ = new QLabel(QStringLiteral("Low Limit"));
  loprLabel_->setFont(labelFont_);
  loprSourceCombo_ = createSourceCombo(valueFont_);
  loprEdit_ = createValueEdit(valueFont_);
  loprEdit_->setValidator(new QDoubleValidator(loprEdit_));
  loprEdit_->setEnabled(false);
  grid->addWidget(loprLabel_, 0, 0);
  grid->addWidget(loprSourceCombo_, 0, 1);
  grid->addWidget(loprEdit_, 0, 2);

  hoprLabel_ = new QLabel(QStringLiteral("High Limit"));
  hoprLabel_->setFont(labelFont_);
  hoprSourceCombo_ = createSourceCombo(valueFont_);
  hoprEdit_ = createValueEdit(valueFont_);
  hoprEdit_->setValidator(new QDoubleValidator(hoprEdit_));
  hoprEdit_->setEnabled(false);
  grid->addWidget(hoprLabel_, 1, 0);
  grid->addWidget(hoprSourceCombo_, 1, 1);
  grid->addWidget(hoprEdit_, 1, 2);

  precisionLabel_ = new QLabel(QStringLiteral("Precision"));
  precisionLabel_->setFont(labelFont_);
  precisionSourceCombo_ = createSourceCombo(valueFont_);
  precisionEdit_ = createValueEdit(valueFont_);
  precisionEdit_->setValidator(new QIntValidator(0, 17, precisionEdit_));
  grid->addWidget(precisionLabel_, 2, 0);
  grid->addWidget(precisionSourceCombo_, 2, 1);
  grid->addWidget(precisionEdit_, 2, 2);

  mainLayout->addWidget(gridWidget);

  auto *buttonBox = new QDialogButtonBox;
  closeButton_ = buttonBox->addButton(QDialogButtonBox::Close);
  helpButton_ = buttonBox->addButton(QStringLiteral("Help"),
      QDialogButtonBox::HelpRole);
  closeButton_->setFont(valueFont_);
  helpButton_->setFont(valueFont_);
  mainLayout->addWidget(buttonBox);

  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, false);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, false);

  connect(closeButton_, &QPushButton::clicked, this, &QDialog::hide);
  connect(helpButton_, &QPushButton::clicked, this, [this]() {
    QMessageBox::information(this, windowTitle(),
        QStringLiteral("Configure channel limits and precision."));
  });
  connect(loprSourceCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, &PvLimitsDialog::handleLowSourceChanged);
  connect(hoprSourceCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, &PvLimitsDialog::handleHighSourceChanged);
  connect(precisionSourceCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, &PvLimitsDialog::handlePrecisionSourceChanged);
  connect(loprEdit_, &QLineEdit::editingFinished, this,
      &PvLimitsDialog::commitLowValue);
  connect(hoprEdit_, &QLineEdit::editingFinished, this,
      &PvLimitsDialog::commitHighValue);
  connect(precisionEdit_, &QLineEdit::editingFinished, this,
      &PvLimitsDialog::commitPrecisionValue);

  adjustSize();
}

void PvLimitsDialog::clearTargets()
{
  mode_ = Mode::kNone;
  precisionSourceGetter_ = {};
  precisionSourceSetter_ = {};
  precisionDefaultGetter_ = {};
  precisionDefaultSetter_ = {};
  meterLimitsGetter_ = {};
  meterLimitsSetter_ = {};
  onChangedCallback_ = {};
  channelLabel_.clear();
  setPrecisionRowVisible(true);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, false);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, false);
  updatePrecisionControls();
  updateMeterControls();
}

void PvLimitsDialog::setTextMonitorCallbacks(const QString &channelName,
    std::function<PvLimitSource()> precisionSourceGetter,
    std::function<void(PvLimitSource)> precisionSourceSetter,
    std::function<int()> precisionDefaultGetter,
    std::function<void(int)> precisionDefaultSetter,
    std::function<void()> changeNotifier,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter)
{
  mode_ = Mode::kTextMonitor;
  precisionSourceGetter_ = std::move(precisionSourceGetter);
  precisionSourceSetter_ = std::move(precisionSourceSetter);
  precisionDefaultGetter_ = std::move(precisionDefaultGetter);
  precisionDefaultSetter_ = std::move(precisionDefaultSetter);
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(true);
  const bool hasLimits = static_cast<bool>(meterLimitsGetter_)
      && static_cast<bool>(meterLimitsSetter_);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, hasLimits);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, hasLimits);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    loprSourceCombo_->setEnabled(hasLimits);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    hoprSourceCombo_->setEnabled(hasLimits);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
  }
  updatePrecisionControls();
  updateMeterControls();
}

void PvLimitsDialog::showForTextMonitor()
{
  if (mode_ != Mode::kTextMonitor) {
    return;
  }
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::setMeterCallbacks(const QString &channelName,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kMeter;
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(true);
  const bool hasLimits = static_cast<bool>(meterLimitsGetter_)
      && static_cast<bool>(meterLimitsSetter_);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, hasLimits);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, hasLimits);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    loprSourceCombo_->setEnabled(hasLimits);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    hoprSourceCombo_->setEnabled(hasLimits);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
  }

  if (meterLimitsGetter_ && meterLimitsSetter_) {
    precisionSourceGetter_ = [this]() {
      if (!meterLimitsGetter_) {
        return PvLimitSource::kChannel;
      }
      return meterLimitsGetter_().precisionSource;
    };
    precisionSourceSetter_ = [this](PvLimitSource source) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      if (source == PvLimitSource::kUser) {
        source = PvLimitSource::kDefault;
      }
      limits.precisionSource = source;
      meterLimitsSetter_(limits);
    };
    precisionDefaultGetter_ = [this]() {
      return meterLimitsGetter_ ? meterLimitsGetter_().precisionDefault : 0;
    };
    precisionDefaultSetter_ = [this](int value) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      limits.precisionDefault = std::clamp(value, 0, 17);
      meterLimitsSetter_(limits);
    };
  } else {
    precisionSourceGetter_ = {};
    precisionSourceSetter_ = {};
    precisionDefaultGetter_ = {};
    precisionDefaultSetter_ = {};
  }

  updateMeterControls();
  updatePrecisionControls();
}

void PvLimitsDialog::showForMeter()
{
  if (mode_ != Mode::kMeter) {
    return;
  }
  updateMeterControls();
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::setStripChartCallbacks(const QString &channelName,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kStripChart;
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  precisionSourceGetter_ = {};
  precisionSourceSetter_ = {};
  precisionDefaultGetter_ = {};
  precisionDefaultSetter_ = {};
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(false);
  const bool hasLimits = static_cast<bool>(meterLimitsGetter_)
      && static_cast<bool>(meterLimitsSetter_);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, hasLimits);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, hasLimits);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    loprSourceCombo_->setEnabled(hasLimits);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    hoprSourceCombo_->setEnabled(hasLimits);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 0, Qt::UserRole - 1);
    precisionSourceCombo_->setEnabled(false);
  }
  updateMeterControls();
  updatePrecisionControls();
}

void PvLimitsDialog::showForStripChart()
{
  if (mode_ != Mode::kStripChart) {
    return;
  }
  updateMeterControls();
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::setSliderCallbacks(const QString &channelName,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kSlider;
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(true);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, true);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, true);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }

  if (meterLimitsGetter_ && meterLimitsSetter_) {
    precisionSourceGetter_ = [this]() {
      if (!meterLimitsGetter_) {
        return PvLimitSource::kChannel;
      }
      return meterLimitsGetter_().precisionSource;
    };
    precisionSourceSetter_ = [this](PvLimitSource source) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      if (source == PvLimitSource::kUser) {
        source = PvLimitSource::kDefault;
      }
      limits.precisionSource = source;
      meterLimitsSetter_(limits);
    };
    precisionDefaultGetter_ = [this]() {
      return meterLimitsGetter_ ? meterLimitsGetter_().precisionDefault : 0;
    };
    precisionDefaultSetter_ = [this](int value) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      limits.precisionDefault = std::clamp(value, 0, 17);
      meterLimitsSetter_(limits);
    };
  } else {
    precisionSourceGetter_ = {};
    precisionSourceSetter_ = {};
    precisionDefaultGetter_ = {};
    precisionDefaultSetter_ = {};
  }

  updateMeterControls();
  updatePrecisionControls();
}

void PvLimitsDialog::showForSlider()
{
  if (mode_ != Mode::kSlider) {
    return;
  }
  updateMeterControls();
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::setWheelSwitchCallbacks(const QString &channelName,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kWheelSwitch;
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(true);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, true);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, true);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }

  if (meterLimitsGetter_ && meterLimitsSetter_) {
    precisionSourceGetter_ = [this]() {
      if (!meterLimitsGetter_) {
        return PvLimitSource::kChannel;
      }
      return meterLimitsGetter_().precisionSource;
    };
    precisionSourceSetter_ = [this](PvLimitSource source) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      if (source == PvLimitSource::kUser) {
        source = PvLimitSource::kDefault;
      }
      limits.precisionSource = source;
      meterLimitsSetter_(limits);
    };
    precisionDefaultGetter_ = [this]() {
      return meterLimitsGetter_ ? meterLimitsGetter_().precisionDefault : 0;
    };
    precisionDefaultSetter_ = [this](int value) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      limits.precisionDefault = std::clamp(value, 0, 17);
      meterLimitsSetter_(limits);
    };
  } else {
    precisionSourceGetter_ = {};
    precisionSourceSetter_ = {};
    precisionDefaultGetter_ = {};
    precisionDefaultSetter_ = {};
  }

  updateMeterControls();
  updatePrecisionControls();
}

void PvLimitsDialog::showForWheelSwitch()
{
  if (mode_ != Mode::kWheelSwitch) {
    return;
  }
  updateMeterControls();
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::setBarCallbacks(const QString &channelName,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kBarMonitor;
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(true);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, true);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, true);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }

  if (meterLimitsGetter_ && meterLimitsSetter_) {
    precisionSourceGetter_ = [this]() {
      if (!meterLimitsGetter_) {
        return PvLimitSource::kChannel;
      }
      return meterLimitsGetter_().precisionSource;
    };
    precisionSourceSetter_ = [this](PvLimitSource source) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      if (source == PvLimitSource::kUser) {
        source = PvLimitSource::kDefault;
      }
      limits.precisionSource = source;
      meterLimitsSetter_(limits);
    };
    precisionDefaultGetter_ = [this]() {
      return meterLimitsGetter_ ? meterLimitsGetter_().precisionDefault : 0;
    };
    precisionDefaultSetter_ = [this](int value) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      limits.precisionDefault = std::clamp(value, 0, 17);
      meterLimitsSetter_(limits);
    };
  } else {
    precisionSourceGetter_ = {};
    precisionSourceSetter_ = {};
    precisionDefaultGetter_ = {};
    precisionDefaultSetter_ = {};
  }

  updateMeterControls();
  updatePrecisionControls();
}

void PvLimitsDialog::showForBarMonitor()
{
  if (mode_ != Mode::kBarMonitor) {
    return;
  }
  updateMeterControls();
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::setScaleCallbacks(const QString &channelName,
    std::function<PvLimits()> limitsGetter,
    std::function<void(const PvLimits &)> limitsSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kScaleMonitor;
  meterLimitsGetter_ = std::move(limitsGetter);
  meterLimitsSetter_ = std::move(limitsSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  setPrecisionRowVisible(true);
  setRowEnabled(loprLabel_, loprSourceCombo_, loprEdit_, true);
  setRowEnabled(hoprLabel_, hoprSourceCombo_, hoprEdit_, true);
  if (loprSourceCombo_) {
    loprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (hoprSourceCombo_) {
    hoprSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setItemData(2, 1, Qt::UserRole - 1);
  }

  if (meterLimitsGetter_ && meterLimitsSetter_) {
    precisionSourceGetter_ = [this]() {
      if (!meterLimitsGetter_) {
        return PvLimitSource::kChannel;
      }
      return meterLimitsGetter_().precisionSource;
    };
    precisionSourceSetter_ = [this](PvLimitSource source) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      if (source == PvLimitSource::kUser) {
        source = PvLimitSource::kDefault;
      }
      limits.precisionSource = source;
      meterLimitsSetter_(limits);
    };
    precisionDefaultGetter_ = [this]() {
      return meterLimitsGetter_ ? meterLimitsGetter_().precisionDefault : 0;
    };
    precisionDefaultSetter_ = [this](int value) {
      if (!meterLimitsGetter_ || !meterLimitsSetter_) {
        return;
      }
      PvLimits limits = meterLimitsGetter_();
      limits.precisionDefault = std::clamp(value, 0, 17);
      meterLimitsSetter_(limits);
    };
  } else {
    precisionSourceGetter_ = {};
    precisionSourceSetter_ = {};
    precisionDefaultGetter_ = {};
    precisionDefaultSetter_ = {};
  }

  updateMeterControls();
  updatePrecisionControls();
}

void PvLimitsDialog::showForScaleMonitor()
{
  if (mode_ != Mode::kScaleMonitor) {
    return;
  }
  updateMeterControls();
  updatePrecisionControls();
  show();
  raise();
  activateWindow();
}

void PvLimitsDialog::updatePrecisionControls()
{
  const bool hasPrecision = (mode_ == Mode::kTextMonitor
      || mode_ == Mode::kMeter || mode_ == Mode::kSlider
      || mode_ == Mode::kWheelSwitch
      || mode_ == Mode::kBarMonitor
      || mode_ == Mode::kScaleMonitor)
      && static_cast<bool>(precisionSourceGetter_);
  if (precisionLabel_) {
    precisionLabel_->setEnabled(hasPrecision);
  }
  const PvLimitSource source = hasPrecision ? precisionSourceGetter_()
                                            : PvLimitSource::kChannel;
  if (precisionSourceCombo_) {
    const QSignalBlocker blocker(precisionSourceCombo_);
    precisionSourceCombo_->setCurrentIndex(sourceIndexFor(source));
    precisionSourceCombo_->setEnabled(hasPrecision);
  }

  if (!precisionEdit_) {
    return;
  }

  const QSignalBlocker blocker(precisionEdit_);
  if (!hasPrecision) {
    precisionEdit_->clear();
    precisionEdit_->setEnabled(false);
    return;
  }

  if (source == PvLimitSource::kDefault) {
    const int value = precisionDefaultGetter_ ? precisionDefaultGetter_() : 0;
    precisionEdit_->setText(QString::number(std::clamp(value, 0, 17)));
    precisionEdit_->setEnabled(true);
  } else {
    precisionEdit_->clear();
    precisionEdit_->setEnabled(false);
  }
}

void PvLimitsDialog::updateMeterControls()
{
  const bool hasLimits = (mode_ == Mode::kTextMonitor
      || mode_ == Mode::kMeter || mode_ == Mode::kStripChart
      || mode_ == Mode::kSlider
      || mode_ == Mode::kWheelSwitch || mode_ == Mode::kBarMonitor
      || mode_ == Mode::kScaleMonitor)
      && static_cast<bool>(meterLimitsGetter_)
      && static_cast<bool>(meterLimitsSetter_);
  PvLimits limits{};
  if (hasLimits) {
    limits = meterLimitsGetter_();
  }

  if (loprSourceCombo_) {
    const QSignalBlocker blocker(loprSourceCombo_);
    const PvLimitSource source = hasLimits ? limits.lowSource
                                           : PvLimitSource::kChannel;
    loprSourceCombo_->setCurrentIndex(sourceIndexFor(source));
    loprSourceCombo_->setEnabled(hasLimits);
  }
  if (hoprSourceCombo_) {
    const QSignalBlocker blocker(hoprSourceCombo_);
    const PvLimitSource source = hasLimits ? limits.highSource
                                           : PvLimitSource::kChannel;
    hoprSourceCombo_->setCurrentIndex(sourceIndexFor(source));
    hoprSourceCombo_->setEnabled(hasLimits);
  }

  if (loprEdit_) {
    const QSignalBlocker blocker(loprEdit_);
    if (!hasLimits || limits.lowSource == PvLimitSource::kChannel) {
      loprEdit_->clear();
      loprEdit_->setEnabled(false);
    } else {
      loprEdit_->setText(QString::number(limits.lowDefault, 'g', 6));
      loprEdit_->setEnabled(true);
    }
  }

  if (hoprEdit_) {
    const QSignalBlocker blocker(hoprEdit_);
    if (!hasLimits || limits.highSource == PvLimitSource::kChannel) {
      hoprEdit_->clear();
      hoprEdit_->setEnabled(false);
    } else {
      hoprEdit_->setText(QString::number(limits.highDefault, 'g', 6));
      hoprEdit_->setEnabled(true);
    }
  }
}

void PvLimitsDialog::handlePrecisionSourceChanged(int index)
{
  if (updating_) {
    return;
  }
  if (!precisionSourceSetter_) {
    updatePrecisionControls();
    return;
  }

  const PvLimitSource selected = sourceForIndex(index);
  if (selected == PvLimitSource::kUser) {
    updatePrecisionControls();
    return;
  }

  updating_ = true;
  precisionSourceSetter_(selected);
  updating_ = false;
  updatePrecisionControls();
  notifyChanged();
}

void PvLimitsDialog::commitPrecisionValue()
{
  if (updating_) {
    return;
  }
  if (!precisionEdit_ || !precisionDefaultSetter_) {
    updatePrecisionControls();
    return;
  }
  if (precisionSourceGetter_ && precisionSourceGetter_() != PvLimitSource::kDefault) {
    updatePrecisionControls();
    return;
  }

  const QString text = precisionEdit_->text().trimmed();
  if (text.isEmpty()) {
    updatePrecisionControls();
    return;
  }
  bool ok = false;
  int value = text.toInt(&ok);
  if (!ok) {
    updatePrecisionControls();
    return;
  }
  value = std::clamp(value, 0, 17);
  updating_ = true;
  precisionDefaultSetter_(value);
  updating_ = false;
  updatePrecisionControls();
  notifyChanged();
}

void PvLimitsDialog::handleLowSourceChanged(int index)
{
  if (updating_) {
    return;
  }
  if ((mode_ != Mode::kTextMonitor && mode_ != Mode::kMeter
          && mode_ != Mode::kStripChart && mode_ != Mode::kSlider
          && mode_ != Mode::kWheelSwitch
          && mode_ != Mode::kBarMonitor && mode_ != Mode::kScaleMonitor)
      || !meterLimitsGetter_ || !meterLimitsSetter_) {
    updateMeterControls();
    return;
  }

  PvLimitSource selected = sourceForIndex(index);
  if (selected == PvLimitSource::kUser) {
    selected = PvLimitSource::kDefault;
  }

  updating_ = true;
  PvLimits limits = meterLimitsGetter_();
  limits.lowSource = selected;
  meterLimitsSetter_(limits);
  updating_ = false;
  updateMeterControls();
  notifyChanged();
}

void PvLimitsDialog::handleHighSourceChanged(int index)
{
  if (updating_) {
    return;
  }
  if ((mode_ != Mode::kTextMonitor && mode_ != Mode::kMeter
          && mode_ != Mode::kStripChart && mode_ != Mode::kSlider
          && mode_ != Mode::kWheelSwitch
          && mode_ != Mode::kBarMonitor && mode_ != Mode::kScaleMonitor)
      || !meterLimitsGetter_ || !meterLimitsSetter_) {
    updateMeterControls();
    return;
  }

  PvLimitSource selected = sourceForIndex(index);
  if (selected == PvLimitSource::kUser) {
    selected = PvLimitSource::kDefault;
  }

  updating_ = true;
  PvLimits limits = meterLimitsGetter_();
  limits.highSource = selected;
  meterLimitsSetter_(limits);
  updating_ = false;
  updateMeterControls();
  notifyChanged();
}

void PvLimitsDialog::commitLowValue()
{
  if (updating_) {
    return;
  }
  if ((mode_ != Mode::kTextMonitor && mode_ != Mode::kMeter
          && mode_ != Mode::kStripChart && mode_ != Mode::kSlider
          && mode_ != Mode::kWheelSwitch
          && mode_ != Mode::kBarMonitor && mode_ != Mode::kScaleMonitor)
      || !meterLimitsGetter_ || !meterLimitsSetter_) {
    updateMeterControls();
    return;
  }
  if (!loprEdit_) {
    return;
  }
  PvLimits limits = meterLimitsGetter_();
  if (limits.lowSource == PvLimitSource::kChannel) {
    updateMeterControls();
    return;
  }

  const QString text = loprEdit_->text().trimmed();
  if (text.isEmpty()) {
    updateMeterControls();
    return;
  }
  bool ok = false;
  double value = text.toDouble(&ok);
  if (!ok) {
    updateMeterControls();
    return;
  }
  updating_ = true;
  limits.lowDefault = value;
  meterLimitsSetter_(limits);
  updating_ = false;
  updateMeterControls();
  notifyChanged();
}

void PvLimitsDialog::commitHighValue()
{
  if (updating_) {
    return;
  }
  if ((mode_ != Mode::kTextMonitor && mode_ != Mode::kMeter
          && mode_ != Mode::kStripChart && mode_ != Mode::kSlider
          && mode_ != Mode::kWheelSwitch
          && mode_ != Mode::kBarMonitor && mode_ != Mode::kScaleMonitor)
      || !meterLimitsGetter_ || !meterLimitsSetter_) {
    updateMeterControls();
    return;
  }
  if (!hoprEdit_) {
    return;
  }
  PvLimits limits = meterLimitsGetter_();
  if (limits.highSource == PvLimitSource::kChannel) {
    updateMeterControls();
    return;
  }

  const QString text = hoprEdit_->text().trimmed();
  if (text.isEmpty()) {
    updateMeterControls();
    return;
  }
  bool ok = false;
  double value = text.toDouble(&ok);
  if (!ok) {
    updateMeterControls();
    return;
  }
  updating_ = true;
  limits.highDefault = value;
  meterLimitsSetter_(limits);
  updating_ = false;
  updateMeterControls();
  notifyChanged();
}

void PvLimitsDialog::notifyChanged()
{
  if (onChangedCallback_) {
    onChangedCallback_();
  }
}

void PvLimitsDialog::setRowEnabled(QLabel *label, QComboBox *combo,
    QLineEdit *edit, bool enabled)
{
  if (label) {
    label->setEnabled(enabled);
  }
  if (combo) {
    combo->setEnabled(enabled);
  }
  if (edit) {
    edit->setEnabled(enabled);
  }
}

void PvLimitsDialog::setPrecisionRowVisible(bool visible)
{
  if (precisionLabel_) {
    precisionLabel_->setVisible(visible);
  }
  if (precisionSourceCombo_) {
    precisionSourceCombo_->setVisible(visible);
  }
  if (precisionEdit_) {
    precisionEdit_->setVisible(visible);
  }
}
