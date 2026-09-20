# Text input ownership and verification

Editing decisions live in `runtime/text_input.kry`. The active Kry and Go hosts
apply generated decisions and store the resulting text and selection. The old
JavaScript/web host path is paused and kept only as future-roadmap reference
material.

| Path | Selection and focus ownership | Platform services |
| --- | --- | --- |
| Immediate Kry (`src/ui/text_field.kry`, `text_area.kry`) | `TextSelectionOwnerMatches`, `TextFocusOwnerDecisionFor`, generated selection ranges and navigation | Caller-buffer/pointer identity, input collection, font measurement, clipboard |
| Retained Kry (`src/ui/tree_input.kry`) | Node-owned state; generated collapse/select-all/navigation, composition input gates and edit-command decisions | Node lifetime, focus registration, buffer movement, paint invalidation |
| Go (`go/kryon/runtime.go`, `composition.go`) | Focus-ID keyed state; generated selection, navigation, deletion and composition decisions | UTF-8 traversal, field registration, event queues, font measurement |
| Web (`web/text_edit.js`, `text_dom.js`) | Runtime focus ID and DOM-node binding; generated editing/navigation/composition decisions | UTF-16/UTF-8 conversion, native visual-row geometry, pointer hit testing, clipboard transport and IME presentation |

The retained Kry composition cancellation gate now uses the same generated
`TextCompositionInputDecisionFor` as Go and web. Numeric editors in `numeric_edit.kry`
retain numeric-control activation and storage; they are not an alternate
TextField implementation. Their separate numeric policy remains in `input.kry`.

DOM events do not enqueue a second synthetic tap. Browser selection is
translated back to byte offsets before editing and after native pointer or
visual-row movement. Stable DOM nodes are not detached or emptied during
redraw. Read-only/disabled transitions, blur, and replacing an editor binding
cancel provisional composition. Native IME presentation is not copied into
the committed application buffer until commit.

The compiler resolves `sizeof(name)` for fixed char arrays declared in module
state into numeric TextField/TextArea capacities. The capacity regression uses
an eight-byte buffer, an emoji replacement, and a preserved Unicode suffix.

## Verified behavior

- `make keyboard-policy-test`: generated policy and queued web editing, Unicode
  offsets, clipboard restrictions, insertion limits, multiline navigation,
  read-only fields, focus handoff, and composition ownership.
- `make web-text-capacity-test`: compiled `.kry` state-buffer capacity enforcement.
- `make web-text-input-browser-test`: live DOM events plus real Chromium keyboard
  and pointer input, wrapped rows, Home/End, Page Up/Down scrolling, backward Tab,
  native browser preedit, redraw, single commit, and screenshots.
- JavaScript generated-runtime parity is paused with the broader JS/web target.
  Keep native C/Go text behavior covered through active runtime and policy gates.
- `make go-runtime-test` and the retained `widget_surface_test`: native ownership,
  navigation, focus, composition and retained-control regressions.

The browser tests use Chromium's native composition API. They do not automate
an operating-system IME candidate window or establish coverage for every
browser/OS pairing. Logical-line queued tests and browser visual-row tests are
reported separately; the full native composition fixture still includes native
layout and pointer scenarios beyond the shared event sequence.
