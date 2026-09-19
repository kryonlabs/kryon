# Focus activation law package

Reference laws for `FocusActivationFor` in
[../../runtime/focus.kry](../../runtime/focus.kry): whether a focused control
may activate on Enter or Space. Seven checked Bend laws cover the blocking
conditions (inactive, keyboard-disabled, content-disabled, captured, text
input active) and the two progress witnesses (Enter activates; Space
activates without text input). An always-false implementation cannot satisfy
the package.

Laws are stated so the pinned checker can reduce them: every scrutinized
position is a literal and quantified binders are never inspected on that
path. The exhaustive check is the native comparison in
[../../tests/activation_bend_laws_test.mjs](../../tests/activation_bend_laws_test.mjs):
all 128 Boolean input combinations of the checked table are compiled against
the generated C `FocusActivationFor` and must agree; missing proofs and a
flipped reference branch are rejected, and the bounded process runner checks,
rejects and times out. `make activation-bend-laws-test` (part of
`make laws-test`) runs it.

This is the phase 6 pilot extension of the phase 2 proof-to-production
connection: finite-domain native execution evidence, not a formal compiler
preservation proof. The comparison covers generated C, a fresh `k2cpp`
lowering and the `go/kryon` runtime package.
