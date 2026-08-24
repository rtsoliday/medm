#include "related_display_utils.h"

#include <QRegularExpression>

namespace {

QString expandMacros(const QString &input,
    const QHash<QString, QString> &macros)
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
        expanded.append(*macro);
        replaced = true;
      }
      lastIndex = match.capturedEnd();
    }
    expanded.append(current.mid(lastIndex));
    if (!replaced) {
      return current;
    }
    current = expanded;
  }
  return current;
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
    const QString name = trimmed.left(equals).trimmed();
    if (name.isEmpty()) {
      if (errorMessage) {
        *errorMessage = QStringLiteral(
            "Related-display macro names cannot be empty.");
      }
      return false;
    }
    macros->insert(name, trimmed.mid(equals + 1).trimmed());
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
  const QString expandedArguments =
      expandMacros(entry.args, inheritedMacros);
  if (!parseMacroArguments(
          expandedArguments, &prepared.macros, errorMessage)) {
    return false;
  }
  *result = prepared;
  return true;
}
