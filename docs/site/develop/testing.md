# Tests and contributions

Run the repository's existing QtEDM test targets from the repository root:

```sh
make test-qtedm-cli
make test-qtedm-unit
make test-qtedm-ioc
make test-qtedm-visual
```

Run all suites with `make test-qtedm`. The targets build QtEDM first; Qt development libraries are required. IOC and visual suites have their own EPICS and rendering requirements.

To test only the Help and About windows, run `make -C qtedm test-help`. It runs offscreen and saves screenshots (`help-topics.png`, `about.png`, and `about-beam.png`) to `test-artifacts/help/` in the QtEDM build directory, for example `qtedm/O.Linux-x86_64/`. The same checks also run in `make test-qtedm-unit`, without the screenshots.

The unit suite includes `test_memory_growth`, which checks PVA get-buffer reuse,
CA inbox overflow and ordered bursts, plugin diagnostic bounds, audit-viewer
limits and cleanup, and shared image-memory reservations without an IOC. Archive
request cleanup and image cancellation are covered by `test_archives_snapshots`
and `test_display_import_ndarray`. These are short regression tests; they do not
replace a multi-day soak test with representative displays and reconnects.

## Visual baselines

Visual regression goldens live in `tests/golden/`. Review differences before replacing a baseline. When a rendering change is intentional, the existing update command is:

```sh
tests/run_qtedm_visual_tests.sh --update-goldens
```

## Preparing a contribution

Follow the repository's `AGENTS.md` and preserve source-file license notices. Keep changes focused, update the affected documentation, and report your platform, Qt/EPICS versions, and test results. Changes to MEDM or QtEDM source require rebuilding both applications with `make -j4`.

For documentation-only changes, follow [Maintain these docs](./documentation). The reference manual is embedded in application help, so changes to it also need an application rebuild to refresh the embedded copy.
