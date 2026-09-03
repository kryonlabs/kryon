# Changelog
## v0.1.39 - 2026-09-03

### Changed

- Release: bump version to v0.1.39
- Fix image front-end duplicates and canvas radial gradient shim
- Make Daochi/ksync optional and refresh the Labs site showcase
- Drop --plan9 from k2cpp
- Ship k2cpp in the tools distribution archive
- Verify k2cpp in the packaged tools bundle
- Fix k2go syntax test filename in make test recipe
- Finish k2g/k2ir rename in runtime parity doc generator and k2kir test rule
- Add k2cpp backend with full k2c parity
- Rename k2g to k2go and k2ir to k2kir
- Decode images in shared front-end for all backends
- Implement Canvas texture updates
- Add Light2D scene nodes
- Modernize the Kryon platform icons
- Add light and dark themes to Kryon Labs
- Give Kryon its crayon color system
- Give Kryon its crayon color system
- Redesign the Kryon Labs website
- Redesign the Kryon Labs website
- Make rejected single-instance launches safe instead of crashing
- Ignore static visitors in Area2D body_enter/body_exit signals
- Read back armed shots through glad when glReadPixels is unresolved
- Fix signal handler dispatch for application-defined kinds
- Add direct Body2D control: transform, velocity set/get
- Emit Area2D body_enter/body_exit signals from Box2D sensor events
- Compute world transforms before ready hooks in SceneTick

## v0.1.38 - 2026-09-01

### Changed

- Release: bump version to v0.1.38
- Build KRB mount test before running it
- Link app storage test filesystem helper
- Allow local network sync URLs
- Guard file app storage on Android
- Add responsive score control
- Add responsive segmented control
- Fallback to fonttools module for subsets
- Add UI font registration from text corpus
- Deduplicate bubbled web input events
- Add shared native Plan 9 application build rules
- Add KryAppDataRoot for per-user app data
- Keep the plan9 rewrite result heap owned
- Iterate the plan9 rewrite for nested literals
- Flatten cast literals nested in field values
- Restrict array literal zeroing to declarations
- Zero arrays declared from matching array literals
- Accept size expressions in array cast validation
- Flatten bracketed array compound literals
- Split array cast types and strip inline in plan9 declarations
- Fix FreeBSD Android DPI test link
- Remove Inbe from Kryon showcase page
- Update showcase and Pages deployment
- Make web embeds resize to their host frame
- Refuse member selections on call results in plan9 typing
- Reject statement keywords as plan9 prototype types
- Parse split static headers that open their body inline
- Join split static prototype prefixes in the plan9 header scan
- Unify void pointers in plan9 expression typing
- Type array casts with size expressions and libc comparisons in the plan9 pass
- Finish plan9 initializer typing: floats, ternaries, externs, arrays
- Type parenthesized and logical initializer expressions in the plan9 pass
- Drop the struct field table from the plan9 pass
- Keep the base identifier typing under member chains
- Restrict struct-body detection in the plan9 field scan
- Type struct fields and member chains in the k2c plan9 pass
- Allow pointer stars in plan9 prototype return types
- Reject expression text in the k2c plan9 prototype map
- Broaden the k2c plan9 type resolution
- Type initializer expressions in the k2c plan9 pass
- Resolve more initializer shapes in the k2c plan9 pass
- Add a plan9-safe output mode to k2c
- Match Canvas2D random values to raylib
- Cache web automation query options
- Ignore empty automation env options
- Add automation options for test runs
- Pace Canvas2D frames with browser vsync
- Respect target FPS on Canvas2D
- Render docs IDE previews directly on canvas
- Build the ksync crypto primitives for Plan 9
- Honor Canvas2D texture filters
- Add web routing stubs to the libdraw backend
- Add android_host and DOM page stubs to the Plan 9 library
- Add the page, overlay, and spritesheet objects and IsTextureValid to the Plan 9 build
- Provide ShowCursor and HideCursor on the native Plan 9 backend
- Improve Debian platform icon
- Define SIZE_MAX in the Plan 9 libc shim umbrella
- Define SIZE_MAX in the Plan 9 limits shim
- Implement rename for the native Plan 9 runtime
- Flatten the remaining form section label compound literal for 8c
- Avoid multi-field designated compound literals in 8c sources
- Export immediate button controls
- Flatten nested designated initializers in form row helpers
- Guard GTK tray fallback deprecation
- Fix KIR parser recursion and tool warnings
- Provide fmaxf/fminf in the Plan 9 math shim
- Add native spritesheet helpers
- Fix recursive KIR expression parsing
- Clean up Kryon build warnings
- Improve Kry language tooling
- Test dismissible overlay input capture
- Add dismissible overlay helper
- Use viewport scale as Android DPI floor
- Stabilize Android viewport scaling
- Add Android viewport policy resolver
- Fix Android UI density scaling
- Implement libdraw texture updates

