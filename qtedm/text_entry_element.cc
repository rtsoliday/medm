#include "text_entry_element.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QSignalBlocker>

#include "cursor_utils.h"
#include "pv_name_utils.h"
#include "text_font_utils.h"
#include "window_utils.h"

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

TextEntryElement::TextEntryElement(QWidget *parent)
  : QWidget(parent)
  , lineEdit_(new QLineEdit(this))
{
  setAutoFillBackground(false);
  lineEdit_->setReadOnly(true);
  lineEdit_->setFrame(true);
  lineEdit_->setAlignment(Qt::AlignLeft);
  lineEdit_->setFocusPolicy(Qt::StrongFocus);
  lineEdit_->setContextMenuPolicy(Qt::NoContextMenu);
  lineEdit_->setAttribute(Qt::WA_TransparentForMouseEvents);
  lineEdit_->setAutoFillBackground(true);
  lineEdit_->installEventFilter(this);
  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();
  applyPaletteColors();
  updateSelectionVisual();
  updateLineEditState();

  connect(lineEdit_, &QLineEdit::textEdited, this,
      [this](const QString &) {
        if (!executeMode_) {
          return;
        }
        updateAllowed_ = false;
        hasPendingRuntimeText_ = false;
      });

  connect(lineEdit_, &QLineEdit::editingFinished, this,
      [this]() {
        if (!executeMode_) {
          return;
        }
        const bool hadEdits = !updateAllowed_;
        updateAllowed_ = true;
        if (hadEdits && activationCallback_) {
          activationCallback_(lineEdit_->text());
        }
        if (hasPendingRuntimeText_) {
          applyRuntimeTextToLineEdit();
        }
      });
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
  applyPaletteColors();
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

bool TextEntryElement::hasExplicitLimitsBlock() const
{
  return hasExplicitLimitsBlock_;
}

void TextEntryElement::setHasExplicitLimitsBlock(bool hasBlock)
{
  hasExplicitLimitsBlock_ = hasBlock;
}

bool TextEntryElement::hasExplicitLimitsData() const
{
  return hasExplicitLimitsData_;
}

void TextEntryElement::setHasExplicitLimitsData(bool hasData)
{
  hasExplicitLimitsData_ = hasData;
}

bool TextEntryElement::hasExplicitLowLimitData() const
{
  return hasExplicitLowLimitData_;
}

void TextEntryElement::setHasExplicitLowLimitData(bool hasData)
{
  hasExplicitLowLimitData_ = hasData;
}

bool TextEntryElement::hasExplicitHighLimitData() const
{
  return hasExplicitHighLimitData_;
}

void TextEntryElement::setHasExplicitHighLimitData(bool hasData)
{
  hasExplicitHighLimitData_ = hasData;
}

bool TextEntryElement::hasExplicitPrecisionData() const
{
  return hasExplicitPrecisionData_;
}

void TextEntryElement::setHasExplicitPrecisionData(bool hasData)
{
  hasExplicitPrecisionData_ = hasData;
}

QString TextEntryElement::channel() const
{
  return channel_;
}

void TextEntryElement::setChannel(const QString &value)
{
  const QString normalized = PvNameUtils::normalizePvName(value);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  if (!executeMode_ && lineEdit_) {
    QSignalBlocker blocker(lineEdit_);
    lineEdit_->setText(channel_);
    /* Mimic MEDM behavior: show beginning of text, not end */
    lineEdit_->setCursorPosition(0);
    updateFontForGeometry();
  }
}

void TextEntryElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (executeMode_) {
    designModeText_ = lineEdit_ ? lineEdit_->text() : QString();
    runtimeConnected_ = false;
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    runtimeText_.clear();
    updateAllowed_ = true;
    hasPendingRuntimeText_ = false;
    if (lineEdit_) {
      QSignalBlocker blocker(lineEdit_);
      lineEdit_->clear();
    }
  } else {
    runtimeConnected_ = false;
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    runtimeText_.clear();
    updateAllowed_ = true;
    hasPendingRuntimeText_ = false;
    if (lineEdit_) {
      QSignalBlocker blocker(lineEdit_);
      lineEdit_->setText(channel_);
      /* Mimic MEDM behavior: show beginning of text, not end */
      lineEdit_->setCursorPosition(0);
    }
  }
  updateLineEditState();
  applyPaletteColors();
  updateFontForGeometry();
  update();
}

bool TextEntryElement::isExecuteMode() const
{
  return executeMode_;
}

void TextEntryElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    hasPendingRuntimeText_ = false;
    updateAllowed_ = true;
    if (lineEdit_) {
      QSignalBlocker blocker(lineEdit_);
      lineEdit_->clear();
    }
  }
  updateLineEditState();
  if (executeMode_) {
    applyPaletteColors();
    update();
  }
}

void TextEntryElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  updateLineEditState();
}

void TextEntryElement::setRuntimeSeverity(short severity)
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

void TextEntryElement::setRuntimeText(const QString &text)
{
  runtimeText_ = text;
  if (!executeMode_ || !lineEdit_) {
    return;
  }
  if (!updateAllowed_) {
    hasPendingRuntimeText_ = true;
    return;
  }
  applyRuntimeTextToLineEdit();
}

void TextEntryElement::setRuntimeLimits(double low, double high)
{
  if (!std::isfinite(low) || !std::isfinite(high)) {
    return;
  }
  if (std::abs(high - low) < 1e-12) {
    high = low + 1.0;
  }
  runtimeLow_ = low;
  runtimeHigh_ = high;
  runtimeLimitsValid_ = true;
}

