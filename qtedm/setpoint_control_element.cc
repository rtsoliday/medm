#include "setpoint_control_element.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
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

QString normalizedToleranceModeString(SetpointToleranceMode mode)
{
  return mode == SetpointToleranceMode::kAbsolute
      ? QStringLiteral("absolute")
      : QStringLiteral("none");
}

SetpointToleranceMode toleranceModeFromString(const QString &value)
{
  const QString normalized = value.trimmed().toLower();
  if (normalized == QStringLiteral("absolute")
      || normalized == QStringLiteral("abs")) {
    return SetpointToleranceMode::kAbsolute;
  }
  return SetpointToleranceMode::kNone;
}

void applyFontIfChanged(QWidget *widget, const QFont &font)
{
  if (!widget || font.family().isEmpty() || widget->font() == font) {
    return;
  }
  widget->setFont(font);
}

QFont forceFitFontToWidth(const QFont &font, const QString &text, int maxWidth)
{
  if (font.family().isEmpty() || text.isEmpty() || maxWidth <= 0) {
    return font;
  }
  QFont fitted = font;
  QFontMetrics metrics(fitted);
  if (metrics.horizontalAdvance(text) <= maxWidth) {
    return fitted;
  }
  int pixelSize = fitted.pixelSize();
  if (pixelSize <= 0) {
    pixelSize = metrics.height();
  }
  while (pixelSize > 1) {
    --pixelSize;
    fitted.setPixelSize(pixelSize);
    metrics = QFontMetrics(fitted);
    if (metrics.horizontalAdvance(text) <= maxWidth) {
      return fitted;
    }
  }
  return fitted;
}

} // namespace

SetpointControlElement::SetpointControlElement(QWidget *parent)
  : QWidget(parent)
  , labelWidget_(new QLabel(this))
  , setpointEdit_(new QLineEdit(this))
  , readbackEdit_(new QLineEdit(this))
{
  setAutoFillBackground(true);

  labelWidget_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  setpointEdit_->setAlignment(Qt::AlignRight);
  setpointEdit_->setFrame(true);
  setpointEdit_->setContextMenuPolicy(Qt::NoContextMenu);
  readbackEdit_->setAlignment(Qt::AlignRight);
  readbackEdit_->setReadOnly(true);
  readbackEdit_->setFrame(true);
  readbackEdit_->setFocusPolicy(Qt::NoFocus);
  readbackEdit_->setContextMenuPolicy(Qt::NoContextMenu);
  readbackEdit_->setAttribute(Qt::WA_TransparentForMouseEvents);

  setpointEdit_->installEventFilter(this);

  foregroundColor_ = defaultForegroundColor();
  backgroundColor_ = defaultBackgroundColor();

  connect(setpointEdit_, &QLineEdit::textEdited, this,
      [this](const QString &) {
        if (!executeMode_) {
          return;
        }
        dirty_ = true;
        pending_ = false;
        runtimeNotice_.clear();
        updateStatusText();
      });

  connect(setpointEdit_, &QLineEdit::returnPressed, this,
      [this]() {
        if (canCommitCurrentText() && activationCallback_) {
          activationCallback_(setpointEdit_->text());
        }
      });

  updateDisplayTexts();
  updateLayoutState();
  updateChildInteraction();
  applyPaletteColors();
  updateSelectionVisual();
}

void SetpointControlElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  updateSelectionVisual();
  update();
}

bool SetpointControlElement::isSelected() const
{
  return selected_;
}

QColor SetpointControlElement::foregroundColor() const
{
  return foregroundColor_;
}

void SetpointControlElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultForegroundColor();
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  applyPaletteColors();
  update();
}

QColor SetpointControlElement::backgroundColor() const
{
  return backgroundColor_;
}

void SetpointControlElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : defaultBackgroundColor();
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  applyPaletteColors();
  update();
}

TextColorMode SetpointControlElement::colorMode() const
{
  return colorMode_;
}

void SetpointControlElement::setColorMode(TextColorMode mode)
{
  colorMode_ = mode;
  applyPaletteColors();
  update();
}

TextMonitorFormat SetpointControlElement::format() const
{
  return format_;
}

void SetpointControlElement::setFormat(TextMonitorFormat format)
{
  if (format_ == format) {
    return;
  }
  format_ = format;
  updateDisplayTexts();
}

