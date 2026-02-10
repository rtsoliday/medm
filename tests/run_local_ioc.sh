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

set_slider_alarm_probe_pvs() {
  local prefix="$1"
  local -a alarm_probe_fields=(
    # pv lopr hopr prec (intentionally no value write, keeps UDF/INVALID)
    "weirdChan -20 20 2"
    "ZzzButton -5 5 3"
  )

  echo "Configuring alarm probe PV limits for tests/test_Slider.adl"
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
set_slider_alarm_probe_pvs "${PV_PREFIX}" || true
wait "${ioc_pid}"
