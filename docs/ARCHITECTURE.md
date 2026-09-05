# Kryon Architecture

Kryon is organized as a reusable runtime with a small set of public headers,
backend implementations, format/tooling paths, and conformance tests. The
runtime should expose primitives; downstream applications compose those
primitives into product behavior.

## Public API

Public headers live in `include/`. They define the app-facing Kryon surface:
window/runtime compatibility, UI widgets, platform services, formats, update,
sync, filesystem, desktop integration, and optional terminal primitives.

Public APIs should be named after the real domain concept. Do not add temporary
prefixes, compatibility aliases, or product-flavored names for new behavior.

## Runtime Implementation

Runtime code lives in `src/`. It owns widget behavior, rendering helpers,
platform adapters, serialization, sync/update primitives, and backend-specific
translation. Internal helpers should stay private to `src/` unless an unrelated
downstream application demonstrably needs the behavior.

### Widget lifecycle consolidation

C dropdown identity records now have stable dynamically allocated storage,
rather than a fixed 24-control array whose overflow reused the first control.
Frame-end overlay processing retires records for owners no longer declared.
C and Go tests declare 41 controls, open/select/reopen the first, and remove it
while checking independent selections and capture release. C dropdown storage
is still process-global; per-window ownership remains to be consolidated.
Options now use dynamically sized owned records and strings, removing the
128-option, 255-byte label and 31-byte font-name truncation limits. Unchanged
strings reuse their allocations; shrinking lists and retired controls release
their owned storage. Overlay painting visits visible rows only, and content
height saturates at the runtime's integer coordinate limit.

The shared KIR parser lowers `.kry` `Disabled { when = condition ... }` blocks
to existing runtime calls and lexical cleanup. The same cleanup pass used for
`defer` closes the scope on normal exit, return, break and continue before any
backend emits code. The generated buttons parity fixture exercises those exits
and nested disabled inheritance in C, Go and JS; C++ syntax coverage checks the
same shared lowering. Native `Scroll` blocks use the same lexical cleanup and
existing `BeginScroll`/`EndScroll` operations. An optional block name binds the
returned content rectangle without leaking it beyond the block. Native generated
scroll parity covers nested clips/input and restoration after return, break and
continue. Other begin/end APIs have not all gained block syntax.

The C widget implementation is still being consolidated. Some constructors
submit retained paint, while others register generic nodes and draw immediately.
Do not treat the presence of a tree node as proof that a widget participates in
deferred painting.

`Button`, `SliderFloat`, `SliderInt`, `VSliderFloat`, `VSliderInt`, and `SliderAngle` resolve
their declaration bounds before handling input and submit typed paint data when
building a tree. The slider painters do not process input. Labels and formats
are copied into node-owned storage; value arrays remain caller-owned. Without
a tree, the same input and paint helpers run immediately. This preserves the
public API while removing the separate prefixed slider draw entry points.
Angle sliders retain the caller's radians pointer and convert to degrees only
within input or paint execution; no pointer to a temporary conversion survives
declaration.

`DragFloat` and `DragInt` follow the same split, including owned label/format
storage and resolved bounds. `DragFloatRange2` and `DragIntRange2` compose two
ordinary typed drag nodes inside a Row, plus retained Text for the label. Their
children preserve the existing input IDs and borrow the caller's endpoint
pointers; no range-specific renderer or prefixed drag draw entry point remains.

`TextInRect` uses a typed retained text node with owned text, captured font
selection, and resolved bounds. Its deferred painter preserves the existing
centering and clipping rules. Headless declaration does not invoke a graphics
backend. The pixel test compares it against the immediate text renderer,
including an overlapping later rectangle and a mutated source string.

The remaining families must migrate with evidence for layout, input timing,
paint order, disabled/clip scopes, and data lifetime. Captured render textures
are currently an internal bridge for mixed painting, not a public popup API or
the completed widget architecture. Native Go keeps its own implementation and
must retain matching behavior through generated-runtime parity coverage.

