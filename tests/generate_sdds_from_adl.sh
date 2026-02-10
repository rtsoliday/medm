#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_FILE="${SCRIPT_DIR}/sddsSoftIOC_tests.sdds"
ADL_GLOB="${SCRIPT_DIR}"/*.adl

usage() {
  cat <<'EOF'
Usage: generate_sdds_from_adl.sh [--output <path>]

Generate an SDDS input file for tests/sddsSoftIOC by scanning PV names from
all tests/*.adl files.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output|-o)
      [[ $# -ge 2 ]] || {
        echo "Missing value for $1" >&2
        exit 1
      }
      OUTPUT_FILE="$2"
      shift 2
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

if ! ls ${ADL_GLOB} >/dev/null 2>&1; then
  echo "No ADL files found in ${SCRIPT_DIR}" >&2
  exit 1
fi

tmp_pvs="$(mktemp)"
tmp_out="$(mktemp)"
trap 'rm -f "${tmp_pvs}" "${tmp_out}"' EXIT

if command -v rg >/dev/null 2>&1; then
  rg --only-matching --no-filename -P 'chan[A-D]?="[^"]+"' "${SCRIPT_DIR}"/*.adl \
    | sed -E 's/^[^=]+="([^"]+)"$/\1/' \
    | sort -u > "${tmp_pvs}"
else
  grep -hEo 'chan[A-D]?="[^"]+"' "${SCRIPT_DIR}"/*.adl \
    | sed -E 's/^[^=]+="([^"]+)"$/\1/' \
    | sort -u > "${tmp_pvs}"
fi

row_count="$(wc -l < "${tmp_pvs}" | tr -d ' ')"

{
  echo 'SDDS1'
  echo '&description text="QtEDM/MEDM local soft IOC PV definitions for tests", contents="sddsSoftIOC input" &end'
  echo '&column name=ControlName, type=string, description="EPICS PV name" &end'
  echo '&column name=Type, type=string, description="PV scalar type" &end'
  echo '&column name=EnumStrings, type=string, description="Comma-separated enum state labels" &end'
  echo '&data mode=ascii &end'
  echo "${row_count}"
  while IFS= read -r pv; do
    type="double"
    enum_strings='""'
    case "${pv}" in
      tm:string:status)
        type="string"
        ;;
      mn:*|cb:*)
        type="enum"
        enum_strings='"OFF,ON,AUTO,ERROR"'
        ;;
      byte:*)
        type="uint"
        ;;
    esac
    printf '"%s" "%s" %s\n' "${pv}" "${type}" "${enum_strings}"
  done < "${tmp_pvs}"
} > "${tmp_out}"

mkdir -p "$(dirname "${OUTPUT_FILE}")"
mv "${tmp_out}" "${OUTPUT_FILE}"
echo "Generated ${OUTPUT_FILE} with ${row_count} PV definitions."
