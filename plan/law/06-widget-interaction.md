# Phase 6 — Shared widget lifecycle and input laws

Status: pilot extension landed 2026-09-19 on `law/program` (commit `aff53b0b`):
[laws/activation](../../laws/activation/README.md) checks seven FocusActivationFor
laws and compares all 128 Boolean rows against generated C. Remaining families,
trace/temporal integration and C++/Go legs are open.
Estimate: **8–15 focused engineer-days; 350–800 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: compiler foundations cover the policy subset used by widgets. Extend the
phase 2 pilot through shared `.kry` lifecycle owners, preserving existing APIs.

Work:

- Define a common state/event/observable trace contract for activation, pointer
  ownership, focus, keyboard traversal, cancellation, disabled/hidden states,
  frame reset, retained identity and destruction.
- Cover buttons, toggles, checkboxes, radio/segmented controls, sliders, spinboxes,
  drag/drop, swipe and reorder with their family-specific laws and actual source
  mappings. The phase 1 inventory decides exact membership; no widget disappears
  because it was omitted from this representative list.
- Prove release is consumed at most once by the proper owner, cancellation cannot
  activate, disabled controls cannot mutate values, capture resets on destruction,
  and keyboard/pointer activation have the specified equivalent effects.
- State progress properties too: eligible valid input eventually activates or
  changes the value under explicit event-delivery assumptions. Safety alone must
  not let a permanently inert widget pass.
- Refactor duplicated decisions into existing `.kry` owners. Connect transition
  functions to proofs through the established compiler/policy mechanism.
- Replay generated and minimized event traces across native C/Go and C++ clients;
  exercise physical routing through a private virtual display where supported.

Acceptance: each assigned widget has complete law rows, connected proofs for pure
policy, effect-adapter tests and mutation evidence. Existing interaction matrix
coverage is reused and extended, not reported as newly implemented.

Working checkpoint: migrate one control family per change; all other controls
continue using tested current policies. Inbe's accepted controls remain usable.

Evaluation: compare real event traces with the model, including overlap and
simultaneous-key cases. Check latency/allocations against the baseline.

## Code guidance for implementation tasks

Start with `runtime/input.kry`, `focus.kry`, `popup_ownership.kry`,
`button.kry`, `checkbox.kry`, `slider.kry`, and
`tests/interaction_policy_matrix_test.c`. Existing production examples are
`InputPointerInteractionFor` and `FocusActivationFor`; keep their contracts and
public names. Use the focus Bend package as a packaging/test example only.

Proposed trace test skeleton:

```text
state = initial_state
for event in generated_trace:
    expected = reference_transition(state, event)
    actual = native_transition(state, event)
    require equal(state, effects, consumed_events)
    state = actual.state
```

Specify whether events are press, release or held state; never replace a
press-origin requirement with current hover. Add positive activation witnesses
as well as disabled/captured/no-press nonactivation laws. A local predicate cannot
prove global at-most-once consumption without modeling the shared event owner.

Small tasks: one family/domain mapping, pure-policy proof, complete finite-domain
comparison if finite, temporal trace integration, one counterexample mutation.
Run `make input-policy-test focus-policy-test interaction-policy-matrix-test`
and the new family proof gate. Confirm target availability in Makefile first.

First patch: one contract for a currently implemented transition and its native
comparison. Do not add widget-local compatibility wrappers or move Kapsule app
logic into the shared runtime. Event-routing/liveness assumptions need review.
