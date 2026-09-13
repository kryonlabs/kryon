# Kryon Feature Matrix

What every widget and feature supports, per compile target and per renderer
backend. For how backends are selected and implemented see `docs/BACKENDS.md`;
for the cartridge binary format see `docs/KRB_FORMAT.md`; for the language see
`docs/KRY_LANGUAGE_SPEC.md`. Keep this file current with the code, and
regenerate the browsable view after every change:
`python3 scripts/feature-matrix-html.py` writes `docs/FEATURE_MATRIX.html`.

Legend: ✅ available · ◐ partial (reason given) · ✗ not available · — not
applicable. **C** is the `libkryon.a` C API; **k2c** is what `.kry` reaches
through the C compiler — always equal to C, because the language is C-close
and every library call is a legal statement (the widget-statement whitelist
below only adds sugar). The other compilers lower subsets, which is where
the columns diverge.

## The pipeline

```
.kry source
   │   shared frontend: cmd/kir/kir_parse.c -> KirProgram
   ├── k2c  -> .c/.h + kryon_project.c/.h  -> cc + libkryon.a    -> native C app
   ├── k2cpp -> .cpp/.hpp + kryon_project.c/.h (extern "C" decls; same lowering as k2c)
   ├── k2go  -> .go (kryon.<Widget> calls)  -> go build + go/kryon -> Go app
   ├── k2js -> .js (web runtime calls)      -> browser/Node ESM   -> Web app
   ├── k2b  -> .krb (+ .krb.c/.krb host)   -> KrbLoad/KrbExec on any KryBackend
   └── k2kir -> .kir (text IR dump; debugging, tests, Krait)
```

| Target | Producer | Runtime | Status |
|---|---|---|---|
| C | `k2c` | `cc` + `libkryon.a` (raylib/null/canvas/libdraw surface backend) | Most complete path; production use |
| C++ | `k2cpp` | `c++` + `libkryon.a`; C++ codegen with C linkage, same lowering as `k2c` | Parity port of the `k2c` backend |
| Go | `k2go` | `go/kryon` native Go package, no cgo | Declarative subset; executable CI gate |
| JS/Web | `k2js` | `web/kryon-runtime.js` ESM recorder/presenter | Syntax, Node recorder snapshots, and generated runtime state parity gated |
| KRB cartridge | `k2b` | `src/krb/krb.c` via the `KryBackend` vtable | Format v2; byte-exact across engines; CI-gated |
| KIR | `k2kir` | — (inspection artifact) | Debugging/tooling only |

Two backend tiers exist (see `docs/BACKENDS.md`):

- **Tier A — surface backends**: implement the ~700-symbol raylib-compatible
  surface (`include/kryon_compat.generated.h`), selected at **link** time via
  `KRYON_BACKEND=raylib|null|canvas|libdraw`. The whole widget catalog runs on
  these.
- **Tier B — cartridge hosts**: implement the ~20-function `KryBackend` vtable
  (`include/kry_backend.h`), selected at **runtime**. Only the KRB widget
  subset runs on these.

## Widget statement whitelist (`.kry` frontend)

`parse_widget_statement` (`cmd/kir/kir_parse.c`) recognizes 150 widget names.
`k2c` compiles any library call regardless (plain call statement); `k2cpp` shares that lowering (C++ output, C linkage); `k2go` lowers
the full whitelist onto its `Runtime` interface;
`k2js` records whitelisted standalone widget calls as browser-loadable runtime
operations; and `k2b` lowers a subset of it:

`AppBackground Background Text Paragraph Abbr Abbreviation Address Article Aside
Box Line Bevel Icon Image Button Card Selectable Audio Bdi Bdo
BidirectionalIsolate BidirectionalOverride BlockQuote Bold Cite Code
CodeBlock Col ColGroup Data Datalist DataList Del Deleted DescriptionDetails DescriptionList
DescriptionTerm Details Dialog Em Embed Emphasis Figcaption Figure Footer Form
Header IFrame Iframe Ins Inserted Italic Kbd Keyboard Label Legend List
ListItem Main Mark Meter Nav Navigation OrderedList OptionGroup OptGroup Option
Output Pre Quote Rp Rt Ruby RubyParenthesis RubyText Samp Sample Select Small Source Strong Sub Subscript Summary Sup Superscript Table
TableBody TableCaption TableColumn TableColumnGroup TableFoot TableHead TableRow
Tbody Tfoot Thead Time Tr Track UnorderedList Var Variable Video Wbr
WordBreakOpportunity Bullet Separator Link
TextField TextArea Dropdown SegmentedControl Slider Menu Toggle Checkbox Radio
Progress Plot Drag Input Spinbox DragDrop Screen Page Section Heading
ParagraphText Column Row Stack Flow Grid Scroll End Modal TitleBar TabBar
NavigationBar Toolbar Toast Fieldset PanedView Collapsible ListBox TreeView
TableView ColorPicker CanvasGrid`

