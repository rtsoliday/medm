#include "related_display_element.h"

#include <algorithm>

#include <QBrush>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QHash>

#include "medm_colors.h"
#include "text_font_utils.h"

namespace {

QString sanitizedLabel(const QString &value, bool &showIcon)
{
  showIcon = true;
  QString trimmed = value;
  if (trimmed.startsWith(QLatin1Char('-'))) {
    showIcon = false;
    trimmed.remove(0, 1);
  }
  return trimmed.trimmed();
}

QString entryDisplayLabel(const RelatedDisplayEntry &entry)
{
  bool dummy = true;
  QString result = sanitizedLabel(entry.label, dummy);
  if (!result.isEmpty()) {
    return result;
  }
  QString name = entry.name.trimmed();
  if (!name.isEmpty()) {
    return name;
  }
  return QString();
}

int messageButtonPixelLimit(int height)
{
  if (height <= 0) {
    return 1;
  }
  const double scaled = 0.90 * static_cast<double>(height);
  int limit = static_cast<int>(scaled) - 4;
  return std::max(1, limit);
}

int relatedDisplayPixelLimit(RelatedDisplayVisual visual, int height,
    int numButtons)
{
  if (height <= 0) {
    return 1;
  }

  constexpr int kShadowSize = 4;

  switch (visual) {
  case RelatedDisplayVisual::kColumnOfButtons: {
    const int buttons = std::max(1, numButtons);
    int limit = height / buttons - kShadowSize;
    return std::max(1, limit);
  }
  case RelatedDisplayVisual::kRowOfButtons: {
    int limit = height - kShadowSize;
    return std::max(1, limit);
  }
  default:
    break;
  }

  return messageButtonPixelLimit(height);
}

QPixmap createRelatedDisplayIcon(const QSize &size, const QColor &color)
{
  if (size.width() <= 0 || size.height() <= 0) {
    return QPixmap();
  }
  
  /* Create icon at exact size with layered rectangles using single-pixel lines */
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);
  
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QPen(color, 1));
  painter.setBrush(Qt::NoBrush);
  
  /* Calculate rectangle positions proportional to the icon size.
   * Draw second rectangle as line segments to show it's partially hidden.
   */
  
  const int w = size.width();
  const int h = size.height();
  
  /* First rectangle - back layer (complete outline) */
  const int x1 = w * 3 / 25;
  const int y1 = h * 3 / 25;
  const int w1 = w * 14 / 25;
  const int h1 = h * 12 / 25;
  painter.drawRect(x1, y1, w1, h1);
  
  /* Second rectangle - front layer (visible segments only)
   * Draw as 4 line segments instead of a complete rectangle to show
   * it's partially obscured by the first rectangle */
  const int x2 = w * 8 / 25;
  const int y2 = h * 7 / 25;
  const int w2 = w * 14 / 25;
  const int h2 = h * 13 / 25;
  
  /* Top edge of second rectangle */
  painter.drawLine(x2 + (w2 - (x2 - x1)), y2, x2 + w2, y2);
  
  /* Right edge of second rectangle */
  painter.drawLine(x2 + w2, y2, x2 + w2, y2 + h2);
  
  /* Bottom edge of second rectangle */
  painter.drawLine(x2 + w2, y2 + h2, x2, y2 + h2);
  
  /* Left edge of second rectangle */
  painter.drawLine(x2, y2 + h2, x2, y2 + (h2 - (y2 - y1)));
  
  painter.end();
  return pixmap;
}

} // namespace

RelatedDisplayElement::RelatedDisplayElement(QWidget *parent)
  : QWidget(parent)
{
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setAutoFillBackground(false);
  foregroundColor_ = palette().color(QPalette::WindowText);
  backgroundColor_ = palette().color(QPalette::Window);
}

void RelatedDisplayElement::setSelected(bool selected)
{
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  update();
}

bool RelatedDisplayElement::isSelected() const
{
  return selected_;
}

QColor RelatedDisplayElement::foregroundColor() const
{
  return foregroundColor_;
}

