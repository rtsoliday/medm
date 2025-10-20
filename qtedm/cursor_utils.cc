#include "cursor_utils.h"

#include <QColor>
#include <QCursor>
#include <QPainter>
#include <QPixmap>

namespace CursorUtils {
namespace {

bool g_useBigCursor = false;
bool g_bigCursorsInitialized = false;
QCursor g_arrowCursorBig;
QCursor g_crossCursorBig;
QCursor g_forbiddenCursorBig;

QCursor createBigArrowCursor()
{
  QPixmap pix(48, 48);
  pix.fill(Qt::transparent);

  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QPolygon arrow;
  arrow << QPoint(4, 4) << QPoint(4, 40) << QPoint(12, 32) << QPoint(18, 40)
        << QPoint(24, 34) << QPoint(18, 26) << QPoint(26, 26);
  painter.setBrush(Qt::black);
  painter.setPen(Qt::black);
  painter.drawPolygon(arrow);

  QPen outline(Qt::white);
  outline.setWidth(2);
  painter.setPen(outline);
  painter.setBrush(Qt::NoBrush);
  painter.drawPolygon(arrow);

  return QCursor(pix, 4, 4);
}

QCursor createBigCrossCursor()
{
  QPixmap pix(48, 48);
  pix.fill(Qt::transparent);

  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QPen pen(Qt::black);
  pen.setWidth(6);
  pen.setCapStyle(Qt::RoundCap);
  painter.setPen(pen);
  painter.drawLine(QPoint(24, 6), QPoint(24, 42));
  painter.drawLine(QPoint(6, 24), QPoint(42, 24));

  pen.setColor(Qt::white);
  pen.setWidth(2);
  painter.setPen(pen);
  painter.drawLine(QPoint(24, 6), QPoint(24, 42));
  painter.drawLine(QPoint(6, 24), QPoint(42, 24));

  return QCursor(pix, 24, 24);
}

QCursor createBigForbiddenCursor()
{
  QPixmap pix(48, 48);
  pix.fill(Qt::transparent);

  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setBrush(QColor(220, 0, 0));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPoint(24, 24), 18, 18);

  painter.setBrush(Qt::white);
  painter.drawEllipse(QPoint(24, 24), 12, 12);

  QPen slashPen(Qt::black);
  slashPen.setWidth(6);
  slashPen.setCapStyle(Qt::RoundCap);
  painter.setPen(slashPen);
  painter.drawArc(QRect(6, 6, 36, 36), 45 * 16, 270 * 16);

  slashPen.setColor(Qt::white);
  slashPen.setWidth(2);
  painter.setPen(slashPen);
  painter.drawLine(QPoint(12, 12), QPoint(36, 36));

  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(Qt::black, 6));
  painter.drawEllipse(QPoint(24, 24), 18, 18);
  painter.setPen(QPen(Qt::white, 2));
  painter.drawEllipse(QPoint(24, 24), 18, 18);

  return QCursor(pix, 24, 24);
}

void ensureBigCursorsInitialized()
{
  if (g_bigCursorsInitialized) {
    return;
  }
  g_arrowCursorBig = createBigArrowCursor();
  g_crossCursorBig = createBigCrossCursor();
  g_forbiddenCursorBig = createBigForbiddenCursor();
  g_bigCursorsInitialized = true;
}

}  // namespace

void setUseBigCursor(bool enabled)
{
  g_useBigCursor = enabled;
}

QCursor arrowCursor()
{
  if (!g_useBigCursor) {
    return QCursor(Qt::ArrowCursor);
  }
  ensureBigCursorsInitialized();
  return g_arrowCursorBig;
}

QCursor crossCursor()
{
  if (!g_useBigCursor) {
    return QCursor(Qt::CrossCursor);
  }
  ensureBigCursorsInitialized();
  return g_crossCursorBig;
}

QCursor forbiddenCursor()
{
  if (!g_useBigCursor) {
    return QCursor(Qt::ForbiddenCursor);
  }
  ensureBigCursorsInitialized();
  return g_forbiddenCursorBig;
}

}  // namespace CursorUtils