int SetpointControlElement::precision() const
{
  if (limits_.precisionSource != PvLimitSource::kChannel) {
    return limits_.precisionDefault;
  }
  return -1;
}

void SetpointControlElement::setPrecision(int precision)
{
  if (precision < 0) {
    limits_.precisionSource = PvLimitSource::kChannel;
    return;
  }
  limits_.precisionDefault = std::clamp(precision, 0, 17);
  limits_.precisionSource = PvLimitSource::kDefault;
}

PvLimitSource SetpointControlElement::precisionSource() const
{
  return limits_.precisionSource;
}

void SetpointControlElement::setPrecisionSource(PvLimitSource source)
{
  limits_.precisionSource = source;
}

int SetpointControlElement::precisionDefault() const
{
  return limits_.precisionDefault;
}

void SetpointControlElement::setPrecisionDefault(int precision)
{
  limits_.precisionDefault = std::clamp(precision, 0, 17);
}

const PvLimits &SetpointControlElement::limits() const
{
  return limits_;
}

void SetpointControlElement::setLimits(const PvLimits &limits)
{
  limits_ = limits;
  limits_.precisionDefault = std::clamp(limits_.precisionDefault, 0, 17);
  updateStatusText();
}

double SetpointControlElement::displayLowLimit() const
{
  if (executeMode_ && limits_.lowSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeLow_;
  }
  return limits_.lowDefault;
}

double SetpointControlElement::displayHighLimit() const
{
  if (executeMode_ && limits_.highSource == PvLimitSource::kChannel
      && runtimeLimitsValid_) {
    return runtimeHigh_;
  }
  return limits_.highDefault;
}

QString SetpointControlElement::label() const
{
  return label_;
}

void SetpointControlElement::setLabel(const QString &label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  updateDisplayTexts();
}

QString SetpointControlElement::setpointChannel() const
{
  return setpointChannel_;
}

void SetpointControlElement::setSetpointChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (setpointChannel_ == normalized) {
    return;
  }
  setpointChannel_ = normalized;
  updateDisplayTexts();
}

QString SetpointControlElement::channel() const
{
  return setpointChannel();
}

void SetpointControlElement::setChannel(const QString &channel)
{
  setSetpointChannel(channel);
}

QString SetpointControlElement::readbackChannel() const
{
  return readbackChannel_;
}

void SetpointControlElement::setReadbackChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (readbackChannel_ == normalized) {
    return;
  }
  readbackChannel_ = normalized;
  updateDisplayTexts();
}

SetpointToleranceMode SetpointControlElement::toleranceMode() const
{
  return toleranceMode_;
}

void SetpointControlElement::setToleranceMode(SetpointToleranceMode mode)
{
  if (toleranceMode_ == mode) {
    return;
  }
  toleranceMode_ = mode;
  updateLayoutState();
  updateStatusText();
}

QString SetpointControlElement::toleranceModeString() const
{
  return normalizedToleranceModeString(toleranceMode_);
}

void SetpointControlElement::setToleranceModeString(const QString &mode)
{
  setToleranceMode(toleranceModeFromString(mode));
}

double SetpointControlElement::tolerance() const
{
  return tolerance_;
}

void SetpointControlElement::setTolerance(double tolerance)
{
  const double sanitized = std::isfinite(tolerance)
      ? std::max(0.0, tolerance)
      : 0.0;
  if (std::abs(tolerance_ - sanitized) < 1e-12) {
    return;
  }
  tolerance_ = sanitized;
  updateStatusText();
}

bool SetpointControlElement::showReadback() const
{
  return showReadback_;
}

void SetpointControlElement::setShowReadback(bool show)
{
  if (showReadback_ == show) {
    return;
  }
  showReadback_ = show;
  updateLayoutState();
  updateStatusText();
  updateFontForGeometry();
}

void SetpointControlElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  clearRuntimeState();
  updateChildInteraction();
  updateDisplayTexts();
  applyPaletteColors();
  updateFontForGeometry();
}

bool SetpointControlElement::isExecuteMode() const
{
  return executeMode_;
}

void SetpointControlElement::setSetpointConnected(bool connected)
{
  runtimeSetpointConnected_ = connected;
  if (!connected) {
    runtimeWriteAccess_ = false;
    hasSetpointValue_ = false;
    dirty_ = false;
    pending_ = false;
  }
  updateChildInteraction();
  updateDisplayTexts();
  updateStatusText();
  applyPaletteColors();
}

