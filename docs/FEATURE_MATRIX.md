# Kryon Feature Matrix

What every widget and feature supports, per compile target and per renderer
backend. For how backends are selected and implemented see `docs/BACKENDS.md`;
for the cartridge binary format see `docs/KRB_FORMAT.md`; for the language see
`docs/KRY_LANGUAGE_PLAN.md`. Keep this file current with the code, and
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
   ├── k2g  -> .go (rt.* calls)            -> go build + go/kryui -> Go app
   ├── k2b  -> .krb (+ .krb.c/.krb host)   -> KrbLoad/KrbExec on any KryBackend
   └── k2ir -> .kir (text IR dump; debugging, tests, Krait)
```

| Target | Producer | Runtime | Status |
|---|---|---|---|
| C | `k2c` | `cc` + `libkryon.a` (raylib/null surface backend) | Most complete path; production use (inbe) |
| Go | `k2g` | `go/kryui` cgo package, links the same `libkryon.a` | Declarative subset; executable CI gate |
| KRB cartridge | `k2b` | `src/krb/krb.c` via the `KryBackend` vtable | Format v2; byte-exact across engines; CI-gated |
| KIR | `k2ir` | — (inspection artifact) | Debugging/tooling only |

Two backend tiers exist (see `docs/BACKENDS.md`):

- **Tier A — surface backends**: implement the ~700-symbol raylib-compatible
  surface (`include/kryon_compat.generated.h`), selected at **link** time via
  `KRYON_BACKEND=raylib|null|canvas`. The whole widget catalog runs on these.
- **Tier B — cartridge hosts**: implement the ~20-function `KryBackend` vtable
  (`include/kry_backend.h`), selected at **runtime**. Only the KRB widget
  subset runs on these.

## Widget statement whitelist (`.kry` frontend)

`parse_widget_statement` (`cmd/kir/kir_parse.c`) recognizes 30 widget names.
`k2c` compiles any library call regardless (plain call statement); `k2g` lowers
the full whitelist onto its `Runtime` interface (except `Canvas`, below), and
`k2b` lowers a subset of it:

`Background Text TextInRect Paragraph TextLines Rect Line Bevel IconTexture
Picture Button IconButton Href TextField Dropdown Slider Toggle Checkbox
Progress Column Row Stack End Scroll Canvas Modal TitleBar TabBar BottomNav
TopNav Toolbar`

(`Canvas` is whitelisted but no `Canvas(...)` widget exists — examples call
`BeginUICanvas` directly. `Scroll` lowers to the `Scroll`/`EndScroll` runtime
pairing, not the C `Begin/EndUIScrollContainer` API.)

## Widget matrix

Columns: **C** = the C API · **k2c** = `.kry`→C codegen (always equal to C) · **k2g** = `.kry`→Go codegen
(`go/kryui` `Runtime` interface) · **Go** = hand-written Go via the `go/kryui`
package API · **KRB** = lowered into a cartridge by `k2b`.

Retained-tree caveat that applies to the whole C column: `BeginUI/EndUI`
records ~29 widget kinds, but the retained painter covers only BACKGROUND, TEXT, RECT,
LINE, BUTTON, TEXT_FIELD, and routes retained input for BUTTON and TEXT_FIELD;
every other kind renders through its immediate-mode call during the
declaration pass (`src/ui/ui_tree.c`).

### UI/Display

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| Background | ✅ | ✅ | ✅ | ✅ `Background` | ✅ node |
| Text | ✅ | ✅ | ✅ | ✅ `Text` | ✅ node |
| TextInRect | ✅ | ✅ | ✅ | ✅ `TextInRect` | ✅ |
| Paragraph (rich text + inline icons) | ✅ | ✅ | ✅ | ✅ `Paragraph` | ✗ |
| TextLines | ✅ | ✅ | ✅ | ✅ `TextLines` | ✗ |
| Rect | ✅ | ✅ | ✅ (+ `RectGradientH`) | ◐ `DrawRectangle*` primitives | ✅ node |
| Line | ✅ | ✅ | ✅ | ✅ `DrawLine` | ✅ |
| Bevel | ✅ | ✅ | ✅ | ✅ `Bevel` | ✅ |
| IconTexture (114 embedded icons) | ✅ | ✅ | ✅ (by icon type) | ✅ `IconTexture` | ✗ |
| Image/Picture | ✅ | ✅ | ✅ `Picture(PictureProps)` | ✅ `Picture`/`DrawPicture` | ✅ node (embedded asset or PNG) |

### UI/Input

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| Button (ButtonProps) | ✅ | ✅ | ✅ | ✅ `Button` | ✅ node |
| GenericButton (5 styles) | ✅ | ✅ | ✗ | ✅ `GenericButton` | ◐ BUTTON style byte |
| IconButton / IconBtn / PaddedIconBtn | ✅ | ✅ | ◐ `IconButton` only | ◐ `IconButton` only | ✗ |
| TextButton | ✅ | ✅ | ✗ | ✅ `TextButton` | ✗ |
| InfoButton | ✅ | ✅ | ✗ | ✗ | ✗ |
| Href (hyperlink) / IconLink | ✅ | ✅ | ◐ `Href` only | ◐ `Href` only | ✗ |
| TextInputControl | ✅ | ✅ | ✗ | ✅ `TextInputControl` | ✗ |
| TextField | ✅ | ✅ | ✅ | ✅ `TextField`/`NewTextField`/`NewPasswordField` | ✅ TEXTINPUT node |
| ReadonlyTextBox | ✅ | ✅ | ✗ | ✅ `ReadonlyTextBox` | ✗ |
| TextArea (selection, syntax highlight) | ✅ | ✅ | ✗ | ✅ `NewTextArea`/`TextArea` | ✗ |
| Dropdown / DropdownEx / LocaleDropdown | ✅ | ✅ | ✗ | ✅ `Dropdown(Ex)`/`LocaleDropdown` | ✅ DROPDOWN control |
| Slider | ✅ | ✅ | ✅ | ✅ `Slider`/`Slider` | ✅ SLIDER control |
| VerticalSlider / WithMarks | ✅ | ✅ | ✗ | ◐ `VerticalSlider` only | ✅ VSLIDER control |
| Toggle (switch) | ✅ | ✅ | ✅ | ✅ `Toggle` | ✅ node |
| Checkbox (+ disabled) | ✅ | ✅ | ✅ | ✅ `Checkbox` | ✅ node |
| Radio | ✅ | ✅ | ✗ | ✅ `Radio` | ✗ |
| Progress | ✅ | ✅ | ✅ | ✅ `Progress` | ✗ |
| Spinbox | ✅ | ✅ | ✗ | ✅ `Spinbox` | ✅ SPINBOX control |
| Combobox | ✅ | ✅ | ✗ | ✅ `Combobox` | ✅ COMBOBOX control (renders like the dropdown, mirroring the C widget) |
| ColorPicker (RGB sliders) | ✅ | ✅ | ✗ | ✗ | ✗ |

### UI/Layout

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| Column / Row / Stack (flex-like) | ✅ | ✅ | ✅ all three | ✅ all three | ✅ structural no-ops (node table is the tree) |
| Group | ✅ (lowers to Stack) | ✅ | ◐ via Column | ✅ via Stack | ✅ |
| Scroll container | ✅ | ✅ | ✅ `Scroll`/`EndScroll` | ✅ `Begin/EndScrollContainer` | ✅ SCROLL node |
| Separator | ✅ | ✅ | ✗ | ✗ | ✅ |
| LabelFrame | ✅ | ✅ | ✗ | ✅ `LabelFrame` | ✗ |
| Notebook (tabs) | ✅ | ✅ | ✗ | ✅ `Notebook` | ✗ |
| PanedView (splitter) | ✅ | ✅ | ✗ | ✅ `PanedView` | ✗ |
| Collapsible | ✅ | ✅ | ✗ | ✅ `Collapsible` | ✗ |
| Tk pack/grid helpers (`UIFramePack`, `UIGridCell`, `UIPlace`) | ✅ | ✅ | ✗ | ✅ | ✗ |
| Canvas (pan/zoom, hit-test, grid) | ✅ | ✅ | ✗ | ✅ `Begin/EndUICanvas`+hit-test | ✗ |

### UI/Collections

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| ListBox | ✅ | ✅ | ✗ | ✅ `ListBox` | ✗ |
| TreeView / CascadingTreeView | ✅ | ✅ | ✗ | ✗ | ✗ |
| SourceView (code + line numbers) | ✅ | ✅ | ✗ | ✅ `SourceView` | ✗ |
| TableView (sortable) | ✅ | ✅ | ✗ | ✅ `TableView` | ✗ |
| CanvasGrid | ✅ | ✅ | ✗ | ✅ `CanvasGrid` | ✗ |
| SelectableText | ✅ | ✅ | ✗ | ✅ `SelectableText` | ✗ |

### UI/Navigation

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| MenuBar / PopupMenu / ContextMenu | ✅ | ✅ | ✗ | ✗ | ✗ |
| TabBar | ✅ | ✅ | ✅ | ✅ `TabBar` | ✗ |
| SubtabBar / PaneTabBar (dock zones) | ✅ | ✅ | ✗ | ✗ | ✗ |
| BottomNav (+ config modal) | ✅ | ✅ | ✅ | ✅ `BottomNav` | ◐ `NavButton` lowers to a BUTTON |
| TopNav / Toolbar / ToolbarHeader | ✅ | ✅ | ◐ `TopNav`/`Toolbar` only | ✅ `TopNav`/`Toolbar` | ✗ |
| TitleBar family | ✅ | ✅ | ◐ `TitleBar` only | ✅ `TitleBar` | ✗ |

### UI/Overlays

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| ActionModal / Modal / Modal3Button / ModalFrame | ✅ | ✅ | ◐ `Modal` only | ◐ `Modal` only | ✗ |
| MessageDialog / ConfirmDialog / PromptDialog | ✅ | ✅ | ✗ | ✅ | ✗ |
| Toast | ✅ | ✅ | ✗ | ✅ `ShowToast(For)` | ✗ |
| GuideOverlay / TutorialImage(Placeholder) | ✅ | ✅ | ✗ | ✗ | ✗ |
| TransitionFade / Focus ring | ✅ | ✅ | ✗ | ✗ | ◐ `AnimNode` + `TIME` opcode drive animation |
| ThemeSettings / ThemeSwitcher / ThemePicker | ✅ | ✅ | ✗ | ◐ `SetCurrentTheme`/`SetThemeStyle` (control only) | ✗ |
| FocusDebugOverlay | ✅ | ✅ | ✗ | ✗ | ✗ |
| InfoRows / OverlayButton / IconSliderPopup | ✅ | ✅ | ✗ | ✗ | ✗ |

### UI/Composite (app-level, Inbe-flavored)

| Widget | C | k2c | k2g | Go | KRB |
|---|---|---|---|---|---|
| LabelTextField / SectionLabel | ✅ | ✅ | ✗ | ✗ | ✗ |
| CheckboxRow / ButtonRow / BottomIconRow | ✅ | ✅ | ✗ | ✗ | ✗ |
| BottomNavConfig | ✅ | ✅ | ✗ | ✗ | ✗ |
| SidebarAccountHeader / ProfilePicturePicker | ✅ | ✅ | ✗ | ✗ | ✗ |
| Reorder (drag handle + placeholder) | ✅ | ✅ | ✗ | ✗ | ✗ |
| ImageBox | ✅ | ✅ | ✗ | ✗ | ✗ |

### Game2D (scene tree, `src/scene/`)

| Node | C | ✅ | k2g | Go | KRB |
|---|---|---|---|---|---|
| Scene / Node2D / Camera2D | ✅ | ✅ | ✗ | ✗ | ✗ |
| Sprite2D / AnimatedSprite2D / TileMap / TileLayer | ✅ | ✅ | ✗ | ✗ | ✗ |
| CollisionShape2D / Area2D / Body2D (box2d) | ✅ | ✅ | ✗ | ✗ | ✗ |
| Timer / AudioSource / AnimationPlayer | ✅ | ✅ | ✗ | ✗ | ✗ |
| Signals (`KrySignal`) / keyframe animation | ✅ | ✅ | ✗ | ✗ | ✗ |

### KRB cartridge vocabulary (no C widget equivalent)

| Item | Notes |
|---|---|
| DATA node | state-field mount read/written by bytecode |
| CIRCLE / RING nodes | lowered from `DrawCircleV` / `DrawRing` calls |
| `NavButton` | `k2b`-only lowering; becomes a BUTTON with `KRB_FLAG_NAV` |
| `AnimNode` | `k2b`-only; animated node driven by `TIME` |
| v2 logic bytecode | stack machine, node mutation, if-guards over node ranges |
| Host capabilities | imported storage/http/audio/notify calls (`krb_caps.c`) |

## Cross-cutting features

| Feature | C | Go | KRB |
|---|---|---|---|
| Theming | ✅ 6 palettes × light/dark INI, scopes, vars, runtime loader | ✅ `SetCurrentTheme`/`SetThemeDarkMode`/`GetTheme*` | ◐ theme color slots (`KrySwSetTheme`); light/dark env knobs on hosts |
| Style tokens | ✅ radius/border/shadow/bevel; RETRO, MATERIAL, SYSTEM styles | ✅ `Get/SetUIStyleTokens` | ✗ |
| Animation | ✅ transitions (smoothstep), ripple, keyframe scene anims | ✗ | ◐ `AnimNode` + `TIME` opcode |
| Text/fonts | ✅ multi-font registry, per-codepoint fallback, italic synthesis, wrap, selectable text; no shaping/bidi/IME | ✅ `RegisterUIFont(Data)`, `Push/Pop/UseUIFont`, input queue | ◐ font8x8 default, or pre-baked KFA1 glyph atlas; no TTF rasterization |
| DPI/scaling | ✅ viewport-derived scale, `ScaleUIPx` | ✅ `ScaleUIPx`, `GetWindowScaleDPI` | ◐ per-mille UI scale (`KRB_RUN_UI_SCALE`) |
| Clipping | ✅ 16-deep scissor stack + input clip stack | ✅ `Begin/EndScissorMode` | ✅ 16-deep `clip_push/pop` in `kry_sw` |
| Z-order/popups | ✅ overlay paint pass, modal capture, input-capture stack | ◐ dropdown popups internal to `Dropdown` | ◐ dropdown menus handled by the engine |
| Input | ✅ unified front-end, pointer gestures/ownership, keyboard focus + Tab + accelerators, clipboard; no IME, no gamepad nav | ✅ key/mouse/char APIs, `QueueUITextInput*` | ◐ vtable: mouse, press, wheel, text-key queue |
| Accessibility | ◐ debug focus overlay + a11y node structs; no screen-reader bridge | ✗ | ✗ |
| i18n | ✅ locale strings + CJK font switching; no RTL | ✗ | ✗ |
| Multi-window | ✅ `OpenUIWindow` (X11 dlopen / SDL) | ✗ | — single framebuffer |
| State/data binding | ✅ retained event queue; scene signals | ✅ `Begin/EndUI` + `NextUIEvent` | ✅ field mounts (`KrbMount*`) + bytecode |
| Dialogs/platform | ✅ file dialogs (web/gtk/zenity/kdialog/yad), tray, notifications (Android/web/Linux) | ✅ tray + notify polling | ◐ host capability imports (storage/http/audio/notify) |
| Debug tooling | ✅ widget inspector, node registry + snippets | ✗ | ◐ `KryBackendRec` call-stream recording |

## Renderer backends

### Tier A — surface backends (link-time, `KRYON_BACKEND`)

| Backend | Graphics API | Platforms | CI | Notes |
|---|---|---|---|---|
| `raylib` (default) | OpenGL ES 2.0 over SDL2 | Linux, FreeBSD | ✅ `ci.yml` (Linux + FreeBSD) | Default everywhere; dist/static SDK targets |
| `raylib` web | WebGL (ES2) over GLFW3 | Emscripten | ✅ netlify/app builds | `mk/web.mk`; present shim for WebKitGTK |
| `raylib` windows `opengl` | OpenGL 2.1 | Windows (mingw) | ✅ release builds | Default Windows flavor |
| `raylib` windows `rlsw` | raylib software renderer (no GPU) | Windows | ✅ release builds | `WINDOWS_RENDERERS` second flavor |
| `raylib` windows RGFW rule | GL 1.1 immediate mode | Windows | ✗ opt-in make rule | `KRYON_RAYLIB_WINDOWS_RULE` |
| `raylib` android | GLES via NDK `NativeActivity` | Android (downstream Gradle) | ✗ downstream apps | `mk/android.mk` |
| `null` | none (zero-return stubs) | all | ✗ (used by local headless tests) | Injected input still works |
| `canvas` | HTML5 Canvas2D via `EM_JS` (ASYNCIFY loop) | Emscripten | ✅ `make canvas-test` (node call-sequence gate) | No raylib; glyphs rasterized from FontFace data; rounded rects/rings/clipboard approximated |

### Tier B — cartridge hosts (`KryBackend` vtable, runtime selection)

| Host | Rendering | Input | Textures | Text | CI | Notes |
|---|---|---|---|---|---|---|
| `KryBackendDraw` | raylib surface | raylib | ✅ | full font stack | ✅ | Auto-installed via constructor; hooks cartridge audio; runs inside raylib apps and `kryon-preview` |
| `kry_sw` | software RGBA8 rasterizer | injected (`KrySwMouse` etc.) | ✅ + PNG decode | font8x8 or KFA1 atlas | ✅ | No GPU/libc deps; `KrySwToRGB565` for embedded panels; one-shot dirty-rect tracking |
| `krb-run` | `kry_sw` headless | injected | ✅ | via `kry_sw` | ✅ linux/windows/macos | PNG dump + `KryBackendRec` call log; conformance host; `--backend fb` presents on `/dev/fb0` (or a raw file target) with dirty-rect conversion |
| `krb-sdl` | `kry_sw` → SDL2 streaming texture | SDL2 | ✅ | via `kry_sw` | ✅ | Template for the planned Android EGL host |
| `krb-web` | `kry_sw` → wasm, Canvas2D `putImageData` | JS bridge exports | ✅ | via `kry_sw` | ✅ renderers.yml | Byte-identical to native; node-capture for tests |
| `KryBackendRec` | wraps any backend | passthrough | passthrough | passthrough | ✅ exactness job | Records the vtable call stream for cross-engine comparison |
| `KryBackendNull` | stub vtable | none | ✗ (`texture_rgba` NULL) | ✗ | ✅ logic tests | Default until `KryBackendSelect` |

## Known gaps

- The `canvas` Tier A backend exists and passes its node call-sequence
  gate, but is young: rounded rectangles and ring segments are
  approximated, the clipboard is not bridged, tint is alpha-only, and it
  has not yet been exercised in a real browser page.
- `k2g`'s `Runtime` interface covers the full widget whitelist and lowers
  typed declarations, arrays, and goto/labels. Remaining boundaries: a
  forward `goto` over declarations is a loud Go compile error (not silently
  miscompiled), and values of C pointer/`Texture2D` type cannot be expressed
  from `.kry` (icons pass by `UIIconType`, option lists as `;`-joined strings
  or `[N]string` arrays).
- `k2b` drops unsupported widget calls and reports them per file; the
  cartridge widget set remains smaller than the C catalog (see matrix).
  `Combobox` lowers to a `KRB_CTRL_COMBOBOX` control that renders like the
  dropdown, matching the C widget.
- Retained tree paints only 5 node kinds and routes input for 2 (see caveat
  above the widget matrix).
- No text shaping: no HarfBuzz, ligatures, bidi, or RTL mirroring anywhere;
  no IME/preedit; no gamepad UI navigation.
- Theme styles beyond RETRO/MATERIAL/SYSTEM (fluent/adwaita/liquid-glass/aero
  labels in `ThemeSettingsProps`) fall back to SYSTEM tokens.
- `kry_sw` tracks the dirty rectangle per call (`KrySwDirty` is one-shot);
  hosts that present full frames (`krb-sdl`, `krb-web`) do not use it yet.
- Plan 11 engines still to come: native Canvas2D fast path, Android EGL
  host, ESP32 RGB565 host. The phase-2 Linux framebuffer host is done:
  `krb-run --backend fb` presents on `/dev/fb0` (or a raw file target for
  headless testing) with per-frame dirty-rect conversion.

Plan 11 (`docs/plans/11-render-engines.md`) drafts further engines — Linux
framebuffer, Android EGL, ESP32 RGB565, native Canvas2D fast path — none
implemented yet.
