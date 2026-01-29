#pragma once

#include <QString>

namespace PvNameUtils {

inline QString normalizePvName(const QString &value)
{
  if (value.isEmpty()) {
    return value;
  }

  int start = 0;
  while (start < value.size() && value.at(start).isSpace()) {
    ++start;
  }

  const QString remainder = value.mid(start);
  if (remainder.startsWith(QStringLiteral("pva::"), Qt::CaseInsensitive)) {
    QString normalized = value;
    normalized.replace(start, 5, QStringLiteral("pva://"));
    return normalized;
  }
  if (remainder.startsWith(QStringLiteral("pva://"), Qt::CaseInsensitive)) {
    QString normalized = value;
    normalized.replace(start, 6, QStringLiteral("pva://"));
    return normalized;
  }

  return value;
}

} // namespace PvNameUtils
