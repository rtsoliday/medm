# Misc Fixed 6x10 Font

This directory packages the X11 bitmap font `-misc-fixed-medium-r-normal--10-100-75-75-c-60-iso8859-1` for use in QtEDM.

The source bitmap font comes from the [X.Org `font-misc-misc` collection](https://xorg.freedesktop.org/releases/individual/font/) which ships with the Debian `xfonts-base` package.  The upstream copyright notice for this font states:

> Public domain font. Share and enjoy.

The bitmap was converted to an OpenType bitmap (`.otb`) container using FontForge (20230101).  The generated file is embedded directly into the QtEDM executable so the UI renders with the original MEDM appearance on Linux, macOS, and Windows.
