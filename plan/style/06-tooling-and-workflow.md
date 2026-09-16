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
- Complete consistent file/line/column diagnostics with offending-token and
  expected-syntax information, including imported sources.
- Implement the KSS formatter with stable output, comment preservation, and
  semantic round-trip tests. Review redundant opacity declarations only after
  proving their removal preserves cascade/state resets.
- Verify live KSS reload in a real app host, including invalid-source recovery.
  Extend preview controls for theme overlays and pack options as their grammar
  becomes available.
- Finish release-time compilation of imports/overlays into typed style tables;
  keep parsing and allocation out of render hot paths. Verify generated and
  dynamically loaded styles produce matching results.
- Expand comparison captures to missing widget families/states, particularly
  popup/modal, navigation, table/menu/list/tree, and text/layout cases. Track
  uncovered cases through the testing matrix rather than rebuilding existing boards.
