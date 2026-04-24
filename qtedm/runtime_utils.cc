#include "runtime_utils.h"

#include <db_access.h>

namespace RuntimeUtils {
namespace {

bool isSddsColumnNameCharacter(QChar ch)
{
  const ushort value = ch.unicode();
  return (value >= 'A' && value <= 'Z')
      || (value >= 'a' && value <= 'z')
      || (value >= '0' && value <= '9')
      || value == '_';
}

QString sanitizeSddsColumnNamePart(const QString &name)
{
  QString sanitized;
  const QString trimmed = name.trimmed();
  sanitized.reserve(trimmed.size());
  for (QChar ch : trimmed) {
    sanitized.append(isSddsColumnNameCharacter(ch) ? ch : QLatin1Char('_'));
  }
  while (sanitized.startsWith(QLatin1Char('_'))) {
    sanitized.remove(0, 1);
  }
  return sanitized;
}

} // namespace

void appendNullTerminator(QByteArray &bytes)
{
  if (bytes.isEmpty() || bytes.back() != '\0') {
    bytes.append('\0');
  }
}

QString normalizeCalcExpression(const QString &expr)
{
  QString result = expr;
  /* Replace != with # (must do this before replacing ==) */
  result.replace("!=", "#");
  /* Replace == with = */
  result.replace("==", "=");
  return result;
}

bool isNumericFieldType(chtype fieldType)
{
  switch (fieldType) {
  case DBR_CHAR:
  case DBR_SHORT:
  case DBR_LONG:
  case DBR_FLOAT:
  case DBR_DOUBLE:
    return true;
  default:
    return false;
  }
}

QString sanitizeSddsColumnName(const QString &name, const QString &fallback)
{
  QString sanitized = sanitizeSddsColumnNamePart(name);
  if (sanitized.isEmpty()) {
    sanitized = sanitizeSddsColumnNamePart(fallback);
  }
  if (sanitized.isEmpty()) {
    sanitized = QStringLiteral("Column");
  }
  return sanitized;
}

} // namespace RuntimeUtils
