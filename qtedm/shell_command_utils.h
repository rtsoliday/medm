#pragma once

#include <QString>
#include <QStringList>
#include <QHash>

struct ShellCommandExpansionContext
{
  QString displayPath;
  QString displayTitle;
  qulonglong windowId = 0;
  QStringList pvNames;
};

bool expandShellCommandTokens(const QString &command,
    const ShellCommandExpansionContext &context, QString *result,
    QString *errorMessage = nullptr);

bool shellCommandMacrosAreSafe(const QHash<QString, QString> &macros);

int executeShellCommand(const QString &command,
    QString *errorMessage = nullptr);
