#pragma once

#include <QFont>
#include <QSize>
#include <QString>

// Returns a font compatible with MEDM sizing rules for Text Entry widgets.
// Uses the (0.90 * height) - 4 constraint formula from medmTextEntry.c.
// The function chooses from the legacy MEDM font aliases used by QtEDM.
QFont medmCompatibleTextFont(const QString &text, const QSize &availableSize);

// Returns a font compatible with MEDM sizing rules for Text Monitor widgets.
// Uses full height with no constraint (matches executeMonitors.c behavior).
// The function chooses from the legacy MEDM font aliases used by QtEDM.
QFont medmTextMonitorFont(const QString &text, const QSize &availableSize);

