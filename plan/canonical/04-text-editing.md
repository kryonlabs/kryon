# Remaining text editing work

TextField/TextArea metrics, edit intent, selection ranges, composition gates,
keyboard navigation, and clipboard command decisions already have `.kry`
owners. Immediate C shortcuts and Go clipboard command decisions now use them.
`EditText` remains the actual buffer-edit service used by both native fields;
it is not an unused compatibility wrapper that can simply be deleted.

Remaining:

- Audit retained/immediate selection ownership, IME focus transitions, and
  Go text editing for decisions still implemented independently of `.kry`.
- Replace web handwritten editing decisions with generated policy where the
  semantics are shared. Test secure and read-only fields, UTF-8 boundaries,
  selection replacement, clipboard commands, and preedit commit/cancel.
- Exercise `tests/parity/composition.kry` behavior in the web runner. Generation
  alone does not verify composition, focus ownership, or platform integration.
- Document storage and OS boundaries separately from unfinished widget logic.

Raw buffers, memory movement, clipboard IO, font measurement, and platform IME
remain native services. Do not create a new text widget or forwarding layer.