void SetpointControlElement::setSetpointWriteAccess(bool writeAccess)
{
  runtimeWriteAccess_ = writeAccess;
  updateChildInteraction();
  updateStatusText();
}

void SetpointControlElement::setSetpointSeverity(short severity)
{
  runtimeSetpointSeverity_ = std::clamp<short>(severity, 0, 3);
  applyPaletteColors();
  updateStatusText();
}

void SetpointControlElement::setSetpointValue(double value,
    const QString &text)
{
  if (!std::isfinite(value)) {
    return;
  }
  setpointValue_ = value;
  setpointText_ = text;
  hasSetpointValue_ = true;
  if (!dirty_) {
    applyRuntimeSetpointToEditor();
  }
  if (!pending_) {
    hasAppliedTarget_ = false;
  }
  updateStatusText();
}

void SetpointControlElement::setSetpointMetadata(double low, double high,
    int precision, const QString &units)
{
  if (std::isfinite(low) && std::isfinite(high)) {
    if (std::abs(high - low) < 1e-12) {
      high = low + 1.0;
    }
    runtimeLow_ = std::min(low, high);
    runtimeHigh_ = std::max(low, high);
    runtimeLimitsValid_ = true;
  }
  runtimePrecision_ = std::clamp(precision, 0, 17);
  units_ = units;
  updateDisplayTexts();
}

void SetpointControlElement::setReadbackConnected(bool connected)
{
  runtimeReadbackConnected_ = connected;
  if (!connected) {
    hasReadbackValue_ = false;
  }
  updateDisplayTexts();
  updateStatusText();
  applyPaletteColors();
}

void SetpointControlElement::setReadbackSeverity(short severity)
{
  runtimeReadbackSeverity_ = std::clamp<short>(severity, 0, 3);
  updateStatusText();
  applyPaletteColors();
}

void SetpointControlElement::setReadbackValue(double value,
    const QString &text)
{
  if (!std::isfinite(value)) {
    return;
  }
  readbackValue_ = value;
  readbackText_ = text;
  hasReadbackValue_ = true;
  if (pending_ && isInTolerance()) {
    pending_ = false;
  }
  updateDisplayTexts();
  updateStatusText();
}

void SetpointControlElement::setRuntimeNotice(const QString &notice)
{
  runtimeNotice_ = notice;
  updateStatusText();
}

void SetpointControlElement::acceptAppliedValue(double value,
    const QString &text)
{
  dirty_ = false;
  pending_ = hasEffectiveReadback();
  hasAppliedTarget_ = true;
  appliedTarget_ = value;
  runtimeNotice_.clear();
  if (setpointEdit_) {
    const QSignalBlocker blocker(setpointEdit_);
    setpointEdit_->setText(text);
  }
  updateStatusText();
}

void SetpointControlElement::clearRuntimeState()
{
  runtimeSetpointConnected_ = false;
  runtimeReadbackConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSetpointSeverity_ = 3;
  runtimeReadbackSeverity_ = 3;
  hasSetpointValue_ = false;
  hasReadbackValue_ = false;
  setpointValue_ = 0.0;
  readbackValue_ = 0.0;
  setpointText_.clear();
  readbackText_.clear();
  units_.clear();
  runtimeLow_ = limits_.lowDefault;
  runtimeHigh_ = limits_.highDefault;
  runtimeLimitsValid_ = false;
  runtimePrecision_ = -1;
  dirty_ = false;
  pending_ = false;
  hasAppliedTarget_ = false;
  appliedTarget_ = 0.0;
  runtimeNotice_.clear();
  if (setpointEdit_) {
    const QSignalBlocker blocker(setpointEdit_);
    setpointEdit_->setText(executeMode_ ? QString() : setpointChannel_);
  }
  updateChildInteraction();
  updateDisplayTexts();
  updateStatusText();
  applyPaletteColors();
}

bool SetpointControlElement::runtimeSetpointConnected() const
{
  return runtimeSetpointConnected_;
}

bool SetpointControlElement::runtimeReadbackConnected() const
{
  return runtimeReadbackConnected_;
}

bool SetpointControlElement::runtimeWriteAccess() const
{
  return runtimeWriteAccess_;
}

short SetpointControlElement::runtimeSetpointSeverity() const
{
  return runtimeSetpointSeverity_;
}

short SetpointControlElement::runtimeReadbackSeverity() const
{
  return runtimeReadbackSeverity_;
}

