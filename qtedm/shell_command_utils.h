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

/* Macros are expanded when a display loads, so only a macro value that occurs
 * in one of commandTexts can have reached the command. Returns false, and the
 * macro name in unsafeMacro, when such a value contains shell syntax or an
 * option word. Macros the command does not contain never block it. */
bool shellCommandMacrosAreSafe(const QHash<QString, QString> &macros,
    const QStringList &commandTexts, QString *unsafeMacro = nullptr);

int executeShellCommand(const QString &command,
    QString *errorMessage = nullptr);
