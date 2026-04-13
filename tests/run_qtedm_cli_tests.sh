#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/qtedm_test_env.sh
source "${SCRIPT_DIR}/lib/qtedm_test_env.sh"

trap 'qtedm_test_cleanup $?' EXIT INT TERM

qtedm_test_setup_env

QTEDM_BIN="${QTEDM_BIN:-$(qtedm_test_default_qtedm_bin)}"

python3 "${SCRIPT_DIR}/testADL_SaveFiles.py" \
  --qtedm "${QTEDM_BIN}" \
  --output "${QTEDM_TEST_TMP_DIR}/qtedmTest.adl" \
  --manifest "${SCRIPT_DIR}/qtedm_roundtrip_manifest.txt" \
  "$@"
