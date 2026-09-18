# Style tooling: remaining work

- Route compiler, preview, inspector, formatter, hot reload, and release-style
  compilation through the canonical `.kry` KSS implementation. Remove tool-local
  parsing or token interpretation; generated files must have a reproducible
  path back to their maintained `.kry` source.

- Finish inspector provenance: active overlay/variant, matched and losing rules,
  per-field winner, token origin, source location, specificity/layer, resolved
  value, and backend degradation. Audit existing inspector output before adding
  missing fields.
  Status: web sheets and style traces now carry the missing core fields.
  `parseWebStyleSheet` results include the active environment (theme, axes,
  variant), source file names for imports, token name -> origin kind
  (pack/import/theme/environment/variant), and per-rule source file/line plus
  the unresolved declaration text. `traceWebStyle`/`webDOMStyleTrace` winners
  gain `source` ("file:line") and `token` (name + origin when the declaration
  referenced a token), matched rules gain `source` and `raw`, and the trace
  result gains `environment`. Covered by the web KSS strict suite (provenance
  assertions on the matched fixture, including the active glow variant) and
  the real-browser inspector test (winner source, matched-rule sources,
  environment, token origin). Still open: per-field losing-rule lists (the
  matched-rules array carries all candidates and scores; a convenience view is
  pending) and backend degradation reporting, which waits on the
  runtime-and-backends conformance matrix.
- Add inspector authoring actions: copy selector, jump to source, unmatched-class
  reporting, and dead-rule diagnostics.
  Status: `webDOMUnmatchedClasses` reports widget classes present in the
  mounted tree that no installed sheet references (group selectors included)
  and `webDOMDeadRules` reports sheet rules matching no mounted node with
  their selector, source location, and pack; both are typed in
  kryon-runtime.d.ts and covered by the real-browser inspector test.
  Remaining: copy-selector and jump-to-source affordances in the inspector UI
  (the trace already carries the selectors and source locations they need).
- Complete consistent file/line/column diagnostics with offending-token and
  expected-syntax information, including imported sources.
- Implement the KSS formatter with stable output, comment preservation, and
  semantic round-trip tests. Review redundant opacity declarations only after
  proving their removal preserves cascade/state resets.
  Status: `runtime/kss_formatter.kry` implements the formatter once for all
  backends. The shared parser now records structural block spans (tokens,
  theme, env, variant, directives, with rules and foreign blocks already
  spanned), and the formatter re-layouts from those spans: construct text is
  copied verbatim per line (semantic identity by construction), indentation
  follows brace depth, top-level constructs get one blank line between them,
  and comments - inside constructs or between them - survive. Hosts assemble
  segment output (C `kss_format_string`, Go/web stitch `KssFormat` segments).
  Idempotence and parse-equivalence are asserted in C (`kss-formatter-test`,
  also wired into `make test`), Go (`TestKssFormatterRoundTrip`), and the web
  strict suite. The k2go cross-module enum reference bug this surfaced
  (double-prefixing members that already carry the enum name) is fixed.
  The CLI entry point landed as `kssfmt` (mirroring kry-fmt: in-place by
  default, `--check` for CI), built from `cmd/kssfmt` against the shared
  formatter, installed alongside the other tools, and covered by
  `tests/kssfmt_cli_test.sh` in `make test`. Still open: declaration re-flow
  (joining/splitting one-line rule bodies).
- Verify live KSS reload in a real app host, including invalid-source recovery.
  Extend preview controls for theme overlays and pack options as their grammar
  Status: `--theme light|dark` re-resolves registered packs (source packs,
  built-ins, and declared variants) through SetStyleTheme before `--style`
  applies, so captures can combine any pack with either theme overlay.
  Pack options were already selectable through `--style <pack>.<variant>`.
  The Go source-retaining `SetStyleTheme` counterpart is implemented, with
  atomic failure recovery, preserved selection/variants, and late-registration
  tests in `go/kryon/style_theme_test.go`. Both hosts and web now use the shared
  `.kry` environment-name interpretation. C registry lifecycle equivalence
  still needs the follow-up recorded in `docs/COMPLETION_EVIDENCE.md`.
  Live-host reload and invalid-source recovery remain open.
- Finish release-time compilation of imports/overlays into typed style tables;
  keep parsing and allocation out of render hot paths. Verify generated and
  dynamically loaded styles produce matching results.
- Expand comparison captures to missing widget families/states, particularly
  popup/modal, navigation, table/menu/list/tree, and text/layout cases. Track
  uncovered cases through the testing matrix rather than rebuilding existing boards.