`Canvas` and `TableCell` are lexical `.kry` blocks rather than ordinary one-call
widgets; `Scroll` is both a parser-recognized widget statement and a lexical
block form for scroll content. The compiler lowers these to host support.
The clean command-menu concept is `Menu`; bar, popup, and context behavior are
selected by `MenuProps`.

## Widget matrix

The upstream widget-family crosswalk and its remaining semantic gaps are kept
in [`IMGUI_WIDGET_COVERAGE.md`](IMGUI_WIDGET_COVERAGE.md).

Columns: **C** = the C API · **k2c** = `.kry`→C codegen (always equal to C) · **k2cpp** = `.kry`→C++ codegen (mirrors `k2c`) · **k2go** = `.kry`→Go codegen
(pure Go importing `go/kryon` as `kryon` and calling `kryon.<Widget>`) · **Go** =
hand-written Go via the `go/kryon` package API · **KRB** = lowered into a
cartridge by `k2b`.

Retained-tree caveat that applies to the whole C column: `#ui` lowering
records ~29 widget kinds, while the retained painter covers BACKGROUND, TEXT,
RECT, LINE, BUTTON, TEXT_FIELD, TEXT_AREA, SLIDER, TOGGLE, and CHECKBOX. It
routes retained interaction for BUTTON, TEXT_FIELD, TEXT_AREA, SLIDER, TOGGLE,
and CHECKBOX;
every other kind renders through its immediate-mode call during the
declaration pass (`src/ui/ui_tree.c`).

### UI/Display

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Background | ✅ | ✅ | ✅ | ✅ | ✅ `Background` | ✅ node |
| Text | ✅ | ✅ | ✅ | ✅ | ✅ `Text` | ✅ node |
| Text properties (bounds, wrap, clip, color, alignment, disabled) | ✅ | ✅ | ✅ | ✅ | ✅ `Text(TextProps)` | ✅ |
| Paragraph (rich text + inline icons) | ✅ | ✅ | ✅ | ✅ | ✅ `Paragraph` | ✗ |
| Box | ✅ | ✅ | ✅ | ✅ `Box` (+ native `RectGradientH`) | ◐ `DrawRectangle*` primitives | ✅ node |
| Line | ✅ | ✅ | ✅ | ✅ | ✅ `DrawLine` | ✅ |
| Bevel | ✅ | ✅ | ✅ | ✅ | ✅ `Bevel` | ✅ |
| Icon sheets (MingCute UI plus 6 full-color families) | ✅ | ✅ | ✅ | ✅ (by icon type) | ✅ `Icon` | ✗ |
| Image | ✅ | ✅ | ✅ | ✅ `kryon.Image(kryon.ImageProps)` | ✅ `kryon.Image` | ✅ node (embedded asset or PNG) |
| Image style / image Button | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| Plot | ✅ | ✅ | ✅ | ✅ | ✅ `Plot` | ✗ |