bool SetpointControlElement::hasSetpointValue() const
{
  return hasSetpointValue_;
}

bool SetpointControlElement::hasReadbackValue() const
{
  return hasReadbackValue_;
}

double SetpointControlElement::runtimeSetpointValue() const
{
  return setpointValue_;
}

double SetpointControlElement::runtimeReadbackValue() const
{
  return readbackValue_;
}

QString SetpointControlElement::runtimeSetpointText() const
{
  return setpointText_;
}

QString SetpointControlElement::runtimeReadbackText() const
{
  return readbackText_;
}

QString SetpointControlElement::runtimeStatusText() const
{
  return statusText_;
}

bool SetpointControlElement::isDirty() const
{
  return dirty_;
}

bool SetpointControlElement::isPending() const
{
  return pending_;
}

bool SetpointControlElement::isInTolerance() const
{
  if (toleranceMode_ == SetpointToleranceMode::kNone || !hasEffectiveReadback()) {
    return true;
  }
  if (!hasReadbackValue_) {
    return false;
  }
  const double diff = std::abs(readbackValue_ - toleranceTargetValue());
  return diff <= std::max(0.0, tolerance_);
}

void SetpointControlElement::setActivationCallback(
    const std::function<void(const QString &)> &callback)
{
  activationCallback_ = callback;
  updateChildInteraction();
}

void SetpointControlElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  updateChildGeometry();
  updateFontForGeometry();
}

void SetpointControlElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  if (!selected_) {
    return;
  }

  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

bool SetpointControlElement::eventFilter(QObject *watched, QEvent *event)
{
  if (!event) {
    return QObject::eventFilter(watched, event);
  }
  if (watched == setpointEdit_ && executeMode_
      && event->type() == QEvent::FocusOut && dirty_) {
    runtimeNotice_.clear();
    dirty_ = false;
    pending_ = false;
    applyRuntimeSetpointToEditor();
    updateStatusText();
  }
  if (executeMode_ && isParentWindowInPvInfoMode(this)) {
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
      if (forwardMouseEventToParent(event)) {
        return true;
      }
      break;
    default:
      break;
    }
  }
  return QObject::eventFilter(watched, event);
}

void SetpointControlElement::applyPaletteColors()
{
  const QColor fg = effectiveForegroundColor();
  const QColor labelFg = foregroundColor_.isValid()
      ? foregroundColor_
      : defaultForegroundColor();
  const QColor bg = effectiveBackgroundColor();

  QPalette pal = palette();
  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::WindowText, fg);
  setPalette(pal);

  if (labelWidget_) {
    QPalette childPal = labelWidget_->palette();
    childPal.setColor(QPalette::Window, bg);
    childPal.setColor(QPalette::WindowText, labelFg);
    labelWidget_->setPalette(childPal);
    labelWidget_->setAutoFillBackground(false);
  }

  if (setpointEdit_) {
    QPalette editPal = setpointEdit_->palette();
    editPal.setColor(QPalette::Base, bg.lighter(112));
    editPal.setColor(QPalette::Text, fg);
    editPal.setColor(QPalette::WindowText, fg);
    setpointEdit_->setPalette(editPal);
  }
  applyReadbackStyle();
  update();
}

void SetpointControlElement::updateSelectionVisual()
{
  update();
}

void SetpointControlElement::updateLayoutState()
{
  if (readbackEdit_) {
    readbackEdit_->setVisible(showReadback_);
  }
  updateChildGeometry();
  updateFontForGeometry();
  applyReadbackStyle();
}

void SetpointControlElement::updateChildInteraction()
{
  const bool interactive = executeMode_ && runtimeSetpointConnected_
      && runtimeWriteAccess_ && activationCallback_;
  if (setpointEdit_) {
    setpointEdit_->setReadOnly(!interactive);
    setpointEdit_->setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
    setpointEdit_->setFocusPolicy(interactive ? Qt::StrongFocus : Qt::NoFocus);
  }
  if (executeMode_ && runtimeSetpointConnected_ && !runtimeWriteAccess_) {
    setCursor(CursorUtils::forbiddenCursor());
  } else {
    unsetCursor();
  }
}

