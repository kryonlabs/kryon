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

1. Repin the five Kryon-based packages to the deletion commit, on one Kryon:
   plot, entity, game2d, css and apps. Build on the Mesh rename worker's
   pushed Entity/Apps commits (`daochi_client` becomes `mesh_client`) without
   reverting them. Include Entity's native-crypto fix: Android and Windows
   builds generate C with the host's `GEN_ZI2C` and target `ZIRAN_DEFINES`.
   Breeze needs all five on one Kryon, and also practices and arcade, which
   pin Kryon themselves.
2. Desktop-host APIs the terminal and Harmony's terminal need before they
   leave `kryon/Raylib` (replacement table in
   `backup/raylib-gaps/terminal-api-map.md`): a wait on program descriptors
   plus window input, a frame-rate setter, an exit key, middle-button polling,
   open flags (undecorated, topmost, always-run), a fullscreen toggle and a
   glyph pre-warm.
3. Shovey standalone desktop: add the app/1 source-native desktop runner and
   its build binding to Kryon (`ReadAppHost` plus the app UI and platform
   capabilities `zx2native` needs; today `libkryon-desktop.so` exports only
   the five `ZxWindow*` capabilities). The node/2 adapter belongs to Apps
   (Kryon must not depend on Apps). Direct node/2 ZX to ELF (aggregate-return
   host ABI plus a lifecycle entry) is a later Ziran item.
4. Consumers off raylib, each in its own task: Breeze still has raylib in its
   Android/Windows hosts, audio and build rules. Shovey, Jungle, Arcade, Wurm
   and Werk have no active raylib references and need only their Kryon repin.
5. macOS: CI builds the desktop ZX player and runs its headless bundle check;
   native macOS execution is unverified.
6. `tests/opus_audio_test.py` (not part of `make test` or CI) fails its
   pause-position check (exit 8) against the local audio package checkout.
