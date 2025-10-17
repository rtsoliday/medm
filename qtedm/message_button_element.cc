#include "message_button_element.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>

namespace {

QString defaultLabel()
{
  return QStringLiteral("Message Button");
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

class SelectionAwarePushButton : public QPushButton
{
public:
  explicit SelectionAwarePushButton(MessageButtonElement *owner)
    : QPushButton(owner)
    , owner_(owner)
  {
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    QPushButton::paintEvent(event);
    if (!owner_ || !owner_->isSelected()) {
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

private:
  MessageButtonElement *owner_ = nullptr;
};

} // namespace

MessageButtonElement::MessageButtonElement(QWidget *parent)
  : QWidget(parent)
  , button_(new SelectionAwarePushButton(this))
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  button_->setAutoFillBackground(true);
  button_->setFocusPolicy(Qt::NoFocus);
  button_->setDefault(false);
  button_->setAutoDefault(false);
  button_->setCheckable(false);
  button_->setContextMenuPolicy(Qt::NoContextMenu);
  button_->setCursor(Qt::ArrowCursor);
  button_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  button_->setText(defaultLabel());

  QObject::connect(button_, &QPushButton::pressed, this,
      [this]() {
        handleButtonPressed();
      });
  QObject::connect(button_, &QPushButton::released, this,
      [this]() {
        handleButtonReleased();
      });

  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
  applyPaletteColors();
  updateSelectionVisual();
  updateButtonFont();
}

void MessageButtonElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  update();
}

bool MessageButtonElement::isSelected() const
{
  return selected_;
}

QColor MessageButtonElement::foregroundColor() const
{
  return foregroundColor_;
}

void MessageButtonElement::setForegroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : palette().color(QPalette::WindowText);
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyPaletteColors();
  updateSelectionVisual();
  update();
}

QColor MessageButtonElement::backgroundColor() const
{
  return backgroundColor_;
}

void MessageButtonElement::setBackgroundColor(const QColor &color)
{
  const QColor effective = color.isValid() ? color : palette().color(QPalette::Window);
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  applyPaletteColors();
  updateSelectionVisual();
  update();
}

TextColorMode MessageButtonElement::colorMode() const
{
  return colorMode_;
}

void MessageButtonElement::setColorMode(TextColorMode mode)
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

QString MessageButtonElement::label() const
{
  return label_;
}

void MessageButtonElement::setLabel(const QString &label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  if (button_) {
    button_->setText(effectiveLabel());
    updateButtonFont();
  }
}

QString MessageButtonElement::pressMessage() const
{
  return pressMessage_;
}

void MessageButtonElement::setPressMessage(const QString &message)
{
  pressMessage_ = message;
}

QString MessageButtonElement::releaseMessage() const
{
  return releaseMessage_;
}

void MessageButtonElement::setReleaseMessage(const QString &message)
{
  releaseMessage_ = message;
}

QString MessageButtonElement::channel() const
{
  return channel_;
}

void MessageButtonElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  if (button_) {
    button_->setToolTip(channel_);
  }
}

void MessageButtonElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = 0;
  if (button_) {
    button_->setAttribute(Qt::WA_TransparentForMouseEvents, !executeMode_);
    button_->setDown(false);
  }
  applyPaletteColors();
  updateButtonState();
  updateSelectionVisual();
  update();
}

bool MessageButtonElement::isExecuteMode() const
{
  return executeMode_;
}

void MessageButtonElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!executeMode_) {
    return;
  }
  updateButtonState();
  applyPaletteColors();
  update();
}

void MessageButtonElement::setRuntimeSeverity(short severity)
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

void MessageButtonElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  if (!executeMode_) {
    return;
  }
  updateButtonState();
}

void MessageButtonElement::setPressCallback(const std::function<void()> &callback)
{
  pressCallback_ = callback;
}

void MessageButtonElement::setReleaseCallback(const std::function<void()> &callback)
{
  releaseCallback_ = callback;
}

void MessageButtonElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (button_) {
    button_->setGeometry(rect());
    updateButtonFont();
  }
}

void MessageButtonElement::changeEvent(QEvent *event)
{
  QWidget::changeEvent(event);
  if (!event) {
    return;
  }
  if (event->type() == QEvent::FontChange) {
    updateButtonFont();
  }
}

void MessageButtonElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);
}

