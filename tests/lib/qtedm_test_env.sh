#!/usr/bin/env bash

set -euo pipefail

QTEDM_TEST_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QTEDM_TEST_REPO_ROOT="$(cd "${QTEDM_TEST_LIB_DIR}/../.." && pwd)"
QTEDM_TEST_TMP_DIR="${QTEDM_TEST_TMP_DIR:-}"
QTEDM_TEST_CHILD_PIDS="${QTEDM_TEST_CHILD_PIDS:-}"
QTEDM_TEST_REPEATER_PID=""

qtedm_test_posix_path() {
  local path="${1:-}"
  if command -v cygpath >/dev/null 2>&1 \
      && [[ "${path}" =~ ^[[:alpha:]]:[/\\] ]]; then
    cygpath -u "${path}"
  else
    printf '%s\n' "${path}"
  fi
}

qtedm_test_native_path() {
  local path="${1:-}"
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "${path}"
  else
    printf '%s\n' "${path}"
  fi
}

qtedm_test_setup_sdds_tools() {
  local executable_suffix=""
  local host_arch=""
  local sdds_epics_root=""
  local sdds_epics_bin=""

  case "$(uname -s)" in
    CYGWIN*)
      host_arch="Windows-x86_64"
      executable_suffix=".exe"
      ;;
    *)
      host_arch="$(uname -s)-$(uname -m)"
      ;;
  esac
  sdds_epics_root="${SDDS_EPICS_ROOT:-${QTEDM_TEST_REPO_ROOT}/../SDDS-EPICS}"
  sdds_epics_bin="${sdds_epics_root}/bin/${host_arch}"
  if [[ -z "${SDDS_SOFT_IOC_REAL_BIN:-}" \
      && -x "${sdds_epics_bin}/sddsSoftIOC${executable_suffix}" ]]; then
    export SDDS_SOFT_IOC_REAL_BIN="${sdds_epics_bin}/sddsSoftIOC${executable_suffix}"
  fi
  if [[ -z "${CAVPUT_REAL_BIN:-}" \
      && -x "${sdds_epics_bin}/cavput${executable_suffix}" ]]; then
    export CAVPUT_REAL_BIN="${sdds_epics_bin}/cavput${executable_suffix}"
  fi
}

qtedm_test_setup_env() {
  if [[ -z "${QTEDM_TEST_TMP_DIR}" ]]; then
    QTEDM_TEST_TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/qtedm-tests.XXXXXX")"
  fi

  mkdir -p "${QTEDM_TEST_TMP_DIR}/home"

  export QTEDM_TEST_TMP_DIR
  export QTEDM_TEST_REPO_ROOT
  export QT_QPA_PLATFORM=offscreen
  export QTEDM_NOLOG=1
  export LC_ALL=C.UTF-8
  export TZ=UTC
  export HOME="${QTEDM_TEST_TMP_DIR}/home"
  # Keep every CA/PVA client and test server on the loopback interface. Use
  # the numeric address so isolation does not depend on name resolution, and
  # clear site settings that could reach production gateways, archivers,
  # display trees, or plugins.
  export EPICS_CA_ADDR_LIST=127.0.0.1
  export EPICS_CA_AUTO_ADDR_LIST=NO
  export EPICS_CAS_INTF_ADDR_LIST=127.0.0.1
  export EPICS_CAS_BEACON_ADDR_LIST=127.0.0.1
  export EPICS_CAS_AUTO_BEACON_ADDR_LIST=NO
  export EPICS_PVA_ADDR_LIST=127.0.0.1
  export EPICS_PVA_AUTO_ADDR_LIST=NO
  export EPICS_PVAS_INTF_ADDR_LIST=127.0.0.1
  export EPICS_PVAS_BEACON_ADDR_LIST=127.0.0.1
  export EPICS_PVAS_AUTO_BEACON_ADDR_LIST=NO
  unset EPICS_CA_NAME_SERVERS EPICS_PVA_NAME_SERVERS EPICS_DISPLAY_PATH
  unset QTEDM_ARCHIVER_URL QTEDM_ARCHIVER_PROVIDER QTEDM_PLUGIN_PATH
  unset MEDM_EXEC_LIST
  qtedm_test_setup_sdds_tools
}

