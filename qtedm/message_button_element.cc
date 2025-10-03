#include "message_button_element.h"

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

} // namespace

MessageButtonElement::MessageButtonElement(QWidget *parent)
  : QWidget(parent)
  , button_(new QPushButton(this))
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  button_->setAutoFillBackground(true);
  button_->setFocusPolicy(Qt::NoFocus);
  button_->setDefault(false);
  button_->setAutoDefault(false);
  button_->setCheckable(false);
  button_->setContextMenuPolicy(Qt::NoContextMenu);
  button_->setAttribute(Qt::WA_TransparentForMouseEvents);
  button_->setText(defaultLabel());

  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
  applyPaletteColors();
  updateSelectionVisual();
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

void MessageButtonElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (button_) {
    button_->setGeometry(rect());
  }
}

void MessageButtonElement::paintEvent(QPaintEvent *event)
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
  button_->update();
}

void MessageButtonElement::updateSelectionVisual()
{
  if (!button_) {
    return;
  }
  if (selected_) {
    QPalette pal = button_->palette();
    pal.setColor(QPalette::ButtonText, QColor(Qt::blue));
    pal.setColor(QPalette::WindowText, QColor(Qt::blue));
    pal.setColor(QPalette::Text, QColor(Qt::blue));
    button_->setPalette(pal);
    button_->update();
  } else {
    applyPaletteColors();
  }
}

QString MessageButtonElement::effectiveLabel() const
{
  const QString trimmed = label_.trimmed();
  return trimmed.isEmpty() ? defaultLabel() : trimmed;
}

QColor MessageButtonElement::effectiveForeground() const
{
  return foregroundColor_.isValid() ? foregroundColor_
                                    : palette().color(QPalette::WindowText);
}

QColor MessageButtonElement::effectiveBackground() const
{
  return backgroundColor_.isValid() ? backgroundColor_
                                    : palette().color(QPalette::Window);
}

