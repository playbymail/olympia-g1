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
[Golden harness — sha256 manifest (issue #4)](#golden-harness--sha256-manifest-issue-4).

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

## Git workflow

- **Do not create git branches until explicitly asked.** Do the work as commits
  on the current branch (usually `main`). Branches and PRs are created only when
  the user says they are ready to deal with PRs — wait for that request.
- Likewise, do not push or open PRs unless explicitly asked.

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
| 5 | `missing-declarations` + sanitizers | ✅ enforced (`-Werror`) + asan/ubsan green |
| 6 | `shorten-64-to-32` (Clang-guarded) + `sizeof-pointer-memaccess` | ✅ enforced (`-Werror`) |
| 7 | `sign-conversion` + the `seed[3]` signedness fix | ✅ enforced (`-Werror`) |
| 8 | `return-type` (+ `return-mismatch`, Clang-guarded) | ✅ enforced (`-Werror`) |

Phases 1–5 are complete and locked in — the dangerous 32→64-bit pointer/int
hazards (bad casts, int/pointer conversions, and implicitly-declared functions
whose `int` return truncates a pointer) build clean as errors, every function
now has a real prototype, and the golden flow runs clean under
AddressSanitizer + UndefinedBehaviorSanitizer. The modernization ladder is
done; the remaining work is the actual 32→64-bit refactoring it cleared the
way for.

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

**Retired: `lib/plist.c`** and its `lib/lists.h` declarations (removed — see
below). It was kept as scaffolding for the 64-bit pointer-list work, but that
work was already done by other means: every pointer collection in the engine
uses an **element-typed** list (`item_ents_list`, `trades_list`, `skill_ents_list`,
`admits_list`, `orders_list`, `req_ents_list`, `exit_views_list`, `fights_list`,
`flag_ents_list`, `wait_args_list`, `cstrings_list`), declared in `lib/lists.h`
and used throughout `oly.h`/`loop.h`/the `.c` files. `ilist` (`int *`) stays for
genuine int lists (entity/skill numbers). The generic `plist` (`void **`) had
zero call sites and was never wired into a build target, so it was deleted
outright — see the *Generic `plist` retired* note below. This mirrors the
sibling `../olympia-g3` engine's `issues/2` migration (final commit `ce5bb33`).

Verified byte-identical golden output after a clean build (`golden_check.sh`
→ `YES`).

### Generic `plist` retired ✅ done

Deleted `lib/plist.c` and the `typedef void **plist;` + its 15 `extern plist_*`
declarations from `lib/lists.h`. `grep -rn '\bplist' olympia/ mapgen/ lib/
CMakeLists.txt` is now empty.

This was already safe: g1's pointer collections had all been moved to
element-typed lists (the `*_list` family above), so the generic untyped `plist`
had no callers and was not in any build target. Removing it means a future
type mismatch (a list of one element type passed where another is expected)
is now a **compile error** with no `(plist)` cast left to silence it — the same
guarantee `../olympia-g3` locked in with its `ce5bb33`. `ilist` (the legitimate
`int *` list) is untouched.

Both presets build clean, the golden gate stays `YES` (byte-identical), and the
asan/ubsan gate stays clean (exit 0, zero diagnostics).

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

### Phase 5 — Lock down ✅ done

`-Werror=missing-declarations` is now enforced on both targets, and the
`asan-ubsan` preset is wired into the build so the address + undefined
sanitizers run against the golden flow.

- **`missing-declarations`** measured **0** across the tree before being
  promoted to `-Werror` — Phase 4's per-target `proto.h` had already given
  every function a prior declaration, so this class was free to lock down.
- **Sanitizers were wired, not just staged.** `olympia_enable_sanitizers()`
  was *defined but never called*, so the `asan-ubsan` preset compiled without
  any instrumentation. It is now invoked on both targets. A latent bug in the
  `OLYMPIA_SANITIZERS` cache `set()` (default value misplaced after the
  docstring, so `${OLYMPIA_SANITIZERS}` expanded to the leftover `CACHE STRING
  …` tokens and the compile failed) was fixed at the same time.
- **Two real UB bugs the sanitizers exposed, fixed as real bugs** (golden
  output stays byte-identical — the affected output is debug-only stdout /
  excluded `master`+`lore` files, and the values are unchanged):
  - `olympia/z.c` — the legacy guard-allocator returned `base + sizeof(int)`,
    leaving every block only **4-byte aligned**. Fine on 32-bit, but once a
    block holds 8-byte pointers on 64-bit it's a misaligned load/store (UB,
    and the root cause of pervasive `struct box *` misalignment via the `bx`
    table). The header is now padded to `_Alignof(max_align_t)`, and the
    arbitrary-offset trailing magic marker is read/written via `memcpy`.
  - `olympia/gm.c` — `gm_show_gold()` computed `part * 100 / sum` for 13 gold
    sources; with no gold recorded `sum == 0`, a division by zero (UB; it
    silently yielded 0 on arm64 rather than trapping, and would SIGFPE on
    x86). Guarded with a `gold_pct()` helper that returns 0 when `sum == 0`.
- The full golden flow (`mapgen` + a turn) now runs clean under
  `-fsanitize=address,undefined -fno-sanitize-recover=all` (exit 0, zero
  diagnostics), and `golden_check.sh` → `YES` on the canonical debug build.

Run the sanitizer gate locally with:

```bash
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan
OLYMPIA_PRESET=asan-ubsan ./run/mapgen/mapgen.sh
OLYMPIA_PRESET=asan-ubsan ./run/olympia-g1.sh   # aborts on any UB/ASan error
OLYMPIA_PRESET=asan-ubsan ./tests/olympia/golden_check.sh   # YES
```

There is no CI workflow yet — automation is deferred until the repo is ready
to run PRs.

### Phase 6 — `shorten-64-to-32` + `sizeof-pointer-memaccess` (issue #10) ✅ done

The **core 64-bit phase**: `-Wshorten-64-to-32` isolates exactly the
`long`/`size_t`/`ssize_t`/`time_t`-into-`int` width truncations that diverge
between ILP32 and LP64. Both flags are now `-Werror` on both targets — the
shorten flag Clang-guarded (`if (CMAKE_C_COMPILER_ID MATCHES "Clang")`, it's a
Clang-only diagnostic), `sizeof-pointer-memaccess` portable. They're inlined in
both `target_compile_options` blocks alongside the Phase 1–5 flags; the dead
`-Wno-sizeof-pointer-memaccess` suppression was removed from `LEGACY_C_FLAGS`.

**10 `-Wshorten-64-to-32` sites** (9 olympia in 5 files + 1 mapgen), all fixed
representation-preservingly (the implicit conversion already truncated exactly
this way, so golden stays byte-identical):

- `z.c` `readlin` path: `nread` retyped `int`→`ssize_t` (its source is
  `read()`), clearing both `nread = read(...)` assignment sites; downstream
  indexing/compares are unaffected.
- `z.c`/`mapgen/z.c` `str_save`: `(unsigned)` cast on `strlen(s) + 1` feeding
  `my_malloc(unsigned size)`.
- `strlen()`→`int` name/line/word lengths, provably `<2^31`: documented `(int)`
  casts in `z.c` `fuzzy_strcmp`, `c2.c` `line_length_check`, `check.c`
  `check_loc_name_lengths`, `eat.c`, `report.c` `strip_leading_stupid_word`.

**`sizeof-pointer-memaccess` audit:** warn-only probe (suppression removed)
reported **0 hits** across both targets — no `sizeof(ptr)`-should-be-`sizeof(*ptr)`
defects (those grow 4→8 bytes on LP64, a real corruption risk), so it was
promoted straight to `-Werror`.

Probe (`-Wshorten-64-to-32`) now reports 0; both targets build clean with the
new `-Werror` flags; debug and asan-ubsan golden gates both `YES`
(byte-identical) and asan/ubsan clean.

### Phase 7 — `sign-conversion` + `seed[3]` signedness fix (issue #11) ✅ done

`-Wsign-conversion` catches signed↔unsigned conversions that can diverge on
LP64. It is now `-Werror` on both targets (inlined alongside the Phase 1–6
flags). Warn-only inventory reported **30 hits in 10 files**, all in the
olympia target (mapgen had 0); all fixed representation-preservingly.

- **The `seed[3]` signedness mismatch (the headline defect).** `z.c:734`
  defines `unsigned short seed[3]` — the `erand48`/`drand48` state contract —
  but `io.c:2459` and `io.c:2583` declared `extern short seed[3]` (signed), a
  type-mismatch that is UB. Both externs are now canonicalized to
  `unsigned short`, so `seed[3]` is unsigned everywhere and the save routine
  (`io.c:2602-2604`, `fprintf("%d", seed[i])`) writes the *correct* value: an
  `unsigned short` promotes to a non-negative `int` (0..65535).
  - **This is the one deliberate golden change in Phase 7** — and the plan's
    "golden-safe / identical bytes" premise was simply wrong, which is why the
    `do-not---update`-without-investigating rule earned its keep. The claim
    assumed the fixed golden seeds were "small positive ints"; in fact the
    input fixture seeds span the full 16-bit range (e.g. `seed0=-3645`), and
    after a turn the saved RNG state routinely has values ≥ `0x8000`. The old
    *signed* extern wrote those negative (`-28312`); the correct *unsigned*
    extern writes them positive (`37224`) — same 16 bits, but the signed text
    was wrong for an unsigned quantity. The golden `system` file was
    re-baselined in the same commit to hold the correct unsigned seed values
    (the only file that changed; one manifest line). The round-trip is
    unaffected — `load` does `atoi()` then truncates back into the 16-bit slot,
    recovering the exact bits from either spelling — so old saved games still
    load correctly.
- **The other 29 hits were all `int → size_t`/`unsigned`**, fixed with explicit
  representation-preserving casts (every value is provably ≥ 0):
  - **`qsort` `nmemb` (the bulk).** The element-typed list accessors
    (`ilist_len`, `admits_len`, `item_ents_len`, `skill_ents_len`) return
    `int`, feeding `qsort`'s `size_t nmemb`. Cast `(size_t)` at each call site
    in `check.c`, `gm.c`, `input.c`, `perm.c`, `report.c`, `seed.c`, `swear.c`,
    `use.c` — and **once in the shared `loop_known` macro** (`loop.h:89`),
    which cleared 6 sites across `gm.c`/`io.c`/`report.c`/`summary.c` at a
    stroke.
  - **`my_malloc` size** (`sout.c:31`): `(unsigned)` cast on `spaces_len + 1`.
  - **array index** (`perm.c:451`): `(size_t) spaces_len - strlen(header)` —
    making explicit the int→size_t promotion the `size_t` operand already
    forced.
  - **guard-allocator size store** (`z.c:260`, `:283`): `*((int *) p) =
    (int) size;` — `size` is `unsigned`, the header slot is `int`; the
    truncation was already implicit.

Probe (`-Wsign-conversion`) now reports 0; both targets build clean with the
new `-Werror=sign-conversion`; debug and asan-ubsan golden gates both `YES`
(the `system` seed re-baseline applied to both) and asan/ubsan clean (exit 0).
The 29 cast fixes are byte-identical; only the seed save changed, deliberately.

### Phase 8 — `return-type` + `return-mismatch` (issue #13) ✅ done

The last suppressed class with 64-bit relevance. A non-void function that
falls off the end (or hits a bare `return;`) leaves the caller reading a
**garbage register** — on LP64 an 8-byte garbage `long`/pointer (vs 4 bytes
on ILP32), so the failure mode is genuinely worse on 64-bit. `-Wno-return-type`
and `-Wno-return-mismatch` were the last two such suppressions in
`LEGACY_C_FLAGS`; both are now removed and the classes locked as `-Werror`.

- **`-Werror=return-type`** is inlined unconditionally in both
  `target_compile_options` blocks (portable: GCC's `-Wreturn-type` covers
  *both* the fall-off and the bare-`return;` cases).
- **`-Werror=return-mismatch`** (clang's split-out diagnostic for a bare
  `return;` in a non-void fn) is added inside the existing **Clang-guarded**
  block next to `-Wshorten-64-to-32` — real GCC does not know the flag and
  folds that case into `-Wreturn-type` already.

**Inventory trap (documented so it doesn't bite the next class):** clang's
default `-ferror-limit=19` *truncated the first warn-only pass*.
`return-mismatch` is **error-by-default** in this clang, so `mapgen.c` hit
the error limit at its early `return-mismatch` sites and clang stopped before
compiling its second half — the initial inventory looked like ~30 sites when
the real count was **12 `return-mismatch` + ~74 `return-type`**. Build with
`-- -k 0` *and* re-inventory after each fix round until the list is empty; do
not trust a single truncated pass.

Two representation-preserving fix shapes (golden byte-identical — every
fall-off path is unreachable in the golden run, and the void conversions
don't change behavior since the discarded returns were already garbage):

- **Corrected the return type to `void`** (~40 functions) for the legacy
  default-`int` helpers that never produce a value and whose callers ignore
  the return — definition **and** every declaration (`proto.h`, `stack.h`,
  `use.h`, `mapgen/proto.h`) changed in lockstep. The bulk is the mapgen
  pipeline (`set_regions`, `random_province`, `make_*`, `gate_*`, `count_*`,
  `print_*`, `bridge_*`, `dump_*`, `open_fps`, `read_map`, …) plus olympia
  void-semantic helpers (`check_db`, `call_init_routines`, `queue`,
  `init_spaces`, `move_prisoner`, `learn_skill`, `gm_count_stuff`,
  `mail_reports`, the `write_*` report writers). The clean `-Werror` build is
  itself the proof no caller consumed a return — a `void` used as a value is a
  compile error.
- **Added the missing return value** where the function genuinely returns one:
  a defensive `return <default>;` after an unreachable `assert(FALSE)` for the
  switch/lookup helpers (`hinder_med_chance`, `repair_points`, `liner_desc`,
  `rank_s`, `exp_s`, `fog_excuse`, `fort_covers`, `lead_char_pos`,
  `find_attacker`/`find_defender`, `name_guild`, `destroy_auraculum`,
  `here_precedes`, `promote_after`, `v_decree`, `hidden_count_to_index`,
  `loc_depth`, `mage_menial_how`, `reduce_qty`), and a real value on the
  genuinely-reachable fall-off paths (`v_unseal_gate`/`i_use`/`i_repair` →
  `TRUE`, `d_rally` → `TRUE`, `new_storm` → `new`). **`i_repair` stayed
  `int`** — it is the `repair` interrupt handler stored in `cmd_tbl` as
  `int (*)(struct command *)`; void-converting it broke the function-pointer
  table (the one revert during the sweep).

Probes now report 0; both targets build clean with the new `-Werror`; debug
and asan-ubsan golden gates both `YES` (byte-identical) and asan/ubsan clean
(exit 0). This completes the planned 32→64-bit warning ladder
(`doc/64bit-refactoring-plan.md`, chain A→B→C→D→E).

### Format/vararg lockdown (issue #7) ✅ done

`legacy_build_flags()`/`LEGACY_C_FLAGS` set `-Wno-format`, which silenced
*every* printf-family check engine-wide even though `out`/`wout`/`wiout`/`log`/
`sout` carry `__attribute__((format(printf,…)))`. With checking off, missing,
extra, and mistyped varargs all slipped past `-Werror`. The sibling **olympia-tag**
found 46 such defects (playbymail/olympia-tag#12); g1 shares the ancestry.

Method: gave the one un-annotated wrapper (`queue`, `vsprintf`-based) its
`format(printf,2,3)` attribute, then re-enabled `-Wformat` in a scratch build to
inventory the tree. **11 real defects** found and fixed (the 5 remaining
`-Wformat-security` non-literal `out(who, buf)`/`out(who, t->none_*)` cases are
intentional and stay deferred behind `-Wno-format-security`/`-Wno-format-nonliteral`
until a dedicated hardening pass). Golden output is byte-identical — every
affected message is turn output not captured in the 30-file snapshot.

- **Memory-unsafe (`-Wformat-insufficient-args`, bare `%s` → garbage-pointer
  deref):** `scry.c` `scry_show_where` dropped the subject name from both
  `"%s is in %s."` calls (canonical form at `scry.c:429` confirms intent);
  `c2.c` `board_message` had a stray third `%s` in `"%s%s%s boarded %s%s"` (its
  twin `unboard_message` shows the correct two-leading-`%s` shape).
- **Type mismatch (`-Wformat`, 64-bit-sensitive):** `main.c` `%d`←`sizeof`
  (→`%zu`); two `u.c` `stage()` timers `%d`←`long` (→`%ld`); and `c2.c`'s
  `"…break at %d%%%."` — the trailing `%.` was an incomplete specifier that
  swallowed the period (→`%%.`).
- **Extra-args:** three vestigial leftover args removed (`garr.c` `add_s(n)` —
  `n` was also read uninitialized, so its decl went too; two `just_name(…)` in
  `stack.c`). `storm.c` `"Killed %s men of %s."` was a *dropped-output* case:
  the `aura_used == 1 ? "man" : "men"` arg was computed but the format hardcoded
  `men`; the format now uses it (`"Killed %s %s of %s."`).
- **Capstone:** `-Wno-format` is replaced by `-Wformat -Werror=format` on both
  targets, so the whole class is now a compile error going forward — locking
  width/format correctness in *before* the remaining 32→64-bit conversion, where
  `%d`←`long`/`size_t` and `int*`-vs-`char*` `scanf` diverge between ILP32 and
  LP64.

### Golden harness — sha256 manifest (issue #4) ✅ done

The golden gate used a dry-run checksum rsync:
```
rsync -a -n -c --delete "${EXCLUDES[@]}" "${RUN_LIB}/" "${GOLDEN}/"
```
On macOS `/usr/bin/rsync` is **openrsync** (protocol 29), which in dry-run (`-n`)
does **not** honor `-c` (checksum) — it falls back to a size+mtime quick-check.
Any content change that preserved file size passed UNDETECTED and the gate
printed a false `YES` (it was masking the issue #5 fixed-width masthead date,
and could mask others). The byte-for-byte "golden is the contract" guarantee
was simply not enforced on macOS for equal-size edits.

Fix mirrors `../olympia-g3`/`../olympia-g2`: the golden tree is replaced by a
single **sha256 hash manifest** `tests/olympia/golden/manifest.sha256` — one
`<sha256>  <relpath>` line per goldened run file, sorted, diffed. This hashes
every byte of every file and is portable across macOS/Linux. The previous
rsync `EXCLUDES` are preserved verbatim as the manifest's `find … -prune` set
(`master`, `email`, `totimes`, `forward`, `factions`, `lore/`, `skill`, plus
`.DS_Store`), so the manifest covers exactly the same 30 files that were
goldened before. The literal golden tree was removed.

Verified: the issue #4 same-size repro (flip one byte of `system`, size
unchanged) now makes the gate go **NO**; a clean full run (mapgen + turn) still
prints **YES**. G1 has no flaky-file holdout (output is deterministic given the
issue #5 const-date flag), so the g3 `FLAKY_FILES` machinery is omitted.

### Deterministic newsletter date (issue #5) ✅ done

`times_masthead()` in `olympia/c2.c` stamped the **wall-clock date** into the
committed golden file `times_0`, making the golden gate date-dependent (it would
fail on any day other than the capture day). On macOS this was masked by the
harness reliability bug (issue #4: openrsync ignores `-c`, and the date line is
fixed-width so the file size never changes); on Linux with GNU rsync it already
failed on any non-capture day.

Fix mirrors `../olympia-g3`'s `test-use-const-report-date` flag:
- `main.c`: `int test_use_const_report_date = FALSE;` plus a pre-`getopt()`
  argv scan that pulls the long-form token `test-use-const-report-date` out of
  argv (getopt only handles single-char options), sets the global, and compacts
  argv so remaining options still parse.
- `c2.c` `times_masthead()`: when the flag is set, `strcpy(date, "January 1,
  2000")` instead of the `localtime()`-derived string.
- `oly.h`: `extern int test_use_const_report_date;`
- `run/olympia-g1.sh`: passes `test-use-const-report-date` on the turn run.
- Re-baselined golden `times_0` (the only dated golden file) with the flag on.

Affects ONLY the newsletter masthead date; all other output is byte-identical.
Normal play (no flag) still prints the real wall-clock date.
