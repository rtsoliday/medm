#pragma once

#include <array>

#include <QColor>

namespace MedmColors {

const std::array<QColor, 65> &palette();
int indexForColor(const QColor &color);
QColor alarmColorForSeverity(short severity);

/* Compute Motif-style shadow colors from a background color.
 * This mimics the XmGetColors() algorithm which handles edge cases
 * like very dark or very light backgrounds better than simple
 * percentage-based lighter()/darker() calls. */
void computeShadowColors(const QColor &background,
    QColor &topShadow, QColor &bottomShadow);

} // namespace MedmColors

