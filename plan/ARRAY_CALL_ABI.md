# Direct fixed-array call ABI

Status: implemented for ordinary functions; native integration checks passed.
Initial audit: 2026-09-19 after `265ff394`. Belongs to milestone 2 of
`NATIVE_LANGUAGE_COMPLETION.md`.

## Gap established before implementation

Separate strict sources declaring `Take(values: [3]i32) -> i32` and
`Make() -> [3]i32` were compiled with `k2c`, `k2cpp` and `k2go`. All six exited
1 with source-located parameter/return portable-value-semantics diagnostics.
Parsing reaches the checker: the first barrier is not new array syntax.

`cmd/kir/kir_check.c:check_function` deliberately rejects these signatures.
`cmd/k2c/k2c_lower.c:convert_args` and its C++ counterpart already print array
parameter declarations, but C adjusts those declarations to pointers. That is
not the required value ABI. `cmd/kir/kir_emit.c:assign_value` uses the destination
size for whole-array copies; leaving a parameter as a pointer would give the
wrong copy size. Returning a raw C array is invalid regardless of local copies.

## Language behavior

- Parameters and results are array values. A callee receives an independent
  array copy; whole-array assignment and element mutation cannot alter the
  caller's array. Existing element semantics remain unchanged: copying borrowed
  strings copies their views and does not extend external storage lifetimes.
- Arguments are evaluated left to right and captured before evaluating the next
  argument. Later arguments that mutate source state must not change an earlier
  argument's captured value.
- Returned local arrays remain valid after the function exits. Forwarding a
  result, assigning it, indexing it and selecting between results must preserve
  value semantics without source-level record wrappers.
- Equal resolved capacities and canonical element types define compatibility.
  Imported declarations resolve bounds/types in the declaring module, not the
  consumer's scope. Local, private and exported functions obey the same rules.
- Host `char` buffers retain their existing C-string interop convention and
  are not part of the portable array-value ABI.
- Existing element/storage restrictions remain. Slice ownership and general
  callable storage are separate milestones, not accidental ABI side effects.

## Native lowering contract

Use compiler-managed input snapshots and a hidden output parameter for C/C++
array returns; Go uses native array parameters/results. Do not introduce a
public wrapper type or a second app-facing array API.

For C/C++ inputs, the generated call evaluates and snapshots each array
argument. The callee copies its incoming array data into a true local fixed
array before executing the body. All references to the source parameter name
resolve to that local array, including whole-array writes and borrowed captures.
Generated input names must avoid every source parameter/local/capture name.

For C/C++ results, lower the function return type to `void` and add a hidden
output parameter declared with the resolved array shape (an element pointer
after C parameter adjustment). The caller allocates the result array and passes
its storage. Every value-return branch evaluates its
expression, copies the full array into that output and then returns. Hidden
output storage belongs to the caller and must never point into expired callee
storage. The copy size is taken from the captured true array value, never from
the adjusted output parameter. This avoids per-shape wrapper types and works
with the same C and C++ signature helper.

This convention must be identical in exported headers, private forward
prototypes, definitions, imported calls and any generated callback adapter that
supports the signature. Existing explicit external/host calling conventions
must not silently acquire the internal output parameter; retain a specific
source diagnostic for foreign array signatures without a defined adapter.

## Implementation locations and order

1. Normalize linked function parameter/return array types before call checking,
   independent of module/function iteration order. Reuse checked bound
   resolution; preserve declaring-module scope and source diagnostics.
2. Add a shared KIR signature-lowering helper for hidden parameters and generated
   names. Route all three signature sites in each of `k2c_lower.c` and
   `k2cpp_lower.c` through it. Keep ordinary scalar/record signatures unchanged.
3. Extend `KirEmitBody` parameter initialization with true local array copies.
   Reuse `declare_array`/`assign_value`; do not duplicate copy-size rules in the
   target-specific body fallbacks.
4. Extend `emit_call`/`emit_expr` to allocate array result storage and perform the
   hidden-output call exactly once. Keep argument snapshots in source order.
5. Extend `KIR_STMT_RETURN` emission to populate hidden output storage. Verify
   early returns and existing cleanup expansion, not only a final return.
6. Enable the checker only for signatures whose complete lowering is supported.
   Go signature lowering must resolve the same normalized imported types.
7. Add execution/rejection fixtures before marking support in the language spec.

## Required evidence

Use the existing multi-target aggregate fixture harness, extending it with an
imported module and native drivers. Execute all positive cases on C, C++ and Go:

- Copy isolation on element writes and whole-array parameter assignment.
- Returned local arrays, empty initialization and partial initialization.
- A result forwarded through another function and nested calls.
- Multiple arrays plus scalar/record arguments with observable side effects.
- Returning and then modifying a result without altering another array value.
- Numeric/named equivalent bounds and imported function/record element types.
- Early return branches and supported borrowed callback captures of parameters.
- Existing debug read/write bounds traps on parameters and returned values.

Reject mismatched capacities/elements, invalid bounds, unsupported stored types
and foreign signatures lacking a defined array convention with source spans.
Compile C/C++ with warning-as-error settings; run existing record, local-array,
string, syntax and generated native parity guards to catch ABI regressions.
No milestone closure until these are executable tests, not just this checklist.

## Implementation verification (2026-09-19)

The extended `tests/array_values_test.py` passes on C, C++ and Go, including
imported calls, early returns, named bounds, captured argument ordering,
parameter mutation isolation, borrowed callbacks, record/string elements,
zero-valued returns and bounds traps on parameters/results. The full record,
string and aggregate fixture suite also passes. C and C++ syntax suites pass.

Follow-up integration on 2026-09-19 passed native generation, Go syntax, all
17 generated C/Go parity fixtures, runtime parity, generated provenance and Go
runtime tests. The slider compilation mismatches were resolved in the shared
checkout. Two old fixtures needed nonoverlapping slider bounds and matching
C/Go tap coordinates; expected interaction results were preserved.

Evidence applies to compiler `aa07f3f4` with `d8cc3a9f` and the current shared
slider/style worktree. It does not assert that the unrelated slider/style work
has been committed or that final platform validation for the whole goal is done.
