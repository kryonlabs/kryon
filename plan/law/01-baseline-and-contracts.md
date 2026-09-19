# Phase 1 — Baseline, law inventory and dependency boundary

Status: implemented 2026-09-19 on branch `law/program` (commit `dc863456`):
law inventory, validator, mutation tests and the dependency-boundary probe
gate `make laws-test`. Evidence: [phase 1 record](../../laws/evidence/phase-1-baseline.md).
Acceptance is pending user review; clean-room package probes and per-row
contracts remain open as recorded there.
Estimate: **3–5 focused engineer-days; 120–250 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: current master plus an explicit inventory of outstanding local changes. The slice
work remains unfinished until its own delivery gates pass; do not silently treat
uncommitted code as the release baseline.

Work:

- Capture a passing native baseline with toolchain versions, commits, supported
  target combinations, current failures and affected app revisions. Fix blocking
  regressions before replacing any behavior.
- Create a machine-readable law inventory, generated documentation and schema.
  Enumerate every public widget from the canonical surface and runtime sources,
  every supported language construct/KIR node, each compiler pass and KRB
  operation. Reconcile the feature and backend matrices; assign every row an
  owner and phase. Inventory helpers, props and interactions, not just exports.
- Each law records domain, preconditions, postconditions, observable effects,
  exclusions, source owner, production connection, proof/test mechanism, target,
  trusted assumptions, evidence revision and status. Use stable dotted names.
- Distinguish proposed, specified, model-proved, implementation-connected,
  integration-verified and deferred. Test passes must not promote proof status.
- Record the existing Bend pin and checker wrapper as baseline assets. Describe
  trusted checker/Base, generators, compiler toolchains, libraries and OS services.
- Separate maintainer proof dependencies from released compiler/app dependencies.
  Define release-tool installation, downstream source build and app launch probes
  in environments without Bend, Node, proof files or network access.

Acceptance: every supported construct/widget has an inventory row and assigned
phase; the baseline runs; package probes establish the present boundary or name
an exact defect for phase 2. Known platform gaps remain visible.

Working checkpoint: unchanged working runtime, with an auditable coverage report.
No new feature or proof-completeness claim. Rollback is removal of additive
inventory/reporting changes; retain baseline artifacts.

Evaluation: review inventory completeness and assumptions with the user. Measure
actual token use and elapsed/engineering time before estimating later work again.

## Code guidance for implementation tasks

Existing entry points: `docs/LAWS.md`, `docs/CANONICAL_WIDGET_SURFACE.md`,
`docs/BACKEND_CAPABILITIES.json`, `cmd/kir/kir.h`, `cmd/kir/kir_laws.c`,
`tools/bend-laws.mjs`, `scripts/conformance-matrix.py` and `Makefile`.

Proposed artifact: `laws/inventory.json`, validated by a new
`tools/check-law-inventory.mjs`. This schema example is a design sketch, not an
existing schema or a claim of proof coverage:

```json
{
  "schema": 1,
  "laws": [{
    "id": "focus.tab.direction",
    "source": "runtime/focus.kry",
    "symbol": "FocusTabDirectionFor",
    "phase": 2,
    "targets": ["c", "cpp", "go"],
    "contract": "laws/focus/LAWS.bend",
    "proof": "laws/focus/PROOF.bend",
    "evidence": {"c": "exhaustive-test", "cpp": "missing", "go": "missing"}
  }]
}
```

Small tasks: (1) collect canonical widget/KIR names; (2) validate IDs, paths and
allowed statuses; (3) compare the inventory against the collected denominator;
(4) render a deterministic report; (5) add missing/duplicate/stale-row tests.
Keep proposals distinct from current rows. Unknown inputs fail validation; never
supply a default `proved` status. Evidence requires the actual run and revision.

First patch: schema/validator with three real existing laws, no runtime changes.
Validate with `node --test tests/bend_laws_test.mjs` and new inventory tests once
created. Pin schema ownership before allowing cheaper-model implementation.
