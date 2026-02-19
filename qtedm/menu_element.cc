#include "menu_element.h"

#include <algorithm>
#include <array>

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStylePainter>

#include "legacy_fonts.h"
#include "cursor_utils.h"
#include "pv_name_utils.h"
#include "window_utils.h"

namespace {

class CenteredDisplayComboBox : public QComboBox
{
public:
  explicit CenteredDisplayComboBox(QWidget *parent = nullptr)
    : QComboBox(parent)
  {
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    Q_UNUSED(event);

    QStylePainter painter(this);
    QStyleOptionComboBox option;
    initStyleOption(&option);

    // Draw frame, arrow and focus as usual, then render centered label text.
    QStyleOptionComboBox frameOption(option);
    frameOption.currentText.clear();
    frameOption.currentIcon = QIcon();
    painter.drawComplexControl(QStyle::CC_ComboBox, frameOption);

    const QRect textRect = style()->subControlRect(
        QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
    if (!textRect.isValid()) {
      return;
    }

    const bool enabled = option.state & QStyle::State_Enabled;
    const QColor textColor = enabled
        ? palette().color(QPalette::ButtonText)
        : palette().color(QPalette::Disabled, QPalette::ButtonText);
    painter.setPen(textColor);
    painter.drawText(
        textRect,
        Qt::AlignCenter,
        fontMetrics().elidedText(
            option.currentText, Qt::ElideRight, textRect.width()));
  }
};

constexpr auto kEditModePlaceholder = "Menu";

const std::array<QString, 16> &menuFontAliases()
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

QFont medmMenuFontForHeight(int widgetHeight)
{
  const int availableHeight = std::max(1, widgetHeight - 8);
  QFont fallback;

  const auto &aliases = menuFontAliases();
  for (auto it = aliases.rbegin(); it != aliases.rend(); ++it) {
    const QFont font = LegacyFonts::font(*it);
    if (font.family().isEmpty()) {
      continue;
    }

    fallback = font;
    const QFontMetrics metrics(font);
    if (metrics.ascent() + metrics.descent() <= availableHeight) {
      return font;
    }
  }

  return fallback;
}

} // namespace

MenuElement::MenuElement(QWidget *parent)
  : QWidget(parent)
  , comboBox_(new CenteredDisplayComboBox(this))
{
  setAutoFillBackground(false);
  setAttribute(Qt::WA_OpaquePaintEvent, false);

  comboBox_->setEditable(false);
  comboBox_->setFocusPolicy(Qt::NoFocus);
  comboBox_->setContextMenuPolicy(Qt::NoContextMenu);
  comboBox_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  comboBox_->setAutoFillBackground(true);
  comboBox_->setCursor(CursorUtils::arrowCursor());

  QObject::connect(comboBox_, QOverload<int>::of(&QComboBox::activated),
      this, [this](int index) {
        if (!executeMode_) {
          return;
        }
        if (!runtimeConnected_ || !runtimeWriteAccess_) {
          if (runtimeConnected_) {
            QApplication::beep();
          }
          QSignalBlocker blocker(comboBox_);
          if (runtimeValue_ >= 0 && runtimeValue_ < comboBox_->count()) {
            comboBox_->setCurrentIndex(runtimeValue_);
          } else {
            comboBox_->setCurrentIndex(-1);
          }
          return;
        }
        if (activationCallback_) {
          activationCallback_(index);
        }
      });

  populateSampleItems();

  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
  applyPaletteColors();
  updateSelectionVisual();
  updateComboBoxEnabledState();
  updateComboBoxFont();
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
  if (executeMode_) {
    applyPaletteColors();
  }
  update();
}

QString MenuElement::channel() const
{
  return channel_;
}

void MenuElement::setChannel(const QString &channel)
{
  const QString normalized = PvNameUtils::normalizePvName(channel);
  if (channel_ == normalized) {
    return;
  }
  channel_ = normalized;
  if (comboBox_) {
    comboBox_->setToolTip(channel_);
  }
}

void MenuElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (comboBox_) {
    comboBox_->setAttribute(Qt::WA_TransparentForMouseEvents, !executeMode_);
    QSignalBlocker blocker(comboBox_);
    comboBox_->clear();
    if (!executeMode_) {
      populateSampleItems();
    }
  }

  runtimeConnected_ = false;
  runtimeWriteAccess_ = false;
  runtimeSeverity_ = 0;
  runtimeValue_ = -1;
  if (!executeMode_) {
    runtimeLabels_.clear();
  }

