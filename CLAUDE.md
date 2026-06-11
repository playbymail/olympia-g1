# CLAUDE.md

Guidance for working in the **Olympia G1** repository.

## What this is

G1 is the first-generation Olympia play-by-mail (PBM) strategy game engine
(~60K lines of C), the ancestor of the later G2/G3/TAG engines. This repo is a
standalone extraction of G1 from the original multi-engine monorepo; it builds
on its own with CMake.

The code is legacy C originally targeting **32-bit** systems. An active
modernization effort is bringing it to clean **C11 on 64-bit**. See
[Modernization status](#modernization-status) — read it before changing build
flags or touching prototypes/headers.

## Build

Requires CMake (>= 4.1), Ninja, and Clang or GCC.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/olympia-g1, build/debug/mapgen-g1
```

Presets (`CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`.
The `asan-ubsan` preset sets `OLYMPIA_SANITIZE=ON` with address+undefined.

### 32-bit build (Linux only — for regenerating golden files)

```bash
mkdir build32 && cd build32
cmake -DBUILD_32BIT=ON ..   # requires gcc-multilib
cmake --build .
```

## Test — golden snapshots (must stay green)

Any change must keep the golden tests passing. The olympia check printing `YES`
is the gate.

```bash
./run/mapgen/mapgen.sh                     # generate gate/loc/road
./run/olympia-g1.sh                        # extract fixtures, run a turn, save DB
./tests/olympia/golden_check.sh            # YES = match (exit 0)
./tests/olympia/golden_check.sh --update   # refresh snapshot (only when output is intentionally changed)
```

Scripts auto-detect the repo root and look for binaries at
`build/<preset>/<target>` (override with `OLYMPIA_PRESET=release ...`).

## Layout

- `olympia/` — G1 engine sources (52 `.c`) and headers
- `mapgen/` — map generator (`mapgen.c`, `z.c`)
- `lib/` — shared support code (entity lists, tiles, roads, allocation, …)
- `tests/` — golden fixtures + golden snapshots for olympia and mapgen
- `run/` — run/test driver scripts and scratch run dirs
- `doc/` — assorted G1 design/reference notes

## Conventions

- Legacy C style: tabs, ANSI prototypes for definitions, terse names. Match the
  surrounding file; don't reformat untouched code.
- **Golden output is the contract.** Behavior changes that alter engine output
  must be deliberate and the snapshot updated in the same change with a note on
  why. Modernization changes (prototypes, casts, dead-code removal) must produce
  byte-identical golden output.
- Build config lives in `CMakeLists.txt`. The per-target compile flags are
  inlined into each `add_executable` block (see lines ~222 and ~262), *not*
  applied via the `phase_N_build_flags()` functions — those are roadmap
  scaffolding (defined, not yet called). `LEGACY_C_FLAGS_STRICT` is likewise
  staged but unused.

## Modernization status

A 5-phase ladder is documented as `phase_N_build_flags()` functions in
`CMakeLists.txt`. What is actually *enforced* is inlined into both targets as
`-Werror`. Current state:

| Phase | Scope | State |
|-------|-------|-------|
| 1 | `int-to-pointer-cast`, `pointer-to-int-cast` | ✅ enforced (`-Werror`) |
| 2 | `incompatible-pointer-types` | ✅ enforced |
| 3 | `int-conversion` | ✅ enforced |
| 3.5 | **Remove dead/unused source files** | ✅ done |
| 4 | `strict-prototypes`, `missing-prototypes`, `implicit-function-declaration` | ⬜ not started (next) |
| 5 | `missing-declarations` + sanitizers in CI | ⬜ wired (asan preset), not enforced |

Phases 1–3 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards build clean as errors. Genuine K&R *function definitions* are already
gone (`-Wold-style-definition` is 0). The remaining work is declarations,
headers, and dead files.

### Phase 3.5 — Remove dead/unused source files ✅ done

Removed five `lib/*.c` files that were never in any build target (and so never
compiled or linked into g1). These almost certainly belong to the later g3/tag
engines, not g1; recover from git history if ever needed:

- `accept_ents.c`, `effects.c`, `entity_builds.c` — list modules, referenced
  only by declarations in `lib/lists.h`; their list types had no use anywhere
  in the compiled tree. Declarations pruned from `lib/lists.h`.
- `checked_alloc.c` (+`.h`), `ring_buffer.c` (+`.h`) — self-contained utilities,
  included only by their own headers; deleted with them.

**Retained: `lib/plist.c`** (pointer list) and its `lib/lists.h` declarations.
Unlike `ilist` (which stores `int`), `plist` stores `void *`, so it's needed by
the upcoming 64-bit refactoring wherever pointers are kept in a list. It's not
wired into any build target yet (nothing uses it); add it to the appropriate
`target_sources` in `CMakeLists.txt` when the first caller appears. It compiles
clean under the enforced phase 1–3 flags.

Verified byte-identical golden output after a clean build (`golden_check.sh`
→ `YES`).

### Phase 4 — Prototypes & declarations (after 3.5)

Turn the phase-4 warnings into errors (`phase_4_build_flags()` /
`-Werror=strict-prototypes,missing-prototypes,implicit-function-declaration`).
Approximate current warning volume across the tree:

- ~1,300 `-Wstrict-prototypes` — empty-paren declarations like
  `extern char *readlin();` (mostly in headers; ~770 in `.h` files)
- ~590 `-Wimplicit-function-declaration` — calls with no visible declaration
- ~560 `-Wmissing-prototypes` — non-static functions defined without a header
  prototype

Suggested order: prototype the headers first (knocks out the bulk of strict +
implicit warnings at once), then make file-local functions `static` or add
their prototypes, then flip the flags to `-Werror` and remove the matching
`-Wno-*` suppressions from `LEGACY_C_FLAGS`. Keep golden output identical.

### Phase 5 — Lock down (later)

Enable `-Werror=missing-declarations` and wire the `asan-ubsan` preset into CI
so sanitizers run against the golden flow.
