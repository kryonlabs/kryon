# Portable borrowed slices: remaining verification

Updated 2026-09-19. Implemented and saved in the current checkpoint; not yet fully
integration-verified. This is milestone 3 of
[NATIVE_LANGUAGE_COMPLETION.md](NATIVE_LANGUAGE_COMPLETION.md).

## Current evidence

`python3 tests/slice_values_test.py build/linux-x86_64/bin` passes on C, C++
and Go after the foreign-signature fix. The fixture covers descriptor aliasing,
parameter/local rebinding, returned subviews, imported helpers, recursion,
module-owned storage, record fields/elements, strings, synchronous borrowed
captures, empty/default views and ordered bounds. Release C/C++ uses `-O2
-DNDEBUG -Wall -Werror`; invalid reads/writes and negative, reversed or oversized
ranges trap. Unsafe local/parameter/temporary returns, inner-scope escapes,
helper escapes, stored descriptors, foreign signatures, invalid bounds,
comparisons/casts, length writes and captured-descriptor rebinding reject in
strict and permissive modes.

KIR unit tests and the existing array/aggregate suites have also passed during
this implementation. The combined record/native regression run passed the
record, string, aggregate, array and then-current slice cases, C syntax and the
runtime parity guard. It exited with status 2 at a stale Go slice diagnostic
expectation. That test has been updated; a complete rerun on final code is still
required. Earlier parser-only parity passes do not prove the new slice runtime.

## Remaining work

- [ ] Add explicit branch/loop provenance and capture-lifetime adversarial cases;
  verify function summaries do not lose an unsafe origin across control flow.
- [ ] Extend focused rejection coverage for index types, element-type mismatch,
  unsupported element storage and C-header permissive fallback. Run the bounds
  cases in debug C/C++ as well as the passing release configuration.
- [ ] Complete native syntax, generated-runtime parity, generation/provenance
  and affected regression checks after the final compiler changes.
- [ ] Update the public language spec/version and implementation/architecture
  documentation. Review the bounded capture restriction below explicitly.
- [ ] Record the final integration-validated revision and results. Move the durable contract to docs and retire this
  task plan only after these remaining requirements pass.

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
  storage remains live. The current checker permits element mutation but rejects
  rebinding a captured slice descriptor. Stored/escaping closures need milestone
  4's contract.
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

## Implemented representation

`KIR_EXPR_SLICE` stores source and optional lower/upper bound expressions.
`kir_check.c` checks element/range types and unsupported storage; `kir_borrow.c`
tracks lexical origins and computes returned-parameter summaries to a fixed
point across linked functions. Origin joins are conservative rather than
flow-sensitive overwrites.

C/C++ uses the shared compiler descriptor in `include/kry_slice.h`: `void *data`
and `int32_t length`. The checker preserves element type; generated accesses
cast the pointer to that checked type and perform always-on bounds checks.
Go uses native `[]T` with capacity restricted to the view's end. No descriptor
owns or extends backing storage. Generated module headers include slice support
only when the module uses it.

The shared emitter borrows array lvalues rather than calling the array value
emitter, which would copy the backing array. It evaluates source, low and high
once in order, and handles empty/null views without null pointer arithmetic.

This replaces the earlier proposed per-element typed descriptor and the
parser-only fail-closed stage. The current implementation executes real views;
only the remaining verification and delivery tasks above are still scheduled.

## Commit-checkpoint validation

Before saving this checkpoint, `make generate-native-runtime`, the complete
`tests/record_values_test.sh` suite (including array/slice cases), and
`make focus-bend-laws-test bend-laws-test` passed. Regeneration removed stale
slice-header includes from unrelated generated prop headers. This does not
replace the broader native integration and proof-coverage tasks above.
