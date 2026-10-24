# Kryon feature matrix

This table describes the maintained Ziran library and hosts. Kryon is
imported by a Ziran application; it is not a language or a built-in Ziran
runtime. Ziran owns compilation and portable `.zx` execution. See
[ARCHITECTURE.md](ARCHITECTURE.md) and [BACKENDS.md](BACKENDS.md) for the
module and host boundaries.

## Library packages

| Package | Ziran source | Native code generation | Package verification |
| --- | --- | --- | --- |
| Core UI (`src/ui`) | ✅ `.zi` modules listed in `modules.txt` and two public import modules | ✅ C and Go generation in `make` | ✅ `make test` builds the library and runs source, saved `.zir`, and `.zx` behavior tests |

The generated C archive is a build output of Ziran source. Behavior tests use
Ziran hosts in `tests/*_host.zi`. Code generation verifies that a module lowers
for a target; it does not prove that every native host runs on that target.

## Application hosts

| Backend | Implementation | Input and presentation verification | Current status |
| --- | --- | --- | --- |
| Terminal | `src/backend/terminal_run.zi` | Private PTY project test covers ASCII, UTF-8, invalid input, and escape sequences | ✅ supported |
| Desktop | `src/backend/desktop_run.zi` | Private Xvfb project test covers image capture, keyboard, UTF-8 text, and wheel input | ✅ supported on the tested Linux host |
| Raylib | `src/backend/raylib_run.zi` | Private Xvfb project test covers image capture and input | ✅ supported on the tested Linux host |
| Web Canvas | `src/backend/canvas_run.zi` | Headless Chromium checks Canvas2D pixels, clips, input, textures, fonts, lifecycle, WebAudio, and file behavior | ✅ supported through Emscripten on the tested browser |
| Web DOM | `src/backend/dom_run.zi` | Headless Chromium checks incremental updates, native Unicode editing, reorder focus, KSS, responsive sizing and browser accessibility | 🧪 hybrid host with opt-in native CSS layout for document widgets |
| Android | app `src/main.zi` + `src/backend/android_run.zi` | `tests/ziran_android_device_test.sh` checks the device text, lifecycle, and inset protocol with the Java side stubbed; a manual emulator run verifies install, rendering, touch, and back | ✅ supported through NativeActivity with raylib OpenGL ES 2 |

The desktop and raylib project tests run separately with
`make desktop-project-test` and `make raylib-project-test`; they must use a
private Xvfb display. CI runs both on every push in their own job. The Canvas2D and semantic DOM
project tests run in `make test` and launch only private headless Chromium.