void RelatedDisplayElement::setForegroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::WindowText);
  if (foregroundColor_ == effective) {
    return;
  }
  foregroundColor_ = effective;
  update();
}

QColor RelatedDisplayElement::backgroundColor() const
{
  return backgroundColor_;
}

void RelatedDisplayElement::setBackgroundColor(const QColor &color)
{
  QColor effective = color.isValid() ? color : palette().color(QPalette::Window);
  if (backgroundColor_ == effective) {
    return;
  }
  backgroundColor_ = effective;
  update();
}

QString RelatedDisplayElement::label() const
{
  return label_;
}

void RelatedDisplayElement::setLabel(const QString &label)
{
  if (label_ == label) {
    return;
  }
  label_ = label;
  update();
}

RelatedDisplayVisual RelatedDisplayElement::visual() const
{
  return visual_;
}

void RelatedDisplayElement::setVisual(RelatedDisplayVisual visual)
{
  if (visual_ == visual) {
    return;
  }
  visual_ = visual;
  
  /* Update widget attributes based on execute mode and visual type */
  const bool shouldBeTransparent =
      visual_ == RelatedDisplayVisual::kHiddenButton && executeMode_;
  setAttribute(Qt::WA_OpaquePaintEvent, !shouldBeTransparent);
  
  update();
}

int RelatedDisplayElement::entryCount() const
{
  return static_cast<int>(entries_.size());
}

RelatedDisplayEntry RelatedDisplayElement::entry(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return RelatedDisplayEntry{};
  }
  return entries_[index];
}

void RelatedDisplayElement::setEntry(int index, const RelatedDisplayEntry &entry)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  entries_[index] = entry;
  update();
}

QString RelatedDisplayElement::entryLabel(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return QString();
  }
  return entries_[index].label;
}

void RelatedDisplayElement::setEntryLabel(int index, const QString &label)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].label == label) {
    return;
  }
  entries_[index].label = label;
  update();
}

QString RelatedDisplayElement::entryName(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return QString();
  }
  return entries_[index].name;
}

void RelatedDisplayElement::setEntryName(int index, const QString &name)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].name == name) {
    return;
  }
  entries_[index].name = name;
  update();
}

QString RelatedDisplayElement::entryArgs(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return QString();
  }
  return entries_[index].args;
}

void RelatedDisplayElement::setEntryArgs(int index, const QString &args)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].args == args) {
    return;
  }
  entries_[index].args = args;
  update();
}

RelatedDisplayMode RelatedDisplayElement::entryMode(int index) const
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return RelatedDisplayMode::kAdd;
  }
  return entries_[index].mode;
}

void RelatedDisplayElement::setEntryMode(int index, RelatedDisplayMode mode)
{
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[index].mode == mode) {
    return;
  }
  entries_[index].mode = mode;
  update();
}

void RelatedDisplayElement::setExecuteMode(bool execute)
{
  if (executeMode_ == execute) {
    return;
  }
  executeMode_ = execute;
  pressedEntryIndex_ = -1;
  
  /* Update widget attributes based on execute mode and visual type */
  const bool shouldBeTransparent =
      visual_ == RelatedDisplayVisual::kHiddenButton && executeMode_;
  setAttribute(Qt::WA_OpaquePaintEvent, !shouldBeTransparent);
  
  update();
}

bool RelatedDisplayElement::isExecuteMode() const
{
  return executeMode_;
}

void RelatedDisplayElement::setActivationCallback(
    const std::function<void(int, Qt::KeyboardModifiers)> &callback)
{
  activationCallback_ = callback;
}

