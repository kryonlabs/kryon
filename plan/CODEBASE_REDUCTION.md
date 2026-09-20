# Codebase reduction and efficiency program

Status ledger for the approved reduction program (analysis: 20 parallel
subsystem audits, 2026-09-20). Baseline at approval: ~360k first-party
tracked lines. No-loss ceiling with the approved decisions: ~90–100k lines
(~27%) plus ~110–125 MB vendor/binary mass and large build/CI/startup wins.

Approved decisions (do not revisit without the owner):

- Generated Go stays **tracked and fully auditable**. Folding is bounded and
  readable by default; `k2go --minify` is the opt-in dense mode. Untracking
  generated output remains a documented future option, not a default.
- `cmd/k2js` and `web/` output stay **frozen and untouched**. Their removal
  (~28k lines) is a documented future option only.
- Relocations approved: Daochi sync module, terminal emulator, Plan 9 port
  pieces move to their downstream repos.

Working agreements learned during execution:

- The tree often carries in-flight migration WIP. Never bisect with
  `git checkout HEAD --` on directories that mix WIP with generated output;
  swap between two regeneration snapshots instead (or use `KIR_NO_FOLD=1`).
- Before deleting any "dead" module, grep the downstream repos under
  `/mnt/storage/Projects` (excluding `vendor/kryon`) for its symbols.
  Several "zero in-repo caller" modules are alive downstream.

## Completed

### Phase 0 — hygiene and dead files
- Deleted 10 unreferenced scripts/tools/examples (594 lines).
- Purged 76 `src/**/*.8` Plan 9 stubs (6.7 MB), stray root `cc*.o`, orphaned
  `.pyc` (working tree only).
- `scripts/kss-host-ledger-check.py` fixed for the src/ui `.kry` migration:
  scans tracked plus untracked files, tolerates staged deletions, EXPECTED
  set updated; `plan/KSS_HOST_SERVICE_LEDGER.md` rows updated to `.kry` hosts.
- Downstream-verified deletions: only `src/preview` and `src/core/kry_settings`
  were truly dead. **Kept because downstream apps use them:** `kry_eval` +
  `kry_quad` (workbook), `kry_process` (krait), `scene_inspect` (kasaival,
  krait), `automation` (kasaival), `kry_activity_monitor` + `app_shell`
  (inbe). `docs/PUBLIC_API_SNAPSHOT.txt` regenerated.

### Phase 1 — codegen density (biggest lever)
- Purity-tracked expression folding in `cmd/kir/kir_emit.c` for the Go
  target: single-use side-effect-free temporaries inline into their
  consumer, 96-char readability cap (`KIR_INLINE_MAX`), binary-shaped
  results parenthesized, enum-typed inlines keep their cast, call arguments
  stay captured (the k2go resolver only accepts identifier arguments, and
  capture preserves argument evaluation order).
- `k2go --minify` density flag; `KIR_NO_FOLD=1` regenerates the pre-folding
  output for A/B verification.
- Measured: changed generated files 71,483 → 42,743 lines; `var value_N`
  temporaries ~38.5k → ~9.6k package-wide. C/C++/JS output unchanged.
- Fixed during bring-up: enum-cast loss on inline, operator precedence
  (`!x` on parenthesized binaries), and a latent resolver-contract
  violation in `emit_call` (whole-call text was passed through the target
  resolver; only identifier arguments are safe there).
- Dead Go modules dropped: `guide.go`, `guide_pager.go`,
  `profile_header.go`, `terminal_pane.go` via `RUNTIME_GO_KRY` filter in
  the Makefile (C-target hosts keep the `.kry` modules).

### Phase 3 slice — compiler fast path
- `has_web_props` counter in `kir_parse.c`: native C/Go builds skip ~100
  `dom_*` field copies per widget statement. Generated C verified
  byte-identical (md5 across all regenerated runtime files).

### Phase 6 slice — artifacts
- Deleted 3 orphan spec HTML files and 7 unreferenced design PNG boards
  (~10.5 MB). Kept `08-magnetic-lightfield.png` (referenced) and
  `design/dropdown/approved.png` (gated golden image).

## Remaining work

