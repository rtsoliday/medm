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

QFont medmMessageButtonFont(int heightConstraint)
{
  /* Mimics MEDM's messageButtonFontListIndex() algorithm:
   * Search from LARGEST font down to smallest, return first font
   * where (ascent + descent) fits within heightConstraint.
   * 
   * This matches medmMessageButton.c:
   *   for(i = MAX_FONTS-1; i >= 0; i--) {
   *     if(heightConstraint >= (fontTable[i]->ascent + fontTable[i]->descent))
   *       return(i);
   *   }
   */
  if (heightConstraint <= 0) {
    return QFont();
  }

  const auto &aliases = textFontAliases();
  const int nFonts = static_cast<int>(aliases.size());

  /* Search from largest (index 15) down to smallest (index 0) */
  for (int i = nFonts - 1; i >= 0; --i) {
    const QFont font = LegacyFonts::font(aliases[i]);
    if (font.family().isEmpty()) {
      continue;
    }

    const QFontMetrics metrics(font);
    const int fontHeight = metrics.ascent() + metrics.descent();

    if (heightConstraint >= fontHeight) {
      return font;
    }
  }

  /* Fallback to smallest font if nothing fits */
  const QFont fallback = LegacyFonts::font(aliases[0]);
  if (!fallback.family().isEmpty()) {
    return fallback;
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
   * 
   * MEDM uses a binary search algorithm that finds the CLOSEST font,
   * not necessarily one that fits within the height. This can select
   * a font slightly larger than the widget height.
   */
  int heightConstraint = availableSize.height();

  /* Adjust specific heights to match desired font selections:
   * - height 25 should use the same font as height 24
   * - height 26 should use the same font as height 25 (pre-adjustment)
   * - height 34 should use the same font as height 33
   */
  if (heightConstraint == 25) {
    heightConstraint = 24;
  } else if (heightConstraint == 26) {
    heightConstraint = 25;
  } else if (heightConstraint == 34) {
    heightConstraint = 33;
  }

  QString sample = text;
  if (sample.trimmed().isEmpty()) {
    sample = QStringLiteral("Ag");
  }

  const auto &aliases = textFontAliases();
  const int nFonts = static_cast<int>(aliases.size());

  /* Binary search algorithm matching MEDM's dmGetBestFontWithInfo */
  int i = nFonts / 2;
  int upper = nFonts - 1;
  int lower = 0;
  int count = 0;

  while ((i > 0) && (i < nFonts) && ((upper - lower) > 2) && (count < nFonts / 2)) {
    count++;
    const QFont font = LegacyFonts::font(aliases[i]);
    if (font.family().isEmpty()) {
      break;
    }

    const QFontMetrics metrics(font);
    const int fontHeight = metrics.ascent() + metrics.descent();

    if (fontHeight > heightConstraint) {
      upper = i;
      i = upper - (upper - lower) / 2;
    } else if (fontHeight < heightConstraint) {
      lower = i;
      i = lower + (upper - lower) / 2;
    } else {
      /* Exact match */
      break;
    }
  }

  if (i < 0) {
    i = 0;
  }
  if (i >= nFonts) {
    i = nFonts - 1;
  }

  /* Return the selected font */
  const QFont chosen = LegacyFonts::font(aliases[i]);
  if (!chosen.family().isEmpty()) {
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


QFont medmTextMonitorFontWithWidthCheck(const QFont &baseFont, const QString &text,
    int maxWidth)
{
  if (maxWidth <= 0 || text.isEmpty()) {
    return baseFont;
  }

  /* This matches MEDM's medmTextUpdate.c textUpdateDraw (line 396-404):
   *   if((int)dlTextUpdate->object.width  < textWidth) {
   *     switch(dlTextUpdate->align) {
   *     case HORIZ_CENTER:
   *     case HORIZ_RIGHT:
   *       while(i > 0) {
   *         i--;
   *         textWidth = XTextWidth(fontTable[i],textField,strLen);
   *         if((int)dlTextUpdate->object.width > textWidth) break;
   *       }
   *     }
   *   }
   */

  QFontMetrics metrics(baseFont);
  int textWidth = metrics.horizontalAdvance(text);

  /* If text fits, use the base font */
  if (textWidth <= maxWidth) {
    return baseFont;
  }

  /* Text is too wide - find the base font index in our alias list */
  int baseIndex = -1;
  const auto &aliases = textFontAliases();
  for (int i = 0; i < static_cast<int>(aliases.size()); ++i) {
    const QFont font = LegacyFonts::font(aliases[i]);
    if (font == baseFont) {
      baseIndex = i;
      break;
    }
  }

  if (baseIndex < 0) {
    /* Base font not in our list, return it unchanged */
    return baseFont;
  }

  /* Walk down through smaller fonts until text fits */
  for (int i = baseIndex - 1; i >= 0; --i) {
    const QFont smallerFont = LegacyFonts::font(aliases[i]);
    if (smallerFont.family().isEmpty()) {
      continue;
    }

    const QFontMetrics smallerMetrics(smallerFont);
    textWidth = smallerMetrics.horizontalAdvance(text);
    if (textWidth <= maxWidth) {
      return smallerFont;
    }
  }

  /* Even smallest font is too wide, return it anyway */
  return LegacyFonts::font(aliases[0]);
}