void RelatedDisplayElement::paintEvent(QPaintEvent *event)
{
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);

  const QRect canvas = rect();

  const bool suppressHiddenVisual =
      visual_ == RelatedDisplayVisual::kHiddenButton && executeMode_;

  if (!suppressHiddenVisual) {
    painter.fillRect(canvas, effectiveBackground());
    QRect content;
    switch (visual_) {
    case RelatedDisplayVisual::kRowOfButtons:
    case RelatedDisplayVisual::kColumnOfButtons:
      /* For button visuals, use the full canvas with no margins to match MEDM */
      content = canvas;
      break;
    default:
      content = canvas.adjusted(1, 1, -1, -1);
      break;
    }
    switch (visual_) {
    case RelatedDisplayVisual::kRowOfButtons:
      paintButtonVisual(painter, content, false);
      break;
    case RelatedDisplayVisual::kColumnOfButtons:
      paintButtonVisual(painter, content, true);
      break;
    case RelatedDisplayVisual::kHiddenButton:
      paintHiddenVisual(painter, content);
      break;
    case RelatedDisplayVisual::kMenu:
    default:
      paintMenuVisual(painter, content);
      break;
    }
  }

  if (selected_) {
    paintSelectionOverlay(painter);
  }
}

QColor RelatedDisplayElement::effectiveForeground() const
{
  return foregroundColor_.isValid() ? foregroundColor_
                                    : palette().color(QPalette::WindowText);
}

QColor RelatedDisplayElement::effectiveBackground() const
{
  return backgroundColor_.isValid() ? backgroundColor_
                                    : palette().color(QPalette::Window);
}

QString RelatedDisplayElement::displayLabel(bool &showIcon) const
{
  QString base = sanitizedLabel(label_, showIcon);
  if (!base.isEmpty()) {
    return base;
  }
  for (const auto &entry : entries_) {
    QString candidate = entryDisplayLabel(entry);
    if (!candidate.isEmpty()) {
      return candidate;
    }
  }
  showIcon = true;
  return QStringLiteral("Related Display");
}

int RelatedDisplayElement::activeEntryCount() const
{
  int count = 0;
  for (const auto &entry : entries_) {
    /* Match MEDM behavior: only count entries with non-empty labels.
     * Entries with just a name but no label are not shown as buttons. */
    if (!entry.label.trimmed().isEmpty()) {
      ++count;
    }
  }
  return count;
}

void RelatedDisplayElement::paintMenuVisual(QPainter &painter,
    const QRect &content) const
{
  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();

  /* Compute Motif-style shadow colors for proper bevel visibility
   * even with very dark backgrounds like black */
  QColor topShadow, bottomShadow;
  MedmColors::computeShadowColors(bg, topShadow, bottomShadow);

  painter.save();

  QRect bevelOuter = content.adjusted(-1, -1, 1, 1);
  painter.setPen(Qt::NoPen);
  painter.setBrush(bg);
  painter.drawRect(bevelOuter);

  painter.setPen(QPen(topShadow, 1));
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.topRight());
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.bottomLeft());
  painter.setPen(QPen(bottomShadow, 1));
  painter.drawLine(bevelOuter.bottomLeft(), bevelOuter.bottomRight());
  painter.drawLine(bevelOuter.topRight(), bevelOuter.bottomRight());

  QRect bevelInner = content;
  painter.setPen(QPen(topShadow.lighter(110), 1));
  painter.drawLine(bevelInner.topLeft(), bevelInner.topRight());
  painter.drawLine(bevelInner.topLeft(), bevelInner.bottomLeft());
  painter.setPen(QPen(bottomShadow.darker(115), 1));
  painter.drawLine(bevelInner.bottomLeft(), bevelInner.bottomRight());
  painter.drawLine(bevelInner.topRight(), bevelInner.bottomRight());

  bool showIcon = true;
  QString labelText = displayLabel(showIcon);
  if (visual_ == RelatedDisplayVisual::kMenu && executeMode_
      && label_.trimmed().isEmpty()) {
    labelText.clear();
  }

  // Calculate constraint using (0.90 * height) - 4, matching MEDM's messageButtonFontListIndex
  // Search from largest to smallest font, return first that fits
  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = medmMessageButtonFont(fontLimit);
  painter.setFont(labelFont);

  QRect inner = content.adjusted(2, 2, -2, -2);

  QRect iconRect = inner;
  if (showIcon) {
    int iconSize = std::min({inner.height(), inner.width(), 24});
    iconRect.setWidth(iconSize - 2);
    iconRect.setHeight(iconSize);
    iconRect.moveTop(inner.top() + (inner.height() - iconSize) / 2);
    //iconRect.moveLeft(inner.left() + 0);

    QRect iconCanvas = iconRect.adjusted(0, 0, 0, 0);
    painter.fillRect(iconCanvas, bg);

    /* Create icon at exact size needed - no scaling required */
    const QPixmap iconPixmap = createRelatedDisplayIcon(iconCanvas.size(), fg);
    if (!iconPixmap.isNull()) {
      QPoint topLeft(iconCanvas.left()
              + (iconCanvas.width() - iconPixmap.width()) / 2,
          iconCanvas.top()
              + (iconCanvas.height() - iconPixmap.height()) / 2);
      painter.drawPixmap(topLeft, iconPixmap);
    } else {
      const int barHeight = std::max(2, iconCanvas.height() / 3);
      QRect fallbackRect(iconCanvas.left(), iconCanvas.top(),
          iconCanvas.width(), barHeight);
      painter.fillRect(fallbackRect, fg);
    }
  }

  const int activeLabels = activeEntryCount();
  QRect textRect = inner.adjusted(4, 0, 0, 0);
  if (showIcon && iconRect.width() > 0) {
    int textLeft = iconRect.right() + 3;
    if (textLeft < textRect.right()) {
      textRect.setLeft(textLeft);
    }
  }
  if (!labelText.isEmpty()) {
    painter.setPen(fg);
    Qt::Alignment align = Qt::AlignCenter;
    if (visual_ == RelatedDisplayVisual::kMenu && activeLabels > 1) {
      align = Qt::AlignVCenter | Qt::AlignLeft;
    }
    painter.drawText(textRect, align, labelText);
  }

  painter.restore();
}