## v0.1.37 - 2026-08-30

### Changed

- Release: bump version to v0.1.37
- Keep UI tree API test headless
- Lock the canvas-space input contract with tests
- Compress encrypted ksync payload envelopes
- Revert X11 pointer translation: the canvas is window-sized
- X11: translate pointer events by the window's root position
- Runtime: log queued taps under the debug gate
- Runtime: log tap consumption under the debug gate
- Runtime: env-gated checkbox tap debug line
- Tray: log registration and (debug-gated) connection death
- Tray: path-scoped DBus serving and the dbusmenu property surface
- Size the web canvas backing store from its CSS layout box
- Add reusable app storage API
- Add reusable Android host services
- Include KIR text helpers in CMake codegen
- Allocate the canvas font glyph scratch on the heap
- Add post-frame callback scheduling
- Fix render-target blits and autoplay-blocked audio decode in the canvas backend
- Add web app screen routing
- Cache bust showcase banner assets
- Go runtime: Primary/OnPrimary/SurfaceVariant theme getters
- Drop unused import
- Tray test: expect path-style registration
- Fix showcase project directory loading
- Tray: register with the object path (XFCE host requirement)
- Tray: serve the SNI surface before registering with the watcher
- DBus: fall back to the XDG_RUNTIME_DIR session bus socket
- Render context menus in overlay pass
- DBus auth: send the hex of the decimal uid string
- Pure-Go DBus: session client, action notifications, SNI tray
- Refactor compiler text helpers
- Fall back to warp-based relative mouse mode when raw mode fails
- Expand Kryon matrix parity coverage
- Add reusable screen and form helpers

## v0.1.36 - 2026-08-30

### Changed

- Release: bump version to v0.1.36
- Unify Kryon site playground entry

## v0.1.35 - 2026-08-30

### Changed

- Remove README link badges
- Release: bump version to v0.1.35
- Refresh Kryon README header
- Harden KRB asset section loading
- Use host arrow cursor for libdraw windows
- Add checked JSON allocation helpers
- Add libdraw key name lookup

## v0.1.34 - 2026-08-30

### Changed

- Release: bump version to v0.1.34
- Serialize the cmark-gfm vendor rule with a grouped target
- Add page semantics and DOM route API

## v0.1.33 - 2026-08-29

### Changed

- Release: bump version to v0.1.33
- Skip rendering unchanged frames and reuse the present-path buffers
- Add rlActiveTextureSlot to the curated rlgl tier
- Build app sources and vendor deps before the linux arch compile

## v0.1.32 - 2026-08-29

### Changed

- Release: bump version to v0.1.32
- Deliver full clicks from the X11 backend so Button widgets respond

## v0.1.31 - 2026-08-29

### Changed

- Release: bump version to v0.1.31
- Implement TabBar in the pure-Go runtime

## v0.1.30 - 2026-08-29

### Changed

- Release: bump version to v0.1.30
- Make cursor-locked SDL mouse behave like GLFW disabled-cursor mode

## v0.1.29 - 2026-08-29

### Changed

- Fix the X11 window runtime dropping all compat input reads
- Keep rlgl wrappers when audio is compiled out
- Resolve rlgl and raymath inputs next to raylib.h in the compat generator
- Release: bump version to v0.1.29
- Add the 3D tier to the kryon surface
- Let headless links resolve the instance window hooks

## v0.1.28 - 2026-08-29

### Changed

- Release: bump version to v0.1.28
- Add custom-header variants to kry_http

## v0.1.27 - 2026-08-29

### Changed

- Document explicit C extern handling
- Release: bump version to v0.1.27
- Polish JavaScript lowering integration
- Add derived theme color helpers
- Add JavaScript lowering pipeline
- Handle unquoted web script cache busting
- Avoid duplicate Linux directory rules

