#include "cartesian_axis_dialog.h"

#include <algorithm>
#include <utility>

#include <QComboBox>
#include <QDoubleValidator>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

int axisStyleToIndex(CartesianPlotAxisStyle style)
{
  switch (style) {
  case CartesianPlotAxisStyle::kLog10:
    return 1;
  case CartesianPlotAxisStyle::kTime:
    return 2;
  case CartesianPlotAxisStyle::kLinear:
  default:
    return 0;
  }
}

CartesianPlotAxisStyle indexToAxisStyle(int index)
{
  switch (index) {
  case 1:
    return CartesianPlotAxisStyle::kLog10;
  case 2:
    return CartesianPlotAxisStyle::kTime;
  default:
    return CartesianPlotAxisStyle::kLinear;
  }
}

int rangeStyleToIndex(CartesianPlotRangeStyle style)
{
  switch (style) {
  case CartesianPlotRangeStyle::kUserSpecified:
    return 1;
  case CartesianPlotRangeStyle::kAutoScale:
    return 2;
  case CartesianPlotRangeStyle::kChannel:
  default:
    return 0;
  }
}

CartesianPlotRangeStyle indexToRangeStyle(int index)
{
  switch (index) {
  case 1:
    return CartesianPlotRangeStyle::kUserSpecified;
  case 2:
    return CartesianPlotRangeStyle::kAutoScale;
  default:
    return CartesianPlotRangeStyle::kChannel;
  }
}

struct TimeFormatItem
{
  CartesianPlotTimeFormat format;
  const char *label;
};

constexpr TimeFormatItem kTimeFormatItems[] = {
    {CartesianPlotTimeFormat::kHhMmSs, "hh:mm:ss"},
    {CartesianPlotTimeFormat::kHhMm, "hh:mm"},
    {CartesianPlotTimeFormat::kHh00, "hh:00"},
    {CartesianPlotTimeFormat::kMonthDayYear, "MMM DD YYYY"},
    {CartesianPlotTimeFormat::kMonthDay, "MMM DD"},
    {CartesianPlotTimeFormat::kMonthDayHour00, "MMM DD hh:00"},
    {CartesianPlotTimeFormat::kWeekdayHour00, "wd hh:00"},
};

int timeFormatToIndex(CartesianPlotTimeFormat format)
{
  for (int i = 0; i < static_cast<int>(std::size(kTimeFormatItems)); ++i) {
    if (kTimeFormatItems[i].format == format) {
      return i;
    }
  }
  return 0;
}

CartesianPlotTimeFormat indexToTimeFormat(int index)
{
  if (index < 0 || index >= static_cast<int>(std::size(kTimeFormatItems))) {
    return CartesianPlotTimeFormat::kHhMmSs;
  }
  return kTimeFormatItems[index].format;
}

QString styleDisplayName(CartesianPlotAxisStyle style)
{
  switch (style) {
  case CartesianPlotAxisStyle::kLog10:
    return QStringLiteral("Log10");
  case CartesianPlotAxisStyle::kTime:
    return QStringLiteral("Time");
  case CartesianPlotAxisStyle::kLinear:
  default:
    return QStringLiteral("Linear");
  }
}

QString rangeDisplayName(CartesianPlotRangeStyle style)
{
  switch (style) {
  case CartesianPlotRangeStyle::kUserSpecified:
    return QStringLiteral("User-specified");
  case CartesianPlotRangeStyle::kAutoScale:
    return QStringLiteral("Auto-scale");
  case CartesianPlotRangeStyle::kChannel:
  default:
    return QStringLiteral("Channel");
  }
}

} // namespace

