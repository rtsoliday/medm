#include "text_entry_element.h"

#include <algorithm>

#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

#include "text_font_utils.h"

TextEntryElement::TextEntryElement(QWidget *parent)
  : QWidget(parent)
  , lineEdit_(new QLineEdit(this))
{
  setAutoFillBackground(false);
  lineEdit_->setReadOnly(true);
  lineEdit_->setFrame(true);
  lineEdit_->setAlignment(Qt::AlignLeft);
  lineEdit_->setFocusPolicy(Qt::NoFocus);
  lineEdit_->setContextMenuPolicy(Qt::NoContextMenu);
  lineEdit_->setAttribute(Qt::WA_TransparentForMouseEvents);
  lineEdit_->setAutoFillBackground(true);
  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();
  applyPaletteColors();
  updateSelectionVisual();
}

void TextEntryElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  update();
}

bool TextEntryElement::isSelected() const
{
  return selected_;
}

QColor TextEntryElement::foregroundColor() const
{
  return foregroundColor_;
}

void TextEntryElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyPaletteColors();
  update();
}

QColor TextEntryElement::backgroundColor() const
{
  return backgroundColor_;
}

void TextEntryElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  applyPaletteColors();
  update();
}

TextColorMode TextEntryElement::colorMode() const
{
  return colorMode_;
}

void TextEntryElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextMonitorFormat TextEntryElement::format() const
{
  return format_;
}

void TextEntryElement::setFormat(TextMonitorFormat format)
{
  format_ = format;
}

int TextEntryElement::precision() const
{
  if (limits_.precisionSource == PvLimitSource::kDefault) {
    return limits_.precisionDefault;
  }
  return -1;
}

void TextEntryElement::setPrecision(int precision)
{
  if (precision < 0) {
    if (limits_.precisionSource != PvLimitSource::kChannel) {
      limits_.precisionSource = PvLimitSource::kChannel;
    }
    return;
  }

  limits_.precisionDefault = std::clamp(precision, 0, 17);
  limits_.precisionSource = PvLimitSource::kDefault;
}

PvLimitSource TextEntryElement::precisionSource() const
{
  return limits_.precisionSource;
}

void TextEntryElement::setPrecisionSource(PvLimitSource source)
{
  switch (source) {
  case PvLimitSource::kChannel:
    limits_.precisionSource = PvLimitSource::kChannel;
    break;
  case PvLimitSource::kDefault:
    limits_.precisionSource = PvLimitSource::kDefault;
    break;
  case PvLimitSource::kUser:
    limits_.precisionSource = PvLimitSource::kDefault;
    break;
  }
}

int TextEntryElement::precisionDefault() const
{
  return limits_.precisionDefault;
}

void TextEntryElement::setPrecisionDefault(int precision)
{
  limits_.precisionDefault = std::clamp(precision, 0, 17);
}

const PvLimits &TextEntryElement::limits() const
{
  return limits_;
}

void TextEntryElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  if (limits_.precisionSource == PvLimitSource::kUser) {
    limits_.precisionSource = PvLimitSource::kDefault;
  }
}

QString TextEntryElement::channel() const
{
  return channel_;
}

void TextEntryElement::setChannel(const QString &value)
{
  if (channel_ == value) {
    return;
  }
  channel_ = value;
  if (lineEdit_) {
    lineEdit_->setText(channel_);
  }
}

void TextEntryElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (lineEdit_) {
    lineEdit_->setGeometry(rect());
  }
  updateFontForGeometry();
}

void TextEntryElement::paintEvent(QPaintEvent *event)
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

void TextEntryElement::applyPaletteColors()
{
  if (!lineEdit_) {
    return;
  }
  QPalette pal = lineEdit_->palette();
  const QColor fg = foregroundColor_.isValid() ? foregroundColor_
                                               : defaultForegroundColor();
  const QColor bg = backgroundColor_.isValid() ? backgroundColor_
                                               : defaultBackgroundColor();
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::Base, bg);
  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::ButtonText, fg);
  lineEdit_->setPalette(pal);
  lineEdit_->update();
}

void TextEntryElement::updateSelectionVisual()
{
  if (!lineEdit_) {
    return;
  }
  if (selected_) {
    QPalette pal = lineEdit_->palette();
    pal.setColor(QPalette::Text, QColor(Qt::blue));
    pal.setColor(QPalette::WindowText, QColor(Qt::blue));
    pal.setColor(QPalette::ButtonText, QColor(Qt::blue));
    lineEdit_->setPalette(pal);
  } else {
    applyPaletteColors();
  }
}

void TextEntryElement::updateFontForGeometry()
{
  if (!lineEdit_) {
    return;
  }
  const QSize available = lineEdit_->contentsRect().size();
  if (available.isEmpty()) {
    return;
  }

  const QFont newFont = medmCompatibleTextFont(lineEdit_->text(), available);
  if (!newFont.family().isEmpty() && lineEdit_->font() != newFont) {
    lineEdit_->setFont(newFont);
  }
}

QColor TextEntryElement::defaultForegroundColor() const
{
  return palette().color(QPalette::WindowText);
}

QColor TextEntryElement::defaultBackgroundColor() const
{
  return palette().color(QPalette::Base);
}