## v0.1.26 - 2026-08-29

### Changed

- Release: bump version to v0.1.26
- Add DOM web backend smoke coverage

## v0.1.25 - 2026-08-29

### Changed

- Release: bump version to v0.1.25
- Install ripgrep in release workflow

## v0.1.24 - 2026-08-29

### Changed

- Add Android window inset API
- Release: bump version to v0.1.24
- Install ripgrep in Linux CI
- Degrade gracefully when instance lock is unavailable
- Select retained text fields on double click
- Add release and sanitizer check targets
- Track backend capabilities in preflight
- Harden Kryon boundaries and preflight checks
- Fix Android surface resize cache
- Sync frame size for platform UI

## v0.1.23 - 2026-08-28

### Changed

- Release: bump version to v0.1.23
- Use detected Android UI scale
- Scale default UI fonts

## v0.1.22 - 2026-08-28

### Changed

- Release: bump version to v0.1.22
- Improve Canvas2D backend parity
- Add Android prelude activity
- Fix Ksync crypto web warnings
- Fix Android UI DPI scaling

## v0.1.21 - 2026-08-28

### Changed

- Release: bump version to v0.1.21
- Treat unknown Unix update installs as source
- Detect BSD home builds as source installs
- Make spec test sed portable
- Fix conformance matrix build directory
- Add layout-key input and terminal UI primitives
- Include stdbool in generated compat header
- Add rich text editor control
- Fix native linux raylib backend rename
- Add app framework helpers
- Expose IsKeyDown in the Go bindings
- Anchor the Go theme enums at their own const blocks
- Note the Go runtime theme parity in the changelog
- Assert theme catalog relations instead of absolute iota ids
- Focus Kryon site on runtime
- Keep theme catalog count assertion relative to Sweet
- Expose Plan9, Xfce and Sweet themes in the Go runtime
- Hide text input when opening dropdowns
- Fix modal release handling
- Allow scroll viewport visual bleed
- Match Sweet theme colors
- Add Sweet Material theme palette
- Add Android secure store helper
- Fix termi partial frame writes
- Blend translucent termi cell backgrounds
- Render thick termi lines as cells
- Keep termi sixel fallback visible
- Rename terminal showcase to ktrem
- Harden termi texture and process defaults
- Reset termi frame clipping
- Remove unused termi env helper
- Respect terminal viewport in termi backend
- Stabilize termi animated image frames
- Batch termi frame output
- Render termi circles as cell arcs
- Add circle click handling primitive
- Drain termi mouse events per frame
- Add live website examples gallery
- Default termi sixel to capable terminals
- Reduce termi frame output
- Clarify termi button states
- Cache unchanged termi sixel draws
- Fix termi mouse click delivery
- Render termi textures with sixel
- Add termi terminal backend
- Revert "Add TUI backend"
- Revert "Avoid TUI OpenURL ownership conflict"
- Revert "Make TUI backend terminal-safe"
- Revert "Scale TUI backend framebuffer"
- Scale TUI backend framebuffer
- Make TUI backend terminal-safe
- Avoid TUI OpenURL ownership conflict
- Add TUI backend
- Avoid compound literal in icon loader
- Guard Plan 9 math compatibility macros
- Use native Plan 9 default cursor
- Decode embedded icons as RGBA
- Use raylib scissor for UI clips
- Preserve BottomNav icon colors
- Add generated progress parity coverage
- Prepare monochrome UI icons as masks
- Restore Material bottom nav icon colors
- Add generated list box parity coverage
- Normalize UI icon textures to RGBA
- Add generated table parity coverage
- Preserve bottom nav icon colors
- Remove native Plan 9 terminal stubs
- Restore Material bottom nav icon tint
- Add generated controls parity coverage
- Stub legacy terminal helpers on native Plan 9
- Fix native Plan 9 terminal pane color resolution
- Use URI stubs in native Plan 9 archive
- Build terminal pane helpers for native Plan 9
- Avoid Plan 9 terminal header collisions
- Add Xfce theme
- Add Plan 9 sys types shim
- Add visual comparison matrix
- Fix libdraw text and color capture handling
- Add web Canvas source capture matrix gate
- Improve native Plan 9 libdraw desktop rendering
- Track generated C renderer matrix gaps
- Rebuild objects when switching backends
- Avoid raylib flush dependency on native Plan 9
- Preserve button paint order in UI tree
- Capture raylib generated C windows
- Add raylib generated C matrix gate
- Expose renderer source matrix on website
- Preserve libdraw text hierarchy
- Use Rect as canonical shape widget
- Normalize matrix sources to current UI APIs
- Clip libdraw native text draws
- Isolate hosted app keyboard input
- Add libdraw generated-C matrix capture
- Let libdraw follow system themes on Linux
- Add KRB web source matrix gate
- Extend AppHost lifecycle callbacks
- Gate widget coverage in conformance matrix
- Add TableView drag range selection
- Avoid libdraw Rect symbol collision
- Add runtime and downstream matrix gates
- Add libdraw compatibility hooks
- Add Go toolbar menu and workbook icons
- Add Go toolbar menu and workbook icons
- Add renderer smoke matrix gate
- Add KRB visual conformance checks
- Test bottom nav icon color preservation
- Add generated Kry conformance matrix
- Resolve Kry host compiler outside Android toolchain
- Use host compiler for Kry generation tools
- Add CMake helper for Kry source generation
- Remove text input sync diagnostics
- Add eye visibility icons
- Coalesce platform text input state per frame
- Keep platform text input active while focused
- Honor text input show pulses without active state
- Log text input platform sync state
- Request text input platform show on field taps
- Sync text input platform state at frame end
- Link only showcase author names
- Remove showcase ranking note
- Decode X11 spreadsheet navigation keys
- Show showcase authors on site
- Decode X11 escape key
- Test platform text input reactivation
- Reset platform text input when deactivated
- Support horizontal text field panning
- Normalize fingerprint icon color
- Use themed text input colors without outer focus box
- Keep fallback text field focus per input
- Add no-cgo mouse drag state
- Render text fields in immediate UI
- Improve material text field contrast
- Outline material text fields
- Allow optional text field focus state
- Fix modal capture and bottom nav icon colors
- Use raylib render batch hook for screenshots
- K2g pass char buffer args to externs
- K2g fix local helper calls
- K2g resolve cross-file helper calls
- K2g support direct Go extern calls
- Keep Material neutral tones unsaturated
- Update generated icon assets
- Revalidate site deploys
- Expose theme settings in Go runtime
- Lower theme settings for Go runtime
- Clean #ui docs site snippets
- Use generated showcase data on homepage
- Add clean pane tab wrappers
- Adopt #ui hierarchy syntax
- Update k2 syntax tests for retained UI apps
- Fix canvas backend smoke source filters
- Track retained UI entry functions in Kir
- Fix k2g generated app entrypoint
- Use top-origin canvas font metrics
- Generate showcase ranking data
- Add table clipboard shortcuts
- Fix Android clipped UI after rotation
- Fix canvas guide paragraph clipping
- Add Android surface resize sync
- Restore site icon artifact
- Update showcase stars and banners
- Support fixed buffers in k2g extern calls
- Let native Plan 9 follow the system theme
- Exclude Plan 9 platform sources from native builds
- Make icons tree the shared source root
- Complete native Plan 9 audio streams
- Implement native Plan 9 audio mixing
- Fix native Plan 9 libdraw font scaling
- Improve native Plan 9 libdraw backend
- Remove the unused Clay layout dependency
- Skip missing optional compat cleanup helper
- Add the install target to the plan9 mkfile
- Keep image decoder stb symbols private
- Exclude Plan 9 sources from non-libdraw builds
- Use vendored stb image include
- Untrack native plan9 build objects
- Ignore plan9 cpp-stage intermediates
- Make the plan9 ui globals inert on hosted builds
- Harden the compound literal rewriter against conditional includes
- Keep the zero constants outside preprocessor conditionals
- Fix the ui internals include depth for hosted builds
- Include the ui internals by relative path for hosted builds
- Drop line-splicing backslashes left by the comment stripper
- Port ApplyCurrentUITheme for the plan9 theme core
- Search src/ui for internal headers in the plan9 build
- Add plan9 ui globals and empty asset table for the theme core
- Rename cpp-stage objects to the mkfile object names
- Assemble UIStyleTokens field-wise for the plan9 compiler
- Scope the plan9 library to the theme core for now
- Drop the Box2D-dependent scene layer from the plan9 build
- Guard the Box2D bridge header behind KRYON_WITH_PHYSICS
- Skip the fontconfig pipe helpers on plan9
- Avoid a scalar compound literal in the sha256 length padding
- Drop the shim offsetof in favor of the libc definition
- Shim stdio.h so u.h precedes the pANS varargs declarations
- Provide size_t from the plan9 libc shim
- Define NULL centrally in the plan9 libc shim
- Replace short compound literals with zero constants for the plan9 compiler
- Guard the plan9 assert shim against the libc definition
- Resolve plan9 shim PI and offsetof redefinitions
- Fix comment apostrophe stripper to keep line-comment newlines
- Enable C++ comments in the plan9 cpp pass
- Use absolute include paths in the plan9 mkfile cpp rule
- Preprocess the plan9 build through the system cpp and strip comment apostrophes
- Guard plan9 shim re-includes of the include-once core headers
- Keep stdarg include unconditional for the plan9 shim build
- Fix plan9 mkfile source paths
- Add native Plan 9 libdraw compatibility
- Preserve libdraw UI icon colors
- Match libdraw rounded radius semantics
- Fix libdraw picture masks and audio
- Handle fractional libdraw texture sources
- Complete libdraw image compatibility
- Expose toast draw helper
- Add libdraw backend and Plan9 theme
- Use Glenda icon for Plan 9 site card
- Make table escape clear selection
- Keep system theme fallback selection neutral
- Use GTK selected colors in system theme
- Add native table row and column selection
- Fix X11 keyboard mapping in Go runtime
- Add widget conformance for text input focus
- Fix Go table double-click selection
- Avoid startup X11 focus errors
- Request X11 input focus for Go windows
- Fix native Go text field input
- Bring Go theme handling to C parity
- Render registered fonts in Go runtime
- Follow system theme in Go tables
- Cover portfolio symbols in Go renderer
- Write screenshots from Go runtime
- Add native table cell support to Go runtime
- Complete Go Kryon window compatibility wrappers
- Add Go Kryon legacy UI compatibility
- Fix Go X11 setup parsing
- Log Go X11 window ids in debug mode
- Sync mapped Go X11 windows
- Report native Go window fallback errors
- Support Xauthority in Go X11 window runtime
- Install FreeBSD CI test tools
- Install FreeBSD CI test tools
- Add native Go X11 window runtime
- Make site navigation page-based
- Elide retained UI lifecycle from k2g output
- Rename retained tree keys
- Rename retained tree node IDs
- Rename navigation and profile API
- Add Kryon app showcase
- Rename menu and accelerator API
- Rename geometry and canvas API
- Rename syntax mode API
- Rename modal action API
- Rename paragraph spec API
- Rename text size API
- Rename button style API
- Rename public button spec API
- Rename internal button render path
- Rename internal text input render path
- Check generated feature matrix docs
- Rename public text helper APIs
- Remove legacy KRB widget aliases
- Rename public text input APIs
- Rename text input platform callback API
- Rename public text layout API
- Hide retained tree draw internals
- Hide text draw internals from public header
- Hide composite DrawUI APIs from public headers
- Hide legacy control draw APIs from public header
- Rename public text input queue API
- Assert generated long text runtime stability
- Clean legacy widget names from API docs
- Remove legacy Go cgo bridge
- Remove legacy generated C surface names
- Document clean runtime parity surfaces
- Remove legacy frame aliases from generated path
- Remove k2g runtime override
- Document qualified generated Go runtime
- Qualify generated Go runtime calls
- Add generated long text parity
- Stress native host text input
- Add native Go host core
- Add fingerprint UI icon
- Use OpenURI for UI links
- Render generated Go parity frames
- Add native Go frame renderer
- Block kryc from public surface
- Fix web URI opener macro
- Improve scroll focus and native window behavior
- Assert generated Go frame operations
- Record native Go frame operations
- Avoid OpenURL duplicate on Android
- Add scroll rect visibility helper
- Prune direct Go field state per frame
- Add direct native Go widget helpers
- Add native Go generated layout semantics
- Expose native Go runtime input helpers
- Map native Go text size constants
- Align generated button click parity
- Expand generated runtime parity coverage
- Disable single-instance lock on Android
- Fix URI launcher compatibility generation
- Add generated runtime parity test
- Emit bare clean Go runtime calls
- Guard clean generated runtime surface
- Emit clean package Go runtime calls
- Add cross-platform URI opener
- Clean generated frame runtime surface
- Expand native Go text input semantics
- Enforce clean generated runtime output
- Add native Go k2g runtime
- Expose toast drawing API
- Keep read-only text out of edit mode
- Fix Go text field state handling
- Add read-only selectable text controls
- Fix X11 input ungrab status check
- Clean picture fit API names
- Rename overlay hook to clean API
- Update picture props lowering
- Clean picture API styling
- Fix screenshot PNG row buffer size
- Add system locale detection
- Use neutral path in session title test fixture
- Add terminal input filtering for Kapsule
- Simplify window unregister loop exit
- Add terminal pane glyph grid rendering
- Add Windows tray menus and registry theme detection
- Let foreground widgets own the cursor over disabled background
- Add terminal profile prompt helpers
- Add terminal search controller
- Add terminal clipboard actions
- Add terminal scroll indicator helper
- Add terminal row reflow helper
- Add terminal profile settings helpers
- Add terminal session record helpers
- Add terminal keyboard input runner
- Add terminal DCS buffer helper
- Add terminal sixel decoder
- Add simple terminal clipboard runner
- Remove legacy terminal prefix aliases
- Add terminal mode state transitions
- Add terminal OSC command parser
- Add terminal OSC palette parsers
- Add terminal clipboard controller builder
- Add terminal OSC color target helpers
- Add terminal function key mapper
- Add terminal clipboard scroll reset hook
- Add terminal search navigation helpers
- Add terminal clipboard command result helper
- Add terminal pane clipboard and cursor helpers
- Add terminal device status report formatter
- Add terminal mode report formatter
- Add terminal XTGETTCAP response formatter
- Add terminal SGR status formatter
- Add terminal cursor style report helper
- Add terminal pane clipboard command controller
- Add terminal pane view color resolver
- Add terminal pane hyperlink sanitizers
- Add terminal pane title sanitizer
- Add terminal pane clipboard selection helpers
- Add terminal pane theme color resolver
- Add terminal pane link theme token
- Add terminal clipboard action dispatcher
- Add terminal pane color resolver
- Add terminal pane text escaping helpers
- Add terminal pane profile color helpers
- Add terminal pane clipboard and cursor helpers
- Add terminal pane session title helper
- Add terminal pane OSC title helpers
- Add terminal pane OSC color helpers
- Add terminal pane profile color helpers
- Add shared terminal mouse and paste helpers
- Add shared terminal key encoding
- Add terminal pane clipboard helpers
- Add shared terminal pane search
- Add shared terminal pane selection clipboard
- Add terminal pane content metrics
- Add shared clipboard source helpers
- Add shared clipboard paste writer
- Add terminal pane status theme tokens
- Add OSC 52 clipboard helper
- Add ABI version guard for prebuilt libraries
- Resolve trace env flags once per process
- Memoize material scheme per theme state
- Rate-limit automatic system theme refresh
- Cover clipboard target helpers
- Add clipboard target helpers
- Move UI clipboard buffer into Kryon source
- Add terminal pane default palette
- Add UI clipboard buffer helpers
- Expose context menu widget
- Add terminal pane selection text color
- Add UI primary selection helpers
- Expose vsync window flag in Go
- Enable Go textarea layout caching
- Use system UI font by default
- Use system style on desktop
- Remove Aero style
- Stop textarea painting past viewport
- Cache versioned textarea heights
- Make styled pictures respect UI style
- Add styled cover picture drawing
- Fix menu bar switching with open popup capture
- Fix runtime downloads and text decoding
- Fix menu close state after popup activation
- Support sticky UI windows on X11
- Fix menu bar switching between open menus
- Allow apps to provide system theme palettes
- Fix Material bottom nav icon contrast
- Apply Canvas cursor to document targets
- Fix material tab bar spacing
- Consolidate Kry language documentation
- Document Kry language and publish benchmarks
- Advance Kry compile-time checks and expression KIR
- Fix Canvas guide rendering and cursor
- Draw menu bars as frame overlays
- Add reusable app runtime helpers
- Fix Android notification priority signature
- Implement Canvas WebAudio backend
- Add reusable terminal pane widget
- Avoid duplicate badge measurement
- Add website backend and widget matrices
- Align installed desktop metadata names
- Add Linux desktop integration helpers
- Add themed Go surface widgets
- Ui_window: baseline the drag at the press position
- CI: gate on public-HTTPS submodule URLs
- Ui_window: follow the global pointer during drags, not motion events
- Ui_window: flush a released drag's final motion
- Ui_window: capture the mouse during drags and drop per-frame position reads
- Ui_window: expose StealUICoreWindowClose for direct close polling
- Ui_window: bridge core-window close and drag on the SDL path
- Kry_update: arch-aware AppImage appcast key
- Kryui: Go bindings for kry_update_flow
- Tray: resolve gtk_status_icon_is_embedded through gtk_dl
- Kry_std: kry_update_flow — the embeddable self-update lifecycle
- Tray: ignore XEMBED-handshake activations on the status icon
- Kry_std: self-update primitives — download, verify, apply
- Kry_std: add kry_update desktop update-check module
- Theme: default to the system theme source on every platform
- Canvas: zero-init LoadFont's failure return
- Canvas: dedupe the JS glue (ctxNow/col/makeCanvas, rounded radius)
- Canvas: split the backend into per-concern sources
- Canvas: frame counter and last-op diagnostics in the JS state
- Canvas: null-grade the rest of the audio surface
- Canvas: SaveFileData/Text, ChangeDirectory, GetWorkingDirectory
- Canvas: IsWindowFocused on the surface (dropdown hover uses it)
- Locale: detect the system default locale (GetDefaultLocaleCode)
- Theme picker: route label fallbacks through the locale catalog
- Dropdown: wobbled and slow clicks must select, not just pixel-perfect taps
- Canvas: PauseSound and IsMusicStreamPlaying on the surface
- Krb-run: remove the Linux framebuffer presentation backend
- Canvas: file-name utils and IsMusicValid on the surface
- Theme: drop the dedicated Aero palette, keep the style palette-agnostic
- Canvas: cover the audio-device, wave, and image surface apps drive
- Dropdown: dismiss popups on Escape and window focus loss
- Dropdown: flip tall popups above the button when space below runs out
- Canvas: real rounded rects, annulus rings, full tint, clipboard mirror

