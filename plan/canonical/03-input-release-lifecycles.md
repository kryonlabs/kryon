# Remaining input lifecycle work

The C `ConsumeRelease` audit is complete: 34 callsites apply generated decision
flags. The state-changing function itself remains native. The nine generic
`InputPointerInteractionFor` calls use `runtime/input.kry` policy; they are not
nine separate C activation implementations. Reorder release policy is done.

Native completion and future boundary:

- Surrounding popup capture, focus and owner-reset transitions now use shared
  ownership policy. Retained tree target selection uses `TreeFocusBegin` /
  `TreeFocusAdvance` in both C and Go; registry storage and token validity stay
  native. The native ownership evidence table records these boundaries.
- Go context-menu activation/outside-close handling now uses shared policy.
  Popup ancestry, branch order, capture, focus and retirement execute through
  `runtime/popup_ownership.kry`, and Tab target selection through
  `FocusTraversalFor`. Composed scroll drag release/cancellation uses
  `ScrollScopeFrameFor`, including loss of popup ownership.
- The old web event path is paused. Its historical down/move/up and wheel
  driver work is reference material for the future web-native target; it is
  not an active native migration task.
- Active generated C/Go backends now have matched lifecycle matrix coverage for
  release-without-press, drag cancellation, disabled controls, popup capture,
  focus loss, and nested scope restoration. Keep future web-native checks out of
  this current gate until the web target resumes.

Native event queues and pointer-owner storage remain host services. The rules
that change ownership must have a generated decision owner and tests.
