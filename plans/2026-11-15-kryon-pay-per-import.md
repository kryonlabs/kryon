# Pay per import: a program carries only what it imports

Owner, 2026-11-15: "why is kryon empty window generating so much c code it
should be really really small amount of code", "only what is necessary",
"i want clean import structure like import a frame it makes a frame, import a
rectangle i have a rectangle, not importing everything by default", "like in
the examples".

Nothing is imported by default. An empty window (`examples/window.zi`) is the
window, its event loop, clearing and presenting. Each example carries the
window plus what it imports. This overrides the 2026-11-13 plan's choice to
keep accessibility in every program: an empty window keeps only window-level
accessibility, and widget roles and nodes come with the widget imports.

Status: in progress. The empty window is done (17 KB executable, pushed);
frame examples still carry pictures, text input and gamepad code they don't
import (see "Modules outside an example's imports" and Steps).

## Measurement

```sh
ziran build --target=c --project --root examples --exe --entry window:main \
    -o build/scratch/ppi/window examples/window.zi
```

| examples/window.zi | C files | C bytes | Headers | Executable |
| --- | --- | --- | --- | --- |
| Before (Kryon 07d74f6d) | 62 | 545 965 | 77 | 265 304 |
| Step A: pictures, video, Lanczos, fonts | 39 | 344 259 | 50 | 178 816 |
| Frame driver, accessibility split, `App.Show` (ba0d3950) | 23 | 132 686 | 34 | 117 536 |
| Bare-window loop `desktop_window` (0fbdc938) | — | — | — | 17 488 |

`examples/window.zi` now holds only: window (the program), app,
app_host_desktop (`Show`), desktop_window, pixmap_sdl, std C strings and the
executable entry.

Ziran's C emitter now writes only what a closed program uses into its headers
and runtime headers (Ziran db0bae50, c0efdfd5). Header bytes: hello
288 004 → 88 777, box 319 181 → 100 051, image 691 846 → 417 354. Image's
`ziran_simd_ops.h` still carries an 81 KB common section from its generator.

## Per-example measurement

Measured 2026-11-15 at Kryon HEAD 0fbdc938 with today's Ziran, one build at a
time through the build queue. Raw numbers in
`build/scratch/measure/results.tsv`, build logs in
`build/scratch/measure/logs/`.

Eleven examples fail to build (first error line):

- `bandchart`, `candlechart`, `gallery`, `kanban`, `linechart`,
  `linechartcompare`, `plotbars`, `plotlines`, `tableview`, `treeview`:
  `unknown initializer field: font`
- `flow`: `ziran: command exited with status 127: zi2c` (cannot build the
  package tool)

Executable sizes of the 90 builds: smallest `window` 17 488 bytes, median
448 552, largest `textarea` 558 792.

