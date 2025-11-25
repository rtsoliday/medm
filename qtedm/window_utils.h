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

