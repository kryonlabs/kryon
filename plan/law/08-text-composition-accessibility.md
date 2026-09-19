# Phase 8 — Text, composition, overlays and accessibility laws

Status: first patch landed 2026-09-19 on `law/program` (commit `06cf09b1`):
the recorded integer-domain blocker is resolved without checker changes —
[laws/selection](../../laws/selection/README.md) models the bounded subdomain
{-1,0,1} x {-1,0,1} x 4 representative actions with fieldless ADTs, checks
nine laws, and compares all 36 rows against generated C
(`make selection-bend-laws-test`). Values outside the subdomain, richer
integer domains, text/IME/composition policies and platform adapter evidence
remain open.
Estimate: **15–30 focused engineer-days; 700–1,800 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: lifecycle, layout and style semantics are available. Preserve completed
text/IME/cache/image work and native accessibility improvements.

Work:

- Specify text index units and conversion boundaries, editing/selection,
  grapheme/UTF-8 validity, wrapping/alignment, clipboard, undo where supported,
  composition start/update/commit/cancel and focus changes. Specify exactly which
  operations are shared policy and which depend on Unicode/font/OS services.
- Cover Text, TextField/TextArea, rich text/drawing primitives and Image's semantic
  props. Prove selection bounds and shared edit transitions; test host shaping,
  bidi, fallback fonts and measurement against the explicit contract.
- Cover menus, popups/modals, dropdowns, scroll containers, tabs/navigation,
  lists, trees, tables and nested composition. State ownership restoration,
  clipping, identity, selection consistency and close/destroy behavior.
- Define accessibility roles, names, values, states, relationships, supported
  actions and notifications as laws of the semantic tree. Connect actions to
  the same widget transitions used by normal input.
- Verify native adapter workflows on available supported systems, using actual
  assistive technology/IME where required. Synthetic traces alone cannot close
  OS behavior; unavailable platform evidence remains explicitly open.
- Bound caches and retained resources and preserve measured performance; test
  eviction/invalidations against lawful semantic output.

Acceptance: each assigned family has connected shared-policy laws and real
adapter evidence for claimed platforms; semantic and visible widget state agree.
Platform exclusions must reflect published support, not convenience.

Working checkpoint: migrate text, overlays and structured collections in separate
batches; accepted Inbe meditation visuals and close-modal behavior are regression
fixtures. Desktop interaction testing uses private displays/services.

Evaluation: inspect keyboard, screen-reader, composition and visual workflows,
not just numerical property results. Record platform-specific unresolved evidence.

## Code guidance for implementation tasks

Start with `runtime/text_input.kry`, `text_rows.kry`,
`accessibility_policy.kry`, `popup_policy.kry`, `menu.kry`,
`tests/text_input_policy_test.c`, `tests/text_rows_policy_test.c`,
`tests/ui_text_edit_test.c` and `plan/NATIVE_ACCESSIBILITY_COMPLETION.md`.
Existing semantic selection entry points include `AccessibilityItemSelectionFor`
and `AccessibilitySingleSelectionFor`.

Proposed edit/adapter observation sketch:

```text
EditObservation = {utf8_bytes, anchor, caret, composition, effects}
require valid_utf8(result.bytes)
require boundary(result.anchor) AND boundary(result.caret)
require cancel_composition preserves the specified committed text
require semantic_selection == widget_selection
```

First establish the actual index unit for each API; byte, code-point and grapheme
indices are not interchangeable. Do not normalize text or add replacement
characters unless the existing contract says to. State clipboard/IME failures
and ownership transitions explicitly.

Small tasks: one edit transition and proof; host-boundary conversion tests;
composition trace; overlay cleanup; semantic action/state consistency. Include
empty text, combining marks, emoji sequences, replacement selections, focus loss,
nested modal closure and item deletion. Actual OS candidate windows and screen
reader notifications require real integration evidence.

Run `make text-input-policy-test`, relevant existing text/accessibility fixtures,
and new proof gates. GUI tests use private displays. First patch: one finite
selection policy; Unicode/IME model changes need strong review.
