# Phase evaluation record

Copy this template into the implementation's evidence location after each phase.
This is a template, not a passing result. Keep it linked from the phase plan.

## Identity and scope

- Phase, dates and reviewer:
- Upstream start/end commits; downstream revision pairs:
- Language/KIR revision, Bend pin, platform/toolchain versions:
- Inventory rows accepted; rows unfinished and exact reason:
- Changed contracts and separate approval/review evidence:

## Evidence

| Requirement / law | Production source / artifact | Proof or test mechanism | Assumptions | Result, revision and artifact |
|---|---|---|---|---|
| Populate every phase acceptance criterion | | | | |

- Explain the connection from the law to production behavior for each proof family.
- Record non-vacuity checks, rejected counterexamples, mutation results and seeds.
- Record actual build/execution commands, outputs and unavailable prerequisites.
- List native/compiler/backend/app regressions and measured performance changes.
- Identify the usable checkpoint and exact rollback commits/artifact set.
- Attach clean package install, downstream build and app-run evidence without
  Bend, Node, proof sources or network access. Do not infer this from imports alone.

## Cost and model evaluation

| Model / task class | Input tokens | Cached input subset | Output / reported reasoning | Attempts | Accepted changes | Review hours |
|---|---:|---:|---:|---:|---:|---:|
| Populate actual accounting; avoid double-counting provider categories | | | | | | |

- Total aggregate tokens and actual billed cost, if available:
- Focused engineering days; elapsed wall time; external waiting time:
- Proof/gate failures, defect escapes and fixes:
- Original range versus actual; explanation of variance:
- Revised remaining-phase ranges and contingency:
- Cheaper-model comparison: comparable task scope, total accepted-change cost,
  quality and review burden; state when the sample is too small to conclude:

## Decision

State one: accepted; usable checkpoint with incomplete acceptance; or blocked by
an exact external/technical prerequisite. User feedback and resulting scope
changes must be recorded. Do not mark a phase accepted merely because its budget
is exhausted, the app still runs, or a model-only proof checks.
