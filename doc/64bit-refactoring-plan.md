# Olympia G1 — 32→64-bit Refactoring Plan (Phase 6+ ladder)

## Context

G1 is a ~60K-line legacy C engine originally written for **32-bit (ILP32)** and
now being brought to clean **C11 on 64-bit (LP64)**. The existing modernization
ladder (Phases 1–5, see `CLAUDE.md`) is complete and locked as `-Werror`: the
*dangerous* pointer/int cast hazards (int↔pointer casts, incompatible pointer
types, int-conversion, implicit declarations, missing prototypes/declarations)
already build clean as errors, and the golden flow runs clean under asan/ubsan.

What remains is the **subtler** class of 64-bit hazards that still compile
silently: **LP64→ILP32 width truncations** (`long`/`size_t`/`ssize_t`/`time_t`
stored into `int`/`short`) and **signed↔unsigned conversions**. These are not
yet caught by any warning. Compile probes against the real tree give the scope:

| Warning class | Hits (olympia+lib) | mapgen | Nature |
|---|---|---|---|
| `-Wshorten-64-to-32` | **9** in 5 files | 1 | True LP64 truncations — the core 64-bit class |
| `-Wsign-conversion` | **30** in 12 files | 0 | signed↔unsigned; includes the `seed[]` defect |
| `-Wconversion` umbrella | 194 | 3 | ~154 are int→short narrowing — **arch-independent, not a 64-bit hazard** |

The goal: extend the proven warning-ladder methodology to surface and fix the
width/sign hazards, **keeping golden output byte-identical at every step**.

### Decisions
- **Full ladder**: Phases 6, 7, 8 + the latent `init_random` fix.
- **`-Wconversion`**: deferred off the 64-bit critical path, scheduled as a
  **separate per-file hardening pass** (its own issue; `io.c` is 92 of 154).
- **Drop the 32-bit build entirely.** There is no way to build ILP32 binaries
  without VMs, the committed goldens are 64-bit captures, and the stale
  `tests/mapgen/golden` 32-bit baseline already diverges. `BUILD_32BIT` and all
  its docs/instructions are removed — the LP64 golden is the sole contract.

## The non-negotiable invariant

Every single commit must pass, with no golden update:

```bash
cmake --build --preset debug
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan
./run/mapgen/mapgen.sh && ./run/olympia-g1.sh && ./tests/olympia/golden_check.sh   # → YES
OLYMPIA_PRESET=asan-ubsan ./run/mapgen/mapgen.sh \
  && OLYMPIA_PRESET=asan-ubsan ./run/olympia-g1.sh \
  && OLYMPIA_PRESET=asan-ubsan ./tests/olympia/golden_check.sh                      # → YES
```

A truncation/sign fix is **representation-preserving** (an explicit cast or a
wider local type produces the same machine code / same stored value the implicit
conversion already produced), so golden cannot change. If it *does* change,
**STOP** — a value actually overflowed 32 bits in the golden run; that is a real
latent bug to fix, never a reason to `--update` the golden. If asan/ubsan goes
red, the fix exposed real UB — fix the value flow, don't suppress.

## Build-system mechanic

- Add each new warning class **inline** in both targets' `target_compile_options`
  blocks (`CMakeLists.txt` ~line 224 `mapgen-g1`, ~line 268 `olympia-g1`),
  matching the Phases 1–5 convention. The `phase_N_build_flags()` functions and
  `LEGACY_C_FLAGS_STRICT` remain unused scaffolding — do **not** rewire them as
  part of this work (a structural change bundled with a flag flip makes a
  regression ambiguous).
- **`-Wshorten-64-to-32` is Clang-only.** Guard it:
  `if (CMAKE_C_COMPILER_ID MATCHES "Clang")`. Do not put it unguarded in a shared
  block. (Primary toolchain here is Clang on macOS; the guard keeps GCC working.)
- Per class: for tiny classes (shorten-64-to-32, 9 hits) go straight to the fix
  commit then a lock commit. For larger classes (sign-conversion, 30) add the
  warn-only flag first (inventory in the build log), fix per-file, then a final
  commit flips `-Wflag` → `-Werror=flag` on both targets.
- Update the `CLAUDE.md` "Modernization status" table in the same commit that
  **locks** each phase (existing convention).

## Work breakdown — sequenced issues + commits on `main`

Per repo convention: commit on `main`, **no branches/PRs until explicitly asked**.
One GitHub issue per item below, each independently golden-gated. The dependency
chain is strict: **A → B → C → D → E**, with **F** an independent side track.

### Issue A — Drop 32-bit build support (do first; pure cleanup)
- `CMakeLists.txt`: remove the `BUILD_32BIT` option + `if(BUILD_32BIT)` block
  (lines ~178–195).
- `CLAUDE.md`: remove the "### 32-bit build (Linux only…)" section (lines ~30–37);
  leave the historical narrative refs (ILP32 in commentary) intact.
- `doc/modernization-prototypes-playbook.md:409`: drop/adjust the stale-32-bit
  mapgen-golden note.
