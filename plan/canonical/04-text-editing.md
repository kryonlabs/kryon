# Remaining text editing work

- Audit retained/immediate selection ownership, IME focus transitions, and
  Go focus/event routing for decisions still implemented independently of `.kry`.
  Cover queued input when Tab moves to a field already drawn in the frame;
  EndFrame currently discards that unconsumed input.
- Replace web handwritten editing decisions with generated policy where the
  semantics are shared. Test secure and read-only fields, UTF-8 boundaries,
  selection replacement, clipboard commands, and preedit commit/cancel.
- Exercise `tests/parity/composition.kry` behavior in the web runner. Generation
  alone does not verify composition, focus ownership, or platform integration.

Raw buffers, memory movement, clipboard IO, font measurement, and platform IME
remain host services. `EditText` is the active native buffer-edit service.
Do not create a new text widget or forwarding layer. See
`docs/BOUNDARIES.md` for the shared policy/host split.
