# Tests and contributions

Run the repository's existing QtEDM test targets from the repository root:

```sh
make test-qtedm-cli
make test-qtedm-unit
make test-qtedm-ioc
make test-qtedm-visual
```

Run all suites with `make test-qtedm`. The targets build QtEDM first; Qt development libraries are required. IOC and visual suites have their own EPICS and rendering requirements.

The CLI suite opens round-trip cases sequentially. On monitored hosts, add a
delay between QtEDM launches with
`make test-qtedm-cli QTEDM_CLI_TEST_ARGS='--launch-delay-seconds 5'`.
The delay must be finite and nonnegative; its default is zero. The comparison
regressions also run without launching QtEDM via
`python3 tests/testADL_SaveFiles.py --self-test`, and are included in both the
CLI and unit targets. They preserve quoted text and behavior-bearing names
while allowing file-header metadata and documented default-value differences.

To test only the Help and About windows, run `make -C qtedm test-help`. It runs offscreen and saves screenshots (`help-topics.png`, `about.png`, and `about-beam.png`) to `test-artifacts/help/` in the QtEDM build directory, for example `qtedm/O.Linux-x86_64/`. The same checks also run in `make test-qtedm-unit`, without the screenshots.

The unit suite includes `test_memory_growth`, which checks PVA get-buffer reuse,
CA inbox overflow and ordered bursts, plugin diagnostic bounds, audit-viewer
limits and cleanup, and shared image-memory reservations without an IOC. Archive
request cleanup and image cancellation are covered by `test_archives_snapshots`
and `test_display_import_ndarray`. These are short regression tests; they do not
replace a multi-day soak test with representative displays and reconnects.

On hosts that prohibit external command execution and native plugin fixtures,
run `QTEDM_TEST_RESTRICTED=1 make test-qtedm-unit`. This explicitly skips the
shell helper and GUI shell-command execution tests, native plugin discovery
and example-plugin tests, and application subprocess rendering tests. Static
shell-token construction, in-process plugin API, and the other unit tests
still run. Report those skips as NOT VERIFIED; this restricted run does not
replace `make test-qtedm` on an approved host. The default suite is unchanged.

For an aggregate run on a restricted host, select an absolute application
binary path whose adjacent `plugins` directory has been checked to contain no
native test fixtures. `QTEDM_TEST_APP_BIN` selects the application used by the
CLI, IOC, and visual recipes; by default it remains the binary in `OBJ_DIR`.
The unit binaries and image comparison tool still come from the build tree.
For example, from the repository root, substitute the installed binary path:

```sh
QTEDM_TEST_RESTRICTED=1 \
QTEDM_TEST_MANAGE_REPEATER=1 \
QTEDM_TEST_CA_REPEATER_BIN=/absolute/path/to/epics/bin/host-arch/caRepeater \
EPICS_CA_REPEATER_PORT=55365 \
make test-qtedm \
  QTEDM_TEST_APP_BIN=/absolute/path/to/medm/bin/Linux-x86_64/qtedm \
  QTEDM_CLI_TEST_ARGS='--launch-delay-seconds 5' \
  QTEDM_IOC_TEST_ARGS='--provider ca --launch-delay-seconds 5' \
  QTEDM_VISUAL_TEST_ARGS='--launch-delay-seconds 5'
```

`QTEDM_IOC_TEST_ARGS` and `QTEDM_VISUAL_TEST_ARGS` forward arguments to their
existing checked-in suite runners. This example skips the restricted unit
checks and PVA integration cases; report them as NOT VERIFIED. The visual
runner's finite, nonnegative `--launch-delay-seconds` option waits before
every actual QtEDM capture, including deterministic repeats; its default is
zero. It preserves the existing golden-image comparisons and never implies
permission to replace goldens. Fixture lifetimes include these requested
delays, and QtEDM captures remain sequential.

`QTEDM_TEST_MANAGE_REPEATER=1` opts into an owned CA repeater for each shell
runner and the unit suite. Set `QTEDM_TEST_CA_REPEATER_BIN` to an explicit
absolute path to the existing executable, and select an unused
`EPICS_CA_REPEATER_PORT` in the private range 49152 through 65535. The helper
exports the same value as `EPICS_CAS_BEACON_PORT`. It starts the executable
once, logs and registers its PID, waits one second, and refuses to start CA
tests if that child has exited. No executable or wrapper is generated. The
common cleanup trap stops and waits for the registered child and prints its
PID and wait status. Repeater output stays in the suite's temporary directory
until cleanup; startup errors are also copied to stderr. The unit recipe uses
bash only when this option is enabled so it can source the existing helper.
The default value is zero, which leaves the previous suite behavior intact.

