#include "choice_button_element.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QButtonGroup>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QResizeEvent>
#include <QSignalBlocker>

#include "legacy_fonts.h"
#include "cursor_utils.h"

namespace {

constexpr int kSampleButtonCount = 2;
constexpr int kButtonMargin = 0;
constexpr int kChoiceButtonShadow = 4;

const std::array<QString, 16> &choiceButtonFontAliases()
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

int availableFontHeight(int widgetHeight, int buttonCount,
    ChoiceButtonStacking stacking)
{
  const int count = std::max(1, buttonCount);
  const int totalHeight = std::max(1, widgetHeight);

  int available = 1;
  switch (stacking) {
  case ChoiceButtonStacking::kRow: {
    const int perButton = totalHeight / count;
    available = perButton - kChoiceButtonShadow;
    break;
  }
  case ChoiceButtonStacking::kRowColumn: {
    const int perSide = std::max(1,
        static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    const int perButton = totalHeight / perSide;
    available = perButton - kChoiceButtonShadow;
    break;
  }
  case ChoiceButtonStacking::kColumn:
  default:
    available = totalHeight - kChoiceButtonShadow;
    break;
  }

  return std::max(1, available);
}

QFont medmChoiceButtonFont(int widgetHeight, int buttonCount,
    ChoiceButtonStacking stacking, int heightLimit)
{
  const int clippingHeight = std::max(1, heightLimit);
  const int maxHeight = std::min(availableFontHeight(widgetHeight, buttonCount,
                                   stacking),
      clippingHeight);

  const auto &aliases = choiceButtonFontAliases();
  QFont fallback;
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

/* Map a logical grid position to an item index. Row-column stacking places
 * labels in column-major order to match the legacy MEDM layout. */
int indexForGridCell(int row, int column, int rows, int columns, int itemCount,
    ChoiceButtonStacking stacking)
{
  if (row < 0 || column < 0 || rows <= 0 || columns <= 0 || itemCount <= 0) {
    return -1;
  }
  if (row >= rows || column >= columns) {
    return -1;
  }

  if (stacking == ChoiceButtonStacking::kRowColumn) {
    const int fullColumns = itemCount / rows;
    const int remainder = itemCount % rows;
    if (column < fullColumns) {
      return column * rows + row;
    }
    if (column == fullColumns && remainder > 0 && row < remainder) {
      return fullColumns * rows + row;
    }
    return -1;
  }

  const int index = row * columns + column;
  return index < itemCount ? index : -1;
}

QFont shrinkFontToFit(const QString &text, const QRect &bounds, QFont font)
{
  if (font.pointSizeF() <= 0.0) {
    font.setPointSize(10);
  }

  if (bounds.width() <= 0 || bounds.height() <= 0) {
    return font;
  }

  QFontMetrics metrics(font);
  while (!text.isEmpty()
      && (metrics.boundingRect(text).width() > bounds.width()
          || metrics.height() > bounds.height())
      && font.pointSize() > 4) {
    font.setPointSize(font.pointSize() - 1);
    metrics = QFontMetrics(font);
  }

  return font;
}

QColor blendedColor(const QColor &base, const QColor &overlay, double factor)
{
  if (!base.isValid()) {
    return overlay;
  }
  if (!overlay.isValid()) {
    return base;
  }
  factor = std::clamp(factor, 0.0, 1.0);
  const double inverse = 1.0 - factor;
  const int red = static_cast<int>(std::round(base.red() * inverse + overlay.red() * factor));
  const int green = static_cast<int>(std::round(base.green() * inverse + overlay.green() * factor));
  const int blue = static_cast<int>(std::round(base.blue() * inverse + overlay.blue() * factor));
  return QColor(red, green, blue);
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

QColor lightenForBevel(const QColor &base, double factor)
{
  return blendedColor(base, QColor(Qt::white), factor);
}

QColor darkenForBevel(const QColor &base, double factor)
{
  return blendedColor(base, QColor(Qt::black), factor);
}

void paintChoiceButton(QPainter &painter, const QRect &bounds, bool checked,
    bool enabled, const QColor &textColor, const QColor &backgroundColor,
    const QString &text, const QFont &font)
{
  if (!bounds.isValid()) {
    return;
  }

  const QColor face =
      backgroundColor.isValid() ? backgroundColor : QColor(Qt::white);

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(face);
  painter.fillRect(bounds, face);

  //QRect outer = bounds.adjusted(0, 0, -1, -1);
  QRect outer = bounds.adjusted(0, 0, 0, 0);
  if (!outer.isValid()) {
    outer = bounds;
  }

  const QRect inner = outer.adjusted(1, 1, -1, -1);
  //const QRect inner = outer.adjusted(0, 0, 0, 0);

  const QColor highlightOuter = lightenForBevel(face, 0.6);
  const QColor highlightInner = lightenForBevel(face, 0.35);
  const QColor shadowOuter = darkenForBevel(face, 0.55);
  const QColor shadowInner = darkenForBevel(face, 0.3);

  const QColor topOuter = checked ? shadowOuter : highlightOuter;
  const QColor topInner = checked ? shadowInner : highlightInner;
  const QColor bottomOuter = checked ? highlightOuter : shadowOuter;
  const QColor bottomInner = checked ? highlightInner : shadowInner;

  painter.setBrush(Qt::NoBrush);

  if (outer.isValid()) {
    painter.setPen(topOuter);
    painter.drawLine(outer.topLeft(), outer.topRight());
    painter.drawLine(outer.topLeft(), outer.bottomLeft());

    painter.setPen(bottomOuter);
    painter.drawLine(outer.bottomLeft(), outer.bottomRight());
    painter.drawLine(outer.topRight(), outer.bottomRight());
  }

  if (inner.isValid()) {
    painter.setPen(topInner);
    painter.drawLine(inner.topLeft(), inner.topRight());
    painter.drawLine(inner.topLeft(), inner.bottomLeft());

    painter.setPen(bottomInner);
    painter.drawLine(inner.bottomLeft(), inner.bottomRight());
    painter.drawLine(inner.topRight(), inner.bottomRight());
  }

  QRect textArea = bounds.adjusted(3, 2, -3, -2);
  if (!textArea.isValid()) {
    textArea = bounds;
  }

  painter.setFont(font);
  QColor penColor =
      textColor.isValid() ? textColor : QColor(Qt::black);
  if (!enabled) {
    penColor = blendedColor(penColor, face, 0.5);
  }
  painter.setPen(penColor);
  painter.drawText(textArea, Qt::AlignCenter | Qt::TextDontClip, text);
  painter.restore();
}

class ChoiceButtonCell : public QAbstractButton
{
public:
  explicit ChoiceButtonCell(QWidget *parent = nullptr)
    : QAbstractButton(parent)
  {
    setCheckable(true);
    setAutoExclusive(false);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    foreground_ = palette().color(QPalette::ButtonText);
    background_ = palette().color(QPalette::Button);
  }

  void setColors(const QColor &foreground, const QColor &background)
  {
    QColor effectiveForeground = foreground.isValid()
        ? foreground
        : palette().color(QPalette::ButtonText);
    QColor effectiveBackground = background.isValid()
        ? background
        : palette().color(QPalette::Button);

    if (foreground_ == effectiveForeground
        && background_ == effectiveBackground) {
      return;
    }
    foreground_ = effectiveForeground;
    background_ = effectiveBackground;
    update();
  }

protected:
  void paintEvent(QPaintEvent *event) override
  {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    paintChoiceButton(painter, rect(), isChecked(), isEnabled(),
        foreground_, background_, text(), font());
  }

private:
  QColor foreground_;
  QColor background_;
};

} // namespace

ChoiceButtonElement::ChoiceButtonElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
}

void ChoiceButtonElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool ChoiceButtonElement::isSelected() const
{
  return selected_;
}

QColor ChoiceButtonElement::foregroundColor() const
{
  return foregroundColor_;
}

void ChoiceButtonElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::WindowText);
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  if (executeMode_) {
    updateButtonPalettes();
  } else {
    update();
  }
}