### Phase 1 remainder — dead-export elimination (~9k lines, medium risk)
- 309 generated Go functions have zero references repo-wide; remove them in
  their `runtime/*.kry` sources and regenerate. Verify no downstream app
  hand-calls `Module_*` Go helpers first (same downstream grep as Phase 0).

### Phase 2 — style subsystem (~9k lines + startup performance)
1. `go/kryon/style_builtins.go` (6,701 lines of embedded KSS) → single
   source of truth. **Blocked:** `go:embed` cannot reach `styles/` because
   the Go module root is `go/kryon` (it lives there to escape the root
   `vendor/` submodule directory, which Go tooling would misread as a
   vendor tree). Options: copy `.kss` into `go/kryon` at regen time
   (workflow cost for plain-package consumers), rename root `vendor/` →
   `third_party/` (broad: 8 submodules, mk/vendor.mk, downstream docs), or
   compiled Go startup tables mirroring `KRYON_RELEASE_STYLE_TABLES`.
2. Table-drive `runtime/kss_parser.kry` keyword chains
   (`KssCSSPropertyKind` 557 lines, kind/role/tone/size lookup ladders;
   ~1,300–1,500 `.kry` lines and ~3× that per target). **Blocked:** KRY
   lacks module-scope const arrays; a packed-string workaround hits the
   4 KB `KIR_TEXT_MAX` literal limit. Needs either a KRY static-array
   feature or chunked string tables generated from
   `tests/fixtures/kss/css-properties.tsv`.
3. KSS parser by-reference stepping: `KssStep` copies a ~40 KB `KssParser`
   struct per token; gigabytes of memcpy at app startup. Fix in the `.kry`
   signatures (or emitter pass-by-ref for large structs).
4. Dedupe the 33 per-widget Metric/Clamp helper copies and the 64
   `StyleKindX()` accessors into shared modules (~600–900 lines).

### Phase 3 — transpiler consolidation (~5k lines, fixes 3 real bugs)
1. Merge `cmd/k2cpp` into a parametrized `cmd/k2c` (89% identical after
   rename). Fixes known k2cpp drift: missing app lifecycle hooks, unfiltered
   implicit routes, silent statement truncation. Byte-identical output
   enforced by `k2c/k2cpp-syntax-test`.
2. `cmd/kir`: table-drive the ~100 `dom_*` fields (~870 lines; the
   `has_web_props` fast path is done); extract `kir_split_params` (19
   copies of a 256 KB stack frame); unify the four widget-name tables;
   X-macro the kind-name switches. Optional: move `dom_*` out of `KirStmt`
   (126 KB → per-widget allocation, 6× less memset/memmove per statement).
3. `cmd/k2go`: consume `runtime_declarations.generated.h` for type/bool/
   slice prop tables instead of hand-synced lists (~620 lines).
4. `cmd/k2b`: shared brace-body/args/props helpers, table-driven widget
   parsers (~800 lines).
5. `cmd/common/` for utilities copy-pasted 8×: `mkdir_parent`, string
   escapers, stem helpers (~250 lines).

### Phase 4 — tests and build (~7k lines, CI ~10× faster compiles)
1. Compile generated runtime TUs once into shared objects (`surface.c` is
   compiled 42× across test targets, `style_sheet.c` 46×).
2. Makefile `foreach` template for the 63 per-widget policy-test targets +
   pattern rule for ~70 test binaries (2,146 → ~1,450 lines).
3. Merge the 63 policy tests (only ~12 run in CI) into 3–4 CI-run domain
   suites — coverage strictly increases.
4. Shared `tests/test_h.h` harness; merge per-widget duplicate test pairs;
   shared syntax-test fixtures; data-driven `smart_test.sh`.
5. `scripts/conformance-matrix.py`: one parameterized visual driver + JSON
   sidecar (1,996 → ~1,150).

### Phase 5 — relocations (owner-approved; ~14k lines, ~110 MB vendor out)
1. Daochi sync → its downstream repo (`src/sync` + headers + tests ~5.6k;
   drop `liboqs` 78 MB + `monocypher` submodules). It is off by default and
   duplicates kry_std JSON/SHA/LZSS internally.
