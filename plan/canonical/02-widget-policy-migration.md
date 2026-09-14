# Remaining widget policy migration

C/Go policy generation and canonical names are established. The remaining task
is an implementation inventory, not another API rename.

- Audit branches and defaults in `src/ui/ui.c`, `src/ui/ui_tk.c`,
  `src/ui/ui_text.c`, and `src/ui/ui_text_layout.c`.
- Include handwritten `go/kryon/runtime.go` and `web/kryon-runtime.js` in the
  inventory. Generated policy files existing beside them is insufficient.
- Record the owning `.kry` function or a specific storage/measurement/paint/OS
  reason for every retained behavior. Move unowned decisions into `runtime/`.
- Regenerate outputs and test behavior through the host callers, including
  disabled controls, captured focus, simultaneous keys, and empty data.

Completed in this cleanup: C text shortcuts, menu Escape/dismissal suppression,
Collapsible key priority, and Go menu Escape/Collapsible/text command routing.
Do not reintroduce separate implementations while finishing the other callers.

Done when the ownership inventory has no unexplained policy and each moved
rule has C/Go/web evidence. Validation commands are in `README.md`.
