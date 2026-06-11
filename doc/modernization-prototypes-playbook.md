# Prototypes & declarations modernization — playbook

A field guide for turning the three "Phase 4" warning classes into hard
errors on a legacy Olympia C engine:

- `-Werror=strict-prototypes`
- `-Werror=missing-prototypes`
- `-Werror=implicit-function-declaration`

This was worked out doing G1. **G2, G3, and TAG share the same ancestry and
the same hazards** — read this before repeating the exercise. The goal each
time: clean `-Werror` for those three flags with **byte-identical golden
output** (no behaviour change).

---

## The one thing to internalize

The three flags are not independent, and the most dangerous interactions are
invisible until you probe for them:

1. **K&R definitions are still present** even when `-Wold-style-definition`
   reports zero. Clang reports a K&R definition (`int f(a,b) int a; char *b;
   {`) under **`-Wdeprecated-non-prototype`**, *not* `-Wold-style-definition`.
   G1's CLAUDE.md claimed K&R defs were "already gone" — that was measured
   with the wrong flag. G1 actually had **95** of them (54 in the map
   generator alone).

2. **`-Werror=strict-prototypes` promotes `-Wdeprecated-non-prototype` to a
   hard error**, because clang treats the deprecated-non-prototype diagnostic
   as belonging to the strict-prototypes group for definitions. So the moment
   you flip `-Werror=strict-prototypes`, every surviving K&R or empty-paren
   *definition* breaks the build — even though `-Wno-deprecated-non-prototype`
   is in the flag list. (Flag *ordering* matters: the later
   `-Wstrict-prototypes` re-enables the group that the earlier
   `-Wno-deprecated-non-prototype` silenced. See "Flag ordering" below.)
   **Conclusion: you must convert every K&R/empty-paren definition to ANSI.
   There is no flag-only shortcut.**

3. **Under C11 + clang, `implicit-function-declaration` is a hard error by
   default** (ISO C99+ dropped implicit declarations). That is *why* the
   legacy flag list carries `-Wno-implicit-function-declaration`. On a 64-bit
   target this is the dangerous class: an implicitly-declared function is
   assumed to return `int`, which **truncates a returned pointer to 32 bits**.

---

## Order of operations (each step keeps golden green; flip the flags LAST)

1. **Convert K&R definitions to ANSI.** Probe for them with
   `-Wdeprecated-non-prototype` (see below). Filter to the message *"a
   function definition without a prototype"* — the same flag also fires on
   *calls* through old-style decls (*"passing arguments to X..."*) and on
   conflicting empty-paren *declarations*, which are not definitions.
2. **Convert empty-paren definitions `name()` → `name(void)`.** These are
   also under `-Wdeprecated-non-prototype` / `-Wstrict-prototypes`. Note the
   asymmetry in step "Empty-paren defs hidden behind a decl" below.
3. **Generate a prototype header from the definitions** and include it
   everywhere. This is the single biggest lever — it clears almost all of
   `missing-prototypes` *and* the internal `implicit-function-declaration`
   calls at once. (Details + gotchas below.)
4. **Delete now-redundant empty-paren forward declarations** — the big
   command-handler blocks (`int v_foo(), d_foo();`) in the dispatch-table
   files, scattered `extern T foo();` lines, and empty-paren decls in headers.
5. **Add libc `#include`s** for the standard functions still called
   implicitly (string.h, stdlib.h, unistd.h, time.h, …).
6. **Flip the three flags to `-Werror`**, delete the now-dead
   `-Wno-implicit-function-declaration` and `-Wno-deprecated-non-prototype`.
7. **Update CLAUDE.md.**

---

## Measurement / probe recipe

The legacy flag list *suppresses* the very warnings you need to count, and
suppression ordering hides them. Probe on a throwaway copy:

```bash
rm -rf /tmp/probe && cp -r . /tmp/probe && cd /tmp/probe && rm -rf build*
# strip the suppressions you want to surface
sed -i '' '/-Wno-implicit-function-declaration/d; /-Wno-deprecated-non-prototype/d' CMakeLists.txt
# enable as plain warnings; keep the build alive so you see ALL of them
cmake -S . -B b -G Ninja \
  -DCMAKE_C_FLAGS="-Wstrict-prototypes -Wmissing-prototypes \
    -Wno-error=implicit-function-declaration -Wno-error=int-conversion \
    -Wno-error=incompatible-pointer-types -Wno-error=int-to-pointer-cast \
    -Wno-error=pointer-to-int-cast" >/dev/null 2>&1
cmake --build b > probe.log 2>&1
```

Then dedupe to unique source sites (raw counts are inflated ~N× because a
header warning repeats per including TU):