CartesianAxisDialog::CartesianAxisDialog(const QPalette &basePalette,
    const QFont &labelFont, const QFont &valueFont, QWidget *parent)
  : QDialog(parent)
  , labelFont_(labelFont)
  , valueFont_(valueFont)
{
  setWindowTitle(QStringLiteral("Cartesian Plot Axis Data"));
  setModal(false);
  setAutoFillBackground(true);
  setPalette(basePalette);
  setBackgroundRole(QPalette::Window);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setSizeGripEnabled(true);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  auto *formLayout = new QGridLayout;
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setHorizontalSpacing(8);
  formLayout->setVerticalSpacing(6);

  auto createLabel = [this]() {
    auto *label = new QLabel;
    label->setFont(labelFont_);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
  };

  auto configureLineEdit = [this](QLineEdit *edit) {
    edit->setFont(valueFont_);
    edit->setAutoFillBackground(true);
    QPalette palette = edit->palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    edit->setPalette(palette);
    return edit;
  };

  int row = 0;
  {
    auto *label = createLabel();
    label->setText(QStringLiteral("Axis"));
    formLayout->addWidget(label, row, 0);
    axisCombo_ = new QComboBox;
    axisCombo_->setFont(valueFont_);
    axisCombo_->setAutoFillBackground(true);
    axisCombo_->addItem(QStringLiteral("X Axis"));
    axisCombo_->addItem(QStringLiteral("Y1 Axis"));
    axisCombo_->addItem(QStringLiteral("Y2 Axis"));
    axisCombo_->addItem(QStringLiteral("Y3 Axis"));
    axisCombo_->addItem(QStringLiteral("Y4 Axis"));
    formLayout->addWidget(axisCombo_, row, 1);
    QObject::connect(axisCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &CartesianAxisDialog::handleAxisChanged);
  }

  ++row;
  {
    auto *label = createLabel();
    label->setText(QStringLiteral("Axis Style"));
    formLayout->addWidget(label, row, 0);
    axisStyleCombo_ = new QComboBox;
    axisStyleCombo_->setFont(valueFont_);
    axisStyleCombo_->setAutoFillBackground(true);
    // Initially populate with Linear and Log10 only
    // Time will be added dynamically for X axis only in refreshForAxis()
    axisStyleCombo_->addItem(styleDisplayName(CartesianPlotAxisStyle::kLinear));
    axisStyleCombo_->addItem(styleDisplayName(CartesianPlotAxisStyle::kLog10));
    formLayout->addWidget(axisStyleCombo_, row, 1);
    QObject::connect(axisStyleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &CartesianAxisDialog::handleAxisStyleChanged);
  }

  ++row;
  {
    auto *label = createLabel();
    label->setText(QStringLiteral("Range Style"));
    formLayout->addWidget(label, row, 0);
    rangeStyleCombo_ = new QComboBox;
    rangeStyleCombo_->setFont(valueFont_);
    rangeStyleCombo_->setAutoFillBackground(true);
    rangeStyleCombo_->addItem(rangeDisplayName(CartesianPlotRangeStyle::kChannel));
    rangeStyleCombo_->addItem(rangeDisplayName(CartesianPlotRangeStyle::kUserSpecified));
    rangeStyleCombo_->addItem(rangeDisplayName(CartesianPlotRangeStyle::kAutoScale));
    formLayout->addWidget(rangeStyleCombo_, row, 1);
    QObject::connect(rangeStyleCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &CartesianAxisDialog::handleRangeStyleChanged);
  }

  ++row;
  {
    auto *label = createLabel();
    label->setText(QStringLiteral("Minimum"));
    formLayout->addWidget(label, row, 0);
    minEdit_ = configureLineEdit(new QLineEdit);
    minEdit_->setValidator(new QDoubleValidator(minEdit_));
    formLayout->addWidget(minEdit_, row, 1);
    QObject::connect(minEdit_, &QLineEdit::editingFinished,
        this, &CartesianAxisDialog::commitMinimum);
  }

  ++row;
  {
    auto *label = createLabel();
    label->setText(QStringLiteral("Maximum"));
    formLayout->addWidget(label, row, 0);
    maxEdit_ = configureLineEdit(new QLineEdit);
    maxEdit_->setValidator(new QDoubleValidator(maxEdit_));
    formLayout->addWidget(maxEdit_, row, 1);
    QObject::connect(maxEdit_, &QLineEdit::editingFinished,
        this, &CartesianAxisDialog::commitMaximum);
  }

  ++row;
  {
    auto *label = createLabel();
    label->setText(QStringLiteral("Time Format"));
    timeFormatCombo_ = new QComboBox;
    timeFormatCombo_->setFont(valueFont_);
    timeFormatCombo_->setAutoFillBackground(true);
    for (const auto &item : kTimeFormatItems) {
      timeFormatCombo_->addItem(QString::fromLatin1(item.label));
    }
    formLayout->addWidget(label, row, 0);
    formLayout->addWidget(timeFormatCombo_, row, 1);
    timeFormatCombo_->setEnabled(false);
    QObject::connect(timeFormatCombo_,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &CartesianAxisDialog::handleTimeFormatChanged);
    timeFormatCombo_->setVisible(false);
    label->setVisible(false);
    timeFormatLabel_ = label;
  }

  mainLayout->addLayout(formLayout);

  auto *buttonLayout = new QHBoxLayout;
  buttonLayout->addStretch(1);
  closeButton_ = new QPushButton(QStringLiteral("Close"));
  closeButton_->setFont(valueFont_);
  buttonLayout->addWidget(closeButton_);
  QObject::connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);
  mainLayout->addLayout(buttonLayout);

  clearCallbacks();
}

