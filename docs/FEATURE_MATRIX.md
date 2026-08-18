# Kryon Feature Matrix

What every widget and feature supports, per compile target and per renderer
backend. For how backends are selected and implemented see `docs/BACKENDS.md`;
for the cartridge binary format see `docs/KRB_FORMAT.md`; for the language see
`docs/KRY_LANGUAGE_PLAN.md`.

Legend: ✅ available · ◐ partial (reason given) · ✗ not available · — not
applicable. "C" means the `libkryon.a` C API, which `.kry` reaches in full
through `k2c` (the language is C-close; every library call is a legal
statement, the widget-statement whitelist below only adds sugar).

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

Columns: **C** = C API and `.kry`→`k2c` · **k2g** = `.kry`→Go codegen
(`go/kryui` `Runtime` interface) · **Go** = hand-written Go via the `go/kryui`
package API · **KRB** = lowered into a cartridge by `k2b`.

Retained-tree caveat that applies to the whole C column: `BeginUI/EndUI`
records ~29 widget kinds, but `DrawUITree` paints only BACKGROUND, TEXT, RECT,
LINE, BUTTON, TEXT_FIELD, and routes retained input for BUTTON and TEXT_FIELD;
every other kind renders through its immediate `DrawUI*` call during the
declaration pass (`src/ui/ui_tree.c`).

### UI/Display

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| Background | ✅ | ✅ | ✅ `Background` | ✅ node |
| Text | ✅ | ✅ | ✅ `Text`/`DrawUIText` | ✅ node |
| TextInRect | ✅ | ✅ | ✅ `TextInRect` | ✅ |
| Paragraph (rich text + inline icons) | ✅ | ✅ | ✅ `Paragraph` | ✗ |
| TextLines | ✅ | ✅ | ✅ `TextLines` | ✗ |
| Rect | ✅ | ✅ (+ `RectGradientH`) | ◐ `DrawRectangle*` primitives | ✅ node |
| Line | ✅ | ✅ | ✅ `DrawLine` | ✅ |
| Bevel | ✅ | ✅ | ✅ `Bevel` | ✅ |
| IconTexture (114 embedded icons) | ✅ | ✅ (by icon type) | ✅ `IconTexture` | ✗ |
| Image/Picture | ✅ | ✅ `Picture(PictureProps)` | ✅ `Picture`/`DrawPicture` | ✅ node (embedded asset or PNG) |

### UI/Input

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| Button (ButtonProps) | ✅ | ✅ | ✅ `DrawUIButton` | ✅ node |
| GenericButton (5 styles) | ✅ | ✗ | ✅ `DrawUIGenericButton` | ◐ BUTTON style byte |
| IconButton / IconBtn / PaddedIconBtn | ✅ | ◐ `IconButton` only | ◐ `DrawUIIconButton` only | ✗ |
| TextButton | ✅ | ✗ | ✅ `DrawUITextButton` | ✗ |
| InfoButton | ✅ | ✗ | ✗ | ✗ |
| Href (hyperlink) / IconLink | ✅ | ◐ `Href` only | ◐ `DrawUIHref` only | ✗ |
| TextInputControl | ✅ | ✗ | ✅ `DrawUITextInputControl` | ✗ |
| TextField | ✅ | ✅ | ✅ `TextField`/`NewTextField`/`NewPasswordField` | ✅ TEXTINPUT node |
| ReadonlyTextBox | ✅ | ✗ | ✅ `DrawUIReadonlyTextBox` | ✗ |
| TextArea (selection, syntax highlight) | ✅ | ✗ | ✅ `NewTextArea`/`DrawUITextArea` | ✗ |
| Dropdown / DropdownEx / LocaleDropdown | ✅ | ✗ | ✅ `DrawUIDropdown(Ex)`/`DrawUILocaleDropdown` | ✅ DROPDOWN control |
| Slider | ✅ | ✅ | ✅ `DrawUISlider`/`Slider` | ✅ SLIDER control |
| VerticalSlider / WithMarks | ✅ | ✗ | ◐ `DrawUIVerticalSlider` only | ✅ VSLIDER control |
| Toggle (switch) | ✅ | ✅ | ✅ `Toggle` | ✅ node |
| Checkbox (+ disabled) | ✅ | ✅ | ✅ `DrawUICheckboxToggle` | ✅ node |
| Radio | ✅ | ✗ | ✅ `Radio` | ✗ |
| Progress | ✅ | ✅ | ✅ `Progress` | ✗ |
| Spinbox | ✅ | ✗ | ✅ `Spinbox` | ✅ SPINBOX control |
| Combobox | ✅ | ✗ | ✅ `Combobox` | ◐ format control exists; `k2b` lowers no `Combobox` call |
| ColorPicker (RGB sliders) | ✅ | ✗ | ✗ | ✗ |