- Decide `tests/mapgen/golden` (stale 32-bit baseline, **not** used by the
  olympia gate): remove it, or add a README noting it's unused/historical.
  Confirm nothing consumes it first (`run/mapgen/mapgen.sh`, `golden_check.sh`).
- Golden gate unaffected (no source change) — still run it.

### Issue B — Phase 6: `-Wshorten-64-to-32` (+ `sizeof-pointer-memaccess` audit)
The core 64-bit phase. 9 hits in 5 files (`z.c` 318/467/489/713/714 etc.), mostly
`strlen`/`read`/`ssize_t` results stored into `int`.
- Commit 1 — fix the 9 sites: type the variable to its source (`ssize_t nread`
  for the `read()` sites in `z.c`; `size_t` where the value flows on) or add a
  documented `(int)` cast where the value is provably small (e.g. `strlen` of a
  fixed name). Gate.
- Commit 2 — audit `-Wno-sizeof-pointer-memaccess` (a `sizeof(ptr)` that should be
  `sizeof(*ptr)` goes 4→8 bytes on LP64 — real corruption risk). Warn-only first;
  fix any hits or document. Gate.
- Commit 3 — lock `-Werror=shorten-64-to-32` (Clang-guarded) on both targets;
  promote `sizeof-pointer-memaccess` if clean. Update CLAUDE.md table. Gate.

### Issue C — Phase 7: `-Wsign-conversion` (+ seed signedness fix)
30 hits in 12 files.
- Commit 1 — add `-Wsign-conversion` warn-only (inventory). Gate.
- Commit 2 — fix the **`seed[3]` signedness mismatch**: `z.c:734` defines
  `unsigned short seed[3]` (the `erand48` contract) but `io.c:2459` and
  `io.c:2583` declare `extern short seed[3]`. Canonicalize both externs to
  `unsigned short`. Golden-safe: the fixed golden seeds are small positive ints
  parsed via `atoi` (`io.c:2503-2511`) and saved via `%d` (`io.c:2602-2604`) —
  identical bytes. Gate.
- Commit 3 — fix remaining sign-conversion hits, sliced by file. Gate each.
- Commit 4 — lock `-Werror=sign-conversion` on both targets. Update CLAUDE.md
  table. Gate.

### Issue D — Latent `init_random()` truncation (standalone correctness)
`z.c:741-746` seeds from `time(NULL)` and `getpid()` with `l >> 16` and 16-bit
truncation — architecture-dependent, but **dead on the golden run** (only used
when no seed is supplied; golden always supplies fixed seeds). Fix defensively:
mask explicitly (`& 0xFFFF`) so behavior is well-defined on both archs. One small
commit; golden unaffected **by construction** — state that in the message (the
warning that would catch it fires on a path the golden never exercises).

### Issue E — Phase 8: `return-mismatch` / `return-type` audit
A missing return value reads a garbage register — on LP64 that's an 8-byte
garbage long/pointer. `-Wno-return-mismatch`/`-Wno-return-type` are still
suppressed in `LEGACY_C_FLAGS`. Warn-only inventory → fix → lock, or document a
deferral with rationale + issue number if the hit list is large. Update table.

### Issue F — (separate track, not 64-bit) `-Wconversion` hardening
The ~154 `implicit-int-conversion` hits are int→short/char narrowing into entity
struct fields — **arch-independent, not a porting hazard**. Keep off the 64-bit
critical path. Either: (a) document the deferral with a `-Wno-`-style comment +
tracking issue (the established `format-security` pattern), or (b) schedule a
standalone per-file hardening pass (`io.c` first, 92 hits) to eventually reach
`-Werror=conversion` as code-quality. Not required for the port to be "done".

## Critical files
- `CMakeLists.txt` — two inline `target_compile_options` blocks (~224, ~268);
  `BUILD_32BIT` block (~178–195); `LEGACY_C_FLAGS` (~136–161).
- `olympia/z.c` — shorten-64-to-32 sites (318/467/489/713/714); `seed[3]` def
  (734); `init_random` truncation (741–746); engine's own portable
  `drand48`/`erand48` (RNG is arch-independent).
- `olympia/io.c` — `extern short seed[3]` mismatches (2459/2583); seed
  parse/save (2503–2511 / 2602–2604); bulk of the deferred implicit-int hits.
- `run/olympia-g1.sh` — fixed RNG seed env vars (152–155); golden run driver.
- `tests/olympia/golden_check.sh` — the `YES` gate (sha256 manifest).
- `CLAUDE.md` — Build section (32-bit removal) + Modernization status table.
- `doc/modernization-prototypes-playbook.md:409` — stale-32-bit note.

## Verification (every commit)
1. `cmake --build --preset debug` — clean (no new warnings; `-Werror` after lock).
2. `./run/mapgen/mapgen.sh && ./run/olympia-g1.sh && ./tests/olympia/golden_check.sh` → **YES**.
3. asan/ubsan flow (build + run + check via `OLYMPIA_PRESET=asan-ubsan`) → **YES**, exit 0.
4. On lock commits, confirm both targets compile with the new `-Werror=` flag.

A phase is "done" when its flag is `-Werror` on both targets, golden is
byte-identical, asan/ubsan is clean, and the CLAUDE.md table row is updated.
