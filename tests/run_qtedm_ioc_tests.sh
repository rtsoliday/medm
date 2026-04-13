#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/qtedm_test_env.sh
source "${SCRIPT_DIR}/lib/qtedm_test_env.sh"

trap 'qtedm_test_cleanup $?' EXIT INT TERM

qtedm_test_setup_env

QTEDM_BIN="${QTEDM_BIN:-$(qtedm_test_default_qtedm_bin)}"
IOC_LOG="${QTEDM_TEST_TMP_DIR}/local_ioc.log"

if [[ ! -x "${SCRIPT_DIR}/sddsSoftIOC" ]]; then
  echo "Missing IOC test dependency: ${SCRIPT_DIR}/sddsSoftIOC" >&2
  exit 1
fi

if [[ ! -x "${SCRIPT_DIR}/cavput" ]]; then
  echo "Missing IOC test dependency: ${SCRIPT_DIR}/cavput" >&2
  exit 1
fi

"${SCRIPT_DIR}/run_local_ioc.sh" \
  --execution-time 60 \
  --log-file "${IOC_LOG}" \
  >/dev/null 2>&1 &
ioc_pid="$!"
qtedm_test_register_child "${ioc_pid}"

ready=0
for _ in $(seq 1 20); do
  if "${SCRIPT_DIR}/cavput" \
      "-list=slider:test:alpha=24.5" \
      >/dev/null 2>&1; then
    ready=1
    break
  fi
  sleep 0.5
done

if [[ "${ready}" -ne 1 ]]; then
  echo "IOC-backed test harness failed to become ready." >&2
  exit 1
fi

set +e
timeout 20s \
  "${QTEDM_BIN}" \
  -x \
  -testExitAfterMs 1500 \
  "${QTEDM_TEST_REPO_ROOT}/tests/test_Meter.adl" \
  "$@"
status=$?
set -e

if [[ "${status}" -ne 0 ]]; then
  if [[ "${status}" -eq 124 ]]; then
    echo "QtEDM IOC smoke test timed out." >&2
  fi
  exit "${status}"
fi
