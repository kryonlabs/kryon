# KSS language: remaining work

Implemented and verified (single implementation in `runtime/kss_parser.kry`,
lowered to C, Go, and JavaScript by the shared transpilers):

- `@theme` token overlays and typed environment axes (`theme`, `contrast`,
  `density`, `pointer`, `platform`) with closed values, parse-time matching,
  and defined precedence: imports < pack tokens < active theme overlay <
  matching `@env` overrides < programmatic variant colors < scoped rules.
- `@import` with deterministic depth-first ordering, cycle detection, missing
  import errors, host-resolved module ids and file names, and the ordered
  `@layer a, b, c;` declaration model alongside built-in layer aliases.
- `@version`, per-rule file/line/column provenance, per-token origin kinds,
  and diagnostics that name the offending file, line, and column.
- Strict .kry gained fixed-capacity array indexing and borrowed string byte
  access (`base[index]`, `text.length`) with C/Go/JS parity; the shared spec
  test executes the contract through Go and JavaScript.

Remaining:

- Reconcile the proposed token/property/value set with all runtime parsers.
  Audit nested token groups, font and line metrics, transition properties,
  deterministic color functions, and explicit-zero behavior. Record unsupported
  features per runtime before adding them; do not add spelling aliases.
- Done since: the web runtime's KSS-to-CSS layer runs the generated
  `kss_parser` module in declarative mode (raw selector/declaration spans,
  foreign blocks); theme/env/import decisions are no longer re-derived in
  `web/kryon-runtime.js`.
- Implement typed pack-option/variant metadata and selector resolution for
  Lightfield's glow treatment. Runtime color-token substitution does not supply
  the planned pack-option grammar.
- Done: matched C/Go/JS fixtures and invalid-input coverage. One fixture
  (`tests/fixtures/kss/matched.kss` + `matched_module.kss`) drives the
  generated C parser (provenance asserted via `KssBegin`/`KssStep`), the Go
  module, and the web runtime with identical winners; truncation sweeps and
  deterministic byte-mutation fuzz run in all three suites (C 4000
  iterations, Go 6000, web 500+truncations) with liveness guaranteed.
- Done: hot-cursor split in `kss_parser.kry`. The per-byte lexical functions
  thread a small `KssCursor` (source/pos/line/column/file) while the full
  parser state only moves at statement level; mid-tier functions sync
  `p.cursor` before parser-level calls and after nested parses. Together with
  the k2js copy elisions (call-site `copyValue` skipped for direct calls;
  same-module record parameters passed by reference under the reassign-from-
  result discipline) this cut web sheet parsing ~30x (3 KB sheet:
  6.2 s -> 0.2 s) with C/Go behavior unchanged. Follow-up if needed: avoid
  zero-building parser-embedding result structs (`KssRuleResult` & co.) per
  declaration.

Reference: [language and overlay design](../../docs/STYLE_SEPARATION_PROPOSALS.md).
Completion requires matching declared support and diagnostics across runtimes,
not just acceptance of syntax in the web parser.
