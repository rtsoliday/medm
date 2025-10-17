#include "menu_element.h"

#include <algorithm>

#include <QComboBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QSignalBlocker>

namespace {

constexpr int kSampleItemCount = 3;

QColor alarmColorForSeverity(short severity)
{
  switch (severity) {
  case 0:
    return QColor(0, 205, 0);
  case 1:
    return QColor(255, 255, 0);
  case 2:
    return QColor(255, 0, 0);
  case 3:
    return QColor(255, 255, 255);
  default:
    return QColor(204, 204, 204);
  }
}

} // namespace

MenuElement::MenuElement(QWidget *parent)
  : QWidget(parent)
  , comboBox_(new QComboBox(this))
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  comboBox_->setEditable(false);
  comboBox_->setFocusPolicy(Qt::NoFocus);
  comboBox_->setContextMenuPolicy(Qt::NoContextMenu);
  comboBox_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  comboBox_->setAutoFillBackground(true);
  comboBox_->setCursor(Qt::ArrowCursor);

  QObject::connect(comboBox_, QOverload<int>::of(&QComboBox::activated),
      this, [this](int index) {
        if (!executeMode_) {
          return;
        }
        if (!runtimeConnected_) {
          QSignalBlocker blocker(comboBox_);
          if (runtimeValue_ >= 0 && runtimeValue_ < comboBox_->count()) {
            comboBox_->setCurrentIndex(runtimeValue_);
          } else {
            comboBox_->setCurrentIndex(-1);
          }
          return;
        }
        if (activationCallback_) {
          activationCallback_(index);
        }
      });

  populateSampleItems();

  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
  applyPaletteColors();
  updateSelectionVisual();
  updateComboBoxEnabledState();
}

void MenuElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  update();
}

bool MenuElement::isSelected() const
{
  return selected_;
}

QColor MenuElement::foregroundColor() const
{
  return foregroundColor_;
}

void MenuElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::WindowText);
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyPaletteColors();
  update();
}

QColor MenuElement::backgroundColor() const
{
  return backgroundColor_;
}

void MenuElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::Window);
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  applyPaletteColors();
  update();
}

TextColorMode MenuElement::colorMode() const
{
  return colorMode_;
}

void MenuElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  if (executeMode_) {
    applyPaletteColors();
  }
  update();
}

QString MenuElement::channel() const
{
  return channel_;
}

void MenuElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  if (comboBox_) {
    comboBox_->setToolTip(channel_);
  }
}

void MenuElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (comboBox_) {
    comboBox_->setAttribute(Qt::WA_TransparentForMouseEvents, !executeMode_);
    QSignalBlocker blocker(comboBox_);
    comboBox_->clear();
    if (!executeMode_) {
      populateSampleItems();
    }
  }

  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = 0;
  runtimeValue_ = -1;
  if (!executeMode_) {
    runtimeLabels_.clear();
  }

  updateComboBoxEnabledState();
  updateComboBoxCursor();
  applyPaletteColors();
  updateSelectionVisual();
  update();
}

bool MenuElement::isExecuteMode() const
{
  return executeMode_;
}

void MenuElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!executeMode_) {
    return;
  }
  updateComboBoxEnabledState();
  updateComboBoxCursor();
  applyPaletteColors();
  update();
}

void MenuElement::setRuntimeSeverity(short severity)
{
  short clamped = std::clamp<short>(severity, 0, 3);
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    applyPaletteColors();
    update();
  }
}

void MenuElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  if (!executeMode_) {
    return;
  }
  updateComboBoxCursor();
}

void MenuElement::setRuntimeLabels(const QStringList &labels)
{
  if (runtimeLabels_ == labels) {
    return;
  }
  runtimeLabels_ = labels;
  if (!executeMode_ || !comboBox_) {
    return;
  }

  QSignalBlocker blocker(comboBox_);
  comboBox_->clear();
  comboBox_->addItems(runtimeLabels_);
  if (runtimeValue_ >= 0 && runtimeValue_ < comboBox_->count()) {
    comboBox_->setCurrentIndex(runtimeValue_);
  } else {
    comboBox_->setCurrentIndex(-1);
  }
  comboBox_->update();
}

void MenuElement::setRuntimeValue(int value)
{
  runtimeValue_ = value;
  if (!executeMode_ || !comboBox_) {
    return;
  }
  QSignalBlocker blocker(comboBox_);
  if (value >= 0 && value < comboBox_->count()) {
    comboBox_->setCurrentIndex(value);
  } else {
    comboBox_->setCurrentIndex(-1);
  }
  comboBox_->update();
}

void MenuElement::setActivationCallback(const std::function<void(int)> &callback)
{
  activationCallback_ = callback;
}

void MenuElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (comboBox_) {
    comboBox_->setGeometry(rect());
  }
}

void MenuElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  if (!selected_) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

QColor MenuElement::effectiveForegroundColor() const
{
  if (executeMode_) {
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    if (colorMode_ == TextColorMode::kAlarm) {
      return alarmColorForSeverity(runtimeSeverity_);
    }
  }
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return palette().color(QPalette::WindowText);
}

QColor MenuElement::effectiveBackgroundColor() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return palette().color(QPalette::Window);
}

void MenuElement::applyPaletteColors()
{
  if (!comboBox_) {
    return;
  }
  QPalette pal = comboBox_->palette();
  QColor fg = effectiveForegroundColor();
  if (!executeMode_ && selected_) {
    fg = QColor(Qt::blue);
  }
  const QColor bg = effectiveBackgroundColor();
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::ButtonText, fg);
  pal.setColor(QPalette::Base, bg);
  pal.setColor(QPalette::Button, bg);
  pal.setColor(QPalette::Window, bg);
  comboBox_->setPalette(pal);
  comboBox_->update();
}

void MenuElement::updateSelectionVisual()
{
  applyPaletteColors();
}

void MenuElement::populateSampleItems()
{
  if (!comboBox_) {
    return;
  }
  for (int i = 0; i < kSampleItemCount; ++i) {
    comboBox_->addItem(QStringLiteral("Menu Item %1").arg(i + 1));
  }
  if (comboBox_->count() > 0) {
    comboBox_->setCurrentIndex(0);
  }
}

void MenuElement::updateComboBoxEnabledState()
{
  if (!comboBox_) {
    return;
  }
  if (executeMode_) {
    comboBox_->setEnabled(runtimeConnected_);
  } else {
    comboBox_->setEnabled(true);
  }
}

void MenuElement::updateComboBoxCursor()
{
  if (!comboBox_) {
    return;
  }
  if (executeMode_) {
    comboBox_->setCursor(runtimeWriteAccess_ ? Qt::ArrowCursor : Qt::ForbiddenCursor);
  } else {
    comboBox_->setCursor(Qt::ArrowCursor);
  }
}