QColor ChoiceButtonElement::backgroundColor() const
{
  return backgroundColor_;
}

void ChoiceButtonElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::Window);
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  if (executeMode_) {
    updateButtonPalettes();
    layoutButtons();
  }
  update();
}

TextColorMode ChoiceButtonElement::colorMode() const
{
  return colorMode_;
}

void ChoiceButtonElement::setColorMode(TextColorMode mode)
{
  if (colorMode_ == mode) {
    return;
  }
  colorMode_ = mode;
  if (executeMode_) {
    updateButtonPalettes();
  }
  update();
}

ChoiceButtonStacking ChoiceButtonElement::stacking() const
{
  return stacking_;
}

void ChoiceButtonElement::setStacking(ChoiceButtonStacking stacking)
{
  if (stacking_ == stacking) {
    return;
  }
  stacking_ = stacking;
  layoutButtons();
  update();
}

QString ChoiceButtonElement::channel() const
{
  return channel_;
}

void ChoiceButtonElement::setChannel(const QString &channel)
{
  if (channel_ == channel) {
    return;
  }
  channel_ = channel;
  for (QAbstractButton *button : buttons_) {
    if (button) {
      button->setToolTip(channel_);
    }
  }
  update();
}

void ChoiceButtonElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  if (executeMode_) {
    ensureButtonGroup();
    rebuildButtons();
    layoutButtons();
    updateButtonPalettes();
    updateButtonEnabledState();
  } else {
    clearButtons();
    runtimeConnected_ = false;
    runtimeWriteAccess_ = false;
    runtimeSeverity_ = 0;
    runtimeValue_ = -1;
  }
  update();
}

