# Accessibility selection law package

Reference laws for `AccessibilitySingleSelectionFor` in
[../../runtime/accessibility_policy.kry](../../runtime/accessibility_policy.kry),
modeled over a **bounded subdomain**: selected and item in {-1, 0, 1} and
four representative actions (Focus as the preserve arm, SelectItem,
DeselectItem, ClearSelection). The finite Bend table path cannot enumerate
integer parameters, so this package models the subdomain with fieldless ADTs
instead of changing the checker — the recorded phase 8 blocker is resolved
without touching proof infrastructure.

Nine checked laws cover: selection returns the item; clearing yields -1;
deselecting the selected item yields -1 (ground instances); deselecting a
distinct item preserves the selection (ground instances); other actions
preserve the selection.

The exhaustive comparison in
[../../tests/selection_bend_laws_test.mjs](../../tests/selection_bend_laws_test.mjs)
checks all 36 subdomain rows against the generated C
`AccessibilitySingleSelectionFor` and rejects a flipped reference branch.
`make selection-bend-laws-test` (part of `make laws-test`) runs it.

This is subdomain-exhaustive finite evidence, not a proof over all integers;
values outside {-1, 0, 1} and unmodeled actions remain uncovered by this
package.
