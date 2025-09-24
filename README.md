# MEDM - Motif Editor and Display Manager

## Overview

MEDM is a graphical user interface for designing and operating control screens in EPICS-based control systems. Each display contains graphical objects that can present and/or modify EPICS process variables over Channel Access. Typical widgets include buttons, meters, sliders, text fields, and time plots.

The application runs in two modes:
- `EDIT` mode is used to create and modify display (.adl) files.
- `EXECUTE` mode renders a display for operators and connects the runtime widgets to process variables.

Additional background, release notes, and documentation are available on the EPICS Extensions site:
<http://www.aps.anl.gov/epics/extensions/medm/index.php>

## Getting Started

1. Unpack the MEDM extension archive into the `extensions/src` directory of an EPICS extensions tree created with `makeBaseExt.pl`.
2. Review the appropriate `CONFIG_SITE` file under `extensions/configure/os/` and ensure `MOTIF_LIB` and `X11_LIB` point to valid locations on your system (for example, `CONFIG_SITE.linux-x86_64.linux-x86_64` on 64-bit Linux).
3. Build MEDM from the top of the extensions tree using `make`. Refer to `Makefile`, `Makefile.build`, and `Makefile.rules` in this repository for details.

## Repository Layout

- `medm/`, `printUtils/`, `xc/`: Core source code.
- `pv/`: MEDM display PV configurations for ADT layouts.
- `snap/`: Reference snapshots of EPICS PVs for testing displays.
- `AGENTS.md`: Contributor guidance for automated agents.

## Licensing

MEDM is distributed under the terms described in the included `LICENSE` file.
