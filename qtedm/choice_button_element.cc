#include "choice_button_element.h"

#include <algorithm>
#include <cmath>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

constexpr int kSampleButtonCount = 2;
constexpr int kButtonMargin = 2;

QColor blendedColor(const QColor &base, const QColor &overlay, double factor)
{
  if (!base.isValid()) {
    return overlay;
  }
  if (!overlay.isValid()) {
    return base;
  }
  factor = std::clamp(factor, 0.0, 1.0);
  const double inverse = 1.0 - factor;
  const int red = static_cast<int>(std::round(base.red() * inverse + overlay.red() * factor));
  const int green = static_cast<int>(std::round(base.green() * inverse + overlay.green() * factor));
  const int blue = static_cast<int>(std::round(base.blue() * inverse + overlay.blue() * factor));
  return QColor(red, green, blue);
}

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

ChoiceButtonElement::ChoiceButtonElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
}

void ChoiceButtonElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ChoiceButtonElement::isSelected() const
{
  return selected_;
}

QColor ChoiceButtonElement::foregroundColor() const
{
  return foregroundColor_;
}

void ChoiceButtonElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::WindowText);
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  if (executeMode_) {
    updateButtonPalettes();
  } else {
    update();
  }
}

QColor ChoiceButtonElement::backgroundColor() const
{
  return backgroundColor_;
}

void ChoiceButtonElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::Window);
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  if (executeMode_) {
    updateButtonPalettes();
    layoutButtons();
  }
  update();
}

TextColorMode ChoiceButtonElement::colorMode() const
{
  return colorMode_;
}

void ChoiceButtonElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  if (executeMode_) {
    updateButtonPalettes();
  }
  update();
}

ChoiceButtonStacking ChoiceButtonElement::stacking() const
{
  return stacking_;
}

void ChoiceButtonElement::setStacking(ChoiceButtonStacking stacking)
{
  if (stacking_ == stacking) {
    return;
  }
  stacking_ = stacking;
  layoutButtons();
  update();
}

QString ChoiceButtonElement::channel() const
{
  return channel_;
}

void ChoiceButtonElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  for (QAbstractButton *button : buttons_) {
    if (button) {
      button->setToolTip(channel_);
    }
  }
  update();
}

void ChoiceButtonElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (executeMode_) {
    ensureButtonGroup();
    rebuildButtons();
    layoutButtons();
    updateButtonPalettes();
    updateButtonEnabledState();
  } else {
    clearButtons();
    runtimeConnected_ = false;
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    runtimeValue_ = -1;
  }
  update();
}

bool ChoiceButtonElement::isExecuteMode() const
{
  return executeMode_;
}

void ChoiceButtonElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeWriteAccess_ = false;
  }
  updateButtonEnabledState();
  updateButtonPalettes();
  update();
}

void ChoiceButtonElement::setRuntimeSeverity(short severity)
{
  short clamped = severity;
  if (clamped < 0) {
    clamped = 0;
  }
  if (clamped > 3) {
    clamped = 3;
  }
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_) {
    updateButtonPalettes();
  }
  update();
}

void ChoiceButtonElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  updateButtonEnabledState();
}

void ChoiceButtonElement::setRuntimeLabels(const QStringList &labels)
{
  if (runtimeLabels_ == labels) {
    return;
  }
  runtimeLabels_ = labels;
  if (executeMode_) {
    rebuildButtons();
    layoutButtons();
    updateButtonPalettes();
    updateButtonEnabledState();
    setRuntimeValue(runtimeValue_);
  }
  update();
}

void ChoiceButtonElement::setRuntimeValue(int value)
{
  runtimeValue_ = value;
  if (!executeMode_ || !buttonGroup_) {
    update();
    return;
  }

  QSignalBlocker blocker(buttonGroup_);
  bool matched = false;
  if (value >= 0) {
    if (QAbstractButton *button = buttonGroup_->button(value)) {
      button->setChecked(true);
      matched = true;
    }
  }
  if (!matched) {
    for (QAbstractButton *candidate : buttons_) {
      if (candidate) {
        candidate->setChecked(false);
      }
    }
  }
  update();
}

void ChoiceButtonElement::setActivationCallback(
    const std::function<void(int)> &callback)
{
  activationCallback_ = callback;
}

QColor ChoiceButtonElement::effectiveForeground() const
{
  if (executeMode_) {
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    switch (colorMode_) {
    case TextColorMode::kAlarm:
      return alarmColorForSeverity(runtimeSeverity_);
    case TextColorMode::kDiscrete:
    case TextColorMode::kStatic:
    default:
      break;
    }
  }
  return foregroundColor_.isValid() ? foregroundColor_
                                    : palette().color(QPalette::WindowText);
}

QColor ChoiceButtonElement::effectiveBackground() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  return backgroundColor_.isValid() ? backgroundColor_
                                    : palette().color(QPalette::Window);
}

