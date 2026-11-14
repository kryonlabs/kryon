# Why an empty window generates 64 C files, and how to shrink it

Owner, 2026-11-13: "why does a simple window in kryon have so many c files
genared tresehakngi semes ocmpletyl borkne analyze and improve ti"

Status, 2026-11-14: steps 1 and 3 done and pushed; step 2 reverted; step 4 stays.

| | Files | C | Executable |
| --- | --- | --- | --- |
| Before | 64 | 748 KB | 346 KB |
| Now (Kryon 1be70904) | 62 | 545 KB | 265 KB |

- Done: text editing. `text_request` calls a boundary procedure that
  `src/ui/text_boundary.zi` registers from a module initializer, and only the
  editing widgets import it. `std/grapheme` is gone from an empty window.
- Done: test driver. `src/backend/test_driver.zi` is a 1.6 KB wrapper; the
  engine (`test_driver_engine`) is built only with `--define KRYON_TEST_DRIVER`
  or `KRYON_TEST_DRIVER` set for `ziran tool kryon build`.
  `tests/program_entry_project_test.py` checks both.
- Reverted: images. A `cursor_picture` split moved the decoders out, but its
  host capability broke the Go backend ("Go global startup cannot call a host
  before binding") and builds that link the hosted widget host. It came out in
  797d7317. An empty window still carries `image_decode_linux`, `avif_decode`
  and `lanczos_cache` (37 KB of C). Doing it again needs registration that does
  not reach a host from global startup.
- Accessibility stays (63 KB), as planned.

Ziran module initializers exist and are documented ("Module initializers" in
Ziran's `docs/USAGE.md`, test `tests/module_registration.sh`), so the
"does not work today" note below is out of date.

## Measurement

`examples/window.zi` is five lines: an `App.Window` and `App.Run(window)`.

```sh
ziran build --target=c --project --root examples --exe --entry window:main \
    -o build/extension-window-test examples/window.zi
ls build/extension-window-test/*.c | wc -l      # 64
cat build/extension-window-test/*.c | wc -c     # 748 KB
stat -c %s build/extension-window-test/window   # 346 KB executable
```

| Share | Size | Files | What it is |
| --- | --- | --- | --- |
| 22% | 168 KB | 17 | Desktop host, painting and the window itself |
| 22% | 163 KB | 21 | Image decoding, WebP, AVIF and Lanczos scaling |
| 20% | 146 KB | 6 | Unicode grapheme tables and text editing |
| 15% | 109 KB | 12 | Widget tree, input and session |
| 11% | 81 KB | 1 | UI test driver |
| 9% | 69 KB | 2 | Accessibility (AT-SPI) |
| 1% | 11 KB | 5 | Standard library |

## Finding

Ziran's pruning works. `--entry` keeps only reachable procedures: for example
27 of `test_driver`'s 52 procedures, 32 of `accessibility_linux`'s 73 and none
of `video_linux`'s 47 are emitted.

The output is large because Kryon's desktop host calls every optional feature
directly, so all of it is reachable for every program:

- `desktop_loop.zi` imports `test_driver` and calls `TestDriverPointer`,
  `TestDriverWheel` and `TestDriverFrame` every frame. Whether a test drives
  the process is decided at run time by `KRYON_TEST_DRIVER`, so the compiler
  must keep the driver.
- `desktop_loop.zi` imports `accessibility_linux` and publishes the tree each
  frame; it is enabled at run time.
- `cairo_raster.zi` imports `image_decode_linux` and `lanczos_cache`, which
  pull in the WebP and AVIF packages, because the paint queue's image
  operation is handled in the rasterizer whether or not the program draws an
  image.
- `text_request.zi` imports `std/grapheme` (134 KB of tables) for text
  editing, which the session's input path reaches for every program.

About 62% of the generated C (460 KB, 30 files) is feature code an empty window
never runs: images, text editing, the test driver and accessibility.

## Plan, by size of win

Each step makes a feature reachable only from the module a program imports to
use it, so the existing pruning drops it from programs that do not.

The first idea, a procedure slot that the feature's module fills in a module
initializer, does not work today: Ziran has no documented module initializer
that runs registration code at startup for the C target. The mechanism
therefore differs per feature.

1. **Text editing (20%)**: doable in Kryon alone. Files:
   `src/ui/text_request.zi`, `src/ui/tree_input.zi`, the text field and text
   area widgets. An editing widget passes its grapheme-boundary procedure when
   it registers its editable text for the frame, and `text_request` calls
   through that value. A program with no editing widget then has no reference
   to `std/grapheme`. Risk: text requests cross the portable host protocol, so
   the Canvas, raylib, Android and ZX hosts and their text tests are affected.
   Test: a program with no editable text generates no `std_grapheme_data.c`,
   and every host's text input test passes.
2. **Images (22%)**: needs a decision first. The rasterizer replays paint
   operations after the widget call, so the image operation must name a
   desktop draw procedure that the portable `kryon/image` module cannot
   reference. Two ways:
   - Ziran gains module initializers (or link-time registration), and the
     image module registers the desktop decoder. A compiler change.
   - Desktop builds save targeted IR (`--target=c`), `kryon/image` selects the
     Cairo image module with `#if TARGET == .C && OS == .LINUX`, and the paint
     operation carries that module's draw procedure. Today only program
     entries save targeted IR; frame applications save portable IR.
   Files either way: `src/backend/cairo_raster.zi` (its image cache, decode
   and Lanczos part moves to its own module), `src/backend/paint_queue.zi`,
   `src/ui/image/`.
3. **UI test driver (11%)**: a build-time choice. Files: the ten host files
   that call `TestDriver*` (desktop, Canvas, raylib, Android),
   `src/backend/test_driver.zi`, `mk/ziran-project.mk`, and the 13 tests that
   set `KRYON_TEST_DRIVER`. The driver becomes a module a test build adds
   through a profile option, not part of every program. Several of those host
   files carry other tasks' uncommitted edits on 2026-11-13.
4. **Accessibility (9%)**: stays. It is the one feature a user benefits from
   without the program asking, so it is not removed by default.

Target after steps 1 to 3: about 36 files and 360 KB for `examples/window.zi`,
roughly half of today.

## Ziran's part

Pruning itself needs no change. Module initializers would make step 2 simple
and give Kryon one registration mechanism for every optional feature. A
separate possible improvement is emitting one C file per program instead of
one per module, which changes the file count but not the amount of code.
