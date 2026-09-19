# Phase 5 — Storage, lifetimes, arrays, slices and callable laws

Status: planned, not implemented by this planning change.
Estimate: **15–30 focused engineer-days; 800–2,000 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: phase 4 infrastructure works; memory/call rows remain explicitly pending.
Coordinate with the existing native language plan. Prove shipped implementations
rather than implementing arrays/slices/callables a second time. If callables are
still missing, their implementation cost belongs to that plan and is additional
unless explicitly folded into the measured baseline.

Work:

- Model allocation identity, initialized storage, lexical lifetimes, array value
  copies, record fields, strings, borrowed descriptors and allowed aliases.
- Prove array ABI argument/result isolation, true copy sizes, import agreement
  and evaluation order. Cover hidden output buffers and caller-owned results.
- Prove slice range/index bounds, aliasing, empty views, descriptor rebinding and
  preservation of backing lifetime through returned-view summaries, recursion,
  branches and loops. State and verify soundness of the actual borrow checker;
  include conservative rejection behavior in the language contract.
- For the accepted callable contract, specify signatures, capture ownership,
  synchronous/escaping cases, invocation, lifetime and cleanup exactly. Prove
  permitted captures cannot dangle or be released twice. Do not expand this into
  arbitrary async closures or a new allocator without a separate scope decision.
- Prove each storage/call lowering against phase 3 semantics, including generated
  native helpers and supported KRB behavior or explicit rejection.
- Add adversarial lifetime, alias, uninitialized access and ABI mutations;
  supplement proofs with fuzzing, sanitizers and executable rejection fixtures.

Acceptance: all supported storage and callable forms have connected safety and
preservation laws; malformed programs are rejected before unsafe code is emitted.
Raw pointers/foreign memory have documented preconditions, never implicit proof.

Working checkpoint: ship law coverage around the existing array ABI first, then
slices, then callables. Each batch passes compiler/runtime/app gates; preserve
working signatures or use the existing language-version compatibility process.

Evaluation: independently inspect source-to-storage model mapping and recursive
summary soundness. Report proof coverage and false-rejection regressions separately.

## Code guidance for implementation tasks

Read `cmd/kir/kir_borrow.c`, `kir_check.c`, `kir_emit.c`,
`include/kry_slice.h`, `docs/ARRAY_CALL_ABI.md`, `tests/array_values_test.py`,
`tests/slice_values_test.py` and the accepted callable design when available.
The borrow implementation is currently local work: recheck its delivered state.

Proposed abstract memory model (pseudocode, not a replacement runtime API):

```text
Region = {identity, parent_scope, alive, initialized_elements}
View = {region_identity, start, length, element_type}
valid(view, store) = alive(view.region) AND range_within_backing(view)
return_view(v) requires region(v) outlives callee
```

Small tasks: fixed-array copies; argument/result buffers; empty views; slice
subranges; lexical escape; function-summary substitution; recursive fixed point;
then permitted callable captures. Prove that abstract origins overapproximate
concrete backing regions. Never assume an unknown origin is global storage.

Counterexamples: returned local array, returned copied array parameter, helper
hiding the local origin, inner-to-outer assignment, loop/branch reassignment,
zero-length null view, capture surviving its scope, wrong copy size after C array
decay. Test mutations that delete a contributing origin or bounds comparison.

First patch: law/evidence for existing array argument isolation. Run
`sh tests/record_values_test.sh build/linux-x86_64/bin` plus the new proof.
Strong review is required for lifetime soundness and accepted callable ownership;
smaller models may implement individually specified model/fixture cases.
