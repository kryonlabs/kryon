# kc Parser Plan: resolving the AST ceiling

**Status:** proposal. **Goal:** replace kc's line-matcher with a real parser
that builds an AST, while keeping generated C byte-identical (validated against
`tests/kc_syntax_test.sh`), then use the AST to add type checking, good errors,
and clean `defer` lowering.

This plan is grounded in a full audit of `cmd/kc/kc.c` (pipeline, state,
emit) and `tests/kc_syntax_test.sh` (the sole string oracle). Every claim is
cited to source.

---

## 1. The problem (the "AST ceiling")

`kc` is a line-translator, not a parser. `parse_kry` (kc.c:3022) reads a line,
dispatches it, and emits translated C fragments into `fn->body[]`
(kc_internal.h:31) — then forgets the line and moves on. This single design
choice caps what the language can ever do:

- **No type checking.** A line-matcher can't know `items` was declared
  `const char**` three lines ago, so `DrawUIListBox(..., items, ...)` with the
  wrong shape surfaces as a C error 500 lines into generated code, not at the
  call site.
- **`__auto_type` for every inferred local.** `x := f()` becomes
  `__auto_type x = f();` because kc can't track `f`'s return type. This makes
  generated C depend on a GCC extension and blocks IDE type hover.
- **`die()` on first error.** One typo aborts the whole compile; the IDE's
  diagnostics pane gets one error at a time.
- **`defer` is a text-reconstruction hack.** `apply_defers` (kc.c:4253) walks
  the flat body re-deriving scope from brace counting to splice deferred
  statements at exit points — the most algorithmically fragile code in kc.
- **Tuple/multi-assign expands to named temps** (`__kryon_assign_L_M`) because
  line-by-line translation can't see a multi-target as one node.

The scanner layer (`kc_scan.c`) is already extracted. The syntax test suite is
an excellent golden-file oracle. This is the moment to swap the foundation.

---

## 2. The approach: restructure, not rewrite

The audit settles this. Three facts make an incremental migration possible:

1. **kc already collects then emits.** `parse_kry` builds structured
   `KryFile` fields (functions[], types[], globals[], includes[], ...) in one
   pass; `write_generated` (kc.c:4513) emits in a separate pass. It is *not*
   read-a-line-write-a-line. The "AST" already exists — it's just untyped
   (`fn->body[]` is `char[512][1024]`).
2. **`fn->body[]` is a flat AST-as-strings.** Every statement is a translated
   C fragment. A real AST replaces this array with typed nodes; the emit pass
   walks nodes instead of printing strings.
3. **The scanner carries over unchanged.** `kc_scan.c` (trim, starts_word,
   parse_ident, parse_quoted, brace counting, the pending_stmt/pending_decl
   joiner) is reusable as-is.

**Verdict:** split kc.c into `parser.c` + `ast.c` + `emit.c`, reusing the
scanner. Not a from-scratch rewrite of the language.

---

## 3. Target architecture

```
.kry source
    │
    ▼
kc_scan.c        (unchanged: line split, tokenizing, brace counting,
│                 multi-line joiner — pending_stmt/pending_decl)
│
▼
parser.c         (NEW: parse_kry → builds typed AST nodes)
│   • top-level: imports, modules, types, globals, functions
│   • function bodies: statements → AstStmt nodes (typed)
│   • reuses parse_statement's dispatch logic, but builds nodes
│   • resolves modules at parse time (not deferred to emit)
│
▼
ast.h            (NEW: the node types — see §4)
│
▼
emit.c           (rewritten write_generated: walk AST → .c/.h)
│   • byte-identical output (see §5 contract)
│   • defer lowered during emit walk (no separate apply_defers pass)
│   • module-qualified calls rewritten during emit walk
│
▼
.c / .h          (identical to today's output)
```

The existing `KryFile`/`KryFunction` structs (kc_internal.h) become thin
wrappers holding AST node pointers instead of `char[][]` arrays, or are
replaced by AST types outright. Either way the *external* behavior
(command-line, file output, error format) is unchanged.

