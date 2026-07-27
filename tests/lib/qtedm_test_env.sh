#!/usr/bin/env bash

set -euo pipefail

QTEDM_TEST_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QTEDM_TEST_REPO_ROOT="$(cd "${QTEDM_TEST_LIB_DIR}/../.." && pwd)"
QTEDM_TEST_TMP_DIR="${QTEDM_TEST_TMP_DIR:-}"
QTEDM_TEST_CHILD_PIDS="${QTEDM_TEST_CHILD_PIDS:-}"

qtedm_test_setup_sdds_tools() {
  local host_arch=""
  local sdds_epics_bin=""

  host_arch="$(uname -s)-$(uname -m)"
  sdds_epics_bin="${QTEDM_TEST_REPO_ROOT}/../SDDS-EPICS/bin/${host_arch}"
  if [[ -z "${SDDS_SOFT_IOC_REAL_BIN:-}" \
      && -x "${sdds_epics_bin}/sddsSoftIOC" ]]; then
    export SDDS_SOFT_IOC_REAL_BIN="${sdds_epics_bin}/sddsSoftIOC"
  fi
  if [[ -z "${CAVPUT_REAL_BIN:-}" \
      && -x "${sdds_epics_bin}/cavput" ]]; then
    export CAVPUT_REAL_BIN="${sdds_epics_bin}/cavput"
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
  export LC_ALL=C
  export TZ=UTC
  export HOME="${QTEDM_TEST_TMP_DIR}/home"
  export EPICS_CA_ADDR_LIST=localhost
  export EPICS_CA_AUTO_ADDR_LIST=NO
  export EPICS_PVA_ADDR_LIST=127.0.0.1
  export EPICS_PVA_AUTO_ADDR_LIST=NO
  export EPICS_PVAS_INTF_ADDR_LIST=127.0.0.1
  export EPICS_PVAS_BEACON_ADDR_LIST=127.0.0.1
  export EPICS_PVAS_AUTO_BEACON_ADDR_LIST=NO
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

qtedm_test_cleanup() {
  local status="${1:-$?}"
  local pid=""

  for pid in ${QTEDM_TEST_CHILD_PIDS}; do
    if kill -0 "${pid}" >/dev/null 2>&1; then
      kill "${pid}" >/dev/null 2>&1 || true
      wait "${pid}" >/dev/null 2>&1 || true
    fi
  done

  if [[ -n "${QTEDM_TEST_TMP_DIR}" && -d "${QTEDM_TEST_TMP_DIR}" ]]; then
    rm -rf "${QTEDM_TEST_TMP_DIR}"
  fi

  return "${status}"
}
