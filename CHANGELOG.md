# Changelog
## v0.1.8 - 2026-08-17

### Changed

- Release: bump version to v0.1.8
- Ci: install Go on FreeBSD
- Build: link FreeBSD curl codec dependencies
- Go: add reusable grid cell editor
- Build: search FreeBSD ports libraries
- Test: build capability cartridge fixture
- Ci: install GTK for Go binding smoke tests
- Build: apply GIO notification include flags
- Ui: finalize deferred overlays at frame end
- Ci: install and link GIO notifications
- Release: automate versions and publish all tools
- Go: support arm64 native linking
- Ui: persist line selection and expose pictures
- Go: expose theme style selection
- Ui: add scroll-safe multiline text selection
- Ui: use neutral colors for tables
- Go: add secure password fields
- Go: expose text area focus state
- Go: expose tab key compatibility constant
- Go: complete raylib drawing compatibility helpers
- Go: expose toolkit collection and dialog widgets
- Go: expose window focus activation
- Go: pin text editor state across cgo calls
- Go: add native runtime and stateful controls
- Go: own the kryui runtime bindings
- K2ir: strip block comments while reading lines
- Krb caps: reference host backends — storage, audio
- Krb: plan-08 capability registry
- KrbAutoMount: state initializers render without a host
- Examples: profile and habit-edit screens — 11 cartridge screens
- Kir_parse: skip plain # comments in TOP mode
- Examples: statistics screen cartridge
- Native exactness gate: PASSES for the language screen
- Dropdown: measured native shape — rounded fill+border r3, diamond indicator
- Language screen: truncating advances + native dropdown shape — structural 1530 -> 241
- Native exactness comparator: AA-only vs structural classification
- Atlas: sub-pixel advances; language cartridge from traced values
- Atlas: raylib's ascent-shifted glyph offsets + per-mille UI scale
- K2b atlas: fix GetCodepointBitmapBox extents; tier collection and UI-scale plumbing
- Kry_sw: ui_text.c advance rounding; krb-sdl: deterministic single-frame dump
- K2b atlas: raylib-identical glyph rasterization
- Language cartridge: matched to the native reference frame
- Screenshots: working native capture — pre-swap read + self-written PNG
- Tests: arm the pre-swap capture in the native harness
- Screenshots: ES2-safe capture — armed EndDrawing reads pre-swap
- Tests: harden the xwd-path kill (TERM, then KILL)
- LoadImageFromScreen: front-end override reading the front buffer
- Tests: xwd-based native capture for software GL
- K2c: initializer items take no inspect wrapper
- K2c: multi-line array initializers
- Tests: native exactness runs the app from the repo root
- K2g: .kry -> Go backend on the shared Kir frontend
- Krb v2: animated geometry — WHM session screen runs live in-cartridge
- Examples: practice config screen cartridge (sliders, spinboxes)
- Examples: habits screen cartridge (bound streak counters)
- Krb v2: screen navigation — the whole app in one cartridge
- Examples: manual screen cartridge (scrolling guide + breath circle)
- Ci: exactness job — cross-engine byte-identity + krb unit tests
- Examples: settings screen cartridge (scroll + dropdowns + checkboxes)
- TakeScreenshot: use absolute paths as-is
- Tests: native-comparison exactness harness (stage 2)
- Tests: cross-engine screenshot-exactness harness
- K2b: fix guard single-term parsing, positional ButtonProps, dropdown height
- K2b: arithmetic expressions in guard conditions
- Krb v2: dropdown controls
- Krb v2: text input — TEXTINPUT node + typed-codepoint vtable path
- Krb v2: scroll containers
- K2b+engines: KFA1 glyph atlas — antialiased Noto text in cartridges
- K2b: else branches and state-vs-state guard comparisons
- K2b: decode png/jpg/bmp/webp and embed as cartridge pixels
- Style tokens: fixed-pixel corner radii, not size fractions
- Renderers: web player opens any .krb + CI builds per-platform downloads
- K2b: embed referenced assets into the cartridge
- Krb v2: embedded asset section + texture_rgba blit
- Material style: subtle control rounding instead of full pills
- K2b: compile .kry state updates and if-guards into v2 VM programs
- Scene_inspect: set SO_REUSEADDR so quick restarts rebind
- Kry_json: expose kry_json_type
- Docs: KRB_FORMAT.md rewritten as an exact byte-level specification
- Scene: live JSON inspection server
- Krb v2: in-cartridge logic VM + circle/ring geometry nodes
- Scene/property/signal/transform: drop the Kry/Kryon prefix from the public API
- Scene: recycle removed node slots through a freelist
- Test: host default style follows the MATERIAL default
- Scene: application-defined node kinds with property callbacks
- Krb-sdl: SDL2 desktop host for KRB cartridges
- K2c: fix one-line control blocks, char literals, and locals named c
- Krb-web: native web engine — kry_sw compiled to wasm, blitted to ImageData
- System_theme web: snprintf not app copy_text
- System_theme: web prefers-color-scheme detection; default source SYSTEM on web
- Krb: kry_sw software rasterizer engine + headless conformance harness
- Kir: 'c:' is a typed decl, not the raw-C 'c' keyword
- Desktop_tray: drop GTK type-cast macros - they expand to gtk_*_get_type() calls the dlopen shim does not forward
- Ksync: harden crypto, exports, JSON, delete, and sync transport
- K2c: fix braced and bare-default switch cases
- Kryon-app.sh: project.kryon target runner with default_target support
- Ui_text: per-size raster tier cache (4 tiers) instead of single base raster
- Docs: plan 11 — renderer engines for KRB cartridges
- K2c: join multi-line initializers on '::' global declarations
- Theme: default to MATERIAL on all platforms for now
- System_theme: read the GTK palette straight from the theme's gtk.css when the process has no GTK
- Theme: platform default source/mode helpers (system theme on desktop)
- Ui_window: WIP center flag, drag/right-click/click-position queries, X11 move hook
- Mem: diagnostics; ui_text: single-tier font rasters; tray: lazy GTK
- K2b: report call kinds the KRB vocabulary cannot express
- Site: add Discord and Telegram community links
- Widgets: PickerDialog; Spinbox value text + wrap; public ButtonRow height
- Ui_window: Windows/macOS compile the no-op stubs
- Guide overlay: Android back key closes the guide
- Kry_term: drop downstream tool name from the feed comment (boundary check)
- Kry_term: KryTermFeedOutput - harness text printed into the terminal
- Kry_http: streaming partial reads (kry_http_partial) + locked write callback
- Ui_window: include stddef.h so the android/web stubs see NULL
- Kry_sfs: the live engine as a synthetic file system + real input injection
- Ui_window: extra OS windows rendered with the regular UI widgets
- Kt: drop the 'target ide' fork of krait
- Kryon owns the transport, not the vendor: drop kry_zai
- Notification/android: declare GetAndroidApp
- K2c: emit '#define' constants in the generated header
- Kry_http: no-curl stub signature must match the header (const mismatch broke PLATFORM_WEB)
- Web present shim: take the GL context before hiding the canvas (display:none canvases cannot create contexts in some engines)
- Kry std: kry_json, kry_http, kry_zai — the AI-harness foundation
- Web present shim: ?kryonpresent=debug forces activation and reports state via document.title
- Web: kryon-web-present.js — 2D-canvas present mode for WebKitGTK+software GL
- Add cross-platform notification helpers
- Kc is fully gone: CHANGELOG/docs/comments now say k2c
- Theme_picker: switch width matches the retro toggle's content minimum
- Ui_dpi: web viewport is already CSS pixels — don't reapply devicePixelRatio
- Kir joiner: adjacent string literals on continuation lines join
- Kir joiner: '} #else_if COND {' region chains complete as statements
- Kry_signal.h: last kc comment says k2c
- Remove the last kc remnants
- K2c: #private functions are file-static with forward prototypes
- K2c: resolve bare function references passed as call arguments ('set_cb(name)' / 'f(a, name)'), and grow LOWER_TEXT_MAX with KIR_TEXT_MAX so long initializers stop truncating mid-identifier.
- K2c: guard statements, array params, alias-call stem matching, init rewrites
- Kir joiner + k2c name-resolution fixes
- Kir joiner: three continuation bugs
- Kir: compute the body '#if' kind once — parse_cond_start strips the trailing '{' in place, so the second parse in the emission branch saw no brace and every body '#if' lowered to '#elif'.
- Kir+k2c: '#if' regions lower to C preprocessor conditionals
- Tests: default tool paths to the canonical per-platform bin dir
- K2c: array-typed parameters emit C syntax ('T name[N]')
- Kir: parse K&R '} else {' as close + else re-open
- Vendor.mk: fix K2C path export (make ate the sed $ anchors)
- Remove legacy build/bin tool aliases; export canonical K2C for consumers
- Kir: the '#if' region skip runs before import capture
- Kir+k2c: top-level 'static name: T = init' globals; skip captures in unevaluated '#if MACRO {' blocks
- Kir+k2c: capture the 'args' header directive; C-style params pass through
- Kir+k2c: '#private' imports include in the .c only
- Kir: ':=' classifies as DECL before the raw-'c' escape hatch
- Kir+k2c: named enums ('Name :: enum { ... }' -> typedef enum)
- K2c: angled includes stay angled; extension-less header imports append .h
- K2c_project: drop IDE name from comment (boundary rule)
- K2c: emit kryon_project.h/.c (app-host ABI) + main() from Kir
- K2b: migrate to the shared Kir frontend — one frontend, two backends
- K2c: #global variables have external linkage; header order typedefs->enums->globals->structs
- K2c: colon functions keep their bare C names; bare-ref resolution only in assignment-RHS
- K2c: don't rewrite member-access calls (x->fn(...) / x.fn(...))
- K2c: krait's full IDE corpus compiles — all 8 generated files, zero errors
- K2c: emit #extern prototypes with alias-stripped args
- Kir+k2c: typedefs with names, enum commas, type emission order, ternary/&& joins
- Kir+k2c: '#type' typedefs; angle-bracket includes don't join
- Kir: enum constants joined by comma-continuation close their enum
- Kir: fix duplicate anonymous enum (use int for enum_return)
- Kir: pointer types don't continue lines; #enum nests inside structs
- Kir+k2c: join operator continuations; alias-strip array sizes
- K2c: cross-module call resolution via a whole-program symbol table
- Kir: '#'-directive lines are header lines (never join their braces)
- Kir: join multi-line compound literals (expression braces)
- K2c: resolve module-local function calls to their full C names
- Kir+k2c: fix UB alias-strip (src==dst snprintf), emit #enum blocks
- Kir+k2c: struct fields converted to C declarators; '#' comments skipped in bodies
- Kir+k2c: fix block-brace depth tracking + alias rewrite in body statements
- Kir+k2c: struct types (Name :: struct), return types stop at '{'
- K2c: emit #include for module imports (alias :: #import "path" -> #include "path.h")
- Kir+k2c: return types, #extern prototypes, #global vars, module prefixes, alias stripping
- K2c: Kir is the only pipeline — delete the entire legacy compiler
- K2c --kir: PushUIInspectSource wrapping for call statements + ui_inspect.h include
- K2c: Kir→C lowering skeleton (--kir flag, structurally valid output)
- Extract Kir parser into shared kir_parse.c (prerequisite for k2c/k2b backends)
- K2ir: parse app{} metadata (title/size/fps/theme/frame/init/scene/shutdown)
- K2ir: multi-line statement joining (paren/bracket/string depth tracking)
- K2ir: classify mutations (count++) as EXPR not UNKNOWN
- Rename kc to k2c (kry-to-C compiler)
- Archive libkryon.a under the platform-tagged build dir
- Improve site text readability
- Add Kryon browser IDE
- Improve Kryon site button contrast
- Fix CI links and platform icons
- Guard KryScenePhysicsDestroy call so physics-off builds link
- Teach k2ir about imports and externs
- Add k2ir tool scaffold
- Add initial KIR core
- Document KIR compiler roadmap
- Fix per-codepoint font fallback switching typefaces for covered glyphs
- K2b: standalone .kry->.krb compiler, fully independent of kc
- Krb: controls[] section + range widgets (Slider, VerticalSlider, Spinbox)
- Krb: interactive Checkbox + Toggle widgets (value-bound, self-contained toggle)
- Krb compiler: widen widget coverage (color literals, Line/Separator/Bevel/TextInRect, Picture)
- Kc: parse top-level 'frame NAME {}' as a hook function definition
- Scene_property.h: refer to "an IDE inspector" instead of naming Krait
- Build: make 2D physics (Box2D) optional via KRYON_WITH_PHYSICS
- Kryon.h: expose embedded_assets.h through the umbrella
- Krb: drop unused GROUP node and export enums for a clean v1
- Krb: implement F32 field read/write and float text formatting
- Expose kry_term and krb in kryon.h; document the cartridge API
- Add krb cartridge tests
- Kryon-preview: render .kry/.krb to PNG via a cartridge subcommand
- Kc: emit a krb cartridge from .kry (--emit-krb); route codegen via the AST
- Add krb: a packed, mmapable Kryon cartridge format
- Add kry_term: a PTY terminal for IDE hosts
- Add kry_backend: a draw/input vtable for non-raylib cartridge hosts
- UI text: resolve font sources from embedded assets before the filesystem
- UI: preserve ui_scale across SaveUIFrameState/RestoreUIFrameState
- Raylib build: purge stale .o/.a from vendored sources before recompiling
- Add Column/Row layout nodes with flexbox-style auto-positioning
- Add raylib rename-check post-build step to fix symbol collision
- Phase 6: TileMap + AudioSource nodes (all 12 Game2D kinds now implemented)
- Phase 5: animation system (keyframe tracks, AnimationPlayer, AnimatedSprite2D)
- Phase 4: Box2D v3 physics integration
- Fix scale-domain mixing in title bar, toast, and text centering
- Use best available DPI estimate, not just device density
- Invalidate DPI cache when device density changes
- Add device density hook, fix dropdown resize, widen theme spacing
- Add kc signal language support, example, and property/signal tests
- Add property model and signal bus for the scene tree
- Add kc scene-tree language support, example, and tests
- Implement built-in scene nodes: Node2D, Camera2D, Sprite2D
- Add retained scene tree foundation and rename UI Sprite to Picture
- Allow custom value text on Slider widget
- Fix pane tab hit testing
- Add UI font init control
- Tune tree view file label color
- Keep UI keyboard input state backend-independent
- Canonicalize widget API
- Clean material toggle rendering
- Document and enforce canonical app api
- Fix sprite example syntax
- Clean kryon app api
- Add Material theme style widgets
- Restore stable tool targets
- Preserve icon texture colors
- Use host-specific root build directories
- Add GTK file dialog backend
- Document default Ksync transport APIs
- Avoid native Ksync transport warnings
- Add default Ksync transport helpers
- Kc: extract C emission into kc_codegen.c, heal project split
- Kc: extract defer lowering into kc_defer.c
- Kc: extract symbol resolution + expression rewrite into kc_resolve.c
- Kc: extract KryFile/KryFunction mutators into kc_ir.c
- Kc: extract diagnostics + error recovery into kc_diag.c
- Kc: extract pure leaf helpers into kc_util.c
- Kc: delete _funcs.txt and dedupe function-name helper
- Own theme picker labels and expose text drawing helpers
- Force retro UI style
- Retain dropdown overlay clip bounds
- Fix UI transition declaration
- Add clean node measurement constructors
- Route node measurement through widget ops
- Store measurement data on widget nodes
- Measure widget nodes generically
- Move UI runtime to declarative nodes
- Fix classic system focus and material buttons
- Simplify theme style selection
- Centralize native theme defaults
- Add selectable native UI styles
- Fix text context menu overlay handling
- Add text input context menus
- Stabilize UI cursor ownership
- Route cursor shape requests through UI intent
- Fix layout-aware shortcuts and bulk paste
- Adapt syntax colors to dark themes
- Fix cascading tree input capture
- Remove bundled C IDE
- Keep standalone IDE changelog name-neutral
- Support standalone KITE IDE
- Add nonblocking IDE console
- Run kt scenarios visually by target
- Expand kt project scenario tests
- Add kt test runner
- Fix IDE source clipboard fallback
- Polish IDE chrome and clipboard fallback
- Add IDE console panel
- Add IDE problems panel tabs
- Normalize kc source paths in generated output
- Add shared font subsetting target
- Add dynamic font fallback loading
- Avoid Windows API name collisions
- Define NULL for platform thread helpers
- Add compiler diagnostics and UI text backend fixes
- Fix Kryon IDE preview input and source selection
- Isolate text input selection ownership
- Rename Lyra sync APIs to Ksync
- Rename Lyra sync APIs to Ksync
- Phase 3 (foundation): type-aware AST classification of declarations
- Phase 2 (partial): fix defer leaking across switch cases
- Phase 1: build the AST alongside the existing path
- Phase 0: harden the kc syntax oracle + add parser plan
- Add kryon-app dev-backend for local sync development
- Fix click-to-source for widgets used in if conditions
- Add `defer` to the Kry language
- Make text input focus persistent and single-owner across frames
- Update website code sample to verb-free Kry syntax

## Unreleased

### Added

- Automatic patch releases after successful `master` CI, with retry-safe
  version resolution and explicit minor/major bump tooling.
- Checksummed native tools archives containing every Kryon command-line binary
  alongside the existing static SDK and cross-platform renderer downloads.
- CI and standalone Kryon builds now install/link the GIO notification
  dependency explicitly instead of discovering its compile flags only.
- Linux and FreeBSD CI install GTK development metadata required by the Go
  desktop-tray bindings exercised by generated-code smoke tests.
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