void MessageButtonElement::applyPaletteColors()
{
  if (!button_) {
    return;
  }
  QPalette pal = button_->palette();
  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();
  pal.setColor(QPalette::ButtonText, fg);
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::Button, bg);
  pal.setColor(QPalette::Base, bg);
  pal.setColor(QPalette::Window, bg);
  button_->setPalette(pal);
  updateButtonState();
  button_->update();
}

void MessageButtonElement::updateSelectionVisual()
{
  if (!button_) {
    return;
  }
  button_->update();
}

QString MessageButtonElement::effectiveLabel() const
{
  const QString trimmed = label_.trimmed();
  return trimmed.isEmpty() ? defaultLabel() : trimmed;
}

void MessageButtonElement::updateButtonFont()
{
  if (!button_) {
    return;
  }

  const int widgetHeight = button_->height();
  if (widgetHeight <= 0) {
    button_->setFont(font());
    button_->update();
    return;
  }

  int fontLimit = static_cast<int>(0.90 * static_cast<double>(widgetHeight)) - 4;
  if (fontLimit < 1) {
    fontLimit = 1;
  }

  QFont adjusted = font();
  if (fontLimit <= 0) {
    button_->setFont(adjusted);
    button_->update();
    return;
  }

  if (adjusted.pixelSize() > 0) {
    adjusted.setPixelSize(fontLimit);
  } else {
    qreal pointSize = adjusted.pointSizeF();
    if (pointSize <= 0.0) {
      pointSize = adjusted.pointSize();
    }
    if (pointSize <= 0.0) {
      QFontInfo info(adjusted);
      pointSize = info.pointSizeF();
    }
    if (pointSize <= 0.0) {
      pointSize = 12.0;
    }

    QFontMetricsF baseMetrics(adjusted);
    qreal textHeight = baseMetrics.ascent() + baseMetrics.descent();
    if (textHeight <= 0.0) {
      textHeight = static_cast<qreal>(fontLimit);
    }

    qreal scaledPoint = pointSize * static_cast<qreal>(fontLimit) / textHeight;
    if (scaledPoint < 1.0) {
      scaledPoint = 1.0;
    }
    adjusted.setPointSizeF(scaledPoint);

    QFontMetricsF scaledMetrics(adjusted);
    qreal scaledHeight = scaledMetrics.ascent() + scaledMetrics.descent();
    int iterations = 0;
    while (scaledHeight > fontLimit && scaledPoint > 1.0 && iterations < 16) {
      scaledPoint = std::max(1.0, scaledPoint - 0.5);
      adjusted.setPointSizeF(scaledPoint);
      scaledMetrics = QFontMetricsF(adjusted);
      scaledHeight = scaledMetrics.ascent() + scaledMetrics.descent();
      ++iterations;
    }
  }

  button_->setFont(adjusted);
  button_->update();
}

QColor MessageButtonElement::effectiveForeground() const
{
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    return alarmColorForSeverity(runtimeSeverity_);
  }
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return palette().color(QPalette::ButtonText);
}

QColor MessageButtonElement::effectiveBackground() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return palette().color(QPalette::Button);
}

void MessageButtonElement::updateButtonState()
{
  if (!button_) {
    return;
  }
  if (!executeMode_) {
    button_->setEnabled(true);
    button_->setCursor(Qt::ArrowCursor);
    return;
  }

  const bool enable = runtimeConnected_;
  button_->setEnabled(enable);
  if (runtimeConnected_ && runtimeWriteAccess_) {
    button_->setCursor(Qt::ArrowCursor);
  } else {
    button_->setCursor(Qt::ForbiddenCursor);
  }
  if (!enable) {
    button_->setDown(false);
  }
}

void MessageButtonElement::handleButtonPressed()
{
  if (!executeMode_) {
    return;
  }
  if (!runtimeConnected_ || !runtimeWriteAccess_) {
    QApplication::beep();
    if (button_) {
      button_->setDown(false);
    }
    return;
  }
  if (pressCallback_) {
    pressCallback_();
  }
}

void MessageButtonElement::handleButtonReleased()
{
  if (!executeMode_) {
    return;
  }
  if (!runtimeConnected_ || !runtimeWriteAccess_) {
    if (button_) {
      button_->setDown(false);
    }
    return;
  }
  if (releaseCallback_) {
    releaseCallback_();
  }
}