```bash
grep -E '\[-Wstrict-prototypes\]' probe.log \
  | grep -oE '(olympia|lib|mapgen)/[a-z0-9_.]+:[0-9]+:[0-9]+' \
  | sort -u | wc -l
```

Gotchas that cost real time on G1:
- **`-Wimplicit-function-declaration` added via `CMAKE_C_FLAGS` is overridden**
  by `-Wno-implicit-function-declaration` in `target_compile_options` (target
  options come *after* `CMAKE_C_FLAGS`). You must *delete* the `-Wno-` line
  from `CMakeLists.txt`, not just add the positive flag.
- Re-enabling `implicit-function-declaration` makes it a **hard error** (C11),
  which stops each TU at its first hit and *undercounts*. Use
  `-Wno-error=implicit-function-declaration` to keep it a warning so the build
  completes and you see the full list.

---

## K&R → ANSI conversion

Olympia K&R defs are extremely regular:

```
int add_road(from, to_loc, hidden, name)
struct tile *from;
int to_loc;
int hidden;
char *name;
{
```

Conversion rule that's provably behaviour-neutral: **rewrite only the
parameter list on the name line, delete the K&R declaration lines, leave the
return type byte-for-byte untouched.** (A script that does exactly this is in
`run/scratch/kr2ansi.py`.) Process bottom-to-top so line numbers stay valid.