void RelatedDisplayElement::paintButtonVisual(QPainter &painter,
    const QRect &content, bool vertical) const
{
  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();

  /* Compute Motif-style shadow colors for proper bevel visibility
   * even with very dark backgrounds like black */
  QColor topShadow, bottomShadow;
  MedmColors::computeShadowColors(bg, topShadow, bottomShadow);

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(bg);
  painter.drawRect(content);

  const int activeCount = activeEntryCount();
  const int displayCount = activeCount > 0 ? activeCount : 2;
  const int columns = vertical ? 1 : std::max(1, displayCount);
  const int rows = vertical ? std::max(1, displayCount) : 1;

  const int buttonWidth = columns > 0 ? content.width() / columns : content.width();
  const int buttonHeight = rows > 0 ? content.height() / rows : content.height();

  // Calculate constraint and use MEDM font table, matching messageButtonFontListIndex
  // Search from largest to smallest font, return first that fits
  const int fontLimit = relatedDisplayPixelLimit(visual_, height(), displayCount);
  const QFont labelFont = medmMessageButtonFont(fontLimit);
  painter.setFont(labelFont);

  int index = 0;
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      if (index >= displayCount) {
        break;
      }
      /* Calculate button rect to fill the full cell with no gaps.
       * MEDM uses XmNmarginWidth=0, XmNmarginHeight=0, XmNspacing=0
       * and XmNshadowThickness=2 for each button. */
      QRect buttonRect(content.left() + column * buttonWidth,
          content.top() + row * buttonHeight,
          buttonWidth, buttonHeight);

      /* Draw outer bevel (shadow thickness = 2 pixels total) */
      painter.setPen(QPen(topShadow, 1));
      painter.drawLine(buttonRect.topLeft(), buttonRect.topRight() - QPoint(1, 0));
      painter.drawLine(buttonRect.topLeft(), buttonRect.bottomLeft() - QPoint(0, 1));
      painter.setPen(QPen(bottomShadow, 1));
      painter.drawLine(buttonRect.bottomLeft(), buttonRect.bottomRight() - QPoint(1, 0));
      painter.drawLine(buttonRect.topRight(), buttonRect.bottomRight() - QPoint(0, 1));

      /* Draw inner bevel */
      QRect innerBevel = buttonRect.adjusted(1, 1, -1, -1);
      painter.setPen(QPen(topShadow.lighter(110), 1));
      painter.drawLine(innerBevel.topLeft(), innerBevel.topRight() - QPoint(1, 0));
      painter.drawLine(innerBevel.topLeft(), innerBevel.bottomLeft() - QPoint(0, 1));
      painter.setPen(QPen(bottomShadow.darker(115), 1));
      painter.drawLine(innerBevel.bottomLeft(), innerBevel.bottomRight() - QPoint(1, 0));
      painter.drawLine(innerBevel.topRight(), innerBevel.bottomRight() - QPoint(0, 1));

      /* Fill button interior */
      QRect interior = buttonRect.adjusted(2, 2, -2, -2);
      painter.fillRect(interior, bg);

      /* Draw text */
      painter.setPen(fg);
      QString text;
      if (index < activeCount) {
        text = entryDisplayLabel(entries_[index]);
      }
      if (text.isEmpty()) {
        text = QStringLiteral("Display %1").arg(index + 1);
      }
      painter.drawText(interior, Qt::AlignCenter, text);
      ++index;
    }
  }

  painter.restore();
}

