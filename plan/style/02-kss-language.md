# KSS language: remaining work

Implemented and verified (single implementation in `runtime/kss_parser.kry`,
lowered to active C and Go hosts by the shared transpilers; old JavaScript
lowering is paused and kept only as future-roadmap reference material):

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
  access (`base[index]`, `text.length`) with active C/Go parity. Historical JS
  checks are no longer current completion evidence.

Remaining:

- Reconcile the proposed token/property/value set with all runtime parsers.
  Audit nested token groups, font and line metrics, transition properties,
  deterministic color functions, and explicit-zero behavior. Record unsupported
  features per runtime before adding them; do not add spelling aliases.
- Done (reconciliation): the value set is recorded below, per surface, from the
  single grammar in `runtime/kss_parser.kry`. One gap was closed: the closed
  set of named colors (`transparent`, `black`, `white`) from the proposal now
  parses in the shared grammar after token lookup (tokens win over builtins),
  asserted by the matched fixture in C, Go, and web (web keeps passing the
  same names through to CSS). Remaining record:
  - Token groups: `color`, `length` (alias `number`), `duration`, `material`
    are supported with flat names. Unsupported: `radius`, `easing`, `font`,
    and `shadow` groups (no StyleData consumer or resolver recipe yet),
    dotted token names, and nested groups (`length control { ... }`). Token
    names may already contain dots (`pad.x`, `accent.hover` parse as one
    identifier); the gap is numeric-first leaves (`space.3` fails the
    identifier-start rule) and the lack of consumers for grouped namespaces.
  - Color values: `#rrggbb`, `#rrggbbaa`, color tokens, and the three
    named colors above (short `#rgb` hex is not accepted). Unsupported: `color-mix(...)` and any computed color
    function - these need a deterministic rounding spec (integer pipeline in
    f64, no float division) before they can match across active C/Go hosts and
    any future web-native target.
  - Number values: decimal literals with optional sign, fraction, and
    exponent; `ms`/`s` duration suffixes on duration-token and
    duration-property positions. Divergence recorded: web mapping also
    accepts a `px` suffix and strips it; the typed path rejects `12px`.
    Web-only CSS-spelled properties are accepted by the web mapper by design
    and are not StyleData properties.
  - Properties: the typed path covers the full `StyleData` field set
    (background, background-end, foreground, border, focus, radius,
    border-width, opacity, padding-x/y, gap, font-size, icon-size,
    offset-x/y, letter-spacing, material, typeface). Explicit zero is
    supported both as literal `0` and through zero-valued tokens; presence
    bits are always set. Unsupported: `transition` (state styles already
    drive `TransitionValues` from host interaction weights; a per-property
    transition list with easing needs its own design) and `font_family`
    (StyleData has no font field yet).
  - Selectors: widget kinds, `[attr=value]` (tone, role, state), `.class`,
    and `:state` are supported on all surfaces. Comma-separated selector
    lists are web-only today (the typed parser yields one selector per
    rule); `@layer base { ... }` block form is unsupported - the ordered
    `@layer a, b;` declaration is the canonical layering mechanism.
- Done since: the web runtime's KSS-to-CSS layer runs the generated
  `kss_parser` module in declarative mode (raw selector/declaration spans,
  foreign blocks); theme/env/import decisions are no longer re-derived in
  `web/kryon-runtime.js`.
- Done: typed pack-option/variant metadata and resolution. `@variant name
  "Label" { token overlays and rules }` lives in the shared grammar;
  declarations are enumerated (name + label) even when inactive, the parse
  environment selects the active variant (C `kss_parse_with_variant`/
  `KssSetVariant`, Go `KssParser_KssSetVariant`, web environment `variant`
  field), active overlays apply at the variant origin (above theme and env,
  below scoped rules), and active variant rules yield in document order.
  `RegisterStylePackSource` registers each declared variant as a selectable
  `<pack>.<variant>` option; activation re-parses the source so base and
  variant rules resolve together. Lightfield's glow treatment belongs here.
  Caveat kept from theme/env semantics: overlays apply from their position
  onward, so sheets declare variant overlays before referencing rules.
  Follow-ups: the Go host now registers declared variants identically
  (`ParseStyleVariants`, `ParseStyleSheetVariant`, `<pack>.<variant>` packs);
  the web layer has no pack registry, so variant selection there is the
  parse environment's `variant` field by design. Remaining: surface the
  active variant in the inspector (06).
- Done: matched C/Go fixtures and invalid-input coverage. One fixture
  (`tests/fixtures/kss/matched.kss` + `matched_module.kss`) drives the
  generated C parser (provenance asserted via `KssBegin`/`KssStep`) and the Go
  module with identical winners. Historical web runtime fixture results are
  paused reference material; truncation sweeps and deterministic byte-mutation
  fuzz remain current evidence for active C and Go hosts.
- Done: hot-cursor split in `kss_parser.kry`. The per-byte lexical functions
  thread a small `KssCursor` (source/pos/line/column/file) while the full
  parser state only moves at statement level; mid-tier functions sync
  `p.cursor` before parser-level calls and after nested parses. Historical
  k2js copy-elision notes remain future web-roadmap input; active C/Go behavior
  is unchanged. Follow-up if needed: avoid
  zero-building parser-embedding result structs (`KssRuleResult` & co.) per
  declaration.

Reference: [language and overlay design](../../docs/STYLE_SEPARATION_PROPOSALS.md).
Completion requires matching declared support and diagnostics across runtimes,
not just acceptance of syntax in the web parser.
