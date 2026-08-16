# Changelog
## Unreleased

### Added

- Memory diagnostics: `KryonMemReport(tag)` / `KryonMemDebugEnabled()` print
  RSS/high-water marks and the glibc arena breakdown to stderr when
  `KRYON_MEM_DEBUG` is set; `UIFontMemoryReport(tag)` prints per-font
  rasterization stats under the same switch.

### Changed

- UI source fonts rasterize once per font at the DPI-scaled base size instead
  of caching up to eight per-size rasterizations, and one lazily built large
  tier keeps big text crisp. New codepoints re-rasterize at most once per
  frame per font, so a locale list in native scripts no longer triggers a
  rebuild storm. Freed atlas build buffers are returned to the OS
  (`malloc_trim` on glibc). Idle heap use of font-heavy apps drops
  accordingly.
- Optional GTK dependency for the GTK_STATUS_ICON tray backend: apps can
  define `KRYON_TRAY_GTK_DL` to resolve GTK through the new `gtk_dl` shim
  (`dlopen("libgtk-3.so.0")` in the tray thread) instead of linking GTK.
  Applications that skip the tray then never map libgtk-3 or its
  gdk/pango/cairo dependency chain.
- PTY terminal (`KryTerm`) for IDE hosts: spawn `$SHELL`, write keys, poll
  a screen grid, resize with `TIOCSWINSZ`. Basic CSI cursor and erase.
- First krb cartridge slice: `k2c --emit-krb` packs widget calls into a
  mmapable `.krb` (VFS nodes, string table, `OP_DRAW_TREE`, host import
  names). `KrbLoad` / `KrbBind` / `KrbDraw` walk the image through a small
  `KryBackend` table. `kryon-preview cartridge` renders a `.kry` or `.krb`
  without building an app host. C emit still walks the reconstructed AST;
  existing C projects are unchanged.
- `k2c --emit-krb` also writes a C host (`*.krb.c` / `*.krb.h`): state
  becomes `KrbBindMem` paths, button `if` bodies become bind functions,
  and `Screen_krb_draw` / `Screen_krb_press` are the C ABI. `02_buttons`
  clicks increment a mounted `click_count`.
- Mount live C memory into a cartridge: `KrbMount` / `KrbBindMem` plus
  `OP_CALL_HOST` and `OP_SET_I32`. A TEXT node whose name is a mounted path
  draws the live value. Raylib builds now drop copied vendor `.o` files so
  the backend rename header actually applies.
- Add `defer` to the Kry language: `defer STMT` runs `STMT` at the enclosing
  block's exit (fall-through, `return`, `break`, `continue`), in reverse
  registration order across nested scopes. A compile-time transform that
  splices the statement into the generated C at every exit point — no runtime
  cost. `goto` out of a deferred scope is left to explicit cleanup.
- Resolve qualified type references across modules. Writing
  `alias.Type` after `alias :: #import "mod/path"` now produces the bare C type
  name in every position — parameter lists, return types, struct fields, local
  declarations, and `#global` declarations — instead of emitting `alias.Type`
  verbatim (a C syntax error) or wrongly module-prefixing it. Structs and enums
  use their declared name across translation units, so the alias prefix is
  dropped. Qualified *calls* (`alias.fn(...)`) still rewrite to `module_fn(...)`
  as before. The first multi-module `.kry` program (state + draw + host) now
  compiles, links, and runs end to end.
- Remove the fixed per-function body cap. `k2c` previously rejected any function
  whose generated body exceeded a fixed 512-statement cap with
  "generated body is too large", which blocks large UI draw functions.
  `KryFunction.body`/`body_line` are now heap arrays that grow geometrically
  (via `grow_body`), and the `apply_defers` splice pass allocates its working
  buffers sized to the body. A 1,500-statement function compiles cleanly.
  Verified clean under ASAN/UBSAN/LeakSanitizer; the previously-leaked
  `file->functions`/`routes` arrays are now freed on exit.
