#pragma once

#include <QColor>
#include <QFont>
#include <QRect>
#include <QString>

class QPainter;

enum class MedmButtonIcon
{
  kRelatedDisplay,
  kShellCommand,
};

// Paints the face of a MEDM related display or shell command button.
// As in medmRelatedDisplay.c and medmShellCommand.c, the icon, a square
// min(width, height) - 8 pixels high, and the label as written form one
// image; a label starting with '-' has no icon.  A push button (one item)
// centers the image and a menu bar (several items) starts it after both
// shadows.  Motif clips the image to the button inside its shadows.
void paintMedmButtonPixmap(QPainter &painter, const QRect &bounds,
    const QString &label, MedmButtonIcon icon, const QFont &font,
    const QColor &foreground, const QColor &background, bool menuBar);
