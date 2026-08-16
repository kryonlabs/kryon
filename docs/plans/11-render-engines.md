# Plan 11 — Renderer Engines for KRB Cartridges

Status: draft
Depends on: 05-k2b, 06-krb-runtime

## Goal

A `.krb` cartridge must render and take input anywhere. That needs a family of
**renderer engines** — one per platform family — behind the existing
`KryBackend` vtable (`include/kry_backend.h`), plus a conformance suite that
proves they all draw the same picture.

The KRB runtime (`src/krb/krb.c`) is already platform-agnostic C: it walks
nodes, mounts state, and interprets `prog[]`. **Engines never touch cartridge
internals** — they only implement the backend table. That boundary is the
whole plan.

## Architecture

```
        .krb cartridge
              |
        KrbLoad / KrbExec          (portable C, no changes per platform)
              |
        KryBackend vtable          (THE contract)
   +----------+-----------+------------------+
   |          |           |                  |
 raylib    canvas      kry_sw (software   host-embedded
 engine    engine      rasterizer)        (game engines, clay,
 (desktop) (web)        - ESP32 framebuf    test harness, null)
   |          |          - fallback
OpenGL    Canvas2D     raw pixels
```

Two tiers of engines:

- **Tier A — surface backends** (full `kryon_compat.generated.h` API, ~700
  symbols). Only worth it where an ecosystem exists: raylib today.
- **Tier B — cartridge hosts** (implement `KryBackend` only, ~16 functions).
  This is where every new platform lands: web canvas, ESP32, SDL, LVGL,
  framebuffer, /dev/draw, embedded in another app.

Rule: **new platform = Tier B engine, never a fork of the runtime.**

## The contract (what we tighten first)

`KryBackend` today covers clear/rect/text/measure/clip/mouse/time/texture.
Before writing engines, extend it once, carefully, so every engine can be
written against a frozen target:

1. **Text**: add `font` handle + baked-glyph metrics (ascent/descent), not
   just `measure_text`. Engines that can't load fonts get a shared glyph
   atlas fed to them by the runtime (see phase 2).
2. **Textures**: add `texture_free`, `texture_info` (w/h/format), and
   `texture_from_pixels` so the software rasterizer's output can be blitted
   by a GPU engine.
3. **Redraw model**: add `present` and a dirty-rect hint. ESP32 and web both
   need "how much of the screen changed"; raylib ignores it.
4. **Pixel format + capabilities query**: `kry_backend_caps` struct —
   target format (RGBA8 / RGB565), max texture size, has_gpu, memory budget.
   k2b already bakes theme-slot colors; cartridges get a **profile** field
   (see phase 6) that must match engine caps.

Everything else stays. The vtable version bumps once here, then freezes.

## Phases

### Phase 1 — Conformance harness (before any new engine)

You cannot have five engines without a golden-image test.

- `cmd/krb-run` (or extend `kryon-preview`): headless tool that loads a
  `.krb`, runs it against an engine for N frames with scripted input,
  dumps the framebuffer to PNG.
- Golden tests: hash-compare per engine against committed reference PNGs
  (`tests/golden/*.png`). Tolerance only for antialiasing differences,
  and then only on text.
- The **null backend grows into a recording backend**: it logs the vtable
  call stream (rect/text/clip sequences) and compares *call streams* between
  engines — catches logic divergence even where pixels differ.
- Input injection via the existing `kry_input.c` Raw hooks, so the same
  script drives click/toggle/slider tests on every engine.

Deliverable: `make test-engines` that runs desktop raylib (offscreen) +
null/Recording and fails on divergence.

### Phase 2 — `kry_sw`: the portable software rasterizer

The single most leveraged engine. Pure C, no libc beyond memcpy/malloc,
renders into a caller-provided buffer:

- Targets: RGBA8 and **RGB565** (ESP32 panels), row-stride configurable.
- Primitives: filled/outlined rects, rounded rects, lines, clip stack —
  everything KRB nodes emit today. No full 2D library; only what
  `draw_node` (krb.c:615) can ask for.
- Text: **runtime-side glyph atlas**. k2b already owns fonts; extend the
  cartridge (or a sidecar section) with a pre-baked atlas: glyphs at one or
  two pixel densities, RGBA bitmap + metrics table. `kry_sw` blits glyphs;
  it never rasterizes outlines. This keeps the engine tiny and makes every
  other engine's font story trivial (atlas → texture).
- Pictures: pre-decoded or DXT/raw in the cartridge; `kry_sw` does
  scaled blits only.
- Dirty-rect tracking from day one (union of damaged regions per frame).

Where it lands: `src/backend/kry_sw.c`, exposed as both a `KryBackend`
(Tier B) and optionally as blit-source for Tier A engines via
`texture_from_pixels`.

