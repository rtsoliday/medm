#pragma once

#include <QString>
#include <QStringList>

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

int executeShellCommand(const QString &command,
    QString *errorMessage = nullptr);