void CartesianAxisDialog::clearCallbacks()
{
  for (auto &getter : styleGetters_) {
    getter = {};
  }
  for (auto &setter : styleSetters_) {
    setter = {};
  }
  for (auto &getter : rangeGetters_) {
    getter = {};
  }
  for (auto &setter : rangeSetters_) {
    setter = {};
  }
  for (auto &getter : minimumGetters_) {
    getter = {};
  }
  for (auto &setter : minimumSetters_) {
    setter = {};
  }
  for (auto &getter : maximumGetters_) {
    getter = {};
  }
  for (auto &setter : maximumSetters_) {
    setter = {};
  }
  for (auto &getter : timeFormatGetters_) {
    getter = {};
  }
  for (auto &setter : timeFormatSetters_) {
    setter = {};
  }
  changeNotifier_ = {};
  currentAxisIndex_ = 0;
  updating_ = true;
  axisCombo_->setCurrentIndex(0);
  axisCombo_->setEnabled(false);
  axisStyleCombo_->setEnabled(false);
  rangeStyleCombo_->setEnabled(false);
  minEdit_->setEnabled(false);
  maxEdit_->setEnabled(false);
  timeFormatCombo_->setEnabled(false);
  timeFormatCombo_->setVisible(false);
  if (timeFormatLabel_) {
    timeFormatLabel_->setEnabled(false);
    timeFormatLabel_->setVisible(false);
  }
  minEdit_->clear();
  maxEdit_->clear();
  updating_ = false;
}

void CartesianAxisDialog::setCartesianCallbacks(
    std::array<std::function<CartesianPlotAxisStyle()>, kCartesianAxisCount> styleGetters,
    std::array<std::function<void(CartesianPlotAxisStyle)>, kCartesianAxisCount> styleSetters,
    std::array<std::function<CartesianPlotRangeStyle()>, kCartesianAxisCount> rangeGetters,
    std::array<std::function<void(CartesianPlotRangeStyle)>, kCartesianAxisCount> rangeSetters,
    std::array<std::function<double()>, kCartesianAxisCount> minimumGetters,
    std::array<std::function<void(double)>, kCartesianAxisCount> minimumSetters,
    std::array<std::function<double()>, kCartesianAxisCount> maximumGetters,
    std::array<std::function<void(double)>, kCartesianAxisCount> maximumSetters,
    std::array<std::function<CartesianPlotTimeFormat()>, kCartesianAxisCount> timeFormatGetters,
    std::array<std::function<void(CartesianPlotTimeFormat)>, kCartesianAxisCount> timeFormatSetters,
    std::function<void()> changeNotifier)
{
  styleGetters_ = std::move(styleGetters);
  styleSetters_ = std::move(styleSetters);
  rangeGetters_ = std::move(rangeGetters);
  rangeSetters_ = std::move(rangeSetters);
  minimumGetters_ = std::move(minimumGetters);
  minimumSetters_ = std::move(minimumSetters);
  maximumGetters_ = std::move(maximumGetters);
  maximumSetters_ = std::move(maximumSetters);
  timeFormatGetters_ = std::move(timeFormatGetters);
  timeFormatSetters_ = std::move(timeFormatSetters);
  changeNotifier_ = std::move(changeNotifier);

  axisCombo_->setEnabled(true);
  axisStyleCombo_->setEnabled(true);
  rangeStyleCombo_->setEnabled(true);
  refreshForAxis(std::clamp(currentAxisIndex_, 0, kCartesianAxisCount - 1));
}

void CartesianAxisDialog::showDialog()
{
  refreshForAxis(std::clamp(currentAxisIndex_, 0, kCartesianAxisCount - 1));
  updateControlStates();
  show();
  raise();
  activateWindow();
}

void CartesianAxisDialog::keyPressEvent(QKeyEvent *event)
{
  // Handle Enter/Return key to commit changes without closing dialog
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    // Check which field has focus and commit that value
    if (minEdit_ && minEdit_->hasFocus()) {
      commitMinimum();
    } else if (maxEdit_ && maxEdit_->hasFocus()) {
      commitMaximum();
    }
    // Accept the event to prevent it from triggering dialog close
    event->accept();
    return;
  }
  
  // For all other keys, use default behavior
  QDialog::keyPressEvent(event);
}

