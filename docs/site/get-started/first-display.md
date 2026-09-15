# Open your first display

Start with a built QtEDM executable and an existing `.adl` display. A live display also needs network access to its EPICS process variables.

## Open the editor

From the repository root on Linux:

```sh
bin/Linux-x86_64/qtedm
```

QtEDM starts in **EDIT** mode. Open an existing ADL file or create a display, then use the object and resource palettes to configure its objects. See [Edit and execute displays](../operate/modes) for the available operations.

## Monitor an existing display

```sh
bin/Linux-x86_64/qtedm -x path/to/display.adl
```

Replace `path/to/display.adl` with an actual display file. `-x` starts **EXECUTE** mode and connects its widgets to their configured channels. The screen should show the display's objects and live values for reachable PVs. If values are missing, check the configured PV names, protocol, and network connection.

## Supply macros and display paths

For a display that uses `$(P)` and `$(R)` substitutions:

```sh
bin/Linux-x86_64/qtedm -x -macro "P=SR:,R=Orbit" path/to/display.adl
```

Choose macro values that match your display and facility. `EPICS_DISPLAY_PATH` supplies directories for display lookup, separated by `:` on Unix or `;` on Windows. See [Environment variables](../reference/environment) and [Macros and dynamic behavior](../configure/features).

## Observe without writes

```sh
bin/Linux-x86_64/qtedm --read-only path/to/display.adl
```

`--read-only` starts execute mode with a persistent red indicator. Monitoring and navigation remain available while CA, PVA, soft-PV, snapshot-restore, and plugin-provider writes are blocked.

## Next steps

- [Find a widget](../reference/widgets) and its configuration fields.
- [Read the command-line reference](../reference/command-line) for additional startup options.
- [Work with sessions and snapshots](../operate/sessions) to restore a workspace or manage configured PV snapshots.
