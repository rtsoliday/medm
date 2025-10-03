#include "text_monitor_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

#include "text_font_utils.h"

TextMonitorElement::TextMonitorElement(QWidget *parent)
  : QLabel(parent)
{
  setAutoFillBackground(true);
  setWordWrap(true);
  setContentsMargins(2, 2, 2, 2);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
  setForegroundColor(defaultForegroundColor());
  setBackgroundColor(defaultBackgroundColor());
  updateSelectionVisual();
  applyPaletteColors();
}

void TextMonitorElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  update();
}

bool TextMonitorElement::isSelected() const
{
  return selected_;
}

QColor TextMonitorElement::foregroundColor() const
{
  return foregroundColor_;
}

void TextMonitorElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyPaletteColors();
  update();
}

QColor TextMonitorElement::backgroundColor() const
{
  return backgroundColor_;
}

void TextMonitorElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  applyPaletteColors();
  update();
}

Qt::Alignment TextMonitorElement::textAlignment() const
{
  return alignment_;
}

void TextMonitorElement::setTextAlignment(Qt::Alignment alignment)
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

TextColorMode TextMonitorElement::colorMode() const
{
  return colorMode_;
}

void TextMonitorElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
}

TextMonitorFormat TextMonitorElement::format() const
{
  return format_;
}

void TextMonitorElement::setFormat(TextMonitorFormat format)
{
  format_ = format;
}

int TextMonitorElement::precision() const
{
  if (limits_.precisionSource == PvLimitSource::kDefault) {
    return limits_.precisionDefault;
  }
  return -1;
}

void TextMonitorElement::setPrecision(int precision)
{
  if (precision < 0) {
    if (limits_.precisionSource != PvLimitSource::kChannel) {
      limits_.precisionSource = PvLimitSource::kChannel;
    }
    return;
  }

  const int clamped = std::clamp(precision, 0, 17);
  limits_.precisionDefault = clamped;
  limits_.precisionSource = PvLimitSource::kDefault;
}

PvLimitSource TextMonitorElement::precisionSource() const
{
  return limits_.precisionSource;
}

void TextMonitorElement::setPrecisionSource(PvLimitSource source)
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

int TextMonitorElement::precisionDefault() const
{
  return limits_.precisionDefault;
}

void TextMonitorElement::setPrecisionDefault(int precision)
{
  limits_.precisionDefault = std::clamp(precision, 0, 17);
}

const PvLimits &TextMonitorElement::limits() const
{
  return limits_;
}

void TextMonitorElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  if (limits_.precisionSource == PvLimitSource::kUser) {
    limits_.precisionSource = PvLimitSource::kDefault;
  }
}

QString TextMonitorElement::channel(int index) const
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return QString();
  }
  return channels_[index];
}

void TextMonitorElement::setChannel(int index, const QString &value)
{
  if (index < 0 || index >= static_cast<int>(channels_.size())) {
    return;
  }
  if (channels_[index] == value) {
    return;
  }
  channels_[index] = value;
}

void TextMonitorElement::resizeEvent(QResizeEvent *event)
{
  QLabel::resizeEvent(event);
  updateFontForGeometry();
}

void TextMonitorElement::paintEvent(QPaintEvent *event)
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
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void TextMonitorElement::updateSelectionVisual()
{
  if (selected_) {
    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, QColor(Qt::blue));
    pal.setColor(QPalette::Text, QColor(Qt::blue));
    setPalette(pal);
  } else {
    applyPaletteColors();
  }
}

void TextMonitorElement::applyPaletteColors()
{
  QPalette pal = palette();
  const QColor fg = foregroundColor_.isValid() ? foregroundColor_
                                               : defaultForegroundColor();
  const QColor bg = backgroundColor_.isValid() ? backgroundColor_
                                               : defaultBackgroundColor();
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::ButtonText, fg);
  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::Base, bg);
  setPalette(pal);
}

void TextMonitorElement::updateFontForGeometry()
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

QColor TextMonitorElement::defaultForegroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::WindowText);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::WindowText);
  }
  return QColor(Qt::black);
}

QColor TextMonitorElement::defaultBackgroundColor() const
{
  if (const QWidget *parent = parentWidget()) {
    return parent->palette().color(QPalette::Window);
  }
  if (qApp) {
    return qApp->palette().color(QPalette::Window);
  }
  return QColor(Qt::white);
}

