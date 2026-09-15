# Remaining text editing work

- Extend matched composition-fixture execution to the remaining native
  layout/pointer scenarios. Web logical editing and live Chromium visual-row,
  pointer and composition tests now run, but are separate tests.
- Verify native OS IME candidate windows and browser/platform combinations
  beyond Chromium. The automated browser test exercises native composition
  events and visible preedit, not the OS candidate window.
- Extend web capacity lowering beyond fixed char arrays in module state where
  maintained callers need local buffers or computed capacity expressions.

Completed behavior, source ownership and verification commands live in
`docs/TEXT_INPUT_BEHAVIOR.md`. Buffers, OS input, font measurement, UTF-8
traversal and browser geometry remain host services. Keep editing decisions in
`runtime/text_input.kry`; do not add a second editor or compatibility layer.
