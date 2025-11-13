#include "message_button_element.h"

#include <algorithm>
#include <array>

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>

#include "cursor_utils.h"
#include "legacy_fonts.h"

namespace {

const std::array<QString, 16> &messageButtonFontAliases()
{
  static const std::array<QString, 16> kAliases = {
      QStringLiteral("widgetDM_4"), QStringLiteral("widgetDM_6"),
      QStringLiteral("widgetDM_8"), QStringLiteral("widgetDM_10"),
      QStringLiteral("widgetDM_12"), QStringLiteral("widgetDM_14"),
      QStringLiteral("widgetDM_16"), QStringLiteral("widgetDM_18"),
      QStringLiteral("widgetDM_20"), QStringLiteral("widgetDM_22"),
      QStringLiteral("widgetDM_24"), QStringLiteral("widgetDM_30"),
      QStringLiteral("widgetDM_36"), QStringLiteral("widgetDM_40"),
      QStringLiteral("widgetDM_48"), QStringLiteral("widgetDM_60"),
  };
  return kAliases;
}

QFont medmMessageButtonFont(int widgetHeight)
{
  if (widgetHeight <= 0) {
    return QFont();
  }

  /* Calculate font limit using medm's formula:
   * don't allow height of font to exceed 90% - 4 pixels of messageButton widget
   * (includes nominal 2*shadowThickness=2 shadow) */
  const int fontLimit = static_cast<int>(0.90 * static_cast<double>(widgetHeight)) - 4;
  const int maxHeight = std::max(1, fontLimit);

  const auto &aliases = messageButtonFontAliases();
  QFont fallback;

  /* Search from largest to smallest font, just like medm */
  for (auto it = aliases.rbegin(); it != aliases.rend(); ++it) {
    const QFont font = LegacyFonts::font(*it);
    if (font.family().isEmpty()) {
      continue;
    }

    fallback = font;
    const QFontMetrics metrics(font);
    if (metrics.ascent() + metrics.descent() <= maxHeight) {
      return font;
    }
  }

  return fallback;
}


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
  button_->setCursor(CursorUtils::arrowCursor());
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
  
  /* Paint solid white background when disconnected or no PV defined in execute mode */
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
  }
}

void MessageButtonElement::applyPaletteColors()
{
  if (!button_) {
    return;
  }
  
  /* Hide button when disconnected or no PV defined in execute mode */
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
    button_->hide();
    return;
  } else {
    button_->show();
  }
  
  QPalette pal = button_->palette();
  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();
  const QPalette::ColorRole foregroundRoles[] = {
      QPalette::ButtonText, QPalette::WindowText, QPalette::Text};
  const QPalette::ColorRole backgroundRoles[] = {
      QPalette::Button, QPalette::Base, QPalette::Window};

  for (QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    for (QPalette::ColorRole role : foregroundRoles) {
      pal.setColor(group, role, fg);
    }
    for (QPalette::ColorRole role : backgroundRoles) {
      pal.setColor(group, role, bg);
    }
  }
  button_->setPalette(pal);

  /* Set stylesheet to prevent gradient rendering with 2-pixel raised bevel matching Shell Command */
  QString fgName = fg.name(QColor::HexRgb);
  QString bgName = bg.name(QColor::HexRgb);
  /* Create bevel colors matching ShellCommandElement: lighter top/left, darker bottom/right */
  QString topColor = bg.lighter(135).name(QColor::HexRgb);
  QString bottomColor = bg.darker(145).name(QColor::HexRgb);
  QString innerTopColor = bg.lighter(150).name(QColor::HexRgb);
  QString innerBottomColor = bg.darker(170).name(QColor::HexRgb);
  QString stylesheet = QStringLiteral(
      "QPushButton { background-color: %1; color: %2; "
      "border-width: 2px; border-style: solid; "
      "border-top-color: %3; border-left-color: %3; "
      "border-bottom-color: %4; border-right-color: %4; }")
      .arg(bgName, fgName, topColor, bottomColor);
  button_->setStyleSheet(stylesheet);

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

  const QFont selectedFont = medmMessageButtonFont(widgetHeight);
  button_->setFont(selectedFont);
  button_->update();
}

QColor MessageButtonElement::effectiveForeground() const
{
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
    return QColor(Qt::white);
  }
  if (executeMode_ && colorMode_ == TextColorMode::kAlarm) {
    return alarmColorForSeverity(runtimeSeverity_);
  }
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return palette().color(QPalette::ButtonText);
}

QColor MessageButtonElement::effectiveBackground() const
{
  if (executeMode_ && (!runtimeConnected_ || channel_.trimmed().isEmpty())) {
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
    button_->setCursor(CursorUtils::arrowCursor());
    return;
  }

  const bool enable = runtimeConnected_;
  button_->setEnabled(enable);
  if (runtimeConnected_ && runtimeWriteAccess_) {
    button_->setCursor(CursorUtils::arrowCursor());
  } else {
    button_->setCursor(CursorUtils::forbiddenCursor());
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

void MessageButtonElement::mousePressEvent(QMouseEvent *event)
{
  // Forward middle button and right-click events to parent window for PV info functionality
  if (executeMode_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

bool MessageButtonElement::forwardMouseEventToParent(QMouseEvent *event) const
{
  if (!event) {
    return false;
  }
  QWidget *target = window();
  if (!target) {
    return false;
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF globalPosF = event->globalPosition();
  const QPoint globalPoint = globalPosF.toPoint();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos, globalPosF,
      event->button(), event->buttons(), event->modifiers());
#else
  const QPoint globalPoint = event->globalPos();
  const QPointF localPos = target->mapFromGlobal(globalPoint);
  QMouseEvent forwarded(event->type(), localPos, localPos,
      QPointF(globalPoint), event->button(), event->buttons(),
      event->modifiers());
#endif
  QCoreApplication::sendEvent(target, &forwarded);
  return true;
}
