Repository Reorganization Plan

Objective: restructure `src/` into clear module directories and move internal headers alongside sources while preserving public headers in `include/` and keeping the build working during incremental migration.

Target layout (high level):
- src/core/      : core runtime, `flint*.c`, `runtime_assets.c`, `embedded_assets.c`, `theme_meta.c`, `theme.c`, `locale.c` etc.
- src/ui/        : UI subsystem implementation; subdirs:
  - src/ui/components/ : widgets and controls (buttons, dropdown, rows, toolbar, etc.)
  - src/ui/layout/     : layout algorithms and helpers (ui_layout.c, ui_text_layout.c)
  - src/ui/icons/      : icon assets + `ui_icons.c`, `ui_icon_assets.c`, `ui_icon_names.c`
  - src/ui/internal/   : ui internal headers
- src/platform/  : platform-specific code (`desktop_tray.c`, `platform.c`, `system_theme.c`, `web.c` if platform-specific)
- src/lyra/      : `lyra_account.c`, `lyra_sync.c`, related internals
- src/file_dialog/: `file_dialog.c`, `file_dialog_internal.h`
- src/web/       : `web.c`, web-related code
- src/tools/     : small helpers and tools (if any)

Headers:
- Public headers remain in `include/` (e.g., `include/ui.h`, `include/lyra_account.h`).
- Module-internal headers move into their module directory (e.g., `src/lyra/internal.h`).
- Add short compatibility shims at original paths that include the new header until final cleanup.

Detected duplicate basenames:
- `ui.h` exists in `src/ui/ui.h` and `include/ui.h`. Action: keep `include/ui.h` as public API; move `src/ui/ui.h` to `src/ui/internal.h` and update sources to include relative internal header.

Initial module move mapping (first pass):
1. `lyra` module
   - Move `src/lyra_account.c` -> `src/lyra/lyra_account.c`
   - Move `src/lyra_sync.c` -> `src/lyra/lyra_sync.c`
   - Move internal headers to `src/lyra/` and keep `include/lyra_account.h` as public API.
   - Add `include/lyra_account.h` compatibility shim if path changes.
2. `file_dialog` module
   - Move `src/file_dialog.c` -> `src/file_dialog/file_dialog.c`
   - Move `src/file_dialog_internal.h` -> `src/file_dialog/internal.h`
3. `ui` module (split)
   - Move `src/ui/*.c` into `src/ui/components/` and `src/ui/layout/` as appropriate.
   - Move `src/ui/ui.h` -> `src/ui/internal.h` and ensure `include/ui.h` remains the public header.
4. `platform` module
   - Move `src/desktop_tray.c`, `src/system_theme.c`, `src/platform.c` -> `src/platform/`
5. `core`
   - Gather `flint.*` core files into `src/core/`.

Migration approach:
- For each module:
  1. Create target directory under `src/`.
  2. `git mv` files to preserve history.
  3. Add a compatibility shim at the old path if a public header moved, e.g. original `src/ui/ui.h` -> `src/ui/internal.h` and leave `include/ui.h` untouched.
  4. Run `make` and fix compile errors limited to that module.
  5. Commit the module move in a single feature branch.

Build system changes:
- Update `mk/common.mk` to include `src/*/*.c` (already present) and adjust `APP_INCLUDE` to include module internal dirs as needed during migration.
- Prefer incremental edits: update Makefile module SRCS variables as modules move.

Automation utilities to add:
- `scripts/find_duplicates.sh` (exists)
- `scripts/move_module.sh <module>`: will perform `git mv` and sed-based include path updates (manual review required).

Next immediate actions I will perform:
- Create `scripts/move_module.sh` scaffold (dry-run) for moving `lyra` files.
- Prepare the `lyra` module move steps and perform a dry-run commit (no destructive actions without your approval).

If you want me to start moving files now, I will implement the `scripts/move_module.sh` scaffold and perform the first module move in a feature branch `reorg/lyra` (I will not push). Let me know to proceed, or I will proceed after creating the scaffold.
