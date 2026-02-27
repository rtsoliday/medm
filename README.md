# MEDM / QtEDM - Display Manager Suite

## QtEDM (Qt-based EDM)

QtEDM is the modern Qt implementation in this repository (`qtedm/`).
It uses EPICS Channel Access / PVAccess and reads the same `.adl` display
format used by MEDM.

### Linux Prerequisites

- Build tools: `gcc`, `g++`, `make`, `pkg-config`
- EPICS Base (required): place an `epics-base` checkout at the same directory
	level as this repository (for example, `../epics-base` when you are in
	`medm/`).
	
	Note: the build system also checks `../../epics-base` from subdirectories
	such as `qtedm/` and `medm/`; this is an internal relative path and can look
	confusing in error messages.
- Qt development packages (required):
	- Qt6 preferred (`Qt6Core`, `Qt6Widgets`, `Qt6PrintSupport`, `Qt6Svg`)
	- Qt5 fallback (`Qt5Core`, `Qt5Widgets`, `Qt5PrintSupport`, `Qt5Svg`)
	- `moc` and `rcc` must be available
- SDDS source tree (required for building shared support libraries used by QtEDM)

Typical Debian/Ubuntu packages:

```bash
sudo apt-get install build-essential pkg-config qt6-base-dev qt6-svg-dev
```

Optional for legacy MEDM build as well:

```bash
sudo apt-get install libmotif-dev libxmu-dev
```

### Build QtEDM

1. Clone dependencies next to this repository:

```bash
git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git
git clone https://github.com/rtsoliday/SDDS.git
```

2. Build EPICS Base and SDDS (see each project README for details).

3. From this repository root, build:

```bash
make
```

Notes:
- On Linux, `qtedm` can be built without Motif, but full top-level builds
	and support libraries may still require SDDS and/or prebuilt local libs.
- If Qt is missing, the top-level build exits with an error after dependency
	checks.

## MEDM (Motif/X11)

MEDM is the legacy Motif/X11 implementation in this repository (`medm/`).

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
