#include "pv_limits_dialog.h"

#include <algorithm>

#include <QComboBox>
#include <QDialogButtonBox>
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
  loprEdit_->setEnabled(false);
  grid->addWidget(loprLabel_, 0, 0);
  grid->addWidget(loprSourceCombo_, 0, 1);
  grid->addWidget(loprEdit_, 0, 2);

  hoprLabel_ = new QLabel(QStringLiteral("High Limit"));
  hoprLabel_->setFont(labelFont_);
  hoprSourceCombo_ = createSourceCombo(valueFont_);
  hoprEdit_ = createValueEdit(valueFont_);
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
  connect(precisionSourceCombo_,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, &PvLimitsDialog::handlePrecisionSourceChanged);
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
  onChangedCallback_ = {};
  channelLabel_.clear();
  updatePrecisionControls();
}

void PvLimitsDialog::setTextMonitorCallbacks(const QString &channelName,
    std::function<PvLimitSource()> precisionSourceGetter,
    std::function<void(PvLimitSource)> precisionSourceSetter,
    std::function<int()> precisionDefaultGetter,
    std::function<void(int)> precisionDefaultSetter,
    std::function<void()> changeNotifier)
{
  mode_ = Mode::kTextMonitor;
  precisionSourceGetter_ = std::move(precisionSourceGetter);
  precisionSourceSetter_ = std::move(precisionSourceSetter);
  precisionDefaultGetter_ = std::move(precisionDefaultGetter);
  precisionDefaultSetter_ = std::move(precisionDefaultSetter);
  onChangedCallback_ = std::move(changeNotifier);
  channelLabel_ = channelName;
  if (titleLabel_) {
    if (channelLabel_.trimmed().isEmpty()) {
      titleLabel_->setText(QStringLiteral("Edit Mode Limits"));
    } else {
      titleLabel_->setText(channelLabel_.trimmed());
    }
  }
  updatePrecisionControls();
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

void PvLimitsDialog::updatePrecisionControls()
{
  const bool hasPrecision = mode_ == Mode::kTextMonitor
      && static_cast<bool>(precisionSourceGetter_);
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
