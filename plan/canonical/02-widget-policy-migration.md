# Remaining widget policy migration

C/Go policy generation and canonical names are established. The remaining task
is an implementation inventory, not another API rename.

- Audit branches and defaults in `src/ui/ui.c`, `src/ui/ui_tk.c`,
  `src/ui/ui_text.c`, and `src/ui/ui_text_layout.c`.
- Include handwritten `go/kryon/runtime.go` in the active inventory. Retain
  `web/kryon-runtime.js` only as historical input for the future web target.
  Generated policy files existing beside hosts is insufficient.
- Record the owning `.kry` function or a specific storage/measurement/paint/OS
  reason for every retained behavior. Move unowned decisions into `runtime/`.
- Regenerate outputs and test behavior through the host callers, including
  disabled controls, captured focus, simultaneous keys, and empty data.

Completed in this cleanup: C text shortcuts, menu Escape/dismissal suppression,
Collapsible key priority, and Go menu Escape/Collapsible/text command routing.
Do not reintroduce separate implementations while finishing the other callers.

Composed popup admission, hover/context activation, release dismissal,
Escape and explicit/caller close now execute through `PopupLifecycle` in
`runtime/popup_policy.kry` in both native hosts. The standalone Escape wrappers
and native lifecycle branches have been removed. `make popup-policy-test`
covers C/C++ transitions; C and Go host tests cover blocked tooltip triggers,
modal capture, nested Escape/focus restoration and scope closure. Generated
C/Go composed-popup parity remains an active gate. Popup registry ancestry,
branch ordering, capture, autofocus, focus restoration and retirement now have
shared owners in `runtime/popup_ownership.kry`; Tab indices use
`FocusTraversalFor`. Composed Scroll geometry and interaction use
`ScrollScopeFrameFor` in C and Go. The old Go ancestor-path allocation and
scrollbar algorithm are removed. Registry storage, token validation and
clip/paint stack operations remain host services. Canvas and editable text
layout are the next audited surfaces.

Done when the ownership inventory has no unexplained policy and each moved
rule has active C/Go evidence. Future web work has a separate roadmap.
Validation commands are in `README.md`.
