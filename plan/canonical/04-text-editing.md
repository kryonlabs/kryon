# Text editing status

The native ownership/layout slice is implemented: `runtime/text_input.kry`
owns editing, selection, navigation intent and composition; `runtime/text_rows.kry`
owns logical/visual rows, wrapping, heading font selection and caret affinity.
C and Go preserve source byte ranges and use host Unicode/font services.
Go pointer placement now includes row Y and scrolling. The matched row fixtures,
editor regressions and generated composition fixture are active evidence.

Remaining platform work:

- Route real native Go OS-window IME events into the existing composition queue.
- Verify OS candidate windows on each supported desktop/mobile host.
- JS/web is paused. Historical DOM composition, visual-row tests and expanded
  buffer-capacity lowering belong to the future web-native target.

These are host integration or future-target tasks, not additional editor policy
implementations. See `docs/TEXT_INPUT_BEHAVIOR.md` and
`docs/NATIVE_POLICY_OWNERSHIP.md`.
