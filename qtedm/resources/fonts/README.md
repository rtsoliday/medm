# Legacy MEDM Bitmap Fonts

This directory packages the X11 bitmap fonts that reproduce the original MEDM
appearance in QtEDM.  Each font is embedded directly into the executable so the
same glyphs are available on Linux, macOS, and Windows.

## Font inventory

The table below lists the resource key exposed through `LegacyFonts::all()` and
the X Logical Font Description (XLFD) that the data was taken from.  MEDM aliases
(such as `widgetDM_12`) map to these resource keys in `LegacyFonts::font()`.

| Resource key             | XLFD                                                                    |
| ------------------------ | ----------------------------------------------------------------------- |
| `miscFixed8`             | `-misc-fixed-medium-r-normal--8-60-100-100-c-50-iso8859-1`              |
| `miscFixed9`             | `-misc-fixed-medium-r-normal--9-80-100-100-c-60-iso8859-1`              |
| `miscFixed10`            | `-misc-fixed-medium-r-normal--10-100-75-75-c-60-iso8859-1`              |
| `miscFixed13`            | `-misc-fixed-medium-r-normal--13-120-75-75-c-80-iso8859-1`              |
| `miscFixed7x13`          | `-misc-fixed-medium-r-normal--13-100-100-100-c-70-iso8859-1`            |
| `miscFixed7x14`          | `-misc-fixed-medium-r-normal--14-110-100-100-c-70-iso8859-1`            |
| `miscFixed9x15`          | `-misc-fixed-medium-r-normal--15-120-100-100-c-90-iso8859-1`            |
| `sonyFixed8x16`          | `-sony-fixed-medium-r-normal--16-120-100-100-c-80-iso8859-1`            |
| `miscFixed10x20`         | `-misc-fixed-medium-r-normal--20-140-100-100-c-100-iso8859-1`           |
| `sonyFixed12x24`         | `-sony-fixed-medium-r-normal--24-170-100-100-c-120-iso8859-1`           |
| `adobeTimes18`           | `-adobe-times-medium-r-normal--25-180-100-100-p-125-iso8859-1`          |
| `adobeHelvetica24`       | `-adobe-helvetica-medium-r-normal--34-240-100-100-p-176-iso8859-1`      |
| `adobeHelveticaBold24`   | `-adobe-helvetica-bold-r-normal--34-240-100-100-p-182-iso8859-1`        |

In addition, the scalable XCharter Bold typeface is embedded as
`bitstream-charter-bold.otf`. XCharter is an extended Bitstream Charter face;
QtEDM instantiates it when MEDM displays request the scalable
`-bitstream-charter-bold-r-normal` XLFD family.

The OpenType file is XCharter 1.26 from the official CTAN package:
`https://ctan.org/tex-archive/fonts/xcharter`. Its SHA-256 is
`baf595556bbd65749a83b41034c900a8d7f8f5b2a1aa24314319501fdcacce9b`.
The XCharter and original Bitstream redistribution terms are retained in
`charter-license.txt`.

## Sources and regeneration

The bitmaps originate from the X.Org `font-misc-misc`, `font-sony-misc`, and
`font-adobe-100dpi` collections which ship with the Debian/Ubuntu
`xfonts-base`, `xfonts-75dpi`, and `xfonts-100dpi` packages.  The upstream fonts
are either in the public domain or distributed under permissive licences.

To regenerate an embedded font:

1. Convert the source PCF (or BDF) file to an OpenType bitmap container using
   FontForge:

   ```sh
   fontforge -lang=ff -c 'Open($1); Generate($2)' \
       /usr/share/fonts/X11/misc/5x8-ISO8859-1.pcf.gz misc-fixed-8.otb
   ```

2. Create the matching header that exposes a `k<FontName>FontData` array:

   ```sh
   python3 generate_font_header.py \
       bitstream-charter-bold.otf bitstream_charter_bold_otf.h \
       --symbol kBitstreamCharterBoldFont
   ```
