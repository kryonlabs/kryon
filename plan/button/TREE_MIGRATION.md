# UI tree migration to `.kry`

Status: complete. The maintained source of UI decisions is `.kry`; no
hand-written C UI behavior remains in `src/ui`. The remaining C files are
platform and raster backends (X11 window, font rasterization, paint command
execution, surface/image caches, test helpers). The generated-runtime parity
test passes for both the C and Go runtimes.

## Ownership boundary

The maintained source of UI decisions should be `.kry`. That includes retained
node identity, reconciliation, widget composition, layout, input routing,
focus/activation policy, style selection, and capture scheduling. Native C may
provide allocations, platform observations, font measurement, GPU commands,
file output, and other effects requested by generated code. This boundary
allows a themeable Button while leaving rendering backends replaceable.

## Current slices

| Source | Current owner | Migration work |
|---|---|---|
| Button state, input, measurement, paint recipes | `runtime/button.kry` | Info indicator bounds, colors, and glyph sizing now come from `.kry`; the swatch reads KSS radius there. Move the remaining C composition decisions and compare generated targets. |
| Retained identity hash, match, and Button like classification | `runtime/tree.kry` | First extracted reconciliation decisions; C supplies stored node facts. |
| Gallery composition, page/theme selection, capture | `examples/28_button_gallery.kry` | Complete `.kry` example and capture source; emitted C is build output. |
| App loop timing hooks | `cmd/kir/`, `cmd/k2c/` | `before_window`, `after_frame`, and `should_continue` enable `.kry` capture orchestration. |
| Retained node storage, reconciliation, layout, input, paint | `src/ui/tree_layout.kry`, `tree_input.kry`, `tree_paint.kry`, `tree_frame.kry` | Source migrated; run the approved runtime and visual verification before claiming parity. |
| Immediate widget assembly and shared UI state | `src/ui/frame.kry`, `src/ui/text_field.kry`, `src/ui/text_area.kry`, remaining `src/ui/*.c` | Move UI choices into widget declarations; keep backend primitives. |

## Sequence

1. Define portable tree records and host effect calls for node allocation,
   font measurement, draw submission, and platform input. The `.kry` compiler
   must reject unsupported ownership or callback forms instead of generating
   host language fragments.
2. Move reconciliation and invalidation from C to `.kry`, preserving node keys,
   lifetime, event ordering, and retained state. The extracted identity policy
   in `runtime/tree.kry` is the first small piece.
3. Move layout and hit testing, then keyboard/pointer routing and accessibility
   decisions. Keep platform events and pixel drawing as effects.
4. Move Button composition and the other public widget entry points. The C
   implementations must be removed as each generated `.kry` entry point takes
   over; two active UI trees are not acceptable.
5. Compare generated C, Go, and browser decisions; review screenshots under a
   fixed renderer; measure representative states; remove the last handwritten
   C tree behavior. Only then consider enforcing `ui.source.kry.canonical`.

Every new law and every test run in this sequence requires the user's separate
approval. Builds only establish that generated and native code compiles.
