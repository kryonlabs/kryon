# Magnetic Lightfield visual baseline

The user approved the generated implementation on 2026-09-09 as the new
visual benchmark. These are unchanged copies of the native C captures shown
for that approval: `dark.png`, `light.png`, and `split.png`.

The original `../widget-proposals/08-magnetic-lightfield.png` remains a design
reference, not a pixel-exact acceptance requirement.

Run `make lightfield-reference-test` after generating native captures to compare
every pixel of all three frames against this baseline. The test never updates
these files. Future baseline replacements require explicit visual approval.

Run `make lightfield-test` for fresh native and Go captures, followed by the
native baseline, both runtimes' rendered animation checks, and translation
checks. This requires `xvfb-run` and the normal native build dependencies.

The layout is defined in `examples/02_buttons.kry`. Shared defaults, style
resolution, surface layers, and transition rules live in `runtime/theme.kry`,
`runtime/button.kry`, `runtime/style.kry`, and `runtime/surface.kry`.

For optional original-proposal diagnostics:

```sh
python3 tests/lightfield_reference_test.py --reference design/widget-proposals/08-magnetic-lightfield.png
```

These static native captures do not certify animation behavior or pixel parity
with Go or JavaScript. The motion, translation, and backend checks remain
separate tests.