- Collect multiple parse errors instead of aborting on the first. Parsing now
  installs a per-statement recovery boundary: a malformed statement records a
  diagnostic and parsing continues with the next, so a file with several errors
  reports all of them (Krait's problems pane becomes useful while editing).
  A file that previously reported one error and exited now reports every
  recoverable error in source order. Fatal conditions (out of memory, CLI
  misuse) still abort as before, and any diagnostics already recorded are
  flushed first. Examples and valid code are unaffected (byte-identical).
- Standalone `.kry` apps can own their frame loop. The `app{}` block now
  accepts `init`, `frame`, and `shutdown` hooks naming `.kry` functions; when
  `frame` is set, `k2c` emits a `main()` that calls `init()` once, `frame()`
  each iteration, and `shutdown()` at exit — without bracketing drawing, so the
  program calls `BeginDrawing`/`BeginUIFrame`/`EndDrawing` itself and can host
  nested render-texture passes and inspection overlays like a hand-written C
  app. This is the entry point Krait uses. Without hooks the
  existing single-screen `app{}`/`screen` dialect is unchanged (byte-identical).
- Add the Kry standard platform library: `kry_process` (spawn a shell command
  with non-blocking stdout/stderr polling, used for builds and consoles),
  `kry_filesystem` (directory iteration, stat/mtime, recursive mkdir, realpath,
  text read/write), and `kry_dylib` (dlopen/dlsym/dlclose for loading a built
  app host). Declared in `kryon.h` and implemented under `src/kry_std/`, so they
  are linked into `libkryon.a` and callable directly from `.kry` via `#extern`.
  These lift Krait's inline POSIX surface into a reusable library so the
  `.kry`-written IDE can spawn `make kryon-host`, walk the project tree, and
  `dlopen` the resulting `app_host.so`. Windows has stub implementations.
- Move the self-hosted IDE rewrite in Kry into the standalone IDE repository,
  Krait (`kryonlabs/krait`): `app.kry` owns the window loop via the new
  `app{}` hooks; `state.kry`, `start_page.kry`, `project.kry`, `tree.kry`, and
  `editor.kry` implement the start page, project-open flow (file dialog +
  `kry_fs`), a file-tree sidebar (`kry_fs_list_dir` +
  `UICascadingTreeViewNode`), and a read-only source viewer (`kry_fs_read_file`
  + `UITextAreaNode`). Krait transpiles those modules in one `k2c` invocation
  and links the generated C against `libkryon.a` + raylib.
- Krait gains an editable editor with multi-tab open files, Ctrl+S save
  (`kry_fs_write_file`), dirty markers, and Ctrl+Z/Ctrl+Y undo/redo
  (heap-snapshot ring), plus a live-preview pane (`preview.kry`) that runs
  `make kryon-host` via `kry_process`, `dlopen`s the resulting `app_host.so`
  via `kry_dylib`, and renders the host into a `RenderTexture` — the full
  non-blocking build + hot-reload loop, written in Kry.
- Add the Krait console and problems panels in Kry (`console.kry`,
  `problems.kry`): an interactive shell that runs typed commands via
  `kry_process` and drains output each frame, and a diagnostics parser that
  splits `path:line:col: message` lines from build output into a problems
  list, shown in a bottom panel that auto-opens when a build reports errors.
- Compile kryon library objects with `-fPIC` so they link into PIE executables
  and `dlopen`'d app hosts on distros that default to PIE.

### Changed

- Remove the bundled C IDE. Kryon now builds only the library, compiler, and
  tooling; the Kry-written IDE, Krait (`kryonlabs/krait`), lives in its own
  repository and vendors Kryon as a submodule.
- Rename the app command `kryon-app` to `kryon`. There is now a single
  `kryon` command for building, packaging, running, previewing, and the local
  Ksync backend. `scripts/kryon-app.sh` → `scripts/kryon.sh`; the Makefile
  installs `$(BINDIR)/kryon`; `kt` execs `kryon` for app runs and `krait` for
  the `ide` target (freeing the bare `kryon` name for the build tool). App
  `project.kryon` target entries that call `sh vendor/kryon/scripts/kryon.sh`
  are updated in downstream apps.
- Add a `value_text_override` parameter to `DrawUISlider`, `UISliderNode`, and
  `Slider`. When non-NULL it replaces the default numeric value label, letting
  callers render custom value text (for example, named steps). This is a
  breaking signature change: all callers must add the new argument (NULL keeps
  the previous behavior).
- Fix copy/cut/paste/select-all in the Krait source editor. `UITextAreaNode`
  had its own generic clipboard handler that raced Krait's
  `editor_handle_source_clipboard` on the same one-shot keypress, so Ctrl+C/V
  worked inconsistently. The generic handler is removed; Krait now owns the
  source-editor clipboard (it has the byte-cap, line-copy fallback, and status
  messages). The stale `!textarea_changed` guard that silently skipped paste is
  dropped too.
- Raise the clipboard size caps so large selections copy and paste in full.
  The kryon UI buffer (`g_clipboard_text`) grew from 4096 to 1 MiB, and the
  vendored raylib SDL `GetClipboardText` buffer (`MAX_CLIPBOARD_BUFFER_LENGTH`,
  a fixed 1024 that truncated pastes to 1019 bytes + `...`) is overridden to the
  same 1 MiB via a `-D` flag passed through `RAY_RAYLIB_CONFIG` — no vendored
  raylib source is edited. Both caps are now symmetric and above the editor's
  512 KiB source buffer, which becomes the real ceiling.
- Make hot-reload (and run targets) non-blocking. Krait ran the app-host build
  as a synchronous `popen`+`fread` on the UI thread, freezing the window for the
  whole compile. Builds now use the same `fork`+`pipe`+`waitpid(WNOHANG)` pattern
  as the console runner (`editor_build_start`/`editor_build_poll`), drained each
  frame from the main loop; the app host is `dlopen`ed on the poll that observes
  a successful exit. The reload poll is lowered from 2.0s to 0.5s so a finished
  build is picked up promptly, and builds are serialized with the console.
- Split the screen-header (title bar) widgets out of `modal.c` into a new
  `src/ui/ui_titlebar.c`, leaving `modal.c` holding only modal/dialog code.
  Removed an orphaned empty scrollbar section marker.
- Consolidate duplicated helpers: the three byte-identical `clampi` copies in
  `guide.c`, `modal.c`, and `theme_picker.c` now use the shared `ui_clampi`.
  Add shared `ui_draw_box_background`, `ui_caret_blink_visible`, and
  `ui_open_url` helpers, replacing inlined copies in the text widgets and links.
- Correct the README preview-projects section: Krait previews `.kry` by
  rebuilding and `dlopen`ing an app host, not by rendering source directly.
- Remove the empty leftover `src/editor/` directory; the C editor lived under
  the command tree before the Krait migration.
- Add CI workflow (`.github/workflows/ci.yml`) that builds and runs `make test`
  on push and pull request for Linux and FreeBSD. Tests previously only ran at
  release time.
- Extract the UTF-8 codec and text-buffer helpers from `ui.c` into a new
  `src/ui/ui_text_edit.c`, with a `tests/ui_text_edit_test.c` covering the
  insert/delete/offset logic.

## v0.1.7 - 2026-07-26

### Changed

- Bump version to v0.1.7
- Remove native_widget_name compiler name-rewrite hack
- Finish removing Kry sugar verbs — language is now minimal
- Remove redundant Kry verbs; widgets are now a library
- Rework Platforms grid: add Windows, macOS, FreeBSD
- Make text input focus exclusive across UITextField and UITextArea

## v0.1.6 - 2026-07-25

### Changed

- Bump version to v0.1.6
- Resolve tray icon from installed hicolor paths
- Fix site card cursors, release CI, and IDE recent-projects grid
- Trigger site deploy (previous run hit GitHub internal error)
- Restyle docs site with refined modern-retro theme
- Add Kryon IDE start page and fix preview inspection
- Improve Kry language and generic APIs
- Support uninitialized C locals in k2c
- Allow args locals in k2c functions
- Add Kryon platform thread primitives
- Support local enum blocks in k2c
- Support bitwise compound assignments in k2c
- Guard Kry compile-time expansion recursion
- Add runtime asset sync API
- Support Kry multidimensional arrays
- Raise Kry type block capacity
- Add Kry named enum syntax
- Add Kry switch syntax
- Update Kryon site goals
- Add Kry web intrinsics
- Add Kry anonymous block scopes
- Add top-level Kry extern declarations
- Add Jai-style Kry defines
- Add Kry top-level macro translation
- Add Kry type aliases
- Allow Kry type-only modules
- Generate project headers from Kry files
- Guard Kry text shorthand parsing
- Respect Kry locals when resolving function values
- Limit Kry function value rewriting
- Resolve Kry function values in expressions
- Add exact C exports for Kry modules
- Make Kry modules the only import path
- Support path-based Kry module use
- Fix IDE preview toolbar and text state
- Tighten leading continuation parsing
- Support leading expression continuations in Kry
- Add native Kry enum blocks
- Support break statements in Kry
- Add native Kry struct declarations
- Support multiline Kry function declarations
- Rewrite nil in Kry static initializers
- Add Jai-style Kry compile-time macros
- Support multiline Kry state initializers
- Use Jai-style typed Kry declarations
- Keep postfix operators on one Kry line
- Support multiline Kry block headers
- Support operator line continuations in Kry
- Support multiline Kry statements
- Add Kry while statements
- Allow app locals in Kry functions
- Lift Kry function limits
- Add flat Kry modules


## v0.1.5 - 2026-07-21

### Added

- Add the Kryon logo as the site and IDE app icon.
- Add C-close Kry language direction docs.
- Add automatic release tagging when the checked-in Kryon version changes.

### Changed

- Replace Kry `include` syntax with explicit `cimport` for C headers.
- Use cleaner changelog version headings without brackets.

### Fixed

- Allocate Kry compiler state on the heap to avoid Linux runner stack crashes.

## v0.1.4 - 2026-07-18

### Changed

- Add Clay as a vendored layout dependency for future Kryon UI work.
- Remove the vendored fontchop dependency and chopped bitmap font assets.

## v0.1.3 - 2026-07-16

### Changed

- Add public Kryon version metadata for release artifacts.
- Move font atlas generation to the vendored fontchop submodule.
- Replace the single implicit UI font with a registered font selection API.
- Improve generated icon asset formatting and desktop/embedded fallbacks.

### Added

- Add Markdown support and text area UI.
- Add pointer release helper APIs and modal layer input capture.

## v0.1.2 - 2026-07-12

### Added

- Add web file dialog loading.

## v0.1.1 - 2026-07-12

### Fixed

- Fix release CI vendor builds.

## v0.1.0 - 2026-07-12

### Added

- Add Kryon release automation.