void ChoiceButtonElement::paintSelectionOverlay(QPainter &painter) const
{
  if (!selected_) {
    return;
  }
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ChoiceButtonElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QRect canvas = rect();
  painter.fillRect(canvas, effectiveBackground());

  if (canvas.width() <= 2 || canvas.height() <= 2) {
    paintSelectionOverlay(painter);
    return;
  }

  if (!executeMode_ || buttons_.isEmpty()) {
    int rows = 1;
    int columns = kSampleButtonCount;
    switch (stacking_) {
    case ChoiceButtonStacking::kRow:
      rows = kSampleButtonCount;
      columns = 1;
      break;
    case ChoiceButtonStacking::kRowColumn: {
      const int base = static_cast<int>(std::ceil(std::sqrt(kSampleButtonCount)));
      columns = std::max(1, base);
      rows = static_cast<int>(std::ceil(static_cast<double>(kSampleButtonCount) / columns));
      break;
    }
    case ChoiceButtonStacking::kColumn:
    default:
      rows = 1;
      columns = kSampleButtonCount;
      break;
    }

    rows = std::max(1, rows);
    columns = std::max(1, columns);

    const QRect content = canvas.adjusted(0, 0, -1, -1);
    const int cellWidth = content.width() / columns;
    const int cellHeight = content.height() / rows;
    const int extraWidth = content.width() - cellWidth * columns;
    const int extraHeight = content.height() - cellHeight * rows;

    QColor foreground = effectiveForeground();
    QColor background = effectiveBackground();
    QColor pressedFill = blendedColor(background, foreground, 0.2);
    QColor unpressedFill = blendedColor(background, QColor(Qt::white), 0.15);
    QColor borderColor = blendedColor(foreground, QColor(Qt::black), 0.25);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::NoBrush);

    int buttonIndex = 0;
    int y = content.top();
    for (int row = 0; row < rows; ++row) {
      int rowHeight = cellHeight;
      if (row < extraHeight) {
        rowHeight += 1;
      }
      int x = content.left();
      for (int column = 0; column < columns; ++column) {
        if (buttonIndex >= kSampleButtonCount) {
          break;
        }
        int columnWidth = cellWidth;
        if (column < extraWidth) {
          columnWidth += 1;
        }
        QRect buttonRect(x, y, columnWidth, rowHeight);
        QRect interior = buttonRect.adjusted(kButtonMargin, kButtonMargin,
            -kButtonMargin, -kButtonMargin);
        if (interior.width() <= 0 || interior.height() <= 0) {
          interior = buttonRect.adjusted(1, 1, -1, -1);
        }

        painter.fillRect(interior,
            buttonIndex == 0 ? pressedFill : unpressedFill);
        painter.setPen(QPen(borderColor, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(interior.adjusted(0, 0, -1, -1));

        QString label = QStringLiteral("%1...").arg(buttonIndex);
        QFont labelFont = font();
        if (labelFont.pointSizeF() <= 0.0) {
          labelFont.setPointSize(10);
        }
        QFontMetrics metrics(labelFont);
        const QRect textBounds = interior.adjusted(2, 2, -2, -2);
        while ((metrics.horizontalAdvance(label) > textBounds.width()
                  || metrics.height() > textBounds.height())
            && labelFont.pointSize() > 4) {
          labelFont.setPointSize(labelFont.pointSize() - 1);
          metrics = QFontMetrics(labelFont);
        }
        painter.setFont(labelFont);
        painter.setPen(foreground);
        painter.drawText(textBounds, Qt::AlignCenter, label);

        ++buttonIndex;
        x += columnWidth;
      }
      y += rowHeight;
    }
  }

  paintSelectionOverlay(painter);
}

void ChoiceButtonElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  layoutButtons();
}

void ChoiceButtonElement::ensureButtonGroup()
{
  if (buttonGroup_) {
    return;
  }
  buttonGroup_ = new QButtonGroup(this);
  buttonGroup_->setExclusive(true);
  QObject::connect(buttonGroup_, &QButtonGroup::idClicked,
      this, [this](int id) {
        if (!activationCallback_) {
          return;
        }
        if (QAbstractButton *button = buttonGroup_->button(id)) {
          if (button->isChecked()) {
            activationCallback_(id);
          }
        }
      });
}

void ChoiceButtonElement::clearButtons()
{
  if (buttonGroup_) {
    const auto groupButtons = buttonGroup_->buttons();
    for (QAbstractButton *button : groupButtons) {
      buttonGroup_->removeButton(button);
    }
  }
  for (QAbstractButton *button : buttons_) {
    if (button) {
      button->hide();
      delete button;
    }
  }
  buttons_.clear();
}

