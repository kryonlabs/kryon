# Kryon C→.kry migration — execution plan

Derived 2026-09-20 from a full read of `plan/` (UI_MIGRATION_REMAINING,
NATIVE_LANGUAGE_COMPLETION, COMPLETION, CODEBASE_REDUCTION, DOWNSTREAM_CONSUMERS,
button/TREE_MIGRATION, button/REVIEW) plus the current Kryon and Inbe working
trees. This is an ordering of the *remaining* work; it does not re-open
completed batches (arrays, text/IME, image, host organization).

## Ground truth (current trees)

- Kryon: large **uncommitted** migration WIP — 36 modified files, 9 deleted
  `src/core/*.c` (`app_host/app_runtime/app_shell/device_preferences/`
  `kry_capabilities/kryon_frame/kryon_frame_pacing/kryon_node/theme.c`), and
  ~20 new `src/ui/*.kry` owners. None of it has been regenerated, compiled, or
  run as a set.
- Remaining handwritten C that must stay (narrow host services, after audit):
  `src/ui/{ui_image_cache,ui_paint,ui_surface_cache,ui_text,ui_text_backend,ui_window}.c`
  and `src/core/{app_storage,automation,embedded_assets,kryon_abi,kryon_mem,locale}.c`.
- Handwritten public UI headers still live **and are included**:
  `ui_style_sheet.h` (20), `theme.h` (12), `ui_tree.h` (9), `ui_window.h`,
  `ui_text.h`, `ui_core.h`, `app_runtime.h`, `locale.h`, `ui_text_layout.h`,
  `ui_draw.h`, `ui_layout.h`, `ui_transition.h`, `ui_nav.h`, `app_shell.h`,
  `kryon_frame.h`, `ui_page.h`, etc.
- Inbe: `vendor/kryon` is at kryon master HEAD `3516ff86` (clean tree, parent
  pointer uncommitted), and its own app migration is uncommitted.

## Inbe "upstream migration stuff" — answer

Only **one** kryon-owned artifact lives in Inbe's non-vendor tree:
`src/build/kryon/preview_shim.c` — a gitignored, untracked, unreferenced shim
implementing the old kryon app-host API (`CreateApp/ApplyRoute/BeginScreenDraw`).
It is dead build output from a previous kryon preview. Delete it (it is not
source and is not in git). No old kryon symbols (`ApplyCurrentTheme`,
`ThemeCatalog`, `app_shell`, `kryon_frame`, …) are referenced anywhere in
Inbe's tracked `src/`/`scripts/`/`Makefile`/`mkfile`. Inbe's own
`app_style.kry` + `app_style_tokens.c` deletion is its **app-specific** palette
bridge, which is legitimately Inbe's to migrate to KSS overlays once upstream
KSS/theme work lands (see phase F).

## Phases (dependency order)

### A. Regenerate and build the current WIP (gate for everything)
- `make generate-native-runtime` then `make generate-runtime`; run
  `k2c/k2cpp/k2go-syntax-test`, `generated-runtime-parity-test`,
  `go-runtime-test`, `generated-provenance-check`. Fix codegen/compile breaks
  in the uncommitted WIP before doing more migration.

### B. Reconcile remaining `src/ui/*.c` and `src/core/*.c`
- Audit each of the 12 retained C files against the ownership table in
  NATIVE_LANGUAGE_COMPLETION.md; move any leftover UI policy to `.kry`, keep
  only renderer/platform/font/cache/storage effects. Key open questions:
  `ui_image_cache.c` clip conversion, `ui_text.c` font-size fallback,
  `ui_window.c` focus-restore + click/drag arbitration.

### C. Remove handwritten UI headers → generated ABI
- For each header still included (list above): provide the `.kry` type/function
  definition, generate the public ABI, re-point consumers, then delete the
  handwritten header. `ui_style_sheet.h`/theme.h first (they block KSS).

### D. Finish KSS/theme ownership (Milestone 5)
- Migrate live `ApplyCurrentTheme`/palette-getter callers (Kryon init, then
  Uku 219 hits, Krait 134, Rill 69 — confirmed downstream size). Delete obsolete
  theme catalogs/getters and scanner allowances only after every caller moves.

### E. Migrate remaining C tests
- `tests/control_appearance_perf_test.c`, `tests/locale_test.c`, and other
  UI-adjacent C fixtures → `.kry`; keep narrow platform fixture code.

