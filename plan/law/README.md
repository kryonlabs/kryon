# Kryon law refactor: ten working phases

Requested 2026-09-19. Status: **phase 1 implemented; every phase 2–10 has
landed, verified first patches (commits `dc863456`..`HEAD` on branch
`law/program`), including branch/loop semantics and the accessibility
subdomain laws — but no phase is accepted yet**. The formal preservation
proof architecture (phases 3–5) and the phase 9 downstream migrations (after
the master merge) remain the structural gates. Existing law tooling and the
starter are foundations, not completion of the phases. Each phase produces a
usable Kryon revision and an evaluation report before subsequent work is
scheduled.

## Outcome and boundaries

Every supported language construct, lowering rule, runtime policy and widget
must have explicit machine-checkable behavioral laws. Pure behavior and supported
transpilation need proofs connected to the production implementation. Rendering,
font/Unicode services, storage and OS integration need explicit assumptions and
adapter contracts backed by native integration tests. A passing property test
is not a universal proof; a proof of a reference model is not a proof of Kryon.

The initial supported target scope is C, C++ and Go, plus the declared native
KIR/KRB serialization and execution surface. Inventory valid renderer/platform
combinations rather than promising every Cartesian combination. JS/web remains
future work; its eventual reactivation must meet the same law requirements.
No current native proof claim can be borrowed to advertise a future target.

Bend 2 is an acceptable **Kryon maintainer/CI dependency**. Released compilers,
SDKs, downstream `.kry` app builds and installed apps must work without Bend,
Node.js, proof source files or proof-time network access. Ordinary native
compiler/platform dependencies remain as documented. Checks happen when Kryon
is developed and released; verified artifacts needed by users ship with it.

Maintained shared runtime behavior stays in `.kry`; laws and proofs may use
Bend. Do not rewrite the product in Bend or maintain two independent copies of
widget behavior. Start with finite equivalence checks, then establish a formal
connection to the actual frontend, KIR and generated target semantics.
Compiler proof/checker code can remain a justified native/build tool.

## Starting evidence and assets to reuse

Inspected on 2026-09-19, not a fresh full test run:

- [LAWS.md](../../docs/LAWS.md): existing syntax restrictions, runtime properties,
  static gates and a separate proof tier. Its old cross-target JS wording needs
  reconciliation with the paused target during phase 1.
- [bend-laws.mjs](../../tools/bend-laws.mjs): pinned checker loading, restricted
  proof packages and finite table evaluation. Its current evaluator accepts only
  finite fieldless input enumerations, at most eight parameters and 4096 rows;
  it cannot already prove arbitrary compiler or widget behavior.
- [bend-pin.json](../../tools/bend-pin.json): Bend 2.0.16 at
  `15ae0c86f3193b8f645b4bedbc438655b648d0da`, with checker/Base hashes.
  Pin upgrades are reviewed dependency changes, never hand edits in `vendor/bend`.
- [bend_laws_test.mjs](../../tests/bend_laws_test.mjs), `tests/laws/`,
  `cmd/kir/kir_laws.c`, `make laws-test` and `.github/workflows/ci.yml` already
  provide foundations. Check their actual coverage rather than recreating them.
- [Native remaining work](../NATIVE_LANGUAGE_COMPLETION.md),
  [slice delivery](../SLICE_VALUES.md), [style ownership](../STYLE_BRIDGE_LEDGER.md)
  and [consumer rollout](../DOWNSTREAM_CONSUMERS.md) remain authoritative for
  their existing implementation/migration work. Arrays and completed text/image/
  IME/KSS batches are not new implementation tasks here.

The existing native goal and this longer-term law program share dependencies.
Finish and record outstanding source work before treating it as a proof baseline.
Count overlapping migrations/tests once in the combined schedule. This planning
change does not start a second competing runtime or expand the paused web target.

## Phase estimates and checkpoints

All phases are **planned**. Follow links for work, acceptance and evaluation.
Days mean focused engineer-days, including implementation, proof work, review and
integration, not unattended model runtime. Token estimates are thousands of
aggregate model tokens across implementation, proof attempts, review and normal
retries, including repeated input context, output and reported reasoning tokens.
They are not output-code size, a fixed budget, a benchmark or a price quote.

