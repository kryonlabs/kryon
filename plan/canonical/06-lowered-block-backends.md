# Remaining lowered block execution

Public Scroll/Popup/Disabled/TableCell/Canvas names are canonical and native/Go
scope helpers are private. Native and Go have substantial executable coverage.

Historical web gaps reconciled against source and parity output on 2026-09-18
before the JS/web pause:

- `tests/generated_runtime_parity_test.sh` generates menus, scroll_content,
  drag_drop, and composed_popup but its JS runner does not execute them.
  Composition executes partially in JS; the report correctly marks it partial.
- The web input test driver lacks pointer down/move/up and wheel events.
  `SubmitTextComposition` exists, but the remaining layout/pointer scenarios
  still prevent a full IME parity claim.
- `k2js` can emit `kryon.expr(...)` for unresolved expressions in composed popup
  fixtures. `web/kryon-runtime.js:expr` returns an object, not an evaluated
  condition; a truthy object is not valid activation behavior.
- Native paint-scope end markers no longer leak into generated web statements;
  parent-path metadata owns the declarative tree. Still verify restoration and
  input behavior on early return, break, continue, nested clipping, and
  disabled/captured children; removing names alone is not behavior parity.

Implement real lowering/host services without compatibility aliases, then run
identical event sequences and compare state. Keep this as future web-roadmap input; do not label any paused JS fixture as
current parity evidence until a redesigned web-native target executes it.
