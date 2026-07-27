#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/qtedm_test_env.sh
source "${SCRIPT_DIR}/lib/qtedm_test_env.sh"

trap 'qtedm_test_cleanup $?' EXIT INT TERM

qtedm_test_setup_env

QTEDM_BIN="${QTEDM_BIN:-$(qtedm_test_default_qtedm_bin)}"
QTEDM_CONVERT_BIN="${QTEDM_CONVERT_BIN:-}"

python3 "${SCRIPT_DIR}/testADL_SaveFiles.py" \
  --qtedm "${QTEDM_BIN}" \
  --output "${QTEDM_TEST_TMP_DIR}/qtedmTest.adl" \
  --manifest "${SCRIPT_DIR}/qtedm_roundtrip_manifest.txt" \
  "$@"

if [[ -z "${QTEDM_CONVERT_BIN}" || ! -x "${QTEDM_CONVERT_BIN}" ]]; then
  echo "qtedm-convert binary not found or not executable: ${QTEDM_CONVERT_BIN}" >&2
  exit 1
fi

"${QTEDM_CONVERT_BIN}" --help \
  | grep -q "Exit status: 0 complete, 2 converted with warnings, 1 fatal"

conversion_dir="${QTEDM_TEST_TMP_DIR}/phase4-conversion"
mkdir -p "${conversion_dir}"
set +e
"${QTEDM_CONVERT_BIN}" \
  --output "${conversion_dir}/converted.adl" \
  --report "${conversion_dir}/report.json" \
  --source-copy "${conversion_dir}/source.ui" \
  "${SCRIPT_DIR}/fixtures/phase4/caqtdm_mixed.ui"
conversion_status=$?
set -e
if [[ "${conversion_status}" -ne 2 ]]; then
  echo "Expected warning conversion exit status 2, got ${conversion_status}" >&2
  exit 1
fi
test -s "${conversion_dir}/converted.adl"
test -s "${conversion_dir}/report.json"
cmp -s "${SCRIPT_DIR}/fixtures/phase4/caqtdm_mixed.ui" \
  "${conversion_dir}/source.ui"
grep -q "qtedm_tabbed_display" "${conversion_dir}/converted.adl"
grep -q "qtedm_ndarray_image" "${conversion_dir}/converted.adl"
grep -q '"schema_version": 1' "${conversion_dir}/report.json"

set +e
"${QTEDM_CONVERT_BIN}" "${SCRIPT_DIR}/index.adl" >/dev/null 2>&1
fatal_status=$?
set -e
if [[ "${fatal_status}" -ne 1 ]]; then
  echo "Expected unsupported-format exit status 1, got ${fatal_status}" >&2
  exit 1
fi