void CartesianAxisDialog::handleAxisChanged(int index)
{
  if (updating_) {
    return;
  }
  refreshForAxis(index);
}

void CartesianAxisDialog::handleAxisStyleChanged(int index)
{
  if (updating_) {
    return;
  }
  const int axis = currentAxisIndex_;
  if (axis < 0 || axis >= kCartesianAxisCount) {
    return;
  }
  if (!styleSetters_[axis]) {
    return;
  }
  styleSetters_[axis](indexToAxisStyle(index));
  if (changeNotifier_) {
    changeNotifier_();
  }
  updateControlStates();
}

void CartesianAxisDialog::handleRangeStyleChanged(int index)
{
  if (updating_) {
    return;
  }
  const int axis = currentAxisIndex_;
  if (axis < 0 || axis >= kCartesianAxisCount) {
    return;
  }
  if (!rangeSetters_[axis]) {
    return;
  }
  rangeSetters_[axis](indexToRangeStyle(index));
  if (changeNotifier_) {
    changeNotifier_();
  }
  updateControlStates();
}

void CartesianAxisDialog::handleTimeFormatChanged(int index)
{
  if (updating_) {
    return;
  }
  const int axis = currentAxisIndex_;
  if (axis < 0 || axis >= kCartesianAxisCount) {
    return;
  }
  if (!timeFormatSetters_[axis]) {
    return;
  }
  timeFormatSetters_[axis](indexToTimeFormat(index));
  if (changeNotifier_) {
    changeNotifier_();
  }
}

void CartesianAxisDialog::commitMinimum()
{
  const int axis = currentAxisIndex_;
  if (axis < 0 || axis >= kCartesianAxisCount || updating_) {
    return;
  }
  if (!minimumSetters_[axis]) {
    if (minimumGetters_[axis]) {
      const QSignalBlocker blocker(minEdit_);
      minEdit_->setText(QString::number(minimumGetters_[axis](), 'g', 7));
    }
    return;
  }
  bool ok = false;
  double value = minEdit_->text().toDouble(&ok);
  if (!ok) {
    if (minimumGetters_[axis]) {
      const QSignalBlocker blocker(minEdit_);
      minEdit_->setText(QString::number(minimumGetters_[axis](), 'g', 7));
    }
    return;
  }
  minimumSetters_[axis](value);
  if (changeNotifier_) {
    changeNotifier_();
  }
  if (minimumGetters_[axis]) {
    const QSignalBlocker blocker(minEdit_);
    minEdit_->setText(QString::number(minimumGetters_[axis](), 'g', 7));
  }
}

void CartesianAxisDialog::commitMaximum()
{
  const int axis = currentAxisIndex_;
  if (axis < 0 || axis >= kCartesianAxisCount || updating_) {
    return;
  }
  if (!maximumSetters_[axis]) {
    if (maximumGetters_[axis]) {
      const QSignalBlocker blocker(maxEdit_);
      maxEdit_->setText(QString::number(maximumGetters_[axis](), 'g', 7));
    }
    return;
  }
  bool ok = false;
  double value = maxEdit_->text().toDouble(&ok);
  if (!ok) {
    if (maximumGetters_[axis]) {
      const QSignalBlocker blocker(maxEdit_);
      maxEdit_->setText(QString::number(maximumGetters_[axis](), 'g', 7));
    }
    return;
  }
  maximumSetters_[axis](value);
  if (changeNotifier_) {
    changeNotifier_();
  }
  if (maximumGetters_[axis]) {
    const QSignalBlocker blocker(maxEdit_);
    maxEdit_->setText(QString::number(maximumGetters_[axis](), 'g', 7));
  }
}

