# Runtime and backends: remaining work

- Replace host-owned KSS parsing/resolution/variant semantics with generated
  code from the shared `.kry` implementation in `02-kss-language.md`. Hosts may
  read assets, supply platform capabilities, and paint resolved values; they
  must not interpret tokens, selectors, overlays, or cascade precedence.

- Audit `go/kryon/render.go` fallback assignments, including the default border
  width/material near the frame-op dispatcher and zero-opacity/default-border
  handling in later paint paths. Preserve explicit zero; remove product chrome
  defaults or document a real backend degradation.
- Carry sufficient resolved field-presence/provenance through frame operations
  to distinguish missing values from explicit zero and explain the painted result.
- Establish a style conformance matrix for C, Go, JS, KRB, DOM, canvas, libdraw,
  termi, and null/test paths. Verify shared generated execution as well as the same field winners, role/state/class
  matching, pack loading, and no-style content behavior on supported paths.
- Implement or verify explicit material/effect degradation on constrained
  backends. Report unsupported effects without changing resolved colors or
  silently activating another style.
- Audit browser/terminal defaults for style leakage and remove any product
  decoration not represented by resolved style data.

Completion requires recorded support and passing matched fixtures for the
matrix, including the language additions tracked in `02-kss-language.md`.
