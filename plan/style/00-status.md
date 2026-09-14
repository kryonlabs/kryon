# 00 - Status

Status: living progress record
Scope: audit snapshot of the style separation project, continuation queue, and parallel-agent coordination rules.

Update this file whenever a phase advances. Files 01-09 describe the target design; this file records where the work actually is. The checklist (10) carries inline markers synced to this file.

## Audit Date

2026-09-14, at `de57be1b` (Document style implementation plan).

## Phase Status

| Phase | Area | State | Evidence |
|---|---|---|---|
| 1 | Style contract vocabulary | done | `StyleSheet`/`StylePack`/`StyleRule`/`StyleToken`/`StyleFacts`/`StyleData` in runtime; no public `Kss*` names |
| 1 | Scanners (visual bases, paint leaks, facts bridges, no-glow, theme chrome) | pending | none exist; `scripts/` only has the pre-existing `check-*` tools |
| 1 | No-style tests | pending | no `no_style`/`KRYON_STYLE` coverage in `Makefile` or `tests/` |
| 2 | Built-in catalog | done | `styles/kryon/{material,tk,vanilla,lightfield}.kss`; Material default; glow folded into lightfield (`a603fc44`) |
| 2 | Canonical grouped token syntax | done | all four packs use `tokens { ... }` blocks |
| 2 | Lightfield glow variant API | pending | glow folded into the catalog; explicit `@pack option` syntax not landed |
| 2 | Makefile gates | done | `kss-parser-test`, `style-assets-test`, `style-builtins-test`, `go-style-builtins[-check]` targets exist |
| 3 | Widget facts helpers | in progress | done: `runtime/toast.kry`, `runtime/table_view.kry` (`TableViewFactsFor`, `TableViewRoleFactsFor`); NavigationBar, PanedView handle, button generic path remain |
| 4 | Zero visual base | pending | six nonzero bases remain (list below) |
| 5 | Metrics/paint policy in `.kry` | partial | reorder, scroll/link/tab-bar release consumption, text input resolution migrated |
| 6 | Parser cleanup | partial | canonical syntax shipped in packs; legacy `@token` prefix removal, source maps, formatter pending |
| 7 | Runtime/backend parity | partial | Go TableView path now uses generated facts helpers; Go generic control path remains (`runtime.go:1747`) |
| 8 | Tooling/inspector | pending | capture boards and inspector phases not verified |
| 9 | Downstream | partial | 15 of 27 `examples/*.kry` attach `#style` |
| 10 | Legacy deletion | pending | theme getters, visual props, legacy syntax all still present |

## Remaining Direct Facts Construction (Phase 3 Queue)

None on the widget surface. The generic control path routes through `ButtonRoleFactsFor` (`runtime/button.kry`); migrated facts helpers: toast, table view, navigation bar, paned view handle, text input, button/generic control.

Done: TableView (`src/ui/ui_tk.c` and `go/kryon/runtime.go` now call `TableViewFactsFor`/`TableViewRoleFactsFor`); NavigationBar (C `navigation_bar.c` and the Go retained path use generated facts helpers with zero base); PanedView handle (C `tab_bar.c` and both Go retained sites, zero base); TextInput field defaults (C `ui.c` sites resolve from zero base via `TextInputFactsFor`); button/generic control (zero base, effective opacity composed in the resolve funnels).

Tests that construct `StyleControlFacts` directly are valid and stay.

## Remaining Nonzero Visual Bases (Phase 4 Queue)

- `src/ui/ui_style.c:242` - `ui_app_style` (opacity)
- `src/ui/ui_style.c:254` - `ui_surface_style` (opacity, font size, icon size, material)
- `src/ui/ui_image_cache.c:38` - `image_widget_tint_style` (opacity)
- `src/ui/ui_page.c:52` - `page_box_style` (gap/padding; structural, classify before moving)
- `src/ui/ui_page.c:67` - `page_text_style` (opacity, font size fallback)
- `src/ui/ui.c:5029` - `paragraph_text_color` (opacity)

## Pack Coverage Audit For The Generic Control Path (2026-09-14)

Kinds flowing through the generic control path (`simpleStyleFrameWithClassRole`, `resolveMinimal*` in Go; `ui_resolve_minimal_control_*` in C): Toggle, Modal, TableView, Collapsible, Slider, Menu, Toolbar, TitleBar, Separator, Progress, ToggleThumb, TabClose, Tab, MenuItem, TreeViewItem, TreeView, TabBar, Spinbox, SpinboxValue, Selectable, SegmentedControl, Segment, Plot, PlotMark, MenuSeparator, ListBox, ListBoxItem, ListBoxMulti, ListBoxMultiItem, Link, Image, Fieldset, Drag, DragValue, ColorPicker, ColorPickerSwatch, Canvas, Button, Radio, Checkbox, Dropdown.

Findings across all four packs:

