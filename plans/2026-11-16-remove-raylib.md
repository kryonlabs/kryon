# Remove raylib from Kryon

Owner, 2026-11-16: "well finish migaritng raylib isuse in breeze fulyl so we
can have araylib free stuf also in kryno and remoev it so we have less codebas
to mainait" and "each of these udpates pelaos ugparde to ont use ralybi at all
updatenig them pfulyl and adning missing efatuers ot kryon or ziran moudels
etc... please fulyl implemnit t". Owner rule: "remove" means remove, never
disable. Owner, 2026-11-17 (relayed): "are yo udonig this please finsh removing
rayilb".

## State

Kryon has no raylib code. The deletion removed the ten raylib host modules,
the raylib ZX player, the raylib audio sink and media provider, the
`renderer-raylib` artifact, `backend = "raylib"`, `mk/raylib-library.mk`,
`mk/zx-raylib.mk`, `mk/raylib-shell.html`, `cmake/renderer-artifact/`, the
raylib tests, docs and CI steps, and `[dependencies.raylib]`. Shared codec
rules live in `mk/codecs.mk`. Browser asset and PCM audio use
`media_audio_web` over the Canvas WebAudio engine; native audio uses Ziran's
core `std/audio_*` modules through `media_audio_desktop`, including the Android
AAudio/OpenSL ES PCM sink. The Ziran toolchain pin is 7fbc8152. All maintained
example locks are regenerated without raylib.

## Open work

1. Breeze needs one Kryon: Plot, Game2D, CSS, Entity and Apps are on the
   deletion commit 3627ea8a with Ziran 7fbc8152; practices and arcade, which
   pin Kryon themselves, still need it.
2. Shovey standalone desktop: add the app/1 source-native desktop runner and
   its build binding to Kryon (`ReadAppHost` plus the app UI and platform
   capabilities `zx2native` needs; today `libkryon-desktop.so` exports only
   the five `ZxWindow*` capabilities). The node/2 adapter belongs to Apps
   (Kryon must not depend on Apps). Direct node/2 ZX to ELF (aggregate-return
   host ABI plus a lifecycle entry) is a later Ziran item.
3. Consumers off raylib, each in its own task: Breeze still has raylib in its
   Android/Windows hosts, audio and build rules. Shovey, Jungle, Arcade, Wurm
   and Werk have no active raylib references and need only their Kryon repin.
   kryonlabs/terminal and Harmony's terminal (`terminal_app_linux.zi`,
   `terminal_window_linux.zi`) still import `kryon/Raylib`; their desktop-host
   replacements, including `kryon/desktop_activity` and `OpenDesktopWindow`,
   are listed in `backup/raylib-gaps/terminal-api-map.md`. Game2D replaced
   its `Raylib` and `RaylibNull` modules with `Game2D/Sdl` (SDL2 on Kryon,
   143803f4); neon and kasaival still import `Game2D/Raylib` and move to it,
   neon also off its own raylib calls and C engine build.
4. macOS: CI builds the desktop ZX player and runs its headless bundle check;
   native macOS execution is unverified.
5. `tests/opus_audio_test.py` (not part of `make test` or CI) fails its
   pause-position check (exit 8) against the local audio package checkout.

## 2026-11-18 follow-up: sized Pixmap pictures (K2)

Implemented header-only dimensions for shipped PNG/WebP/JPEG/GIF/BMP pictures.
First drawing requests the resolution needed for its source crop and device
scale. The Android picture owner retains one premultiplied Lanczos reduction
per asset; larger draws upgrade it and smaller draws reuse it. Intrinsic
dimensions remain stable, including mirrored crops and meshes. Unsupported
metadata formats keep their bounded full-decode path. Codecs still use a
temporary full RGBA allocation before reduction; reducing codec peak memory
would need a scaled decoder API upstream.

Local tests passed: source/saved Android image services with an actual draw
of a 2048×1024 WebP and exact Pillow Lanczos golden; Pixmap image, mirrored
image, mesh and both Lanczos configurations; source/saved ARMv7/AArch64 links.
The published Ziran pin passes `push-check` with local overrides removed.
The first full gate reached a ZX media deadline; focused native and fresh-cache
native/browser reruns passed, as did that test in the full-gate retry. Failure
diagnostics now identify its stage and media/fetch states.

The full `make test` retry passed with the published toolchain and local
overrides removed. USB Moto source/saved execution passed real AssetManager
metadata, first-use decode, reduced retained pixels with stable dimensions,
AVIF/WebP RGBA goldens, missing-bridge reporting and cleanup without a window.