## Unreleased

### Added

- Add an Android secure-store helper that saves and unlocks secrets with
  biometric/user-authenticated Android Keystore keys.
- Add a Sweet app palette, with dark colors matched to the Sweet-Dark GTK
  theme, and make it the default Material app palette.
- Expose the Plan9, Xfce and Sweet palettes in the Go runtime: the bindings
  previously stopped at Cobalt and clamped every other id to Mono. Also keep
  the circle/accent catalog bytes, add `GetUIMaterialScheme` with the full
  Material color roles, and `DefaultThemeForThemeStyle` for pairing a widget
  style with its default palette.
- Let scroll containers render a small visual bleed around their viewport so
  control borders, focus rings, and Material state layers are not clipped.

### Removed

- Drop the unused Clay layout dependency: nothing in Kryon or downstream
  apps called it, and it only compiled dead symbols into libkryon.

## v0.1.20 - 2026-08-18

### Changed

- Release: bump version to v0.1.20
- Canvas: compile to an empty TU outside emcc
- K2g: reach Go parity on the Runtime surface
- Canvas: implement the HTML5 Canvas2D Tier A backend

## v0.1.19 - 2026-08-18

### Changed

- Release: bump version to v0.1.19
- K2c: emit a runnable main for screen-only apps
- Build: keep the canvas backend out of native builds
- Screenshots: neutral arm name, opt-in debug dumps, shared PNG writer
- Docs: k2c column, bare widget names, and a permanent HTML renderer

