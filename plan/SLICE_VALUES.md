# Portable borrowed slice values

Status: range parsing implemented; runtime slice values are not yet supported. This is the
bounded contract for milestone 3 in `NATIVE_LANGUAGE_COMPLETION.md`.

## Source behavior

`[]T` is a borrowed mutable view of contiguous elements. `source[low:high]`
creates a view with inclusive low and exclusive high. Omitted low means zero;
omitted high means the source length. Sources may be fixed-array lvalues or
existing slices. `.length` is the number of visible elements, with type `i32`.
Indexing and element assignment use indices relative to the view.

A slice assignment copies its descriptor, so views alias the same backing
storage. It does not copy elements or transfer ownership. Whole-view rebinding
changes that binding alone. A zero-initialized slice is empty. Empty ranges,
including `[length:length]`, are valid. Slice casts, arithmetic, comparisons,
append/resizing and implicit conversion from arbitrary pointers are unsupported.
Array-to-slice conversion is explicit through range syntax.

Evaluate the source once, then low, then high, in source order. Require
`0 <= low <= high <= length`; require `0 <= index < length` for element access.
Invalid operations trap consistently on every native target, including release
builds. Validate signed indices before converting to pointer offsets. An empty
slice must never require arithmetic on a null C pointer.

## Storage and escape rules

Slice storage never owns or extends the backing storage lifetime. The checker
tracks provenance separately from the visible `[]T` type:

- A view of a local array or a record's local array field borrows that lexical
  storage. It can be used or passed synchronously while that storage is alive.
- An array parameter is a local value copy. A view of that parameter cannot be
  returned to the caller, even though a Go implementation could keep it alive.
- A slice parameter borrows caller storage. A function may return that parameter
  or a subview of it. Returned views retain their actual caller provenance.
- A module-owned fixed array has module lifetime and may back a returned view.
- Reject views of temporary array/record results; do not silently allocate to
  extend their lifetime. Bind a returned array to a local first if a local view
  is desired.
- Rebinding a slice in an outer scope to a view of an inner local is an escape.
  Check assignment lifetime, not just return statements. Merge possible origins
  across branches and loops so an unsafe path cannot disappear during checking.
- Existing synchronous borrowed slots may capture a slice only while its backing
  storage remains live. Stored/escaping closures need milestone 4's contract.
- Slice fields in records, global/state slice descriptors and foreign slice
  signatures remain rejected in this initial contract. This bounds storage
  analysis without excluding ordinary slice arguments and returned subviews.

Array elements retain their existing value/lifetime rules. Nested slice/array
containers and owning/resizable containers are separate extensions.

## Function summaries

Record which slice parameters or module storage can contribute to a function's
returned view. Resolve these summaries over the linked call graph, including
forward references, imports and recursion, before accepting callers. A return
with local-array provenance is invalid at its source.

Substitute each contributing parameter's actual argument origins at a call.
Do not simply mark every returned slice as caller-owned, or lose provenance
when a view passes through an identity helper. Conservative uncertainty must
reject an unsafe escape, not delegate its lifetime to Go's collector.

## Native representation and implementation order

1. Add an explicit range expression node with source/low/high children. Keep
   ordinary indexing and existing conditional `?:` parsing intact.
2. Add slice type recognition, length/index checking and storage diagnostics.
   Implement origin propagation, control-flow joins and function summaries.
3. Lower C/C++ descriptors to a typed data pointer plus length, with deterministic
   shared declarations across imported modules. Go uses native `[]T`; restrict
   capacity to the view's end so generated code cannot extend the view.
4. Range emission must borrow the backing lvalue. Calling the existing array
   value emitter would snapshot the array and produce incorrect aliasing or a
   dangling slice. Reuse address/destination evaluation with one evaluation of
   source and bounds instead.
5. Emit always-on range/index validation and empty-slice handling across targets.
   Existing debug-only fixed-array checks cannot establish this slice contract.
6. Enable construction, indexing, rebinding, arguments and returned subviews as
   one coherent checked feature, then update the language support documentation.

## Required executable evidence

Use one multi-target harness with C/C++ warning-as-error compilation and Go
execution, without requiring the UI runtime. Cover empty/default views, omitted
bounds, first/last indices, subviews of subviews, nonzero offsets, mutation
aliasing, descriptor rebinding and ordered side effects. Cover imported identity/
subview functions, module storage and multiple possible safe return origins.

Negative fixtures must reject returned local-array views, returned views of
array parameters, temporary-array views, inner-to-outer rebinding, escapes hidden
through helper calls or branch joins, unsupported descriptor storage, invalid
index types and foreign signatures. Runtime cases must trap negative/reversed/
oversized ranges and out-of-range reads/writes in release as well as debug C/C++.

Passing parser tests or rejecting every slice is not completion. The documented
parameter/return and aliasing examples must execute safely on all native targets.

## Parser implementation evidence

`KIR_EXPR_SLICE` preserves source, optional low bound and optional high bound
as separate expression children. `KirSliceElementType` distinguishes `[]T` from
fixed arrays. KIR tests cover omitted bounds, nested ranges, ternary bounds,
ordinary conditional indexing, malformed ranges and source locations.

Until provenance analysis and native descriptors are implemented, the checker
rejects range values explicitly in strict and permissive modes. Cross-target
negative fixtures verify that a C-header import cannot bypass this guard.
This closes only parsing; construction, ownership, execution and returned-view
semantics remain unfinished.

Validation for this parser stage: KIR unit tests, cross-target array/range
fixtures, C/C++/Go syntax suites, all 17 generated native parity fixtures,
runtime parity and generated provenance passed on 2026-09-19. The active Go
syntax fixture now uses the existing array-to-host conversion instead of an
unchecked target-only full-slice expression.