bool ChoiceButtonElement::isExecuteMode() const
{
  return executeMode_;
}

void ChoiceButtonElement::setRuntimeConnected(bool connected)
{
  if (runtimeConnected_ == connected) {
    return;
  }
  runtimeConnected_ = connected;
  if (!runtimeConnected_) {
    runtimeWriteAccess_ = false;
  }
  updateButtonEnabledState();
  updateButtonPalettes();
  update();
}

void ChoiceButtonElement::setRuntimeSeverity(short severity)
{
  short clamped = severity;
  if (clamped < 0) {
    clamped = 0;
  }
  if (clamped > 3) {
    clamped = 3;
  }
  if (runtimeSeverity_ == clamped) {
    return;
  }
  runtimeSeverity_ = clamped;
  if (executeMode_) {
    updateButtonPalettes();
  }
  update();
}

void ChoiceButtonElement::setRuntimeWriteAccess(bool writeAccess)
{
  if (runtimeWriteAccess_ == writeAccess) {
    return;
  }
  runtimeWriteAccess_ = writeAccess;
  updateButtonEnabledState();
}

void ChoiceButtonElement::setRuntimeLabels(const QStringList &labels)
{
  if (runtimeLabels_ == labels) {
    return;
  }
  runtimeLabels_ = labels;
  if (executeMode_) {
    rebuildButtons();
    layoutButtons();
    updateButtonPalettes();
    updateButtonEnabledState();
    setRuntimeValue(runtimeValue_);
  }
  update();
}

void ChoiceButtonElement::setRuntimeValue(int value)
{
  runtimeValue_ = value;
  if (!executeMode_ || !buttonGroup_) {
    update();
    return;
  }

  QSignalBlocker blocker(buttonGroup_);
  bool matched = false;
  if (value >= 0) {
    if (QAbstractButton *button = buttonGroup_->button(value)) {
      button->setChecked(true);
      matched = true;
    }
  }
  if (!matched) {
    for (QAbstractButton *candidate : buttons_) {
      if (candidate) {
        candidate->setChecked(false);
      }
    }
  }
  update();
}