### UI/Input

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Button (ButtonProps) | ✅ | ✅ | ✅ | ✅ | ✅ `kryon.Button(kryon.ButtonProps)` | ✅ node |
| Selectable / Checkbox | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| Button options (small, arrow, info/help, menu, split) | ✅ | ✅ | ✅ | ✅ | ✅ `Button(ButtonProps)` | ◐ BUTTON style byte |
| Link (open URL) | ✅ | ✅ | ✅ | ✅ | ✅ `Link` | ✗ |
| TextField | ✅ | ✅ | ✅ | ✅ | ✅ `kryon.TextField(kryon.TextFieldProps)` / `kryon.TextField("Name", &value)` | ✅ TEXTINPUT node |
| Read-only text | ✅ | ✅ | ✅ | ✅ via `Text` | ✅ `Text(TextProps)` | ✗ |
| TextArea (selection, syntax highlight) | ✅ | ✅ | ✅ | ✅ | ✅ `TextArea` | ✗ |
| Dropdown | ✅ | ✅ | ✅ | ✅ `Dropdown` | ✅ `Dropdown` | ✅ DROPDOWN control |
| SegmentedControl | ✅ | ✅ | ✅ | ✅ `SegmentedControl` | ✅ `SegmentedControl` | ✗ |
| Slider | ✅ | ✅ | ✅ | ✅ `Slider(SliderProps)` | ✅ `Slider` | ✅ SLIDER/VSLIDER control |
| Toggle (switch) | ✅ | ✅ | ✅ | ✅ | ✅ `Toggle` | ✅ node |
| Checkbox (+ disabled) | ✅ | ✅ | ✅ | ✅ | ✅ `Checkbox` | ✅ node |
| Radio | ✅ | ✅ | ✅ | ✅ | ✅ `Radio` | ✅ `KRB_CTRL_RADIO` |
| Progress | ✅ | ✅ | ✅ | ✅ | ✅ `Progress` | ✅ `KRB_CTRL_PROGRESS` |
| Spinbox | ✅ | ✅ | ✅ | ✅ | ✅ `Spinbox` | ✅ SPINBOX control |
| ColorPicker (RGB sliders) | ✅ | ✅ | ✅ | ✅ | ✅ `ColorPicker` | ✗ |
| Drag values and ranges | ✅ | ✅ | ✅ | ✅ | ✅ `Drag(DragProps)` | ✗ |
| Slider values, vertical sliders, and angle sliders | ✅ | ✅ | ✅ | ✅ | ✅ `Slider(SliderProps)` | ✗ |
| Numeric keyboard inputs | ✅ | ✅ | ✅ | ✅ | ✅ `Input(InputProps)` | ✗ |
| Button sizing/options plus invisible hit-test support | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| Separator labels | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| ColorPicker / swatch Button | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |

### UI/Layout

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Column / Row / Stack (flex-like) | ✅ | ✅ | ✅ | ✅ all three | ✅ all three | ✅ structural no-ops (node table is the tree) |
| Group | ✅ (lowers to Stack) | ✅ | ✅ | ◐ via Column | ✅ via Stack | ✅ |
| Scroll block | ✅ `Scroll` | ✅ `.kry` block lowering | ✅ `.kry` block lowering | ✅ generated wheel/drag/nested-child tests | ✅ wheel scrolling, scrollbar dragging, and nested clipping via `Scroll` | ✅ SCROLL node |
| Separator | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Fieldset | ✅ | ✅ | ✅ | ✅ | ✅ `Fieldset` | ✅ rect/text lowering |
| TabBar (tabs) | ✅ | ✅ | ✅ | ✅ | ✅ `TabBar` | ✗ |
| PanedView (splitter) | ✅ | ✅ | ✅ | ✅ | ✅ `PanedView` | ✗ |
| Collapsible | ✅ | ✅ | ✅ | ✅ | ✅ `Collapsible` | ✗ |
| Geometry layout (`Rectangle`, `Grid`, `Column`, `Row`, `Stack`) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ structural |
| Canvas (pan/zoom, hit-test, grid) | ✅ | ✅ | ✅ | ✅ `Canvas` block | ✅ host canvas scope + hit-test | ✗ |

### UI/Collections

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| ListBox (+ disabled) | ✅ | ✅ | ✅ | ✅ | ✅ `ListBox` | ✗ |
| TreeView (+ disabled) | ✅ | ✅ | ✅ | ◐ `TreeView` | ◐ `TreeView` | ✗ |
| TableView (resizing, frozen rows, sort direction, cell colors, column visibility/order, focus/keyboard/clipboard, disabled) | ✅ | ✅ | ✅ | ✅ | ✅ `TableView` | ✗ |
| CanvasGrid | ✅ | ✅ | ✅ | ✅ | ✅ `CanvasGrid` | ✗ |
| Selectable text | ✅ `TextProps.selectable` | ✅ | ✅ | ✅ via `Text` | ✅ via `TextProps.Selectable` | ✗ |
| ListBox multi-select props (focus, keyboard navigation, Ctrl/Shift range selection) | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| Nested disabled scope | ✅ | ✅ | ✅ | ✅ | ✅ `Disabled` block | ✗ |