void RelatedDisplayElement::paintHiddenVisual(QPainter &painter,
    const QRect &content) const
{
  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();

  /* Compute Motif-style shadow colors for proper bevel visibility
   * even with very dark backgrounds like black */
  QColor topShadow, bottomShadow;
  MedmColors::computeShadowColors(bg, topShadow, bottomShadow);

  painter.save();
  QRect hiddenOuter = content.adjusted(-1, -1, 1, 1);
  painter.setPen(QPen(topShadow, 1));
  painter.drawLine(hiddenOuter.topLeft(), hiddenOuter.topRight());
  painter.drawLine(hiddenOuter.topLeft(), hiddenOuter.bottomLeft());
  painter.setPen(QPen(bottomShadow, 1));
  painter.drawLine(hiddenOuter.bottomLeft(), hiddenOuter.bottomRight());
  painter.drawLine(hiddenOuter.topRight(), hiddenOuter.bottomRight());

  QRect hiddenInner = content;
  painter.setPen(QPen(topShadow.lighter(110), 1));
  painter.drawLine(hiddenInner.topLeft(), hiddenInner.topRight());
  painter.drawLine(hiddenInner.topLeft(), hiddenInner.bottomLeft());
  painter.setPen(QPen(bottomShadow.darker(115), 1));
  painter.drawLine(hiddenInner.bottomLeft(), hiddenInner.bottomRight());
  painter.drawLine(hiddenInner.topRight(), hiddenInner.bottomRight());

  QRect inner = content.adjusted(1, 1, -1, -1);
  painter.fillRect(inner, bg);

  bool showIcon = false;
  const QString text = displayLabel(showIcon);
  
  // Calculate constraint using (0.90 * height) - 4, matching MEDM's messageButtonFontListIndex
  // Search from largest to smallest font, return first that fits
  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = medmMessageButtonFont(fontLimit);
  painter.setFont(labelFont);
  painter.setPen(fg);
  painter.drawText(inner.adjusted(4, 0, -4, 0), Qt::AlignCenter, text);

  painter.restore();
}