---

## 4. The AST node types

Minimal, statement-focused (kc has no expressions of its own — they pass
through as C text). From the parse_statement inventory (kc.c:2662-2954):

```c
/* A statement. Expressions are held as opaque C text (kc does not
   parse expressions today; adding that is a later, optional phase). */
typedef enum {
    AST_STMT_DECL,          /* x := expr  |  x: Type = expr  |  x: Type */
    AST_STMT_ASSIGN,        /* x = expr   (incl. compound += %= etc.) */
    AST_STMT_MULTI_ASSIGN,  /* a, b = b, a   (lowers to temps) */
    AST_STMT_MULTI_DECL,    /* a, b := expr           (lowers to temps) */
    AST_STMT_EXPR,          /* bare call / expression statement */
    AST_STMT_IF,            /* cond + body[] + optional else_body[] */
    AST_STMT_WHILE,
    AST_STMT_FOR,           /* C-style or `NAME in A..B` */
    AST_STMT_SWITCH,        /* expr + cases[] */
    AST_STMT_RETURN,        /* optional expr */
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
    AST_STMT_GOTO,
    AST_STMT_LABEL,
    AST_STMT_DEFER,         /* lowered at emit time */
    AST_STMT_BLOCK,         /* anonymous { } scope */
    AST_STMT_RAW,           /* c <line> escape hatch */
    AST_STMT_UNUSED,        /* unused expr */
    AST_STMT_ENUM,          /* inline enum block */
    AST_STMT_GUARD,         /* guard cond → if(cond) return; */
} AstStmtKind;

typedef struct AstStmt {
    AstStmtKind kind;
    int source_line;        /* the .kry line — load-bearing (see §5) */
    /* union of kind-specific fields, e.g.: */
    struct { char *text; } raw;                       /* RAW */
    struct { char *expr; } expr;                      /* EXPR/RETURN/UNUSED */
    struct { char *name; char *type; char *init; int is_state; } decl;
    struct { char *target; char *op; char *value; } assign;
    struct { char **names; int name_count; char *expr; } multi;
    struct { char *cond; struct AstStmt **body; int body_count;
             struct AstStmt **else_body; int else_count; } iff;
    struct { char *label; } goto_label;
    struct { char *statement; } defer;                /* lowered later */
    /* ... etc */
} AstStmt;

typedef struct AstFunction {
    char name[KC_NAME_MAX];
    char args[512];
    char return_type[KC_NAME_MAX];
    char guard[KC_BODY_LINE_MAX];
    int exact_name, is_public, global_name;
    AstStmt **body;        /* replaces fn->body[512][1024] */
    int body_count;
    /* calls[] carried over (used for forward-decl heuristics) */
} AstFunction;
```

The 16 `die()` branches for *removed* keywords (let/var/do/draw/widget/etc.)
are dropped entirely — the parser simply doesn't recognize them.

---

## 5. The byte-identity contract

This is the load-bearing section. The oracle (`tests/kc_syntax_test.sh`) greps
for exact strings. The rewrite must reproduce these ~10 formatting decisions
exactly:

| # | What | Where asserted | Must produce |
|---|------|----------------|--------------|
| 1 | Inferred decl | valid.kry | `__auto_type first = 0;` |
| 2 | Multi-assign temps | valid.kry (regex) | `__auto_type __kryon_assign_<LINE>_N = ...;` — **suffix = source line number** |
| 3 | Swap temps | valid.kry | two temps in source order, same `_N` scheme |
| 4 | Unused | valid.kry | `(void)second;` |
| 5 | Call-in-if wrapping | if_call.kry | `PushUIInspectSource(path, LINE);` → `__auto_type __kryon_cond_<LINE> = Call(...);` → `PopUIInspectSource();` → `if(__kryon_cond_<LINE>) {` — **temp suffix = source line** |
| 6 | Non-call if | valid.kry | `if(value > 0 && first >= 0) {` (unchanged) |
| 7 | Defer splice | defer*.kry | deferred stmt on the line before each `return`/`break`/scope-close; LIFO order; **`defer` keyword itself absent from output** |
| 8 | write_body_line indent | all | recompute indent from brace delta (kc.c:4428-4448): `}` decrements before emit, brace_delta after |
| 9 | Module prefix | module_default.kry etc. | `module_screen_kry_draw` unless `exact_name`/`global_name`/`#export` |
| 10 | Qualified call | colon_import_host | `alias.member(...)` → `module_member(...)` rewritten at emit |

