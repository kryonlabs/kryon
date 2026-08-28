# Canvas2D Raylib Parity

Kryon's `canvas` backend is the no-WebGL web backend. It aims to run
raylib-style 2D/UI applications with the same public surface while leaving
non-2D raylib areas on generated weak null stubs.

The live required surface comes from:

```sh
sh tools/backend-required-symbols.sh
```

The no-WebGL 2D/browser subset is guarded by:

```sh
make canvas2d-parity-check
```

## Status Model

| Status | Meaning |
|---|---|
| `implemented` | Canvas2D has a real backend implementation. |
| `partial` | Implemented, but browser limits or known gaps remain. |
| `stub` | Provided by the generated weak null backend only. |
| `untested` | Implemented but not covered by `make canvas-test` yet. |

## 2D/UI Surface

| Area | Status | Notes |
|---|---|---|
| Window/frame lifecycle | `implemented` | `InitWindow`, frame timing, logical screen size, render size, title, cursor, and basic state flags. |
| HiDPI sizing | `implemented` | Main canvas uses logical coordinates over a `devicePixelRatio` backing store; render size reports backing pixels. |
| Keyboard/mouse/wheel | `implemented` | Includes key repeat, mouse buttons, pointer capture, mouse delta, and fractional wheel vectors. |
| Touch | `implemented` | Browser touch points map to raylib touch APIs and update mouse position from touch point 0. |
| Gamepad | `partial` | Standard browser Gamepad API buttons/axes are mapped; SDL mapping strings and vibration are no-ops. |
| Dropped files | `partial` | Browser drop events queue file paths and asynchronously write file contents into MEMFS. |
| Basic shapes | `implemented` | Pixels, lines, rectangles, rounded rectangles, circles, rings, triangles, fans, strips, and outlines. |
| Gradients | `implemented` | Vertical, horizontal, and four-corner rectangle gradients. |
| 2D camera/scissor | `implemented` | `BeginMode2D` and active scissor clipping use Canvas transforms/clips. |
| Textures | `implemented` | RGBA textures, render textures, tinting, alpha, rotation/origin, source rectangles, and negative-size flips. |
| Images/screenshots | `partial` | RGBA image load/export/screenshot and format validity are implemented; non-PNG decoders stay limited. |
| Fonts/text | `implemented` | FontFace-backed TTF loading, glyph atlas metrics, custom text drawing, codepoint drawing, and measurement. |
| Files/path helpers | `implemented` | MEMFS-backed load/save/path helpers and directory listing. |
| Clipboard | `partial` | Writes mirror locally and attempt browser clipboard writes; synchronous browser clipboard reads are unavailable. |
| Audio | `partial` | WebAudio sound/music/streams exist, with normal browser user-gesture unlock limits. |

## Explicit Non-Goals

| Area | Status | Notes |
|---|---|---|
| 3D cameras/models/meshes | `stub` | Requires WebGL or another renderer to match raylib. |
| Shaders/rlgl/render batches | `stub` | Canvas2D cannot faithfully expose shader or rlgl behavior. |
| VR/stereo rendering | `stub` | Out of scope for the no-WebGL backend. |
| Gesture recognizer | `stub` | Raw touch points are implemented; raylib gesture synthesis is still fallback. |
