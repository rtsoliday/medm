#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

constexpr qsizetype kMaximumAdlInputCharacters = 16 * 1024 * 1024;
constexpr qint64 kMaximumAdlInputBytes = 16LL * 1024LL * 1024LL;
constexpr int kMaximumAdlNestingDepth = 256;

/* A macro value that substitution placed in a string, starting at
 * position. */
struct AdlMacroUse
{
  qsizetype position = 0;
  QString name;
  QString value;
};

struct AdlProperty
{
  QString key;
  QString value;
  QList<AdlMacroUse> macroUses;
};

struct AdlNode
{
  QString name;
  QList<AdlProperty> properties;
  QList<AdlNode> children;
};

class AdlParser
{
public:
  /* Each $(name) in a section name, key or value whose name is defined in
   * macros is replaced by that macro's expanded value. As in MEDM, the
   * value is used literally: it is not escape-decoded and cannot end a
   * quoted string or token. Property values record where macros went. */
  static std::optional<AdlNode> parse(const QString &text,
      QString *errorMessage,
      const QHash<QString, QString> &macros = QHash<QString, QString>());
};

/* Replaces $(name) references in input, then in the values they insert, up
 * to ten levels deep. Fails when the result would exceed the ADL input
 * limits. */
bool expandMacros(const QString &input, const QHash<QString, QString> &macros,
    QString *output, QString *errorMessage = nullptr);

/* Replaces each $(name) in text that names a macro with its expanded value,
 * once and literally, and records each value's position in uses. Text that
 * the existing uses describe is copied unchanged, and their positions are
 * updated. */
bool substituteMacroReferences(const QString &text,
    const QHash<QString, QString> &macros, QString *output,
    QList<AdlMacroUse> *uses, QString *errorMessage = nullptr);

/* True when uses are in order, do not overlap, and text holds each value at
 * its position. */
bool macroUsesMatch(const QString &text, const QList<AdlMacroUse> &uses);

const AdlProperty *findProperty(const AdlNode &node, const QString &key);
QString propertyValue(const AdlNode &node, const QString &key,
    const QString &defaultValue = QString());
const AdlNode *findChild(const AdlNode &node, const QString &name);
QList<const AdlNode *> findChildren(const AdlNode &node, const QString &name);
QString normalizedAdlName(const QString &name);
