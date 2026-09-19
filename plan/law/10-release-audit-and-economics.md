# Phase 10 — Independent audit, release and sustainable implementation workflow

Status: first patch landed 2026-09-19 on `law/program` (commit `aff53b0b`):
`make law-cleanroom-probe` builds and runs a downstream-style program with k2c + cc
while node/bend are poisoned shims. Package/install audits, mutation campaigns and
the economics trial remain open.
Estimate: **8–15 focused engineer-days; 400–1,000 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: phases 1–9 have acceptance evidence, not merely implementation claims.

Work:

- Conduct an independent requirement-by-requirement audit: laws express intended
  behavior, preconditions are satisfiable, proofs use allowed assumptions, and
  evidence reaches the shipped code. Challenge vacuous laws, missing liveness,
  disconnected models, stale certificates and bypassable build dependencies.
- Run mutation campaigns against checker/import/pin handling, compiler lowering,
  widget policies, helpers and generated artifacts. Require each deliberate
  contract violation to fail its named gate; document what mutations cannot test.
- Complete the supported native toolchain/platform matrix and downstream smoke
  tests. Reproduce release artifacts with pinned proof inputs. Inspect packages,
  dependency graphs and actual clean installs/builds/runs without Bend, Node,
  source proof trees or network access. Release tags remain workflow-owned.
- Publish generated law coverage, semantics revision, trust assumptions,
  reproducibility instructions, known unsupported boundaries and performance
  results. Remove stale plans only after preserving durable contracts/evidence.
- Trial cheaper models on representative compiler, widget and migration tasks,
  with frozen laws and limited attempts. Measure total tokens by model, wall time,
  proof retries, review time, accepted changes and escaped regressions.
- Establish ongoing CI: ordinary implementations cannot weaken contracts/checkers
  unnoticed; new surface requires laws; source/toolchain changes invalidate the
  correct evidence. Record a reviewed Bend upgrade and revalidation procedure.

Acceptance: no missing in-scope evidence or unpublished support exceptions;
release and downstream builds work without proof dependencies; the coverage
report accurately distinguishes formal proofs, trusted boundaries and tests.
The goal is not proof of third-party compilers, hardware or OS internals.

Working checkpoint: a releasable Kryon with full declared law coverage and a
measured maintenance workflow. Roll back to the last accepted release commit
through normal upstream/downstream workflow if a regression appears.

Evaluation: user reviews the final scope/evidence/cost report. Adopt cheaper-model
routing only for task classes with demonstrated savings and acceptable outcomes;
retain stronger review for contracts, proof infrastructure and architecture.

## Code guidance for implementation tasks

Read `scripts/check-tools-package.sh`, `scripts/check-static-package.sh`,
`.github/workflows/ci.yml`, the phase evidence reports and the project release
workflow. Keep release-tag creation exclusively in that workflow.

Proposed release evidence sketch:

```json
{
  "source_commit": "actual full commit hash",
  "dirty": false,
  "semantics_revision": "reviewed revision",
  "checker_pin": "reviewed checker hash set",
  "artifact_hashes": {},
  "proof_results": [],
  "native_runs": [],
  "dependency_free_user_build": "actual evidence reference"
}
```

Small tasks: evidence schema/validation; stale/missing-input rejection;
reproducible artifact check; clean release-tool/app-build probe; mutation audit;
then report generation. Do not use placeholders from this sketch in real results.
Fail release acceptance if required results are missing; keep diagnostics that
show the exact missing requirement rather than only a percentage.

Test offline clean environments without Bend/Node/proof trees, and a poisoned
PATH shim that fails if those tools are invoked. Restore normal platform tools
needed by app builds. Exercise both installed binaries and documented downstream
source builds; checking shared-library dependencies alone is insufficient.

First patch: package probe using an existing released-tool fixture. Run focused
package checks before broad `make preflight`/native release gates. Separate
contract/evaluator audit from cheaper-model implementation; compare total tokens,
retries, review effort and accepted changes, not model unit prices alone.
