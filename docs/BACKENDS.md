# Kryon Backends

How the graphics/input backend is selected, what a backend must implement, and
how to add a new one. For the machine-readable backend inventory see
`docs/BACKEND_CAPABILITIES.json`; for the operational guide see
`docs/AGENTS.md`; for the public API see `docs/API.md`; for what runs where
see `docs/FEATURE_MATRIX.md`.

## The surface and how selection works

Kryon's public graphics/input API is `include/kryon_compat.generated.h`:
kryon owns the surface, and it currently tracks raylib's header verbatim so
raylib-style code compiles unchanged. A backend is a translation unit (or set
of them) that defines the surface's functions. No `raylib.h` include and no
raylib build path may appear in `include/`, `src/`, `examples/`, or `tests/`
(`make kryon-boundary-check`).

Backend selection is link-time, via the `KRYON_BACKEND` make variable:

- `raylib` (default) - generated forwarders (`build/generated/
  kryon_raylib_wrappers.c`) call into raylib, which is compiled with every
  symbol renamed `KryonRaylibBackend_*` (`build/generated/
  raylib_backend_rename.h`, applied in `mk/raylib.mk`). Applications can never
  bind raylib symbols directly.
- `null` - generated zero-return stubs (`build/generated/kryon_null_backend.c`)
  so the library links and runs headless with no GPU or window. Injected input
  (below) still works, which is what drives headless widget tests.
- `canvas` - HTML5 Canvas2D backend, no raylib (`src/backend/canvas_*.c`,
  split by concern: window, input, draw, texture, text, os, audio).
  Web-only (native sweeps of its sources compile to empty translation
  units; a native canvas link fails at symbol resolution). Implements the
  required surface through `EM_JS`: draws as canvas paths (rounded rects via
  `roundRect` with an `arcTo` fallback, rings as filled even-odd annulus
  segments, gradients carry both full colors), textures as offscreen canvases
  (`putImageData`/`drawImage` are synchronous — no async ImageBitmap on the
  draw path), full RGBA tinting through cached multiply + destination-in
  composites per (texture, tint), text as glyph atlases rasterized from
  `FontFace` data, and input via the `KryonBackendRaw_*` hooks fed by JS
  event listeners. Audio uses WebAudio for decoded sound/music buffers,
  scheduled PCM streams, stream callbacks, and stream/mixed processors on
  Canvas-managed stream buffers; wave export writes synchronously to MEMFS.
  Encoded loaders rely on browser `decodeAudioData`, so Canvas web links use
  `-sASYNCIFY`, and playback still follows normal browser user-gesture
  unlock rules. The app's `while (!WindowShouldClose())` loop works
  because the build uses `-sASYNCIFY` and the frame end yields with
  `emscripten_sleep`. The clipboard keeps a mirror of the last written text
  (copy/paste round-trips in-app; the browser write is attempted
  fire-and-forget — cross-app paste stays out of reach of the sync API).
  `make canvas-test` builds a full app with this backend under emcc and
  verifies the draw-call sequence in node (skips when emcc is absent); `make
  canvas2d-parity-check` guards the no-WebGL 2D/browser subset, and `make
  web-canvas-matrix-check` captures generated-C visual fixtures through the
  same backend. `docs/CANVAS2D_PARITY.md` is the Canvas2D parity ledger.
  The rendering has additionally been pixel-verified in a real browser page.
- `libdraw` - plan9port libdraw/devdraw backend, no raylib
  (`src/backend/libdraw_*.c`). It keeps the same public surface as the other
  backends: C apps still include `kryon.h` and call `InitWindow`,
  `BeginDrawing`, `DrawRectangle`, `Button`, `TextField`, and the rest of
  Kryon's normal API. Internally, the backend renders through `kry_sw` into an
  RGBA buffer and presents it with libdraw, so the widget catalog shares the
  same software drawing behavior as the KRB hosts. TTF/TrueType-outline font
  data is rasterized into Kryon `Texture2D` atlases with `stb_truetype`
  (header-only; no `libraylib` link). Plan9port owns the window, event,
  keyboard, mouse, resize, and devdraw connection. Non-UI raylib areas such as
  3D, shaders, gestures, and audio fall back to the generated weak null stubs
  until a real libdraw implementation is added for them. `make libdraw-test`
  builds and runs
  a widget smoke app under plan9port devdraw (Xvfb when available),
  pixel-verifies rounded widgets, thick lines, source-rectangle texture blits,
  alpha blending, rotated texture blits, render textures, TTF glyph atlas
  coverage, and screenshot export, and compiles/links a clean-surface C app
  through plan9port `9c`/`9l`.
