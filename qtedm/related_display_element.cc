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

QFont scaledFontForHeight(const QFont &base, int pixelLimit)
{
  if (pixelLimit <= 0) {
    return base;
  }

  QFont adjusted(base);
  if (adjusted.pixelSize() > 0) {
    adjusted.setPixelSize(pixelLimit);
    return adjusted;
  }

  qreal pointSize = adjusted.pointSizeF();
  if (pointSize <= 0.0) {
    pointSize = adjusted.pointSize();
  }
  if (pointSize <= 0.0) {
    QFontInfo info(adjusted);
    pointSize = info.pointSizeF();
  }
  if (pointSize <= 0.0) {
    pointSize = 12.0;
  }

  QFontMetricsF metrics(adjusted);
  qreal textHeight = metrics.ascent() + metrics.descent();
  if (textHeight <= 0.0) {
    textHeight = static_cast<qreal>(pixelLimit);
  }

  qreal scaledPoint = pointSize * static_cast<qreal>(pixelLimit) / textHeight;
  if (scaledPoint < 1.0) {
    scaledPoint = 1.0;
  }
  adjusted.setPointSizeF(scaledPoint);

  QFontMetricsF scaledMetrics(adjusted);
  qreal scaledHeight = scaledMetrics.ascent() + scaledMetrics.descent();
  int iterations = 0;
  while (scaledHeight > pixelLimit && scaledPoint > 1.0 && iterations < 16) {
    scaledPoint = std::max<qreal>(1.0, scaledPoint - 0.5);
    adjusted.setPointSizeF(scaledPoint);
    scaledMetrics = QFontMetricsF(adjusted);
    scaledHeight = scaledMetrics.ascent() + scaledMetrics.descent();
    ++iterations;
  }

  return adjusted;
}

const QPixmap &baseRelatedDisplayIcon()
{
  static const QPixmap pixmap(QStringLiteral(":/icons/related_display.xpm"));
  return pixmap;
}