| Example | C files | C bytes | Headers | Header bytes | Executable |
| --- | --- | --- | --- | --- | --- |
| window | 7 | 16078 | 11 | 8524 | 17488 |
| hello | 58 | 788055 | 67 | 90997 | 329336 |
| actionmodal | 101 | 1499367 | 120 | 436778 | 480272 |
| alert | 97 | 1433335 | 115 | 428980 | 457280 |
| avatar | 87 | 1342601 | 102 | 421252 | 431944 |
| badge | 88 | 1340254 | 103 | 421130 | 427920 |
| bandchart | failed | failed | failed | failed | failed |
| banner | 97 | 1433413 | 115 | 429230 | 457576 |
| box | 60 | 836889 | 70 | 102271 | 284800 |
| breadcrumb | 101 | 1552036 | 120 | 443609 | 501200 |
| button | 96 | 1413863 | 112 | 425458 | 448312 |
| buttongroup | 95 | 1430159 | 111 | 429044 | 456960 |
| candlechart | failed | failed | failed | failed | failed |
| card | 97 | 1426474 | 115 | 428119 | 453368 |
| carousel | 101 | 1483740 | 120 | 437021 | 479872 |
| checkbox | 89 | 1390975 | 105 | 420504 | 439064 |
| checklist | 99 | 1453038 | 118 | 430801 | 465952 |
| collapsible | 67 | 920730 | 79 | 115131 | 317576 |
| colorpicker | 65 | 924435 | 78 | 115281 | 317272 |
| column | 98 | 1447480 | 117 | 431554 | 458104 |
| commandpalette | 109 | 1493741 | 123 | 444538 | 481168 |
| confirmbutton | 95 | 1430967 | 111 | 427004 | 452832 |
| confirmpopover | 106 | 1461063 | 120 | 438469 | 467912 |
| contextmenu | 99 | 1473582 | 117 | 431044 | 471352 |
| copybutton | 96 | 1424073 | 112 | 426992 | 448632 |
| countdown | 98 | 1436210 | 115 | 428998 | 457608 |
| counter | 96 | 1412167 | 112 | 425461 | 448272 |
| discreteslider | 65 | 901351 | 77 | 112301 | 312432 |
| dismissibleoverlay | 97 | 1429452 | 115 | 428822 | 453536 |
| dpad | 92 | 1371822 | 108 | 417816 | 429448 |
| dropdown | 97 | 1514099 | 115 | 439233 | 492352 |
| emptystate | 96 | 1420494 | 113 | 427230 | 452568 |
| fieldset | 63 | 860878 | 75 | 107343 | 294472 |
| flow | failed | failed | failed | failed | failed |
| gallery | failed | failed | failed | failed | failed |
| grid | 98 | 1447454 | 117 | 431546 | 462288 |
| gridmulti | 94 | 1435595 | 111 | 435800 | 463800 |
| group | 98 | 1447467 | 117 | 431550 | 453600 |
| guidedtour | 106 | 1473204 | 120 | 438970 | 476208 |
| holdbutton | 95 | 1423804 | 111 | 426797 | 452808 |
| hovercard | 72 | 896614 | 80 | 117347 | 312952 |
| image | 87 | 1322763 | 103 | 419574 | 415448 |
| inlineedit | 108 | 1593686 | 125 | 450013 | 514408 |
| joystick | 64 | 863254 | 75 | 106020 | 297760 |
| kanban | failed | failed | failed | failed | failed |
| keycap | 61 | 857367 | 71 | 105428 | 297776 |
| linechart | failed | failed | failed | failed | failed |
| linechartcompare | failed | failed | failed | failed | failed |
| link | 64 | 868874 | 76 | 108604 | 299224 |
| listbox | 65 | 915201 | 75 | 115642 | 321728 |
| loading | 61 | 839394 | 71 | 102882 | 288760 |
| loadingscreen | 96 | 1407144 | 115 | 430438 | 448472 |
| menu | 68 | 956081 | 79 | 116517 | 327200 |
| modalframe | 100 | 1481594 | 119 | 434039 | 471576 |
| moneyfield | 108 | 1600729 | 125 | 450594 | 518760 |
| navigationbar | 102 | 1568985 | 120 | 444857 | 518736 |
| navigationbarconfig | 107 | 1662263 | 128 | 458484 | 542432 |
| onetimecode | 66 | 889654 | 75 | 110657 | 303888 |
| page | 65 | 872481 | 77 | 109941 | 303408 |
| panedview | 62 | 872226 | 74 | 109012 | 302928 |
| plotbars | failed | failed | failed | failed | failed |
| plotlines | failed | failed | failed | failed | failed |
| popup | 61 | 843534 | 72 | 103891 | 289224 |
| press | 61 | 856156 | 72 | 104602 | 297752 |
| progress | 65 | 864960 | 77 | 110844 | 303040 |
| progressring | 98 | 1437508 | 115 | 429001 | 461784 |
| radio | 64 | 878313 | 76 | 110006 | 303656 |
| reorder | 63 | 877727 | 75 | 108448 | 303016 |
| row | 98 | 1447441 | 117 | 431542 | 458056 |
| screen | 98 | 1447480 | 117 | 431554 | 453576 |
| scroll | 64 | 924182 | 75 | 115085 | 324384 |
| scrubfield | 66 | 893573 | 76 | 111297 | 308096 |
| segmentedcontrol | 97 | 1442971 | 115 | 430359 | 462272 |
| selectable | 64 | 868282 | 76 | 109201 | 303448 |
| separator | 64 | 867160 | 76 | 109334 | 303344 |
| shape | 61 | 849572 | 72 | 104928 | 293656 |
| sheet | 106 | 1463592 | 120 | 438820 | 467808 |
| shortcutrecorder | 66 | 886691 | 75 | 110462 | 303928 |
| slider | 67 | 914919 | 80 | 117264 | 317264 |
| spinbox | 99 | 1439183 | 117 | 429734 | 462344 |
| stack | 98 | 1447467 | 117 | 431550 | 457896 |
| stylepicker | 97 | 1426040 | 115 | 428167 | 453456 |
| svgicon | 86 | 1328572 | 100 | 411386 | 410912 |
| swipearea | 65 | 868123 | 76 | 106758 | 297904 |
| swiperow | 89 | 1372734 | 106 | 417893 | 433104 |
| tabbar | 68 | 997414 | 80 | 127398 | 360328 |
| tableview | failed | failed | failed | failed | failed |
| taginput | 108 | 1606435 | 125 | 450746 | 518888 |
| text | 95 | 1447310 | 109 | 427377 | 462552 |
| textarea | 106 | 1674133 | 124 | 462086 | 558792 |
| textfield | 107 | 1582462 | 125 | 449127 | 514544 |
| tilemap | 88 | 1327529 | 104 | 420558 | 419336 |
| timeline | 88 | 1352151 | 103 | 414939 | 424640 |
| timepicker | 103 | 1564263 | 122 | 446529 | 515808 |
| titlebar | 104 | 1589953 | 124 | 451243 | 524224 |
| toast | 97 | 1439826 | 115 | 429379 | 462160 |
| toggle | 63 | 892958 | 75 | 110805 | 307704 |
| toolbar | 104 | 1587984 | 124 | 450622 | 523992 |
| tooltip | 72 | 886759 | 80 | 115410 | 308544 |
| transition | 96 | 1421368 | 113 | 427551 | 452936 |
| treeview | failed | failed | failed | failed | failed |