## v0.1.18 - 2026-08-18

### Changed

- Release: bump version to v0.1.18
- Backend: compile the KRB audio cap out without the raylib audio module
- K2g: lower typed declarations, arrays, and goto/labels for real
- Compat: let apps drop the audio forwarders from the generated wrappers
- Krb-run: --backend fb presents cartridges on the Linux framebuffer
- Kry_sw: track the dirty rectangle per call
- K2b: lower Combobox to a KRB_CTRL_COMBOBOX control
- Screenshots: fix PNG chunk CRCs in the native writer
- K2g: cover the full widget whitelist on the Go Runtime interface

## v0.1.17 - 2026-08-18

### Changed

- Release: bump version to v0.1.17
- Docs: add FEATURE_MATRIX.md mapping widgets and features across targets and backends
- Ci: drop the renderer KRY_SW_SRCS overrides

## v0.1.16 - 2026-08-18

### Changed

- Release: bump version to v0.1.16
- Ci: include kry_sw_png in the renderer KRY_SW_SRCS overrides
- Build: link kry_sw_png into the KRB logic and asset tests
- Build: link kry_sw_png into the KRB logic and asset tests
- Tray+notify: SetDesktopTrayIcon emblem swap, action-capable notifications
- Kry_sw: decode PNG assets for texture draws
- Ui: build and test the secondary window paths

