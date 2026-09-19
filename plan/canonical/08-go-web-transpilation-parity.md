# Remaining backend parity proof

`make generate-runtime` generates C/Go policy; focused keyboard policy tests
also compile all runtime modules to JS and execute the new decisions.
`make generated-runtime-parity-test` requires Node and compares shared fixture
outputs. This establishes tested cases, not blanket runtime equivalence.

Remaining:

- Replace handwritten policy in Go/web host runtimes with generated decisions.
- Execute scroll_content and the remaining composed_popup cases in JS with
  matching native/Go event sequences. Composition, drag_drop, and menus now run
  their generated behavior checks in C/Go/JS. Scroll_content has partial JS
  coverage for clipping, wheel offset, nested content, mixed controls/text
  editing, dropdown overlay capture, dropdown keyboard/flipped-popup selection, dropdown scrollbar/edge-popup selection, rotated table hit geometry/resize, custom table-cell layout/disabled editing, and tree opening/keyboard navigation; composed_popup has
  partial JS coverage for content/tools/tooltip/modal-open, modal capture/Escape, and context popup behavior. The web runtime has
  queued mouse move/down/up/wheel primitives and frame-scoped key/mouse queries;
  remaining fixture-level routing/capture promotion remains open. Extend
  Disabled/TableCell/Canvas scope restoration, nesting and early-exit coverage.
- Add real lowering for every supported expression form. Unsupported lowering
  now fails visibly instead of emitting the removed `kryon.expr` runtime
  placeholder, executable fixture generation rejects any placeholder calls, and
  fixed-capacity module state/global/local `sizeof(...)` lowers to the
  declared capacity, and fixed-array element-size operands support
  `sizeof(array) / sizeof(array[0])`; accelerator compound literals lower
  through the web runtime shortcut helper for generated fixture execution.
  Broader computed capacities remain open.
- Report coverage per backend and fixture; distinguish generating, executing,
  comparing state, and checking rendered output.
- Verify every policy change in C, Go, and JS; syntax and snapshots are useful
  guards but do not establish interaction parity.

Use `make go-runtime-test`, not the old cross-module root Go command. See
`README.md` for the full validation sequence and completion requirements.