void ChoiceButtonElement::setActivationCallback(
    const std::function<void(int)> &callback)
{
  activationCallback_ = callback;
}

QColor ChoiceButtonElement::effectiveForeground() const
{
  if (executeMode_) {
    if (!runtimeConnected_) {
      return QColor(204, 204, 204);
    }
    switch (colorMode_) {
    case TextColorMode::kAlarm:
      return alarmColorForSeverity(runtimeSeverity_);
    case TextColorMode::kDiscrete:
    case TextColorMode::kStatic:
    default:
      break;
    }
  }
  return foregroundColor_.isValid() ? foregroundColor_
                                    : palette().color(QPalette::WindowText);
}

QColor ChoiceButtonElement::effectiveBackground() const
{
  if (executeMode_ && !runtimeConnected_) {
    return QColor(Qt::white);
  }
  return backgroundColor_.isValid() ? backgroundColor_
                                    : palette().color(QPalette::Window);
}

void ChoiceButtonElement::paintSelectionOverlay(QPainter &painter) const
{
  if (!selected_) {
    return;
  }
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void ChoiceButtonElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QRect canvas = rect();
  painter.fillRect(canvas, effectiveBackground());

  if (canvas.width() <= 2 || canvas.height() <= 2) {
    paintSelectionOverlay(painter);
    return;
  }

  if (!executeMode_ || buttons_.isEmpty()) {
    int rows = 1;
    int columns = kSampleButtonCount;
    switch (stacking_) {
    case ChoiceButtonStacking::kRow:
      rows = kSampleButtonCount;
      columns = 1;
      break;
    case ChoiceButtonStacking::kRowColumn: {
      const int base = static_cast<int>(std::ceil(std::sqrt(kSampleButtonCount)));
      columns = std::max(1, base);
      rows = static_cast<int>(std::ceil(static_cast<double>(kSampleButtonCount) / columns));
      break;
    }
    case ChoiceButtonStacking::kColumn:
    default:
      rows = 1;
      columns = kSampleButtonCount;
      break;
    }

    rows = std::max(1, rows);
    columns = std::max(1, columns);

    const QRect content = canvas.adjusted(0, 0, 0, 0);
    const int cellWidth = content.width() / columns;
    const int cellHeight = content.height() / rows;
    const int extraWidth = content.width() - cellWidth * columns;
    const int extraHeight = content.height() - cellHeight * rows;

    QColor foreground = effectiveForeground();
    QColor background = effectiveBackground();

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::NoBrush);

    int drawnCount = 0;
    int y = content.top();
    bool done = false;
    for (int row = 0; row < rows; ++row) {
      int rowHeight = cellHeight;
      if (row < extraHeight) {
        rowHeight += 1;
      }
      int x = content.left();
      for (int column = 0; column < columns; ++column) {
        int columnWidth = cellWidth;
        if (column < extraWidth) {
          columnWidth += 1;
        }
        QRect buttonRect(x, y, columnWidth, rowHeight);
        QRect interior = buttonRect.adjusted(kButtonMargin, kButtonMargin,
            -kButtonMargin, -kButtonMargin);
        if (interior.width() <= 0 || interior.height() <= 0) {
          interior = buttonRect.adjusted(1, 1, -1, -1);
        }

        const int sampleIndex = indexForGridCell(row, column, rows, columns,
            kSampleButtonCount, stacking_);
        if (sampleIndex < 0 || sampleIndex >= kSampleButtonCount) {
          x += columnWidth;
          continue;
        }

        QString label = QStringLiteral("%1...").arg(sampleIndex);
        const QRect textBounds = interior.adjusted(3, 2, -3, -2);
        QFont labelFont = medmChoiceButtonFont(height(), kSampleButtonCount,
            stacking_, std::max(1, textBounds.height()));
        if (labelFont.family().isEmpty()) {
          labelFont = shrinkFontToFit(label, textBounds, font());
        }
        paintChoiceButton(painter, interior,
            sampleIndex == 0, true, foreground, background, label, labelFont);

        x += columnWidth;
        ++drawnCount;
        if (drawnCount >= kSampleButtonCount) {
          done = true;
          break;
        }
      }
      if (done) {
        break;
      }
      y += rowHeight;
    }
  }

  paintSelectionOverlay(painter);
}

