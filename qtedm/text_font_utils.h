#pragma once

#include <QFont>
#include <QSize>
#include <QString>

// Returns a font compatible with MEDM sizing rules for the supplied text and
// available geometry. The function chooses from the legacy MEDM font aliases
// used by QtEDM to mimic MEDM's layout.
QFont medmCompatibleTextFont(const QString &text, const QSize &availableSize);

