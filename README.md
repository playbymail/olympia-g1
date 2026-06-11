# Olympia G1

**G1** is the first-generation Olympia play-by-mail (PBM) strategy game engine
(~51K lines of C) — the foundation version from which the later G2, G3, and TAG
engines descend.

This repository is a standalone extraction of the G1 engine from the original
multi-engine Olympia monorepo. It builds on its own with CMake.

The code is legacy K&R-style C originally targeting 32-bit systems. A modernization
effort is underway to make it compile cleanly on 64-bit systems.

## Targets

- `g1-olympia` — the main game engine
- `g1-mapgen` — the map generator (inputs `Map`/`Land`/`Cities`/`Regions`,
  outputs `gate`/`loc`/`road`)

## Building

Requires CMake (>= 4.1), Ninja, and a Clang or GCC toolchain.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/g1-olympia, build/debug/g1-mapgen
```

Presets (see `CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`.

Without presets:

```bash
mkdir build && cd build && cmake .. && cmake --build .
```

### 32-bit build (Linux, for golden-file generation)

```bash
mkdir build32 && cd build32
cmake -DBUILD_32BIT=ON ..   # requires gcc-multilib
cmake --build .
```

## Running / golden tests

Build first (default `debug` preset), then:

```bash
# mapgen: generates gate/loc/road and can be compared to tests/g1/mapgen/golden
./run/g1/mapgen/mapgen.sh

# olympia: extracts fixtures, runs a turn, saves the database
./run/g1-olympia.sh

# compare the olympia run output against the golden snapshot
./tests/g1/olympia/golden_check.sh           # YES = match
./tests/g1/olympia/golden_check.sh --update   # refresh the snapshot
```

The scripts auto-detect the repo root and look for binaries at
`build/<preset>/<target>` (override the preset with `OLYMPIA_PRESET=release ...`).

## Layout

- `g1/olympia/` — the G1 engine sources and headers
- `g1/mapgen/` — the map generator
- `lib/` — shared support code (entity lists, tiles, roads, allocation, …)
- `tests/g1/` — golden-test fixtures and golden files
- `run/` — run/test driver scripts and scratch run directories
- `g1/doc/` — assorted G1 design/reference notes

## License

GNU Affero General Public License v3 — see [LICENSE](LICENSE). The original
Olympia sources are public domain.
