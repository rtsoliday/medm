#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/qtedm_test_env.sh
source "${SCRIPT_DIR}/lib/qtedm_test_env.sh"

trap 'qtedm_test_cleanup $?' EXIT INT TERM

qtedm_test_setup_env

export XDG_RUNTIME_DIR="${QTEDM_TEST_TMP_DIR}/xdg-runtime"
mkdir -p "${XDG_RUNTIME_DIR}"
chmod 700 "${XDG_RUNTIME_DIR}"

QTEDM_BIN="${QTEDM_BIN:-$(qtedm_test_default_qtedm_bin)}"
QTEDM_SOFT_IOC_PVA="${QTEDM_SOFT_IOC_PVA:?QTEDM_SOFT_IOC_PVA is required}"
QTEDM_PVGET="${QTEDM_PVGET:?QTEDM_PVGET is required}"
QTEDM_PVPUT="${QTEDM_PVPUT:?QTEDM_PVPUT is required}"
QTEDM_CAGET="${QTEDM_CAGET:?QTEDM_CAGET is required}"
QTEDM_BIN="$(qtedm_test_posix_path "${QTEDM_BIN}")"
QTEDM_SOFT_IOC_PVA="$(qtedm_test_posix_path "${QTEDM_SOFT_IOC_PVA}")"
QTEDM_PVGET="$(qtedm_test_posix_path "${QTEDM_PVGET}")"
QTEDM_PVPUT="$(qtedm_test_posix_path "${QTEDM_PVPUT}")"
QTEDM_CAGET="$(qtedm_test_posix_path "${QTEDM_CAGET}")"

python3 "${SCRIPT_DIR}/qtedm_ioc_smoke.py" \
  --qtedm "${QTEDM_BIN}" \
  --run-local-ioc "${SCRIPT_DIR}/run_local_ioc.sh" \
  --cavput "${SCRIPT_DIR}/cavput" \
  --caget "${QTEDM_CAGET}" \
  --soft-ioc-pva "${QTEDM_SOFT_IOC_PVA}" \
  --pvget "${QTEDM_PVGET}" \
  --pvput "${QTEDM_PVPUT}" \
  --pva-database "${QTEDM_TEST_REPO_ROOT}/qtedm/tests/data/safety_controls_pva.db" \
  --cases "${SCRIPT_DIR}/qtedm_ioc_cases.json" \
  "$@"
