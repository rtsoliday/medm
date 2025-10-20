#include "legacy_fonts.h"

#include <QByteArray>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHash>
#include <QStringList>
#include <QtGlobal>

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
#include "resources/fonts/adobe_helvetica_24_otb.h"
#include "resources/fonts/adobe_helvetica_bold_24_otb.h"
#include "resources/fonts/adobe_times_18_otb.h"
#include "resources/fonts/misc_fixed_10_otb.h"
#include "resources/fonts/misc_fixed_10x20_otb.h"
#include "resources/fonts/misc_fixed_13_otb.h"
#include "resources/fonts/misc_fixed_7x13_otb.h"
#include "resources/fonts/misc_fixed_7x14_otb.h"
#include "resources/fonts/misc_fixed_8_otb.h"
#include "resources/fonts/misc_fixed_9_otb.h"
#include "resources/fonts/misc_fixed_9x15_otb.h"
#include "resources/fonts/sony_fixed_12x24_otb.h"
#include "resources/fonts/sony_fixed_8x16_otb.h"
#endif

#include "resources/fonts/bitstream_charter_bold_otf.h"

namespace {

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)

QFont loadSystemFont(const char *family, int pixelSize,
    QFont::StyleHint styleHint, bool fixedPitch, QFont::Weight weight,
    int stretch = 100)
{
  QFont font(QString::fromLatin1(family));

  font.setStyleHint(styleHint, fixedPitch ? QFont::PreferMatch
                                          : QFont::PreferDefault);
  font.setStyleStrategy(QFont::PreferDefault);
  font.setFixedPitch(fixedPitch);
  font.setPixelSize(pixelSize);
  font.setWeight(weight);
  font.setBold(weight >= QFont::DemiBold);
  if (stretch != 100) {
    font.setStretch(stretch);
  }

  const QFontInfo info(font);
  if (info.family() != QString::fromLatin1(family)) {
    const QFontDatabase::SystemFont fallback =
        fixedPitch ? QFontDatabase::FixedFont : QFontDatabase::GeneralFont;
    font = QFontDatabase::systemFont(fallback);
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    font.setBold(weight >= QFont::DemiBold);
    font.setFixedPitch(fixedPitch);
    font.setStyleHint(styleHint, QFont::PreferDefault);
    if (stretch != 100) {
      font.setStretch(stretch);
    }
  }

  return font;
}

#endif

QFont loadEmbeddedFont(const unsigned char *data, std::size_t size,
    int pixelSize, QFont::StyleHint styleHint, bool fixedPitch,
    QFont::Weight weight, QFont::StyleStrategy strategy)
{
  using CacheKey = quintptr;
  static QHash<CacheKey, int> fontIds;

  const CacheKey key = reinterpret_cast<CacheKey>(data);
  int fontId = -2;

  if (fontIds.contains(key)) {
    fontId = fontIds.value(key);
  } else {
    const QByteArray bytes(reinterpret_cast<const char *>(data),
        static_cast<int>(size));
    fontId = QFontDatabase::addApplicationFontFromData(bytes);
    fontIds.insert(key, fontId);
  }

  QFont font;
  if (fontId != -1) {
    const QStringList families = QFontDatabase::applicationFontFamilies(
        fontId);
    if (!families.isEmpty()) {
      font = QFont(families.first());
    }
  }

  if (font.family().isEmpty()) {
    const QFontDatabase::SystemFont fallback =
        styleHint == QFont::TypeWriter ? QFontDatabase::FixedFont
                                       : QFontDatabase::GeneralFont;
    font = QFontDatabase::systemFont(fallback);
  }

  font.setStyleHint(styleHint, strategy);
  font.setStyleStrategy(strategy);
  font.setFixedPitch(fixedPitch);
  font.setPixelSize(pixelSize);
  font.setWeight(weight);
  font.setBold(weight >= QFont::DemiBold);
  return font;
}

QFont loadBitstreamCharterBold(int pixelSize)
{
  return loadEmbeddedFont(kBitstreamCharterBoldFontData,
      kBitstreamCharterBoldFontSize, pixelSize, QFont::Serif, false,
      QFont::Bold, QFont::PreferDefault);
}

bool isBitstreamCharterXLFD(const QString &key, int *pixelSize)
{
  if (!key.startsWith(QStringLiteral("-bitstream-charter-bold-r-normal--"))) {
    return false;
  }

  const QStringList parts = key.split('-', Qt::KeepEmptyParts);
  if (parts.size() < 16) {
    return false;
  }

  if (parts.at(1) != QLatin1String("bitstream") ||
      parts.at(2) != QLatin1String("charter") ||
      parts.at(3) != QLatin1String("bold") ||
      parts.at(4) != QLatin1String("r") ||
      parts.at(5) != QLatin1String("normal")) {
    return false;
  }

  bool ok = false;
  const int value = parts.at(7).toInt(&ok);
  if (!ok || value <= 0) {
    return false;
  }

  if (pixelSize) {
    *pixelSize = value;
  }
  return true;
}

