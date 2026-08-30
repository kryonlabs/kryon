# Widget Conformance

Kryon widgets must be tested at the runtime boundary, not only inside downstream
apps. Each widget family should have the same coverage shape:

1. Direct Go runtime tests in `go/kryon/*_test.go`.
   These drive public widget APIs with `QueueTap`, `QueueText`, `QueueKey`,
   focus, selection, and frame rendering. They catch native Go runtime bugs.

2. Generated runtime parity in `tests/generated_runtime_parity_test.sh`.
   Fixtures in `tests/parity/*.kry` are lowered to Go, C, and JS, driven
   through the same workflow, rendered where the target has a renderer in the
   harness, and compared by final state JSON.

3. Backend or platform translation tests.
   Backend event adapters must translate native events into the same public
   input queue contracts. Example: `window_linux_test.go` verifies X11 keysyms
   for printable text, Backspace, Enter, and shortcuts.

4. Performance harnesses for hot widgets.
   Text input has `make perf-text-input`, which validates generated lowering and
   measures retained-core C typing, backspace, selection replacement, tab
   traversal, idle frames, and random-focus precision.

Required workflow coverage for editable widgets:

- click/tap focus
- focus switch between multiple fields
- printable text insertion
- Backspace and Delete
- Enter/commit
- Tab and Shift-Tab traversal
- selection replacement
- clipboard copy/cut/paste where supported
- secure text rendering does not leak plaintext
- Unicode cursor movement and deletion
- stable frame operation counts under long input

Required workflow coverage for collection widgets:

- single click selection
- double-click activation
- keyboard navigation
- scrolling
- context/right-click selection where supported
- selected item/cell rendering without unintended row or panel highlights

New widgets are not complete until their direct runtime test, generated parity
fixture or explicit unsupported note, backend event coverage where relevant, and
benchmark coverage for hot paths are present.
