#include "legacy_fonts.h"

#include <QByteArray>
#include <QFontDatabase>
#include <QStringList>

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

namespace {

QFont loadEmbeddedFont(const unsigned char *data, std::size_t size,
    int pixelSize, QFont::StyleHint styleHint, bool fixedPitch,
    QFont::Weight weight)
{
  const int fontId = QFontDatabase::addApplicationFontFromData(QByteArray(
      reinterpret_cast<const char *>(data), static_cast<int>(size)));

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

  font.setStyleHint(styleHint, QFont::PreferBitmap);
  font.setStyleStrategy(QFont::PreferBitmap);
  font.setFixedPitch(fixedPitch);
  font.setPixelSize(pixelSize);
  font.setWeight(weight);
  font.setBold(weight >= QFont::DemiBold);
  return font;
}

} // namespace

namespace LegacyFonts {

const QHash<QString, QFont> &all()
{
  static const QHash<QString, QFont> fonts = [] {
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

    QHash<QString, QFont> fonts;
    for (const FontSpec &spec : fontSpecs) {
      fonts.insert(QString::fromLatin1(spec.key), loadEmbeddedFont(spec.data,
          spec.size, spec.pixelSize, spec.styleHint, spec.fixedPitch,
          spec.weight));
    }

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

QFont font(const QString &key)
{
  return all().value(key);
}

QFont fontOrDefault(const QString &key, const QFont &fallback)
{
  const QHash<QString, QFont> &fonts = all();
  if (fonts.contains(key)) {
    return fonts.value(key);
  }
  return fallback;
}

} // namespace LegacyFonts