### F. Downstream (Inbe first, then others)
- Delete Inbe `src/build/kryon/preview_shim.c`.
- Migrate Inbe `app_style.kry` palette bridge to KSS overlays (after D).
- Commit Kryon on master; move clean Inbe vendor pointer; commit Inbe app
  changes on master. Then Uku/Krait/Rill theme migration.

### G. Remove legacy completely
- Delete obsolete catalogs/getters, compatibility shims, scanner allowances,
  dead generated artifacts. Refresh API/architecture/feature-matrix docs. Run
  the full gate list in COMPLETION.md.

## Verification (each phase, before committing)
`make test`, `make laws-test`, k2c/k2cpp/k2go-syntax-test,
`generated-runtime-parity-test`, `go-runtime-test`,
`python3 scripts/check-clean-text-api.py`, `sh tests/public_api_names_test.sh`,
`sh tests/canonical_surface_test.sh`, style gates, `git diff --check`.
GUI/input/benchmark runs under private Xvfb only (scrub DISPLAY/WAYLAND_DISPLAY).
Long builds detached via `setsid`.


## Round 2–3 additions (verified state + header-removal analysis)

### Verified green (all runs this session)
generated-runtime-parity-test, k2c/k2cpp/k2go-syntax-test, go-runtime-test,
runtime-parity-check, public-api-names-check, canonical-surface-test,
kryon-boundary-check, public-api-snapshot-check, laws-test, fast-test,
smart-test, text-policy-test, image-policy-test, locale-test,
locale-policy-kry-test. Both repos pass `git diff --check`.

### Remaining-C reconcile — closed
The 6 surviving `src/ui/*.c` and 6 `src/core/*.c` are narrow host services
(font-tier/atlas, camera/clip transform, native window effects, storage,
automation transport, assets, ABI/memory, locale lifetime). No UI policy left
to extract. Focus-restore and click-vs-drag swallow in `ui_window.c` are
native-window effects, not widget policy.

### Header-removal architecture (the remaining big slice)
Handwritten headers declare TYPES that `.kry` implementations `#import`.
To remove a header you must move its types into a `runtime/*_props.kry`
(which generates `include/ui_*.generated.h`, tracked), then re-point the
`.kry` + `kryon.h` + consumers.

Two blockers define the ceiling:

1. **Function-pointer typedefs are not expressible in `.kry`.** E.g.
   `KryonPostFrameCallback`, `TextInputFilter`, `AccessibilitySink`.
   These must stay in a (minimal) handwritten header or gain a language
   feature. So `kryon_frame.h`, `ui_controls.h`, `ui_tree.h` cannot be
   fully deleted yet.
2. **Circular includes.** Generated `src/ui/*.h` headers `#include "kryon.h"`
   because their `.kry` sources `#import "kryon.h"`; making them the public
   header inside `kryon.h` would be circular.

### Fully-expressible first targets (no function pointers)
- `kryon_node.h` — `KryonNodeKind`/`KryonNodeFlags` enums, `KryonNode` +
  `KryonNodeEdit` structs, and the `KRYON_NODE_*_MAX` constants are all
  expressible. Functions already live in `src/ui/node.kry`. Consumers:
  `node.kry`, `kryon.h`, `kryon_edit_host.h`.
- `locale.h` — `LocaleEntry`/`LocaleLanguage` structs expressible; the
  catalog/language functions are the public host API (loading/lifetime stays C).
- `app_shell.h`, `app_runtime.h` — structs/enums expressible; functions
  now in `app_shell_layout/route.kry` + `screen_routes.kry`.

### KSS/theme — downstream-gated
`ApplyCurrentTheme` / palette-getter callers are in apps (Uku 219, Krait 134,
Rill 69), not Kryon. Upstream deletion of obsolete catalogs/getters follows
those apps' migrations. Blocked upstream until then.

### Recommended commit order
1. Land the verified slice first as a checkpoint (functional migration +
   dedup + inbe cleanup), or
2. Push the first fully-expressible header (`kryon_node.h`) → generated
   `ui_node_props.generated.h`, then continue header-by-header.



### Round 4 finding — header-removal ceiling is architectural, not cleanup

Attempted the `kryon_node.h` → `runtime/node_props.kry` migration as
proof-of-concept and hit a hard constraint, then verified it on
`kry_capabilities.h`:

