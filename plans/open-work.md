# Open Kryon work

One line per open item: what, where in the code, how to check. Everything not
listed here is finished behavior documented in docs/.

- Widgets that accept optional pictures without importing `kryon/image` (textarea image blocks, dropdown option images, avatar, badge, carousel) still pull the picture chain into every app that draws them: `src/ui/textarea`, `src/ui/dropdown`, `src/ui/avatar`, `src/ui/badge`, `src/ui/carousel`; move the lookup behind `image_lookup` as `button` does, then rebuild such an example and confirm no image-decoder modules are generated.
- The shared desktop ZX player cannot watch hosted-app releases yet: `src/backend/desktop_zx.zi` answers unavailable; extend the provider and cover it with `make zx-hosted-test`.
- Accessibility adapters exist only for Linux (`src/backend/accessibility_linux.zi`, `src/backend/accessibility_tree_linux.zi`); text boundary/extents methods, list Selection, table interfaces and Windows/macOS/mobile adapters remain unimplemented; `sh tests/native_accessibility_test.sh` covers the Linux bridge.
- The hosted component/widget fixture has no Android device coverage: fixtures under `tests/fixtures/component_*`, checked by `sh tests/ziran_component_host_test.sh` and `python3 tests/component_browser_test.py`; add an emulator run next to `python3 tests/zx_android_test.py`.
- `tests/opus_audio_test.py` fails its pause-position check (exit 8); reproduce with `python3 tests/opus_audio_test.py`.
- Native macOS execution of the desktop ZX player is unverified: CI builds it and runs only the headless bundle check; run `make zx-test` on macOS hardware.
- Downstream raylib migrations (Breeze Android/Windows hosts, neon, kasaival, kryonlabs/terminal) remain in their own repositories; Kryon itself contains no raylib code.
