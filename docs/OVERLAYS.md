# Overlays

The six overlay families are retained Ziran widgets with caller-owned state and deterministic results. `Apply` functions are pure state transitions suitable to component hosting; widget functions take a `Session`, explicit props and a caller-owned state pointer, then submit popup/modal tree nodes and paint effects. No family owns process-global UI state.

Import `kryon/tooltip`, `kryon/sheet`, `kryon/command_palette`,
`kryon/hover_card`, `kryon/confirm_popover` or `kryon/guided_tour` under the
corresponding widget name. Each module exposes `Props` and `Draw`.

## Composition

- `Tooltip.Draw(session, props, state)` and `Tooltip.TooltipApply(state, props)` implement hover/focus delay, warm replay, touch/press suppression, truncation-only mode, placement fitting, flipping, clamping and ellipsis text.
- `Sheet.Draw(session, props, state)` and `Sheet.SheetApply(state, props)` implement all four sides, clamped min/max size, safe-area insets, scrim/Escape/drag close, deterministic motion and focus wrap/return.
- `CommandPalette.Draw(session, props, state)` and `CommandPalette.CommandPaletteApply(state, props)` implement fuzzy prefix/run/word scoring, stable command order, contiguous group starts, text editing, Up/Down/Enter/Escape, pointer activation and explicit owner takeover.
- `HoverCard.Draw(session, props, state)` and `HoverCard.HoverCardApply(state, props)` implement open delay, movement tolerance, content grace, interactive action dispatch, focus paths and placement fitting.
- `ConfirmPopover.Draw(session, props, state)` and `ConfirmPopover.ConfirmPopoverApply(state, props)` implement danger styling, initial confirm focus, Enter/Space confirm, Escape/cancel, outside close and trigger focus return.
- `GuidedTour.Draw(session, props, state)` and `GuidedTour.GuidedTourApply(state, props)` implement target geometry, padded highlight hole, four-band scrim, card fitting, count formatting, Back/Next/Done/Skip and three-focus wrap.

Each `*StateFor` constructor returns an isolated, serializable state value. Results identify nodes, geometry, interaction outcome and state change; callers supply `now_ms` and retain ownership. Empty `id`, `class_name`, key and action values follow the explicit prop values, while nonpositive geometry/delay fallbacks use the documented defaults in the matching `*_props.zi` module.

## Styles and semantics

Every family has typed `StyleData` props with installed selector facts for kind, ID, class, role, state and popup/modal/guide style kind. Caller styles merge over theme/selector values with `MergeValues`; state overrides resolve through `StyleStates`. Light and dark defaults come from `CurrentTheme`, and motion honors duration, multiplier and reduced-motion fields.

Tree nodes carry style ID/class facts plus interactive, popup, semantic kind, heading and label metadata. Paint uses the real queued raster API; no mock painter or parallel styling protocol exists.