Deliverable: Linux framebuffer host (`/dev/fb0`) as the proof — a real
`krb-run --backend=fb` on the console.

### Phase 3 — Web engine (canvas)

`docs/BACKENDS.md` mentions a canvas backend that isn't really there.

- Tier B engine in the wasm module exporting the vtable; JavaScript shim
  (grow `web/kryon-web-present.js`) drives Canvas2D: rects via
  `fillRect`, text via `fillText` (fonts from the atlas or CSS), images
  via ImageBitmap.
- Alternative under evaluation: compile `kry_sw` to wasm, render into
  `ImageData`, `putImageData` per dirty rect. Slower per-pixel but pixel-
  identical to native — start with this one for conformance parity, add
  the native-Canvas2D fast path second.
- Build via existing `mk/web.mk`; cartridge fetched as a byte array,
  `KrbLoad` runs on the mmap-equivalent view (it's already a flat image,
  so a JS `ArrayBuffer` view works with a tiny loader shim).

Deliverable: `examples/` cartridge running in a browser, wired into the
existing Netlify preview.

### Phase 4 — Android

- Keep the raylib NDK path for Tier A apps, but the cartridge story is
  Tier B: an `ANativeActivity` host app (~200 lines Java-free glue) that
  owns the window and calls a thin EGL engine — clear + `texture_from_pixels`
  blitting `kry_sw` output, or direct GLES rect/text draws if profiling
  demands it.
- Input: map MotionEvent → `KryonBackendRaw_*` hooks (already shared via
  `kry_input.c`); text input via JNI IME callback.
- Assets: cartridge + atlas in APK assets/, streamed into memory.

Deliverable: `mk/android.mk` builds a host APK that runs any example
cartridge from assets.

### Phase 5 — ESP32 / embedded

The hardest and the best proof of the architecture.

- Engine = `kry_sw` with RGB565 target + a small SPI/panel flush function
  (dirty-rect DMA to the display). No GPU assumptions, no heap beyond a
  couple of framebuffers in PSRAM (or single-buffer + careful flush).
- **Profiles** (phase 6) do the heavy lifting: the embedded cartridge is a
  separate k2b build — reduced node set, 16-bit colors baked at compile
  time, font atlas subsetted to used codepoints (k2b already walks the
  tree, so it knows the glyph set), no pictures above panel size.
- Event loop: one frame on input or state change; deep-sleep-friendly.
- Also validated on: nothing else. Embedded scope stays ESP32 + framebuffer
  until someone asks.

Deliverable: `examples/` breathing-timer-class cartridge on an ESP32-S3
with an ILI9341-class panel, 30s of interaction on battery-reasonable
behavior.

### Phase 6 — Profiles + asset pipeline (cross-cutting, lands with 3–5)

- **KRB profiles**: header flag (or k2b `--profile=embedded|web|desktop`)
  selecting color depth, atlas densities, max texture size, allowed node
  types. The loader checks engine caps (from the tightened vtable) and
  refuses mismatches loudly instead of garbling.
- **Atlas/textures in the cartridge**: extend `KRB_FORMAT.md` with an assets
  section (or sidecar `.krb` + `.kra`) — pre-baked, pre-scaled, ready to
  blit. Engines never decode PNG/JPEG.
- Version the vtable and the format together; both bump rarely.

### Phase 7 — Polish and long tail

- SDL2/SDL3 engine (Tier B, ~a day once `kry_sw` exists — good first
  contribution).
- macOS/iOS via the same pattern as Android.
- Plan 9 `/dev/draw` host — it's already named in BACKENDS.md; pure fun,
  low priority.
- Documentation: BACKENDS.md gets one page per engine: build flags, caps,
  known divergences.

## Sequencing & rationale

1 → 2 → 3 in strict order: harness first (or engines can't be trusted),
software rasterizer second (it *is* the web, embedded, and fallback engine),
web third (cheapest visible win). 4 and 5 can proceed in either order once
2 is solid; 6 items land incrementally alongside them.

The invariant across all of it: **`src/krb/krb.c` never grows an `#ifdef`**.
Platform code lives in engines, engines live behind the vtable, and the
conformance suite is the proof.

## Risks

- **Text quality divergence** between atlas-blitting engines (kry_sw, ESP32)
  and native-text engines (canvas `fillText`, raylib). Mitigation: prefer
  atlas-everywhere; native text is an opt-in fast path excluded from strict
  golden comparison.
- **vtable churn** stranding engines. Mitigation: one breaking extension
  (phase 0 tightening), then additive-only with caps-gated entry points.
- **ESP32 memory ceiling**: a node tree + atlas + framebuffer can exceed
  PSRAM on small modules. Mitigation: profiles + subsetted atlases; measure
  in phase 5 before promising panel sizes.
