#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IOC_BIN="${SCRIPT_DIR}/sddsSoftIOC"
CAVPUT_BIN="${SCRIPT_DIR}/cavput"
SDDS_FILE="${SCRIPT_DIR}/sddsSoftIOC_tests.sdds"
LOG_FILE="${SCRIPT_DIR}/sddsSoftIOC.log"
PV_PREFIX=""
EXECUTION_TIME="0"
REGENERATE="0"
STANDALONE="1"

usage() {
  cat <<'EOF'
Usage: run_local_ioc.sh [options]

Run tests/sddsSoftIOC with a local SDDS file and controlled lifecycle.

Options:
  --sdds-file <path>       SDDS definition file to load
  --log-file <path>        Log file path (default: tests/sddsSoftIOC.log)
  --pv-prefix <prefix>     Prefix added to served PVs (default: none)
  --execution-time <sec>   IOC runtime in seconds, 0 means forever
  --regenerate             Regenerate SDDS file from tests/*.adl first
  --no-standalone          Do not pass -standalone to sddsSoftIOC
  --help                   Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sdds-file)
      [[ $# -ge 2 ]] || {
        echo "Missing value for $1" >&2
        exit 1
      }
      SDDS_FILE="$2"
      shift 2
      ;;
    --log-file)
      [[ $# -ge 2 ]] || {
        echo "Missing value for $1" >&2
        exit 1
      }
      LOG_FILE="$2"
      shift 2
      ;;
    --pv-prefix)
      [[ $# -ge 2 ]] || {
        echo "Missing value for $1" >&2
        exit 1
      }
      PV_PREFIX="$2"
      shift 2
      ;;
    --execution-time)
      [[ $# -ge 2 ]] || {
        echo "Missing value for $1" >&2
        exit 1
      }
      EXECUTION_TIME="$2"
      shift 2
      ;;
    --regenerate)
      REGENERATE="1"
      shift
      ;;
    --no-standalone)
      STANDALONE="0"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ ! -x "${IOC_BIN}" ]]; then
  echo "sddsSoftIOC binary not found or not executable: ${IOC_BIN}" >&2
  exit 1
fi
if [[ ! -x "${CAVPUT_BIN}" ]]; then
  echo "cavput binary not found or not executable: ${CAVPUT_BIN}" >&2
  exit 1
fi

if [[ "${REGENERATE}" == "1" ]]; then
  "${SCRIPT_DIR}/generate_sdds_from_adl.sh" --output "${SDDS_FILE}"
fi

if [[ ! -f "${SDDS_FILE}" ]]; then
  echo "SDDS file not found: ${SDDS_FILE}" >&2
  exit 1
fi

mkdir -p "$(dirname "${LOG_FILE}")"

cmd=(
  "${IOC_BIN}"
  "${SDDS_FILE}"
  "-executionTime=${EXECUTION_TIME}"
)
if [[ -n "${PV_PREFIX}" ]]; then
  cmd+=("-pvPrefix=${PV_PREFIX}")
fi
if [[ "${STANDALONE}" == "1" ]]; then
  cmd+=("-standalone")
fi

echo "Starting IOC:"
printf '  %q' "${cmd[@]}"
echo
echo "Logging to ${LOG_FILE}"

ioc_pid=""

set_local_ca_env() {
  EPICS_CA_ADDR_LIST=localhost
  EPICS_CA_AUTO_ADDR_LIST=NO
  export EPICS_CA_ADDR_LIST EPICS_CA_AUTO_ADDR_LIST
}

set_slider_test_pvs() {
  local prefix="$1"
  local -a slider_init_values=(
    # pv value lopr hopr prec
    "slider:test:alpha   24.5  -12.5   84.3   2"
    "slider:test:beta   -35    -95     5      1"
    "slider:test:gamma   180     0     360     3"
    "slider:test:delta    60   -240.8 240.8    1"
    "slider:test:epsilon   1.8   -6.2   6.2    2"
    "slider:test:zeta    120    10.5  310.5    1"
    "slider:test:eta       0.3   -1.2   1.2    4"
    "slider:test:theta    35    -42.7 142.7    2"
    "slider:test:iota     10    -52.7  52.7    1"
    "slider:test:kappa   220     10    510     0"
    "slider:test:lambda   75      0    150     2"
    "slider:test:mu      100   -220.5 420.5    1"
    "slider:test:nu       40    -54.2 154.2    2"
    "slider:test:xi        5    -17.5  17.5    1"
    "slider:test:omicron  22     -8.9  58.9    3"
    "slider:test:pi      210      0    500     0"
    "slider:test:rho      15      0     42     2"
    "slider:test:sigma    12     -2.4   24     3"
    "slider:test:tau      55   -160    160     2"
    "slider:test:upsilon  42      0    100     1"
  )

  echo "Initializing slider PVs for tests/test_Slider.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${slider_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Slider PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize slider PV values after ${retries} retries." >&2
  return 1
}

set_scale_monitor_test_pvs() {
  local prefix="$1"
  local -a scale_init_values=(
    # pv value lopr hopr prec
    "scale:test:alpha    12.4   -63.5   78.2   1"
    "scale:test:beta     64.2   -12.7  125.9   2"
    "scale:test:gamma    17.375  -5.4   97.3   3"
    "scale:test:delta    -3.125  -8.4    8.4   3"
    "scale:test:epsilon  21.7   -48.9   38.6   1"
    "scale:test:zeta     44.125 -32.6   58.8   4"
    "scale:test:eta      -7.25  -28.1   14.7   2"
    "scale:test:theta    11.2  -101.4   64.3   1"
    "scale:test:iota      2.75   -5.5    5.5   2"
    "scale:test:kappa   150.5  -250    250     2"
  )

  echo "Initializing scale monitor PVs for tests/test_ScaleMonitor.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${scale_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Scale monitor PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize scale monitor PV values after ${retries} retries." >&2
  return 1
}

set_meter_test_pvs() {
  local prefix="$1"
  local -a meter_init_values=(
    # pv value lopr hopr prec
    "meter:fast:alpha      24.5    -14.3    92.7   2"
    "meter:slow:beta       64.2    -85.6   125.4   1"
    "meter:burst:gamma      1.275   -3.8     3.8   3"
    "meter:demo:delta     -12      -47      47     2"
    "meter:cycle:epsilon   40.8     -5.5    68.9   1"
    "meter:drift:zeta     -22.3   -120.6    15.4   1"
    "meter:ramp:eta         2.4     -9.2     9.2   2"
    "meter:noise:theta     56.2    -32.1   105.6   3"
    "meter:pulse:iota       0.375   -1.5     1.5   4"
    "meter:wave:kappa     150     -250     250     0"
    "meter:scan:lambda    -22.5    -75.3    12.9   2"
    "meter:burst:mu        21.1     -3.1    42.2   1"
    "meter:flux:nu          2.125   -0.75    8.35  3"
    "meter:loop:xi         -1.25   -11       8.35  5"
    "meter:random:omicron   1.875  -11       8.35  5"
  )

  echo "Initializing meter PVs for tests/test_Meter.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${meter_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Meter PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize meter PV values after ${retries} retries." >&2
  return 1
}

set_strip_chart_test_pvs() {
  local prefix="$1"
  local -a strip_chart_init_values=(
    # pv value lopr hopr prec
    "strip:test:alpha       10     -20     60   1"
    "strip:test:beta        35     -10    100   0"
    "strip:test:gamma       -5     -50     50   2"
    "strip:test:delta      120       0    200   0"
    "strip:test:epsilon     40     -25    125   1"
    "strip:test:zeta       -12     -80     40   1"
    "strip:test:eta        2.5      -5      5   2"
    "strip:test:theta      0.5      -1      1   3"
    "strip:test:iota         3      -2      8   2"
    "strip:test:kappa       75       0    150   1"
    "strip:test:lambda     -30    -100     20   0"
    "strip:test:mu          15       0     30   1"
    "strip:test:drive:a      0     -10     10   2"
    "strip:test:drive:b      5     -20     20   2"
    "strip:test:drive:c      8     -30     30   1"
    "strip:test:stress:a    55       0    100   1"
    "strip:test:stress:b   -15     -40     40   1"
  )

  echo "Initializing strip chart PVs for tests/test_StripChart.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${strip_chart_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Strip chart PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize strip chart PV values after ${retries} retries." >&2
  return 1
}

set_cartesian_plot_test_pvs() {
  local prefix="$1"
  local -a cartesian_plot_init_values=(
    # pv value lopr hopr prec
    "cartesian:test:alpha:y1         4.75    0.01     160   3"
    "cartesian:test:alpha:count    120         1      200   0"

    "cartesian:test:beta:y1          8.25    -80      160   3"
    "cartesian:test:beta:count     100         1      200   0"

    "cartesian:test:gamma:y1         4.20    -10       10   3"
    "cartesian:test:gamma:count    140         1      200   0"

    "cartesian:test:delta:y1         6.35    -80      160   3"
    "cartesian:test:delta:count    160         1      200   0"

    "cartesian:test:epsilon:y1      12.50    0.01     950   3"
    "cartesian:test:epsilon:count  110         1      200   0"

    "cartesian:test:zeta:y1         18.25    -45    155.5   3"
    "cartesian:test:zeta:count     180         1      200   0"

    "cartesian:test:baselinea:y1     4.20    -10       10   3"
    "cartesian:test:baselinea:count 130        1      200   0"

    "cartesian:test:baselineb:y1     4.20    -10       10   3"
    "cartesian:test:baselineb:count 130        1      200   0"

    "cartesian:test:probe:y1         6.75    -80      160   3"
    "cartesian:test:probe:count    150         1      200   0"
  )

  echo "Initializing cartesian plot PVs for tests/test_CartesianPlot.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${cartesian_plot_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Cartesian plot PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize cartesian plot PV values after ${retries} retries." >&2
  return 1
}

set_bar_test_pvs() {
  local prefix="$1"
  local -a bar_init_values=(
    # pv value lopr hopr prec
    "bar:test:alpha      24.5      -58.4    139.7   1"
    "bar:test:alphaa    -12.75     -58.4    139.7   1"
    "bar:test:beta       18.2      -12.8     64.5   2"
    "bar:test:gamma     -33.1      -95.3     12.4   1"
    "bar:test:delta      42.6      -18.6     98.2   2"
    "bar:test:epsilon     7.125    -42.1     42.1   3"
    "bar:test:zeta       32.4       -7.3     73.9   2"
    "bar:test:eta        -6.4      -33.5     55.7   1"
    "bar:test:theta    -101.7     -120.9      4.3   1"
    "bar:test:iota       55.5       -8.8     88.8   2"
    "bar:test:kappa      -9.875    -64.2     24.6   3"
    "bar:test:lambda     88.2      -15.4    115.4   2"
    "bar:test:mu         11.5       -2.5     65.1   1"
    "bar:test:limit_source 25.0      0.001    50.0   2"
    "test_bar_pv          1.4       -2.0      3.0   1"
  )
  local -a bar_choice_button_values=(
    # pv value (enum channels used by Section B quick probes)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing bar monitor PVs for tests/test_Bar.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${bar_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${bar_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Bar monitor PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize bar monitor PV values after ${retries} retries." >&2
  return 1
}

set_byte_test_pvs() {
  local prefix="$1"
  local -a byte_init_values=(
    # pv value lopr hopr prec
    "byte:test:gamma01      2730         0   2147483647  0"
    "byte:test:gamma02   16711935        0   2147483647  0"
    "byte:test:gamma03 2147418112        0   2147483647  0"
    "byte:test:gamma04       170         0   2147483647  0"
    "byte:test:gamma05      1024         0   2147483647  0"
    "byte:test:gamma06   1048575         0   2147483647  0"
    "byte:test:gamma07    262143         0   2147483647  0"
    "byte:test:gamma08 2139095040        0   2147483647  0"
  )

  echo "Initializing byte PVs for tests/test_Byte.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${byte_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Byte PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize byte PV values after ${retries} retries." >&2
  return 1
}

set_arc_test_pvs() {
  local prefix="$1"
  local -a arc_init_values=(
    # pv value lopr hopr prec
    "channelA_PV   1.25   -2      3      2"
    "channelB_PV  -0.75   -5      5      2"
    "channelC_PV  12      -45.5  45.5    1"
    "channelD_PV -22      -45.5  45.5    1"
  )
  local -a arc_choice_button_values=(
    # pv value (enum channels used by Section A and Section B visibility choice buttons)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing arc PVs for tests/test_Arc.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${arc_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${arc_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Arc PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize arc PV values after ${retries} retries." >&2
  return 1
}

set_line_test_pvs() {
  local prefix="$1"
  local -a line_init_values=(
    # pv value lopr hopr prec
    "channelA_PV   1.25   -2      3      2"
    "channelB_PV  -0.75   -5      5      2"
    "channelC_PV  12      -45.5  45.5    1"
    "channelD_PV -22      -45.5  45.5    1"
  )
  local -a line_choice_button_values=(
    # pv value (enum channels used by Section A and Section B visibility choice buttons)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing line PVs for tests/test_Line.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${line_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${line_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Line PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize line PV values after ${retries} retries." >&2
  return 1
}

set_menu_test_pvs() {
  local prefix="$1"
  local -a menu_init_values=(
    # pv value (enum channels used by tests/test_Menu.adl)
    "mn:ctrl:8127 1"
    "mn:ops:9365 2"
    "mn:data:4710 3"
    "mn:diag:2583 0"
    "mn:scan:3419 1"
    "mn:test:7841 2"
    "mn:util:5092 3"
    "mn:fast:6730 0"
    "mn:slow:9126 1"
    "mn:macro:1824 2"
    "mn:aux:3579 3"
    "mn:proto:6687 0"
    "mn:eng:2950 1"
    "mn:logic:8243 2"
    "mn:bench:5775 3"
    "mn:pilot:9468 0"
  )
  local -a menu_choice_button_values=(
    # pv value (shared enum channels used by Section B quick probes)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing menu PVs for tests/test_Menu.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value full_pv
    for entry in "${menu_init_values[@]}"; do
      read -r pv value <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" "-list=${full_pv}=${value}" >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${menu_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Menu PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize menu PV values after ${retries} retries." >&2
  return 1
}

set_choice_button_test_pvs() {
  local prefix="$1"
  local -a choice_button_init_values=(
    # pv value (enum channels used by tests/test_ChoiceButton.adl)
    "cb:test:4193     0"
    "cb:sample:9827   1"
    "cb:aux:1574      2"
    "cb:ops:6410      3"
    "cb:ctrl:2845     1"
    "cb:logic:7319    0"
    "cb:diag:5930     2"
    "cb:data:8754     3"
    "cb:eng:1148      1"
    "cb:util:7056     2"
    "cb:fast:3472     0"
    "cb:slow:9210     3"
    "cb:macro:2681    1"
    "cb:scan:8095     2"
    "cb:testbed:5524  0"
    "cb:proto:6678    3"
  )

  echo "Initializing choice button PVs for tests/test_ChoiceButton.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value full_pv
    for entry in "${choice_button_init_values[@]}"; do
      read -r pv value <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" "-list=${full_pv}=${value}" >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Choice button PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize choice button PV values after ${retries} retries." >&2
  return 1
}

set_message_button_test_pvs() {
  local prefix="$1"
  local -a message_button_init_values=(
    # pv value lopr hopr prec
    "mb:stat:01      0   -20    20   1"
    "mb:stat:02      0   -20    20   1"
    "mb:stat:03      0   -20    20   1"
    "mb:stat:04      1   -20    20   1"
    "mb:stat:05      2   -20    20   1"
    "mb:alarm:01     0   -20    20   1"
    "mb:alarm:02     0   -20    20   1"
    "mb:alarm:03    -1   -20    20   1"
    "mb:alarm:04     1   -20    20   1"
    "mb:alarm:05     2   -20    20   1"
    "mb:disc:01      0   -20    20   1"
    "mb:disc:02      1   -20    20   1"
    "mb:disc:03      2   -20    20   1"
    "mb:disc:04      3   -20    20   1"
    "mb:disc:05      0   -20    20   1"
    "mb:alt:01       0   -20    20   1"
    "mb:alt:02       0   -20    20   1"
    "mb:alt:03      -1   -20    20   1"
    "mb:alt:04       1   -20    20   1"
    "mb:alt:05       2   -20    20   1"
    "mb:base:ab      0   -20    20   1"
    "mb:beh:press    0   -20    20   1"
    "mb:beh:full     0   -20    20   1"
  )

  echo "Initializing message button PVs for tests/test_MessageButton.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${message_button_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Message button PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize message button PV values after ${retries} retries." >&2
  return 1
}

set_image_test_pvs() {
  local prefix="$1"
  local -a image_init_values=(
    # pv value lopr hopr prec
    "img:vis:a    1   -5    5   1"
    "img:vis:b    0   -5    5   1"
  )
  local -a image_choice_button_values=(
    # pv value (enum channels used by Section B quick probes)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing image PVs for tests/test_Image.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${image_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${image_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Image PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize image PV values after ${retries} retries." >&2
  return 1
}

set_rectangle_test_pvs() {
  local prefix="$1"
  local -a rectangle_init_values=(
    # pv value lopr hopr prec
    "channelA_PV   1.25   -2      3      2"
    "channelB_PV  -0.75   -5      5      2"
    "channelC_PV  12      -45.5  45.5    1"
    "channelD_PV -22      -45.5  45.5    1"
  )
  local -a rectangle_choice_button_values=(
    # pv value (enum channels used by Section A and Section B visibility choice buttons)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing rectangle PVs for tests/test_rectangle.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${rectangle_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${rectangle_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Rectangle PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize rectangle PV values after ${retries} retries." >&2
  return 1
}

set_polygon_test_pvs() {
  local prefix="$1"
  local -a polygon_init_values=(
    # pv value lopr hopr prec
    "channelA_PV   1.25   -2      3      2"
    "channelB_PV  -0.75   -5      5      2"
    "channelC_PV  12      -45.5  45.5    1"
    "channelD_PV -22      -45.5  45.5    1"
  )

  echo "Initializing polygon PVs for tests/test_Polygon.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${polygon_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Polygon PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize polygon PV values after ${retries} retries." >&2
  return 1
}

set_oval_test_pvs() {
  local prefix="$1"
  local -a oval_init_values=(
    # pv value lopr hopr prec
    "channelA_PV   1.25   -2      3      2"
    "channelB_PV  -0.75   -5      5      2"
    "channelC_PV  12      -45.5  45.5    1"
    "channelD_PV -22      -45.5  45.5    1"
  )
  local -a oval_choice_button_values=(
    # pv value (enum channels used by Section A and Section B visibility choice buttons)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing oval PVs for tests/test_Oval.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${oval_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${oval_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Oval PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize oval PV values after ${retries} retries." >&2
  return 1
}

set_text_test_pvs() {
  local prefix="$1"
  local -a text_init_values=(
    # pv value lopr hopr prec
    "channelA_PV   1.25   -2      3      2"
    "channelB_PV  -0.75   -5      5      2"
    "channelC_PV  12      -45.5  45.5    1"
    "channelD_PV -22      -45.5  45.5    1"
  )
  local -a text_choice_button_values=(
    # pv value (enum channels used by Section A and Section B visibility choice buttons)
    "cb:ctrl:2845 1"
    "cb:logic:7319 0"
  )

  echo "Initializing text PVs for tests/test_Text.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${text_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      local enum_entry enum_pv enum_value
      for enum_entry in "${text_choice_button_values[@]}"; do
        read -r enum_pv enum_value <<< "${enum_entry}"
        full_pv="${prefix}${enum_pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${enum_value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Text PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize text PV values after ${retries} retries." >&2
  return 1
}

set_text_fonts_test_pvs() {
  local prefix="$1"

  # tests/test_TextFonts.adl is intentionally static text-only (no PV channels).
  echo "Text fonts screen tests/test_TextFonts.adl uses static text only; no PV initialization required."
  : "${prefix}"
  return 0
}

set_text_entry_test_pvs() {
  local prefix="$1"
  local -a text_entry_numeric_values=(
    # pv value lopr hopr prec
    "tm:compact:1          12.345      -72.3      18.6      3"
    "tm:decimal:slow        0.000031   -0.0001     0.0001   6"
    "tm:decimal:telemetry 2048           0       4096       1"
    "tm:engr:mix            0.0025      -0.01       0.01    6"
    "tm:exp:fastA           0.000042    -0.008      0.009   6"
    "tm:hex:count         1023        -255       4095       0"
    "tm:octal:plc          511           0       1024       0"
    "tm:sexagesimal:az      47.5       -90         90       1"
    "tm:sexagesimal:dms    -21.345    -180        180       3"
    "tm:sexagesimal:hms   4321          -0.5     7200       2"
    "tm:trunc:edge        9876       -1024       1024       0"
    "channelA_PV            1.25        -2          3       2"
    "channelB_PV           -0.75        -5          5       2"
    "channelC_PV           12          -45.5       45.5     1"
    "channelD_PV          -22          -45.5       45.5     1"
  )
  local -a text_entry_string_values=(
    # pv value
    "tm:string:status READY"
  )

  echo "Initializing text/text monitor PVs for tests/test_TextEntry.adl and tests/test_TextMonitor.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${text_entry_numeric_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      for entry in "${text_entry_string_values[@]}"; do
        read -r pv value <<< "${entry}"
        full_pv="${prefix}${pv}"
        if ! "${CAVPUT_BIN}" "-list=${full_pv}=${value}" >/dev/null 2>&1; then
          initialized=0
          break
        fi
      done
    fi
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Text/text monitor PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize text/text monitor PV values after ${retries} retries." >&2
  return 1
}

set_wheel_switch_test_pvs() {
  local prefix="$1"
  local -a wheel_switch_init_values=(
    # pv value lopr hopr prec
    "wheel:test:A1          42.75     -17.25    103.5    2"
    "wheel:test:B7          12.3      -50        47.75   1"
    "wheel:test:delta       -4.25      -9         9      2"
    "wheel:test:eta          3.125     -1.25      6.75   3"
    "wheel:test:kappa      -12.345    -55.75     19.125  3"
    "wheel:theta            12.875   -150       150      3"
    "wheel:phi              85.2     -200       250      1"
    "ws:gamma15              1.2345    -3.5       3.5    4"
    "ws:zeta               -10.5      -80        80      2"
    "ws:theta9               0.045     -0.005     0.125  4"
    "ws:lambda              -0.25      -2         0      3"
    "ws:mu                   1.4       -2         2      2"
    "ws:nu                -120       -360       360      1"
    "ws:xi                 321.09     -99.99    999.99   2"
    "random:ws:epsilon      77       -120       220      0"
  )

  echo "Initializing wheel switch PVs for tests/test_WheelSwitch.adl"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local initialized=0
  for _ in $(seq 1 "${retries}"); do
    initialized=1
    local entry pv value lopr hopr prec full_pv
    for entry in "${wheel_switch_init_values[@]}"; do
      read -r pv value lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec},${full_pv}=${value}" \
          >/dev/null 2>&1; then
        initialized=0
        break
      fi
    done
    if [[ "${initialized}" -eq 1 ]]; then
      echo "Wheel switch PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize wheel switch PV values after ${retries} retries." >&2
  return 1
}

set_slider_alarm_probe_pvs() {
  local prefix="$1"
  local -a alarm_probe_fields=(
    # pv lopr hopr prec (intentionally no value write, keeps UDF/INVALID)
    "weirdChan -20 20 2"
    "ZzzButton -5 5 3"
  )

  echo "Configuring alarm probe PV limits for slider/scale monitor/bar/byte/arc/line/rectangle/polygon/oval/text/text entry/wheel switch tests"
  set_local_ca_env

  local retries=20
  local delay=0.25
  local configured=0
  for _ in $(seq 1 "${retries}"); do
    configured=1
    local entry pv lopr hopr prec full_pv
    for entry in "${alarm_probe_fields[@]}"; do
      read -r pv lopr hopr prec <<< "${entry}"
      full_pv="${prefix}${pv}"
      if ! "${CAVPUT_BIN}" \
          "-list=${full_pv}.LOPR=${lopr},${full_pv}.HOPR=${hopr},${full_pv}.PREC=${prec}" \
          >/dev/null 2>&1; then
        configured=0
        break
      fi
    done
    if [[ "${configured}" -eq 1 ]]; then
      echo "Alarm probe PV field setup complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to configure alarm probe PV fields after ${retries} retries." >&2
  return 1
}

set_related_display_test_pvs() {
  local _prefix="$1"

  echo "Related display harness uses launch links only for tests/test_RelatedDisplay.adl (no PV initialization required)."
}

cleanup() {
  local status=$?
  if [[ -n "${ioc_pid}" ]] && kill -0 "${ioc_pid}" >/dev/null 2>&1; then
    kill "${ioc_pid}" >/dev/null 2>&1 || true
    wait "${ioc_pid}" >/dev/null 2>&1 || true
  fi
  if [[ ${status} -eq 130 ]]; then
    echo "IOC stopped by user."
  fi
}
trap cleanup EXIT INT TERM

set_local_ca_env
"${cmd[@]}" > "${LOG_FILE}" 2>&1 &
ioc_pid="$!"
set_slider_test_pvs "${PV_PREFIX}" || true
set_scale_monitor_test_pvs "${PV_PREFIX}" || true
set_meter_test_pvs "${PV_PREFIX}" || true
set_strip_chart_test_pvs "${PV_PREFIX}" || true
set_cartesian_plot_test_pvs "${PV_PREFIX}" || true
set_bar_test_pvs "${PV_PREFIX}" || true
set_byte_test_pvs "${PV_PREFIX}" || true
set_arc_test_pvs "${PV_PREFIX}" || true
set_line_test_pvs "${PV_PREFIX}" || true
set_menu_test_pvs "${PV_PREFIX}" || true
set_related_display_test_pvs "${PV_PREFIX}" || true
set_choice_button_test_pvs "${PV_PREFIX}" || true
set_message_button_test_pvs "${PV_PREFIX}" || true
set_image_test_pvs "${PV_PREFIX}" || true
set_rectangle_test_pvs "${PV_PREFIX}" || true
set_polygon_test_pvs "${PV_PREFIX}" || true
set_oval_test_pvs "${PV_PREFIX}" || true
set_text_test_pvs "${PV_PREFIX}" || true
set_text_fonts_test_pvs "${PV_PREFIX}" || true
set_text_entry_test_pvs "${PV_PREFIX}" || true
set_wheel_switch_test_pvs "${PV_PREFIX}" || true
set_slider_alarm_probe_pvs "${PV_PREFIX}" || true
wait "${ioc_pid}"
