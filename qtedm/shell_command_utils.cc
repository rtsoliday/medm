#include "shell_command_utils.h"

#include <cstdlib>

namespace {

bool safeMacroValue(const QString &value,
    const QString &extraAllowed = QString())
{
  for (const QChar ch : value) {
    if (ch.isLetterOrNumber() || ch == QLatin1Char(' ')
        || QStringLiteral("_./:@+=,-").contains(ch)
        || extraAllowed.contains(ch)) {
      continue;
    }
#ifdef Q_OS_WIN
    if (ch == QLatin1Char('\\') || ch == QLatin1Char('\'')) {
      continue;
    }
#endif
    return false;
  }
  return true;
}

// A macro word starting with '-' can add an option to the program it is
// passed to, such as xterm -e. Plain numbers like -5 remain allowed.
bool hasOptionWord(const QString &value)
{
  for (const QString &word : value.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
    bool numeric = false;
    word.toDouble(&numeric);
    if (word.startsWith(QLatin1Char('-')) && !numeric) {
      return true;
    }
  }
  return false;
}

// Quoting does not survive a newline inside a shell comment, so dynamic
// values must stay on one line.
bool hasControlCharacter(const QString &value)
{
  for (const QChar ch : value) {
    if (ch.category() == QChar::Other_Control) {
      return true;
    }
  }
  return false;
}

#ifndef Q_OS_WIN
enum class ShellQuote { kNone, kSingle, kDouble };

// Follows the outer shell's quoting one character at a time.
struct ShellQuoteTracker
{
  ShellQuote quote = ShellQuote::kNone;
  bool escaped = false;

  void advance(QChar ch)
  {
    if (quote == ShellQuote::kSingle) {
      if (ch == QLatin1Char('\'')) quote = ShellQuote::kNone;
    } else if (escaped) {
      escaped = false;
    } else if (ch == QLatin1Char('\\')) {
      escaped = true;
    } else if (ch == QLatin1Char('"')) {
      quote = quote == ShellQuote::kDouble
          ? ShellQuote::kNone : ShellQuote::kDouble;
    } else if (ch == QLatin1Char('\'') && quote == ShellQuote::kNone) {
      quote = ShellQuote::kSingle;
    }
  }
};

QString quotedValue(const QString &value, ShellQuote quote)
{
  if (quote == ShellQuote::kDouble) {
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    escaped.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    return escaped;
  }
  if (quote == ShellQuote::kSingle) {
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return escaped;
  }
  // Unquoted, MEDM's shell splits the value into words. Quote each word so
  // the value still splits there but is not otherwise interpreted.
  QStringList words;
  for (const QString &word :
      value.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
    words.append(QLatin1Char('\'') + quotedValue(word, ShellQuote::kSingle)
        + QLatin1Char('\''));
  }
  return words.join(QLatin1Char(' '));
}

// Command substitution and here-documents start nested shell text with its
// own quoting rules. Single-quoted text is literal to the outer shell, and
// only $( and ` stay active inside double quotes.
int firstNestedShellSyntax(const QString &command)
{
  ShellQuoteTracker tracker;
  for (int index = 0; index < command.size(); ++index) {
    const QChar ch = command.at(index);
    const QChar next = index + 1 < command.size()
        ? command.at(index + 1) : QChar();
    if (tracker.quote != ShellQuote::kSingle && !tracker.escaped) {
      if (ch == QLatin1Char('`')
          || (ch == QLatin1Char('$') && next == QLatin1Char('('))) {
        return index;
      }
      if (tracker.quote == ShellQuote::kNone && ch == QLatin1Char('<')
          && next == QLatin1Char('<')) {
        return index;
      }
    }
    tracker.advance(ch);
  }
  return -1;
}
#endif

} // namespace

bool shellCommandMacrosAreSafe(const QHash<QString, QString> &macros,
    const QStringList &commandTexts, QString *unsafeMacro)
{
  for (auto it = macros.cbegin(); it != macros.cend(); ++it) {
    const QString &value = it.value();
    if (value.isEmpty()
        || (safeMacroValue(value) && !hasOptionWord(value))) {
      continue;
    }
    for (const QString &text : commandTexts) {
      if (text.contains(value)) {
        if (unsafeMacro) *unsafeMacro = it.key();
        return false;
      }
    }
  }
  return true;
}

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
  const auto rejectValue = [errorMessage](const QString &value) {
    if (hasControlCharacter(value)) {
      if (errorMessage) *errorMessage = QStringLiteral(
          "Dynamic shell values cannot contain control characters.");
      return true;
    }
#ifdef Q_OS_WIN
    // cmd.exe treats parentheses literally outside command blocks, and
    // paths such as C:\Program Files (x86) contain them.
    if (!safeMacroValue(value, QStringLiteral("()"))) {
      if (errorMessage) *errorMessage = QStringLiteral(
          "A dynamic shell value contains characters cmd.exe could interpret.");
      return true;
    }
#endif
    return false;
  };
#ifndef Q_OS_WIN
  // This quote tracker describes the outer command only. Command
  // substitutions and here-documents have their own shell quoting rules.
  ShellQuoteTracker tracker;
  const int nestedSyntax = firstNestedShellSyntax(command);
#endif
  for (int index = 0; index < command.size(); ++index) {
    const QChar ch = command.at(index);
    if (ch != QLatin1Char('&') || index + 1 >= command.size()
        || (command.at(index + 1) != QLatin1Char('P')
            && command.at(index + 1) != QLatin1Char('A')
            && command.at(index + 1) != QLatin1Char('T')
            && command.at(index + 1) != QLatin1Char('X'))) {
      output.append(ch);
#ifndef Q_OS_WIN
      tracker.advance(ch);
#endif
      continue;
    }
    const QChar token = command.at(index + 1);
#ifndef Q_OS_WIN
    if (token != QLatin1Char('X') && nestedSyntax >= 0
        && index > nestedSyntax) {
      if (errorMessage) *errorMessage = QStringLiteral(
          "Dynamic shell tokens are not supported after nested shell syntax.");
      return false;
    }
    if (tracker.escaped) {
      if (errorMessage) *errorMessage = QStringLiteral(
          "A shell token cannot follow a backslash.");
      return false;
    }
#endif
    if (token == QLatin1Char('P')) {
      if (context.pvNames.isEmpty()) {
        if (errorMessage) {
          *errorMessage = QStringLiteral(
              "The shell command requires at least one selected PV.");
        }
        return false;
      }
      for (const QString &name : context.pvNames) {
        if (rejectValue(name)) return false;
      }
#ifdef Q_OS_WIN
      output.append(context.pvNames.join(QLatin1Char(' ')));
#else
      QStringList names;
      for (const QString &name : context.pvNames) {
        names.append(quotedValue(name, tracker.quote));
      }
      output.append(names.join(QLatin1Char(' ')));
#endif
      ++index;
    } else if (token == QLatin1Char('A')) {
      if (rejectValue(context.displayPath)) return false;
#ifdef Q_OS_WIN
      output.append(context.displayPath);
#else
      output.append(quotedValue(context.displayPath, tracker.quote));
#endif
      ++index;
    } else if (token == QLatin1Char('T')) {
      if (rejectValue(context.displayTitle)) return false;
#ifdef Q_OS_WIN
      output.append(context.displayTitle);
#else
      output.append(quotedValue(context.displayTitle, tracker.quote));
#endif
      ++index;
    } else if (token == QLatin1Char('X')) {
      output.append(QString::number(context.windowId));
      ++index;
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