- `runtime/*.kry` is generated for **both C and Go** (`generate-native-runtime`
  runs k2c and k2go). A C-only type such as `PropertyValue` (contains a
  `union`) fails k2go with `unsupported or unresolved Go type`.
- Every `.c → .kry` implementation in this migration landed in `src/ui/*.kry`
  (C-only, k2c only). So the functions those handwritten headers declare
  (`KryCapabilitiesHas`, `BeginFrame`, `KryonNodeInit`, …) have **no Go
  implementation**, and cannot be forward-declared `#extern #export` from a
  `runtime/*_props.kry` without breaking the Go target.

Therefore the full "delete the handwritten UI headers" step is blocked on
toolchain gaps, not on ordinary cleanup:

1. `.kry` has no function-pointer typedef (`KryonPostFrameCallback`,
   `TextInputFilter`, `KrySystemDarkModeFn`, …).
2. `runtime/*_props.kry` requires Go-compatibility, excluding unions and
   C-only functions.
3. There is no C-only public-header generation path (`src/ui/*.kry` headers
   are build artifacts in `build/`, not `include/`).

Achievable near-term subset: move Go-compatible, cross-target **types** (pure
enums/structs with no union and no function pointer, and whose functions have
Go equivalents) into `runtime/*_props.kry`.

**Done:**
- (round 4) `kry_capabilities.h` types (`KryCapability`, `KrySafeArea`,
  `KryViewportSpec`) → `runtime/capability_props.kry` →
  `include/ui_capability_props.generated.h`.
- (round 5) `kryon_node.h` types (`KryonNodeKind`, `KryonNodeFlags`,
  `KryonNode`, and the `KRYON_NODE_*_MAX` constants) →
  `runtime/node_props.kry` → `include/ui_node_props.generated.h`; the header
  keeps only `KryonNodeEdit` (PropertyValue union) + the two C-only function
  declarations. Build + fast-test + parity all green.

Remaining handwritten headers and their concrete blockers:
- `locale.h` — `LocaleEntry` uses mutable `char*` (no Go mapping; only
  `const char*` maps to `string`).
- `app_shell.h` — `KryRouteAllowedFn` function pointer.
- `device_preferences.h` — three function pointers.
- `kryon_frame.h` — `KryonPostFrameCallback` function pointer.
- `theme.h` — includes `kryon.h` (circular) and depends on `ThemeMetrics` from
  `ui_controls.h`; ~60 C-only functions.
- `app_runtime.h` — `void*` + `AppHost` dependency. The remaining handwritten
headers are the documented "true host-facing declarations" per
`UI_MIGRATION_REMAINING.md`. Full removal needs a language feature
(function-pointer typedefs) and/or a C-only public-header emitter, which is a
separate toolchain milestone beyond this migration.



## Round 6 — committed + downstream pointer bumped

The migration is committed on Kryon `master` as `0a655400`
("Migrate retained UI to .kry and fix the native C boundary"; 77 files).
Kryon's working tree is clean.

Inbe `vendor/kryon` was moved to that exact commit (parent shows
` M vendor/kryon`, uncommitted). It is NOT pushed anywhere.

### Downstream build-integration gap (needs an inbe Makefile change)

Inbe's `make native` now fails compiling Kryon's own sources:

```
vendor/kryon/src/ui/ui_internal.h:23: fatal error: ui/grapheme.h
vendor/kryon/src/ui/ui_text.c:1:      fatal error: ui/text_rows.h
vendor/kryon/src/ui/ui_window.c:3:    fatal error: ui/window_policy.h
```

Cause: the migration replaced the handwritten
`src/ui/ui_grapheme.h` (still present at `405fc5c`) with the generated
`ui/grapheme.h` from `src/ui/grapheme.kry`, and likewise
`text_rows`/`window_policy`. Kryon's own build generates `src/ui/*.kry`
into `build/.../generated/src/ui/`; Inbe generates only
`$(KRYON_DIR)/runtime/*.kry` (`KRYON_RUNTIME_KRY`), so the generated
`ui/*.h` are absent.

Fix shape (app repository, not Kryon): generate Kryon's `src/ui/*.kry`
with `--root $(KRYON_DIR)/src` into `$(KRYON_GENERATED_SRC_DIR)` (so
`ui/*.h` resolves on the existing `-I`), and add the generated
`src/ui/*.c` (the migrated implementations) to `KRYON_SRCS`; the
handwritten `find src -name '*.c'` only covers host services now.

