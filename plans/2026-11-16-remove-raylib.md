# Remove raylib from Kryon

Owner, 2026-11-16: "well finish migaritng raylib isuse in breeze fulyl so we
can have araylib free stuf also in kryno and remoev it so we have less codebas
to mainait". Owner rule: "remove" means remove, never disable. Kryon ends with
no raylib code at all.

Planning audit of kryonlabs/kryon @ master (e865c067 working tree),
2026-11-16, read-only measurements with rg/wc.

## 1. Inventory inside Kryon

### Host modules to delete

| Module | Lines | Bytes | Role |
| --- | --- | --- | --- |
| `src/backend/raylib_runtime.zi` | 4,703 | 195,732 | Core raylib host: window, events, OpenGL ES 2 rendering, input, frame loop; `_WIN32` (9) and `PLATFORM_WEB` (7) branches; Android frame threads |
| `src/backend/raylib_secondary_window.zi` | 870 | 29,766 | Secondary windows for the raylib host (already opens SDL windows internally) |
| `src/backend/raylib_trace.zi` | 335 | 11,443 | `SetTraceLogCallback` routing to Kryon logging |
| `src/backend/media_audio_raylib.zi` | 380 | 19,865 | MediaAudio provider over raylib/miniaudio |
| `src/backend/bundle_raylib.zi` | 237 | 9,283 | Bundled-app host surface (`GetApplicationDirectory`, `GetTime`) |
| `src/backend/raylib_image_decode.zi` | 194 | 7,508 | Image decode via raylib |
| `src/backend/raylib_frame_render.zi` | 161 | 6,398 | Threaded frame render for Android/`KRYON_FRAME_THREADS` |
| `src/backend/raylib_clip.zi` | 150 | 4,815 | Scissor/polygon clip via rlgl matrix + `BeginScissorMode`; `OS == .WEB` branch implements canvas clipping |
| `src/backend/raylib_run.zi` | 107 | 3,411 | raylib host entry point |
| `src/backend/audio_sink_raylib.zi` | 32 | 2,326 | Audio sink feeding the opus mixer into raylib/miniaudio |
| **Total** | **7,169** | **286,547** | |

### Modules importing raylib today (rewire, keep the feature)

- `src/backend/media_audio_opus.zi` -> `audio_sink_raylib` (desktop SDL sink `audio_sink_sdl` already exists).
- `src/backend/page_mesh.zi`, `src/backend/canvas_raster.zi`, `src/backend/Canvas/module.zi` -> `raylib_clip` (canvas branch must move into the canvas host; desktop host clips through Cairo and does not use it).
- `src/backend/window_icon.zi`, `src/backend/image_export.zi` -> `raylib_image_decode` (`ziran_image_decode` exists; `ExportImage` needs a non-raylib export path).
- `src/backend/notification_desktop.zi` -> raylib `IsWindowFocused` (SDL query instead).
- `src/backend/android_jni.zi` -> raylib `GetAndroidApp` (lines 220-221, used by `JniGetAndroidApp` in 4 places). This is the only raylib tie in the Android host: `cmake/KryonAndroid.cmake` and `templates/droid/` are raylib-free.
- `src/project/options.zi`, `src/project/cli_linux.zi` -> `backend = "raylib"` profile and the `platform = "web"` raylib validation.
- Comment-only references: `src/ui/bitmap_store.zi`, `box/box_props.zi`, `frame_transition.zi`, `shape/*`, `shape_paint.zi`, `svgicon/svg_icon_behavior.zi`, `touch_replay.zi`, `test_driver.zi`, `test_driver_engine.zi` (raylib key-code/behavior parity notes).
- Canvas-host raylib-ABI shims: `canvas_os.zi` (TraceLog/memory entry points), `canvas_events.zi` ("raylib-compatible reads"), `canvas_input.zi` (raylib key/gamepad IDs) - decide keep-as-ZX-compat or strip with the rest.

### Build rules, artifact, manifests

