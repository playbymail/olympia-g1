# CLAUDE.md

Guidance for working in the **Olympia G1** repository.

## What this is

G1 is the first-generation Olympia play-by-mail (PBM) strategy game engine
(~60K lines of C), the ancestor of the later G2/G3/TAG engines. This repo is a
standalone extraction of G1 from the original multi-engine monorepo; it builds
on its own with CMake.

The code is legacy C originally targeting **32-bit** systems. An active
modernization effort is bringing it to clean **C11 on 64-bit**. See
[Modernization status](#modernization-status) and the full
[BUILD_HISTORY.md](BUILD_HISTORY.md) — read them before changing build flags or
touching prototypes/headers.

## Build

Requires CMake (>= 4.1), Ninja, and Clang or GCC.

```bash
cmake --preset debug
cmake --build --preset debug
# Binaries: build/debug/olympia-g1, build/debug/mapgen-g1
```

Presets (`CMakePresets.json`): `debug` (default), `release`, `asan-ubsan`.
The `asan-ubsan` preset sets `OLYMPIA_SANITIZE=ON` with address+undefined.

## Test — golden snapshots (must stay green)

Any change must keep the golden tests passing. The olympia check printing `YES`
is the gate.

```bash
./run/mapgen/mapgen.sh                     # generate gate/loc/road
./run/olympia-g1.sh                        # extract fixtures, run a turn, save DB
./tests/olympia/golden_check.sh            # YES = match (exit 0)
./tests/olympia/golden_check.sh --update   # refresh snapshot (only when output is intentionally changed)
```

The golden is a **sha256 hash manifest** (`tests/olympia/golden/manifest.sha256`):
one `<sha256>  <relpath>` line per goldened run file, sorted. The gate hashes
every byte of every file and diffs the manifest, so it is portable across
macOS/Linux and cannot be fooled by equal-size content edits. See
[Golden harness — sha256 manifest (issue #4)](BUILD_HISTORY.md#golden-harness--sha256-manifest-issue-4).

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
- Build config lives in `CMakeLists.txt`. There is **one flag set for the whole
  project**: `olympia_compile_flags(tgt)` holds every warning flag (suppressions
  + the locked-in `-Werror` classes) and is applied identically to both targets;
  `olympia_enable_sanitizers(tgt)` adds the optional asan/ubsan instrumentation.
  Enforced classes are written as a deliberate `-Wfoo -Werror=foo` pair, one per
  line, as a record of completed modernization work — a comment block at the top
  of the file explains the convention; don't collapse the pairs. Optimization
  level is driven by `CMAKE_BUILD_TYPE` (via the presets), not hardcoded per
  target.

## Git workflow

- **Do not create git branches until explicitly asked.** Do the work as commits
  on the current branch (usually `main`). Branches and PRs are created only when
  the user says they are ready to deal with PRs — wait for that request.
- Likewise, do not push or open PRs unless explicitly asked.

## Modernization status

The C11/64-bit modernization ran as a phased warning ladder and is **complete**.
Every targeted warning class is now enforced as `-Werror` via
`olympia_compile_flags()` in `CMakeLists.txt` (applied to both targets), and the
golden flow runs clean under AddressSanitizer + UndefinedBehaviorSanitizer. The
per-phase scaffolding has been removed; the remaining work is the actual
32→64-bit refactoring the ladder cleared the way for.

Enforced classes (all `-Werror`):

| Phase | Scope |
|-------|-------|
| 1 | `int-to-pointer-cast`, `pointer-to-int-cast` |
| 2 | `incompatible-pointer-types` |
| 3 | `int-conversion` |
| 3.5 | removed dead/unused source files |
| 4 | `strict-prototypes`, `missing-prototypes`, `implicit-function-declaration` |
| 5 | `missing-declarations` + sanitizers wired |
| 6 | `shorten-64-to-32` (Clang-guarded) + `sizeof-pointer-memaccess` |
| 7 | `sign-conversion` + `seed[3]` signedness fix |
| 8 | `return-type` (+ `return-mismatch`, Clang-guarded) |

Also locked in: `implicit-int-conversion` (code-quality, not a 64-bit hazard),
`-Wformat`/`-Werror=format` vararg checking, a portable sha256-manifest golden
harness, and a deterministic newsletter date for reproducible goldens.

**Before changing build flags, prototypes, or headers, read
[BUILD_HISTORY.md](BUILD_HISTORY.md)** — it has the full phase-by-phase record,
the traps each class hid, and the rationale behind every locked-in flag.
