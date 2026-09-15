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
- Wire the web runtime's KSS-to-CSS layer through the generated
  `kss_parser` module so theme/env/import decisions stop being re-derived in
  `web/kryon-runtime.js`; today only the shared byte helpers are generated.
- Implement typed pack-option/variant metadata and selector resolution for
  Lightfield's glow treatment. Runtime color-token substitution does not supply
  the planned pack-option grammar.
- Add focused invalid-input/fuzz coverage for the new grammar and matched
  C/Go/JS fixtures for each supported addition, including provenance and
  import-chain diagnostics.

Reference: [language and overlay design](../../docs/STYLE_SEPARATION_PROPOSALS.md).
Completion requires matching declared support and diagnostics across runtimes,
not just acceptance of syntax in the web parser.
