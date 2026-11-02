#pragma once

#include <QString>

/* MEDM's generateNameValueTable() drops every whitespace character from a
 * macro name and value, so "xx=05 yy=BM" defines xx as "05yy=BM". */
inline QString removeMacroWhitespace(const QString &text)
{
  QString result;
  result.reserve(text.size());
  for (const QChar ch : text) {
    const char16_t code = ch.unicode();
    if (code != u' ' && (code < u'\t' || code > u'\r')) {
      result.append(ch);
    }
  }
  return result;
}
