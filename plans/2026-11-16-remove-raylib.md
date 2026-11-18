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
   are listed in `backup/raylib-gaps/terminal-api-map.md`. Game2D's own
   `Raylib` module (used by neon and kasaival) took its raylib version from
   Kryon's lock; it needs its own decision: port those games and remove it, or
   pin raylib in Game2D.
4. macOS: CI builds the desktop ZX player and runs its headless bundle check;
   native macOS execution is unverified.
5. `tests/opus_audio_test.py` (not part of `make test` or CI) fails its
   pause-position check (exit 8) against the local audio package checkout.