| Phase | Deliverable / working checkpoint | Engineer-days | Model tokens (thousands) |
|---|---|---:|---:|
| [1](01-baseline-and-contracts.md) | Usable baseline, complete surface inventory and dependency contract | 3–5 | 120–250 |
| [2](02-proof-gates-and-pilot.md) | One production-connected verified policy; dependency-free user packages | 5–10 | 250–600 |
| [3](03-language-semantics.md) | Executable semantics and frontend/KIR laws alongside the working compiler | 10–20 | 500–1,200 |
| [4](04-transpiler-preservation.md) | Incrementally verified C/C++/Go and supported KRB lowering | 20–40 | 1,000–2,500 |
| [5](05-memory-and-callables.md) | Connected storage, borrow, array/slice ABI and callable proofs | 15–30 | 800–2,000 |
| [6](06-widget-interaction.md) | Verified shared lifecycle and primary control families | 8–15 | 350–800 |
| [7](07-layout-and-style.md) | Layout and KSS laws with tested renderer boundaries | 10–20 | 500–1,200 |
| [8](08-text-composition-accessibility.md) | Text, overlays, collections and semantic accessibility contracts | 15–30 | 700–1,800 |
| [9](09-complete-surface-and-downstream.md) | Complete supported surface coverage and migrated consumers | 15–30 | 700–1,800 |
| [10](10-release-audit-and-economics.md) | Independently audited release and measured cheaper-model workflow | 8–15 | 400–1,000 |
| **Base total** | Sequential, one experienced engineer assisted by models | **109–215** | **5,320–13,150** |

At five focused days per week the base is approximately **22–43 working weeks**.
Reserve **25–50%** for unknown proof complexity and integration: approximately
**136–323 engineer-days and 6.65–19.73 million tokens** including contingency.
Calendar delays for devices, external review or dependency bugs are additional.
These are low-confidence planning ranges, particularly phases 3–5; a new proof
approach or a checker limitation can exceed them. They are deliberately not a
promise that a full compiler proof is a short refactor.

Assumptions: one experienced compiler/runtime engineer, stronger-model help on
contracts/proof architecture, cheaper models only on bounded established tasks,
existing CI/build assets, and access to claimed native platforms. New callable
features or unrelated unfinished app work are additional unless explicitly
included in the baseline. No cost for a new general-purpose prover, verification
of third-party optimizing compilers, hardware or OS implementation is included.
Those remain explicit trusted dependencies, not silently completed proof tasks.

After every phase, replace estimates with measured tokens, review effort,
engineering days and wall time using [EVALUATION.md](EVALUATION.md). Reforecast
remaining phases; do not consume their upper estimates merely because available.
Dollar cost requires the actual chosen models and cached/uncached input, output
and reasoning billing. Record those categories rather than assuming a cheaper
model saves money if it needs more retries.

## How production connects to laws

The intended chain is:

`reviewed laws -> actual .kry / typed KIR -> checked semantic relationships -> target AST/source -> native build -> adapter tests`

Bend checks pure claims and any proof certificates in that chain. The plan must
establish how each edge preserves meaning. Hashes identify artifacts; they do
not prove equivalence. Reference interpreters, golden results and differential
runs are useful evidence but do not replace preservation proofs. Likewise,
agreement between all backends can hide a bug shared by all of them.

For finite policies, exhaustive checked evaluation can bridge actual `.kry`
policy and the proved specification. For general programs, phase 4 must select
and implement a proof-producing lowering or sound semantic validation approach,
including extraction and target printing. If that connection cannot be built,
report a blocked proof milestone and revise the engineering approach with the
user; do not relabel model tests as completed compiler verification.

Use non-vacuous laws: a sorting function must preserve elements as well as order;
a button must activate on eligible input as well as refuse disabled input.
State environment/fairness assumptions for progress and concurrency properties.

## Working-version gate after every phase

1. Preserve the last accepted upstream and affected downstream revisions, build
   commands, assets and evidence. Work directly on Kryon master in small coherent
   commits; no side branches unless requested. Revert specific owned commits if
   needed rather than discarding unrelated changes or resetting shared history.