  updateComboBoxEnabledState();
  updateComboBoxCursor();
  applyPaletteColors();
  updateSelectionVisual();
  updateComboBoxFont();
  update();
}

bool MenuElement::isExecuteMode() const
{
  return executeMode_;
}

void MenuElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!executeMode_) {
    return;
  }
  updateComboBoxEnabledState();
  updateComboBoxCursor();
  applyPaletteColors();
  update();
}

void MenuElement::setRuntimeSeverity(short severity)
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

void MenuElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  if (!executeMode_) {
    return;
  }
  updateComboBoxCursor();
}

void MenuElement::setRuntimeLabels(const QStringList &labels)
{
  if (runtimeLabels_ == labels) {
    return;
  }
  runtimeLabels_ = labels;
  if (!executeMode_ || !comboBox_) {
    return;
  }

  QSignalBlocker blocker(comboBox_);
  comboBox_->clear();
  comboBox_->addItems(runtimeLabels_);
  if (runtimeValue_ >= 0 && runtimeValue_ < comboBox_->count()) {
    comboBox_->setCurrentIndex(runtimeValue_);
  } else {
    comboBox_->setCurrentIndex(-1);
  }
  updateComboBoxFont();
  comboBox_->update();
}

void MenuElement::setRuntimeValue(int value)
{
  runtimeValue_ = value;
  if (!executeMode_ || !comboBox_) {
    return;
  }
  QSignalBlocker blocker(comboBox_);
  if (value >= 0 && value < comboBox_->count()) {
    comboBox_->setCurrentIndex(value);
  } else {
    comboBox_->setCurrentIndex(-1);
  }
  comboBox_->update();
}

void MenuElement::setActivationCallback(const std::function<void(int)> &callback)
{
  activationCallback_ = callback;
}

void MenuElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  if (comboBox_) {
    comboBox_->setGeometry(rect());
  }
  updateComboBoxFont();
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

QColor MenuElement::effectiveForegroundColor() const
{
  if (foregroundColor_.isValid()) {
    return foregroundColor_;
  }
  return palette().color(QPalette::WindowText);
}

QColor MenuElement::effectiveBackgroundColor() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  if (backgroundColor_.isValid()) {
    return backgroundColor_;
  }
  return palette().color(QPalette::Window);
}

void MenuElement::applyPaletteColors()
{
  if (!comboBox_) {
    return;
  }
  QPalette pal = comboBox_->palette();
  const QColor fg = effectiveForegroundColor();
  const QColor bg = effectiveBackgroundColor();
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
  applyPaletteColors();
}

void MenuElement::populateSampleItems()
{
  if (!comboBox_) {
    return;
  }
  comboBox_->addItem(QString::fromLatin1(kEditModePlaceholder));
  comboBox_->setCurrentIndex(0);
  updateComboBoxFont();
}

void MenuElement::updateComboBoxEnabledState()
{
  if (!comboBox_) {
    return;
  }
  if (executeMode_) {
    comboBox_->setEnabled(runtimeConnected_);
  } else {
    comboBox_->setEnabled(true);
  }
}

void MenuElement::updateComboBoxCursor()
{
  if (!comboBox_) {
    return;
  }
  if (executeMode_) {
    comboBox_->setCursor(runtimeWriteAccess_ ? CursorUtils::arrowCursor()
                                             : CursorUtils::forbiddenCursor());
  } else {
    comboBox_->setCursor(CursorUtils::arrowCursor());
  }
}

void MenuElement::updateComboBoxFont()
{
  if (!comboBox_) {
    return;
  }
  const QFont font = medmMenuFontForHeight(height());
  if (font.family().isEmpty()) {
    return;
  }
  if (comboBox_->font() != font) {
    comboBox_->setFont(font);
  }
  if (QAbstractItemView *view = comboBox_->view()) {
    if (view->font() != font) {
      view->setFont(font);
    }
  }
}

void MenuElement::mousePressEvent(QMouseEvent *event)
{
  // Forward middle button and right-click events to parent window for PV info functionality
  if (executeMode_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }
  // Forward left clicks to parent when PV Info picking mode is active
  if (executeMode_ && event->button() == Qt::LeftButton && isParentWindowInPvInfoMode(this)) {
    if (forwardMouseEventToParent(event)) {
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

bool MenuElement::forwardMouseEventToParent(QMouseEvent *event) const
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
