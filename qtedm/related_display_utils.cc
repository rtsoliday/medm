#include "related_display_utils.h"

#include <QRegularExpression>

#include "adl_parser.h"
#include "macro_utils.h"

namespace {

/* Returns false when the arguments would expand beyond what a display file
 * may hold: a display that passes $(A)$(A) as A to itself doubles them at
 * each pass, and with each display it opens. */
bool expandArgumentMacros(const QString &input,
    const QHash<QString, QString> &macros, QString *output)
{
  static constexpr int kMaxIterations = 10;
  static const QRegularExpression pattern(
      QStringLiteral(R"(\$\(([^)]+)\))"));
  QString current = input;
  for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
    QString expanded;
    expanded.reserve(current.size());
    int lastIndex = 0;
    bool replaced = false;
    QRegularExpressionMatchIterator matches = pattern.globalMatch(current);
    while (matches.hasNext()) {
      const QRegularExpressionMatch match = matches.next();
      expanded.append(current.mid(
          lastIndex, match.capturedStart() - lastIndex));
      const auto macro = macros.constFind(match.captured(1));
      if (macro == macros.constEnd()) {
        expanded.append(match.captured(0));
      } else {
        if (macro->size() > kMaximumAdlInputCharacters - expanded.size()) {
          return false;
        }
        expanded.append(*macro);
        replaced = true;
      }
      lastIndex = match.capturedEnd();
    }
    expanded.append(current.mid(lastIndex));
    if (expanded.size() > kMaximumAdlInputCharacters) {
      return false;
    }
    if (!replaced) {
      *output = current;
      return true;
    }
    current = expanded;
  }
  *output = current;
  return true;
}

bool parseMacroArguments(const QString &arguments,
    QHash<QString, QString> *macros, QString *errorMessage)
{
  if (!macros) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Missing macro output storage.");
    }
    return false;
  }
  macros->clear();
  for (const QString &argument : arguments.split(
           QLatin1Char(','), Qt::KeepEmptyParts)) {
    const QString trimmed = argument.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }
    const int equals = trimmed.indexOf(QLatin1Char('='));
    if (equals <= 0) {
      if (errorMessage) {
        *errorMessage = QStringLiteral(
            "Invalid related-display macro definition: %1").arg(trimmed);
      }
      return false;
    }
    const QString name = removeMacroWhitespace(trimmed.left(equals));
    if (name.isEmpty()) {
      if (errorMessage) {
        *errorMessage = QStringLiteral(
            "Related-display macro names cannot be empty.");
      }
      return false;
    }
    /* MEDM's lookupNameValue returns the first definition of a name. */
    if (!macros->contains(name)) {
      macros->insert(name, removeMacroWhitespace(trimmed.mid(equals + 1)));
    }
  }
  return true;
}

}  // namespace

bool prepareRelatedDisplayLaunch(const RelatedDisplayEntry &entry,
    Qt::KeyboardModifiers modifiers,
    const QHash<QString, QString> &inheritedMacros,
    RelatedDisplayLaunchSpec *result, QString *errorMessage)
{
  if (!result) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Missing related-display launch output.");
    }
    return false;
  }
  const QString fileName = entry.name.trimmed();
  if (fileName.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Related display has no target file.");
    }
    return false;
  }

  RelatedDisplayLaunchSpec prepared;
  prepared.fileName = fileName;
  prepared.replace = entry.mode == RelatedDisplayMode::kReplace
      || modifiers.testFlag(Qt::ControlModifier);
  QString expandedArguments;
  if (!expandArgumentMacros(entry.args, inheritedMacros,
          &expandedArguments)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "Related-display macro arguments expand to more than %1 "
          "characters.").arg(kMaximumAdlInputCharacters);
    }
    return false;
  }
  if (!parseMacroArguments(
          expandedArguments, &prepared.macros, errorMessage)) {
    return false;
  }
  *result = prepared;
  return true;
}
