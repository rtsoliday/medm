# Misc Fixed Bitmap Fonts

This directory packages the X11 bitmap fonts used to reproduce the legacy
MEDM appearance in QtEDM:

- `-misc-fixed-medium-r-normal--10-100-75-75-c-60-iso8859-1`
- `-misc-fixed-medium-r-normal--13-120-75-75-c-80-iso8859-1`

The source bitmap fonts come from the [X.Org `font-misc-misc` collection](https://xorg.freedesktop.org/releases/individual/font/) which ships with the Debian `xfonts-base` package.  The upstream copyright notice for these fonts states:

> Public domain font. Share and enjoy.

The bitmaps were converted to OpenType bitmap (`.otb`) containers using FontForge (20230101).  The generated files are embedded directly into the QtEDM executable so the UI renders with the original MEDM appearance on Linux, macOS, and Windows.
