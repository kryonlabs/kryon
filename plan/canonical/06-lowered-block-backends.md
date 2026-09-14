# Remaining lowered block execution

Public Scroll/Popup/Disabled/TableCell/Canvas names are canonical and native/Go
scope helpers are private. Native and Go have substantial executable coverage.

Confirmed web gaps from the 2026-09-14 audit:

- `tests/generated_runtime_parity_test.sh` generates scroll_content, drag_drop,
  composition and composed_popup but its JS runner does not execute them.
- The web input test driver lacks pointer down/move/up, wheel and composition
  events, so drag/release and IME parity cannot currently be claimed.
- `k2js` can emit `kryon.expr(...)` for unresolved expressions in composed popup
  fixtures. `web/kryon-runtime.js:expr` returns an object, not an evaluated
  condition; a truthy object is not valid activation behavior.
- Native paint-scope end markers no longer leak into generated web statements;
  parent-path metadata owns the declarative tree. Still verify restoration and
  input behavior on early return, break, continue, nested clipping, and
  disabled/captured children; removing names alone is not behavior parity.

Implement real lowering/host services without compatibility aliases, then run
identical event sequences and compare state. Add web coverage incrementally,
but do not label a fixture three-backend parity until all three execute it.
