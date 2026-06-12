#!/usr/bin/env bash
set -euo pipefail
############################################################################
#
# golden_check.sh — byte-for-byte gate on the olympia-g1 engine output.
#
# The engine writes its post-turn database to run/olympia/lib (a few hundred
# files). Rather than commit a literal golden tree and diff it with a dry-run
# rsync, the golden is a **hash manifest**: one `<sha256>  <relpath>` line per
# run file, sorted. That captures every byte of every goldened file in a small,
# diffable artifact. Any content change anywhere shows up as a manifest line
# that no longer matches.
#
# Why the change (issue #4): the previous harness used
#   rsync -a -n -c --delete ...
# to compare run output to a committed golden tree. On macOS /usr/bin/rsync is
# openrsync, which in dry-run (-n) does NOT honor -c (checksum) — it falls back
# to a size+mtime quick-check. Any content change that preserves file size
# (e.g. the fixed-width Olympia Times masthead date) passed UNDETECTED and the
# gate printed a false YES. Hashing every byte is portable across macOS/Linux
# and cannot be fooled by equal-size edits. Adapted from olympia-g3's harness.
#
# The EXCLUDES from the rsync harness are preserved as the manifest prune set
# (master, email, totimes, forward, factions, lore, skill, plus .DS_Store), so
# the manifest covers exactly the same files that were goldened before.
#
# Workflow:
#   1. ./run/mapgen/mapgen.sh         # generate gate/loc/road
#   2. ./run/olympia-g1.sh            # extract fixtures, run a turn, save DB
#                                     # leaving the post-turn DB in run/olympia/lib
#   3. ./tests/olympia/golden_check.sh        # gate: prints YES on a match
#      ./tests/olympia/golden_check.sh --update # (re)capture the baseline
#
############################################################################
#
# Resolve the repo root from this script's location so the repo is relocatable.
OLYMPIA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OLYMPIA_ENGINE=g1
OLYMPIA_COMMAND=olympia
############################################################################
#
ROOT="${OLYMPIA_ROOT}"
RUN_LIB="${ROOT}/run/${OLYMPIA_COMMAND}/lib"
GOLDEN="${ROOT}/tests/${OLYMPIA_COMMAND}/golden"
MANIFEST="${GOLDEN}/manifest.sha256"

usage() {
  echo "usage: $0 [--update]"
  echo "  --update   replace the golden manifest from current run output"
  exit 2
}

UPDATE=0
if [[ $# -gt 1 ]]; then usage; fi
if [[ $# -eq 1 ]]; then
  [[ "$1" == "--update" ]] || usage
  UPDATE=1
fi

[[ -d "${RUN_LIB}" ]] || { echo "NO: missing run lib dir: ${RUN_LIB} (run ./run/olympia-${OLYMPIA_ENGINE}.sh first)"; exit 1; }

# Pick a sha256 tool (shasum on macOS, sha256sum on Linux; either works on both).
if command -v sha256sum >/dev/null 2>&1; then
  SHA_CMD="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  SHA_CMD="shasum -a 256"
else
  echo "NO: no sha256 tool (need sha256sum or shasum)"; exit 1
fi

# Files/dirs held out of the golden (volatile or intentionally not goldened).
# These mirror the EXCLUDES of the previous rsync harness:
#   .DS_Store  — macOS noise
#   master     — regenerated master index
#   email, totimes, forward, factions, skill — volatile bookkeeping files
#   lore/      — regenerated lore tree
# (Tree has only simple relative paths — no spaces/newlines — so newline-safe.)
gen_manifest() {
  ( cd "${RUN_LIB}" \
      && find . -type f \( \
                -name .DS_Store \
             -o -path './master' \
             -o -path './email' \
             -o -path './totimes' \
             -o -path './forward' \
             -o -path './factions' \
             -o -path './lore/*' \
             -o -path './skill' \
           \) -prune \
              -o -type f -print \
      | LC_ALL=C sort \
      | xargs ${SHA_CMD} \
      | LC_ALL=C sort )
}

############################################################################
# Update mode: (re)write the golden manifest.
if [[ "${UPDATE}" -eq 1 ]]; then
  mkdir -p "${GOLDEN}"
  rm -f "${MANIFEST}"
  gen_manifest > "${MANIFEST}"
  echo "OK: updated golden manifest ($(wc -l < "${MANIFEST}" | tr -d ' ') files)"
  exit 0
fi

############################################################################
# Test mode: compare current run output to golden.
[[ -f "${MANIFEST}" ]] || { echo "NO: missing golden manifest: ${MANIFEST}. Run: $0 --update"; exit 1; }

DIFF_OUT="$(diff "${MANIFEST}" <(gen_manifest) || true)"
if [[ -n "${DIFF_OUT}" ]]; then
  echo "NO: run output diverges from golden manifest:"
  echo "${DIFF_OUT}" | head -50
  exit 1
fi

echo "YES"
exit 0
