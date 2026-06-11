#!/usr/bin/env bash
set -euo pipefail
############################################################################
#
# Resolve the repo root from this script's location so the repo is relocatable.
OLYMPIA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OLYMPIA_PRESET="${OLYMPIA_PRESET:-debug}"
OLYMPIA_BIN="${OLYMPIA_ROOT}/build/${OLYMPIA_PRESET}"
OLYMPIA_FIXTURES="${OLYMPIA_ROOT}/tests"
OLYMPIA_RUN="${OLYMPIA_ROOT}/run"
OLYMPIA_ENGINE=g1
OLYMPIA_COMMAND=olympia
############################################################################
#
ROOT="${OLYMPIA_ROOT}"
RUN_LIB="${ROOT}/run/g1/olympia/lib"
GOLDEN="${ROOT}/tests/g1/olympia/golden"

usage() {
  echo "usage: $0 [--update]"
  echo "  --update   replace golden files with current run output"
  exit 2
}

UPDATE=0
if [[ $# -gt 1 ]]; then usage; fi
if [[ $# -eq 1 ]]; then
  [[ "$1" == "--update" ]] || usage
  UPDATE=1
fi

[[ -d "${RUN_LIB}" ]] || { echo "NO: missing run lib dir: ${RUN_LIB}"; exit 1; }
mkdir -p "${GOLDEN}"

# Optional: ignore noisy files/dirs you don't want in goldens.
# Adjust as you learn what's stable.
EXCLUDES=(
  "--exclude=.DS_Store"
  "--exclude=master"       # if you don't want to golden the regenerated master index
  "--exclude=email"
  "--exclude=totimes"
  "--exclude=forward"
  "--exclude=factions"
  "--exclude=lore"
  "--exclude=skill"
)

# Update mode: replace golden snapshot
if [[ "${UPDATE}" -eq 1 ]]; then
  rm -rf "${GOLDEN:?}/"*
  rsync -a "${EXCLUDES[@]}" "${RUN_LIB}/" "${GOLDEN}/"
  echo "OK: updated golden snapshot from ${RUN_LIB}"
  exit 0
fi

# Test mode: compare current run output to golden
if [[ ! -d "${GOLDEN}" || -z "$(ls -A "${GOLDEN}" 2>/dev/null)" ]]; then
  echo "NO: golden directory is empty. Run: $0 --update"
  exit 1
fi

# rsync in dry-run + checksum mode gives a clean yes/no.
# Any output indicates differences.
DIFF_OUT="$(rsync -a -n -c --delete "${EXCLUDES[@]}" "${RUN_LIB}/" "${GOLDEN}/" || true)"

if [[ -n "${DIFF_OUT}" ]]; then
  echo "NO"
  echo "${DIFF_OUT}"
  exit 1
fi

echo "YES"

