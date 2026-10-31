# Release notes

## QtEDM 1.1.0

Changes since QtEDM 1.0.0. `qtedm -version` and **Help > About QtEDM** report the release.

### Security

- **Shell Command macro check.** QtEDM refuses a Shell Command when a display macro value substituted into it could change what the shell runs, and shows a message explaining why. QtEDM records where each macro value was substituted, so macros the command does not use never block it, and each value is judged in its own quoting context. A quoted value may contain characters such as braces, parentheses, `#`, or `*`. An unquoted value is limited to letters, digits, spaces, and `_ . / : @ + = , - % ~`, plus braces when it contains no comma or `..`. PV names such as `SR:C01-BI{BPM:1}Pos:X-I` work quoted or unquoted. Option words such as `-e` are refused everywhere. The [Shell Command reference](../reference/widgets#WidgetShellCommand) gives the exact rules. Execute menu commands from `MEDM_EXEC_LIST` are not checked.
- **Dynamic Shell Command values.** On Unix, the `&A`, `&T`, and `&P` values QtEDM inserts into a Shell Command are quoted so the shell does not interpret them, and values containing control characters such as line breaks are refused.
- **Read-only mode.** `--read-only` now blocks Shell Commands and `MEDM_EXEC_LIST` Execute menu commands, as documented.
- **Literal macro values.** As in MEDM, a macro value is inserted literally into the display file. Backslash escapes in a value are no longer decoded, so `C:\temp` no longer becomes a tab, and a quote, brace, or space in a value can no longer end a string or add display elements.
- **Input limits.** Remote display requests are limited to 64 KiB; a `qtedm` that cannot send its request to a running QtEDM reports it and exits with status 1. Sessions are limited to 4 MiB and 100 windows. The animated GIF frame cache is limited to 64 MiB per image and 256 MiB in total.
- **Hardened Linux build.** The executable is built as a position-independent executable with full RELRO, immediate binding, a stack protector, and a non-executable stack. QtEDM no longer raises the core-dump size limit at startup.

### Behavior changes

- **Writes are shown when the IOC reports them.** After a Channel Access write, monitors of the PV no longer show the requested value before the IOC confirms it. If the IOC clamps, rejects, or overrides the write, the display keeps showing the IOC's value and alarm state. The control making the write still shows the value you set, as in MEDM.
- **Composite files inherit display macros.** A composite file without its own macros now resolves `$(P)` and similar references from the display's macros, as MEDM does. Macros given with the composite file name are added to the display's macros and take precedence. MEDM instead uses only the composite's own macros when it has any.
- **Choice buttons clip long labels** to the button instead of drawing past its edge.
- **Resource Palette** sizes itself to the screen, keeps the size you choose, and aligns its columns.
- **Saved numbers** use the shortest exact form where fixed notation would lose precision.
- **Symbol state colors** can be edited with an alpha channel in the editor. Saving stores the nearest palette color.
- **Build variables.** `CC`, `CXX`, `CPPFLAGS`, `CXXFLAGS`, and `LDLIBS` set in the environment no longer change the build. Pass them on the `make` command line instead.
- **SDDS** is no longer required to build QtEDM. A build check confirms SDDS6 export where SDDS is available.

### Fixes

- Legacy (pre-2.2) ADL files whose quoted values contain braces, such as `chan="SR:C01{BPM}"`, load correctly.
- A rejected setpoint edit no longer stays displayed as if it had been applied.
- Menus, related display buttons, and wave tables fit their labels and empty-state text to the widget.
- `make clean` works on Windows, and QtEDM compiles without warnings.

### Known issues

- Saving a display written by MEDM rewrites strings that contain backslashes, because QtEDM decodes escapes such as `\t` when it reads a file. Keep an original copy of MEDM displays whose labels or shell commands contain backslashes.
- Opening a display with unreachable `pva://` PVs pauses for about one second per PV.
