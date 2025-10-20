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

In addition, the scalable Bitstream Charter Bold typeface (distributed under
the SIL Open Font License 1.1) is embedded as `bitstream-charter-bold.otf`.
QtEDM instantiates it at runtime when MEDM displays request the scalable
`-bitstream-charter-bold-r-normal` XLFD family.

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

2. Create the matching header that exposes a `k<FontName>FontData` array.  The
   existing headers were generated with a small Python helper that emits
   comma-separated hexadecimal bytes:

   ```sh
   python3 - <<'PY'
   from pathlib import Path
   data = Path('misc-fixed-8.otb').read_bytes()
   hex_bytes = [f"0x{b:02x}" for b in data]
   for i in range(0, len(hex_bytes), 12):
       print('    ' + ', '.join(hex_bytes[i:i+12]) + ',')
   PY
   ```

3. Wrap the byte list with the boilerplate used by the existing headers.
