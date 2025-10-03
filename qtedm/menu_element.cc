#include "menu_element.h"

#include <QComboBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

namespace {

constexpr int kSampleItemCount = 3;

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
  comboBox_->setAttribute(Qt::WA_TransparentForMouseEvents);
  comboBox_->setAutoFillBackground(true);

  for (int i = 0; i < kSampleItemCount; ++i) {
    comboBox_->addItem(QStringLiteral("Menu Item %1").arg(i + 1));
  }

  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
  applyPaletteColors();
  updateSelectionVisual();
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
  updateSelectionVisual();
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
  updateSelectionVisual();
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

void MenuElement::applyPaletteColors()
{
  if (!comboBox_) {
    return;
  }
  QPalette pal = comboBox_->palette();
  const QColor fg = foregroundColor_.isValid() ? foregroundColor_
                                               : palette().color(QPalette::WindowText);
  const QColor bg = backgroundColor_.isValid() ? backgroundColor_
                                               : palette().color(QPalette::Window);
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
  if (!comboBox_) {
    return;
  }
  if (selected_) {
    QPalette pal = comboBox_->palette();
    pal.setColor(QPalette::Text, QColor(Qt::blue));
    pal.setColor(QPalette::WindowText, QColor(Qt::blue));
    pal.setColor(QPalette::ButtonText, QColor(Qt::blue));
    comboBox_->setPalette(pal);
    comboBox_->update();
  } else {
    applyPaletteColors();
  }
}