QPixmap relatedDisplayIconForColor(const QColor &color)
{
  const QPixmap &base = baseRelatedDisplayIcon();
  if (base.isNull()) {
    return QPixmap();
  }

  static QHash<QRgb, QPixmap> cache;
  const QRgb key = color.rgba();
  const auto it = cache.constFind(key);
  if (it != cache.constEnd()) {
    return it.value();
  }

  QPixmap tinted(base.size());
  tinted.fill(Qt::transparent);

  QPainter painter(&tinted);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(tinted.rect(), color);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawPixmap(0, 0, base);
  painter.end();

  cache.insert(key, tinted);
  return cache.value(key);
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
  painter.fillRect(canvas, effectiveBackground());

  const QRect content = canvas.adjusted(1, 1, -1, -1);
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
    if (!entry.label.trimmed().isEmpty() || !entry.name.trimmed().isEmpty()
        || !entry.args.trimmed().isEmpty()) {
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

  painter.save();

  QRect bevelOuter = content.adjusted(-1, -1, 1, 1);
  painter.setPen(Qt::NoPen);
  painter.setBrush(bg);
  painter.drawRect(bevelOuter);

  painter.setPen(QPen(bg.lighter(135), 1));
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.topRight());
  painter.drawLine(bevelOuter.topLeft(), bevelOuter.bottomLeft());
  painter.setPen(QPen(bg.darker(145), 1));
  painter.drawLine(bevelOuter.bottomLeft(), bevelOuter.bottomRight());
  painter.drawLine(bevelOuter.topRight(), bevelOuter.bottomRight());

  QRect bevelInner = content;
  painter.setPen(QPen(bg.lighter(150), 1));
  painter.drawLine(bevelInner.topLeft(), bevelInner.topRight());
  painter.drawLine(bevelInner.topLeft(), bevelInner.bottomLeft());
  painter.setPen(QPen(bg.darker(170), 1));
  painter.drawLine(bevelInner.bottomLeft(), bevelInner.bottomRight());
  painter.drawLine(bevelInner.topRight(), bevelInner.bottomRight());

  bool showIcon = true;
  QString labelText = displayLabel(showIcon);
  if (visual_ == RelatedDisplayVisual::kMenu && executeMode_
      && label_.trimmed().isEmpty()) {
    labelText.clear();
  }

  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = scaledFontForHeight(painter.font(), fontLimit);
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

    const QPixmap iconPixmap = relatedDisplayIconForColor(fg);
    if (!iconPixmap.isNull()) {
      QPixmap scaled = iconPixmap.scaled(iconCanvas.size(),
          Qt::KeepAspectRatio, Qt::SmoothTransformation);
      QPoint topLeft(iconCanvas.left()
              + (iconCanvas.width() - scaled.width()) / 2,
          iconCanvas.top()
              + (iconCanvas.height() - scaled.height()) / 2);
      painter.drawPixmap(topLeft, scaled);
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

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(bg);
  painter.drawRect(content.adjusted(-1, -1, 0, 0));

  const int activeCount = activeEntryCount();
  const int displayCount = activeCount > 0 ? activeCount : 2;
  const int columns = vertical ? 1 : std::max(1, displayCount);
  const int rows = vertical ? std::max(1, displayCount) : 1;

  const int buttonWidth = columns > 0 ? content.width() / columns : content.width();
  const int buttonHeight = rows > 0 ? content.height() / rows : content.height();

  const int fontLimit = relatedDisplayPixelLimit(visual_, height(), displayCount);
  const QFont labelFont = scaledFontForHeight(painter.font(), fontLimit);
  painter.setFont(labelFont);

  int index = 0;
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      if (index >= displayCount) {
        break;
      }
      QRect buttonRect(content.left() + column * buttonWidth,
          content.top() + row * buttonHeight,
          buttonWidth, buttonHeight);
      buttonRect = buttonRect.adjusted(2, 2, -2, -2);

      QRect buttonOuter = buttonRect.adjusted(-1, -1, 1, 1);
      painter.setPen(QPen(bg.lighter(135), 1));
      painter.drawLine(buttonOuter.topLeft(), buttonOuter.topRight());
      painter.drawLine(buttonOuter.topLeft(), buttonOuter.bottomLeft());
      painter.setPen(QPen(bg.darker(145), 1));
      painter.drawLine(buttonOuter.bottomLeft(), buttonOuter.bottomRight());
      painter.drawLine(buttonOuter.topRight(), buttonOuter.bottomRight());

      QRect buttonInner = buttonRect;
      painter.setPen(QPen(bg.lighter(150), 1));
      painter.drawLine(buttonInner.topLeft(), buttonInner.topRight());
      painter.drawLine(buttonInner.topLeft(), buttonInner.bottomLeft());
      painter.setPen(QPen(bg.darker(170), 1));
      painter.drawLine(buttonInner.bottomLeft(), buttonInner.bottomRight());
      painter.drawLine(buttonInner.topRight(), buttonInner.bottomRight());

      painter.fillRect(buttonInner.adjusted(1, 1, -1, -1), bg);
      painter.setPen(fg);

      QString text;
      if (index < activeCount) {
        text = entryDisplayLabel(entries_[index]);
      }
      if (text.isEmpty()) {
        text = QStringLiteral("Display %1").arg(index + 1);
      }
      painter.drawText(buttonRect.adjusted(4, 0, -4, 0),
          Qt::AlignCenter, text);
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

  painter.save();
  QRect hiddenOuter = content.adjusted(-1, -1, 1, 1);
  painter.setPen(QPen(bg.lighter(135), 1));
  painter.drawLine(hiddenOuter.topLeft(), hiddenOuter.topRight());
  painter.drawLine(hiddenOuter.topLeft(), hiddenOuter.bottomLeft());
  painter.setPen(QPen(bg.darker(145), 1));
  painter.drawLine(hiddenOuter.bottomLeft(), hiddenOuter.bottomRight());
  painter.drawLine(hiddenOuter.topRight(), hiddenOuter.bottomRight());

  QRect hiddenInner = content;
  painter.setPen(QPen(bg.lighter(150), 1));
  painter.drawLine(hiddenInner.topLeft(), hiddenInner.topRight());
  painter.drawLine(hiddenInner.topLeft(), hiddenInner.bottomLeft());
  painter.setPen(QPen(bg.darker(170), 1));
  painter.drawLine(hiddenInner.bottomLeft(), hiddenInner.bottomRight());
  painter.drawLine(hiddenInner.topRight(), hiddenInner.bottomRight());

  QRect inner = content.adjusted(1, 1, -1, -1);
  painter.fillRect(inner, bg);

  bool showIcon = false;
  const QString text = displayLabel(showIcon);
  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = scaledFontForHeight(painter.font(), fontLimit);
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
    showMenu(event->modifiers());
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
  QRect content = rect().adjusted(1, 1, -1, -1);
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
