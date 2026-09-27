# Kryon feature matrix

This table describes the maintained Ziran library and hosts. Kryon is
imported by a Ziran application; it is not a language or a built-in Ziran
runtime. Ziran owns compilation and portable `.zib` execution. See
[ARCHITECTURE.md](ARCHITECTURE.md) and [BACKENDS.md](BACKENDS.md) for the
module and host boundaries.

## Library packages

| Package | Ziran source | Native code generation | Package verification |
| --- | --- | --- | --- |
| Core UI (`src/ui`) | ✅ `.zi` modules listed in `modules.txt` and two public import modules | ✅ C, C++, and Go generation in `make` | ✅ `make test` builds the library and runs source, saved `.zir`, and `.zib` behavior tests |
| Plot (`src/plot`) | ✅ optional `.zi` package | ✅ C, C++, and Go generation in `make plot` | ✅ imported by the terminal package project test |
| TableView and TreeView (`src/data_views`) | ✅ optional `.zi` package | ✅ C, C++, and Go generation in `make data-views` | ✅ imported by the terminal package project test |
| KSS (`src/kss`) | ✅ optional `.zi` package | ✅ C, C++, and Go generation in `make kss` | ✅ imported by the terminal package project test |
| Syntax (`src/syntax`) | ✅ optional `.zi` package | ✅ C, C++, and Go generation in `make syntax` | ✅ imported by the terminal package project test |

The generated C archive is a build output of Ziran source. Behavior tests use
Ziran hosts in `tests/*_host.zi`. Code generation verifies that a module lowers
for a target; it does not prove that every native host runs on that target.

## Application hosts

| Backend | Implementation | Input and presentation verification | Current status |
| --- | --- | --- | --- |
| Terminal | `src/backend/terminal_run.zi` | Private PTY project test covers ASCII, UTF-8, invalid input, and escape sequences | ✅ supported |
| Desktop | `src/backend/desktop_run.zi` | Private Xvfb project test covers image capture, keyboard, UTF-8 text, and wheel input | ✅ supported on the tested Linux host |
| Raylib | `src/backend/raylib_run.zi` | Private Xvfb project test covers image capture and input | ✅ supported on the tested Linux host |
| Libdraw | `src/backend/libdraw_run.zi` | Private Xvfb project test covers capture and input | ✅ supported on the tested Linux host |
| Web | `src/backend/canvas_run.zi` | Headless Chromium checks Canvas2D pixels, clips, input, textures, fonts, lifecycle, WebAudio, and file behavior | ✅ supported through Emscripten on the tested browser |

The desktop, raylib, and libdraw project tests run separately with
`make desktop-project-test`, `make raylib-project-test`, and
`make libdraw-project-test`; they must use a private Xvfb display. The
Canvas2D project test runs in `make test` and launches only private headless
Chromium.