void RelatedDisplayElement::paintSelectionOverlay(QPainter &painter) const
{
  QPen pen(Qt::black);
  pen.setStyle(Qt::DashLine);
  pen.setWidth(1);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void RelatedDisplayElement::mousePressEvent(QMouseEvent *event)
{
  if (!event) {
    return;
  }

  if (!executeMode_ || event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  if (visual_ == RelatedDisplayVisual::kMenu) {
    pressedEntryIndex_ = -1;
    event->accept();
    /* If there is only one usable entry, open it directly instead of showing
     * a menu with a single option. */
    int usableCount = 0;
    int singleIndex = -1;
    for (int i = 0; i < entryCount(); ++i) {
      if (entryHasTarget(i)) {
        ++usableCount;
        if (usableCount == 1) {
          singleIndex = i;
        }
      }
    }
    if (usableCount == 1 && singleIndex >= 0) {
      if (activationCallback_) {
        activationCallback_(singleIndex, event->modifiers());
      }
    } else {
      showMenu(event->modifiers());
    }
    return;
  }

  if (visual_ == RelatedDisplayVisual::kRowOfButtons
      || visual_ == RelatedDisplayVisual::kColumnOfButtons) {
    pressedEntryIndex_ = buttonEntryIndexAt(event->pos());
    event->accept();
    return;
  }

  pressedEntryIndex_ = firstUsableEntryIndex();
  event->accept();
}

void RelatedDisplayElement::mouseReleaseEvent(QMouseEvent *event)
{
  if (!event) {
    return;
  }

  if (!executeMode_ || event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  int targetIndex = -1;
  if (visual_ == RelatedDisplayVisual::kRowOfButtons
      || visual_ == RelatedDisplayVisual::kColumnOfButtons) {
    int hitIndex = buttonEntryIndexAt(event->pos());
    if (hitIndex >= 0 && hitIndex == pressedEntryIndex_) {
      targetIndex = hitIndex;
    }
  } else if (visual_ == RelatedDisplayVisual::kHiddenButton) {
    const int candidate = firstUsableEntryIndex();
    if (candidate >= 0 && pressedEntryIndex_ == candidate
        && rect().contains(event->pos())) {
      targetIndex = candidate;
    }
  }

  pressedEntryIndex_ = -1;

  if (targetIndex >= 0 && entryHasTarget(targetIndex)) {
    if (activationCallback_) {
      activationCallback_(targetIndex, event->modifiers());
    }
  }

  event->accept();
}

bool RelatedDisplayElement::entryHasTarget(int index) const
{
  if (index < 0 || index >= entryCount()) {
    return false;
  }
  return !entries_[index].name.trimmed().isEmpty();
}

int RelatedDisplayElement::firstUsableEntryIndex() const
{
  const int count = entryCount();
  for (int i = 0; i < count; ++i) {
    if (entryHasTarget(i)) {
      return i;
    }
  }
  return -1;
}

int RelatedDisplayElement::buttonEntryIndexAt(const QPoint &pos) const
{
  /* For button visuals, use the full rect with no margins to match painting */
  QRect content = rect();
  if (!content.contains(pos)) {
    return -1;
  }

  const int activeCount = activeEntryCount();
  const int displayCount = activeCount > 0 ? activeCount : 2;
  if (displayCount <= 0) {
    return -1;
  }

  const bool vertical = (visual_ == RelatedDisplayVisual::kColumnOfButtons);
  const int columns = vertical ? 1 : std::max(1, displayCount);
  const int rows = vertical ? std::max(1, displayCount) : 1;
  const int buttonWidth = columns > 0 ? content.width() / columns : content.width();
  const int buttonHeight = rows > 0 ? content.height() / rows : content.height();

  int index = 0;
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      if (index >= displayCount) {
        return -1;
      }
      QRect buttonRect(content.left() + column * buttonWidth,
          content.top() + row * buttonHeight, buttonWidth, buttonHeight);
      if (buttonRect.contains(pos)) {
        if (index < entryCount() && entryHasTarget(index)) {
          return index;
        }
        return -1;
      }
      ++index;
    }
  }

  return -1;
}

void RelatedDisplayElement::showMenu(Qt::KeyboardModifiers modifiers)
{
  QMenu menu(this);
  int labelIndex = 0;
  for (int i = 0; i < entryCount(); ++i) {
    if (!entryHasTarget(i)) {
      continue;
    }
    QString label = entryDisplayLabel(entries_[i]);
    if (label.isEmpty()) {
      label = QStringLiteral("Display %1").arg(labelIndex + 1);
    }
    QAction *action = menu.addAction(label);
    action->setData(i);
    ++labelIndex;
  }

  if (menu.actions().isEmpty()) {
    return;
  }

  const QPoint globalPos = mapToGlobal(QPoint(width() / 2, height()));
  QAction *selected = menu.exec(globalPos);
  if (!selected) {
    return;
  }
  bool ok = false;
  int index = selected->data().toInt(&ok);
  if (!ok) {
    return;
  }
  if (activationCallback_ && entryHasTarget(index)) {
    activationCallback_(index, modifiers);
  }
}