**Phase 0 (below) adds oracle coverage for the blind spots** so these are
locked *before* the rewrite touches them. The two most fragile: the temp
suffixes (must equal source line numbers) and the defer splice ordering.

---

## 6. Phased migration

Each phase keeps `make test` green. No phase is merged unless the oracle passes
byte-for-byte.

### Phase 0 — Harden the oracle (no parser work)

Fill the audited blind spots so the rewrite can't silently regress them. Add
test cases for:

1. **Compound assigns** `-=`, `*=`, `/=`, `--` (only `+=`,`%=`,`&=`,`|=`,`^=`,
   `<<=`,`>>=`,`++` are covered today). One screen body.
2. **Defer in `while`, `switch`, before `continue`, in anonymous blocks** —
   the scope-unwinding code is the most likely thing a hand-written parser
   gets wrong, and it's the cleverest part of the oracle.

   **Bug found during Phase 0:** defer inside `switch` cases is broken today.
   `apply_defers` treats the whole switch as one scope and runs every
   case's accumulated defers at the switch's closing `}`, so a defer in
   `case 1` leaks into `default` and runs on the wrong path. Phase 0 does
   **not** pin this (it would lock in a bug); the AST rewrite gives each
   case its own scope node and fixes it. Phase 0 covers defer in `while`,
   before `continue`, and in anonymous blocks — all of which work correctly.
3. **`pub struct` / `pub enum`** — a real parse path (kc.c:~1697,~1733) with
   zero coverage today.
4. **`let` and widget-keyword removals** (`background`,`rect`,`line`,`swatch`,
   `text`) — lock the error surface.
5. **Main generation** (omit `--no-main`) — zero coverage today.
6. **Forward-decl heuristic** — function A calls B-defined-later, assert B's
   prototype appears before A's body.