2. Introduce laws and instrumentation alongside working code, then replace one
   proven unit at a time. Uncovered units remain usable with visible proof gaps.
   They do not become silently unsupported to increase coverage percentages.
3. Run the phase's proofs, rejection and mutation tests plus affected compiler,
   generated runtime, property, API/provenance and app checks. Use C/C++/Go
   execution evidence for changed semantics; check supported KRB cases too.
4. Build/run a representative native app at each checkpoint. Use Inbe when
   affected and other maintained consumers appropriate to the changed surface.
   Record behavior and performance deltas. Visual checks use a private display.
5. Verify release tools, downstream source builds and installed apps in an
   environment without Bend/Node/proofs and without network access. This gate
   applies from phase 2 onward; phase 1 establishes the baseline probe.
6. Record law coverage, assumptions, revision-bound results, unresolved platform
   requirements and costs. Evaluate each phase before scheduling the next.
   A working checkpoint can exist while proof acceptance remains incomplete.

Reuse existing gate entry points: `make laws-test`, `make spec-test`,
`make k2c-syntax-test k2cpp-syntax-test k2go-syntax-test`,
`make generated-runtime-parity-test runtime-parity-check`, `make go-runtime-test`,
`make generated-provenance-check`, `make preflight` and affected native tests.
The `runtime-parity-check` guard alone does not execute every backend. New
inventory/certificate/package gates named in phase deliverables must be
implemented and wired explicitly; they are not existing commands today.
Run broad final integration at phase boundaries, not after every trivial edit.

## Coverage and acceptance

Coverage uses the phase 1 inventory as its denominator. Require rows for all
widgets, props, supported language forms, lowering rules and relevant native
services, including less-used surfaces. Record the evidence level per row and
phase; a percentage without that denominator is not an acceptance measure.

Each phase depends on accepted preceding foundations. Widget inventory and
specification may be prepared early, but final widget proofs depend on the
implemented compiler/policy connection. No schedule assumes concurrent agents.

A weaker model may implement code and proofs within reviewed contracts. It must
not edit the laws, checker, pin, evidence evaluator or CI requirements as part of
a routine implementation task. Separate review and actual access controls
protect that boundary. Evaluate cost per accepted change and escaped regressions,
not token price alone. Contract/proof-system review remains specialized work.

Completion requires all ten accepted phase reports, no missing required native
platform evidence, no unclassified supported surface, and proof-free user
packages. Third-party assumptions and future targets remain published. Preserve
durable law/spec/evidence documentation before removing completed plan files.

## Work orders for smaller models

Each phase document now contains code entry points, an implementation sequence,
example schemas/pseudocode, counterexamples and a first bounded patch. Examples
explicitly marked proposed are not existing APIs or verified code. Most phases
still need several work orders; do not assign an entire research phase to a
small model in one prompt.

Use this work-order template:

```text
Phase and inventory law IDs:
Approved contract, semantics version and trusted assumptions:
One concrete behavior to implement:
Existing source symbols and exact allowed files:
Input/output interface and error behavior:
Required positive examples and counterexamples:
Commands that must pass, including mutation/rejection checks:
Forbidden changes: laws, checker, pin, evaluator, CI policy, vendor trees,
                   unrelated code and expected results without reviewed cause.
Attempt/time/token allowance and escalation condition:
Deliver: owned diff, commands/results, source/artifact revisions,
         uncovered assumptions and actual token/review cost.
```

Keep contracts and proof infrastructure under separate review. Small models may
implement an approved rule, helper, fixture or mapping. Phases 3–5 require
specialized review of semantics and the proof architecture before rule-by-rule
implementation; the code guidance cannot replace that research with a checklist.
If a task cannot satisfy its contract, return the concrete counterexample or
missing premise. Never weaken a law, omit a difficult case, mark missing evidence
as passed or replace a sound validator with a stub.

Worked starter: [focus direction laws](../../laws/focus/README.md). Three Bend
proofs and exhaustive four-case generated-C comparisons pass, with missing-proof
and incorrect-reference/native mutation checks. The starter leaves formal KIR
extraction, general semantic preservation and C++/Go comparison explicitly open.
It is useful early evidence, not completion of phase 2.