This is a build-contract change shared by every downstream app, so it
likely deserves a documented Kryon snippet or a helper fragment rather
than a one-off Inbe edit.



## Round 7 — inbe native build restored (end-to-end)

Inbe's `make native` now produces a 7.8 MB ELF binary against the migrated
Kryon. Fixes landed:

- **Inbe build integration** (`inbe/Makefile`): generate Kryon's `src/ui/*.kry`
  with `--root <kryon>/src` into `build/obj/kryon/generated/src` (so the
  generated `ui/*.h` resolve), add the generated `src/ui/*.c` to
  `KRYON_SRCS`, and add the `src/ui`, `src/platform`, `src/backend`,
  generated `runtime`, and `vendor/utf8proc` include paths.
- **Kryon k2c type mapping** (`dcaa99eb`): `const u8*` (and trailing-star
  scalars generally) now emit `const uint8_t*`.
- **Kryon k2c cast mapping** (`441a3ffb`): `(const u8*)` / `(i32)` paren casts
  rewrite the scalar; `nil` and general parenthesized expressions are
  untouched.
- **Inbe app migration** (`src/app/application.kry`): the Button image moved
  from the removed `image_asset_path`/`image_bounds`/`image_fit` fields to
  `image = (ImageProps){...}`.

Kryon is clean at `441a3ffb`; Inbe `vendor/kryon` points there (uncommitted).
Inbe's own migration edits remain uncommitted in its working tree.


## Round 8 — header-by-header props migrations + Inbe committed

Eight further Go-compatible type sets moved out of handwritten headers into
`runtime/*_props.kry` (each generating `include/ui_*_props.generated.h`), with
the handwritten header reduced to that include plus its C-only declarations:

- `b02e11f9` `TransitionState` → `transition_props.kry`
- `00651e19` inspector records → `inspect_props.kry`
- `b5c2103d` profile icon enum → `profile_icon_props.kry`
- `b8b825a1` native window flags → `window_props.kry`
- `1e43fb3a` DPI state record → `dpi_props.kry`
- `611dfda4` style color token → `style_token_props.kry`
- `c552478d` node registry record (`NodeType`) → `node_registry_props.kry`
- `8f1a8608` icon sheet enum → `icon_sheet_props.kry`

Every commit passed `generated-runtime-parity-test fast-test` and refreshed the
public-API snapshot. Kryon `master` is clean at `8f1a8608`.

### Inbe build and commit

- `vendor/kryon` moved to `8f1a8608` and Inbe's own migration was committed on
  `master` as `64c1b6b`. The committed gitlink is exactly
  `8f1a860853f2c147a8f7dcfb81aa6942411e5dd4`; `git status` is clean and
  `make no-vendor-edits` passes.
- `make native` was initially red on `build/kryon/generated/src/app/screenshots.c`
  for an implicit `import_sync_key_path`. The generated tree was stale: the
  source had already been re-pointed to the exported
  `settings_data_import_sync_key_path`, but the `.fresh` stamp was newer than
  the edited source, so `make` skipped regeneration. `rm -f
  build/kryon/generated/.fresh` forces the `k2c --root .` pass and the build
  goes green. Treat `.fresh` regeneration as mandatory after app `.kry` edits.
- Inbe gates all pass at the committed revision: `make native`,
  `no-vendor-edits`, `clean-text-api-check`, `button-api-check`,
  `version-test`, `proof-test`, `sync-recovery-test`.
- The only Kryon-owned artifact in Inbe's non-vendor tree was the gitignored
  `src/build/kryon/preview_shim.c`; it is confirmed absent, so Inbe carries no
  Kryon-owned migration artifact.

### Still blocked upstream

- Handwritten UI headers cannot be fully deleted without a `.kry`
  function-pointer typedef (`KryonPostFrameCallback`, `TextInputFilter`,
  `KryRouteAllowedFn`, `AccessibilitySink`), a C-only public-header emitter
  for `src/ui/*.kry`, or Go-mappable replacements for unions / mutable
  `char*` / Go-host-divergent names (`PropertyValue`, `LocaleEntry`,
  `ThemeId`/`ThemeSky`, `IconType`).
