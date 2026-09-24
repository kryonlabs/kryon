# Button gallery

Historical audit only. The `.kry` gallery source and the build targets below
were removed during the Ziran split. The current runnable example is
[`hello.zi`](hello.zi), built with `make -C examples`; it exercises a checked
Ziran Button through the portable SVG host. The historical commands and
coverage claims below do not describe the current build.

[`28_button_gallery.kry`](28_button_gallery.kry) owns the gallery UI and PNG
capture loop. `make button-gallery` compiles it to a native app. It refuses to
open unless
`KRYON_PRIVATE_DISPLAY=1` and a nonzero X display are set. Run it only inside a
private Xvfb or Xephyr session. The capture target starts its own Xvfb display
and clears inherited `DISPLAY` and `WAYLAND_DISPLAY`:

```sh
make button-gallery-capture
```

This writes 24 PNGs to `build/linux-x86_64/button-gallery-builtins/`, after
two settling frames per page so each saved frame uses its requested theme.
Twenty matrix pages show every `ButtonTone` (neutral, accent, danger,
success, warning) against every `ButtonEmphasis` (filled, soft, outline, ghost,
link) and seven states. They cover Material dark, Classic light, and Lightfield
dark with glow on and off. Four more pages show sizes, content, and shape
variants for those same cases. The running
gallery also has an automatic-state button for mouse and keyboard exploration.
Use the left and right arrow keys to change pages.

Button verification is authored in Kry: `tests/button_interaction_trace.kry`
checks pointer, keyboard, disabled, and loading behavior, while
`tests/button_kss_test.kry` checks button policy and resolves all 700 built-in
theme, effect, tone, emphasis, and state combinations. Their explicit Make
targets are `button-interaction-trace-test` and `button-kss-test`. Both require
a private X display. The native C files under the build directory are compiler
output from these `.kry` sources.

The built-in style packs currently define fixed palettes: Material and
Lightfield are dark, and Classic is light. Kryon reselects Material when no
pack is active, so the gallery selects one of these packs explicitly on every
page. App theme colors do not recolor the fixed style-pack palette.

The isolated performance run uses the same `.kry` cells:

```sh
make button-gallery-benchmark
```

It writes `build/linux-x86_64/button-gallery-benchmark-isolated.csv` on a private
Xvfb display. Each matrix page runs each emphasis separately, with 20 warm-up
frames and 100 measured frames per emphasis and the FPS limit disabled. Its
seven buttons cover all states for one tone and emphasis. `button_call_us`
measures the `Button(ButtonProps)` call; `frame_us` measures the whole frame
through presentation, including those seven buttons and the fixed labels.
Variant pages record whole-frame time with all variants visible. The benchmark
has 70,400 data rows when complete (70,000 matrix cells plus 400 variant
frames). The benchmark
does not yet establish a target or regression threshold; review the visuals
and the named-machine baseline before proposing one. Obtain approval before
each run.

An approved baseline run on AMD Ryzen 9 9950X with Mesa llvmpipe is recorded
in `plan/button/REVIEW.md`; its analyzer output is
`build/linux-x86_64/button-gallery-benchmark-summary.csv`. The measurements
include software rendering and gallery labels, so compare future runs in the
same environment and with the same cells.

These images are actual Kryon rendering, not the design illustration in
`docs/assets/button-widget-sprite-sheet.png`. They are review artifacts until
the appearance is approved; creating a screenshot does not establish visual
correctness. The loading indicator animates with elapsed time, so its exact
pixels are not yet suitable for an unmasked golden image comparison.
