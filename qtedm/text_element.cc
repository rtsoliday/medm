#include "text_element.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

#include "text_font_utils.h"

TextElement::TextElement(QWidget *parent)
  : QLabel(parent)
{
  setAutoFillBackground(false);
  setWordWrap(true);
  setContentsMargins(2, 2, 2, 2);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
  setForegroundColor(palette().color(QPalette::WindowText));
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  updateSelectionVisual();
}

void TextElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
}

bool TextElement::isSelected() const
{
  return selected_;
}

QColor TextElement::foregroundColor() const
{
  return foregroundColor_;
}

void TextElement::setForegroundColor(const QColor &color)
{
  QColor effective = color;
  if (!effective.isValid()) {
    effective = defaultForegroundColor();
  }
  const bool changed = (foregroundColor_ != effective);
  foregroundColor_ = effective;
  applyTextColor();
  if (changed) {
    update();
  }
}

void TextElement::setText(const QString &value)
{
  QLabel::setText(value);
  updateFontForGeometry();
}

Qt::Alignment TextElement::textAlignment() const
{
  return alignment_;
}

void TextElement::setTextAlignment(Qt::Alignment alignment)
{
  Qt::Alignment effective = alignment;
  if (!(effective & Qt::AlignHorizontal_Mask)) {
    effective |= Qt::AlignLeft;
  }
  effective &= ~Qt::AlignVertical_Mask;
  effective |= Qt::AlignTop;
  if (alignment_ == effective) {
    return;
  }
  alignment_ = effective;
  QLabel::setAlignment(alignment_);
}

TextColorMode TextElement::colorMode() const
{
  return colorMode_;
}

void TextElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextVisibilityMode TextElement::visibilityMode() const
{
  return visibilityMode_;
}

void TextElement::setVisibilityMode(TextVisibilityMode mode)
{
  visibilityMode_ = mode;
}

QString TextElement::visibilityCalc() const
{
  return visibilityCalc_;
}

void TextElement::setVisibilityCalc(const QString &calc)
{
  if (visibilityCalc_ == calc) {
    return;
  }
  visibilityCalc_ = calc;
}

QString TextElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void TextElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void TextElement::resizeEvent(QResizeEvent *event)
{
  QLabel::resizeEvent(event);
  updateFontForGeometry();
}

void TextElement::paintEvent(QPaintEvent *event)
{
  QLabel::paintEvent(event);
  if (!selected_) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

QColor TextElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

void TextElement::applyTextColor()
{
  QPalette pal = palette();
  pal.setColor(QPalette::WindowText, foregroundColor_);
  pal.setColor(QPalette::Text, foregroundColor_);
  pal.setColor(QPalette::ButtonText, foregroundColor_);
  setPalette(pal);
}

void TextElement::updateSelectionVisual()
{
  QPalette pal = palette();
  if (selected_) {
    pal.setColor(QPalette::WindowText, QColor(Qt::blue));
  } else {
    pal.setColor(QPalette::WindowText, foregroundColor_);
  }
  setPalette(pal);
  update();
}

void TextElement::updateFontForGeometry()
{
  const QSize available = contentsRect().size();
  if (available.isEmpty()) {
    return;
  }

  const QFont newFont = medmCompatibleTextFont(text(), available);
  if (newFont.family().isEmpty()) {
    return;
  }

  if (font() != newFont) {
    QLabel::setFont(newFont);
  }
}

