# Agent Instructions

This repository contains MEDM source code.

## Project Layout

This repository is organized into the following top-level files and directories:

- `AGENTS.md`: agent instructions and guidelines.
- `LICENSE`: licensing information.
- `Makefile`, `Makefile.build`, `Makefile.rules`: top-level build configuration.
- `printUtils/`: source code.
- `xc/`: source code.
- `medm/`: source code.
- `snap/`: reference snapshots for EPICS PVs.
- `pv/`: PV files for ADT used to configure it's layout.

## Coding Conventions

- Prefer C for runtime code and keep compatibility with legacy Motif/EPICS dependencies; match surrounding language when touching existing files.
- Indent with two spaces; align wrapped arguments beneath the first argument, as in multi-line `XDrawLine` calls in `medm/medmRisingLine.c`.
- Place opening braces on the same line as control statements; for long function signatures, break before the brace but keep it immediately after the declaration block.
- Use uppercase `#define` names for compile-time constants and guard platform-specific sections with clear `#ifdef`/`#endif` blocks.
- Declare pointers with the asterisk adjacent to the variable name (e.g. `Channel *pCh`) and mark internal helpers `static` when possible.
- Retain the license banner at the top of C source files and use block comments (`/* ... */`) for multi-line descriptions; reserve single-line `/* comment */` for brief notes.
- Keep line length near 80 columns; when necessary, wrap expressions and continue with logical indentation instead of trailing backslashes.
