# Style tooling: remaining work

- Finish inspector provenance: active overlay/variant, matched and losing rules,
  per-field winner, token origin, source location, specificity/layer, resolved
  value, and backend degradation. Audit existing inspector output before adding
  missing fields.
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
