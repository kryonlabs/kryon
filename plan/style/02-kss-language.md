# KSS language: remaining work

## First: one implementation in `.kry`

- Implement the KSS lexer/parser, typed tokens and values, diagnostics,
  import/layer handling, overlays, variants, and rule resolution in maintained
  `runtime/*.kry` source. Reuse the existing shared `.kry` style/cascade policy.
- Generate the required C, Go, JavaScript, and KRB implementations/data from
  that source. If a target cannot express the shared implementation, fix its
  lowering or add a minimal host service; do not write another KSS parser.
- Switch native, Go, web, compiler, preview, and app consumers to the generated
  path, then delete the handwritten logic below in the same migration. Retain
  existing public behavior through the canonical implementation, without aliases,
  compatibility parsers, forwarding shims, or hidden fallback paths.

| Handwritten implementation to remove | Work to consolidate |
|---|---|
| `src/ui/kss_parser.c` and its private header | Parsing, tokens, diagnostics, `kss_parse_variant` |
| `go/kryon/kss_parser.go` | Separate parser and `parseStyleVariant` |
| KSS parser/token helpers in `web/kryon-runtime.js` | `parseWebStyleSheet` parsing, token substitution, selectors and declarations |
| `src/ui/style_pack_source.c`, `go/kryon/style_pack.go` | Variant/registration semantics that currently dispatch to independent parsers |

Platform allocation, asset reads, and rendering may remain host services.
Registration and parser entry points must execute shared generated policy.
Audit other KSS consumers before declaring the handwritten paths removed.

## Then finish the shared language

- Add `@theme` token overlays and typed environment axes: theme, contrast,
  density, pointer, and platform. Specify precedence relative to pack tokens
  and scoped rules in the shared implementation.
- Add source/file imports with deterministic resolution and cycle/missing
  import diagnostics; finish ordered layer declarations.
- Add typed pack-option metadata and variant selection for Lightfield's glow
  treatment. Color-token substitution alone is not pack-option grammar.
- Reconcile the proposed token/property/value set with the shared language:
  nested token groups, font and line metrics, transitions, deterministic color
  functions, and explicit-zero behavior. Resolve differences in existing web
  behavior before removing its parser; do not silently drop maintained features.
- Carry file/line/column and token-origin provenance through generated output.
- Add invalid-input/fuzz and generated-target fixtures for the shared grammar.

Reference: [language and overlay design](../../docs/STYLE_SEPARATION_PROPOSALS.md).
Completion requires a single maintained `.kry` implementation, deletion of the
handwritten alternatives, and verified generated consumers. Matching results
from independent parsers are not sufficient.