void SetpointControlElement::updateDisplayTexts()
{
  if (labelWidget_) {
    labelWidget_->setText(displayLabelText());
  }
  if (!executeMode_ && setpointEdit_) {
    const QSignalBlocker blocker(setpointEdit_);
    setpointEdit_->setText(setpointChannel_);
  }
  if (readbackEdit_) {
    const QSignalBlocker blocker(readbackEdit_);
    if (!executeMode_) {
      readbackEdit_->setText(readbackChannel_);
    } else if (showReadback_) {
      readbackEdit_->setText(hasReadbackValue_ ? readbackText_
                                                : QStringLiteral("--"));
    } else {
      readbackEdit_->clear();
    }
  }
  updateFontForGeometry();
  applyReadbackStyle();
}

void SetpointControlElement::updateStatusText()
{
  if (!runtimeNotice_.isEmpty()) {
    statusText_ = runtimeNotice_;
  } else if (!executeMode_) {
    statusText_ = QStringLiteral("Setpoint Control");
  } else if (setpointChannel_.trimmed().isEmpty()) {
    statusText_ = QStringLiteral("No setpoint PV");
  } else if (!runtimeSetpointConnected_) {
    statusText_ = QStringLiteral("Disconnected");
  } else if (!runtimeWriteAccess_) {
    statusText_ = QStringLiteral("Read-only");
  } else if (dirty_) {
    statusText_ = QStringLiteral("Edited");
  } else if (pending_) {
    statusText_ = isInTolerance() ? QStringLiteral("In tolerance")
                                  : QStringLiteral("Pending readback");
  } else if (hasEffectiveReadback() && hasReadbackValue_) {
    statusText_ = isInTolerance() ? QStringLiteral("In tolerance")
                                  : QStringLiteral("Out of tolerance");
  } else if (hasEffectiveReadback()) {
    statusText_ = QStringLiteral("Readback unavailable");
  } else {
    statusText_ = QStringLiteral("Ready");
  }
  updateFontForGeometry();
  applyReadbackStyle();
  update();
}

void SetpointControlElement::updateFontForGeometry()
{
  if (width() <= 0 || height() <= 0) {
    return;
  }

  updateChildGeometry();

  const QString fontBasisText = (!executeMode_ && labelWidget_)
      ? labelWidget_->text()
      : QStringLiteral("9.876543");
  const QSize fontBasisSize = (!executeMode_ && labelWidget_)
      ? labelWidget_->size()
      : (setpointEdit_ ? setpointEdit_->size() : size());
  const QFont commonFont = medmCompatibleTextFont(fontBasisText,
      fontBasisSize);
  if (commonFont.family().isEmpty()) {
    return;
  }

  QFont fittedFont = commonFont;
  if (labelWidget_) {
    fittedFont = medmTextMonitorFontWithWidthCheck(fittedFont,
        labelWidget_->text(), labelWidget_->width());
    fittedFont = forceFitFontToWidth(fittedFont, labelWidget_->text(),
        labelWidget_->width());
  }
  if (executeMode_) {
    if (setpointEdit_) {
      fittedFont = medmTextMonitorFontWithWidthCheck(fittedFont,
          setpointEdit_->text(), setpointEdit_->contentsRect().width());
      fittedFont = forceFitFontToWidth(fittedFont, setpointEdit_->text(),
          setpointEdit_->contentsRect().width());
    }
    if (readbackEdit_ && readbackEdit_->isVisible()) {
      fittedFont = medmTextMonitorFontWithWidthCheck(fittedFont,
          readbackEdit_->text(), readbackEdit_->contentsRect().width());
      fittedFont = forceFitFontToWidth(fittedFont, readbackEdit_->text(),
          readbackEdit_->contentsRect().width());
    }
  }

  applyFontIfChanged(labelWidget_, fittedFont);
  applyFontIfChanged(setpointEdit_, fittedFont);
  applyFontIfChanged(readbackEdit_, fittedFont);
}

void SetpointControlElement::updateChildGeometry()
{
  const int margin = 3;
  const int spacing = 6;
  const int contentX = margin;
  const int contentY = margin;
  const int contentWidth = std::max(0, width() - 2 * margin);
  const int contentHeight = std::max(1, height() - 2 * margin);
  const int totalSpacing = showReadback_ ? 2 * spacing : spacing;
  const int usableWidth = std::max(0, contentWidth - totalSpacing);
  const int labelWidth = std::max(40, usableWidth * 34 / 100);
  const int valueWidth = std::max(1, usableWidth - labelWidth);
  const int setpointWidth = showReadback_ ? valueWidth / 2 : valueWidth;
  const int readbackWidth = showReadback_ ? valueWidth - setpointWidth : 0;

  int x = contentX;
  if (labelWidget_) {
    labelWidget_->setGeometry(x, contentY, labelWidth, contentHeight);
  }
  x += labelWidth + spacing;
  if (setpointEdit_) {
    setpointEdit_->setGeometry(x, contentY, setpointWidth, contentHeight);
  }
  x += setpointWidth + spacing;
  if (readbackEdit_) {
    readbackEdit_->setGeometry(x, contentY, readbackWidth, contentHeight);
  }
}

