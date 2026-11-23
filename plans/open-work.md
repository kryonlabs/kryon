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
- The five text-area widget checks (`tests/ziran_text_area_widget_test.sh`, `_scroll_selection_test`, `_touch_scroll_test`, `_wheel_test`, `_word_selection_test`) have different host bindings; build one shared host, then merge them into one table-driven module and compare behavior output and wall time before/after.
- Breeze's native figures move to the current Kryon `scene_skin`/`watercolor` painters only after Breeze's shared Kryon repin (downstream, tracked in Breeze's `plans/2026-11-15-breeze-desktop-without-raylib.md`); check by building Breeze's Tai Chi and Workout figures.
- Watercolor materials and the finishing pass are measured only on llvmpipe and the CPU renderer; run `make watercolor-test practice-material-gpu-test` on real phone hardware and record frame time, memory and thermals.
- Verify Breeze's figure preview holds 60 fps after its Kryon pin update (Kryon side landed: sparse glTF accessor sharing, Cairo GOOD mesh filter); measure the full preview on the VRoid model.
- Update CSS and Harmony to the exact single Kryon identity through package commands and verify the native stylesheet milestone (downstream repos; the Kryon side, the hosted StyleData codec with width/height, landed in c7286549).

- `:last-child`, `:nth-child`, `:only-child` and `:first-of-type` fail the sheet (packages/css/src/parser/selectors.zi:227) because a widget resolves its style before its later siblings exist; restyle the parent's children when `End` closes it, then accept them with a selector test over a three-row Column.
- Radial gradients: Row, Column and Grid paint only `linear-gradient()` (src/ui/layout_paint.zi `LayoutGradientStops`; packages/css/src/parser/gradients.zi:14 refuses `radial-gradient()`); add a radial fill to the layout painter and the reader, and check it with a gradient shot.
- The `tree_draw` export in ziran.toml is a second name for `kryon/Frame`; remove it once kryonlabs/explorer (src/taiji_host.zi:15, tests/split_test.zi:12, tests/host_test.zi:5) and the other downstream importers (Breeze, Photos, Jungle and Entity tests) import `kryon/Frame`.
- The gallery shell's demo scenes (examples/gallery/src/scenes.zi) still place their content at `GalleryBounds` rects; scenes 0 and 1 are content-sized columns now. Port the rest of each scene's page content to Layout containers as the examples were, keeping overlays at explicit rects, and check with `make gallery-zx-test`.
- `python3 tests/site_browser_test.py` times out selecting the Toggle example by keyboard: the picker lands on scene 54, so the test's `navigation_scenes` order no longer matches the picker's; make the test read the picker's order or select by label.
- Native-artifact fixture projects rebuild from the live Kryon override on every run (`tests/toolchain.py` `fixture_identity`); keep that while the tests examine this checkout, and pin them to the pushed commit when they no longer do.
