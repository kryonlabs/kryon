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


## Approval notes
Project docs ask for per-run user approval for visual captures, benchmarks, and
Bend law proofs. The live-desktop rule (AGENTS.md) is absolute: everything GUI
runs under `xvfb-run`/private display, never the developer's real desktop.
