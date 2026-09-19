# Style bridge ledger

This ledger records surviving styling bridges and deprecated visual-property
surfaces that are still allowed by the ratchet gates. A row is not approval for
new code; it is the deletion plan and regression evidence for debt that still
exists while downstream migrations continue.

| ID | Bridge surface | File/symbol | Maintained caller | Replacement owner | Deletion condition | Regression gate |
|---|---|---|---|---|---|---|
| B-001 | Legacy theme catalog application | `src/ui/ui.c`: `ApplyCurrentTheme`, `ui_set_theme_colors`, `GetThemeText`, `GetThemeBackground`, `GetThemeSurface`, `GetThemeCircle`, `GetThemeButton` | `InitUI` applies the pre-KSS global theme palette for legacy callers. | Built-in KSS packs plus `SetStyleTheme`/`ResolveActiveStyle`; platform preference remains a host service. | Delete after downstream apps no longer depend on the global color catalog and the app theme bridge is migrated to KSS overlays. | `paint-style-leak-check`, `no-theme-chrome-check`, style pack/theme tests. |
| B-002 | Platform background fallback | `src/ui/ui_tree.c`: `AppBackground` calls `GetThemeBackground` only when no KSS `App` background resolves. | App frame background painting. | KSS `App` rule; platform/system background service only as an explicit fallback. | Keep as a documented host degradation until every supported host supplies an explicit app background or the fallback moves behind a named platform service. | `paint-style-leak-check`, `no-theme-chrome-check`, app/background runtime tests. |
| B-004 | Native structural style bases | `src/ui/ui_page.c`: `page_box_style`, `page_text_style` seed page spacing and font metrics. | Page/section semantic layout and text measurement. | KSS rules own decoration; these bases are structural zero spacing and font fallback metrics. | Shrink/delete when Page/Section style rules can express all page layout defaults without host-side structural bases. | `no-theme-chrome-check`, `page-policy-test`. |
| B-005 | Go structural style bases | `go/kryon/control_style_host.go`: `styleForClassKind`, `defaultTextStyle`, `defaultTextStyleForKind`, `defaultTextStyleForClassKind`. | Go runtime layout/text measurement paths. | KSS rules own decoration; bases only seed spacing/font/opacity metrics required for measurement/content visibility. | Shrink/delete when Go runtime can resolve these metrics entirely from packs without changing no-style content behavior. | `no-theme-chrome-check`, `go-runtime-test`. |
| B-006 | Internal style data model fields | `runtime/control_props.kry`: `Style`, `ControlStyle` visual fields. | Generated style resolver, packs, and material/surface policies. | Keep in canonical KSS/style data model; do not expose as per-widget decorative props. | Do not delete while `StyleData` remains the generated cross-target style representation; instead keep widget prop surfaces free of duplicate decorative fields. | `visual-props-check`, style parser/resolver tests. |
| B-007 | Scene light visual content | `runtime/node2d_props.kry`: `Light2DProps.radius`, `Light2DProps.color`. | Scene/2D light content, not widget chrome. | Scene lighting API; future KSS integration may theme light defaults but should not erase authored light content. | Reclassify only if Node2D lighting becomes KSS-driven decoration rather than scene content. | `visual-props-check`, node/scene tests. |
| B-008 | Reorder placeholder paint output | `runtime/reorder_props.kry`: `ReorderPlaceholderPaint.radius`. | Generated reorder policy paint result consumed by renderers. | `runtime/reorder.kry` owns placeholder geometry; KSS should provide styling inputs before this paint output shrinks. | Delete or move after reorder placeholder appearance is fully KSS-resolved and renderers no longer consume a raw radius output. | `visual-props-check`, `reorder-policy-test`. |

The checker in `scripts/bridge-ledger-check.py` verifies that the ledger covers
the currently allowed style gate classes. It complements the ratchet gates in
`scripts/check-style-gates.py`, which fail on newly introduced unclassified
callers.

B-003 was removed: recorder Background, Box and Group snippets now use the
canonical `Surface(Rectangle, Style)` with an empty style resolved through KSS.
Its getter allowance was deleted, so theme getter snippets cannot return.

The unused `SetLinkColor` setter and the unconsumed hover/icon/link globals were
removed after scanning maintained source callers. B-001 now has five getter
calls. `ApplyCurrentTheme` is still called by Kryon and maintained Uku, Krait and
Rill entry points; it is not an unused API. Its remaining palette supplies
`GetThemeScheme` and the text-content fallback, not widget chrome. Migrating that
live contract is a separate downstream change, not a reason to keep dead globals.

The role derivation behind B-001 now comes from `runtime/theme.kry`:
`SchemeFor`, `OnColor` and `ToneFor` are shared by C, C++ and Go. The C cache and
native color adapters remain host services. `theme-policy-test` and Go tests
exercise the same fixtures, including alpha and channel-clamping boundaries.