void ChoiceButtonElement::rebuildButtons()
{
  if (!executeMode_) {
    clearButtons();
    return;
  }

  ensureButtonGroup();
  clearButtons();

  const int buttonCount = runtimeLabels_.size();
  if (buttonCount <= 0) {
    return;
  }

  buttons_.reserve(buttonCount);
  for (int i = 0; i < buttonCount; ++i) {
    auto *button = new QToolButton(this);
    button->setCheckable(true);
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setText(runtimeLabels_.value(i).trimmed());
    button->setToolTip(channel_);
    button->setAutoExclusive(false);
    button->setCursor(runtimeWriteAccess_ ? Qt::ArrowCursor : Qt::ForbiddenCursor);
    buttonGroup_->addButton(button, i);
    button->show();
    buttons_.append(button);
  }

  QSignalBlocker blocker(buttonGroup_);
  if (runtimeValue_ >= 0) {
    if (QAbstractButton *button = buttonGroup_->button(runtimeValue_)) {
      button->setChecked(true);
    }
  }
}

void ChoiceButtonElement::layoutButtons()
{
  if (!executeMode_ || buttons_.isEmpty()) {
    return;
  }

  const int buttonCount = buttons_.size();
  int rows = 1;
  int columns = buttonCount;
  switch (stacking_) {
  case ChoiceButtonStacking::kRow:
    rows = buttonCount;
    columns = 1;
    break;
  case ChoiceButtonStacking::kRowColumn: {
    const int base = static_cast<int>(std::ceil(std::sqrt(buttonCount)));
    columns = std::max(1, base);
    rows = static_cast<int>(std::ceil(static_cast<double>(buttonCount) / columns));
    break;
  }
  case ChoiceButtonStacking::kColumn:
  default:
    rows = 1;
    columns = buttonCount;
    break;
  }

  rows = std::max(1, rows);
  columns = std::max(1, columns);

  const QRect content = rect().adjusted(0, 0, -1, -1);
  const int cellWidth = columns > 0 ? content.width() / columns : content.width();
  const int cellHeight = rows > 0 ? content.height() / rows : content.height();
  const int extraWidth = content.width() - cellWidth * columns;
  const int extraHeight = content.height() - cellHeight * rows;

  int buttonIndex = 0;
  int y = content.top();
  for (int row = 0; row < rows; ++row) {
    int rowHeight = cellHeight;
    if (row < extraHeight) {
      rowHeight += 1;
    }
    int x = content.left();
    for (int column = 0; column < columns; ++column) {
      if (buttonIndex >= buttonCount) {
        break;
      }
      int columnWidth = cellWidth;
      if (column < extraWidth) {
        columnWidth += 1;
      }
      QRect buttonRect(x, y, columnWidth, rowHeight);
      QRect interior = buttonRect.adjusted(kButtonMargin, kButtonMargin,
          -kButtonMargin, -kButtonMargin);
      if (interior.width() <= 0 || interior.height() <= 0) {
        interior = buttonRect.adjusted(1, 1, -1, -1);
      }

      if (QAbstractButton *button = buttons_.value(buttonIndex)) {
        button->setGeometry(interior);
        applyButtonFont(button, interior);
      }

      ++buttonIndex;
      x += columnWidth;
    }
    y += rowHeight;
  }
}

void ChoiceButtonElement::updateButtonPalettes()
{
  if (!executeMode_) {
    update();
    return;
  }

  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();
  for (QAbstractButton *button : buttons_) {
    if (!button) {
      continue;
    }
    QPalette pal = button->palette();
    pal.setColor(QPalette::ButtonText, fg);
    pal.setColor(QPalette::WindowText, fg);
    pal.setColor(QPalette::Text, fg);
    pal.setColor(QPalette::Button, bg);
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Window, bg);
    button->setPalette(pal);
    button->update();
  }
  update();
}

void ChoiceButtonElement::updateButtonEnabledState()
{
  const bool enabled = runtimeConnected_;
  const Qt::CursorShape cursorShape = runtimeWriteAccess_ ? Qt::ArrowCursor : Qt::ForbiddenCursor;
  for (QAbstractButton *button : buttons_) {
    if (!button) {
      continue;
    }
    button->setEnabled(enabled);
    button->setCursor(cursorShape);
  }
}

void ChoiceButtonElement::applyButtonFont(QAbstractButton *button,
    const QRect &bounds) const
{
  if (!button) {
    return;
  }

  const QRect textBounds = bounds.adjusted(2, 2, -2, -2);
  QFont buttonFont = font();
  if (buttonFont.pointSizeF() <= 0.0) {
    buttonFont.setPointSize(10);
  }

  QFontMetrics metrics(buttonFont);
  const QString label = button->text();
  while (!label.isEmpty()
      && (metrics.horizontalAdvance(label) > textBounds.width()
          || metrics.height() > textBounds.height())
      && buttonFont.pointSize() > 4) {
    buttonFont.setPointSize(buttonFont.pointSize() - 1);
    metrics = QFontMetrics(buttonFont);
  }
  button->setFont(buttonFont);
}