void CartesianAxisDialog::refreshForAxis(int axisIndex)
{
  updating_ = true;
  currentAxisIndex_ = std::clamp(axisIndex, 0, kCartesianAxisCount - 1);

  {
    const QSignalBlocker blocker(axisCombo_);
    axisCombo_->setCurrentIndex(currentAxisIndex_);
  }

  // Update axis style combo box items based on which axis is selected
  // Time is only valid for X axis (index 0)
  const bool isXAxis = (currentAxisIndex_ == 0);
  {
    const QSignalBlocker blocker(axisStyleCombo_);
    
    // Save current style before clearing
    CartesianPlotAxisStyle currentStyle = CartesianPlotAxisStyle::kLinear;
    if (styleGetters_[currentAxisIndex_]) {
      currentStyle = styleGetters_[currentAxisIndex_]();
      // If a Y axis somehow has Time style, treat it as Linear
      if (!isXAxis && currentStyle == CartesianPlotAxisStyle::kTime) {
        currentStyle = CartesianPlotAxisStyle::kLinear;
      }
    }
    
    // Rebuild combo box items
    axisStyleCombo_->clear();
    axisStyleCombo_->addItem(styleDisplayName(CartesianPlotAxisStyle::kLinear));
    axisStyleCombo_->addItem(styleDisplayName(CartesianPlotAxisStyle::kLog10));
    if (isXAxis) {
      axisStyleCombo_->addItem(styleDisplayName(CartesianPlotAxisStyle::kTime));
    }
    
    // Restore selection
    axisStyleCombo_->setCurrentIndex(axisStyleToIndex(currentStyle));
  }

  const bool hasStyleSetter = static_cast<bool>(styleSetters_[currentAxisIndex_]);
  axisStyleCombo_->setEnabled(hasStyleSetter);

  const bool hasRangeGetter = static_cast<bool>(rangeGetters_[currentAxisIndex_]);
  const bool hasRangeSetter = static_cast<bool>(rangeSetters_[currentAxisIndex_]);
  if (hasRangeGetter) {
    const QSignalBlocker blocker(rangeStyleCombo_);
    rangeStyleCombo_->setCurrentIndex(
        rangeStyleToIndex(rangeGetters_[currentAxisIndex_]()));
  } else {
    const QSignalBlocker blocker(rangeStyleCombo_);
    rangeStyleCombo_->setCurrentIndex(0);
  }
  rangeStyleCombo_->setEnabled(hasRangeSetter);

  if (minimumGetters_[currentAxisIndex_]) {
    const QSignalBlocker blocker(minEdit_);
    minEdit_->setText(QString::number(minimumGetters_[currentAxisIndex_](), 'g', 7));
  } else {
    const QSignalBlocker blocker(minEdit_);
    minEdit_->clear();
  }

  if (maximumGetters_[currentAxisIndex_]) {
    const QSignalBlocker blocker(maxEdit_);
    maxEdit_->setText(QString::number(maximumGetters_[currentAxisIndex_](), 'g', 7));
  } else {
    const QSignalBlocker blocker(maxEdit_);
    maxEdit_->clear();
  }

  if (currentAxisIndex_ == 0 && timeFormatGetters_[0]) {
    const QSignalBlocker blocker(timeFormatCombo_);
    timeFormatCombo_->setCurrentIndex(
        timeFormatToIndex(timeFormatGetters_[0]()));
  } else if (currentAxisIndex_ == 0) {
    const QSignalBlocker blocker(timeFormatCombo_);
    timeFormatCombo_->setCurrentIndex(0);
  }

  updateControlStates();
  updating_ = false;
}

void CartesianAxisDialog::updateControlStates()
{
  const bool hasMinSetter = currentAxisIndex_ >= 0
      && currentAxisIndex_ < kCartesianAxisCount
      && static_cast<bool>(minimumSetters_[currentAxisIndex_]);
  const bool hasMaxSetter = currentAxisIndex_ >= 0
      && currentAxisIndex_ < kCartesianAxisCount
      && static_cast<bool>(maximumSetters_[currentAxisIndex_]);
  const auto rangeStyle = indexToRangeStyle(rangeStyleCombo_->currentIndex());
  const bool enableMinMax = rangeStyle == CartesianPlotRangeStyle::kUserSpecified;
  minEdit_->setEnabled(enableMinMax && hasMinSetter);
  maxEdit_->setEnabled(enableMinMax && hasMaxSetter);

  const bool isXAxis = currentAxisIndex_ == 0;
  const bool axisIsTime = axisStyleCombo_->currentIndex()
      == axisStyleToIndex(CartesianPlotAxisStyle::kTime);
  const bool hasTimeSetter = isXAxis
      && static_cast<bool>(timeFormatSetters_[currentAxisIndex_]);
  const bool enableTime = isXAxis && axisIsTime && hasTimeSetter;
  timeFormatCombo_->setEnabled(enableTime);
  timeFormatCombo_->setVisible(isXAxis && hasTimeSetter);
  if (timeFormatLabel_) {
    timeFormatLabel_->setEnabled(enableTime);
    timeFormatLabel_->setVisible(isXAxis && hasTimeSetter);
  }
}
