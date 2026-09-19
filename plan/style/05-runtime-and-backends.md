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
  Status: resolved presence bits survive rasterization. Unstyled editors and
  layout scopes no longer synthesize white fills, gray/blue borders or debug
  outlines. Border width is never invented. Explicit transparent content and
  opacity zero paint nothing. Unset text/caret colors retain a content fallback;
  selection derives a neutral highlight from that content color through shared
  `runtime/text.kry` policy. Pixel tests cover both editors, transparent surfaces
  and layout scopes. C immediate text areas now use the resolved material path;
  menu/navigation elevation and avatar decoration are supplied by styles.
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
  Status: `docs/BACKEND_CAPABILITIES.json` records material/effect degradation
  for every backend. `backend-style-degradation-check` verifies constrained
  backends document stable resolved colors/style data and explicit no-op,
  ignored, unsupported, or degraded effects; it is wired into `preflight`,
  `test`, and backend capability laws.
- Verify release style tables can be consumed by renderers without runtime KSS
  parsing. Status: `style-release-table-repro-test` builds typed tables for the
  bundled packs and an import/theme/variant overlay fixture, compares them with
  the source-loaded path, and checks repeated active resolution does not invoke
  the parser. `style-release-table-emitter-test` and
  `style-release-table-import-emitter-test` now link generated C table artifacts
  emitted under `build/.../generated/src`, covering built-ins plus an imported
  theme/variant fixture. The default library build archives the generated
  built-in table and `style-release-startup-test` proves built-in registration
  no longer parses on startup; app-authored package registration remains tracked
  in `06-tooling-and-workflow.md`.
- Audit browser/terminal defaults for style leakage and remove any product
  decoration not represented by resolved style data.

Completion requires recorded support and passing matched fixtures for the
matrix, including the language additions tracked in `02-kss-language.md`.
