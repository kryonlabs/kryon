# Runtime and backends: remaining work

- Replace host-owned KSS parsing/resolution/variant semantics with generated
  code from the shared `.kry` implementation in `02-kss-language.md`. Hosts may
  read assets, supply platform capabilities, and paint resolved values; they
  must not interpret tokens, selectors, overlays, or cascade precedence.
  Status: complete for active C/Go/style-tool call sites. The maintained
  inventory is `plan/KSS_HOST_SERVICE_LEDGER.md`, and
  `make kss-host-ledger-check` fails on unclassified production call sites.

- Audit `go/kryon/render.go` fallback assignments, including the default border
  width/material near the frame-op dispatcher and zero-opacity/default-border
  handling in later paint paths. Preserve explicit zero; remove product chrome
  defaults or document a real backend degradation.
  Status: FrameOp now carries the resolved style presence bits (`Fields`),
  populated by the Surface, text-input, text-area, and shared
  `styleFrameRectOp` frame recorders. The
  rasterizer's unstyled-value fallbacks are gated on presence: a missing
  border-width/opacity still gets the documented 1px/opaque fallback, an
  explicit zero keeps its zero (covered by
  `TestRenderPreservesExplicitZeroBorder`), and an explicit transparent
  background no longer paints the white debug fill. The remaining hardcoded
  colors in renderTextInput's fully-unstyled branch are now covered as no-style
  debug affordances: text fields paint a white fill, gray unfocused border,
  blue focused border, and gray fallback caret; text fields and text areas use
  the blue fallback selection color unless an op supplies a selection color. A
  fully unstyled text area leaves the frame background intact while still
  painting fallback selection/caret affordances. Folding these into pack data is
  part of the conformance-matrix work below.
- Carry sufficient resolved field-presence/provenance through frame operations
  to distinguish missing values from explicit zero and explain the painted result.
  Status: Go frame operations now retain field-presence masks, and the Go style
  inspector path exposes parser file names, origins, selector/body spans, and
  per-field winner/loser/missing-field decisions through `ParseStyleSheetTrace`
  plus `TraceStyleField`.
  Note: the structured JS emission notes are historical now that the old JS/web
  path is paused. Treat web pack parsing follow-ups as future web-roadmap input,
  not current style-backend completion gates.
- Establish a style conformance matrix for C, Go, KRB, DOM-roadmap, canvas, libdraw,
  termi, and null/test paths. (Resolution parity anchor: the matched fixture
  now drives the shared cascade in C (`ResolveStyle`) and Go (`ResolveStyle`)
  with identical winner assertions - base state including the dark-theme
  overlay winner, an imported token, explicit zero with presence, an `@env`
  axis field, the pressed state, class matching, and an active `@variant`
  rule plus overlay. The web leg asserts the same winners through the CSS
  mapping. KRB and the renderer backends consume the same generated C
  resolver by construction.) Verify shared generated execution as well as the same field winners, role/state/class
  matching, pack loading, and no-style content behavior on supported paths.
- Implement or verify explicit material/effect degradation on constrained
  backends. Report unsupported effects without changing resolved colors or
  silently activating another style.
- Audit browser/terminal defaults for style leakage and remove any product
  decoration not represented by resolved style data.

Completion requires recorded support and passing matched fixtures for the
matrix, including the language additions tracked in `02-kss-language.md`.
