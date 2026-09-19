# Phase 1 evidence — baseline, law inventory and dependency boundary

Filled from [the template](../../plan/law/EVALUATION.md) on 2026-09-19 in the
`law/program` worktree (`~/Projects/kryon-law`). This is the phase 1
evaluation record, not an acceptance claim; acceptance requires user review.

## Identity and scope

- Phase 1; implemented 2026-09-19; reviewer: pending (user).
- Upstream start `b97812e3` ("Add borrowed slice checkpoint, starter Bend laws
  and phased law plan"), implementation commit `dc863456` on branch
  `law/program`. No downstream revisions consumed yet.
- Toolchain: cc (Debian) 14.2.0, Node v22.23.2, Python 3.13.5, git 2.47.3,
  go1.25.0 linux/amd64, Linux 6.12.107+deb13-amd64 x86_64, 32 cores.
- Bend pin: 2.0.16 at `15ae0c86f3193b8f645b4bedbc438655b648d0da`
  (`vendor/bend` submodule matches); raylib at `caadb48e`.
- Inventory rows: 155 total — 81 widget families (covering all 122 canonical
  runtime modules), 37 KIR constructs (19 statement + 18 expression kinds),
  8 compiler passes, 7 KRB operations, 19 existing enforced-law rows, the
  model-proved focus law and 2 cross-target parity rows. Statuses:
  35 implementation-connected, 1 model-proved, 118 proposed, 1 deferred
  (paused JS parity).
- Outstanding local changes at entry: none — the previously uncommitted slice
  and law-planning work was committed as `b97812e3` on master before this
  phase started; the worktree tree was clean at `dc863456`.

## Evidence

| Requirement / law | Production source / artifact | Proof or test mechanism | Assumptions | Result, revision and artifact |
|---|---|---|---|---|
| Inventory validates against all denominators | `laws/inventory.json` + `tools/check-law-inventory.mjs` | `node tools/check-law-inventory.mjs` | Canonical surface + kir.h + runtime dir are the denominators | 155 rows valid, `dc863456` |
| Missing/duplicate/stale rows are rejected | `tests/law_inventory_test.mjs` | `node --test tests/law_inventory_test.mjs` | Synthetic denominators isolate mutations | 16/16 pass, `dc863456` |
| Law gates stay wired into the build | `Makefile` (`law-inventory-check`, `law-release-boundary-check` in `laws-test`) | `make laws-test` | Full law suite incl. bend proofs and native law binaries | EXIT=0, `dc863456` |
| Released surfaces free of proof dependencies | `tools/check-law-release-boundary.sh` | `make law-release-boundary-check` | `include/`, `cmd/`, `runtime/` are the released source surfaces | PASS, `dc863456` |
| Bend pin is the recorded baseline asset | `tools/bend-pin.json`, `vendor/bend` | pin/HEAD comparison in the boundary probe | submodule checkout = pinned checker | match at `15ae0c86`, `dc863456` |
| Focus starter laws still pass | `laws/focus`, `tests/focus_bend_laws_test.mjs` | `make focus-bend-laws-test` | pinned checker, 4-case Boolean domain | 3/3 pass, `dc863456` |
| Native baseline | whole tree | `make -j32 test` | dev machine only; see baseline note below | see below |

- Full-baseline result (`make -k -j32 test`, worktree at `dc863456` plus the
  baseline fixes below): every target passes except three, all traced to the
  slice checkpoint `b97812e3` meeting the paused JS target and a stale parity
  fixture —
  1. `web/control_props.js` (Makefile:1356): `kss_parser.kry:621: array
     values are supported only by native targets`. The runtime now uses
     array values, so the paused JS target cannot regenerate its runtime;
     JS/web reactivation is future work per the plan boundary.
  2. `dom-test` (Makefile:480): crashes in the k2js wasm downstream of the
     same failed web runtime generation.
  3. `conformance-matrix-check` (Makefile:581):
     `tests/parity/widget_catalog.kry:43: array and slice values require a
     fully checked portable body: WidgetCatalog` — the fixture passes
     array/slice values (toolbar options, table rows, tree items, tabs)
     inside a `#ui` composition body, which the new `check.array_body` rule
     rejects on all four backends (k2c/k2cpp/k2go/k2kir). Named defect for
     the slice delivery follow-up: the fixture must adopt the intended
     post-slice legal pattern for array props; this is a
     [SLICE_VALUES](../../plan/SLICE_VALUES.md) workstream decision, not a
     law-plan change.
- Baseline defects found and fixed while capturing the broad baseline
  (blocking regressions, per the phase 1 entry condition):
  1. `public-api-snapshot-check` was stale: `include/kry_slice.h function
     SliceIndex` (added with the slice checkpoint `b97812e3`) was missing
     from `docs/PUBLIC_API_SNAPSHOT.txt`. Regenerated with
     `scripts/public-api-snapshot.py --write`.
  2. `kryon-boundary-check` failed only on machines with `rg` installed
     (its searches swallow a missing `rg`, so CI without `rg` stayed green):
     internal test build paths linking `build/raylib/libraylib.a`
     (same artifact the Makefile links), the blocked-name enforcement files
     themselves (`cmd/kir/kir_laws.c`, `tests/compiler_laws_test.sh`) and
     the site showcase selector (`docs/site/home.js`) tripped the scans.
     The scans were narrowed to their documented intent (app-facing
     surfaces; enforcement files and showcase content excluded by glob).
     No actual leak, legacy use or app material was present.
- Non-vacuity: the validator rejects a missing widget row, a duplicate id, a
  stale source path, an unevidenced claimed target, an unpinned js target and
  a model-proved row without proof files; all are exercised by the mutation
  tests above.
- Rollback: revert `dc863456` (additive only; no runtime behavior changed).

## Cost and model evaluation

| Model / task class | Input tokens | Cached input subset | Output / reported reasoning | Attempts | Accepted changes | Review hours |
|---|---:|---:|---:|---:|---:|---:|
| Cline (this session): worktree setup + phase 1 implementation | not measured | — | not measured | 1 | dc863456 | pending user review |

- Actual billed cost unavailable from this environment; token counts were
  not instrumented. Phase estimate was 120–250k model tokens; treat this
  session as a lower bound sample, not a calibrated measurement.
- Elapsed wall time: one session on 2026-09-19 (worktree + submodule setup +
  implementation + gates).
- Revised remaining-phase estimates: unchanged; re-estimate after phase 2's
  measured pilot per the plan.

## Decision

Usable checkpoint, pending acceptance review. The inventory denominator is
complete (every canonical module, KIR construct, compiler pass and KRB
operation has a row with an owner phase), the validator and boundary probe
gate `make laws-test`, and no runtime behavior changed. Not yet done within
phase 1 scope, explicitly open:

1. The clean-room package/install/downstream-build probes (no Bend, Node,
   proof files or network) are defined by the boundary probe at source level
   only; executing them against real packages and a downstream app remains
   the phase 2 entry condition or a named defect.
2. Inventory row contracts (preconditions/postconditions) are schema-ready
   but unfilled for the 118 proposed rows; they are filled by their owning
   phases, not by phase 1.
3. The broad baseline carries three recorded failures (web runtime
   generation, dom-test, conformance-matrix-check) traced to the slice
   checkpoint and the paused JS target; the conformance fixture defect is
   named above for the slice workstream.
4. User review of inventory completeness and phase assignment is required
   before phase 1 is marked accepted.