QHash<QString, QFont> &fontCache()
{
  static QHash<QString, QFont> fonts = [] {
    QHash<QString, QFont> fonts;

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    struct FontSpec {
      const char *key;
      const char *family;
      int pixelSize;
      QFont::StyleHint styleHint;
      bool fixedPitch;
      QFont::Weight weight;
      int stretch;
    };

    const FontSpec fontSpecs[] = {
        {"miscFixed8", "Courier New", 8, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"miscFixed9", "Courier New", 9, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"miscFixed10", "Courier New", 10, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"miscFixed13", "Courier New", 13, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"miscFixed7x13", "Courier New", 13, QFont::TypeWriter, true,
         QFont::Normal, 90},
        {"miscFixed7x14", "Courier New", 14, QFont::TypeWriter, true,
         QFont::Normal, 90},
        {"miscFixed9x15", "Courier New", 15, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"sonyFixed8x16", "Courier New", 16, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"miscFixed10x20", "Courier New", 20, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"sonyFixed12x24", "Courier New", 24, QFont::TypeWriter, true,
         QFont::Normal, 100},
        {"adobeTimes18", "Times New Roman", 25, QFont::Serif, false,
         QFont::Normal, 100},
        {"adobeHelvetica24", "Arial", 34, QFont::SansSerif, false,
         QFont::Normal, 100},
        {"adobeHelveticaBold24", "Arial", 34, QFont::SansSerif, false,
         QFont::Bold, 100},
    };

    for (const FontSpec &spec : fontSpecs) {
      fonts.insert(QString::fromLatin1(spec.key), loadSystemFont(spec.family,
          spec.pixelSize, spec.styleHint, spec.fixedPitch, spec.weight,
          spec.stretch));
    }
#else
    struct FontSpec {
      const char *key;
      const unsigned char *data;
      std::size_t size;
      int pixelSize;
      QFont::StyleHint styleHint;
      bool fixedPitch;
      QFont::Weight weight;
    };

    const FontSpec fontSpecs[] = {
        {"miscFixed8", kMiscFixed8FontData, kMiscFixed8FontSize, 8,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed9", kMiscFixed9FontData, kMiscFixed9FontSize, 9,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed10", kMiscFixed10FontData, kMiscFixed10FontSize, 10,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed13", kMiscFixed13FontData, kMiscFixed13FontSize, 13,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed7x13", kMiscFixed7x13FontData, kMiscFixed7x13FontSize, 13,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed7x14", kMiscFixed7x14FontData, kMiscFixed7x14FontSize, 14,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed9x15", kMiscFixed9x15FontData, kMiscFixed9x15FontSize, 15,
         QFont::TypeWriter, true, QFont::Normal},
        {"sonyFixed8x16", kSonyFixed8x16FontData, kSonyFixed8x16FontSize, 16,
         QFont::TypeWriter, true, QFont::Normal},
        {"miscFixed10x20", kMiscFixed10x20FontData, kMiscFixed10x20FontSize,
         20, QFont::TypeWriter, true, QFont::Normal},
        {"sonyFixed12x24", kSonyFixed12x24FontData, kSonyFixed12x24FontSize,
         24, QFont::TypeWriter, true, QFont::Normal},
        {"adobeTimes18", kAdobeTimes18FontData, kAdobeTimes18FontSize, 25,
         QFont::Serif, false, QFont::Normal},
        {"adobeHelvetica24", kAdobeHelvetica24FontData,
         kAdobeHelvetica24FontSize, 34, QFont::SansSerif, false,
         QFont::Normal},
        {"adobeHelveticaBold24", kAdobeHelveticaBold24FontData,
         kAdobeHelveticaBold24FontSize, 34, QFont::SansSerif, false,
         QFont::Bold},
    };

    for (const FontSpec &spec : fontSpecs) {
      fonts.insert(QString::fromLatin1(spec.key), loadEmbeddedFont(spec.data,
          spec.size, spec.pixelSize, spec.styleHint, spec.fixedPitch,
          spec.weight, QFont::PreferBitmap));
    }
#endif

    struct FontAlias {
      const char *alias;
      const char *key;
    };

    const FontAlias fontAliases[] = {
        {"widgetDM_4", "miscFixed8"},
        {"widgetDM_6", "miscFixed8"},
        {"widgetDM_8", "miscFixed9"},
        {"widgetDM_10", "miscFixed10"},
        {"widgetDM_12", "miscFixed7x13"},
        {"widgetDM_14", "miscFixed7x14"},
        {"widgetDM_16", "miscFixed9x15"},
        {"widgetDM_18", "sonyFixed8x16"},
        {"widgetDM_20", "miscFixed10x20"},
        {"widgetDM_22", "sonyFixed12x24"},
        {"widgetDM_24", "sonyFixed12x24"},
        {"widgetDM_30", "adobeTimes18"},
        {"widgetDM_36", "adobeHelvetica24"},
        {"widgetDM_40", "adobeHelveticaBold24"},
        {"widgetDM_48", "adobeHelveticaBold24"},
        {"widgetDM_60", "adobeHelveticaBold24"},
    };

    for (const FontAlias &alias : fontAliases) {
      const QString key = QString::fromLatin1(alias.key);
      const QFont font = fonts.value(key);
      if (!font.family().isEmpty()) {
        fonts.insert(QString::fromLatin1(alias.alias), font);
      }
    }

    return fonts;
  }();

  return fonts;
}

} // namespace

namespace LegacyFonts {

const QHash<QString, QFont> &all()
{
  return fontCache();
}

QFont font(const QString &key)
{
  QHash<QString, QFont> &fonts = fontCache();
  const auto it = fonts.constFind(key);
  if (it != fonts.constEnd()) {
    return it.value();
  }

  int pixelSize = 0;
  if (isBitstreamCharterXLFD(key, &pixelSize)) {
    const QFont charter = loadBitstreamCharterBold(pixelSize);
    if (!charter.family().isEmpty()) {
      fonts.insert(key, charter);
      return charter;
    }
  }

  return QFont();
}

QFont fontOrDefault(const QString &key, const QFont &fallback)
{
  const QFont candidate = font(key);
  if (!candidate.family().isEmpty()) {
    return candidate;
  }
  return fallback;
}

} // namespace LegacyFonts
