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

  echo "Initializing text entry PVs for tests/test_TextEntry.adl"
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
      echo "Text entry PV initialization complete."
      return 0
    fi
    sleep "${delay}"
  done

  echo "Warning: Failed to initialize text entry PV values after ${retries} retries." >&2
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

  echo "Configuring alarm probe PV limits for slider/scale monitor/byte/polygon/text entry/wheel switch tests"
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
set_byte_test_pvs "${PV_PREFIX}" || true
set_polygon_test_pvs "${PV_PREFIX}" || true
set_text_entry_test_pvs "${PV_PREFIX}" || true
set_wheel_switch_test_pvs "${PV_PREFIX}" || true
set_slider_alarm_probe_pvs "${PV_PREFIX}" || true
wait "${ioc_pid}"