qtedm_test_default_qtedm_bin() {
  printf '%s\n' "${QTEDM_TEST_REPO_ROOT}/bin/$(uname -s)-$(uname -m)/qtedm"
}

qtedm_test_register_child() {
  if [[ -n "${1:-}" ]]; then
    QTEDM_TEST_CHILD_PIDS="${QTEDM_TEST_CHILD_PIDS} $1"
    export QTEDM_TEST_CHILD_PIDS
  fi
}

qtedm_test_start_repeater() {
  local manage_repeater="${QTEDM_TEST_MANAGE_REPEATER:-0}"
  local repeater_bin="${QTEDM_TEST_CA_REPEATER_BIN:-}"
  local repeater_port="${EPICS_CA_REPEATER_PORT:-}"
  local port_number=""

  if [[ "${manage_repeater}" == "0" ]]; then
    return 0
  fi
  if [[ "${manage_repeater}" != "1" ]]; then
    echo "QTEDM_TEST_MANAGE_REPEATER must be 0 or 1." >&2
    return 1
  fi
  if [[ "${repeater_bin}" != /* || ! -f "${repeater_bin}" \
      || ! -x "${repeater_bin}" ]]; then
    echo "Managed caRepeater requires an absolute executable QTEDM_TEST_CA_REPEATER_BIN." >&2
    return 1
  fi
  if [[ ! "${repeater_port}" =~ ^[0-9]{1,5}$ ]]; then
    echo "Managed caRepeater requires an explicit private EPICS_CA_REPEATER_PORT (49152..65535)." >&2
    return 1
  fi
  port_number=$((10#${repeater_port}))
  if (( port_number < 49152 || port_number > 65535 )); then
    echo "Managed caRepeater requires EPICS_CA_REPEATER_PORT in 49152..65535." >&2
    return 1
  fi
  if [[ -z "${QTEDM_TEST_TMP_DIR}" || ! -d "${QTEDM_TEST_TMP_DIR}" ]]; then
    echo "Managed caRepeater requires qtedm_test_setup_env first." >&2
    return 1
  fi
  if [[ -n "${QTEDM_TEST_REPEATER_PID}" ]]; then
    if kill -0 "${QTEDM_TEST_REPEATER_PID}" >/dev/null 2>&1; then
      return 0
    fi
    echo "Owned caRepeater exited; refusing to continue with unmanaged CA." >&2
    return 1
  fi

  export EPICS_CA_REPEATER_PORT="${port_number}"
  export EPICS_CAS_BEACON_PORT="${port_number}"
  "${repeater_bin}" > "${QTEDM_TEST_TMP_DIR}/caRepeater.log" 2>&1 &
  QTEDM_TEST_REPEATER_PID="$!"
  qtedm_test_register_child "${QTEDM_TEST_REPEATER_PID}"
  echo "Started owned caRepeater PID: ${QTEDM_TEST_REPEATER_PID} (port ${port_number})"
  sleep 1
  if ! kill -0 "${QTEDM_TEST_REPEATER_PID}" >/dev/null 2>&1; then
    echo "Owned caRepeater exited during startup; refusing to run CA tests." >&2
    cat "${QTEDM_TEST_TMP_DIR}/caRepeater.log" >&2
    return 1
  fi
}

qtedm_test_cleanup() {
  local status="${1:-$?}"
  local pid=""
  local wait_status=""

  for pid in ${QTEDM_TEST_CHILD_PIDS}; do
    if kill -0 "${pid}" >/dev/null 2>&1; then
      echo "Stopping owned test child PID: ${pid}"
      kill "${pid}" >/dev/null 2>&1 || true
    fi
    wait_status=0
    wait "${pid}" >/dev/null 2>&1 || wait_status=$?
    echo "Owned test child cleanup completed PID: ${pid} (wait status ${wait_status})"
  done
  QTEDM_TEST_CHILD_PIDS=""
  QTEDM_TEST_REPEATER_PID=""
  export QTEDM_TEST_CHILD_PIDS

  if [[ -n "${QTEDM_TEST_TMP_DIR}" && -d "${QTEDM_TEST_TMP_DIR}" ]]; then
    rm -rf "${QTEDM_TEST_TMP_DIR}"
  fi

  return "${status}"
}