- `termi` - terminal-cell backend, no raylib (`src/backend/termi_*.c`). It
  keeps the normal Kryon surface and widget API: apps still call
  `InitWindow`, `BeginDrawing`, `DrawRectangle`, `Button`, `TextField`, and
  `DrawUIText`-using widgets. The backend maps Kryon's pixel layout units onto
  terminal cells (`TERMI_CELL_WIDTH` x `TERMI_CELL_HEIGHT`) and renders native
  terminal primitives: rectangles become colored cell regions, rectangle
  outlines become terminal line cells, and UI text goes through the native text
  hook to real terminal text. PNG and other stb-supported images are decoded
  into Kryon textures; texture draws render as a colored-cell fallback, and
  sixel image data is emitted only when the terminal advertises sixel support
  or `TERMI_SIXEL=1` is set (`TERMI_SIXEL=0` disables sixel explicitly). It uses the
  generated weak null stubs for unsupported raylib compatibility areas such as
  shaders and 3D. Input is read from the terminal in raw mode, including arrows,
  basic mouse SGR events, and Ctrl-C as an application close request. `make
  termi-test` builds a widget smoke app with `KRYON_BACKEND=termi`, runs it in
  a pseudo-terminal, verifies alternate screen entry/exit, rendered widget text,
  sixel texture output, and confirms Ctrl-C exits.

Exactly one backend TU is compiled into `libkryon.a` (root `Makefile`), and
`KRYON_BACKEND_LIBS`/`KRYON_BACKEND_LDLIBS` carry the backend's own link
inputs (only raylib needs `libraylib.a` and its SDL/GL system libs). The
downstream fragments (`mk/common.mk`, `mk/native.mk`, `mk/web.mk`) honor the
same variable, so `make KRYON_BACKEND=null` produces binaries with no raylib
in the link graph, `make KRYON_BACKEND=termi` links a terminal backend, and
`make KRYON_BACKEND=libdraw PLAN9PORT_DIR=/path/to/plan9port` links against
plan9port instead. The
cross-build dist targets (`dist-linux`, `dist-windows`, Android, and `make
dist-static`, whose pkg-config/cmake manifests hardcode raylib) are raylib-only
today.

## The shared input front-end

Input is not the backend's job beyond polling. `src/backend/kry_input.c`
defines the public input queries (`IsKeyPressed`, `IsKeyDown`,
`IsMouseButton*`, `GetMouse*`, `GetCharPressed`, `GetKeyPressed`, ...) once for
every backend, merging, in order:

1. synthetic input injection (`kry_inject.h` - headless tests and the
   synthetic file system),
2. the keyboard enable/disable switch and keyboard platform callbacks
   (`SetKeyboardInputEnabled`, `SetKeyPlatformCallbacks` - non-raylib key
   sources such as an IME),
3. the modal input override (`BeginKryonInputOverride`/`EndKryonInputOverride`
   - offscreen windows stealing the pointer),
4. the backend's raw poll, via the `KryonBackendRaw_*` hooks declared in
   `include/kry_input.h`.

A backend implements only those raw hooks; it gets injection, overrides, and
platform-key routing for free. The generated raylib wrappers forward the
hooks; the generated null backend answers them with zeros.

## Shared surface functions

Pure-math surface symbols - `Fade`, `ColorLerp`, `CheckCollisionPointRec`,
`CheckCollisionRecs`, `GetWorldToScreen2D`, `GetScreenToWorld2D`,
`GetCodepointNext` - are backend-independent arithmetic (colors, rectangles,
the Camera2D transform, UTF-8 decoding) and are defined once for every
backend in `src/backend/kry_surface_math.c`. The generator skips them in both
generated files; a new backend implements none of them. Without this the null
backend's zero-return stubs would break pointer hit-testing, since
`ui_mouse_world()` transforms the mouse position through
`GetScreenToWorld2D` before any click test.

## What a backend must implement

The full surface is ~700 symbols, but kryon's own code, the tooling, and
generated `.kry` apps call a much smaller subset. `sh
tools/backend-required-symbols.sh` prints the exact list from current call
sites (~100 symbols); keep it in sync when adding surface usage. Grouped:

- **Window/frame**: `InitWindow`, `CloseWindow`, `WindowShouldClose`,
  `IsWindowReady`, `SetConfigFlags`, `SetTargetFPS`, `SetWindowSize`,
  `BeginDrawing`, `EndDrawing`, `GetScreenWidth/Height`, `GetFrameTime`,
  `GetTime`, `GetWindowScaleDPI`, `SetTraceLogLevel`, `SetMouseCursor`.