- `Makefile`: 89 raylib lines of 1,005. Targets to remove: `raylib-project-test`, `raylib-web-project-test`, `raylib-window-test`, `raylib-typeface-test`, `raylib-text-pixels-test`, `raylib-mesh-pixels-test`, `raylib-frame-input-test`, `raylib-frame-pixels-test`, `raylib-secondary-window-test`, `raylib-trace-test`, `raylib-clip-test`, `raylib-smooth-shape-test`, `raylib-webp-decode-test`, `android-raylib-host-link-test`, `android-raylib-jni-test`, `android-frame-input-test`, `zx-raylib`, `zx-raylib-test`; `zx-headless-test`, `zx-services-test`, `zx-hosted-test` depend on `zx-raylib`; `test:` pulls `raylib-web-project-test` and both android-raylib tests; `CI_SHARD_WEB` carries `zx-raylib-test` + `raylib-web-project-test`.
- `mk/raylib-library.mk` (114 lines): builds the raylib package native and `PLATFORM_WEB`; also hosts the webp/avif/aom archive recipes and `RAYLIB_ARCHIVES` - codec recipes must move out before deletion.
- `mk/renderer-artifact.mk` (1 line): includes `raylib-library.mk`.
- `mk/zx-raylib.mk` (63 lines) + `mk/raylib-shell.html` (25): the raylib ZX native player and its web shell.
- `cmake/renderer-artifact/CMakeLists.txt` (5) + `RaylibAndroid.cmake` (82).
- `scripts/renderer_artifact.py` (364 lines, 18 raylib refs): provides/consumes the `renderer-raylib` artifact (linux-x86_64-gnu, windows-x86_64-mingw, android-aarch64/armv7).
- `ziran.toml`: `[dependencies.raylib]` (lines 11-12), exports `Raylib`/`raylib_runtime` (94-95), `bundle_raylib` (118), `media_audio_raylib` (226), `raylib_secondary_window`/`raylib_trace` (295-296), `[artifacts.renderer-raylib]` (401-403, outputs `lib/libraylib.a` + raylib headers + codecs + fonts).
- `ziran.lock`: raylib package entry and kryon's `raylib` dependency (lock 6,633 B).
- `ziran.local.toml`: raylib source override (line 3).
- 99 `examples/*/ziran.lock` files carry the transitive raylib entry; regenerate after the dependency drops.

### Tests

43 raylib test files, 5,168 lines, 206,545 bytes under `tests/` (`raylib_*` behavior/pixel/project suites, `ziran_raylib_graphics/retained/windows_typeface`, `android_raylib_host_link/jni/frame_pixels`, `bitmap_raylib`, `backend_render_benchmark_raylib`, `rotated_text_raylib`, renderer probes). Affected neighbors: `zx_native_test.py` (runs the raylib ZX player), `shader_host_test`, `renderer_consumer_test`, `project_static_archive_test`.

### Docs and CI

- Docs hits: `BACKENDS.md` 61, `API.md` 51, `PROJECTS.md` 23, `FEATURE_MATRIX.md` 4, `ARCHITECTURE.md` 3, plus `ACCESSIBILITY`, `BOUNDARIES`, `COMPOSITION_INPUT`, `WIDGET_CONFORMANCE`, `ZOOM`, `window-focus`; also `README.md`, `THIRD_PARTY_NOTICES.md`, `CHANGELOG.md`, `icons/README.md`.
- CI `.github/workflows/ci.yml`: line 115 (`zx-raylib --check`), lines 315-373 ("Desktop and raylib hosts on private Xvfb" job, "Raylib host" step, and the typeface/trace/secondary-window/frame-pixels/frame-input step with SDL2/drm/gbm/egl/glesv2 libs).

### Total

About 13,000 tracked lines (~0.5 MB): 7,169 host-module lines, 5,168 test lines, ~654 build/tooling lines, plus Makefile/docs/manifest edits - and the `raysan5/raylib` source dependency disappears from every consumer lock.

## 2. Consumers

### Breeze (`waozixyz/apps/breeze`)

- Linux desktop: raylib host today through its own build (Makefile builds `libraylib.a`, 45 raylib lines); `src/platform/{host_input,host_window,app_trace,radio_native,audio_runtime,app_host,app_breaks,application_frame}` and `src/screens/node_app_screen.zi` import `kryon/raylib_runtime`/window runtime. In flight: `plans/2026-11-15-breeze-desktop-without-raylib.md` moves Linux desktop to Kryon's desktop host (SDL2 + Cairo/Pango) with a before/after benchmark.
- Windows: MinGW-cross-compiled raylib host (`mk/windows.mk`, `WINDOWS_RAYLIB_RULE`, raylib audio + AVIF_RAYLIB defines).
- Web: canvas host already (`application_frame.zi` canvas branch, `mk/web.mk`) - no raylib.
- Android: `KRYON_ANDROID_HOST` + `KRYON_RAYLIB_HOST`; builds its own `breeze_raylib` static lib (rcore/rshapes/rtextures/rtext/raudio) for its GL renderer; `droid/.../CMakeLists.txt` 19 raylib lines.

### Wurm (`waozixyz/games/wurm`)

