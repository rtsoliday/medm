# Agent Instructions

This repository contains MEDM source code.

## Project Layout

This repository is organized into the following top-level files and directories:

- `AGENTS.md`: agent instructions and guidelines.
- `KNOWN_PROBLEMS.html`: historical bug notes and known limitations.
- `LICENSE`: licensing information.
- `Makefile`, `Makefile.build`, `Makefile.rules`: top-level build configuration.
- `README.1st`, `README.md`: getting-started documentation.
- `bin/`: prebuilt binaries grouped by host architecture.
- `lib/`: shared libraries grouped by host architecture.
- `medm/`: core MEDM source code.
- `printUtils/`: printing utility sources.
- `xc/`: additional source modules.

## Coding Conventions

- Prefer C for runtime code and keep compatibility with legacy Motif/EPICS dependencies; match surrounding language when touching existing files.
- Indent with two spaces; align wrapped arguments beneath the first argument, as in multi-line `XDrawLine` calls in `medm/medmRisingLine.c`.
- Place opening braces on the same line as control statements; for long function signatures, break before the brace but keep it immediately after the declaration block.
- Use uppercase `#define` names for compile-time constants and guard platform-specific sections with clear `#ifdef`/`#endif` blocks.
- Declare pointers with the asterisk adjacent to the variable name (e.g. `Channel *pCh`) and mark internal helpers `static` when possible.
- Retain the license banner at the top of C source files and use block comments (`/* ... */`) for multi-line descriptions; reserve single-line `/* comment */` for brief notes.
- Keep line length near 80 columns; when necessary, wrap expressions and continue with logical indentation instead of trailing backslashes.

## Build Expectations

- When proposing any change to the MEDM or QtEDM source code, rebuild both `medm` and `qtedm` to ensure the modifications compile cleanly.
- Prefer `make -j4` for faster parallel builds. Only switch back to `make` (single-threaded) if you need to track down a compiler warning or error, as sequential output is easier to diagnose.
- A full build can take **multiple minutes** to complete. Do not assume the build is stuck if it appears to hang on a single file—this is normal behavior. Wait for the build to finish before concluding there is an issue.

## Machine-managed coding quickstart
This supplement is generated from repository evidence and leaves the handwritten guidance above unchanged.

<!-- BEGIN MACHINE:summary -->
## Quick start
- Repository-local guidance is sufficient: start with `AGENTS.md`, `README.md`, `docs/`, build/test/config files, and the source tree.
- MEDM - Motif Editor and Display Manager
- Primary work areas: `.github`, `docs`, `medm`, `printUtils`, `qtedm`, `tests`.

## Read first
- `README.1st`: Primary project overview and workflow notes
- `README.md`: Primary project overview and workflow notes
- `medm/notes/README.FONTS`: Supporting documentation with operational details
- `medm/notes/Readme.txt`: Supporting documentation with operational details
- `medm/notes/README.WHITEPAGE`: Supporting documentation with operational details

## Build and test
- Documented setup/build commands: `make -j4`, `make`, `make these fonts available. If you have Netscape or Internet Explorer`.
- Detected build systems: GNU Make.
- Documented test commands: `make test-qtedm-cli`, `make test-qtedm-unit`, `make test-qtedm-ioc`, `make test-qtedm-visual`, `make test-qtedm`.
- Likely run commands or operator entry points: `level as this repository (for example, ../epics-base when you are in`, `python3 - <<'PY'`, `../tests/run_qtedm_visual_tests.sh`.

## Operational warnings
- Local checkout layout appears significant; avoid casual changes to sibling-repo assumptions or relative paths.
- Legacy compatibility paths are still present; confirm which mode is actually in use before changing defaults.
- Platform-specific dependency setup matters; do not assume one platform's build recipe carries over unchanged.

## Compatibility constraints
- Local builds appear to assume sibling checkouts or fixed relative paths.
- Legacy components are still present; prefer the documented default path before switching to older modes.
- Cross-platform support exists, but platform-specific dependency setup matters.
- Build and runtime behavior likely depends on neighboring core toolkit checkouts.

## Related knowledge
- Repository-local documentation should be treated as authoritative.
- If a shared `llm-wiki/` directory is present in this workspace or parent folder, consult [the matching repo page](../llm-wiki/repos/medm.md) for additional architectural context.
- If no shared wiki is present, continue using repository-local evidence only.
- If available, [the EPICS concept page](../llm-wiki/concepts/epics.md) adds broader cross-repo context.
- If present in this workspace, [the cross-repo map](../llm-wiki/insights/cross-repo-map.md) helps explain related repositories.
<!-- END MACHINE:summary -->
