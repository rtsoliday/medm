#pragma once

#include <QFont>
#include <QPalette>
#include <QString>
#include <QWidget>

void centerWindowOnScreen(QWidget *window);
void positionWindowTopRight(QWidget *window, int rightMargin, int topMargin);
void showVersionDialog(QWidget *parent, const QFont &titleFont,
    const QFont &bodyFont, const QPalette &palette, bool autoClose = true);
void showHelpBrowser(QWidget *parent, const QString &title,
    const QString &htmlFilePath, const QFont &font, const QPalette &palette);

/* Returns true if the parent window of the given widget is in PV Info picking mode.
 * When PV Info mode is active, left clicks should be forwarded to the parent window
 * to show the PV Info dialog instead of being handled by the widget. */
bool isParentWindowInPvInfoMode(QWidget *widget);

/* Returns true if the parent window of the given widget is in PV Limits picking mode.
 * When PV Limits mode is active, left clicks should be forwarded to the parent window
 * to show the PV Limits dialog instead of being handled by the widget. */
bool isParentWindowInPvLimitsMode(QWidget *widget);