void ChoiceButtonElement::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  layoutButtons();
}

void ChoiceButtonElement::ensureButtonGroup()
{
  if (buttonGroup_) {
    return;
  }
  buttonGroup_ = new QButtonGroup(this);
  buttonGroup_->setExclusive(true);
  QObject::connect(buttonGroup_, &QButtonGroup::idClicked,
      this, [this](int id) {
        if (QAbstractButton *button = buttonGroup_->button(id)) {
          if (!runtimeWriteAccess_) {
            if (runtimeConnected_) {
              QApplication::beep();
            }
            QSignalBlocker blocker(buttonGroup_);
            if (runtimeValue_ >= 0) {
              if (QAbstractButton *current = buttonGroup_->button(runtimeValue_)) {
                current->setChecked(true);
              } else {
                button->setChecked(false);
              }
            } else {
              button->setChecked(false);
            }
            return;
          }
        }
        if (!activationCallback_) {
          return;
        }
        if (QAbstractButton *button = buttonGroup_->button(id)) {
          if (button->isChecked()) {
            activationCallback_(id);
          }
        }
      });
}

void ChoiceButtonElement::clearButtons()
{
  if (buttonGroup_) {
    const auto groupButtons = buttonGroup_->buttons();
    for (QAbstractButton *button : groupButtons) {
      buttonGroup_->removeButton(button);
    }
  }
  for (QAbstractButton *button : buttons_) {
    if (button) {
      button->hide();
      delete button;
    }
  }
  buttons_.clear();
}

void ChoiceButtonElement::rebuildButtons()
{
  if (!executeMode_) {
    clearButtons();
    return;
  }

  ensureButtonGroup();
  clearButtons();

  const int buttonCount = runtimeLabels_.size();
  if (buttonCount <= 0) {
    return;
  }

  buttons_.reserve(buttonCount);
  for (int i = 0; i < buttonCount; ++i) {
    auto *button = new ChoiceButtonCell(this);
    button->setCheckable(true);
    button->setText(runtimeLabels_.value(i).trimmed());
    button->setToolTip(channel_);
    button->setAutoExclusive(false);
    button->setCursor(runtimeWriteAccess_ ? CursorUtils::arrowCursor()
                                         : CursorUtils::forbiddenCursor());
    button->installEventFilter(this);
    buttonGroup_->addButton(button, i);
    button->show();
    buttons_.append(button);
  }

  QSignalBlocker blocker(buttonGroup_);
  if (runtimeValue_ >= 0) {
    if (QAbstractButton *button = buttonGroup_->button(runtimeValue_)) {
      button->setChecked(true);
    }
  }
}

