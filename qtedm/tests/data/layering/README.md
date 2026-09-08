# Steering label regression

`steering_label.adl` isolates the missing `L2:SC3` label observed in
`liPmWithSteering.adl`. The arrow and following label retain their original
declaration order, sizes, colors, and x coordinates; y coordinates are shifted
up by 420 pixels. An identical reference label is declared before the arrow.
There are no PVs, external displays, or IOC dependencies.

The single backslash before the arrow's closing quote is intentional. Do not
escape it again or regenerate this fixture with QtEDM's writer: that would
change the input which triggers the MEDM compatibility problem.

MEDM renders both labels and the literal arrow. This was checked on Linux with
MEDM 3.1.22: the label regions `(760,19,70,20)` and `(760,59,70,20)` were pixel
identical, and each contained 206 dark pixels. The fixture can also be opened
directly in either application for visual comparison.

`test_layering` checks both the parsed objects and the execute-mode screenshot
captured through the built QtEDM application's supported test CLI.
The image assertion compares the affected label against the unaffected label
in the same capture, avoiding platform-specific font baselines. A separate
check requires visible text in the reference so two blank regions cannot pass.
The capture is saved as `steering-label-actual.png` in the test binary's build
directory for diagnosis. A positive-control data row changes only the arrow's
backslash to a vertical bar in a temporary ADL copy and must pass the same
visual assertion. Its capture is saved as `steering-label-control.png`.

Run from the repository root on Linux:

```sh
make -C qtedm -j4 O.Linux-x86_64/test_layering
QT_QPA_PLATFORM=offscreen QTEDM_NOLOG=1 qtedm/O.Linux-x86_64/test_layering
```

The test is also registered in `make test-qtedm-unit`. It covers the parser compatibility fix for a literal
backslash before a closing quote. The parser assertion distinguishes label loss during
parsing from later rendering/stacking failures.

The fixture also includes the shaft and lower `\/` arrowhead at their original
relative positions. A separate arrowhead uses an escaped backslash as the
rendering reference; both lower arrowheads must render identically. This
checks that the parser preserves MEDM's literal backslash before a slash.

## Hidden related display under status graphics

`hidden_related_composite.adl` preserves the FS#10 composite from
`FS_ctrl.adl`: an invisible related display followed by a rectangle, a nested
composite of dynamic status rectangles, and a text label. The target is
`linac/xxlinacflags.adl` with the original FS10 macros. Neither that external
file nor connected PVs is needed for the regression test.

`TestObserveOnlyControls::hiddenRelatedDisplayUnderGraphicComposite` loads
this fixture, resolves the real Qt mouse target at the label, and verifies
that clicking activates the related display entry. It substitutes a callback
to avoid opening the unavailable target and repeats after an edit/execute mode
cycle. Before the fix, the graphics-only nested composite intercepted the
click. The outer composite must remain interactive to preserve its button.

Run from `qtedm/` after building the test binary:

```sh
QT_QPA_PLATFORM=offscreen QTEDM_NOLOG=1 O.Linux-x86_64/test_observe_only_controls hiddenRelatedDisplayUnderGraphicComposite
```