- KSS/theme bridge B-001 (`ApplyCurrentTheme` + palette getters) stays until the
  live Uku (`SetThemeStyle`/`GetThemeScopeName`), Krait, and Rill callers move
  to `SetStyleTheme`/`ResolveActiveStyle`, and Inbe's `app_style.kry` palette
  bridge becomes KSS overlays. Uku/Krait are pinned to pre-KSS Kryon and are not
  broken until their pointers move.

## Round 14 — header audit and the function-migration barrier

Re-audited every remaining handwritten public header against the current tree:

- The only fully orphaned header was `include/ui_menu_types.h` (a one-line
  re-export of `ui_menu_props.generated.h` that `kryon.h` already includes
  directly). No maintained source, test, example, or downstream app referenced
  it. Deleted.
- Every other remaining type is genuinely non-expressible: `FrameState` and
  `Event` contain anonymous nested structs and unions; `TextEdit`,
  `SliderMarkCallback`, `TextInputFilter`, `TextInputPlatformCallback`,
  `KryonPostFrameCallback`, `AccessibilitySink`, and `KryRouteAllowedFn`
  are function-pointer types; `IconAsset` depends on the script-generated
  `IconType`; `theme.h` pulls `kryon.h` and `ThemeMetrics`.
- No dead public `theme.h`/`theme_meta.h` getter is left: every declared
  symbol still has a maintained caller (the ledger cleanup already removed the
  unused ones), so there is no residual dead-API batch to delete.

## Round 15 — color functions move to the shared runtime, and a k2c stack-overflow fix

The round-14 conclusion was too pessimistic: moving Go-compatible *functions*
into `runtime/*_props.kry` was blocked by a compiler bug, not by the backend
contract.

- `runtime/drawing_props.kry` now owns `LightenColor`/`DarkenColor` and their
  private HSL helpers, so `include/ui_color.h` and `src/ui/ui_color.kry` are
  deleted. `kryon.h`, `src/ui/ui_internal.h`, `src/ui/button.kry`, and
  `system_theme.c` consume `ui_drawing_props.generated.h`. The C symbols keep
  their names (`LightenColor`/`DarkenColor`); Go exposes
  `DrawingProps_LightenColor`/`DrawingProps_DarkenColor`, so the handwritten
  `web/kryon-runtime.js` and `go/kryon/style_runtime.go` copies stay
  independent and the per-backend contract is not disturbed.
- Adding those functions exposed a latent k2c/k2cpp/k2go stack overflow:
  `runtime_program()` assigned its cache slot only after `kir_parse_source()`
  returned, so a type probe during parsing (a parenthesized expression whose
  first token is an identifier, e.g. `(q - p)`, which the expression parser
  probes as a cast type) re-entered and parsed the same embedded source forever.
  `cmd/kir/kir.c` now tracks the in-progress source and returns NULL until its
  parse completes.

Gates: `generated-runtime-parity-test`, `fast-test`,
k2c/k2cpp/k2go-syntax-test, `go-runtime-test`, `examples-syntax-test`, and the
refreshed public-API snapshot.

## Round 16 — shim-header sweep

Removed the last pure re-export headers, which were duplicates of tracked props
headers rather than host declarations:

- `include/ui_draw.h` and `include/ui_modal.h` (re-exports only) are deleted;
  `kryon.h`, `ui_tree.h`, `ui_internal.h`, and `style_picker.kry` include the
  props headers directly.
- `include/ui_image.h` is deleted; its includers and `runtime/node2d_props.kry`
  use `ui_image_props.generated.h`, and `KRY_IMAGE_CACHE_MAX` moved into the
  image host's `ui_image_internal.h`.

After the sweep, an orphan scan over Kryon plus Uku/Krait/Rill/Inbe/Atr/Kapsule
finds no unreferenced UI header, and a duplicate scan finds no handwritten
declaration that also appears in a tracked props header. The remaining
handwritten UI headers are host-facing declarations whose functions read global
input/style/DPI state (`GetPaneDropZone`, `ui_tab_bar_height`,
`UpdateReorderList`, `UpdateSwipe`, `Toast`, …) or hold the blocked types, so
they are the documented ceiling rather than cleanup targets.

## Round 18 — the "#extern #export" pattern removes the host-header ceiling

