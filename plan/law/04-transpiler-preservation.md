# Phase 4 — Prove supported transpilation preserves semantics

Status: not started; the executable effect-ordering fixtures this phase supplements
already exist (`tests/array_values_test.py` ordering cases, focus mutation checks).
The preservation proof architecture requires strong review before any rule is
implemented, per this document.
Estimate: **20–40 focused engineer-days; 1,000–2,500 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: phase 3 semantics and encoding have passed independent review. This is
the highest-risk research phase; do not substitute cross-target output equality
for semantic preservation when a proof becomes difficult.

Work:

- Inventory all passes from typed KIR through lowering, temporary introduction,
  numeric helpers, symbol mapping, calls and final source emission. Include C,
  C++ and Go, and the supported KIR/KRB serialization/execution subset. Paused
  JS has inventory rows but is deferred, not restored to the release matrix.
- Introduce typed target ASTs where string emission prevents reliable reasoning.
  Define source/target state relations and prove each supported lowering rule
  preserves results, visible effects, ordering and traps under stated assumptions.
- Use a reviewed proof-producing translation or semantic certificate validator
  for actual generated artifacts. Choose and record the viable mechanism after
  a vertical prototype. Hash-binding a model to source is necessary but does not
  establish equivalence by itself; validators must check semantic relationships.
- Model defined target arithmetic, aliasing, value copies and runtime helpers.
  Prove generated C/C++ avoids undefined behavior for the claimed subset.
  Extend memory/call proofs in phase 5; keep incomplete rows explicitly open.
- Connect AST serialization/pretty-printing to emitted source; validate reparse
  structure and establish preservation for printer rules. Source-target
  correspondence includes helper implementations, not only function bodies.
- Keep external C/C++/Go compilers, hardware and OS inside the stated trusted
  boundary. Do not claim verified machine code from verified source lowering.
- Exercise negative transformations, optimizer mistakes, reordered effects and
  stale helper artifacts; retain multi-target execution and sanitizer checks.

Acceptance: all non-memory supported lowering rules in the frozen phase inventory
have implementation-connected preservation evidence; C/C++/Go executable behavior
and KRB's declared subset agree with semantics. Unsupported forms diagnose.
Any unresolved rule blocks this phase's full acceptance, not current runtime use.

Working checkpoint: replace one pass/construct family per reviewed change.
Unmigrated passes keep their prior tested implementation and visible proof gaps;
each intermediate revision runs Kryon. No big-bang compiler rewrite.

Evaluation: require a worked proof trace from source through emitted target for
an effectful example and an intentional faulty-lowering rejection.

## Code guidance for implementation tasks

Read `cmd/kir/kir_emit.c`, `kir_emit.h`, `cmd/k2c/k2c_lower.c`,
`cmd/k2cpp/k2cpp_lower.c`, `cmd/k2go/k2go_lower.c`, `cmd/k2b/` and
`docs/KRB_FORMAT.md`. Actual shared entry points include `KirEmitBody`,
`KirTargetType` and `KirCanEmitBody`. C/C++ headers, forward declarations and
function definitions must keep one ABI.

Proposed preservation obligation (mathematical pseudocode):

```text
related(source_state, target_state) AND well_typed(program)
  => source_step(program, source_state)
     is matched by target_steps(lower(program), target_state)
     with equal observable effects and related resulting states
```

Small tasks: typed target nodes for a single operation; source-to-node mapping;
printer rule; evidence encoding; proof/validation rule; executable positive and
negative fixtures. Only then extend the operation inventory. A certificate
validator with `return true`, a source hash alone, or a parser round trip alone
is not a semantic check. Reject unknown certificate versions/rules.

First vertical patch: scalar assignment with one temporary, followed by ordered
addition. Add a mutation that swaps effectful operands. Keep memory lowering
explicitly pending phase 5. Run C/C++/Go syntax and generated execution fixtures;
check KRB's actual supported encoding separately.

This phase is not mechanically specified by this document: a stronger reviewer
must select and validate the proof architecture. A weaker model gets one approved
rule and its interface, never the instruction to invent a sound verifier unaided.
