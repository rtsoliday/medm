#include "text_monitor_element.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

#include "text_font_utils.h"

namespace {

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

TextMonitorElement::TextMonitorElement(QWidget *parent)
  : QLabel(parent)
{
  setAutoFillBackground(true);
  setWordWrap(false);
  // Reduce margins to match MEDM text positioning and maximize text space
  // Top margin 0 for vertical alignment, left/right 1 for minimal padding
  setContentsMargins(0, 0, 1, 2);
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
    QLabel::setAlignment(alignment_);
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

void TextMonitorElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (executeMode_) {
    designModeText_ = QLabel::text();
    QLabel::setText(QString());
    runtimeConnected_ = false;
    runtimeSeverity_ = 0;
  } else {
    QLabel::setText(designModeText_);
    designModeText_.clear();
    runtimeConnected_ = false;
    runtimeSeverity_ = 0;
  }
  applyPaletteColors();
  updateFontForGeometry();
  update();
}

bool TextMonitorElement::isExecuteMode() const
{
  return executeMode_;
}

void TextMonitorElement::setRuntimeText(const QString &text)
{
  if (!executeMode_) {
    return;
  }
  if (QLabel::text() == text) {
    return;
  }
  QLabel::setText(text);

  /* MEDM's medmTextUpdate.c textUpdateDraw() (line 394-405):
   * - Always starts with the stored base font (ptu->fontIndex from line 118-121)
   * - Checks if actual runtime text fits in widget width
   * - For HORIZ_CENTER and HORIZ_RIGHT: shrinks font until text fits
   * - For HORIZ_LEFT: just clips the text */
  
  /* Start with the stored base font */
  if (!baseFontForExecuteMode_.family().isEmpty()) {
    QFont fontToUse = baseFontForExecuteMode_;
    
    /* For center/right alignment, shrink font if text is too wide */
    if (alignment() & (Qt::AlignHCenter | Qt::AlignRight)) {
      fontToUse = medmTextMonitorFontWithWidthCheck(baseFontForExecuteMode_, text, width());
    }
    
    if (font() != fontToUse) {
      QLabel::setFont(fontToUse);
    }
  }

  update();
}

void TextMonitorElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (executeMode_) {
    applyPaletteColors();
    update();
  }
}

void TextMonitorElement::setRuntimeSeverity(short severity)
{
  if (severity < 0) {
    severity = 0;
  }
  if (runtimeSeverity_ == severity) {
    return;
  }
  runtimeSeverity_ = severity;
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    applyPaletteColors();
    update();
  }
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
  applyPaletteColors();
}

void TextMonitorElement::applyPaletteColors()
{
  QPalette pal = palette();
  const QColor fg = effectiveForegroundColor();
  const QColor bg = effectiveBackgroundColor();
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::ButtonText, fg);
  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::Base, bg);
  setPalette(pal);
}

void TextMonitorElement::updateFontForGeometry()
{
  // Use full widget size, not contentsRect(), to match MEDM behavior
  // MEDM uses dlTextUpdate->object.height directly in executeMonitors.c
  const QSize available(width(), height());
  if (available.isEmpty()) {
    return;
  }

  /* Text Monitor behavior matches MEDM's medmTextUpdate.c:
   * 1. In Edit mode: use actual text for font sizing (line 154)
   * 2. In Execute mode: ALWAYS use DUMMY_TEXT_FIELD ("9.876543") for base font,
   *    store it, and apply it (line 118-121)
   * 3. Runtime text updates will use the stored base font and may shrink from there
   *    (handled in setRuntimeText for center/right alignment) (line 394-405) */
  QString sampleText;
  if (executeMode_) {
    /* Execute mode: calculate and store base font from DUMMY_TEXT_FIELD */
    sampleText = QStringLiteral("9.876543");  /* DUMMY_TEXT_FIELD from medmWidget.h */
    const QFont baseFont = medmTextMonitorFont(sampleText, available);
    if (!baseFont.family().isEmpty()) {
      baseFontForExecuteMode_ = baseFont;
      if (font() != baseFont) {
        QLabel::setFont(baseFont);
      }
    }
  } else {
    /* Edit mode uses actual text */
    sampleText = text();
    const QFont newFont = medmTextMonitorFont(sampleText, available);
    if (!newFont.family().isEmpty() && font() != newFont) {
      QLabel::setFont(newFont);
    }
  }
}

QColor TextMonitorElement::effectiveForegroundColor() const
{
  if (!executeMode_) {
    return foregroundColor_.isValid() ? foregroundColor_
        : defaultForegroundColor();
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
    if (foregroundColor_.isValid()) {
      return foregroundColor_;
    }
    return defaultForegroundColor();
  }
}

QColor TextMonitorElement::effectiveBackgroundColor() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackgroundColor();
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

