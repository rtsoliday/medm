#pragma once

#include <QFont>
#include <QHash>
#include <QString>

namespace LegacyFonts {

const QHash<QString, QFont> &all();
QFont font(const QString &key);
QFont fontOrDefault(const QString &key, const QFont &fallback);

enum class WidgetDMAliasMode {
  kFixed,
  kScalable,
};

void setWidgetDMAliasMode(WidgetDMAliasMode mode);

} // namespace LegacyFonts