- Linux desktop: `backend = "desktop"` but `native_artifacts = "kryon:renderer-raylib"` (pulls `libraylib.a` + opus/webp/avif archives) and `src/desktop_host.zi` imports `kryon/media_audio_raylib` directly (raylib/miniaudio provider on the SDL desktop host).
- Portable: `codegen = "zx"` desktop bundle; Android `backend = "android"` (NativeActivity, raylib-free except the glue); web audio test uses `canvas_audio_host`.
- Tests: `audio_transport_test.zi` imports `media_audio_raylib`.

### Jungle (`waozixyz/learning/jungle`)

- Android: Kryon's android host draws the Frame; sound calls raw raylib audio C API from `host/sound.zi` (`sound_raylib` system_library: `InitAudioDevice`, `LoadAudioStream`, `UpdateStream`, `LoadWaveFromMemory`/stb_vorbis); `droid` CMake compiles `raudio.c` (`jungle_audio`).
- Linux desktop: its own `host/desktop.zi` entry (native_codegen), no Kryon raylib host; Makefile references `mk/raylib-library.mk` only as a dependency of the avif `.so` rule.
- Treehouse 3D uses `scene_render` (raylib-free).

### Shovey (`waozixyz/games/shovey`)

- Linux desktop: ships Kryon's raylib ZX player (`make -C KRYON zx-raylib`, copies `build/zx-raylib/zx`).
- Windows: `src/player_windows.zi` imports `kryon/raylib_runtime`; `player_sound.zi` uses raylib audio device.
- Android: packaging links raylib through the `renderer-raylib` artifact metadata (`SHOVEY_RENDERER_LINK` feeds a fake `raylib` INTERFACE target).

### Arcade (`waozixyz/games/arcade`)

- Linux desktop: already raylib-free (`kryon/desktop_runtime` + Cairo in `src/desktop.zi`).
- Windows: standalone player on Kryon's raylib host (`src/windows.zi` imports `kryon/raylib_runtime`); `src/sound.zi` uses raylib audio device; `scripts/build-windows.py` builds raylib from the locked package.
- Android: `backend = "android"` (raylib-free except glue).

### Werk (`waozixyz/apps/werk`)

- Linux desktop: `backend = "desktop"` - no direct raylib code; `ziran.lock` carries the transitive raylib entry and `RAYLIB_DIR` is passed into the droid Gradle/Android build (6 lock hits, Makefile lines 14/216/223/228).

### Apps (`waozixyz/modules/apps`)

- Tests/hosts select the raylib provider on native: `tests/audio_host_behavior.zi` (`MediaAudioUseRaylib()`), `tests/art_host_behavior.zi` (`UnloadImage`, `RaylibDecodePixels`); README documents `MediaAudioUseRaylib()` and `RAYLIB_A` test setup.

### Entity and other packages

- Entity (`waozixyz/modules/entity`), auth, dew, gamekit: lock-only raylib entries - resolve automatically when Kryon drops the dependency.
- Harmony clients: `harness/harmony/plugins/jobs/kryon` is the legacy C "Kryon" (raylib-style `kryon.h` support library) with its own vendored raylib submodule, `mk/raylib.mk`, generated compat wrappers, and a raylib conformance matrix. Separate codebase and repository from Ziran Kryon; removing its raylib is an independent Harmony-side decision, not a step of this plan.
- Archived (`ARCHIVE/*`, `harmony-legacy`) and doc-only mentions (`ziranlang/packages/audio/README.md`): no open work.

## 3. Platform status and gaps