Numeric editor state uses collision chains keyed by numeric type, widget ID,
and component index,
with stable record addresses. A bucket is not a single replaceable editor slot:
distinct keys must preserve independent text, cursor, and focus. C and Go tests
cover 129 keys that share the C bucket index. Each component reserves three
separate numeric control IDs for its field, decrement, and increment controls;
vectors larger than 16 components do not alias neighboring numeric widgets.
Tests cover 396 component keys and the emitted IDs for a 17-component input
beside another input. C's process-wide numeric state still needs
per-window lifetime integration as numeric widgets are consolidated.
Numeric ID allocation is not yet a shared identity service for every widget
family or arbitrary application-supplied control IDs.

Numeric input wrappers resolve their bounds before invoking the editor and do
not retain pointers to stack-local props. Their increment/decrement controls
use standard `Button` submissions, so step handling is independent of graphics
availability and participates in retained painting and disabled scopes. C and
Go tests cover normal, fast, and disabled stepping inside Row layout. Numeric
text editing also runs without a graphics window and returns changes during
declaration. Its painter receives an owned display-text snapshot plus caret,
selection, scroll, style, and font information; retained painting does not
process editing input again. Disabled typing is discarded, not replayed on
re-enabling the field. The ordinary retained TextField input-routing path and
numeric editor input path are still separate; consolidation is not complete.

Native Go collects deferred popup paint in `go/kryon/paint_layers.go`, using
ordinary frame operations rather than dropdown-specific drawing records.
Nested layers paint above their parents and restore the surrounding layout,
clip and disabled state. Existing dropdowns use this collector and the private
`popup_input.go` registry for scoped click ownership. That registry tracks
parent/child and sibling order, preserves capture before owner declaration in
the following frame, and removes descendants on closure or owner removal.
Native Go scroll scopes, lists, trees, source views and tables share a pointer
reachability check for wheel input, respecting popup ownership, disabled state
and parent clips. Generated C/Go tests verify that a background scroll scope
declared before an open combo cannot steal its wheel input.
Go drag-and-drop sources and targets also use that pointer check for starting
and accepting a drag; an already active source retains its payload when the
pointer leaves its original bounds. Rejected targets do not consume the release
or payload. Generated C/Go tests cover clipped sources/targets and copied data
lifetime across press and release.
General keyboard focus, active-drag routing, C composed-popup integration and a public
arbitrary-content combo scope remain unfinished.

## Backends

Backend selection is controlled by `KRYON_BACKEND`. The default native backend
uses raylib through Kryon's compatibility surface. Other backends are expected
to implement Kryon behavior through the same public/runtime contracts, with
their support documented in `docs/BACKEND_CAPABILITIES.json`,
`docs/BACKENDS.md`, and `docs/FEATURE_MATRIX.md`.

## Formats And Tools

The `.kry`, KIR, generated C, generated Go, and KRB paths are Kryon-owned
tooling surfaces. Tool changes should update the matching specs, conformance
matrix data, examples or fixtures, and generated documentation when applicable.

## Examples And Fixtures

Examples under `examples/` should remain generic and readable. They demonstrate
Kryon capabilities, not downstream product screens. Exact rendering and parity
fixtures should be generic enough that they can run in any app-independent
Kryon build.

## Tests And Matrices

Kryon uses several test layers:

- boundary and naming checks for repository hygiene
- public API snapshot checks for app-facing identifier drift
- public header compile checks for app-facing include hygiene
- examples manifest checks for example inventory and exactness fixtures
- backend capability checks for backend inventory drift
- generated-file checks for docs, compatibility headers, icons, and matrices
- parser, runtime, sync, update, platform, and widget tests
- conformance and visual matrix checks across renderers and runtime paths

Use `make preflight` before committing focused Kryon changes. Use `make test`
for the broader local regression suite. Use `make test-asan` or
`make test-ubsan` for sanitizer-backed regression runs, and
`make release-preflight` before publishing release artifacts.

## Downstream Integration

Downstream applications vendor Kryon as a submodule. Permanent Kryon changes
must be made and committed in this repository first, then brought into apps by
updating the submodule pointer. Never edit a downstream `vendor/kryon` tree as
the source of a Kryon change.
