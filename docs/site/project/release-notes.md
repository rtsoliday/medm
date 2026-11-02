# Release notes

## QtEDM 1.1.0

Changes since QtEDM 1.0.0. `qtedm -version` and **Help > About QtEDM** report the release.

### Security

- **Shell Command macro check.** QtEDM records where each display macro value was substituted into a Shell Command, checks each value in its own quoting context, and refuses the command with a message explaining why when a value breaks the rules. Macros the command does not use never block it. A quoted value may contain characters such as braces, parentheses, `#`, or `*`. An unquoted value is limited to letters, digits, spaces, and `_ . / : @ + = , - % ~`, plus braces when it contains no comma or `..`. PV names such as `SR:C01-BI{BPM:1}Pos:X-I` work quoted or unquoted. A value containing an option word such as `-e` is refused. Each value is checked on its own, so values joined to each other or to neighboring text can still form an option word or a brace expansion; do not join macros whose values you do not control in option positions. A command edited in the display editor, or rebuilt by undo, redo, or paste, uses a stricter check until the display is opened again. The [Shell Command reference](../reference/widgets#WidgetShellCommand) gives the exact rules and limits. Execute menu commands from `MEDM_EXEC_LIST` are not checked.
- **Dynamic Shell Command values.** On Unix, the `&A`, `&T`, and `&P` values QtEDM inserts into a Shell Command are quoted so the shell does not interpret them, and values containing control characters such as line breaks are refused.
- **Read-only mode.** `--read-only` now blocks Shell Commands and `MEDM_EXEC_LIST` Execute menu commands, as documented.
- **Literal macro values.** As in MEDM, a macro value is inserted literally into the display file. Backslash escapes in a value are no longer decoded, so `C:\temp` no longer becomes a tab, and a quote, brace, or space in a value can no longer end a string or add display elements.
- **Input limits.** Remote display requests are limited to 64 KiB; a `qtedm` that cannot send its request to a running QtEDM reports it and exits with status 1. Sessions are limited to 4 MiB and 100 windows. The animated GIF frame cache is limited to 64 MiB per image and 256 MiB in total.
- **Hardened Linux build.** The executable is built as a position-independent executable with full RELRO, immediate binding, a stack protector, and a non-executable stack. QtEDM no longer raises the core-dump size limit at startup.

### Behavior changes

- **Writes are shown when the IOC reports them.** After a Channel Access write, monitors of the PV no longer show the requested value before the IOC confirms it. If the IOC clamps, rejects, or overrides the write, the display keeps showing the IOC's value and alarm state. The control making the write shows the value you set until the IOC reports a value, then shows the IOC's value, as in MEDM, even when that value has not changed. If the IOC reports no value, the control keeps showing the value you set.
- **Composite files inherit display macros.** A composite file without its own macros now resolves `$(P)` and similar references from the display's macros, as MEDM does. Macros given with the composite file name are added to the display's macros and take precedence. MEDM instead uses only the composite's own macros when it has any.
- **Negative hexadecimal values** show as their low 32 bits, as in MEDM, so -2 shows as `0xfffffffe` instead of `-0x2`. Entering such text in a hexadecimal text entry, text area, or setpoint control writes the negative value to a short or long channel.
- **Choice buttons clip long labels** to the button instead of drawing past its edge.
- **Resource Palette** sizes itself to the screen, keeps the size you choose, and aligns its columns.
- **Saved numbers** use the shortest exact form where fixed notation would lose precision.
- **Symbol state colors** can be edited with an alpha channel in the editor. Saving stores the nearest palette color.
- **Plugin string-list properties** are entered in the Resource Palette as a JSON array, such as `["first", "second"]`, instead of semicolon-separated text. Invalid input restores the previous value.
- **NTNDArray image limits.** Maximum Input and Maximum Image are entered in bytes instead of MiB and are no longer capped at 4096 MiB. Maximum Dimension is no longer capped at 65536.
- **Build variables.** `CC`, `CXX`, `CPPFLAGS`, `CXXFLAGS`, and `LDLIBS` set in the environment no longer change the build. Pass them on the `make` command line instead. `LDFLAGS` set in the environment is still added to the link on Linux and Windows.
- **SDDS** is no longer required to build QtEDM. A build check confirms SDDS6 export where SDDS is available.

### Fixes

- Legacy (pre-2.2) ADL files whose quoted values contain braces, such as `chan="SR:C01{BPM}"`, load correctly.
- ADL files with an extra `}` between objects open as they do in MEDM instead of failing with "Expected section or key".
- A rejected setpoint edit no longer stays displayed as if it had been applied.
- A wheel switch whose value is outside its limits steps toward them by the step you choose, as in MEDM. It used to write the nearest limit, so one Down click on a value of 50 with limits 0 to 1 wrote 1. A step that would move the value further out of range does nothing.
- Menus, related display buttons, and wave tables fit their labels and empty-state text to the widget.
- Related display and shell command buttons show their icon and label as MEDM does: together, centered on a button with one item and at the left of a menu button, with the label's leading spaces kept. On a small button the label no longer covers the icon.
- Text 16 to 19 or 21 to 25 pixels high is no longer wider than in MEDM. The fonts for those heights had no space character, so spaces were drawn from a wider font and long labels could be cut off.
- Composites no longer cut off their contents in execute mode. A button, outline, or label that extends past its composite's stored size is drawn in full, as in MEDM, and a composite file's contents start at the composite's position instead of a few pixels away.
- `make clean` works on Windows, and QtEDM compiles without warnings.

### Known issues

- Saving a display written by MEDM rewrites strings that contain backslashes, because QtEDM decodes escapes such as `\t` when it reads a file. Keep an original copy of MEDM displays whose labels or shell commands contain backslashes.
- Opening a display with unreachable `pva://` PVs pauses for about one second per PV.