## v0.1.15 - 2026-08-18

### Changed

- Release: bump version to v0.1.15
- Ci: run workflow actions on Node 24 runtimes

## v0.1.14 - 2026-08-18

### Changed

- Release: bump version to v0.1.14
- Ui: make retained frames work without a render context
- Test: trace and stress retained text input
- Ui: select all text on double click
- Test: cover click then tab field traversal
- Correct secondary window texture orientation
- Ui: route retained focus traversal
- Build: always index static runtime archives
- Runtime: enforce one app instance by default
- Synchronize shared secondary window textures
- Runtime: pace event-driven UI frames
- Kryui: window flags and min size on the k2g AppConfig
- Present secondary window textures without readback
- Ui: keep font atlases immutable while typing
- K2g: host bridge, enums, switch and full for headers
- Render SDL secondary windows with shared GL contexts
- Disable unsafe Linux secondary SDL windows

## v0.1.13 - 2026-08-18

### Changed

- Release: bump version to v0.1.13
- Web: build the current KRB compiler sources
- Build: avoid case-colliding makefiles
- Ci: invoke Homebrew GNU make directly
- Ci: pin the macOS renderer compiler
- Ci: ignore macOS make compatibility preload
- Ci: reset inherited make flags on macOS
- Web: link shared KIR parser into IDE tools
- Ci: prevent recursive macOS renderer build
- Ci: install renderer toolchain dependencies
- Ci: build renderer targets from complete checkouts
- Ui: enforce cross-lowering text input latency
- Avoid fixed buffer when canonicalizing tray icon path
- Pass absolute icon path to AppIndicator
- Resolve tray icon theme path for GNOME
- Ui: complete retained text editing actions
- Revert "Fix C-style local declarations in k2c"
- Test: cover legacy k2c declarations
- Ui: retain mouse and keyboard text selection
- Fix C-style local declarations in k2c
- Ui: rebuild font atlases only between frames
- Add SDL auxiliary windows for Wayland
- K2c: preserve anonymous enum declarations

