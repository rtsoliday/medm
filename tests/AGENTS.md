# Test Display Authoring Guide

This file documents the pattern used for `test_Slider.adl` and
`run_local_ioc.sh` so other widget test screens can follow the same approach.

## Goals

- Make MEDM vs QtEDM visual differences easy to spot.
- Make behavior differences reproducible in execute mode.
- Keep a deterministic local IOC setup for manual testing.

## ADL Layout Pattern

Use a two-section layout in each widget test screen.

- Section A: visual matrix
- Section B: behavior checks

### Section A (visual matrix)

- Build a compact matrix that varies one or more display attributes:
  - direction/orientation
  - label mode
  - color mode (`static`, `alarm`, `discrete` as applicable)
  - decoration style
- Include a wide range of widget sizes:
  - tiny (font stress)
  - medium (typical usage)
  - large (label/value clipping and scaling stress)
  - narrow vertical where relevant
- Add text headers/row labels so the intent of each row/column is explicit.

### Section B (behavior checks)

- Add text instructions directly in the ADL for manual execute-mode tests.
- Include at least:
  - writable reference widgets
  - disconnected/non-connected widget behavior where relevant
  - alarm-rendering probes where relevant
  - stress geometry widgets
- Add an explicit A/B baseline row where only one variable changes.
  - Example from slider screen: same channel, colors, limits, and precision;
    only `direction` differs.
  - This supports strict visual diffing and isolates regressions.

## Manual Test Text Conventions

Write short, action-oriented test instructions in the ADL, such as:

- drag/click behavior
- keyboard behavior
- right-click/context dialog behavior
- increment/step behavior
- clipping/overlap/alignment checks

For sliders specifically, call out:

- right-click dialog parity with MEDM in execute mode
- increment changes via preset choices and direct text entry
- no dialog open for disconnected/non-writable cases (match MEDM behavior)

## IOC Initialization Pattern (`run_local_ioc.sh`)

Use deterministic PV initialization before manual testing.

### Required CA environment

Before PV writes, set and export:

- `EPICS_CA_ADDR_LIST=localhost`
- `EPICS_CA_AUTO_ADDR_LIST=NO`

In script form, use a helper (`set_local_ca_env`) and call it before starting
the IOC and before `cavput` initialization loops.

### Normal test PV initialization

Use a retry loop (20 retries, small delay) to avoid startup races.

- Keep a per-widget list like:
  - `pv value lopr hopr prec`
- Write all fields in one `cavput -list=...` call when possible.
- Log a clear success/warning message.

### Alarm probe initialization

When you want a predictable alarm-color test without changing IOC internals:

- Use dedicated probe channels not used by normal value initialization.
  - Slider example: `weirdChan`, `ZzzButton`
- Set metadata fields only (`LOPR`, `HOPR`, `PREC`).
- Do not write the base value for those channels.
  - This keeps startup `UDF/INVALID` state available for alarm rendering tests.

## Running Without Cross-IOC Collisions

If another IOC may already be running, use a unique PV prefix:

`tests/run_local_ioc.sh --pv-prefix <unique>: ...`

This avoids collisions and makes test behavior repeatable.

## Applying This Pattern To Other Widgets

For each widget test file:

- Add Section A visual matrix with varied geometry and styles.
- Add Section B behavior checklist with explicit manual actions.
- Add one-variable-only A/B baseline row.
- Extend `run_local_ioc.sh` widget init arrays for values/limits/precision.
- Add dedicated alarm probe channels if alarm-style parity is important.