The round-14 and round-16 conclusions were both wrong about function-only
headers: `runtime/*_props.kry` already supports C-only host functions through
`#extern #export` declarations (`canvas_props` and `menu_props` have used it
all along), and k2go skips them, so no Go body is needed. The stateful host
implementations stay in `src/ui/*.kry`; only the *declarations* move into the
tracked props headers.

Deleted with this pattern (`kryon.h` and internal consumers now include the
props headers directly):

- `include/ui_reorder.h` → `UpdateReorderList` in `reorder_props`
- `include/ui_swipe.h` → `UpdateSwipe`/`ResetSwipe` in `swipe_props`
- `include/ui_toast.h` → `Toast` in `toast_props`
- `include/ui_transition.h` → the five transition-state calls in
  `transition_props`
- `include/ui_nav.h` → `GetPaneDropZone` in `paned_view_props` and
  `GetTabBarHeight`/`TabBarHeight` in `tab_bar_props`
- `include/ui_scroll.h` → the three scrollbar width calls in `scroll_props`

Session total: twelve handwritten UI headers removed. Gates after every step:
`generated-runtime-parity-test`, `fast-test`, the affected policy tests, and
the refreshed public-API snapshot. Inbe builds green at `cf152103` and its
pointer is committed; `app_types.kry` imports `ui_swipe_props.generated.h`.

What still blocks the remaining headers, now precisely:

- function-pointer typedefs: `ui_core.h`, `ui_controls.h`, `kryon_frame.h`,
  `ui_tree.h`, `app_shell.h`
- unions / anonymous nested structs: `Event`, `FrameState`
- script-generated `IconType`: `ui_icons.h`, `ui_profile.h`
- `Camera2D` has no props home: `ui_inspect.h` (partial)
- `const StyleSheet*` in `StylePack`: `ui_style_sheet.h`
- `FlowProps` alias and `NodeId`: `ui_page.h`
- `extern DPIState dpi_state` plus inline getters: `ui_dpi.h`
- opaque `NativeWindow`: `ui_window.h`

## Round 20 — node-registry and inspector hosts move; k2c's hardcoded include fixed

Two more headers fell to the `#extern #export` pattern:

- `include/ui_node_registry.h` → the ten NodeType queries in
  `node_registry_props`, including `const NodeType*` returns and
  `NodeTypeSnippet`'s `char*` output buffer (both accepted by k2go as
  signatures).
- `include/ui_inspect.h` → the eighteen inspector calls in `inspect_props`.
  `Camera2D` needed no new home: `kryon_compat.generated.h` already defines it
  and `inspect_props` imports that header. This also fixed a hidden coupling:
  k2c and k2cpp hard-coded `#include "ui_inspect.h"` into every instrumented
  module — the compilers now emit `ui_inspect_props.generated.h`, the syntax
  tests assert the new include, and the round-13 mystery of inbe's generated
  `screenshots.c` carrying that include is explained.

Session total: fourteen handwritten UI headers removed. `widget-instance-test`
fails identically before these changes (kry_math3d operator conflicts) and
stays outside the standard gates. Gates: `generated-runtime-parity-test`,
`fast-test`, k2c/k2cpp/k2go-syntax-test, `go-runtime-test`,
`examples-syntax-test`, refreshed snapshots. Inbe builds green at `e2629fae`
and its pointer is committed (`68622fb`); the concurrent session's 103-file
in-flight inbe migration was left untouched.

## Round 21 — the page host moves; FlowProps/NodeId handled

`include/ui_page.h` is deleted: the nine page_metadata helpers (implemented in
`src/platform/page_metadata.c`) and the six composition calls (implemented in
`src/ui/page.kry`) are now `#extern #export` declarations in
`runtime/page_props.kry`. Two type questions resolved:

- `Page`/`Section`/`Flow` return `NodeId`; the props declare `-> i32`, which is
  ABI-identical since `NodeId` is `typedef int`.
- `FlowProps` is `typedef ColumnProps`; the props declare `Flow(ColumnProps)`
  (compatible), and the alias itself moved into `ui_tree.h` next to the existing
  `RowProps` alias so `.kry` and test code keep the name.

One convention learned: props modules import sibling props modules through
their tracked header names (`#import "ui_layout_props.generated.h"`), not the
module name — a module-name import emits `layout_props.h`, which does not exist
as a tracked file.

