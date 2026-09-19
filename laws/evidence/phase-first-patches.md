# Phase first-patch sweep — evidence record (2026-09-19, second session)

Filled from [the template](../../plan/law/EVALUATION.md). This records the
first patches and registrations landed for phases 2–10 on branch `law/program`
after phase 1. No phase is claimed complete or accepted here; each row below
states exactly what exists and what remains.

## Identity and scope

- Implementation commits: `aff53b0b` (activation package, focus C++/Go
  comparisons, bounded checker, clean-room probe), `55f8196e` (inventory and
  phase statuses), `34946141` (KIR semantic evaluator vertical case, full
  activation parity).
- Toolchain: as in the phase 1 record; additionally `go1.25.0` and `c++`
  (Debian 14.2.0) exercised by the cross-target comparisons.

## Evidence

| Requirement / law | Production source / artifact | Proof or test mechanism | Assumptions | Result, revision and artifact |
|---|---|---|---|---|
| `focus.tab.direction` on C++/Go | `tests/focus_bend_laws_test.mjs` | checked table vs fresh k2cpp lowering and vs go/kryon package | go/kryon freshness gated by RUNTIME_GO + go-runtime-test | 5/5 tests pass, `aff53b0b` |
| Omitted domain case rejected | same file | checker rejection of non-exhaustive match | — | pass, `aff53b0b` |
| Bounded checker process | `tools/bend-laws.mjs` `checkLawsProcess` | subprocess with timeout + memory cap; check/reject/timeout paths | — | pass, `aff53b0b` |
| `focus.activation` (phase 6 pilot) | `laws/activation/*`, `tests/activation_bend_laws_test.mjs` | 7 checked laws; exhaustive 128-row comparison on C, C++, Go | pinned checker; literals reduce, quantifiers never scrutinized | 4/4 tests pass, `34946141` |
| `kir.semantics.vertical` (phase 3) | `tools/kir-semantics.mjs`, `tests/kir_semantics_test.mjs` | evaluator vs k2c lowering over boundary inputs; unsupported constructs reject with spans; operand-swap mutation detected | restricted subset; INT_MIN/-1 excluded (C UB); div-by-zero traps on both sides | 4/4 tests pass, `34946141` |
| Phase 4 vertical executable case | `KirSemTemp` in the same test | scalar assignment with one temporary + ordered addition; non-commutative operand swap detected | test-tier only; preservation proofs await reviewed architecture | pass, `34946141` |
| `array.abi.argument.isolation`, `slice.values.bounded`, `kir.borrow.conservative` (phase 5) | existing suites `tests/array_values_test.py`, `tests/slice_values_test.py`, `cmd/kir/kir_borrow.c` | registered inventory rows over existing executable coverage | coverage reused, not reimplemented | rows valid at `34946141` |
| `style.explicit-zero.precedence` (phase 7) | `tests/style_widget_policy_test.c` | registered inventory row over existing coverage | same | row valid at `34946141` |
| Clean-room probe (phase 10) | `tools/check-law-cleanroom-probe.sh` | k2c + cc build/run with node/bend/bun/deno poisoned shims | dev machine; CI package audits still separate | PASS, `aff53b0b` |
| Inventory gate | `laws/inventory.json` | validator + 16 mutation tests | — | 161 rows valid, `34946141` |

- Corrected record: the phase 3 blocker previously written in this sweep's
  phase doc was wrong — the k2kir dump does contain bodies. The status line
  was corrected rather than left misleading.
- Phase 8 remains blocked on a reviewed finite-domain extension for integer
  parameters (recorded in its status line); phase 9 downstream migrations
  require merging to master first per the vendor rule.

## Cost and model evaluation

| Model / task class | Input tokens | Cached input subset | Output / reported reasoning | Attempts | Accepted changes | Review hours |
|---|---:|---:|---:|---:|---:|---:|
| Cline (this session): phase 2–10 first patches | not measured | — | not measured | 1 | 3 commits | pending user review |

- Notable checker-caught defects during development (non-vacuity evidence):
  the Bend checker rejected two incorrect reference-model drafts for the
  activation package (non-reducible quantifier over a scrutinized input;
  multi-binder clause syntax), and the semantic comparison caught a
  division-by-zero trap case in the ground-truth driver design.

## Decision

Usable checkpoints for phases 2, 3 (vertical), 5, 6, 7 and 10; phase 4 has
its executable vertical case; phases 8 and 9 carry recorded blockers/limits.
None of this substitutes for the reviewed proof architecture that phases
3–5 require for full acceptance, and no phase is marked accepted without
user review.