### Before and after

Kryon 07d74f6d exported with `git archive` into
`build/scratch/measure/before-07d74f6d/`, the same example sources, built with
today's Ziran:

| Example | Before C files | Before C bytes | Before exe | After C files | After C bytes | After exe |
| --- | --- | --- | --- | --- | --- | --- |
| window | 62 | 665 364 | 265 304 | 7 | 16 078 | 17 488 |
| hello | 79 | 989 958 | 402 736 | 58 | 788 055 | 329 336 |
| box | 79 | 1 026 647 | 334 528 | 60 | 836 889 | 284 800 |
| button | 93 | 1 389 644 | 444 144 | 96 | 1 413 863 | 448 312 |
| image | 84 | 1 298 544 | 411 296 | 87 | 1 322 763 | 415 448 |
| text | 92 | 1 423 047 | 454 304 | 95 | 1 447 310 | 462 552 |

Window, hello and box shrank by the empty-window work. Button, image and text
grew by a few files: today's Kryon adds modules their widget imports reach.

### Modules outside an example's imports

All 88 frame examples carry these although their `.zi` imports do not name
them (`examples/window.zi` and `examples/hello.zi`, the two `App.Show` and
`App.Run` programs, do not):

| Module | Likely caller |
| --- | --- |
| `cursor` | `src/ui/tree.zi`, `src/ui/tree_frame.zi`, `src/ui/tree_input.zi` (any widget reaches `tree`) |
| `scroll_device` | `src/ui/tree_draw.zi`, `src/ui/tree_frame.zi`, `src/ui/tree_input.zi`, `src/ui/scroll/scroll.zi`, `src/ui/tabs/tabs.zi`, `src/ui/textarea/textarea.zi`, `src/ui/gridmulti/gridmulti.zi` |
| `gamepad_navigation` | `src/ui/tree_frame.zi` |
| `text_request` | `src/ui/tree_draw.zi`, `src/ui/text_boundary.zi`, `src/ui/textarea/textarea.zi`, `src/ui/textfield/textfield.zi` |
| `text_input_platform` | `src/ui/tree_draw.zi`, `src/ui/textarea/textarea.zi`, `src/ui/textfield/textfield.zi` |
| `program_title` | `src/backend/desktop_loop.zi`, `src/backend/desktop_runtime.zi`, `src/backend/cairo_raster.zi` |
| `accessibility_linux` | `src/backend/desktop_loop.zi`, `src/backend/accessibility_tree_linux.zi` |
| `accessibility_tree_linux` | `src/backend/app_host_desktop.zi`, `src/backend/desktop_run.zi` |
| `atspi_protocol` | `src/backend/accessibility_tree_linux.zi` |

Fifty-eight of those 88 also carry the whole picture chain although they draw
no picture. `box` draws shapes and text and does not: the chain arrives when
`cairo_raster` or a widget import reaches `src/ui/image/` (button imports
`kryon/image` for its icon slot).

| Module | Likely caller |
| --- | --- |
| `image_decode_linux` | `src/backend/cairo_raster.zi` (line 263), `src/ui/image/` |
| `avif_decode`, `ziran_image_decode`, `riff`, `bmff` | `image_decode_linux`'s picture chain |
| `native_api` | the image-codec package (`WebpNativeDecodeInto` in `p*_dynamic.c`) |
| `lanczos_cache` | `src/backend/cairo_raster.zi` (line 270) |
| `std_thread_linux`, `std_parallel_linux` | `lanczos_cache` and the picture chain |
| `app_asset_path` | `src/backend/cairo_raster.zi`, `src/backend/app_asset.zi`, `src/backend/raylib_runtime.zi` |

Only `examples/gallery` carries `video_linux` and `test_driver` (its
`src/ui/gallery/` uses both). `gallery` itself fails to build today, so this
comes from its imports rather than a measured build.

Suspect (listed, caller not traced): none beyond the modules above.

## Why an empty window is big

