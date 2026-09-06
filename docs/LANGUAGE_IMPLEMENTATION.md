# C replacement implementation status

The requested goal is a systems language that replaces application C while
continuing to transpile through KIR to C, Go, JavaScript, and other targets.
That goal is **not complete**. This file records the implemented foundation and
the remaining work; the current contract remains `KRY_LANGUAGE_SPEC.md`.

## Implemented foundation

- Token-based expression trees with C precedence, casts, conditional
  expressions, indirect calls, postfix operations, and chained indexing.
- Separate assignment destinations and declaration bindings in KIR.
- Shared scalar type annotation and opt-in strict checking before emission.
- Shared lexical cleanup lowering for normal block exits, returns, loop breaks,
  and loop continuation. Return values are evaluated before cleanup.
- One structured statement emitter for checked scalar C/C++/Go/JS functions:
  declarations, assignments, calls, conditionals, while loops, and early exits.
- Fixed-width integer arithmetic with wrapping overflow, exact JS BigInt i64/u64,
  arithmetic signed shifts, and traps for invalid division, shifts, and casts.
- Left-to-right operand/argument evaluation and lazy logical/conditional arms.
- Explicit integer narrowing, numeric/boolean casts, and JS f32 rounding.
- Executable C/C++/Go/JS numeric and cleanup parity fixtures, arithmetic trap
  checks, and negative source diagnostic tests.
- Removed duplicate backend cleanup implementations and the Go cgo generator.
  Explicit C ABI imports now fail in Go; native host/package bindings remain.
- JS execution of module/host call statements, scalar while/switch control flow,
  and increment/decrement statements.
- Go output omits its UI runtime import when generated code does not use it.

## Remaining: complete typed KIR

- Structured aggregate types/initializers, array and slice types, pointer and
  function types, complete statement trees, and structured `for` clauses.
- Binding identities, hygienic lowering, import visibility, duplicate module
  diagnostics, cross-module member resolution, and control-flow return analysis.
- Type checking for all constructs, complete literal/initializer validation, and
  target capability checking. Scalar integer literal range checks and Go C-ABI
  rejection exist; the current strict checker remains a scalar subset.
- Backend emission exclusively from checked KIR. Scalar functions use the shared
  emitter; non-scalar bodies still interpret raw text outside strict mode.
  The old paths cannot be removed until those callers have been migrated.

## Remaining: numeric and memory semantics

- Extend the [scalar numeric contract](LANGUAGE_SCALARS.md) to every expression,
  including complex state/global initializers, aggregate copying, and all loops.
- Complete floating-point literal validation, IEEE exceptional-value behavior,
  cross-target conversion rounding, and architecture coverage. Current tests
  cover scalar f32 addition and selected conversions, not full IEEE conformance.
- Fixed arrays, bounds-checked slices, pointer-sized types, layout and alignment
  rules, with execution tests across targets and architectures.

## Remaining: systems programming

- Explicit unsafe scopes, pointers/arithmetic, function pointers, untagged
  unions, C-compatible layouts, volatile access, and atomics.
- Checked ABI imports/exports and target capability diagnostics. Native-only
  operations must fail explicitly on targets without a representation.
- Freestanding compilation without an implicit UI runtime.

## Remaining: allocation and ownership

- Allocator interfaces, arenas, buffer/handle ownership rules, and slices.
- Cleanup binding identities instead of current shadowing restrictions.
- Defined exceptional unwinding and cleanup across foreign/runtime boundaries.

## Remaining: generics, modules, and compile-time execution

- Typed specialization of generic functions and aggregate types.
- Compile-time function evaluation beyond the existing integer `#run` evaluator.
- Explicit public module interfaces and reliable dependency ordering.
- Tagged unions with exhaustive matching, `Result`, and `Option`.

Each remaining feature needs execution fixtures on every supported backend and
negative tests for targets that cannot preserve its semantics. Generating target
syntax or adding a parser node is insufficient evidence of language support.
