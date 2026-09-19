# Native Accessibility Completion

This tracks the six-milestone accessibility plan. It is not a claim of complete
accessibility conformance. Changes belong upstream in Kryon master; downstream
repositories receive committed submodule updates only.

## Verified on Linux (2026-09-19)

- Native Go full race suite.
- C/Go private D-Bus tree and selection tests; C UI modules and adapter rebuilt
  with AddressSanitizer/UndefinedBehaviorSanitizer (leak detection disabled).
- A 4,096-option C snapshot with ordered indices, keys, and parent assertions.
- All 17 generated C/Go parity fixtures, also with the sanitized C UI build.
- Real libatspi single/multiple list selection smoke test.
- Public API snapshot and generated-output/clean Text API checks.

Final C checks used an isolated export of the staged accessibility changes to
avoid mixing unrelated, concurrently edited slider code into the build.

## 1. Shared Semantic Tree

- Implemented: one-based parent references, explicit semantic keys, nested
  C/Go groups, nearest exposed ancestors, composed-label suppression.
- Implemented: Linux child queries, parent-relative geometry, cache hierarchy,
  keyed group reordering and control reparenting.
- Implemented: list option identity with per-list item keys.
- Remaining: general descriptions/relations, expansion and table metadata,
  complete semantic projection for all composed controls and popup workflows.

## 2. Complex Controls

- Implemented: single/multiple ListBox selection, deselection, select-all,
  clearing, off-screen options, scroll reveal, live eligibility checks, and
  stable-key delivery across reorder.
- Remaining: per-item disabled state, dropdown choices, expandable tree items,
  table row/cell selection, tabs, slider/spinbox range values.
- Completion gate: matching generated C/Go assertions and external-client tests
  for each control, including removal, reordering, disabled state, and popups.

## 3. Linux Completion

- Implemented: native C/Go AT-SPI registration, nested object trees, cache,
  focus/activation, editor reads/replacement/selections, list Selection methods.
- Remaining: reconnection, C insertion/deletion parity, rendered-line and glyph
  geometry, relations/attributes, clipboard operations, legacy text boundaries,
  and end-to-end Orca workflows.
- Completion gate: a representative app is usable without pointer input and
  survives accessibility bus/registry restarts.

## 4. Windows and macOS

- Remaining: native adapters, object lifetime handling, text/selection/value
  mappings, and notifications.
- Native Go non-Linux window hosts are an explicit prerequisite, not provided
  by an accessibility transport alone.
- Completion gate: target-native automated checks plus Narrator/NVDA and
  VoiceOver workflows. Linux-only tests cannot satisfy this gate.

## 5. Android and iOS

- Remaining: semantic element export, accessibility focus, activation, editing,
  scrolling, adjustable values, and lifecycle/keyboard handling.
- Completion gate: matching TalkBack and VoiceOver workflows on devices.

## 6. Downstream Validation

- Remaining: select representative maintained apps, audit labels/keys/reading
  order/modals, update their committed Kryon pointers, and publish a tested
  platform/control matrix with explicit exclusions.
- Completion gate: recorded evidence for every supported combination; no
  platform marked complete from compilation alone.