### UI/Layout

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| Column / Row / Stack (flex-like) | ✅ | ✅ all three | ✅ all three | ✅ structural no-ops (node table is the tree) |
| Group | ✅ (lowers to Stack) | ◐ via Column | ✅ via Stack | ✅ |
| Scroll container | ✅ | ✅ `Scroll`/`EndScroll` | ✅ `Begin/EndScrollContainer` | ✅ SCROLL node |
| Separator | ✅ | ✗ | ✗ | ✅ |
| LabelFrame | ✅ | ✗ | ✅ `LabelFrame` | ✗ |
| Notebook (tabs) | ✅ | ✗ | ✅ `Notebook` | ✗ |
| PanedView (splitter) | ✅ | ✗ | ✅ `PanedView` | ✗ |
| Collapsible | ✅ | ✗ | ✅ `Collapsible` | ✗ |
| Tk pack/grid helpers (`UIFramePack`, `UIGridCell`, `UIPlace`) | ✅ | ✗ | ✅ | ✗ |
| Canvas (pan/zoom, hit-test, grid) | ✅ | ✗ | ✅ `Begin/EndUICanvas`+hit-test | ✗ |

### UI/Collections

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| ListBox | ✅ | ✗ | ✅ `ListBox` | ✗ |
| TreeView / CascadingTreeView | ✅ | ✗ | ✗ | ✗ |
| SourceView (code + line numbers) | ✅ | ✗ | ✅ `SourceView` | ✗ |
| TableView (sortable) | ✅ | ✗ | ✅ `TableView` | ✗ |
| CanvasGrid | ✅ | ✗ | ✅ `CanvasGrid` | ✗ |
| SelectableText | ✅ | ✗ | ✅ `SelectableText` | ✗ |

### UI/Navigation

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| MenuBar / PopupMenu / ContextMenu | ✅ | ✗ | ✗ | ✗ |
| TabBar | ✅ | ✅ | ✅ `TabBar` | ✗ |
| SubtabBar / PaneTabBar (dock zones) | ✅ | ✗ | ✗ | ✗ |
| BottomNav (+ config modal) | ✅ | ✅ | ✅ `BottomNav` | ◐ `NavButton` lowers to a BUTTON |
| TopNav / Toolbar / ToolbarHeader | ✅ | ◐ `TopNav`/`Toolbar` only | ✅ `TopNav`/`Toolbar` | ✗ |
| TitleBar family | ✅ | ◐ `TitleBar` only | ✅ `TitleBar` | ✗ |

### UI/Overlays

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| ActionModal / Modal / Modal3Button / ModalFrame | ✅ | ◐ `Modal` only | ◐ `Modal` only | ✗ |
| MessageDialog / ConfirmDialog / PromptDialog | ✅ | ✗ | ✅ | ✗ |
| Toast | ✅ | ✗ | ✅ `ShowToast(For)` | ✗ |
| GuideOverlay / TutorialImage(Placeholder) | ✅ | ✗ | ✗ | ✗ |
| TransitionFade / Focus ring | ✅ | ✗ | ✗ | ◐ `AnimNode` + `TIME` opcode drive animation |
| ThemeSettings / ThemeSwitcher / ThemePicker | ✅ | ✗ | ◐ `SetCurrentTheme`/`SetThemeStyle` (control only) | ✗ |
| FocusDebugOverlay | ✅ | ✗ | ✗ | ✗ |
| InfoRows / OverlayButton / IconSliderPopup | ✅ | ✗ | ✗ | ✗ |

### UI/Composite (app-level, Inbe-flavored)

| Widget | C | k2g | Go | KRB |
|---|---|---|---|---|
| LabelTextField / SectionLabel | ✅ | ✗ | ✗ | ✗ |
| CheckboxRow / ButtonRow / BottomIconRow | ✅ | ✗ | ✗ | ✗ |
| BottomNavConfig | ✅ | ✗ | ✗ | ✗ |
| SidebarAccountHeader / ProfilePicturePicker | ✅ | ✗ | ✗ | ✗ |
| Reorder (drag handle + placeholder) | ✅ | ✗ | ✗ | ✗ |
| ImageBox | ✅ | ✗ | ✗ | ✗ |

