# 10 - Execution Checklist

Status: implementation plan
Scope: concrete end-to-end order for finishing full Kryon styling support.

## Objective

Provide the 1:1 work plan for finishing the style separation project.

This file is the execution checklist. The other nine files explain each area in detail.

Progress is recorded in `00-status.md` (audit 2026-09-14). Inline markers below are synced to it.

## Phase 1 - Stabilize Contract

Tasks:

1. Keep `docs/STYLE_SEPARATION_PROPOSALS.md` aligned with this plan.
2. Confirm public vocabulary: `StyleSheet`, `StylePack`, `StyleRule`, `StyleToken`, `StyleFacts`.
3. Keep `kss_` names internal to parser/formatter/import loader.
4. Document zero-base style resolution as invariant.
5. [pending] Add or update scanner for nonzero visual bases in widget code.

Evidence:

- docs updated;
- scanner exists or tracked as pending;
- no new public `Kss*` API names.

## Phase 2 - Finish Built-In Catalog

Tasks:

1. [done] Keep built-ins at `material`, `tk`, `vanilla`, `lightfield`.
2. [done] Keep Material as default.
3. [done] Keep Glow out of the top-level registry.
4. [pending] Add Lightfield variant plan/API for glow treatment.
5. [done] Convert built-in packs to final grouped token syntax.
6. [pending] Verify every pack covers every style kind and role.

Commands:

```sh
make kss-parser-test
make style-assets-test
make style-builtins-test
make go-style-builtins-check
go test ./go/kryon
```

Evidence:

- `FindStylePack("glow") == NULL`;
- pack coverage tests pass;
- generated Go built-ins are current.

## Phase 3 - Widget Facts Helpers

Tasks:

1. Audit direct C/Go `StyleControlFacts` calls.
2. Add `.kry` facts helpers per widget.
3. Replace direct C/Go facts construction.
4. Keep facts semantic and class-aware.
5. Add policy tests for helpers.

Priority (remaining call sites from `00-status.md`):

1. [done 2026-09-14] TableView - `src/ui/ui_tk.c`, `go/kryon/runtime.go`
2. [done 2026-09-14] NavigationBar - `src/ui/navigation_bar.c`, Go retained path (zero base)
3. PanedView handle - `src/ui/tab_bar.c:778`
4. Button generic facts path - `src/ui/button.c:33`, `go/kryon/runtime.go:1747`
5. Modal
6. Menu/ListBox/TreeView
7. Toolbar/TitleBar
8. Remaining smaller widgets

Done: toast facts helpers in `runtime/toast.kry`; table view facts helpers in `runtime/table_view.kry`; navigation bar facts helpers in `runtime/navigation_bar.kry`.

Evidence:

- `rg "StyleControlFacts\\(" src/ui go/kryon` has only approved hits;
- widget policy tests assert facts roles/classes/states.

## Phase 4 - Remove Hidden Visual Bases

Tasks:

1. [done 2026-09-14] Audit `StyleData base = {.fields = ...}` in widget paths (six sites remain, listed in `00-status.md`).
2. Replace visual bases with zero base.
3. Move needed visual defaults into `.kss`.
4. Keep structural metric fallbacks in `.kry`.
5. Add no-style regression tests.

Evidence:

- no unapproved visual base in widget code;
- no-style tests for key widgets;
- built-in pack tests still pass.

## Phase 5 - Metrics And Paint Policy

Tasks:

1. Move metric fallback policy to `.kry`.
2. Preserve explicit zero.
3. Move paint geometry helpers to `.kry` where backend-independent.
4. Keep host text measurement and drawing in host code.
5. Add policy tests for metrics and paint structs.

Evidence:

- C/Go widget code calls generated `*MetricsFor` and `*PaintFor`;
- tests cover zero/missing/negative metrics;
- UI remains measurable without a style pack.

## Phase 6 - Parser And Syntax Cleanup

Tasks:

1. Implement final grouped token syntax.
2. Implement imports and layers fully.
3. Implement theme overlays.
4. Add source maps.
5. Delete old token-prefix syntax.
6. Add formatter after syntax stabilizes.

Evidence:

- parser tests cover final syntax;
- shipped packs use final syntax;
- old syntax fails with clear diagnostics;
- source maps can identify winning declarations.

## Phase 7 - Runtime And Backend Parity

Tasks:

1. Ensure C and Go retained runtimes use same generated helpers.
2. Add JS/KRB parity fixtures.
3. Ensure DOM/canvas/libdraw/termi consume resolved frames.
4. Add backend degradation reporting for unsupported material/effects.
5. Add matrix tests.

Evidence:

- runtime matrix passes;
- renderer matrix passes;
- no backend injects style values;
- inspector can report degradation.

## Phase 8 - Tooling And Inspector

Tasks:

1. Add KSS hot reload in preview.
2. Add style pack/theme selector in preview.
3. Add capture boards for widget families.
4. Add inspector rule explanation.
5. Add dead-rule/unmatched-class diagnostics.

Evidence:

- preview reloads KSS without rebuild;
- captures exist for built-in packs;
- inspector shows matched and winning rules.

## Phase 9 - Downstream Migration

Tasks:

1. Migrate Kryon examples to explicit `#style`.
2. Migrate parity fixtures.
3. Update downstream apps by submodule bump only.
4. Move product visuals into app `.kss`.
5. Remove theme compatibility from app source.
6. Add StylePicker where desired.

Evidence:

- downstream `vendor/kryon` trees are clean;
- apps build;
- app source attaches style packs explicitly.

## Phase 10 - Final Legacy Deletion

Tasks:

1. Delete compatibility theme import/export paths.
2. Delete public visual props.
3. Delete stale docs/examples.
4. Delete parser legacy syntax.
5. Delete unneeded C helpers.
6. Turn scanners into required gates.

Evidence:

- scanners pass;
- no-style mode passes;
- `make test` passes;
- public API snapshot is updated intentionally;
- docs describe only the final architecture.

## Standard Commit Loop

For each implementation commit:

```sh
git status --short
rg "<target>" runtime src/ui go/kryon tests styles
edit scoped files
make generate-runtime
make <target>-policy-test
go test ./go/kryon
make style-builtins-test
git diff --check
git add scoped files
git commit -m "<specific message>"
```

Skip broad tests only when the change is docs-only or when a focused target cannot touch runtime behavior.

## Final Acceptance

The project is complete when:

- every app-facing widget has zero hidden product styling;
- every visual value comes from KSS or explicit primitive drawing;
- built-in packs cover the full widget surface;
- Material is default and cheap;
- TK, Vanilla, and Lightfield are selectable;
- Glow exists only as Lightfield treatment/variant;
- examples and downstream apps attach packs explicitly;
- parser/compiler/generator/tooling are stable;
- inspector can explain visible style values;
- legacy theme compatibility is removed;
- CI prevents regressions.
