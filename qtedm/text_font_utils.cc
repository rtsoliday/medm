#include "text_font_utils.h"

#include <array>

#include <QFont>
#include <QFontMetrics>

#include "legacy_fonts.h"

namespace {

const std::array<QString, 16> &textFontAliases()
{
  static const std::array<QString, 16> kAliases = {
      QStringLiteral("widgetDM_4"), QStringLiteral("widgetDM_6"),
      QStringLiteral("widgetDM_8"), QStringLiteral("widgetDM_10"),
      QStringLiteral("widgetDM_12"), QStringLiteral("widgetDM_14"),
      QStringLiteral("widgetDM_16"), QStringLiteral("widgetDM_18"),
      QStringLiteral("widgetDM_20"), QStringLiteral("widgetDM_22"),
      QStringLiteral("widgetDM_24"), QStringLiteral("widgetDM_30"),
      QStringLiteral("widgetDM_36"), QStringLiteral("widgetDM_40"),
      QStringLiteral("widgetDM_48"), QStringLiteral("widgetDM_60"),
  };
  return kAliases;
}

} // namespace

QFont medmCompatibleTextFont(const QString &text, const QSize &availableSize)
{
  if (availableSize.width() <= 0 || availableSize.height() <= 0) {
    return QFont();
  }

  /* Apply MEDM's text entry constraint formula:
   * Don't allow height of font to exceed 90% - 4 pixels of text entry widget
   * (includes nominal 2*shadowThickness=2 shadow)
   */
  const int heightConstraint = static_cast<int>(0.90 * availableSize.height()) - 4;
  if (heightConstraint <= 0) {
    return QFont();
  }

  QString sample = text;
  if (sample.trimmed().isEmpty()) {
    sample = QStringLiteral("Ag");
  }

  QFont chosen;
  bool found = false;

  for (const QString &alias : textFontAliases()) {
    const QFont font = LegacyFonts::font(alias);
    if (font.family().isEmpty()) {
      continue;
    }

    const QFontMetrics metrics(font);
    const int fontHeight = metrics.ascent() + metrics.descent();
    if (fontHeight > heightConstraint) {
      continue;
    }

    chosen = font;
    found = true;
  }

  if (found) {
    return chosen;
  }

  for (const QString &alias : textFontAliases()) {
    const QFont fallback = LegacyFonts::font(alias);
    if (!fallback.family().isEmpty()) {
      return fallback;
    }
  }

  return QFont();
}

QFont medmTextMonitorFont(const QString &text, const QSize &availableSize)
{
  if (availableSize.width() <= 0 || availableSize.height() <= 0) {
    return QFont();
  }

  /* Text Monitor widgets in MEDM use FULL HEIGHT with no constraint formula.
   * This matches executeMonitors.c behavior:
   *   dmGetBestFontWithInfo(fontTable, MAX_FONTS, DUMMY_TEXT_FIELD,
   *       dlTextUpdate->object.height, dlTextUpdate->object.width, ...)
   */
  const int heightConstraint = availableSize.height();

  QString sample = text;
  if (sample.trimmed().isEmpty()) {
    sample = QStringLiteral("Ag");
  }

  QFont chosen;
  bool found = false;

  for (const QString &alias : textFontAliases()) {
    const QFont font = LegacyFonts::font(alias);
    if (font.family().isEmpty()) {
      continue;
    }

    const QFontMetrics metrics(font);
    const int fontHeight = metrics.ascent() + metrics.descent();
    if (fontHeight > heightConstraint) {
      continue;
    }

    chosen = font;
    found = true;
  }

  if (found) {
    return chosen;
  }

  for (const QString &alias : textFontAliases()) {
    const QFont fallback = LegacyFonts::font(alias);
    if (!fallback.family().isEmpty()) {
      return fallback;
    }
  }

  return QFont();
}