Session total: fifteen handwritten UI headers removed. Gates:
`generated-runtime-parity-test`, `fast-test`, k2c/k2cpp/k2go-syntax-test,
`go-runtime-test`, `examples-syntax-test`, refreshed snapshots. Inbe builds
green at `4fa8986e`, pointer committed (`197fc01`); the concurrent session's
in-flight inbe migration stays untouched.

## Round 22 — scaling, clip, and layout hosts move

A re-read of the remaining headers sharpened the rule: the extern pattern does
not care whether an implementation is stateful (`GetPaneDropZone` reads live
style state and moved in round 18), so the only real blockers are *types*,
globals, and inline functions — not state. Three more function-only headers
fell:

- `include/ui_scaling.h` → `SetScale`/`GetScale`/`Scale`/`ClampPx` in
  `runtime/dpi_props.kry` (thematically the DPI home).
- `include/ui_clip.h` → the five clip calls in `runtime/drawing_props.kry`.
- `include/ui_layout.h` → the seven view/flex calls in
  `runtime/layout_props.kry`, with `GetCenteredColumn`'s int out-params as
  `*i32`.

Session total: eighteen handwritten UI headers removed. Gates:
`generated-runtime-parity-test`, `fast-test`, k2c/k2cpp/k2go-syntax-test,
`go-runtime-test`, `examples-syntax-test`, refreshed snapshots. Inbe builds
green at `2fcb30c7`, pointer committed (`c238bc5`).

Remaining, all genuine type/global/inline blockers: `IconType` (`ui_icons`,
`ui_profile`), unions (`Event`, `FrameState`), function-pointer typedefs
(`ui_core`, `ui_controls`, `kryon_frame`, `ui_tree`, `app_shell`),
`dpi_state` global + inline getters (`ui_dpi`), opaque `NativeWindow`
(`ui_window`), `const StyleSheet*` (`ui_style_sheet`), `Font` and the
`Text8..` macros (`ui_text`), `void*` + `size_t` (`ui_instance`), the
`SYNC_PROFILE_ICON_*` enum (`ui_profile`), `LocaleEntry` (`locale.h`).
B-001 (KSS/theme) still waits on Uku/Krait/Rill and Inbe.

## Round 23 — profile-header and text hosts move; IconType demoted from blocker

Two more deletions, both clearing "blockers" that turned out to be softer than
recorded:

- `include/ui_profile.h` → the five profile-icon queries in
  `runtime/profile_icon_props.kry`. `IconType` arrives via
  `#import "ui_icon_types.h"`: an opaque C type is fine in extern signatures
  because k2go skips externs entirely — the IconType blocker only ever applied
  to stored struct fields (`IconAsset`).
- `include/ui_text.h` → the ten `Text*` size macros (constants emit as the
  same `#define`s) and the twenty-six font calls in `runtime/text_props.kry`,
  with `Font` supplied by `kryon_compat`. The handwritten `Text*` constants
  in `go/kryon/runtime.go` gave way to the generated ones (`table_host.go`
  now types its fallback explicitly), and `runtime/button.kry` follows the
  props-header import.

Session total: twenty handwritten UI headers removed. Gates:
`generated-runtime-parity-test`, `fast-test`, k2c/k2cpp/k2go-syntax-test,
`go-runtime-test`, `examples-syntax-test`, refreshed snapshots. Inbe builds
green at `254dabbd`, pointer committed (`7337178`).

Remaining: unions (`Event`, `FrameState`), function-pointer typedefs
(`ui_core`, `ui_controls`, `kryon_frame`, `ui_tree`, `app_shell`),
`dpi_state` global + inline getters (`ui_dpi`), opaque `NativeWindow`
(`ui_window`), `const StyleSheet*` (`ui_style_sheet`), `IconAsset`'s stored
`IconType` field + the `ui_icon_names[]` extern (`ui_icons`), `void*` +
`size_t` (`ui_instance`), `LocaleEntry` (`locale.h`). B-001 (KSS/theme) still
waits on Uku/Krait/Rill and Inbe.

Note: the main checkout remained detached at `120390fb` (concurrent session);
commits again landed through a temporary linked worktree on `master`, removed
afterwards so `master` is free to check out.

## Approval notes
Project docs ask for per-run user approval for visual captures, benchmarks, and
Bend law proofs. The live-desktop rule (AGENTS.md) is absolute: everything GUI
runs under `xvfb-run`/private display, never the developer's real desktop.
