#include "runtime_utils.h"

#include <db_access.h>

namespace RuntimeUtils {

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

} // namespace RuntimeUtils
