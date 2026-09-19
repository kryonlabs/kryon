# Downstream Kryon consumers

Reviewed after Kryon `153d7b2e`. This inventory scans git projects under
`/mnt/storage/Projects` for `.gitmodules` entries whose submodule path basename
is `kryon` or whose URL basename is `kryon.git`. It records the current local
checkout state only; it does not imply every app has been migrated or verified.

| Project | Submodule path | URL | Current local HEAD | Submodule status | Parent status for submodule |
|---|---|---|---|---|---|
| `atr` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `95f8170` | clean | clean |
| `dashboard` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `074bd9403` | clean | clean |
| `inbe` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `153d7b2ef` | clean | clean |
| `job` | `kryon` | `https://github.com/kryonlabs/kryon.git` | `b7a72150d` | clean | `M kryon` |
| `kam` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `14c8ddf` | clean | clean |
| `kasaival` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `e583287df` | clean | clean |
| `katalis` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `97790eb6` | clean | clean |
| `krait` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `34f677c9` | clean | clean |
| `neocats` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `b44b067` | clean | `A .gitmodules`; `AM vendor/kryon` |
| `oroot` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `8289a7b` | clean | clean |
| `pass` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `0c864559e` | clean | clean |
| `taiji` | `sys/src/kryon` | `https://github.com/kryonlabs/kryon.git` | `745c2d30` | clean | clean |
| `uku` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `74e2dcda8` | clean | clean |
| `workbook` | `vendor/kryon` | `https://github.com/kryonlabs/kryon.git` | `815aa28` | clean | clean |

## Follow-up ownership

- `inbe` is the only downstream app intentionally moved to current Kryon during
  this plan pass; its vendored tree is clean and tracked by the Inbe pointer
  commits.
- `ktrem` is not a Kryon submodule consumer in this scan, but it is a direct
  downstream gate in `scripts/conformance-matrix.py --verify-downstream`. ktrem
  `5106e42` migrates it to the current Kryon clipboard/theme APIs, and the
  downstream gate passes.
- `job` has a clean Kryon checkout but an uncommitted parent pointer change.
- `neocats` has an in-progress Kryon submodule addition in the parent checkout.
- The remaining direct consumers still need per-app caller, asset, platform, and
  migration evidence before P6 can close.

## Caller, asset, and platform scan

This scan excludes vendored Kryon submodules, `.git`, generated build trees,
Android `.cxx` output, `dist`, and dependency caches. Counts are maintained local
source signals, not proof of successful builds or migration completion.

| Project | Maintained `.kry` files | Maintained `.kss` files | Maintained Kryon API C/C++ files | Platform/build surfaces | Asset/style dirs | Follow-up signal |
|---|---:|---:|---:|---|---|---|
| `atr` | 9 (`src/screens/bot_detail_screen.kry`, `src/screens/devices_screen.kry`, ...) | 0 | 9 (`src/main.c`, `src/widgets/ui_bits.c`, ...) | Android, Gradle, Plan 9 mkfile, Makefile | assets | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `dashboard` | 4 (`dashboard/kry/contacts.kry`, `dashboard/kry/news.kry`, ...) | 0 | 0 | Android, Plan 9 mkfile, Makefile | none detected | audit `.kry` callers; verify Android; verify Plan 9 |
| `inbe` | 88 (`src/main.kry`, `src/breaks/app_breaks.kry`, ...) | 0 | 34 (`tests/app_bottom_nav_test.c`, `tests/storage_import_test.c`, ...) | Android, Gradle, Plan 9 mkfile, Makefile | assets | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `job` | 1 (`app.kry`) | 0 | 0 | Android, Plan 9 mkfile, Makefile | assets, fonts | audit `.kry` callers; verify Android; verify Plan 9; resolve parent status |
| `kam` | 0 | 0 | 11 (`src/kam_terrain.c`, `src/kam_sprites.c`, ...) | Android, Plan 9 mkfile, Makefile | assets | audit C/C++ API callers; verify Android; verify Plan 9 |
| `kasaival` | 37 (`modules/game_crabs.kry`, `modules/game_fuel_pickups.kry`, ...) | 0 | 9 (`src/storage_sync.c`, `src/prelog.c`, ...) | Android, Gradle, Plan 9 mkfile, Makefile | assets | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `katalis` | 0 | 0 | 43 (`client/src/raycast.c`, `client/src/world.c`, ...) | Android, Plan 9 mkfile, Makefile | assets | audit C/C++ API callers; verify Android; verify Plan 9 |
| `krait` | 36 (`samples/live_widgets.kry`, `samples/settings_panel.kry`, ...) | 0 | 59 (`tests/git_test.c`, `tests/daochi_test.c`, ...) | Android, Plan 9 mkfile, Makefile | none detected | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `neocats` | 0 | 0 | 2 (`src/main.c`, `include/neocats.h`) | Android, Plan 9 mkfile, Makefile | assets | audit C/C++ API callers; verify Android; verify Plan 9; resolve parent status |
| `oroot` | 0 | 0 | 2 (`src/main.c`, `src/app_internal.h`) | Android, Plan 9 mkfile, Makefile | assets | audit C/C++ API callers; verify Android; verify Plan 9 |
| `pass` | 4 (`app/settings.kry`, `app/profiles.kry`, ...) | 0 | 4 (`native/pass_plan9_main.c`, `native/pass_runtime_test.c`, ...) | Android, Gradle, Plan 9 mkfile, Makefile | assets | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `taiji` | 107 (`sys/src/pass/app/settings.kry`, `sys/src/pass/app/profiles.kry`, ...) | 0 | 294 (`scripts/kryon-probe.c`, `sys/src/pass/native/pass_plan9_main.c`, ...) | Android, Gradle, Plan 9 mkfile, Makefile | none detected | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `uku` | 2 (`src/dashboard_empty.kry`, `src/app_chrome.kry`) | 0 | 2 (`src/main.c`, `droid/app/src/main/cpp/android_bridge.c`) | Android, Gradle, Plan 9 mkfile, Makefile | assets | audit `.kry` callers; audit C/C++ API callers; verify Android; verify Plan 9 |
| `workbook` | 2 (`workbook.kry`, `src/engine.kry`) | 0 | 0 | Android, Plan 9 mkfile, Makefile | fonts | audit `.kry` callers; verify Android; verify Plan 9 |

No maintained downstream app `.kss` files were found in this scan. That means
style migration evidence should focus first on `.kry` callers, C/C++ API
callers, bundled assets, and platform builds; it does not prove apps have no
style migration work, because app palettes and theme bridges may be expressed in
`.kry`, C, or other host code.
