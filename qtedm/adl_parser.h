#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

constexpr qsizetype kMaximumAdlInputCharacters = 16 * 1024 * 1024;
constexpr qint64 kMaximumAdlInputBytes = 16LL * 1024LL * 1024LL;
constexpr int kMaximumAdlNestingDepth = 256;

struct AdlProperty
{
  QString key;
  QString value;
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
  static std::optional<AdlNode> parse(const QString &text,
      QString *errorMessage);
};

const AdlProperty *findProperty(const AdlNode &node, const QString &key);
QString propertyValue(const AdlNode &node, const QString &key,
    const QString &defaultValue = QString());
const AdlNode *findChild(const AdlNode &node, const QString &name);
QList<const AdlNode *> findChildren(const AdlNode &node, const QString &name);
QString normalizedAdlName(const QString &name);
