# Remaining text editing work

- Audit retained/immediate selection ownership and focus transitions for
  decisions still implemented independently of `runtime/text_input.kry`.
- Connect browser DOM input/composition events and selection presentation to
  the shared editor path. Queued web input is covered; browser-native IME
  candidate windows and actual DOM selection still need integration tests.
- Finish web parity for composition presentation, wrapped-line geometry,
  pointer selection and scrolling. The composition fixture now executes web
  preedit, commit/cancel and read-only behavior; its full native scenario
  also tests layout-dependent movement and selection.
- Carry declared `.kry` buffer capacities into web props. The queued web host
  enforces explicit numeric text sizes and codepoint limits, but `sizeof(buffer)`
  is not yet evaluated in serialized widget props.

Raw buffers, memory movement, clipboard IO, font measurement, UTF-8 traversal,
and platform IME remain host services. `EditText` is the active native
buffer-edit service. Do not create a new text widget or forwarding layer.
See `docs/BOUNDARIES.md` for the shared policy/host split.
