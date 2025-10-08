#include "related_display_element.h"

#include <algorithm>

#include <QBrush>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>

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
  painter.setPen(QPen(fg, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(content.adjusted(0, 0, -1, -1));

  bool showIcon = true;
  const QString labelText = displayLabel(showIcon);

  const int fontLimit = messageButtonPixelLimit(height());
  const QFont labelFont = scaledFontForHeight(painter.font(), fontLimit);
  painter.setFont(labelFont);

  QRect inner = content.adjusted(1, 1, -1, -1);
  int iconWidth = std::min(inner.height(), 24);
  QRect iconRect = inner;
  iconRect.setWidth(iconWidth);

  if (showIcon && iconRect.width() > 6 && iconRect.height() > 6) {
    QRect frame = iconRect.adjusted(0, 0, -1, -1);
    painter.fillRect(frame.adjusted(1, 1, -1, -1), bg.darker(108));
    painter.drawRect(frame);
    const int barHeight = std::max(2, frame.height() / 3);
    QRect topRect(frame.left() + 2, frame.top() + 2,
        frame.width() - 4, barHeight);
    painter.fillRect(topRect, fg);
    painter.setPen(QPen(bg, 1));
    painter.drawLine(topRect.bottomLeft(), topRect.bottomRight());
  }

  QRect textRect = inner;
  if (showIcon) {
    textRect.setLeft(iconRect.right() + 6);
  } else {
    textRect.setLeft(inner.left() + 4);
  }
  textRect.setRight(inner.right() - 12);
  painter.setPen(fg);
  painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, labelText);

  QPoint arrow[3];
  const int arrowWidth = 7;
  const int arrowHeight = 5;
  const int arrowX = inner.right() - arrowWidth - 3;
  const int arrowY = inner.center().y() - arrowHeight / 2;
  arrow[0] = QPoint(arrowX, arrowY);
  arrow[1] = QPoint(arrowX + arrowWidth, arrowY);
  arrow[2] = QPoint(arrowX + arrowWidth / 2, arrowY + arrowHeight);
  painter.setBrush(fg);
  painter.drawPolygon(arrow, 3);

  painter.restore();
}

void RelatedDisplayElement::paintButtonVisual(QPainter &painter,
    const QRect &content, bool vertical) const
{
  const QColor fg = effectiveForeground();
  const QColor bg = effectiveBackground();

  painter.save();
  painter.setPen(QPen(fg, 1));
  painter.drawRect(content.adjusted(0, 0, -1, -1));

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

      painter.fillRect(buttonRect, bg);
      painter.setPen(QPen(bg.lighter(130), 1));
      painter.drawLine(buttonRect.topLeft(), buttonRect.topRight());
      painter.drawLine(buttonRect.topLeft(), buttonRect.bottomLeft());
      painter.setPen(QPen(bg.darker(130), 1));
      painter.drawLine(buttonRect.bottomLeft(), buttonRect.bottomRight());
      painter.drawLine(buttonRect.topRight(), buttonRect.bottomRight());

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
  painter.setPen(QPen(fg, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(content.adjusted(0, 0, -1, -1));

  QRect inner = content.adjusted(1, 1, -1, -1);
  painter.fillRect(inner, QBrush(fg, Qt::BDiagPattern));
  painter.fillRect(inner.adjusted(2, 2, -2, -2), bg);

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
