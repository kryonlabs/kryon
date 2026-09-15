# KSS language: remaining work

- Implement KSS `@theme` token overlays and typed environment axes: theme,
  contrast, density, pointer, and platform. Define and verify their precedence
  relative to pack tokens and scoped rules. The current C directive parser
  accepts pack/layer directives, not these overlay blocks.
- Implement source/file imports with deterministic resolution and cycle/missing
  import errors; finish the planned ordered layer declaration model.
- Implement typed pack-option/variant metadata and selector resolution for
  Lightfield's glow treatment. Runtime color-token substitution does not supply
  the planned pack-option grammar.
- Reconcile the proposed token/property/value set with all runtime parsers.
  Audit nested token groups, font and line metrics, transition properties,
  deterministic color functions, and explicit-zero behavior. Record unsupported
  features per runtime before adding them; do not add spelling aliases.
- Add file/line/column and token-origin provenance to parsed declarations so
  tools can identify winning rules across imports and overlays.
- Add focused invalid-input/fuzz coverage for the new grammar and matched
  C/Go/JS/KRB fixtures for each supported addition.

Reference: [language and overlay design](../../docs/STYLE_SEPARATION_PROPOSALS.md).
Completion requires matching declared support and diagnostics across runtimes,
not just acceptance of syntax in the web parser.
