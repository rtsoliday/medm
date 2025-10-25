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

// Returns a font compatible with MEDM sizing rules for Message Button / Shell Command / Related Display.
// Searches from largest to smallest font, returns first that fits within heightConstraint.
// This matches medmMessageButton.c's messageButtonFontListIndex() algorithm.
QFont medmMessageButtonFont(int heightConstraint);

// Shrinks a font to fit text within width constraint, starting from a base font.
// Matches MEDM's medmTextUpdate.c behavior (line 396-404) where font is reduced
// if text is too wide. Returns the original font if text fits, or a smaller font.
QFont medmTextMonitorFontWithWidthCheck(const QFont &baseFont, const QString &text,
    int maxWidth);
