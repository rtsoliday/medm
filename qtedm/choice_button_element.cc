#include "choice_button_element.h"

#include <algorithm>
#include <cmath>

#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>

namespace {

constexpr int kSampleButtonCount = 2;
constexpr int kButtonMargin = 2;

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
  update();
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
  update();
}

QColor ChoiceButtonElement::effectiveForeground() const
{
  return foregroundColor_.isValid() ? foregroundColor_
                                    : palette().color(QPalette::WindowText);
}

QColor ChoiceButtonElement::effectiveBackground() const
{
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

  const QRect content = canvas.adjusted(0, 0, -1, -1);
  const int cellWidth = content.width() / columns;
  const int cellHeight = content.height() / rows;
  const int extraWidth = content.width() - cellWidth * columns;
  const int extraHeight = content.height() - cellHeight * rows;

  QColor foreground = effectiveForeground();
  QColor background = effectiveBackground();
  QColor pressedFill = blendedColor(background, foreground, 0.2);
  QColor unpressedFill = blendedColor(background, QColor(Qt::white), 0.15);
  QColor borderColor = blendedColor(foreground, QColor(Qt::black), 0.25);

  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::NoBrush);

  int buttonIndex = 0;
  int y = content.top();
  for (int row = 0; row < rows; ++row) {
    int rowHeight = cellHeight;
    if (row < extraHeight) {
      rowHeight += 1;
    }
    int x = content.left();
    for (int column = 0; column < columns; ++column) {
      if (buttonIndex >= kSampleButtonCount) {
        break;
      }
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

      painter.fillRect(interior, buttonIndex == 0 ? pressedFill : unpressedFill);
      painter.setPen(QPen(borderColor, 1));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(interior.adjusted(0, 0, -1, -1));

      QString label = QStringLiteral("%1...").arg(buttonIndex);
      QFont labelFont = font();
      if (labelFont.pointSizeF() <= 0.0) {
        labelFont.setPointSize(10);
      }
      QFontMetrics metrics(labelFont);
      const QRect textBounds = interior.adjusted(2, 2, -2, -2);
      while ((metrics.horizontalAdvance(label) > textBounds.width()
                || metrics.height() > textBounds.height())
          && labelFont.pointSize() > 4) {
        labelFont.setPointSize(labelFont.pointSize() - 1);
        metrics = QFontMetrics(labelFont);
      }
      painter.setFont(labelFont);
      painter.setPen(foreground);
      painter.drawText(textBounds, Qt::AlignCenter, label);

      ++buttonIndex;
      x += columnWidth;
    }
    y += rowHeight;
  }

  paintSelectionOverlay(painter);
}
