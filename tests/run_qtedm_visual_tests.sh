#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tests/lib/qtedm_test_env.sh
source "${SCRIPT_DIR}/lib/qtedm_test_env.sh"

trap 'qtedm_test_cleanup $?' EXIT INT TERM

qtedm_test_setup_env

export QT_SCALE_FACTOR=1
export QT_AUTO_SCREEN_SCALE_FACTOR=0
export QT_ENABLE_HIGHDPI_SCALING=0
export QT_STYLE_OVERRIDE=Fusion
export QT_FONT_DPI=96
export QTEDM_WATERFALL_DRIVER_MODE=static
export XDG_RUNTIME_DIR="${QTEDM_TEST_TMP_DIR}/xdg-runtime"
mkdir -p "${XDG_RUNTIME_DIR}"
chmod 700 "${XDG_RUNTIME_DIR}"

QTEDM_BIN="${QTEDM_BIN:-$(qtedm_test_default_qtedm_bin)}"
QTEDM_IMAGE_COMPARE_BIN="${QTEDM_IMAGE_COMPARE_BIN:-${QTEDM_TEST_REPO_ROOT}/qtedm/O.$(uname -s)-$(uname -m)/qtedm_image_compare}"

python3 "${SCRIPT_DIR}/qtedm_visual_tests.py" \
  --qtedm "${QTEDM_BIN}" \
  --compare-tool "${QTEDM_IMAGE_COMPARE_BIN}" \
  --run-local-ioc "${SCRIPT_DIR}/run_local_ioc.sh" \
  --cases "${SCRIPT_DIR}/qtedm_visual_cases.json" \
  "$@"
