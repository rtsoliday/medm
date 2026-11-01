#include "medm_button_pixmap.h"

#include <algorithm>

#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QPoint>

namespace {

/* MEDM scales its 25-pixel icon designs by single-precision fractions and
 * truncates, so compute in float to land on the same pixels. */
int scaled(double twentyFifths, int size)
{
  const float fraction = static_cast<float>(twentyFifths / 25.0);
  return static_cast<int>(fraction * static_cast<float>(size));
}

void paintRelatedDisplayIcon(QPainter &painter, const QPoint &origin,
    int size, const QColor &foreground, const QColor &background)
{
  const QPoint corners[] = {
      origin + QPoint(scaled(16, size), scaled(9, size)),
      origin + QPoint(scaled(22, size), scaled(9, size)),
      origin + QPoint(scaled(22, size), scaled(22, size)),
      origin + QPoint(scaled(10, size), scaled(22, size)),
      origin + QPoint(scaled(10, size), scaled(18, size)),
  };
  painter.setPen(QPen(foreground, 0));
  for (int i = 0; i < 4; ++i) {
    painter.drawLine(corners[i], corners[i + 1]);
  }

  /* The front window hides the ends of the back window's outline. */
  const QRect front(origin.x() + scaled(4, size),
      origin.y() + scaled(4, size), scaled(13, size), scaled(14, size));
  painter.fillRect(front, background);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(front);
}

void paintShellCommandIcon(QPainter &painter, const QPoint &origin,
    int size, const QColor &foreground)
{
  const int barWidth = std::max(1, scaled(3, size));
  painter.fillRect(origin.x() + scaled(12, size),
      origin.y() + scaled(4, size), barWidth,
      std::max(1, scaled(14, size)), foreground);
  painter.fillRect(origin.x() + scaled(12, size),
      origin.y() + scaled(20, size), barWidth,
      std::max(1, scaled(3, size)), foreground);
}

} // namespace

void paintMedmButtonPixmap(QPainter &painter, const QRect &bounds,
    const QString &label, MedmButtonIcon icon, const QFont &font,
    const QColor &foreground, const QColor &background, bool menuBar)
{
  const int height = std::max(1,
      std::min(bounds.width(), bounds.height()) - 8);
  QString text = label;
  bool showIcon = true;
  if (text.startsWith(QLatin1Char('-'))) {
    showIcon = false;
    text.remove(0, 1);
  }
  const QFontMetrics metrics(font);
  const int textWidth = text.isEmpty() ? 0 : metrics.horizontalAdvance(text);
  const int iconWidth = showIcon ? height : 0;
  const int width = std::max(1, iconWidth + textWidth);

  /* Motif centers a push button's image between its two-pixel shadow and
   * two-pixel margin, truncating toward zero for an image wider than that.
   * A menu bar shrinks its cascade button to fit, so the image starts after
   * the bar's and the button's shadows. */
  const QPoint origin(bounds.left()
          + (menuBar ? 4 : 4 + (bounds.width() - 8 - width) / 2),
      bounds.top() + 4 + (bounds.height() - 8 - height) / 2);
  const int inset = menuBar ? 4 : 2;
  const QRect image(origin, QSize(width, height));

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setClipRect(
      image & bounds.adjusted(inset, inset, -inset, -inset),
      Qt::IntersectClip);
  painter.fillRect(image, background);
  if (showIcon) {
    if (icon == MedmButtonIcon::kRelatedDisplay) {
      paintRelatedDisplayIcon(painter, origin, height, foreground,
          background);
    } else {
      paintShellCommandIcon(painter, origin, height, foreground);
    }
  }
  if (!text.isEmpty()) {
    painter.setFont(font);
    painter.setPen(foreground);
    const int baseline =
        (height + metrics.ascent() - metrics.descent()) / 2;
    painter.drawText(origin + QPoint(iconWidth, baseline), text);
  }
  painter.restore();
}
