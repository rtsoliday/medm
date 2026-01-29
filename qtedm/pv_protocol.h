#pragma once

#include <QString>

enum class PvProtocol
{
  kCa,
  kPva
};

struct ParsedPvName
{
  PvProtocol protocol = PvProtocol::kCa;
  QString rawName;
  QString pvName;
};

inline ParsedPvName parsePvName(const QString &value)
{
  ParsedPvName parsed;
  parsed.rawName = value;
  QString trimmed = value.trimmed();
  if (trimmed.isEmpty()) {
    parsed.pvName = QString();
    return parsed;
  }

  if (trimmed.startsWith(QStringLiteral("pva://"), Qt::CaseInsensitive)) {
    parsed.protocol = PvProtocol::kPva;
    parsed.pvName = trimmed.mid(6);
    return parsed;
  }

  parsed.protocol = PvProtocol::kCa;
  parsed.pvName = trimmed;
  return parsed;
}

inline QString stripPvProtocol(const QString &value)
{
  return parsePvName(value).pvName;
}
