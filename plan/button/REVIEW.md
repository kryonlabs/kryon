# Button verification review

The dated cross-project missing-pieces checklist is
[../UI_MIGRATION_REMAINING.md](../UI_MIGRATION_REMAINING.md). A passing capture
or benchmark is valid only for its source revision; the current uncommitted
paint, style board, and gallery changes still require their own approved
visual and timing evidence.

Status: the Button widget and its retained UI decisions are authored in
`.kry`. The repository-wide UI migration and revision-specific visual
verification remain open. The generated-runtime parity test previously passed
for both the C and Go runtimes; it has not been rerun for the current changes.
The Bend candidate laws and new checks below remain proposals, and each
manual test still requires approval before running.

## Current evidence

- `examples/28_button_gallery.kry` renders the public `Button(ButtonProps)`
  surface and exports PNGs after settled frames. Its 24 pages cover each
  built-in style pack and Lightfield with glow enabled and disabled.
- `docs/assets/button-widget-sprite-sheet.png` is a design illustration.
- `tests/style_capture_boards.kry` builds a ten-board capture across Material,
  Classic, Lightfield flat, Lightfield glow, and no style in light and dark
  modes. Its PNG output awaits approval and visual review. The separate button
  gallery contains the full tone, emphasis, and state matrix.
- `law/program` contains nine candidate Bend button laws and an exhaustive
  Boolean comparison against generated C. They are not yet on `master`.
- `tests/control_appearance_perf_test.c` currently measures one button recipe.
- The latest approved 24-page capture under private Xvfb exited successfully and
  produced `build/linux-x86_64/button-gallery-builtins/*.png`. The pages show
  Material dark, Classic light, Lightfield flat dark, and Lightfield glow dark;
  each has five tone matrices and a variants page. Visual review confirmed the
  full-color asset, inset swatch, larger glyph, and distinct Classic Outline
  and Ghost treatments. A second approved 24-page capture verified the `.kry`
  variants layout after spacing its rows and changing the Info example from
  56 to 36 pixels. The updated Info circle and captions are clear in Material,
  Classic, Lightfield flat, and Lightfield glow variant images.
- The approved isolated benchmark under private Xvfb exited successfully,
  produced 70,400 data rows in
  `build/linux-x86_64/button-gallery-benchmark-isolated.csv`, and passed the
  analyzer's row/state/sample checks. Its summary is
  `build/linux-x86_64/button-gallery-benchmark-summary.csv`.
- The Classic pack was revised toward square, gray, flat controls with muted
  tone colors. An approved 24-page recapture exited successfully; visual review
  of the Classic neutral, accent, danger, and variants pages confirms the
  gradient is gone and the fills are flat. The user accepted this revised
  Classic appearance. The Classic benchmark rows above
  predate this change; a separately approved benchmark run is needed before
  using them as the current baseline.
- Info indicator paint geometry and the swatch's KSS-driven rounding were then
  moved into `runtime/button.kry`. Native and Go runtime generation and the
  gallery build succeeded. The accepted Classic capture predates this small
  rendering refactor; a separately approved capture is needed to verify that
  its pixels remain acceptable.
- Earlier 14-page and 48-page runs found stale first-frame styling and
  mislabeled default/custom pages. The 24-page source waits two settling
  frames and names only the four actual built-in treatments. Earlier runs
  returned status 1 in the workspace sandbox after writing artifacts because
  private Xvfb cleanup failed there. The approved runs outside the sandbox
  returned status 0. No inherited desktop display was used for capture or
  benchmark.
- `runtime/tree.kry` now owns the extracted retained identity hash/match and
  Button-like eligibility decisions. Generated C and Go output exists, but no
  parity or performance test for this extraction has been run yet.
- `tests/button_interaction_trace.kry` is a compiled, unrun interaction
  harness. It sends SDL input events only to its own hidden window and checks
  Button activation counts in `.kry`. Its Make target clears inherited display
  variables and starts a private Xvfb display. It awaits approval for its
  first test run.
- `src/ui/button.c` has been deleted. Native Button and SegmentedControl
  entry points now come from `src/ui/button.kry` and
  `src/ui/segmented_control.kry`. Button image content is one nested
  `ImageProps` field; both C and Go consume it without the former six fields.
  The terminal Button path resolves its colors and border width from KSS and
  the former hardcoded fallback policy is removed. Native runtime generation,
  the gallery link build, and `go build` succeeded. Interaction, visual,
  benchmark, and runtime parity runs after this migration await approval.
- The repository-wide migration has also removed handwritten C for icon
  controls, guide pager, Toast, swipe, dismissible overlay, style picker,
  Toolbar, transitions, view layout, scaling, and color helpers. The complete
  retained tree implementation moved from `ui_tree.c` to `src/ui/*.kry`, and
  `ui_tree.c` was removed. The gallery links; the style picker and transition
  test executables compile, but neither test was run without approval.
  `ui.c` and the widget modules have since been migrated to `.kry`; the
  generated-runtime parity test now passes for both the C and Go runtimes.

## Isolated benchmark baseline

The table shows median whole-frame milliseconds for seven state cells of one
emphasis. Each value aggregates 500 measured frames: 100 per tone, with 20
warm-up frames for that tone/emphasis. The fixed gallery labels are included.
The button-call timer does not include all deferred paint work. Measurements
were on an AMD Ryzen 9 9950X with Mesa llvmpipe under private Xvfb. The
summary CSV also has p95 frame and Button-call timings for every case.

