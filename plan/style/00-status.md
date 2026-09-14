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

- `src/ui/tab_bar.c:778` - PanedView handle frame
- `src/ui/button.c:33` - generic control facts path (decide: shared `.kry` helper or widget-local)
- `go/kryon/runtime.go:1747` - `resolveMinimalControlRoleState` generic helper
- `go/kryon/runtime.go` - ~100 generic sites via `simpleStyleFrameWithClassRole`/`resolveMinimal*` + `minimalControlStyleData` base (the big Phase 3/4 combined step)

Done: TableView (`src/ui/ui_tk.c` and `go/kryon/runtime.go` now call `TableViewFactsFor`/`TableViewRoleFactsFor`); NavigationBar (C `navigation_bar.c` and the Go retained path now use generated facts helpers with zero base, replacing `simpleStyleFrameWithClassRole` and its hidden `minimalControlStyleData` base); PanedView handle (C `tab_bar.c` and both Go retained sites, zero base); TextInput field defaults (C `ui.c` text-input sites resolve from zero base via `TextInputFactsFor`; the former hidden `opacity: 1` base moved into `TextField`/`TextArea` rules in all four packs).

Tests that construct `StyleControlFacts` directly are valid and stay.

## Remaining Nonzero Visual Bases (Phase 4 Queue)

- `src/ui/ui_style.c:242` - `ui_app_style` (opacity)
- `src/ui/ui_style.c:254` - `ui_surface_style` (opacity, font size, icon size, material)
- `src/ui/ui_image_cache.c:38` - `image_widget_tint_style` (opacity)
- `src/ui/ui_page.c:52` - `page_box_style` (gap/padding; structural, classify before moving)
- `src/ui/ui_page.c:67` - `page_text_style` (opacity, font size fallback)
- `src/ui/ui.c:5029` - `paragraph_text_color` (opacity)

## Continuation Queue

Next commits, in order:

1. [done 2026-09-14] TableView facts helper in `runtime/table_view.kry` (C `ui_tk.c` sites + Go `runtime.go` in one commit).
2. [done 2026-09-14] NavigationBar facts helper (C `navigation_bar.c` + Go retained path, zero base).
3. [done 2026-09-14] PanedView handle facts helper (`tab_bar.c`, Go retained path, zero base).
4. [done 2026-09-14] TextInput field defaults: `ui.c` sites resolve from zero base via `TextInputFactsFor`; `opacity: 1` moved into `TextField`/`TextArea` rules in all four packs.
5. Button/generic control facts path (`button.c` + `resolveMinimalControlRoleState` + ~100 Go generic sites) - the remaining Phase 3/4 combined step.
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