### UI/Navigation

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Menu (bar, popup, and context modes) | ✅ | ✅ | ✅ | ✅ | ✅ `Menu` | ✗ |
| Popup scope (ordinary, hover-tooltip, modal arbitrary native content) | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| TabBar | ✅ | ✅ | ✅ | ✅ | ✅ `TabBar` | ✗ |
| Button add-tab actions / closable tab items | ✅ | ✅ | ✅ | ✅ | ✅ | ✗ |
| DragDrop (typed source and target roles) | ✅ | ✅ | ✅ | ✅ | ✅ `DragDrop` | ✗ |
| NavigationBar (+ config modal) | ✅ | ✅ | ✅ | ✅ | ✅ `NavigationBar` | ◐ `NavButton` lowers to a BUTTON |
| Toolbar | ✅ | ✅ | ✅ | ◐ `Toolbar` only | ✅ `Toolbar` | ✗ |
| TitleBar family | ✅ | ✅ | ✅ | ◐ `TitleBar` only | ✅ `TitleBar` | ✗ |

### UI/Overlays

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Modal | ✅ | ✅ | ✅ | ✅ `Modal` | ✅ `Modal` | ✗ |
| Toast | ✅ | ✅ | ✅ | ✅ `Toast` | ✅ `Toast` | ✗ |
| Transition/focus policy | ✅ | ✅ | ✅ | ✗ | ✗ | ◐ `AnimNode` + `TIME` opcode drive animation |

### UI/Composite And App Framework

| Widget | C | k2c | k2cpp | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Reorder (drag handle + placeholder) | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| Route stack / shell measurement | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| Capabilities / safe content rect | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| Setting normalization helpers | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |

### Game2D (scene tree, `src/scene/`)

| Node | C | ✅ | ✅ | k2go | Go | KRB |
|---|---|---|---|---|---|---|
| Scene / Node2D / Camera2D | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| Sprite2D / AnimatedSprite2D / TileMap / Light2D | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| CollisionShape2D / Area2D / Body2D (box2d) | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| AnimationPlayer / AudioSource | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |
| Signals (`KrySignal`) / keyframe animation | ✅ | ✅ | ✅ | ✗ | ✗ | ✗ |

### KRB cartridge vocabulary (no C widget equivalent)

| Item | Notes |
|---|---|---|
| DATA node | state-field mount read/written by bytecode |
| CIRCLE / RING nodes | lowered from `DrawCircleV` / `DrawRing` calls |
| `NavButton` | `k2b`-only lowering; becomes a BUTTON with `KRB_FLAG_NAV` |
| `AnimNode` | `k2b`-only; animated node driven by `TIME` |
| v2 logic bytecode | stack machine, node mutation, if-guards over node ranges |
| Host capabilities | imported storage/http/audio/notify calls (`krb_caps.c`) |

## Cross-cutting features

| Feature | C | Go | KRB |
|---|---|---|---|---|
| Theming | ✅ 6 palettes × light/dark INI, scopes, vars, runtime loader | ✅ `SetCurrentTheme`/`SetThemeDarkMode`/`GetTheme*` | ◐ theme color slots (`KrySwSetTheme`); light/dark env knobs on hosts |
| Style tokens | ✅ radius/border/shadow/bevel; RETRO, MATERIAL, SYSTEM styles | ✅ `Get/SetThemeMetrics` | ✗ |
| Animation | ✅ transitions (smoothstep), ripple, keyframe scene anims | ✗ | ◐ `AnimNode` + `TIME` opcode |
| Text/fonts | ◐ multi-font registry, per-codepoint fallback, italic synthesis, wrap, selectable text, Android/browser IME preedit+commit; no shaping/bidi | ✅ `RegisterTextFont(Data)`, `Push/Pop/UseTextFont`, input queue | ◐ font8x8 default, or pre-baked KFA1 glyph atlas; no TTF rasterization |
| DPI/scaling | ✅ viewport-derived scale, `Scale` | ✅ `Scale`, `GetWindowScaleDPI` | ◐ per-mille UI scale (`KRB_RUN_UI_SCALE`) |
| Clipping | ✅ 16-deep scissor stack + input clip stack | ✅ `Begin/EndScissorMode` | ✅ 16-deep `clip_push/pop` in `kry_sw` |
| Z-order/popups | ✅ overlay paint pass, modal capture, input-capture stack | ◐ dropdown popups internal to `Dropdown` | ◐ dropdown menus handled by the engine |
| Input | ✅ unified front-end, pointer gestures/ownership, keyboard focus + Tab + accelerators, clipboard, backend-neutral IME composition; no gamepad nav | ✅ native host key/mouse/text queues | ◐ vtable: mouse, press, wheel, text-key queue |
| Accessibility | ◐ retained accessibility snapshots + host sink + DOM ARIA bridge; native screen-reader bridge pending | ✗ | ✗ |
| i18n | ✅ locale strings + CJK font switching; no RTL | ✗ | ✗ |
| Multi-window | ✅ `OpenNativeWindow` (X11 dlopen / SDL) | ✗ | — single framebuffer |
| 3D rendering | ✅ camera + mesh/shader tier incl. curated rlgl entry points and raymath-style math3d (GLSL 100); raylib backend implements it, others stub it | ✗ | ✗ |
| State/data binding | ✅ retained event queue; scene signals | ✅ `#ui` functions + retained events | ✅ field mounts (`KrbMount*`) + bytecode |
| Dialogs/platform | ✅ file dialogs (web/portal/gtk/zenity/kdialog/yad), XDG paths, desktop metadata packaging, single-instance lock, tray, notifications (Android/web/Linux) | ✅ tray + notify polling | ◐ host capability imports (storage/http/audio/notify) |
| Debug tooling | ✅ widget inspector, node registry + snippets | ✗ | ◐ `KryBackendRec` call-stream recording |