Ziran's pruning works: a procedure is emitted only when something reachable
calls it, a `#foreign host_api` call links the host's export of that name
only when the call is reachable, and a module with nothing reachable gets no
`.c` file. The output is large because the desktop loop calls every optional
feature directly, so each is reachable from `main`:

| Feature in an empty window | The direct call that keeps it |
| --- | --- |
| PNG/WebP/AVIF decoders, asset paths | `DesktopCursor` decodes cursor pictures (`CairoImagePixels`) |
| Lanczos cache, threads, parallel | `BeginRasterFrame` pumps, `CloseRaster` closes, capture waits, open sets its wake |
| Video | `CloseDesktop` calls `VideoStopHost` |
| Tree, paint queue, input routing | the loop runs `BeginFrame`, `EndFrameRetained`, `TreePointerUpdate`, `KeyboardSend` |
| Text input, text requests, bitmaps | `BeginFrame`, `CloseSession` and accessibility reach them |
| Cursor shapes and themes | `DesktopCursor` every frame |
| Scroll devices | `BeginScrollFrame` every frame |
| Gamepad navigation | `PollDesktop` maps controller buttons through it |
| Test driver | `TestDriverPointer`, `TestDriverWheel`, `TestDriverFrame` |
| Accessibility nodes and roles | `AccessibilityOpen` and `AccessibilitySync` publish the tree |
| Program title file | `OpenDesktopSized` reads the title file beside the program |
| Fonts | `OpenRaster` loads the Kryon font before any text |

## Design

One rule: a feature is reached only from the module a program imports to use
it, through registration, never a run-time flag.

1. **Frame driver.** The desktop loop keeps the window, events, clearing and
   presenting. Everything that needs the widget tree (input routing, frame
   begin and end, scroll frames, cursor shape, test-driver input) is one
   portable frame driver that `tree_draw` (the `Frame` export) registers into
   a small slot module from its module initializer. Without a widget import
   the loop clears and presents.
2. **Host-internal features register on first use.** The rasterizer registers
   its Lanczos pump, close and wait when it first makes a smooth copy; video
   registers its stop when a video opens; the font loads with the first text.
3. **Cursor pictures** come with `kryon/cursor` (`cursor_picture` registers a
   decoder that asks the host's `CursorPictureHost`), as in c80e022f. It was
   reverted only because Ziran's Go check treated storing a procedure as
   calling the host; that check is fixed in Ziran first.
4. **Accessibility.** The host keeps the AT-SPI connection and the
   application and window objects. Widget roles, states, children, actions
   and text come from a portable provider that the tree registers.
5. **Program title.** `App.Run` passes the window's title straight to the
   window; only the frame host reads the title file the project tool writes.

## Ziran's part

- `zi2go`'s startup check refused a module initializer that stores a
  procedure which later calls a host. Go binds the host interface before the
  first frame, so only a call made during startup is wrong. The check now
  follows calls only.

## Frame driver: where it registers

The first attempt registered the driver from `kryon/tree`'s module
initializer. Two things broke: a module with an initializer turns any
existing import cycle among the modules that import it (such as `modal` and
`modal_frame`) into a fatal startup cycle, and test builds without `--entry`
keep every `#program_export` of every imported module, so importing `tree`
suddenly required a full raster host. The driver is therefore registered by
an explicit call, `TreeFrameUse()`, from the hosts that run frames: the frame
host `desktop_run` and `App.Run(window, Draw)`. `App.Show(window)` never
makes that call, so an empty window carries none of it. Ziran has no
overloading and no compile-time branch on a generic's type, so the
window-only entry has its own name. Accessibility nodes are registered the
same way (`AccessibilityTreeUse()`).

## Steps

Open work only; finished steps are described in Design and pushed.

- [ ] Pictures only with a picture: `cairo_raster` reaches `image_decode_linux`
  and `lanczos_cache` directly (lines 263 and 270), and `button` imports
  `kryon/image` for its icon slot, so 58 frame examples carry the decoders,
  threads and asset paths without drawing a picture. Register the picture
  path from the module that draws a picture; a button without an icon
  carries none of it.
- [ ] Text input only with a text field: `tree_draw` reaches `text_request`
  and `text_input_platform` in every frame example.
- [ ] Gamepad navigation and scroll devices only through the widgets or
  inputs that use them (`tree_frame`, `tree_draw`).
- [ ] Extend `make pay-per-import-test` to assert each of the above (box and
  button carry no decoder or text input; image and textfield do).
- [ ] Eleven examples don't build today: ten chart and table examples fail
  with "unknown initializer field: font" (`Text.Props` still has `font`),
  and `flow` fails because `zi2c` exits 127. Find the cause and fix it, or
  route a Ziran-side cause to the Ziran owner.
- [ ] `make check` once Ziran's shared native code generator edits land (it
  currently fails from other lanes' in-progress work), then commit and push.
