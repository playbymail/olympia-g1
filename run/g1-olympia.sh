#!/bin/bash
############################################################################
# How g1-olympia Runs
#
# Command-line flags:
#    -l dir    Specify libdir (default: ./lib)
#    -r        Run a turn
#    -a        Add new players mode
#    -e        Eat orders from libdir/spool
#    -i        Immediate mode (interactive commands)
#    -S        Save the database at completion
#    -M        Mail reports
#    -A        Charge player accounts
#    -R        Test random number generator
#    -t        Test ilist code
#
# Required Inputs
#  We need a "library" (a data folder) with these files:
#
#    Core Data Files (from mapgen):
#    - loc - Province/location data (1.9 MB in example)
#    - gate - Magical gate connections (20 KB)
#    - road - Road connections (21 KB)
#
#    Game State Files:
#    - system - System config (player IDs, random seeds, clock)
#    - master - Fast index file (560 KB)
#    - item - Items/resources (163 KB)
#    - skill - Skills/knowledge (9 KB)
#    - ship - Ships (can be empty)
#    - unform - Unit formations (can be empty)
#    - misc - Miscellaneous entities (156 bytes)
#
#    Player Data:
#    - players - Player registry
#    - orders/ - Directory with player orders
#    - email - Email addresses
#    - forward - Email forwarding
#    - fact/ - Faction information
#    - lore/ - Lore sheets per player
#    - spool/ - Order processing queue
#
#    Logging:
#    - log/ - Turn logs and reports
############################################################################
# Testing Strategy
#
#  Can we load the database?
#    g1-olympia -l ./lib
#
#  Can we run in immediate mode (should be simplest)?
#    g1-olympia -l ./lib -i
#
#  Can we run a turn?
#    g1-olympia -l ./lib -r -S
#
#  Then
#    1. Try loading the existing database
#    2. Capture the output as golden files
#    3. Start refactoring
#
############################################################################
#
# Resolve the repo root from this script's location so the repo is relocatable.
OLYMPIA_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# CMakePresets emits binaries to build/<presetName> (default preset: debug).
OLYMPIA_PRESET="${OLYMPIA_PRESET:-debug}"
OLYMPIA_BIN="${OLYMPIA_ROOT}/build/${OLYMPIA_PRESET}"
OLYMPIA_FIXTURES="${OLYMPIA_ROOT}/tests"
OLYMPIA_RUN="${OLYMPIA_ROOT}/run"
OLYMPIA_ENGINE=g1
OLYMPIA_COMMAND=olympia
############################################################################
# verify some paths
[ -d "${OLYMPIA_ROOT}" ] || {
  echo "OLYMPIA_ROOT       == '${OLYMPIA_ROOT}'"
  echo "error: invalid OLYMPIA_ROOT"
  exit 2
}
[ -d "${OLYMPIA_BIN}" ] || {
  echo "OLYMPIA_ROOT       == '${OLYMPIA_ROOT}'"
  echo "OLYMPIA_BIN        == '${OLYMPIA_BIN}'"
  echo "error: invalid OLYMPIA_BIN"
  exit 2
}
[ -d "${OLYMPIA_FIXTURES}" ] || {
  echo "OLYMPIA_ROOT       == '${OLYMPIA_ROOT}'"
  echo "OLYMPIA_FIXTURES   == '${OLYMPIA_FIXTURES}'"
  echo "error: invalid OLYMPIA_FIXTURES"
  exit 2
}
OLYMPIA_INPUTS="${OLYMPIA_FIXTURES}/${OLYMPIA_ENGINE}/${OLYMPIA_COMMAND}/fixtures"
[ -d "${OLYMPIA_INPUTS}" ] || {
  echo "OLYMPIA_FIXTURES   == '${OLYMPIA_FIXTURES}'"
  echo "OLYMPIA_ENGINE     == '${OLYMPIA_ENGINE}'"
  echo "OLYMPIA_COMMAND    == '${OLYMPIA_COMMAND}'"
  echo "error: invalid fixtures path"
  exit 2
}
[ -d "${OLYMPIA_RUN}" ] || {
  echo "OLYMPIA_ROOT       == '${OLYMPIA_ROOT}'"
  echo "OLYMPIA_RUN        == '${OLYMPIA_RUN}'"
  echo "error: invalid OLYMPIA_RUN"
  exit 2
}
OLYMPIA_OUTPUTS="${OLYMPIA_RUN}/${OLYMPIA_ENGINE}/${OLYMPIA_COMMAND}"
[ -d "${OLYMPIA_OUTPUTS}" ] || {
  echo "OLYMPIA_RUN        == '${OLYMPIA_RUN}'"
  echo "OLYMPIA_ENGINE     == '${OLYMPIA_ENGINE}'"
  echo "OLYMPIA_COMMAND    == '${OLYMPIA_COMMAND}'"
  echo "error: invalid run path"
  exit 2
}
############################################################################
#
cd "${OLYMPIA_OUTPUTS}" || {
  echo "error: unable to set def to run path"
  echo "OLYMPIA_RUN        == '${OLYMPIA_RUN}'"
  echo "OLYMPIA_ENGINE     == '${OLYMPIA_ENGINE}'"
  echo "OLYMPIA_COMMAND    == '${OLYMPIA_COMMAND}'"
  exit 2
}

############################################################################
# copy the test fixtures to the run folder
echo " info: copying inputs to $( pwd )"
[ -f "${OLYMPIA_INPUTS}/lib.tgz" ] || {
  echo "error: missing fixtures tarball"
  exit 2
}
[ -d lib ] && {
  echo " warn: removing existing lib/ from run folder"
  rm -rf lib
}
echo " info: extracting fixtures tarball..."
tar zxf "${OLYMPIA_INPUTS}/lib.tgz" || {
  echo "error: failed to extract fixtures tarball"
  exit 2
}
echo " info: rebuilt lib/ from fixtures tarball"
[ -f lib/master ] && {
  # Remove master index file - it will be regenerated on load
  # The master file can be out of date or have box type mismatches
  rm lib/master || {
    echo "error: failed to remove master index file"
    exit 2
  }
  echo " info: removed master index file (will be regenerated)"
}

############################################################################
# set seed values for random number generator
export G1_MAPGEN_SEED_1=18481
export G1_MAPGEN_SEED_2=28078
export G1_MAPGEN_SEED_3=26982

############################################################################
## run the program in "immediate" mode
##   inputs: lib/
##   outputs: unknown
#"${OLYMPIA_BIN}/${OLYMPIA_ENGINE}-${OLYMPIA_COMMAND}" </dev/null || {
#  echo "error: ${OLYMPIA_ENGINE}-${OLYMPIA_COMMAND} failed"
#  exit 2
#}
#echo " info: command thinks that it ran successfully"

############################################################################
# run a turn and update the data store (the flat files in ./lib)
touch .g1.olympia.before
rm -rf lib-before lib-after
cp -a lib lib-before
"${OLYMPIA_BIN}/${OLYMPIA_ENGINE}-${OLYMPIA_COMMAND}" -r -l ./lib -S </dev/null || {
  echo "error: ${OLYMPIA_ENGINE}-${OLYMPIA_COMMAND} failed"
  exit 2
}
echo " info: command thinks that it ran successfully"
echo "       find lib -type f -newer .g1.olympia.before | sort"
echo "       diff -ru lib-before lib"

exit 0
