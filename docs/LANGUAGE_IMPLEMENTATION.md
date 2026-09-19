# C replacement implementation status

The requested goal is a systems language that replaces application C while
continuing to transpile through KIR to C, Go, KRB, and other active targets.
The old JavaScript/web target is paused and tracked separately as future web
roadmap work.
That goal is **not complete**. This file records the implemented foundation and
the remaining work; the current contract remains `KRY_LANGUAGE_SPEC.md`.

## Implemented foundation

- Token-based expression trees with C precedence, casts, conditional
  expressions, indirect calls, postfix operations, and chained indexing.
- Separate assignment destinations and declaration bindings in KIR.
- Shared type annotation and opt-in strict checking before emission.
  Strict mode fully checks Kry-to-Kry scalar code. Modules that import C
  headers interoperate with C: calls, statements, and enum/identifier names
  the frontend cannot see are left to the C compiler instead of failing the
  strict pass, and runtime enum members resolve through the embedded runtime
  declarations the same way the emitter resolves them.
- Shared lexical cleanup lowering for normal block exits, returns, loop breaks,
  and loop continuation. Return values are evaluated before cleanup.
- One structured statement emitter for checked scalar C/C++/Go functions:
  declarations, assignments, calls, conditionals, while loops, and early exits.
- Fixed-width integer arithmetic with wrapping overflow, arithmetic signed
  shifts, and traps for invalid division, shifts, and casts.
- Left-to-right operand/argument evaluation and lazy logical/conditional arms.
- Explicit integer narrowing and numeric/boolean casts.
- Executable C/C++/Go numeric and cleanup parity fixtures, arithmetic trap
  checks, and negative source diagnostic tests.
- Removed duplicate backend cleanup implementations and the Go cgo generator.
  Explicit C ABI imports now fail in Go; native host/package bindings remain.
- Historical JS execution experiments remain in the tree only as paused
  reference material for a future web-native target.
- Go output omits its UI runtime import when generated code does not use it.
- Go lowering rejects unresolved types, unsupported C typedefs, and unsupported
  statements with source locations. It never substitutes `any` or a comment for
  executable code, and publishes a module only after lowering succeeds.

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

## Aggregate validation and indexing

Strict checking validates stored record shapes, including nested records and
fixed-array element types. It rejects unknown types, zero/invalid capacities,
recursive value layouts, nested fixed arrays, and borrowed slots stored through
aggregates or pointers. C-header imports retain their foreign-type boundary.
Slices and array-valued parameters/returns receive explicit diagnostics until
portable ownership and value semantics are implemented. Array and string byte
indices must be integers. C/C++ debug bounds checks now cover writes as well as
reads, including symbolic capacities; Go checks indices natively.

Local fixed arrays now support recursive zero initialization, positional
literals, independent value copies, conditional selection and synchronous
borrowed callback captures. The shared emitter snapshots array values and uses
explicit copies for C/C++, preserving Go's array value behavior. Named bounds
now normalize through a checked integer constant evaluator, including imported
constants, local aliases and evaluated `#run` results. Literal counts and array
copies are checked against the resolved size. General compile-time evaluation
and direct array parameters/returns remain unfinished.

`tests/aggregate_types_test.py` executes positive indexing and read/write traps
on C, C++ and Go and compares source diagnostics across all three compilers.
`tests/array_values_test.py` additionally exercises local copies, initializer
evaluation order, nested records, captured mutations and local bounds traps.
This hardens the existing aggregate subset; it does not complete the systems
language work listed above.
