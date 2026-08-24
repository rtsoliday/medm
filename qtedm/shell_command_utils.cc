#include "shell_command_utils.h"

#include <cstdlib>

bool expandShellCommandTokens(const QString &command,
    const ShellCommandExpansionContext &context, QString *result,
    QString *errorMessage)
{
  if (!result) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("A shell-command output is required.");
    }
    return false;
  }
  QString output;
  output.reserve(command.size());
  for (int index = 0; index < command.size(); ++index) {
    const QChar ch = command.at(index);
    if (ch != QLatin1Char('&') || index + 1 >= command.size()) {
      output.append(ch);
      continue;
    }
    const QChar token = command.at(index + 1);
    if (token == QLatin1Char('P')) {
      if (context.pvNames.isEmpty()) {
        if (errorMessage) {
          *errorMessage = QStringLiteral(
              "The shell command requires at least one selected PV.");
        }
        return false;
      }
      output.append(context.pvNames.join(QLatin1Char(' ')));
      ++index;
    } else if (token == QLatin1Char('A')) {
      output.append(context.displayPath);
      ++index;
    } else if (token == QLatin1Char('T')) {
      output.append(context.displayTitle);
      ++index;
    } else if (token == QLatin1Char('X')) {
      output.append(QString::number(context.windowId));
      ++index;
    } else {
      output.append(ch);
    }
  }
  *result = output;
  return true;
}

int executeShellCommand(const QString &command, QString *errorMessage)
{
  const QString trimmed = command.trimmed();
  if (trimmed.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("The shell command is empty.");
    }
    return -1;
  }
  QByteArray nativeCommand = trimmed.toLocal8Bit();
#ifdef Q_OS_WIN
  /* cmd.exe strips the first quote from a /c command unless a quoted
   * executable command line is enclosed in an additional pair. */
  if (trimmed.startsWith(QLatin1Char('"'))) {
    nativeCommand.prepend('"');
    nativeCommand.append('"');
  }
#endif
  const int status = std::system(nativeCommand.constData());
  if (status == -1 && errorMessage) {
    *errorMessage = QStringLiteral("Failed to start the shell command.");
  } else if (status != 0 && errorMessage) {
    *errorMessage = QStringLiteral("Shell command exited with status %1.")
        .arg(status);
  }
  return status;
}