2. Terminal → Kapsule per `plan/canonical/07-host-service-boundary.md`
   (`src/ui/terminal_pane_*` 4.5k C + 1k `.kry`, `runtime/terminal_pane.kry`,
   `include/terminal_pane.h`); keep shared cell/grid primitives here.
3. Plan 9: move `src/platform/plan9` + stubs out-of-tree (~1.3k). Keep
   `k2c --plan9`. During the move decide libdraw/libdraw_audio's home (up
   to −7.4k more if it goes; otherwise apply only the audio trim).
4. Update FEATURE_MATRIX, OWNERSHIP_LEDGER, DOWNSTREAM_CONSUMERS; bump
   downstream submodule pointers after each repo accepts the code.

### Phase 6 — headers, backends, platform, docs (~12k lines)
1. `include/`: `kry_math3d.generated.h` → types-only + the 11 used
   functions (−3,000; every TU parses 3,166 fewer lines); curate
   `kryon_compat.generated.h` to used symbols (−1,000; update
   `tools/check-raylib-compat-symbols.sh`); extract the duplicated
   14-line String prelude from the 51 props headers (−900).
2. `src/backend`: shared window/misc surface via the null-backend ops
   pattern (−450–550); one shared draw-adapter file for the 64 pure
   forwarding functions (−280–330); dedupe canvas/dom EM_JS boot + input
   JS (−300–400); move Android/SDL glue from `kry_input.c` to
   `src/platform`; shared util file (−130).
3. `src/platform`/`src/core`: split `system_theme.c` per-OS and dedupe its
   five config-path resolvers (−350–450); locale string-builder dedup;
   single route store (web delegates to app_runtime); shared Android JNI
   env helper.
4. `src/ui`: table-driven role→tone/emphasis style-frame maps (~2–3.5k);
   collapse the parallel text API family into canonical `Text(TextProps)`
   with `scripts/check-clean-text-api.py` gating; dedupe metric constants
   with `runtime/*.kry`. Note: `docs` commit `2a209d20` already dropped
   some preview text paths — re-audit before starting.
5. Docs: merge THEME_AND_BUTTON_SPEC + THEME_AND_TEXT_SPEC +
   TEXT_NODE_PROPOSALS into one spec; fold WEB_DOCUMENT_IR /
   WEB_NATIVE_DOM_PROJECT_PLAN into WEB_JS_ROADMAP (k2js stays frozen —
   keep the condensed IR contract); archive executed `plan/` ledgers
   (keep the two gated ledgers, OWNERSHIP, DOWNSTREAM_CONSUMERS); fix
   stale `src/ui/*.c` references in ARCHITECTURE/OWNERSHIP/KSS ledgers.

### Explicitly declined (documented future options)
- Untracking the generated Go runtime (−70k, workflow change).
- Deleting k2js + web output + dead JS test cluster (−28k).
- Cutting KRB, scene engine, file_dialog, notification, live kry_std
  modules, docs/site: all verified actively maintained and CI-gated.

## Verification gates (run after every phase)

`make test`, `make laws-test`, `k2c/k2cpp/k2go-syntax-test`,
`check_clean_generated_output`, `generated-runtime-parity-test`,
`go-runtime-test`, `generated-provenance-check`,
`python3 scripts/check-clean-text-api.py` (text rendering changes),
`make generate-native-runtime` idempotence, language formatter + diff
review (Readability Rule).

Known pre-existing failures not caused by this program (as of 2026-09-20):
`public-api-names-check` and `canonical-surface-test` fail from the
uncommitted src/ui migration; `kryon-boundary-check` fails on clean HEAD
(a committed laws-evidence file mentions "inbe"); `k2cpp-syntax-test`
breaks on the WIP-generated `grapheme.h` include path.

## Commit protocol

Phases 0–1 changes are in the working tree, intentionally uncommitted: the
Makefile, `include/kryon.h`, and `docs/PUBLIC_API_SNAPSHOT.txt` still carry
the src/ui migration's uncommitted hunks, and concurrent work landed in
`2a209d20`. Commit selectively once the migration lands, then bump
downstream submodule pointers per the upstream workflow in AGENTS.md.

Delete this document when every phase above is either done or re-scoped.
