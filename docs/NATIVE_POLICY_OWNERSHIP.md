# Native policy ownership evidence

This is a historical audit of the former `.kry` runtime and native widget
hosts. For the maintained Ziran library and its current host boundary, see
`ARCHITECTURE.md` and `API.md`.

This audit covers the remaining input, composed-scope and text-layout slices
of the canonical migration. C and Go are active targets; C++ executes generated
policy over the C host. JS/web stays paused and is future roadmap work.

| Behavior | Shared source | Host responsibility | Evidence |
|---|---|---|---|
| Popup admission, hover/context activation, dismissal and close | `runtime/popup_policy.kry`: `PopupLifecycle*` | Caller-owned open values, event sampling and paint scopes | Popup C/C++ policy tests, native/Go popup tests, generated composed-popup fixture |
| Popup ancestry, branch priority, capture, autofocus, restoration and retirement | `runtime/popup_ownership.kry` | Parent links/maps, opaque IDs, allocation and token lifetime checks | Ownership policy tests, all 49 branch-order pairs, late child after parent closure |
| Tab order and retained tree target traversal | `runtime/focus.kry`: `FocusTraversalFor`, `TreeFocusBegin`, `TreeFocusAdvance` | Eligible-ID registry and node-depth lookup | Focus policy tests and generated button/tree interaction sequences |
| Scroll content, thumb/wheel interaction, drag cancellation and release use | `runtime/scroll.kry`: `ScrollScopeFrameFor` | Caller offset pointers, owner tokens and clip/paint stacks | Scroll policy and Go scope tests, generated scroll fixture |
| Canvas camera selection, coordinates and hit result | `runtime/canvas.kry` | C matrix snapshots; Go frame-operation adaptation; per-scope clip restoration | `canvas-scope-test`, Go Canvas tests, generated Scroll/Canvas early exits |
| Disabled, Popup, Scroll, TableCell and layout scope teardown | Canonical block lowering plus the widget policy modules | Saved native stacks and compiler-inserted matching exits | Generated return/break/continue and disabled/captured-child interactions |
| Read-only paragraph tokens, line assembly, spacing and alignment | `runtime/paragraph.kry` | Owned text/element arrays, shaped text measurement, glyph/icon painting | 16 shared paragraph fixtures, C/C++ policy, Go Paragraph integration and inline icons |
| Editable logical/visual rows, word breaks, heading size and caret affinity | `runtime/text_rows.kry` | UTF-8 strings, Unicode grapheme services, measured prefixes and row storage | 13 identical C/Go fixtures and C++ policy; native editor and Go click/scroll tests |
| Selection, editing, navigation intent and composition ranges | `runtime/text_input.kry` | Buffer mutation, clipboard transport, event queues, Unicode boundary/column lookup and OS composition delivery | Native editor tests, Go navigation/composition tests, generated composition fixture |
| Table sort and tree row activation | `runtime/table_view.kry`, `runtime/tree_view.kry` | Caller selection/sort pointers, collection lookup and drawing | Existing policy suites and generated table/tree fixtures |
| Layout, style resolution and drawing geometry | `runtime/layout.kry`, widget modules and style/material modules listed in `CANONICAL_WIDGET_SURFACE.md` | Retained node storage, cache invalidation, renderer commands and font/image resources | Layout/style policy gates and generated C/Go runtime parity |

The replaced ancestry-path allocation, independent Scroll geometry, Canvas depth
counter, duplicated text row loops, whitespace-collapsing Go editor wrapper,
retained tree target scans and Go sort-cycle branches have been removed. No
compatibility names or parallel editor were added.

Storage and rendering adapters intentionally remain native. Moving allocations,
GPU matrices, Unicode library calls or OS event delivery into forwarding `.kry`
functions would not transfer behavior ownership.

## Platform and roadmap boundaries

- Go OS-window IME routing and platform candidate-window validation remain
  integration work. Synthetic composition parity does not verify OS UI.
- Native Go raw texture/image upload is renderer work; inline built-in paragraph
  icons are supported. Clip tests validate operation bounds and restoration,
  not complete image decoding/upload support.
- Read-only text selection capabilities differ between host surfaces; the
  matched cross-line editing contract is TextArea. This audit does not claim
  browser-style rich-document range selection in every renderer.
- Game2D physics/audio/scene storage remain native host systems. Kapsule's
  terminal emulator and product behavior remain in Kapsule.
- The broader language, KSS and website roadmap is separate. This document is
  evidence for the audited native migration, not a claim that every plan in
  the repository is finished.

## Repeatable checks

```sh
make generate-native-runtime
make popup-policy-test focus-policy-test scroll-policy-test canvas-policy-test
make text-rows-policy-test paragraph-policy-test canvas-scope-test
make generated-runtime-parity-test runtime-parity-check
make canonical-surface-test public-api-names-check public-api-snapshot-check generated-provenance-check
make build/linux-x86_64/tests/widget_surface_test build/linux-x86_64/tests/ui_text_edit_test
xvfb-run -a build/linux-x86_64/tests/widget_surface_test
xvfb-run -a build/linux-x86_64/tests/ui_text_edit_test
go -C go/kryon test ./...
```

Verified on 2026-09-19: the 17 generated C/Go interaction fixtures, all 13
text-row fixtures, C/C++ policy checks, Go runtime suite, Go compiler syntax
suite, API/provenance/surface guards, native editor tests and the full native
UI test passed. Graphics tests ran under Xvfb. Canvas coverage includes the
headless host as well as actual backend matrix restoration.