void TextEntryElement::setRuntimePrecision(int precision)
{
  int clamped = std::clamp(precision, 0, 17);
  if (runtimePrecision_ == clamped) {
    return;
  }
  runtimePrecision_ = clamped;
}

void TextEntryElement::clearRuntimeState()
{
  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = 0;
  runtimeText_.clear();
  updateAllowed_ = true;
  hasPendingRuntimeText_ = false;
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimeLimitsValid_ = false;
  runtimePrecision_ = -1;
  if (lineEdit_ && executeMode_) {
    QSignalBlocker blocker(lineEdit_);
    lineEdit_->clear();
  }
  updateLineEditState();
  if (executeMode_) {
    applyPaletteColors();
    update();
  }
}

void TextEntryElement::setActivationCallback(
    const std::function<void(const QString &)> &callback)
{
  activationCallback_ = callback;
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
  QColor fg = effectiveForegroundColor();
  QColor bg = effectiveBackgroundColor();
  pal.setColor(QPalette::Text, fg);
  pal.setColor(QPalette::WindowText, fg);
  pal.setColor(QPalette::ButtonText, fg);
  pal.setColor(QPalette::Base, bg);
  pal.setColor(QPalette::Window, bg);
  lineEdit_->setPalette(pal);

  /* Set stylesheet with 2-pixel lowered bevel matching Shell Command style */
  QString fgName = fg.name(QColor::HexRgb);
  QString bgName = bg.name(QColor::HexRgb);
  /* Create lowered bevel: darker top/left, lighter bottom/right (opposite of raised) */
  QString topColor = bg.darker(145).name(QColor::HexRgb);
  QString bottomColor = bg.lighter(135).name(QColor::HexRgb);
  QString stylesheet = QStringLiteral(
      "QLineEdit { background-color: %1; color: %2; "
      "border-width: 2px; border-style: solid; "
      "border-top-color: %3; border-left-color: %3; "
      "border-bottom-color: %4; border-right-color: %4; }")
      .arg(bgName, fgName, topColor, bottomColor);
  lineEdit_->setStyleSheet(stylesheet);

  lineEdit_->update();
}

void TextEntryElement::updateSelectionVisual()
{
  applyPaletteColors();
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

QColor TextEntryElement::effectiveForegroundColor() const
{
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    return alarmColorForSeverity(runtimeSeverity_);
  }
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return defaultForegroundColor();
}

QColor TextEntryElement::effectiveBackgroundColor() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return defaultBackgroundColor();
}

void TextEntryElement::updateLineEditState()
{
  if (!lineEdit_) {
    return;
  }
  const bool interactive = executeMode_ && runtimeConnected_
      && runtimeWriteAccess_ && activationCallback_;
  lineEdit_->setReadOnly(!interactive);
  lineEdit_->setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
  lineEdit_->setFocusPolicy(interactive ? Qt::StrongFocus : Qt::NoFocus);

  /* Update cursor based on write access */
  if (executeMode_ && runtimeConnected_ && !runtimeWriteAccess_) {
    lineEdit_->setCursor(CursorUtils::forbiddenCursor());
    setCursor(CursorUtils::forbiddenCursor());
  } else if (executeMode_) {
    lineEdit_->unsetCursor();
    unsetCursor();
  }
}

void TextEntryElement::applyRuntimeTextToLineEdit()
{
  if (!lineEdit_) {
    return;
  }
  QSignalBlocker blocker(lineEdit_);
  if (lineEdit_->text() != runtimeText_) {
    lineEdit_->setText(runtimeText_);
    /* Mimic MEDM behavior: show beginning of text, not end */
    lineEdit_->setCursorPosition(0);
  }
  hasPendingRuntimeText_ = false;
  updateFontForGeometry();
}

bool TextEntryElement::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == lineEdit_ && event && executeMode_) {
    auto forwardMouseEvent = [&](QMouseEvent *mouseEvent) {
      if (!mouseEvent) {
        return false;
      }
      QWidget *target = window();
      if (!target) {
        return false;
      }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const QPointF globalPosF = mouseEvent->globalPosition();
      const QPoint globalPoint = globalPosF.toPoint();
      const QPointF localPos = target->mapFromGlobal(globalPoint);
      QMouseEvent forwarded(mouseEvent->type(), localPos, localPos, globalPosF,
          mouseEvent->button(), mouseEvent->buttons(),
          mouseEvent->modifiers());
#else
      const QPoint globalPoint = mouseEvent->globalPos();
      const QPointF localPos = target->mapFromGlobal(globalPoint);
      QMouseEvent forwarded(mouseEvent->type(), localPos, localPos,
          QPointF(globalPoint), mouseEvent->button(),
          mouseEvent->buttons(), mouseEvent->modifiers());
#endif
      QCoreApplication::sendEvent(target, &forwarded);
      return true;
    };

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::MiddleButton) {
        if (forwardMouseEvent(mouseEvent)) {
          return true;
        }
      }
      // Forward left clicks to parent when PV Info picking mode is active
      if (mouseEvent->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
        if (forwardMouseEvent(mouseEvent)) {
          return true;
        }
      }
      break;
    }
    case QEvent::MouseMove: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->buttons().testFlag(Qt::MiddleButton)) {
        if (forwardMouseEvent(mouseEvent)) {
          return true;
        }
      }
      break;
    }
    default:
      break;
    }
  }
  return QObject::eventFilter(watched, event);
}
