#pragma once

#include <QString>

class QDialog;
class QWidget;

/* Reusable modeless windows owned by the invoking window or modal dialog. */
QDialog *showQtEdmHelp(QWidget *parent, const QString &topic = QString());
QDialog *showQtEdmAbout(QWidget *parent);