void SetpointControlElement::applyRuntimeSetpointToEditor()
{
  if (!setpointEdit_) {
    return;
  }
  const QSignalBlocker blocker(setpointEdit_);
  setpointEdit_->setText(setpointText_);
  setpointEdit_->setCursorPosition(0);
  updateFontForGeometry();
}

void SetpointControlElement::applyReadbackStyle()
{
  if (!readbackEdit_) {
    return;
  }
  const QColor fg = effectiveForegroundColor();
  const QColor bg = effectiveBackgroundColor().lighter(108);
  const QColor border = readbackBorderColor();
  QPalette editPal = readbackEdit_->palette();
  editPal.setColor(QPalette::Base, bg);
  editPal.setColor(QPalette::Text, fg);
  editPal.setColor(QPalette::WindowText, fg);
  readbackEdit_->setPalette(editPal);
  readbackEdit_->setStyleSheet(QStringLiteral(
      "QLineEdit { background-color: %1; color: %2; "
      "border: 2px solid %3; padding-left: 2px; padding-right: 2px; }")
      .arg(bg.name(QColor::HexRgb), fg.name(QColor::HexRgb),
           border.name(QColor::HexRgb)));
}

QColor SetpointControlElement::defaultForegroundColor() const
{
  return palette().color(QPalette::WindowText);
}

QColor SetpointControlElement::defaultBackgroundColor() const
{
  return palette().color(QPalette::Window);
}

QColor SetpointControlElement::effectiveForegroundColor() const
{
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    return alarmColorForSeverity(std::max(runtimeSetpointSeverity_,
        runtimeReadbackSeverity_));
  }
  return foregroundColor_.isValid() ? foregroundColor_ : defaultForegroundColor();
}

QColor SetpointControlElement::effectiveBackgroundColor() const
{
  if (executeMode_ && !runtimeSetpointConnected_) {
    return QColor(Qt::white);
  }
  return backgroundColor_.isValid() ? backgroundColor_ : defaultBackgroundColor();
}

QColor SetpointControlElement::readbackBorderColor() const
{
  if (!executeMode_) {
    return QColor(96, 96, 96);
  }
  if (!hasEffectiveReadback() || !runtimeReadbackConnected_) {
    return QColor(160, 160, 160);
  }
  if (hasReadbackValue_ && !isInTolerance()) {
    return QColor(210, 60, 0);
  }
  return backgroundColor_.isValid() ? backgroundColor_.darker(145)
                                    : defaultBackgroundColor().darker(145);
}

bool SetpointControlElement::hasEffectiveReadback() const
{
  return showReadback_ && !readbackChannel_.trimmed().isEmpty();
}

bool SetpointControlElement::canCommitCurrentText() const
{
  return executeMode_ && runtimeSetpointConnected_ && runtimeWriteAccess_
      && dirty_ && setpointEdit_ && !setpointEdit_->text().trimmed().isEmpty();
}

double SetpointControlElement::toleranceTargetValue() const
{
  if (pending_ && hasAppliedTarget_) {
    return appliedTarget_;
  }
  return setpointValue_;
}

QString SetpointControlElement::displayLabelText() const
{
  if (!label_.trimmed().isEmpty()) {
    return label_;
  }
  if (!setpointChannel_.trimmed().isEmpty()) {
    return setpointChannel_;
  }
  return QStringLiteral("Setpoint");
}

bool SetpointControlElement::forwardMouseEventToParent(QEvent *event) const
{
  auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
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
      mouseEvent->button(), mouseEvent->buttons(), mouseEvent->modifiers());
#else
  const QPoint globalPoint = mouseEvent->globalPos();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(mouseEvent->type(), localPos, localPos,
      QPointF(globalPoint), mouseEvent->button(), mouseEvent->buttons(),
      mouseEvent->modifiers());
#endif
  QCoreApplication::sendEvent(target, &forwarded);
  return true;
}
