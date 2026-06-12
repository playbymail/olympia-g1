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
| 4 | `strict-prototypes`, `missing-prototypes`, `implicit-function-declaration` | ✅ enforced (`-Werror`) |
| 5 | `missing-declarations` + sanitizers in CI | ⬜ wired (asan preset), not enforced (next) |

Phases 1–4 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards (bad casts, int/pointer conversions, and implicitly-declared functions
whose `int` return truncates a pointer) build clean as errors, and every
function now has a real prototype. The remaining work is Phase 5
(`missing-declarations` + sanitizers in CI).

> **Correction:** earlier notes claimed K&R *definitions* were "already gone"
> because `-Wold-style-definition` reported 0. That was the wrong probe — clang
> reports K&R definitions under `-Wdeprecated-non-prototype`, and the tree
> actually had **95** of them (54 in the map generator). They are now all
> converted to ANSI. See `doc/modernization-prototypes-playbook.md`.

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

### Phase 4 — Prototypes & declarations ✅ done

`strict-prototypes`, `missing-prototypes`, and `implicit-function-declaration`
are now `-Werror` on both targets (inlined at the `target_compile_options`
blocks ~lines 224 and 267), and the dead `-Wno-implicit-function-declaration`
/ `-Wno-deprecated-non-prototype` suppressions have been removed from
`LEGACY_C_FLAGS`. All three classes measure **0** across the tree.

What it took (full method + every trap in
`doc/modernization-prototypes-playbook.md`):

- Converted **95** K&R definitions to ANSI and ~240 empty-paren `name()`
  definitions to `name(void)` (probe with `-Wdeprecated-non-prototype`, *not*
  `-Wold-style-definition`).
- Generated a prototype header per target — **`olympia/proto.h`** (included at
  the end of `oly.h`, after the type defs and a `#include <stdio.h>` for
  `FILE`) and **`mapgen/proto.h`** (included from the map generator's own
  header). This one move cleared almost all of `missing-prototypes` and the
  internal `implicit-function-declaration` calls at once. `z.c` doesn't include
  `oly.h`, so its functions are declared in `z.h` instead.
- Deleted redundant empty-paren forward declarations (the `use.c`/`glob.c`
  command-handler blocks, scattered `extern T foo();` locals, header decls) and
  gave the surviving header decls real prototypes.
- Added the real libc headers (`string.h`, `stdlib.h`, `unistd.h`, `time.h`,
  `fcntl.h`, `sys/stat.h`) at the **top of `z.h` / `mapgen/z.h`**, before the
  engine's `bzero`/`bcopy`/`abs` shadow macros (and ahead of `oly.h`'s `wait`
  macro, which collides with `sys/wait.h` on macOS). `z.h` is the single
  chokepoint every engine TU includes first, so this supplies real prototypes
  for the remaining implicit libc calls without breaking the legacy macros.
- Latent bugs the new prototypes exposed, fixed as real bugs:
  `make_appropriate_subloc` arg-count mismatch; `queue()` made genuinely
  variadic; a wrong-return-type `clear_wait_parse` decl; an orphan
  `fetch_inside_name` (and `wrap_done`) decl; and the `qsort` comparators
  (incl. the last straggler `rank_comp`) canonicalized to
  `(const void *, const void *)`.

### Phase 5 — Lock down (next)

Enable `-Werror=missing-declarations` and wire the `asan-ubsan` preset into CI
so sanitizers run against the golden flow.