- Linux: complete non-raylib desktop stack exists - `desktop_run`/`desktop_loop`/`desktop_window` (SDL2), `cairo_raster`, `desktop_secondary_window` (SDL window + Cairo surface, up to 3), `audio_sink_sdl` + `media_audio_desktop`, Vorbis via `std/vorbis`, Opus via the audio package, GTK tray, clipboard, background windows, and the desktop-host ZX player (`zx-player` + `zx2native` linking the desktop host library, `zx-window-test`). Open gaps: `media_audio_opus` still sinks into `audio_sink_raylib`; `notification_desktop` focus query; `window_icon`/`image_export` decode/export rewires.
- Web: complete canvas stack - `canvas_run` (Canvas2D + Emscripten/Asyncify), canvas audio host, canvas ZX player (`zx_canvas.zi`), browser ZX window via the canvas player (commit e865c067). The raylib WebGL page dies with the raylib host; the only loss is custom raylib-host WebGL content (Kryon's raylib-web 3D-cube demo) - no maintained consumer uses it.
- Android: Kryon's host is already raylib-free at presentation - NativeActivity owned in `android_native.zi`, direct `ANativeWindow` pixels, Pixmap runtime, JNI services, `MediaPlayer` for portable guest audio. Single gap: `android_jni.zi` takes the `android_app` pointer from raylib's `GetAndroidApp` glue; replace with the NDK `native_app_glue` bridge. Consumer-side gap: Jungle's raw raudio calls need the Android media service (or AAudio/OpenSL).
- Windows: no non-raylib host exists. The desktop host is Linux-only today (no `_WIN32`/Darwin paths in `desktop_loop`/`desktop_window`; sibling modules are `*_linux`). Gap: port the desktop host to Windows (SDL2 window, Cairo raster, `*_linux` platform modules) so Breeze/Arcade/Shovey Windows can leave the raylib host; `audio_sink_sdl`/`media_audio_desktop` already speak SDL2, which covers Windows audio.
- macOS: no non-raylib host; raylib is currently the only macOS surface (`RAYLIB_HOST_OS = Darwin` handling in `zx-raylib.mk`). Scope decision needed from the owner: port desktop host to macOS or accept losing macOS until then.
- ZX player: two native players exist - the raylib one (`tools/zx_raylib.zi` + `mk/zx-raylib.mk`) and the desktop-host one (`zx-player`, `zx2native`, canvas web player). Gap: point Kryon's `zx-headless/services/hosted` tests and Shovey's desktop packaging at the desktop-host player, then delete the raylib player.
- GPU/3D `scene_render`: `src/ui/scene_render.zi` + `scene_gpu_math`/`scene_gpu_sources` are pure Ziran software rendering - no raylib imports, no foreign GL. Consumers (Jungle treehouse, Wurm terrain) are unaffected. The only raylib GL path is the raylib host itself; nothing else needs it.

## 4. Ordered removal plan

Consumers move first (non-raylib hosts already cover Linux/web/Android), Kryon host gaps close in parallel where they unblock consumers, raylib deletion is last.

1. Finish Breeze Linux desktop on the desktop host - existing `plans/2026-11-15-breeze-desktop-without-raylib.md`, benchmark before/after, then remove Breeze's Linux raylib link. Size: port already scoped (13 `src/platform` files, Makefile 45 lines). Type: consumer port (Breeze).
3. Switch Wurm and Apps to the desktop audio/decode path - Wurm `desktop_host.zi` to `media_audio_desktop`, Wurm `native_artifacts` to a codec-carrying non-raylib artifact, Apps tests to `media_audio_desktop` + `ziran_image_decode`; refresh locks. Size: ~4 files + locks. Type: consumer port. Proves `media_audio_desktop` parity before Kryon deletes the raylib provider.
5. Jungle Android audio - replace `host/sound.zi` raw raudio calls with Kryon's Android media service and drop `raudio.c` from the droid CMake. Size: ~1 file + cmake. Type: consumer port (depends on step 2 for a raylib-free Android link).
6. Desktop-host Windows port - SDL2 window + Cairo raster + platform modules so Breeze/Arcade/Shovey Windows builds leave the raylib host; then move those three Windows players and their raylib audio to it. Size: largest new surface (new platform backend; consumers ~3 files each). Type: Kryon host work, then consumer ports. Include the owner's macOS scope decision here.
7. Consolidate the ZX player on the desktop-host player - repoint `zx-headless-test`, `zx-services-test`, `zx-hosted-test`, `zx_native_test.py` and Shovey's desktop packaging to `zx-player`/`zx2native`. Size: Makefile/test edits (~40 lines) + Shovey Makefile. Type: CI/tests + consumer port.
8. Rewire remaining Kryon internals - `media_audio_opus` -> `audio_sink_sdl`, `notification_desktop` -> SDL focus query, `window_icon`/`image_export` -> ziran decode (+ non-raylib export), split `raylib_clip`'s canvas branch into the canvas host, rename or strip the canvas raylib-ABI shims, clean comment references. Size: ~8 modules, each small. Type: Kryon host work.
9. Delete raylib from Kryon - remove the 10 backend modules (7,169 lines), 43 test files (5,168 lines), `mk/raylib-library.mk`, `mk/renderer-artifact.mk`, `mk/zx-raylib.mk`, `mk/raylib-shell.html`, `cmake/renderer-artifact/*`, `renderer-raylib` artifact and `backend = "raylib"` support from `scripts/renderer_artifact.py`/`src/project`, docs sections, CI steps and shards, `[dependencies.raylib]` from `ziran.toml`, `ziran.lock`, `ziran.local.toml`, and regenerate the 99 example locks. Size: ~13,000 lines. Type: Kryon deletion + CI.

Open work only; Breeze's Linux-desktop move, Wurm/Apps switches, and the Harmony legacy C tree are tracked as above - nothing in this plan assumes a raylib fallback stays behind.
