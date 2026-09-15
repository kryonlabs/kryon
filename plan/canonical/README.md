# Canonical migration: remaining work

Audited 2026-09-14 against Kryon master, starting at `0a79e330`.
The public naming cleanup is complete; the entire policy migration is not.
This folder tracks unfinished work. Standing API rules live in `AGENTS.md`;
the authoritative surface and ownership audit is `docs/CANONICAL_WIDGET_SURFACE.md`.

## Completed and removed from the task list

- Canonical public names and private lowered scope exports: the name, surface,
  and documentation guards pass. Removed the completed public-surface plan.
- Reorder release/capture/commit/cancel policy: committed in `c2a1a1c4`, with
  focused decision tests. Do not restart that migration.
- Subsequent focus, menu accelerator, list/table, slider/drag, and text keyboard
  migrations are already on master.
- This cleanup routes immediate menu Escape/dismissal suppression, Collapsible
  arrow priority, and text shortcut interpretation through `.kry`. Go menu
  Escape, Collapsible keyboard behavior and text edit commands use shared policy.
- Removed the Go desktop pointer compatibility fallback. The desktop host uses
  its current mouse interface.
- Audited all 34 C release-consumption calls: they apply generated decisions;
  the separate `ConsumeRelease` definition is host input state. See the shared
  surface document for the file/owner table.
- Added generated JavaScript and Go keyboard policy tests. Native focused
  tests cover capture, simultaneous keys, and release boundaries.
- Updated the browser-generated table column fixture to canonical
  `TableColumnGroup`/`TableColumn` names.
- Removed obsolete per-button style overrides from the button example; its
  appearance now comes from its existing Material KSS pack.
- Web lowering consumes native paint-scope end markers instead of leaking them
  into recorded statements. Parent-path metadata owns the declarative tree;
  the separate disabled-input stack retains executable teardown.
- Three-backend parity now requires Node instead of reporting JS success after
  skipping it. Rendering assertions explicitly select a style pack.
- Removed separate standing test/execution documents; their commands and order
  are below. Their deletion does not mean all runtime behavior is verified.

## Remaining execution order

1. Finish the host/policy inventory, especially handwritten Go and web behavior
   (`02`, `03`, `07`). Public names alone are not evidence of shared execution.
2. Finish text ownership and layout decisions (`04`, `05`). Keep true platform
   services native; do not replace them with aliases or forwarding wrappers.
3. Remove web expression placeholders and complete executable block paths
   (`06`, `08`), then run matched event sequences on C, Go, and JS.
4. Map every requirement to source ownership and passing tests before deleting
   its remaining plan document.

## Validation

Run from the repository root:

```sh
make generate-runtime
make fast-test
make reorder-policy-test paragraph-policy-test text-policy-test image-policy-test icon-policy-test
make generated-runtime-parity-test
make k2go-syntax-test k2js-syntax-test k2js-runtime-snapshot-test
make go-runtime-test
python3 tests/canonical_widget_surface_doc_test.py
sh tests/public_api_names_test.sh
sh tests/canonical_surface_test.sh
make build/linux-x86_64/tests/ui_tk_test
xvfb-run -a build/linux-x86_64/tests/ui_tk_test
git diff --check
```

`go/kryon` is a separate module: use `make go-runtime-test` or run `go test ./...`
inside it. The old root command `go test ./... ./go/kryon/...` is invalid: it
crosses module boundaries and includes a directory of native C tests.
Use the Make target for generated parity so it supplies native link flags.

## Completion boundary

Do not claim zero compatibility debt across the entire repository just because
public-name scanners pass. Web lowering still emits expression placeholders,
and host runtimes still duplicate policy. Delete those paths only after their
maintained callers execute through the canonical implementation and parity
proves the replacement. No compatibility alias is an acceptable fix.

## Verified in this cleanup

Passed: runtime generation; fast/public-surface/header checks; menu,
Collapsible, text-input, reorder, paragraph, text, image and icon policy tests;
Go runtime tests; Go and JS syntax tests; web KSS control-style tests; JS
snapshots; generated C/Go/JS parity; both real-browser DOM suites; retained
`ui_tk_test` under Xvfb; and generated C syntax for the updated button example.
Initial failures in obsolete button styling, leaked web end-scope statements,
and browser table-name fixtures were fixed and their checks rerun successfully.
The root cross-module Go command was replaced with the real module test target.

## Inbe interaction follow-up

- SDL pointer input now preserves press and release edges drained in one poll.
  The native regression proves the raw backend misses that tap, the shared
  frontend activates once, quick drags remain blocked, and other-window edges
  do not leak into the app.
- Input capture scopes can be popped without clearing outer captures; native
  UI regression covers nested inert previews.
- All four built-in packs give selected buttons a visible border. Built-in pack
  coverage and style-picker tests cover the four supported choices.
- Downstream startup must bundle the KSS assets and explicitly select its pack.
  A successful native build alone does not prove controls are styled or clickable.

The policy migration work listed above remains open. These interaction fixes
are not evidence that every legacy path or every platform has been verified.

Native SDL applications must compile the shared input frontend with
`KRYON_BACKEND_RAYLIB=1`; the native build template now supplies it. Verify the
actual downstream executable with quick taps, not only the core test binary.
Run `make sdl-pointer-test` for the SDL edge regression. Dropdown pack metrics
also keep their disclosure arrows inside the trigger bounds.

The Inbe virtual-desktop check also found two independent integration issues:
its inner draw callback ended the frame a second time, consuming input before
its next update; and native scroll paint clipping ignored the camera offset.
Inbe now lets its host own the frame boundary. Scroll scopes transform their
paint clip to screen coordinates, with offset/zoom regression coverage.
TitleBar leading actions supply the standard back arrow when no texture is
provided, matching the native and Go surfaces without an app wrapper.
The isolated Inbe checks cover 5 ms taps, all four inert style previews,
onboarding, phone bottom navigation, list editing, and persisted state.
