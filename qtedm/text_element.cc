#include "text_element.h"

#include <algorithm>

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

#include "text_font_utils.h"

namespace {

constexpr int kTextMargin = 0;

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

TextElement::TextElement(QWidget *parent)
  : QLabel(parent)
{
  setAutoFillBackground(false);
  setWordWrap(false);
  setContentsMargins(kTextMargin, kTextMargin, kTextMargin, kTextMargin);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
  setForegroundColor(palette().color(QPalette::WindowText));
  setColorMode(TextColorMode::kStatic);
  setVisibilityMode(TextVisibilityMode::kStatic);
  updateSelectionVisual();
  designModeVisible_ = QLabel::isVisible();
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
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyTextColor();
  update();
}

void TextElement::setText(const QString &value)
{
  QLabel::setText(value);
  updateFontForGeometry();
}

QRect TextElement::boundingRect() const
{
  QRect bounds = QLabel::rect();
  bounds.adjust(-kTextMargin, -kTextMargin, kTextMargin, kTextMargin);
  return bounds;
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

void TextElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }

  if (execute) {
    designModeVisible_ = QLabel::isVisible();
  }

  executeMode_ = execute;
  runtimeConnected_ = false;
  runtimeVisible_ = true;
  runtimeSeverity_ = 0;
  updateExecuteState();
}

bool TextElement::isExecuteMode() const
{
  return executeMode_;
}

void TextElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    if (colorMode_ == TextColorMode::kAlarm) {
      applyTextColor();
    }
    applyTextVisibility();
    update();
  }
}

void TextElement::setRuntimeVisible(bool visible)
{
  if (runtimeVisible_ == visible) {
    return;
  }
  runtimeVisible_ = visible;
  if (executeMode_) {
    applyTextVisibility();
    update();
  }
}

void TextElement::setRuntimeSeverity(short severity)
{
  if (severity < 0) {
    severity = 0;
  }
  severity = std::min<short>(severity, 3);
  if (runtimeSeverity_ == severity) {
    return;
  }
  runtimeSeverity_ = severity;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    applyTextColor();
    update();
  }
}

void TextElement::setVisible(bool visible)
{
  if (!executeMode_) {
    designModeVisible_ = visible;
  }
  QLabel::setVisible(visible);
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
  const QColor color = effectiveForegroundColor();
  QPalette pal = palette();
  pal.setColor(QPalette::WindowText, color);
  pal.setColor(QPalette::Text, color);
  pal.setColor(QPalette::ButtonText, color);
  setPalette(pal);
}

void TextElement::applyTextVisibility()
{
  if (executeMode_) {
    const bool visible = designModeVisible_ && runtimeVisible_ && runtimeConnected_;
    QLabel::setVisible(visible);
  } else {
    QLabel::setVisible(designModeVisible_);
  }
}

void TextElement::updateSelectionVisual()
{
  // Keep the configured foreground color even when selected; the dashed border
  // drawn in paintEvent() is sufficient to indicate selection.
  applyTextColor();
  update();
}

QColor TextElement::effectiveForegroundColor() const
{
  QColor baseColor = foregroundColor_.isValid() ? foregroundColor_ : defaultForegroundColor();
  if (!executeMode_) {
    return baseColor;
  }

  switch (colorMode_) {
  case TextColorMode::kAlarm:
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    return alarmColorForSeverity(runtimeSeverity_);
  case TextColorMode::kDiscrete:
  case TextColorMode::kStatic:
  default:
    return baseColor;
  }
}

void TextElement::updateExecuteState()
{
  applyTextColor();
  applyTextVisibility();
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

