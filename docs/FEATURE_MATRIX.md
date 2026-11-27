# Kryon feature matrix

This table describes the maintained Ziran library and hosts. Kryon is
imported by a Ziran application; it is not a language or a built-in Ziran
runtime. Ziran owns compilation and portable `.zx` execution. See
[ARCHITECTURE.md](ARCHITECTURE.md) and [BACKENDS.md](BACKENDS.md) for the
module and host boundaries.

## Library packages

| Package | Ziran source | Native code generation | Package verification |
| --- | --- | --- | --- |
| Core UI (`src/ui`) | ✅ `.zi` modules listed in `modules.txt` and named package exports | ✅ C and Go generation in `make` | ✅ `make test` builds the library and runs source, saved `.zir`, and `.zx` behavior tests |

The generated C archive is a build output of Ziran source. Behavior tests use
Ziran hosts in `tests/*_host.zi`. Code generation verifies that a module lowers
for a target; it does not prove that every native host runs on that target.

Core UI includes measured flex/grid layout (`src/ui/layout.zi`), the style
sheet with structural selectors and sibling combinators (`src/ui/style_tree.zi`) and gradient box
painting (`src/ui/layout_paint.zi`); the supported CSS subset and remaining
browser-only features are documented in
[HTML_TO_KRYON.md](HTML_TO_KRYON.md#4-kryon-css-support-kryonlabscss).

## Application hosts

| Backend | Implementation | Input and presentation verification | Current status |
| --- | --- | --- | --- |
| Terminal | `src/backend/frames_terminal.zi` | Private PTY project test covers ASCII, UTF-8, invalid input, and escape sequences | ✅ supported |
| Desktop | `src/backend/frames_desktop.zi` | Private Xvfb project test covers image capture, keyboard, UTF-8 text, and wheel input | ✅ supported on the tested Linux host |
| Web Canvas | `src/backend/frames_web.zi` | Headless Chromium checks WebGL2 pixels, clips, input, textures, fonts, lifecycle, WebAudio, and file behavior | ✅ supported through Emscripten on the tested browser |
| Android | app `src/main.zi` + `src/backend/android_run.zi` | `tests/ziran_android_device_test.sh` checks the device text, lifecycle, and inset protocol with the Java side stubbed; a manual emulator run verifies install, rendering, touch, and back | ✅ supported through NativeActivity with direct ANativeWindow pixel presentation |

The desktop project test (`make desktop-project-test`) uses a private Xvfb
display. The canvas project test runs in `make test` and launches only private
headless Chromium. `make zx-window-test` covers bare-window bundles through
the native desktop player and host library. Code generation alone does not
establish platform execution.