Cases to handle: pointer params (`int *row`), multi-var lines
(`int row, col;`), pointer-to-struct (`struct tile *from`), typedef'd list
types (`tiles_list l`), array params (`unsigned short seed16v[3]`), trailing
comments on the name line, implicit-int return (leave it — don't add `int`;
that's a separate `-Wimplicit-int` concern, not in scope).

**Macro-generated K&R defs exist.** G1's `z.c` has a `NEST(TYPE,f,F)` macro
that expands to a K&R definition (the drand48 family). ctags can't see them,
and the converter can't either — fix the macro by hand.

**The RNG is golden-critical.** Olympia ships a vendored SysV `drand48.c`
(`drand48/erand48/lrand48/mrand48/srand48/seed48/lcong48/nrand48/jrand48` +
a static `next()`). The local definitions deliberately override libc so the
pseudo-random sequence — and thus all golden output — is deterministic.
Convert it carefully; never "simplify" it.

---

## Generating the prototype header

The high-leverage move. Two approaches, in order of robustness:

- **Source-based extraction (preferred).** For each function clang flags as
  `missing-prototypes` you get `file:line:name`. Read the definition: params
  are on the name line (single line in this code base), return type is the
  name-line prefix or the immediately-preceding line, else implicit `int`.
  Emit `extern <ret> <name>(<params>);`. See `run/scratch/gen_proto3.py`.
- **universal-ctags** (`--_xformat="%N %{typeref} %{signature}"`) works but its
  `typeref` is unreliable for implicit-int functions (it grabs a neighbouring
  type) and emits `struct:tag` colon syntax. Use it only as a cross-check.

**Why this clears three birds:** every definition now has a visible matching
prototype (`missing-prototypes` gone), and every caller that includes the
master header sees it (`implicit-function-declaration` gone for internal
calls). What remains implicit is *only libc*.

### Gotchas (all hit on G1)

- **The compiler verifies you.** A wrong extracted prototype → `conflicting
  types` error; a wrong arg type at a call site → `int-conversion` /
  `incompatible-pointer-types` (already `-Werror` from earlier phases). So
  extraction mistakes fail loudly rather than corrupting behaviour.

- **C prototype-scope rule / file-private structs.** If a function takes a
  `struct foo *` where `struct foo` is defined privately inside one `.c`
  file (not in any header), the *first* mention of `struct foo` — inside the
  generated prototype's parameter list — creates a **distinct prototype-scope
  tag**, and you get `conflicting types` / "incompatible `struct foo *` to
  `struct foo *`". **Fix: forward-declare those tags at the top of the
  prototype header** (`struct foo;`) so the prototypes bind to the real
  file-scope tag. Find them with: structs defined in `.c` files. (G1: build_ent,
  harvest, make, wield, fight, cookie_monster_tbl.)

- **Type visibility / include placement.** The prototype header references
  engine structs, list typedefs, `FILE`, etc. Include it at the **end of the
  master header** (`oly.h`), after all type definitions, and make sure
  `FILE` is available (add `#include <stdio.h>`). The header will look broken
  to an IDE/LSP analyzing it in isolation — that's expected; it only compiles
  in context.

- **The one file that doesn't include the master header.** G1's `z.c` (the
  low-level util/RNG layer) deliberately doesn't include `oly.h`, so it never
  saw the prototype header and its own functions stayed `missing-prototypes`.
  Declare those in the low-level header it *does* include (`z.h`), not the
  generated one.

- **Separate build targets need separate headers.** The map generator is its
  own executable that doesn't include `oly.h`; it needs its own generated
  prototype header included from *its* header.

---

## Removing redundant empty-paren declarations

Once the prototype header exists, the legacy forward declarations are dead
weight and each is a `strict-prototypes` site:

- **Dispatch-table handler blocks.** Files with a command table carry a big
  block like `int v_look(), v_stack(), ...;` declaring every handler. Delete
  the whole block — the handlers are in the prototype header. (G1: ~357 sites
  across `use.c` + `glob.c`.)
- **Scattered `extern T foo();`** local decls — redundant, delete.
- **Empty-paren decls embedded in macros** (G1's `loop_known` macro held
  `extern int int_comp();`). Give them the real prototype.
- **Header empty-paren decls** (`extern void foo();`) — give the full
  prototype, matching the definition.

### Empty-paren defs hidden behind a decl (subtle)

An empty-paren *definition* `void f() {}` that has a **prior empty-paren
declaration** is NOT separately flagged by `-Wstrict-prototypes` (only the
declaration is). But the instant you fix that declaration to `f(void)`, the
`()` definition *becomes* flagged (and `-Werror=strict-prototypes` makes it
fatal). **So fix the declaration and its definition together.** Don't trust
the strict-site list alone for definitions — sweep all `name()` definitions
tree-wide (probe with `-Wdeprecated-non-prototype`, message "a function
definition without a prototype").

---

## Latent bugs this exposes (feature, not chore)

Giving everything real prototypes makes the compiler check calls it never
checked before. On G1 this surfaced genuine defects — expect the same on
g2/g3/tag, and **fix them as real bugs, don't paper over them**:

- **Arg-count mismatch.** `make_appropriate_subloc(row, col, 0)` called with 3
  args, defined with 2. K&R silently discarded the extra; the body never used
  it. (Dropped the dead arg.)
- **"Poor man's varargs."** `queue(int who, char *s, long a1..a9)` doing
  `sprintf(buf, s, a1..a9)`, called with however many args the format string
  consumes. Real fix: make it genuinely variadic (`...` + `vsprintf`).
- **Wrong return-type declaration.** A file locally declared
  `extern char *clear_wait_parse()` but the function returns `void`. The
  prototype header (extracted from the definition) is authoritative; delete
  the bogus local decl.
- **qsort comparator function-pointer mismatch.** K&R comparators
  (`int cmp(int *a, int *b)`) are compatible with `qsort`'s
  `int(*)(const void*,const void*)` *only because* they're unprototyped.
  Giving them real prototypes exposes `-Wincompatible-function-pointer-types`
  at every `qsort` call. Fix: convert each comparator to the canonical
  `(const void *, const void *)` signature, casting back to the real type via
  local variables so the body stays byte-identical.
- **Orphan declarations.** Forward decls for functions that don't exist
  anywhere (G1: `fetch_inside_name`). Just delete them.
- **`int`/pointer return truncation** — the headline 64-bit hazard. Watch for
  functions implicitly assumed to return `int` whose value is used as a
  pointer.

---

## Flag ordering (CMakeLists)

The legacy `-Wno-*` suppressions and the new positive flags live in the same
`target_compile_options`. Clang applies left-to-right, last-wins. To turn a
class into an error you must *remove* its `-Wno-` line, not merely append the
positive flag after it. When you flip to `-Werror`, delete:
`-Wno-implicit-function-declaration` and `-Wno-deprecated-non-prototype`
(the latter because converting all K&R/empty-paren defs makes it 0, and
keeping it would re-mask any regression).

---

## Verification gate

After every meaningful change, and especially before/after the flag flip:

```bash
cmake --build --preset debug          # clean, no errors
./run/mapgen/mapgen.sh && ./run/olympia-g1.sh && ./tests/olympia/golden_check.sh   # YES
```

Note: `tests/mapgen/golden` is a **stale 32-bit baseline** and diverges from
64-bit output even on a clean tree — it is *not* the gate. The olympia
`golden_check.sh` (`YES`) is the contract; it exercises the map generator
end-to-end as input, so mapgen behaviour changes show up there.

---

## Reusable tooling

Working scripts from the G1 pass are preserved in `doc/phase4-tools/`:

- `kr2ansi.py` — K&R definition → ANSI (params only; return type untouched).
- `gen_proto3.py` — source-based prototype-header generator from a warning log.
- `fix_comp.py` — qsort comparators → canonical `(const void*, const void*)`.
- `fix_void_defs.py` — empty-paren definitions → `(void)`.

They have G1-specific paths and file lists baked in (e.g. `fix_comp.py`'s
comparator table) but are small and self-documenting; adapt them for
g2/g3/tag. The general method matters more than the exact scripts.