## Renderer backends

### Tier A — surface backends (link-time, `KRYON_BACKEND`)

| Backend | Graphics API | Platforms | CI | Notes |
|---|---|---|---|---|---|
| `raylib` (default) | OpenGL ES 2.0 over SDL2 | Linux, FreeBSD | ✅ `ci.yml` (Linux + FreeBSD) | Default everywhere; dist/static SDK targets; implements the full 3D tier (camera/mesh/shader + rlgl entry points) |
| `raylib` web | WebGL (ES2) over GLFW3 | Emscripten | ✅ web/app builds | `mk/web.mk`; present shim for WebKitGTK |
| `raylib` windows `opengl` | OpenGL 2.1 | Windows (mingw) | ✅ release builds | Default Windows flavor |
| `raylib` windows `rlsw` | raylib software renderer (no GPU) | Windows | ✅ release builds | `WINDOWS_RENDERERS` second flavor |
| `raylib` windows RGFW rule | GL 1.1 immediate mode | Windows | ✗ opt-in make rule | `KRYON_RAYLIB_WINDOWS_RULE` |
| `raylib` android | GLES via NDK `NativeActivity` | Android (downstream Gradle) | ✗ downstream apps | `mk/android.mk` |
| `null` | none (zero-return stubs) | all | ✗ (used by local headless tests) | Injected input still works |
| `canvas` | HTML5 Canvas2D via `EM_JS` (ASYNCIFY loop) | Emscripten | ✅ `make canvas-test`, `make canvas2d-parity-check`, `make web-canvas-matrix-check` | No raylib/WebGL; HiDPI Canvas2D, 2D raylib aliases, touch/gamepad/drop-file hooks, glyph atlases, textures/render textures/tinting, WebAudio, and weak stubs for non-2D raylib areas |
| `libdraw` | `kry_sw` RGBA8 presented through plan9port libdraw/devdraw | plan9port on Unix/X11 | ✅ `make libdraw-test` (devdraw smoke + `9c`/`9l` clean-surface check) | No libraylib; widgets/drawing/screenshots through software backend; TTF/TrueType-outline glyph atlases via `stb_truetype`; rounded controls, alpha/tint texture blits, rotation, render textures, plan9port input/resize; non-UI raylib areas use weak null stubs |

### Tier B — cartridge hosts (`KryBackend` vtable, runtime selection)

