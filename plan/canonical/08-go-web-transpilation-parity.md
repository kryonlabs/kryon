# Remaining backend parity proof

`make generate-runtime` generates C/Go policy; focused keyboard policy tests
also compile all runtime modules to JS and execute the new decisions.
`make generated-runtime-parity-test` requires Node and compares shared fixture
outputs. This establishes tested cases, not blanket runtime equivalence.

Remaining:

- Replace handwritten policy in Go/web host runtimes with generated decisions.
- Execute menus, scroll_content, drag_drop, composition, and composed_popup in
  JS with matching native/Go event sequences. Extend Disabled/TableCell/Canvas
  scope restoration, nesting and early-exit coverage.
- Add real lowering for every supported expression form. Unsupported lowering
  now fails visibly instead of emitting the removed `kryon.expr` runtime
  placeholder, and executable fixture generation rejects any placeholder calls.
- Report coverage per backend and fixture; distinguish generating, executing,
  comparing state, and checking rendered output.
- Verify every policy change in C, Go, and JS; syntax and snapshots are useful
  guards but do not establish interaction parity.

Use `make go-runtime-test`, not the old cross-module root Go command. See
`README.md` for the full validation sequence and completion requirements.
