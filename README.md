# Olympia G1

**G1** is the first-generation Olympia play-by-mail (PBM) strategy game engine
(~60K lines of C) — the foundation version from which the later G2, G3, and TAG
engines descend.

Sibling engine repositories:

- [olympia-g2](https://github.com/playbymail/olympia-g2)
- [olympia-g3](https://github.com/playbymail/olympia-g3)
- [olympia-tag](https://github.com/playbymail/olympia-tag)

This repository is a standalone extraction of the G1 engine from the original
multi-engine Olympia monorepo. It builds on its own with CMake.

The code is legacy K&R-style C originally targeting 32-bit systems. A modernization
effort is underway to make it compile cleanly on 64-bit systems.

> [!IMPORTANT]
> **This is a modernization project, not a development project — no new features.**
> The goal is to bring the existing G1 engine to clean C11 on 64-bit while
> preserving its exact behavior. **Golden output is the contract:** every change
> must keep the golden tests passing (byte-identical), and any behavior change
> must be deliberate, justified, and re-baselined in the same commit. New game
> features, gameplay tweaks, and scope expansion are out of bounds. See
> [BUILD_HISTORY.md](BUILD_HISTORY.md) for the full modernization record.

## Targets

- `olympia-g1` — the main game engine
- `mapgen-g1` — the map generator (inputs `Map`/`Land`/`Cities`/`Regions`,
  outputs `gate`/`loc`/`road`)

## Building

Requires CMake (>= 4.1), Ninja, and a Clang or GCC toolchain.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/olympia-g1, build/debug/mapgen-g1
```

Presets (see `CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`.
The `asan-ubsan` preset builds with AddressSanitizer + UndefinedBehaviorSanitizer.

Without presets:

```bash
mkdir build && cd build && cmake .. && cmake --build .
```

## Running / golden tests

Build first (default `debug` preset), then:

```bash
# mapgen: generates gate/loc/road from the map fixtures
./run/mapgen/mapgen.sh

# olympia: extracts fixtures, runs a turn, saves the database
./run/olympia-g1.sh

# compare the olympia run output against the golden snapshot
./tests/olympia/golden_check.sh            # YES = match (exit 0)
./tests/olympia/golden_check.sh --update   # refresh the snapshot (only when output changed intentionally)
```

The golden snapshot is a sha256 hash manifest
(`tests/olympia/golden/manifest.sha256`): one `<sha256>  <relpath>` line per
goldened run file. The gate hashes every byte of every file, so it is portable
across macOS/Linux and cannot be fooled by equal-size content edits. `YES` is the
gate that must stay green.

The scripts auto-detect the repo root and look for binaries at
`build/<preset>/<target>` (override the preset with `OLYMPIA_PRESET=release ...`).

## Layout

- `olympia/` — the G1 engine sources and headers
- `mapgen/` — the map generator (`mapgen.c`, `z.c`)
- `lib/` — shared support code (entity lists, tiles, roads, allocation, …)
- `tests/` — golden-test fixtures and the golden sha256 manifest
- `run/` — run/test driver scripts and scratch run directories
- `doc/` — assorted G1 design/reference and modernization notes
- `CLAUDE.md` — working guidance (build, test, conventions)
- `BUILD_HISTORY.md` — full phase-by-phase modernization record
- `AUTHORS.md` — authors and credits

## Authors

See [AUTHORS.md](AUTHORS.md). Olympia was created by Rich Skrenta; this
repository is maintained and modernized by Michael Henderson.

## License

GNU Affero General Public License v3 — see [LICENSE](LICENSE). The original
Olympia sources are public domain.
