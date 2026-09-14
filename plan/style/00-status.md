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
| 3 | Widget facts helpers | started | only `runtime/toast.kry` (`ToastSurfaceFactsFor`, `ToastLabelFactsFor`) |
| 4 | Zero visual base | pending | six nonzero bases remain (list below) |
| 5 | Metrics/paint policy in `.kry` | partial | reorder, scroll/link/tab-bar release consumption, text input resolution migrated |
| 6 | Parser cleanup | partial | canonical syntax shipped in packs; legacy `@token` prefix removal, source maps, formatter pending |
| 7 | Runtime/backend parity | partial | Go still mirrors TableView with direct facts (`go/kryon/runtime.go:6238`) |
| 8 | Tooling/inspector | pending | capture boards and inspector phases not verified |
| 9 | Downstream | partial | 15 of 27 `examples/*.kry` attach `#style` |
| 10 | Legacy deletion | pending | theme getters, visual props, legacy syntax all still present |

## Remaining Direct Facts Construction (Phase 3 Queue)

- `src/ui/ui_tk.c:3614-3627` - TableView table/header/cell/divider frames (4 sites)
- `src/ui/navigation_bar.c:255` - NavigationBar role frames
- `src/ui/tab_bar.c:778` - PanedView handle frame
- `src/ui/button.c:33` - generic control facts path (decide: shared `.kry` helper or widget-local)
- `go/kryon/runtime.go:1747` - `resolveMinimalControlRoleState` generic helper
- `go/kryon/runtime.go:6238` - `tableViewMetricFrame`

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

1. TableView facts helper in `runtime/table_view.kry` (C `ui_tk.c` sites + Go `runtime.go:6238` in one commit).
2. NavigationBar facts helper (C `navigation_bar.c` + Go retained path).
3. PanedView handle facts helper (`tab_bar.c`).
4. Button generic facts path decision (`button.c` + `resolveMinimalControlRoleState`).
5. Phase 4 visual bases, one widget per commit, moving needed values into the built-in packs.
6. No-style tests, then the five scanners as Makefile gates.

## Parallel Agent Coordination

A second agent works in this same repository on the canonical `.kry` surface (plans in `plan/canonical/` and `plan/dom/`). This plan owns styling only.

Hard rules while both agents share the working tree:

- Never run `git checkout`, `git switch`, `git restore`, `git reset`, `git stash`, or `git clean`. No branch or tree operations, ever.
- Never touch `plan/canonical/`, `plan/dom/`, or files the canonical agent is actively editing.
- Never stage broadly: `git add` explicit paths only, then verify the staged set with `git diff --cached --stat` before committing.
- Run `make generate-runtime` only when a styling change requires it, and only after checking `git status` for the other agent's in-flight edits.
- The migration rule from file 04 applies: only migrate widgets whose `.kry` surface is already stable; skip widgets mid-conversion.