| Treatment | Filled | Soft | Outline | Ghost | Link |
|---|---:|---:|---:|---:|---:|
| Material dark | 2.054 | 2.076 | 2.014 | 1.941 | 1.848 |
| Classic light | 1.930 | 1.899 | 1.924 | 1.770 | 1.724 |
| Lightfield flat dark | 2.283 | 2.255 | 2.043 | 1.921 | 1.877 |
| Lightfield glow dark | 6.206 | 6.840 | 3.408 | 5.149 | 4.721 |

Glow costs substantially more in this software-rendered environment. The
variant-page timings and run order show that frame medians are sensitive to
workload and environmental noise; these numbers are a baseline for this
environment, not universal performance targets or proof of an optimization.

## UI source rule and proposed Bend enforcement

**`ui.source.kry.canonical`:** Every maintained UI tree, widget composition,
interaction, layout, style, and capture decision must have one canonical
`.kry` implementation. Handwritten C may implement only narrow platform and
renderer effects requested by that `.kry` code; it must not retain an
independent UI tree or choose UI behavior. Generated C from `.kry` is build
output, not an alternate source. New handwritten C UI behavior is prohibited.

The retained tree and widget C modules have been removed, and their decisions
live in `.kry`. Repository-wide enforcement is still open: native window event
arbitration needs an ownership pass, handwritten UI headers remain, and the
remaining native raster/cache modules need a final policy audit. The
generated-runtime parity test passed for both C and Go on an earlier revision;
the current working tree is unverified.

## Candidate laws from `law/program`

| Name | Exact behavior to approve |
|---|---|
| `action_disabled_blocks` | For either content-disabled value, a disabled button cannot act. |
| `action_content_disabled_blocks` | Disabled content blocks an otherwise enabled button. |
| `action_enabled_when_clear` | A button can act when neither blocking flag is set. |
| `activate_disabled_blocks` | For either loading value, a disabled button cannot activate. |
| `activate_loading_blocks` | A loading button cannot activate when otherwise enabled. |
| `activate_clear_allows` | An enabled, nonloading button is eligible to activate. |
| `menu_click_toggles_open` | Clicking a closed menu opens it. |
| `menu_click_toggles_closed` | Clicking an open menu closes it. |
| `menu_no_click_preserves` | Without a click, open and closed menu values remain unchanged. |

These laws cover pure policies only. Eligibility to activate is distinct from
an actual delivered click, and none of these laws proves renderer output.

## Candidate state precedence laws

The domain is the eight declared `ButtonState` values and six Boolean inputs:
disabled, loading, pressed, hovered, focused, and selected. The comparison
domain is 8 × 2^6 = 512 rows. Each proposed law below applies to every value
of the inputs that are not explicitly constrained in its statement.

| Name | Exact behavior to approve |
|---|---|
| `state_disabled_wins` | If disabled is true, `ResolveState` returns Disabled. |
| `state_loading_wins` | If disabled is false and loading is true, it returns Loading. |
| `state_explicit_wins` | If neither blocks and explicit state is not Auto, it returns that explicit state. |
| `state_pressed_wins` | With Auto, no blocker, and pressed true, it returns Pressed. |
| `state_hover_wins` | With Auto, no blocker, no press, and hovered true, it returns Hover. |
| `state_focus_wins` | With Auto, no blocker, no press or hover, and focused true, it returns Focus. |
| `state_selected_wins` | With Auto, no blocker, no press, hover, or focus, and selected true, it returns Selected. |
| `state_normal_default` | With Auto and all flags false, it returns Normal. |

The planned Bend model must compare all 512 rows with generated production C;
the model proof alone is not a proof of the generated implementation. Rendering,
pointer ownership, keyboard routing, and pixel appearance require other checks.

## Proposed manual checks

Approval is requested separately for each run:

1. `button-gallery-capture`: render 24 gallery PNGs under private Xvfb for
   review. The animated loading ring is excluded from exact pixel baselines
   until its capture time can be fixed.
2. `button-bend-laws-test`: check the nine approved policy laws, compare all
   Boolean rows with generated C, and reject a deliberate mutation.
3. `button-state-bend-laws-test`: check approved precedence laws and compare
   all 512 rows with generated C.
4. `button-interaction-trace-test`: run the `.kry` harness on a private display.
   Its ten cases check one enabled pointer activation; press inside/release
   outside; press outside/release inside; disabled and loading pointer blocks;
   Tab focus; Enter and Space activation once each; and disabled Enter and
   loading Space blocks. It targets the ordinary `Button(ButtonProps)` path;
   variant-specific interaction traces remain future work.
5. `button-visual-regression-test`: compare only visually approved captures in
   a fixed rendering environment. Any baseline update requires visual review.
6. `button-appearance-parity-test`: compare resolved state and style values
   across supported generated targets.
7. `button-gallery-benchmark`: record 100 measured frames per emphasis and
   theme page after 20 warm-up frames, isolating each emphasis's seven states
   in the same `.kry` gallery cells. It also records each Button call and
   whole-frame time. Lightfield flat and glow have separate pages. Review
   machine details, median, spread, and whether style differences are visually
   meaningful before setting targets.

No timing threshold should be inferred from an unmeasured baseline. Report
machine details, median and spread. An optimization proposal must identify the
hot operation and show its before/after values without regressing other states.
