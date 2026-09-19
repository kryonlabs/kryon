# Focus direction laws

Production owner: `runtime/focus.kry`, `FocusTabDirectionFor`.

`LAWS.bend` states three requirements: no Tab means no movement regardless of
Shift; Tab moves forward; Shift+Tab moves backward. `PROOF.bend` proves them for
the reference definition in `main.bend`. The two movement laws rule out an
always-idle implementation. Runtime policy remains authored in `.kry`.

Run from Kryon root:

```sh
node tools/bend-laws.mjs laws/focus/PROOF.bend
make focus-bend-laws-test
```

The Make target regenerates native sources through the existing dependency graph,
checks the three proofs, validates the complete Boolean domain/constructor mapping,
and compares all four cases to the generated C function. It also rejects each
missing proof, three wrong reference results, and inert/reversed generated C
mutants. Native comparisons run with optimization and `NDEBUG`; explicit failure
checks are used instead of assertions that disappear in release builds.

This is a checked Bend reference plus exhaustive native-C execution evidence for
one finite policy. It is not a formal proof of the Kryon compiler, an extractor,
C++/Go equivalence, or global keyboard focus traversal. The checker/Base, harness,
constructor mapping, generator, C compiler and host execution remain trusted.
Hashes/recorded revisions and full multi-target packaging evidence remain work
in `plan/law`. Review laws separately from routine implementation changes.

Bend and Node run only during maintainer tests. Neither is linked into the policy,
Kryon tools or user applications by this change. No vendor file is modified.