## v0.1.12 - 2026-08-17

### Changed

- Fix AppIndicator icon lookup on Wayland
- Ui: retain selected font for deferred text
- Release: bump version to v0.1.12
- Go: expose retained declarative UI
- Ui: add retained declarative tree and typed events

## v0.1.11 - 2026-08-17

### Changed

- Release: bump version to v0.1.11
- Ui: make text rendering event efficient

## v0.1.10 - 2026-08-17

### Changed

- Release: bump version to v0.1.10
- Ui: add fixed-subset font sources

## v0.1.9 - 2026-08-17

### Changed

- Ui: copy font type string in RegisterUIFontSource
- Ui: raise font raster tier cache from 4 to 8 slots
- Ui: sample system theme from CSS before initializing GTK in-process
- Release: bump version to v0.1.9
- Build: log missing SDL2 DPI query once instead of every frame

### Fixed

- Text entry stays responsive when an app mixes its regular UI font with emoji or icon fonts, even when the fallback is registered first.
- Typing and redrawing text no longer repeatedly scans the full font atlas for every glyph.
- UI text uses exact-size font rasters, and idle windows can sleep until the next input event.

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
- Terminal: drop downstream tool name from the feed comment (boundary check)
- Terminal: TerminalFeedOutput - harness text printed into the terminal
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
- Expose terminal and krb in kryon.h; document the cartridge API
- Add krb cartridge tests
- Kryon-preview: render .kry/.krb to PNG via a cartridge subcommand
- Kc: emit a krb cartridge from .kry (--emit-krb); route codegen via the AST
- Add krb: a packed, mmapable Kryon cartridge format
- Add terminal: a PTY terminal for IDE hosts
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
- PTY terminal (`Terminal`) for IDE hosts: spawn `$SHELL`, write keys, poll
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