| Host | Rendering | Input | Textures | Text | CI | Notes |
|---|---|---|---|---|---|---|---|
| `KryBackendDraw` | raylib surface | raylib | ✅ | full font stack | ✅ | Auto-installed via constructor; hooks cartridge audio; runs inside raylib apps and `kryon-preview` |
| `kry_sw` | software RGBA8 rasterizer | injected (`KrySwMouse` etc.) | ✅ + PNG decode | font8x8 or KFA1 atlas | ✅ | No GPU/libc deps; `KrySwToRGB565` for embedded panels; one-shot dirty-rect tracking |
| `krb-run` | `kry_sw` headless | injected | ✅ | via `kry_sw` | ✅ linux/windows/macos | PNG dump + `KryBackendRec` call log; conformance host |
| `krb-sdl` | `kry_sw` → SDL2 streaming texture | SDL2 | ✅ | via `kry_sw` | ✅ | Template for the planned Android EGL host |
| `krb-web` | `kry_sw` → wasm, Canvas2D `putImageData` | JS bridge exports | ✅ | via `kry_sw` | ✅ renderers.yml | Byte-identical to native; node-capture for tests |
| `KryBackendRec` | wraps any backend | passthrough | passthrough | passthrough | ✅ exactness job | Records the vtable call stream for cross-engine comparison |
| `KryBackendNull` | stub vtable | none | ✗ (`texture_rgba` NULL) | ✗ | ✅ logic tests | Default until `KryBackendSelect` |

## Known gaps

- The `canvas` Tier A backend passes its node call-sequence gate and has
  been pixel-verified in a real browser page (rounded rects, annulus ring
  segments, dual-color gradients, and full RGBA texture tinting all check
  out). Remaining limits: cross-app clipboard paste is unreachable through
  the synchronous API (an in-app mirror handles round-trips), WebAudio
  playback depends on browser user-gesture unlock rules, and stream/mixed
  processors apply to Canvas-managed stream buffers, not decoded browser
  `AudioBuffer` playback that has already been handed to WebAudio.
- The `libdraw` Tier A backend passes the plan9port devdraw widget smoke and a
  `9c`/`9l` clean-surface compile/link check. It is intended for C UI apps:
  windowing, input, resize, software drawing, widgets, rounded controls, thick
  lines, image loading, alpha/tint and source-rectangle texture blits, image
  rotation, render textures, TTF/TrueType-outline glyph-atlas text,
  screenshots, files, and clipboard mirror are covered. Advanced raylib areas
  outside UI apps (3D, shaders, gestures, audio) are null-grade fallback.
- `k2go` targets native Go: the `Runtime` interface covers the widget
  whitelist plus the generated-code widget families in `go/kryon` (controls,
  Props widgets, dialogs, canvas, Tk layout helpers, toasts, theme control),
  with `.kry` array declarations lowering to Go slices at the use site.
  Remaining boundaries: a forward `goto` over declarations is a loud Go
  compile error, and C pointer/`Texture2D` values cannot be written (icons pass by
  `IconType`, option lists as joined strings or `[N]string`).
- `k2js` emits ESM for the web recorder runtime. `make
  k2js-runtime-snapshot-test` lowers every conformance source, imports the
  generated ESM in Node, runs `frame()`, and compares the recorded widget
  streams. `make generated-runtime-parity-test` also drives generated JS
  through the same state/input workflows as generated Go and C, then diffs the
  final state JSON. This is runtime state parity for the recorder surface, not
  browser pixel comparison.
- `k2b` drops unsupported widget calls and reports them per file; the
  cartridge widget set remains smaller than the C catalog (see matrix).
  `Dropdown` lowers to a dropdown-style control. `Progress` lowers to a read-only
  `KRB_CTRL_PROGRESS` control bound to an integer state field. `Radio` lowers
  to `KRB_CTRL_RADIO` for the common `selected == id` pattern and writes `id`
  into the mounted selection field on click. `Fieldset` lowers to border
  rectangles plus title background/text nodes.
- Retained tree paints 10 node kinds and routes interaction for 6 (see caveat
  above the widget matrix).
- No text shaping yet: no HarfBuzz, ligatures, bidi, or RTL mirroring. Android
  and DOM text controls do have backend-neutral IME preedit/commit; gamepad UI
  navigation remains absent.
- Theme styles beyond RETRO/MATERIAL/SYSTEM (fluent/adwaita/liquid-glass
  labels in app-composed theme controls) fall back to SYSTEM tokens.
- `kry_sw` tracks the dirty rectangle per call (`KrySwDirty` is one-shot);
  hosts that present full frames (`krb-sdl`, `krb-web`) do not use it yet.
- Plan 11 engines still to come: native Canvas2D fast path, Android EGL
  host, ESP32 RGB565 host. The Linux framebuffer host was removed again -
  raylib covers desktop presentation and the canvas backend covers native
  web, so it carried maintenance cost without a user.