### Game2D (scene tree, `src/scene/`)

| Node | C | k2g | Go | KRB |
|---|---|---|---|---|
| Scene / Node2D / Camera2D | ✅ | ✗ | ✗ | ✗ |
| Sprite2D / AnimatedSprite2D / TileMap / TileLayer | ✅ | ✗ | ✗ | ✗ |
| CollisionShape2D / Area2D / Body2D (box2d) | ✅ | ✗ | ✗ | ✗ |
| Timer / AudioSource / AnimationPlayer | ✅ | ✗ | ✗ | ✗ |
| Signals (`KrySignal`) / keyframe animation | ✅ | ✗ | ✗ | ✗ |

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
| Z-order/popups | ✅ overlay paint pass, modal capture, input-capture stack | ◐ dropdown popups internal to `DrawUIDropdown` | ◐ dropdown menus handled by the engine |
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
| `canvas` | HTML5 Canvas2D | Web | ✗ | **Documented but not implemented** — `src/backend/canvas_backend.c` does not exist; build fails if selected |

### Tier B — cartridge hosts (`KryBackend` vtable, runtime selection)

| Host | Rendering | Input | Textures | Text | CI | Notes |
|---|---|---|---|---|---|---|
| `KryBackendDraw` | raylib surface | raylib | ✅ | full font stack | ✅ | Auto-installed via constructor; hooks cartridge audio; runs inside raylib apps and `kryon-preview` |
| `kry_sw` | software RGBA8 rasterizer | injected (`KrySwMouse` etc.) | ✅ + PNG decode | font8x8 or KFA1 atlas | ✅ | No GPU/libc deps; `KrySwToRGB565` for embedded panels; dirty-rect = full frame |
| `krb-run` | `kry_sw` headless | injected | ✅ | via `kry_sw` | ✅ linux/windows/macos | PNG dump + `KryBackendRec` call log; conformance host |
| `krb-sdl` | `kry_sw` → SDL2 streaming texture | SDL2 | ✅ | via `kry_sw` | ✅ | Template for the planned Android EGL host |
| `krb-web` | `kry_sw` → wasm, Canvas2D `putImageData` | JS bridge exports | ✅ | via `kry_sw` | ✅ renderers.yml | Byte-identical to native; node-capture for tests |
| `KryBackendRec` | wraps any backend | passthrough | passthrough | passthrough | ✅ exactness job | Records the vtable call stream for cross-engine comparison |
| `KryBackendNull` | stub vtable | none | ✗ (`texture_rgba` NULL) | ✗ | ✅ logic tests | Default until `KryBackendSelect` |

## Known gaps

- `canvas` Tier A backend is selectable and documented but has no
  implementation; the working web path is `krb-web` (`kry_sw` → wasm).
- `k2g`'s `Runtime` interface covers the full widget whitelist, but the
  imperative surface is still a subset: `goto`, labels, and local array
  declarations emit TODO comments that fail the build loudly, and values of
  C pointer/`Texture2D` type cannot be expressed from `.kry` (icons are
  passed by `UIIconType`, string lists as `;`-joined strings).
- `k2b` drops unsupported widget calls and reports them per file; the cartridge
  widget set is much smaller than the C catalog (see matrix).
- `KRB_CTRL_COMBOBOX` exists in the format but no `k2b` lowering produces it.
- Retained tree paints only 5 node kinds and routes input for 2 (see caveat
  above the widget matrix).
- No text shaping: no HarfBuzz, ligatures, bidi, or RTL mirroring anywhere; no
  IME/preedit; no gamepad UI navigation.
- Theme styles beyond RETRO/MATERIAL/SYSTEM (fluent/adwaita/liquid-glass/aero
  labels in `ThemeSettingsProps`) fall back to SYSTEM tokens.
- `kry_sw` reports the full frame dirty; per-call dirty tracking is deferred to
  the framebuffer host (plan 11, phase 2).

Plan 11 (`docs/plans/11-render-engines.md`) drafts further engines — Linux
framebuffer, Android EGL, ESP32 RGB565, native Canvas2D fast path — none
implemented yet.
