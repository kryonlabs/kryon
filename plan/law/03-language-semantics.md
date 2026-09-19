# Phase 3 — Executable language and KIR semantics

Status: first vertical case implemented 2026-09-19 on `law/program`. The
earlier recorded premise was wrong: `k2kir`'s inspection dump already contains
statement and expression trees. `tools/kir-semantics.mjs` parses the dump,
encodes a restricted checked subset (integer literals, identifiers, binary
`+ - * / %`, unary minus, scalar declarations, assignments, return) and
evaluates with exact i32 wrapping; `tests/kir_semantics_test.mjs` compares it
against the production k2c lowering over boundary inputs, rejects constructs
outside the subset with source spans, and detects operand-order mutations.
The operational semantics for the remaining constructs, stores, effects,
traps and divergence still need reviewed specification.
Estimate: **10–20 focused engineer-days; 500–1,200 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: the pilot establishes a viable proof-to-production connection. Agree the
supported language revision, including whether slice/callable work has shipped.

Work:

- Define a small-step or equivalent operational semantics for supported Kry/KIR:
  values, environments, stores, effects, normal return, traps and divergence.
  Cover integer width/sign/overflow, floating-point rules including NaN, strings,
  records, locals, branches, loops, modules and left-to-right evaluation.
- Specify parsing/name/type/bound resolution and source-to-KIR meaning. Preserve
  source locations and explicit diagnostics for unsupported constructs. Raw host
  code/foreign calls are explicit contract boundaries, not silently proved code.
- Develop a restricted, deterministic semantic representation consumable by Bend.
  Keep import/name mapping faithful to production KIR, with a versioned encoding,
  checked decoders and source hashes. Avoid a parallel unconnected parser.
- Prove core evaluator/transition properties and agreement for supported frontend
  rules. Establish invariant preservation for type checking and each currently
  modeled transformation. Record termination assumptions rather than forbidding
  legitimate nonterminating user programs just to simplify proofs.
- Build property generators, shrinking and reference traces from these semantics.
  Compare real compiler outputs for boundary arithmetic, short-circuiting,
  ordering, shadowing and traps. These tests supplement, not substitute for proofs.

Acceptance: every primitive covered in this phase has a precise contract,
checker-accepted evidence and frontend/KIR connection; adversarial source and IR
mutations are detected. The remaining memory/call/lowering rows stay open.

Working checkpoint: semantics and checks initially run beside the existing
compiler; enable blocking gates only for completed rows. Existing accepted
programs retain behavior and build normally without Bend installed.

Evaluation: independently review model adequacy and non-vacuity using concrete
counterexamples. Re-estimate phases 4–5 from proof complexity actually observed.

## Code guidance for implementation tasks

Read `cmd/kir/kir.h`, `kir_parse.c`, `kir_expr.c`, `kir_check.c`,
`kir_laws.c`, `tests/kir_test.c` and `tests/language_semantics_test.py`.
`KirCheckPrograms` already checks linked programs. Extend a typed KIR export
instead of maintaining another source parser. `cmd/k2kir/main.c` is an existing
inspection entry point; confirm its dump format before introducing an encoder.

Proposed semantic pseudocode, not Bend syntax or implemented APIs:

```text
Observation = Return(value, store, effects) | Trap(reason, effects) | Continue(state)
step(typed_program, state) -> Observation
encode_checked_kir(program) -> SemanticProgram | Unsupported(node, source_span)
```

Small tasks: encode literals and variables; implement one arithmetic operation;
add short-circuit evaluation; add branches; then loops/calls with explicit store
and effects. Keep bit widths in the encoded type. Never map machine integers to
unbounded Nat without a proved conversion/wrapping rule. Fuel exhaustion means
unknown/inconclusive, not termination or semantic equality.

Fixtures must include boundary integer arithmetic, two side-effecting operands,
short-circuit suppression of a trap, shadowed names and malformed typed IR.
Compare observations including effect order and trap location, not only returns.

First patch: one integer literal/addition vertical case with reviewed semantics,
encoder checks and counterexample. Run the existing KIR and semantic fixtures
plus the new proof. Strong review must settle numeric/trap/divergence semantics
before a weaker model implements additional cases from the approved pattern.