void ChoiceButtonElement::layoutButtons()
{
  if (!executeMode_ || buttons_.isEmpty()) {
    return;
  }

  const int buttonCount = buttons_.size();
  int rows = 1;
  int columns = buttonCount;
  switch (stacking_) {
  case ChoiceButtonStacking::kRow:
    rows = buttonCount;
    columns = 1;
    break;
  case ChoiceButtonStacking::kRowColumn: {
    const int base = static_cast<int>(std::ceil(std::sqrt(buttonCount)));
    columns = std::max(1, base);
    rows = static_cast<int>(std::ceil(static_cast<double>(buttonCount) / columns));
    break;
  }
  case ChoiceButtonStacking::kColumn:
  default:
    rows = 1;
    columns = buttonCount;
    break;
  }

  rows = std::max(1, rows);
  columns = std::max(1, columns);

  const QRect content = rect().adjusted(0, 0, 0, 0);
  const int cellWidth = columns > 0 ? content.width() / columns : content.width();
  const int cellHeight = rows > 0 ? content.height() / rows : content.height();
  const int extraWidth = content.width() - cellWidth * columns;
  const int extraHeight = content.height() - cellHeight * rows;

  int positionedCount = 0;
  bool done = false;
  int y = content.top();
  for (int row = 0; row < rows; ++row) {
    int rowHeight = cellHeight;
    if (row < extraHeight) {
      rowHeight += 1;
    }
    int x = content.left();
    for (int column = 0; column < columns; ++column) {
      int columnWidth = cellWidth;
      if (column < extraWidth) {
        columnWidth += 1;
      }
      QRect buttonRect(x, y, columnWidth, rowHeight);
      QRect interior = buttonRect.adjusted(kButtonMargin, kButtonMargin,
          -kButtonMargin, -kButtonMargin);
      if (interior.width() <= 0 || interior.height() <= 0) {
        interior = buttonRect.adjusted(1, 1, -1, -1);
      }

      const int index = indexForGridCell(row, column, rows, columns,
          buttonCount, stacking_);
      if (index >= 0 && index < buttonCount) {
        if (QAbstractButton *button = buttons_.value(index)) {
          button->setGeometry(interior);
          applyButtonFont(button, interior);
        }
        ++positionedCount;
        if (positionedCount >= buttonCount) {
          done = true;
        }
      }

      x += columnWidth;
      if (done) {
        break;
      }
    }
    if (done) {
      break;
    }
    y += rowHeight;
  }
}

void ChoiceButtonElement::updateButtonPalettes()
{
  if (!executeMode_) {
    update();
    return;
  }

  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();
  for (QAbstractButton *button : buttons_) {
    if (!button) {
      continue;
    }
    if (auto *cell = dynamic_cast<ChoiceButtonCell *>(button)) {
      cell->setColors(fg, bg);
    } else {
      QPalette pal = button->palette();
      pal.setColor(QPalette::ButtonText, fg);
      pal.setColor(QPalette::WindowText, fg);
      pal.setColor(QPalette::Text, fg);
      pal.setColor(QPalette::Button, bg);
      pal.setColor(QPalette::Base, bg);
      pal.setColor(QPalette::Window, bg);
      button->setPalette(pal);
    }
    button->update();
  }
  update();
}

void ChoiceButtonElement::updateButtonEnabledState()
{
  const bool enabled = runtimeConnected_;
  const QCursor &cursor = runtimeWriteAccess_ ? CursorUtils::arrowCursor()
                                              : CursorUtils::forbiddenCursor();
  for (QAbstractButton *button : buttons_) {
    if (!button) {
      continue;
    }
    button->setEnabled(enabled);
    button->setCursor(cursor);
  }
}

void ChoiceButtonElement::applyButtonFont(QAbstractButton *button,
    const QRect &bounds) const
{
  if (!button) {
    return;
  }

  const QString label = button->text();
  const QRect textBounds = bounds.adjusted(3, 2, -3, -2);
  QFont buttonFont = medmChoiceButtonFont(height(), buttons_.size(),
      stacking_, std::max(1, textBounds.height()));
  if (buttonFont.family().isEmpty()) {
    buttonFont = shrinkFontToFit(label, textBounds, font());
  }
  button->setFont(buttonFont);
}

bool ChoiceButtonElement::eventFilter(QObject *watched, QEvent *event)
{
  if (executeMode_ && event && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::MiddleButton || mouseEvent->button() == Qt::RightButton) {
      if (forwardMouseEventToParent(mouseEvent)) {
        return true;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

bool ChoiceButtonElement::forwardMouseEventToParent(QMouseEvent *event) const
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