## Optional IOC workload resource samples

The IOC runner accepts `--resource-log PATH` to append JSON Lines for each
sequential QtEDM child that it starts. For example, with the usual EPICS tool
environment configured for the IOC suite:

```sh
tests/run_qtedm_ioc_tests.sh --cases qtedm/O.Linux-x86_64/test-artifacts/workloads.json --resource-log qtedm/O.Linux-x86_64/test-artifacts/resources.jsonl --launch-delay-seconds 5
```

The same options are available when invoking `tests/qtedm_ioc_smoke.py`
directly with its required binary, helper, and case-manifest arguments. Keep
artifacts under the active build directory. `--launch-delay-seconds` is a
finite, nonnegative delay between completed cases; its default is zero.

The direct Python runner also accepts `--provider ca` or `--provider pva`.
This filters the manifest after any `--case` selections and before validating
the selected fixture requirements. The default runs both providers. An empty
selection is an error. In particular, `--provider ca` runs the CA cases
without starting the PVA fixture or requiring the optional PVA tool arguments.
`run_local_ioc.sh` prints `Started local IOC PID: <pid>` and
`Local IOC cleanup completed PID: <pid>` to its runner log, so cleanup can be
checked using only the PID of the IOC that this run started.

Resource records contain the case name, the owned QtEDM PID, elapsed seconds,
the launch command, and the output of
`ps -o pid,rss,vsz,etime,cmd -p <owned-pid>`. Sampling begins after launch and
continues about every five seconds while that child runs, including during
readiness waits and IOC restarts. On Linux, records also include that child's
`/proc/<pid>/status` and a count of entries in `/proc/<pid>/fd`. Linux FD and
status samples are unavailable on macOS. Failed or unavailable probes are
recorded as errors; they are not measurements. These samples do not measure
CPU usage. The exit record retains the complete captured stdout and stderr,
including built-in `TRACK_MEM` output when that build enables it. Only the
current child is sampled, and the runner drains both output pipes throughout
the run. Without `--resource-log`, the normal suite behavior is unchanged.

For a disconnected CA workload, set `"use_ioc": false` in its case object.
The default is `true`. The runner defers CA IOC startup until a case needs it,
and stops its own CA fixture before a disconnected case. Such a case cannot
use `pre_writes`, `writes`, `verify_pvs`, or `restart_provider`. This option is
CA-only; PVA cases continue to use the PVA fixture. Display rewriting still
assigns the run's unique PV prefix, and the suite's loopback CA/PVA environment
still applies. Use the existing `exit_after_ms`, `actions`, selectors, and
expectations to describe bounded workloads, with one QtEDM child at a time.

## Visual baselines

The visual runner normally stops at the first failure. To collect every
selected case's result using the same owned IOC, run
`make test-qtedm-visual QTEDM_VISUAL_TEST_ARGS='--keep-going --launch-delay-seconds 5'`.
`--keep-going` preserves the screenshots, diffs and IOC logs if any case fails,
prints each result immediately, and reports final pass/fail/not-run counts.
The command still exits nonzero when any case fails. It keeps all existing
assertions, tolerances, golden comparisons and PV-prefix rules; it does not
restart the IOC for each case or authorize updating goldens.

Some goldens show channel names with the canonical `qtedm_visual:` prefix.
The runner also isolates the IOC with a private CA server port. If a host
requires a unique `QTEDM_VISUAL_PV_PREFIX`, rendered labels can differ from
those goldens even when the application is behaving correctly. Preserve and
review those differences as test-input limitations; changing the goldens is
not a way to validate a run with different inputs.

Visual regression goldens live in `tests/golden/`. Review differences before replacing a baseline. When a rendering change is intentional, the existing update command is:

```sh
tests/run_qtedm_visual_tests.sh --update-goldens
```

## Preparing a contribution

Follow the repository's `AGENTS.md` and preserve source-file license notices. Keep changes focused, update the affected documentation, and report your platform, Qt/EPICS versions, and test results. Changes to MEDM or QtEDM source require rebuilding both applications with `make -j4`.

For documentation-only changes, follow [Maintain these docs](./documentation). The reference manual is embedded in application help, so changes to it also need an application rebuild to refresh the embedded copy.
