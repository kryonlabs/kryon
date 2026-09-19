# Phase 9 — Complete widget coverage and downstream adoption

Status: the phase 1 completeness gate already fails on unregistered surface (missing
module rows, disk/surface drift) with mutation tests; downstream consumer migrations
and the CI completeness enforcement beyond the inventory gate remain open.
Estimate: **15–30 focused engineer-days; 700–1,800 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: major widget families are covered. Reconcile against phase 1 inventory
and current canonical surface; representative fixtures alone are insufficient.

Work:

- Close every remaining widget/props/operation row, including cards, separators,
  progress, plots, canvas/drawing, toolbars, pages/routers, node/scene surfaces,
  transitions and less common controls. Assign lawful inputs, state/effects,
  composition, styling and accessibility obligations or an explicit justified
  not-applicable classification for each dimension.
- Cover shared runtime services used by these widgets, cleanup, event order,
  reentrancy restrictions and supported threaded/async host boundaries. Pure
  policies get connected proofs; native services get explicit assumptions and
  integration obligations. Keep app-specific behavior outside Kryon.
- Close supported KRB/runtime/renderer combinations from the capability matrix;
  test rejection of unsupported combinations. Future JS remains deferred.
- Introduce a CI completeness gate: new/changed widgets, semantics or lowering
  rules require inventory and law updates. Detect unregistered public surface,
  missing proof packages and stale source/evidence hashes.
- Migrate confirmed Uku/Krait/Rill theme callers and other affected maintained
  consumers in coordination with the existing rollout plan. Commit upstream
  first; then bump pristine vendor pointers and build each affected app.
- Exercise installation, assets, state persistence, theme choice, input and hot
  reload where supported. Ship no proof tooling to downstream developers or users.
- Remove replaced legacy implementations and obsolete allowances only after
  consumers pass. Keep provenance for required release-generated artifacts.

Acceptance: no supported widget or compiler operation is unclassified; every
required law has its promised evidence level. All affected consumer revisions
are recorded with passing checks or explicit unresolved required platform rows.

Working checkpoint: each consumer migration is independently usable/revertible;
keep the last accepted upstream/downstream revision pair available.

Evaluation: compare the full inventory with actual exported/runtime code, and
sample end-to-end proofs and adapter evidence across every family.

## Code guidance for implementation tasks

Read `docs/CANONICAL_WIDGET_SURFACE.md`, `docs/WIDGET_CONFORMANCE.md`,
`docs/BACKEND_CAPABILITIES.json`, `runtime/*_props.kry`, the phase 1 inventory,
`plan/DOWNSTREAM_CONSUMERS.md` and `scripts/conformance-matrix.py`.

Proposed completeness check:

```python
missing = supported_surface - inventory_surface
assert not missing
for row in required_rows:
    assert row.has_contract_and_source_owner()
    assert row.has_required_evidence_for_each_supported_target()
    assert row.evidence_matches_current_inputs()
```

This pseudocode requires a reviewed definition of supported surface and evidence
levels. Do not implement `has_required_evidence` as file existence. Require law
IDs, proof result, connected source, toolchain/pin, artifact hash and actual target
execution where promised. Unsupported/deferred rows cannot count as proved.

Small tasks: one uncovered widget family; one integration adapter; one downstream
migration. Refresh app source scans first. Implement upstream, commit it, verify
vendor cleanliness, then move the downstream pointer and commit app changes.
Never edit vendor files, auto-rewrite unrelated apps or delete legacy helpers
while a maintained caller still needs them.

First patch: report missing/unclassified public widgets without changing behavior.
Then add deletion/new-widget mutation tests for the completeness gate. Run API,
provenance, capability and affected app tests. Any support-matrix reduction needs
explicit review rather than a smaller denominator chosen to pass CI.