- **Input**: the `KryonBackendRaw_*` hooks above only.
- **Drawing (immediate mode)**: `ClearBackground`, `DrawRectangle*`,
  `DrawCircle*`, `DrawLine*`, `DrawRing`, `DrawTriangle`, `DrawTexture`,
  `DrawTexturePro`, `DrawText` (default font), `BeginScissorMode`/
  `EndScissorMode`, `BeginMode2D`/`EndMode2D` (scene camera). Callers assume
  the backend batches internally (raylib/rlgl does); a backend without
  batching is correct but slow.
- **Text**: `LoadFontFromMemory` (TTF with an explicit codepoint set),
  `UnloadFont`, `GetFontDefault`, `GetGlyphInfo`, `GetGlyphAtlasRec`,
  `GetGlyphIndex`, `SetTextureFilter` on the atlas. Layout, fallback, and
  glyph blits are kryon's (`ui_text.c`, `ui_text_backend.h`); the backend
  rasterizes glyphs into an atlas texture and reports metrics. `ui_text.c`
  detects a backend that silently returns the default font as load failure.
- **Textures/images**: `LoadTexture`, `LoadTextureFromImage`,
  `LoadImageFromMemory`, `UnloadTexture`, `UnloadImage`,
  `LoadRenderTexture`, `BeginTextureMode`/`EndTextureMode`,
  `LoadImageFromTexture`, `ImageFlipVertical`, `ExportImage` (screenshot and
  `kryon-preview` PNG capture path), `GetImageColor`-class pixel access where
  used.
- **Math/collision helpers**: `Fade`, `ColorLerp`, `CheckCollisionPointRec`,
  `CheckCollisionRecs`, `GetWorldToScreen2D`, `GetScreenToWorld2D`,
  `GetCodepointNext` already come from `src/backend/kry_surface_math.c`
  (above). Only `TextFormat`/`TraceLog` remain backend-supplied (the null
  backend implements them; raylib forwards).
- **OS services** (may be no-ops on a given platform): `GetClipboardText`/
  `SetClipboardText`, `FileExists`, `DirectoryExists`, `GetDirectoryPath`,
  `MakeDirectory`, `LoadFileData/Text`, `UnloadFileData/Text`, `OpenURL`.
- **Audio** (apps opt in; null-grade stubs are acceptable for UI-only
  backends): `LoadSound`/`UnloadSound`/`PlaySound`, `SetSoundVolume/Pitch/Pan`,
  music stream functions, and raw `AudioStream` update/playback functions.

Everything else on the surface (3D, shaders, gestures, VR, ...) can be a
zero-return stub exactly like the generated null backend.

## Struct-layout obligations

The surface passes `Font`, `Texture2D`, `Image`, `GlyphInfo`,
`RenderTexture2D`, `Camera2D`, `Rectangle`, `Vector2`, `Color` by value with
raylib's layouts, and kryon state stores them directly (font caches, frame
camera, icon atlases). A backend must fill those structs with its own objects
behind them. The standard pattern is a side table keyed by the integer
`Texture2D.id`/`Font.texture.id` mapping to native handles; ids must be
non-zero for loaded resources (0 means "invalid/default"). `src/ui/
ui_text_backend.c` is the only place that reads `Font` fields directly
(baseSize, glyphPadding, texture, readiness); respect those semantics.

## Adding a backend

1. Create `src/backend/<name>_backend.c` implementing the required subset
   above under the surface's public names, plus the `KryonBackendRaw_*` hooks.
   Everything not implemented should match the null backend's zero-return
   behavior so the library still links.
2. Register it in the root `Makefile`'s `KRYON_BACKEND` block and in
   `mk/common.mk` (backend TU + link inputs), mirroring the existing entries.
3. Prove it end-to-end: `make KRYON_BACKEND=<name> check`-style build of the
   library, one example app, and a headless test slice. The `.krb` cartridge
   interpreter (`src/krb/krb.c`) draws through the `KryBackend` vtable
   (`include/kry_backend.h`) and is a quick way to smoke-test drawing without
   the full UI stack.
4. Do not patch vendor code or add `#ifdef <backend>` branches to `src/`;
   backend differences live behind the surface, `kry_input.h`, and
   `kry_backend.h`.

## Checks

- `make kryon-boundary-check` - no `raylib.h` includes or raylib paths in
  app-facing code.
- `make kryon-compat-check` - generated surface/rename/wrappers are in sync
  with `vendor/raylib/src/raylib.h` and every raylib symbol has a rename plus
  a public wrapper or `KryonBackendRaw_*` hook.
- `make libdraw-test` - plan9port/devdraw smoke for the libdraw surface backend
  plus a `9c`/`9l` clean-public-surface compile/link check.
- `sh tools/backend-required-symbols.sh` - the live required-subset list a
  backend must cover.
