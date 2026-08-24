#pragma once

#include <QString>

class QWidget;

bool renderWidgetImageToPath(QWidget *widget, const QString &filePath,
    const QString &title, const QString &description,
    QString *errorMessage = nullptr);
