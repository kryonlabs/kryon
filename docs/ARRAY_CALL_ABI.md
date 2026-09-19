# Direct Fixed-Array Call ABI

Ordinary portable functions implement this contract in C, C++ and Go. The
implementation landed in `aa07f3f4`; integration evidence was recorded in
`de5374f9`. This is a maintained contract, not an open implementation plan.
Foreign array signatures, slices and general callable storage are separate
contracts. See [language specification](KRY_LANGUAGE_SPEC.md).

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

The completed task plan was retired on 2026-09-19. Verification above preserves
its original scope; this documentation cleanup did not rerun compiler tests.
