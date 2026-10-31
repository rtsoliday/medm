#pragma once

#include <QString>
#include <QStringList>
#include <QHash>

#include "adl_parser.h"

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

/* Returns false, and the macro name in unsafeMacro, when a macro value that
 * uses place in command could change what the shell runs. On Unix, a value
 * inside single or double quotes may contain any character except quotes,
 * a backslash, $, `, ;, &, |, <, > or a control character. An unquoted
 * value, or one after command substitution or here-document syntax, is
 * limited to letters, digits, spaces and _ . / : @ + = , - % ~, plus
 * braces when it has no comma or "..". A word that starts with - and is
 * not a number is refused everywhere. */
bool shellCommandMacroUsesAreSafe(const QString &command,
    const QList<AdlMacroUse> &uses, QString *unsafeMacro = nullptr);

/* For a command that has no record of where macros were substituted, such
 * as one edited in the display editor. Returns false, and the macro name in
 * unsafeMacro, when a macro value occurring in one of commandTexts contains
 * shell syntax or an option word. Macros the command does not contain never
 * block it. */
bool shellCommandMacrosAreSafe(const QHash<QString, QString> &macros,
    const QStringList &commandTexts, QString *unsafeMacro = nullptr);

int executeShellCommand(const QString &command,
    QString *errorMessage = nullptr);