Effort: ~1–2 days. Pure test additions. Pays off immediately (catches bugs in
*today's* kc) and de-risks every later phase.

### Phase 1 — Introduce the AST alongside the existing path  ✅ DONE

Added `cmd/kc/kc_ast.h` + `cmd/kc/kc_ast.c` with the node types (§4). Rather
than instrument every `parse_statement` branch, Phase 1 reconstructs the AST
**from the already-emitted `fn->body[]` strings** in a post-parse pass
(`ast_function_from_body`). This validates that the strings contain enough
information to rebuild structure — exactly what Phase 2 needs to know before
swapping the emit path.

`--dump-ast` parses without writing files and prints the reconstructed tree
with per-statement kind labels and depth indentation. Verified: zero
`AST_STMT_UNKNOWN` nodes on the full-feature test inputs (every statement
classifies into a known kind), meaning the AST captures 100% of what kc emits.

**Exit criterion met:** every existing test passes unchanged; the AST is built
but not used for output; a new test pins the dump output (function name,
expected kinds present, no UNKNOWN, no file written).

Effort: ~1 day (reconstruction is simpler than parallel-build instrumentation).

### Phase 2 — Emit from the AST (the cutover)

Rewrite `write_generated` to walk `AstFunction.body` (the node tree) instead of
`fn->body[]` (the string array). `write_body_line` becomes `emit_stmt`. The
defer lowering moves *into* the emit walk (a node visitor that tracks scope
depth natively, instead of `apply_defers` reconstructing it from text).
Module-qualified call rewriting also moves into the walk.

**Exit criterion:** `fn->body[]` is deleted; the oracle passes byte-for-byte;
`apply_defers` is deleted (its logic now lives in the emit visitor).

Effort: ~1 week. The riskiest phase — concentrated in §5's 10 formatting
decisions. Mitigated by Phase 0's hardened oracle.

### Phase 3 — Type checking (the payoff)

Now that bodies are typed nodes, add a type pass between parse and emit. Start
narrow: track declared types of locals and params, and check call arguments
against... what? kc has no function-signature database. Two options:

- **Option A (cheap):** check *shape* only — pointer-ness, array-ness —
  against the C declaration text. Catches `DrawUIListBox(..., items, ...)` when
  `items` is `[4] const char*` and the call wants `const char**`. Doesn't need
  a type DB.
- **Option B (full):** parse `#import`ed headers' signatures into a symbol
  table. Bigger; enables hover-types in the IDE.

Recommend Option A first — it catches the common foot-guns with a week's work
and no header parsing.

Effort: ~1 week (Option A).

### Phase 4 — Error recovery

Replace `die()` with a diagnostic collector that records errors and continues
where possible. The IDE's diagnostics pane (which parses compiler output)
becomes useful. Naturally enabled by the AST: a malformed node doesn't abort
the whole parse.

Effort: ~3–5 days.

---

## 7. What this unlocks

After Phase 2 (AST emit), even without type checking:

- **`defer` becomes correct by construction** — scope is a property of the
  node tree, not reconstructed from text.
- **Error recovery** — collect-many-errors is natural on a tree.
- **IDE features** — goto-definition, find-references, hover all become
  possible because the compiler has structured knowledge.
- **Future language features** — closures, real generics, pattern matching
  become *possible* (currently impossible on a line-matcher).
- **Removes the GCC `__auto_type` dependency** (after Phase 3 tracks types).

---

## 8. What stays the same

- The `.kry` language surface (no syntax changes — this is a compiler
  internals project).
- The generated C output (byte-identical, by contract).
- The command line, file layout, error message format.
- The scanner (`kc_scan.c`), text helpers (`convert_var_decl`,
  `convert_arg_list`, `find_assignment_op`), and module-symbol logic.
- The IDE's use of kc (it shells out and checks exit status + `.so` existence
  — unaffected).

---

## 9. Sizing and sequencing

| Phase | Effort | Risk | Depends on |
|-------|--------|------|------------|
| 0. Harden oracle | 1–2 days | none | — |
| 1. AST alongside | ~1 week | low | 0 |
| 2. Emit from AST | ~1 week | **medium** (§5) | 1 |
| 3. Type checking (opt A) | ~1 week | low | 2 |
| 4. Error recovery | 3–5 days | low | 2 |

**Total to a typed, error-recovering compiler: ~5 weeks.** Phases 0–2 alone
(inside ~2.5 weeks) deliver a structurally-sound compiler with identical
output — the foundation everything else builds on. Phases 3–4 are independent
payoffs that can land in any order after 2.

Phase 0 is shippable and valuable *today*, independent of whether the parser
work proceeds — it just adds test coverage for untested paths.

---

## 10. Open questions

1. **Replace `KryFile`/`KryFunction` or wrap them?** The structs in
   kc_internal.h are large and cross-referenced. Wrapping (AST nodes point
   into them, or they hold AST pointers) is less churn than replacing. Decide
   at Phase 1.
2. **Expression parsing.** This plan holds expressions as opaque C text (kc
   does today). A future phase could tokenize expressions for real type
   inference on `:=`. Out of scope for the initial restructure; noted as a
   later enhancement.
3. **Should the AST be serialized for IDE tooling?** A `--dump-ast-json` mode
   would let the IDE do structural queries without re-parsing. Cheap to add
   once the AST exists; deferred unless the IDE needs it.
4. **Per-file or whole-program?** kc is per-file today (one .kry → one .c/.h).
   Cross-file type checking (Phase 3 Option B) needs a multi-file mode. Decide
   when Phase 3 is scoped.