- `opacity` and `material` are declared for nearly all kinds - exceptions: `Separator` (material/opacity gaps in tk/vanilla/lightfield), `ColorPicker` (opacity+material missing everywhere), `Dropdown` (opacity missing everywhere).
- `font-size` is missing for nearly every kind except Button and TextField/TextArea - the hidden base's `font_size = 16` is load-bearing.
- `icon-size` is missing for nearly every kind except Button - the hidden base's `icon_size = 20` is load-bearing.

Worklist before the generic base can be zero-based:

1. [done 2026-09-14] Complete `font-size`/`icon-size` declarations for every text/icon-bearing kind listed above in all four packs (29-30 base rules per pack updated, using each pack's own `font`/`icon` tokens).
2. [done 2026-09-14] Close the `Separator`/`ColorPicker`/`Dropdown` opacity+material gaps.
3. Subpart kinds with no text or icons (ToggleThumb, TabClose, Segment, SpinboxValue, MenuSeparator, PlotMark, ColorPickerSwatch, Separator) intentionally carry no font/icon fields; their metrics fallbacks are structural.
4. [done 2026-09-14] Generic control base zeroed: `ui_minimal_control_style_data()`/`minimalControlStyleData()` deleted; resolve funnels compose effective opacity through `StyleOpacityValue`. Full Go matrix, C policy gates, and `ui_tk_test` green in the same commit.

The Go runtime auto-activates Material when the style registry is empty (`go/kryon/style_pack.go` `EnsureBuiltInStylePacks`), so runtime tests exercise pack-styled rendering, not no-style rendering.

## Opacity Contract (decided 2026-09-14)

Opacity is optional and absent means fully visible, like browser CSS:

- `StyleOpacityValue(fields, opacity)` in `runtime/style.kry` is the consumption helper: declared value wins (explicit `0` stays `0`), absent field composes as `1`.
- The Text record path now composes color with it (`runtime.go` `textWithFont`).
- Packs do NOT need `opacity: 1` boilerplate; the 410 `opacity: 1;` lines currently in packs are redundant but harmless, and state rules legitimately use `opacity: 1` to reset lower base values - do not bulk-strip without cascade review.
- Known gap: `FrameOp` does not carry field presence, so record sites are the only place that can distinguish absent from explicit zero. `render.go` already contains ad-hoc `if Opacity == 0 { = 1 }` patches - unify them through `StyleOpacityValue` when the record sites are normalized.

## Zero-Base Flip (landed 2026-09-14)

The generic control hidden base is gone. `ui_resolve_minimal_control_role_state` (C) and `resolveMinimalControlRoleState`/`defaultStyleFrame` (Go) resolve from `(StyleData){0}` and compose effective opacity through `StyleOpacityValue`: declared value wins, explicit zero is preserved, absent composes as visible. Field bits keep declared-ness for explicit-zero tests.

Test expectations migrated in the same commit:

- plain surface without a rule no longer fabricates flat material (`material_test.go`);
- ambient-styling tests attach the material pack via `useMaterialStyleForTest` (`runtime_test.go`, `surface_test.go`, `imgui_widgets_test.go`);
- the no-style button test asserts `Fields == 0` (`style_runtime_test.go`);
- the remaining raw-opacity consumers are covered by the funnel normalization, so `render.go` needs no changes.

## Continuation Queue

Next commits, in order:

1. [done 2026-09-14] TableView facts helper in `runtime/table_view.kry` (C `ui_tk.c` sites + Go `runtime.go` in one commit).
2. [done 2026-09-14] NavigationBar facts helper (C `navigation_bar.c` + Go retained path, zero base).
3. [done 2026-09-14] PanedView handle facts helper (`tab_bar.c`, Go retained path, zero base).
4. [done 2026-09-14] TextInput field defaults: `ui.c` sites resolve from zero base via `TextInputFactsFor`; `opacity: 1` moved into `TextField`/`TextArea` rules in all four packs.
5. [done 2026-09-14] Button/generic control facts path: `ButtonRoleFactsFor` in `runtime/button.kry`, `button-policy-test` gate, pack coverage for every generic-path kind, and the zero-base flip with effective opacity in the resolve funnels. Full matrix green.
6. Phase 4 visual bases (list below), one widget per commit, moving needed values into the built-in packs.
7. No-style tests, then the five scanners as Makefile gates.

## Parallel Agent Coordination

A second agent works in this same repository on the canonical `.kry` surface (plans in `plan/canonical/` and `plan/dom/`). This plan owns styling only.

Hard rules while both agents share the working tree:

- Never run `git checkout`, `git switch`, `git restore`, `git reset`, `git stash`, or `git clean`. No branch or tree operations, ever.
- Never touch `plan/canonical/`, `plan/dom/`, or files the canonical agent is actively editing.
- Never stage broadly: `git add` explicit paths only, then verify the staged set with `git diff --cached --stat` before committing.
- Run `make generate-runtime` only when a styling change requires it, and only after checking `git status` for the other agent's in-flight edits.
- The migration rule from file 04 applies: only migrate widgets whose `.kry` surface is already stable; skip widgets mid-conversion.
