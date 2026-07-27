# Darwin arm64 visual references

These screenshots are the reviewed QtEDM visual references for Darwin arm64.
QtEDM uses native fonts on macOS because the legacy X11 bitmap-font containers
are not supported by the CoreText backend. Native glyph metrics differ from the
portable Linux bitmap-font references in the parent directory.

`qtedm_visual_tests.py` selects this directory automatically on Darwin arm64.
Set `QTEDM_VISUAL_GOLDEN_VARIANT` to select a different reviewed variant.
