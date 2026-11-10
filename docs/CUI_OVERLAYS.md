
The six overlay families are retained Ziran widgets with caller-owned state and deterministic results. `Apply` functions are pure state transitions suitable to component hosting; widget functions take a `Session`, explicit props and a caller-owned state pointer, then submit popup/modal tree nodes and paint effects. No family owns process-global UI state.

## Composition

- `Tooltip(session, props, state)` and `TooltipApply(state, props)` implement hover/focus delay, warm replay, touch/press suppression, truncation-only mode, placement fitting, flipping, clamping and ellipsis text.
- `Sheet(session, props, state)` and `SheetApply(state, props)` implement all four sides, clamped min/max size, safe-area insets, scrim/Escape/drag close, deterministic motion and focus wrap/return.
- `CommandPalette(session, props, state)` and `CommandPaletteApply(state, props)` implement fuzzy prefix/run/word scoring, stable command order, contiguous group starts, text editing, Up/Down/Enter/Escape, pointer activation and explicit owner takeover.
- `HoverCard(session, props, state)` and `HoverCardApply(state, props)` implement open delay, movement tolerance, content grace, interactive action dispatch, focus paths and placement fitting.
- `ConfirmPopover(session, props, state)` and `ConfirmPopoverApply(state, props)` implement danger styling, initial confirm focus, Enter/Space confirm, Escape/cancel, outside close and trigger focus return.
- `GuidedTour(session, props, state)` and `GuidedTourApply(state, props)` implement target geometry, padded highlight hole, four-band scrim, card fitting, count formatting, Back/Next/Done/Skip and three-focus wrap.

Each `*StateFor` constructor returns an isolated, serializable state value. Results identify nodes, geometry, interaction outcome and state change; callers supply `now_ms` and retain ownership. Empty `id`, `class_name`, key and action values follow the explicit prop values, while nonpositive geometry/delay fallbacks use the documented defaults in the matching `*_props.zi` module.

## Styles and semantics

Every family has typed `StyleData` props with installed selector facts for kind, ID, class, role, state and popup/modal/guide style kind. Caller styles merge over theme/selector values with `MergeValues`; state overrides resolve through `StyleStates`. Light and dark defaults come from `CurrentTheme`, and motion honors duration, multiplier and reduced-motion fields.

Tree nodes carry style ID/class facts plus interactive, popup, semantic kind, heading and label metadata. Paint uses the real queued raster API; no mock painter or parallel styling protocol exists.

## Verification

`tests/ziran_cui_overlays_test.sh` checks source and saved-IR bundle byte identity, runs both ZX programs, builds and executes generated C and Go from both representations, and exercises behavior, committed tree metadata, custom geometry/font/motion/style IDs and real 480×360 light/dark pixmap raster hashes without a desktop display.

The six overlay families are retained Ziran widgets with caller-owned state and deterministic results. `Apply` functions are pure state transitions suitable to component hosting; widget functions take a `Session`, explicit props and a caller-owned state pointer, then submit popup/modal tree nodes and paint effects. No family owns process-global UI state.

## Composition

- `Tooltip(session, props, state)` and `TooltipApply(state, props)` implement hover/focus delay, warm replay, touch/press suppression, truncation-only mode, placement fitting, flipping, clamping and ellipsis text.
- `Sheet(session, props, state)` and `SheetApply(state, props)` implement all four sides, clamped min/max size, safe-area insets, scrim/Escape/drag close, deterministic motion and focus wrap/return.
- `CommandPalette(session, props, state)` and `CommandPaletteApply(state, props)` implement fuzzy prefix/run/word scoring, stable command order, contiguous group starts, text editing, Up/Down/Enter/Escape, pointer activation and explicit owner takeover.
- `HoverCard(session, props, state)` and `HoverCardApply(state, props)` implement open delay, movement tolerance, content grace, interactive action dispatch, focus paths and placement fitting.
- `ConfirmPopover(session, props, state)` and `ConfirmPopoverApply(state, props)` implement danger styling, initial confirm focus, Enter/Space confirm, Escape/cancel, outside close and trigger focus return.
- `GuidedTour(session, props, state)` and `GuidedTourApply(state, props)` implement target geometry, padded highlight hole, four-band scrim, card fitting, count formatting, Back/Next/Done/Skip and three-focus wrap.

Each `*StateFor` constructor returns an isolated, serializable state value. Results identify nodes, geometry, interaction outcome and state change; callers supply `now_ms` and retain ownership. Empty `id`, `class_name`, key and action values follow the explicit prop values, while nonpositive geometry/delay fallbacks use the documented defaults in the matching `*_props.zi` module.

## Styles and semantics

Every family has typed `StyleData` props with installed selector facts for kind, ID, class, role, state and popup/modal/guide style kind. Caller styles merge over theme/selector values with `MergeValues`; state overrides resolve through `StyleStates`. Light and dark defaults come from `CurrentTheme`, and motion honors duration, multiplier and reduced-motion fields.

Tree nodes carry style ID/class facts plus interactive, popup, semantic kind, heading and label metadata. Paint uses the real queued raster API; no mock painter or parallel styling protocol exists.

## Verification

`tests/ziran_cui_overlays_test.sh` checks source and saved-IR bundle byte identity, runs both ZX programs, builds and executes generated C and Go from both representations, and exercises behavior, committed tree metadata, custom geometry/font/motion/style IDs and real 480×360 light/dark pixmap raster hashes without a desktop display.
