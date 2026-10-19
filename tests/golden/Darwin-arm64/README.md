# Darwin arm64 visual references

These screenshots are the reviewed QtEDM visual references for Darwin arm64.
QtEDM uses native fonts on macOS because the legacy X11 bitmap-font containers
are not supported by the CoreText backend. Native glyph metrics differ from the
portable Linux bitmap-font references in the parent directory.

On 2026-10-19, the maintainer reviewed and approved the default Qt6 6.11.2
rendering and requested a refresh of these references. The reviewed layout
includes the intended extended slider trough geometry, also used by the
Thermometer fixture. This records approval of the Qt6 appearance; it does not
establish that every test passes or qualify Qt5 rendering. Qt5 font/style
differences require separate review and, if appropriate, a separate variant.

The canonical fixture PV prefix is `qtedm_visual:`. Visible channel labels are
part of the reference images, so a different prefix changes the comparison
input. The visual runner isolates its IOC with a private CA server port on
numeric loopback, binds server interfaces to `127.0.0.1`, and disables automatic
address discovery. The fixed prefix does not replace those isolation settings.

`qtedm_visual_tests.py` selects this directory automatically on Darwin arm64.
Set `QTEDM_VISUAL_GOLDEN_VARIANT` to select a different reviewed variant.
